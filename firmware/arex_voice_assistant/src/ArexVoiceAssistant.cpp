#include "ArexVoiceAssistant.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "AppConfig.h"

namespace arex {

ArexVoiceAssistant* ArexVoiceAssistant::instance_ = nullptr;

void ArexVoiceAssistant::begin() {
  instance_ = this;
  Serial.begin(115200);
  display_.begin();
  setupVoiceButton();

  if (!audio_.begin()) {
    display_.show(Emotion::Error, "I2S failed");
    Serial.println("FATAL: failed to initialize I2S");
  }

  connectWifi();
  startWebSocket();
}

void ArexVoiceAssistant::loop() {
  const uint32_t nowMs = millis();

  if (WiFi.status() == WL_CONNECTED && !webSocketStarted_) {
    Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    startWebSocket();
  } else if (WiFi.status() != WL_CONNECTED &&
             nowMs - lastReconnectAttemptMs_ >= config::kReconnectPeriodMs) {
    webSocketConnected_ = false;
    webSocketStarted_ = false;
    connectWifi();
  }

  webSocket_.loop();
  updateVoiceButton(nowMs);
  updateWakeListener(nowMs);
  updateRecording(nowMs);
}

void ArexVoiceAssistant::connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  display_.show(Emotion::Thinking, "Connecting Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // Lower jitter while streaming PCM audio.
  if (!wifiStarted_) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiStarted_ = true;
  } else {
    WiFi.reconnect();
  }
  lastReconnectAttemptMs_ = millis();
}

void ArexVoiceAssistant::startWebSocket() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  const String path = String("/ws/audio/") + AUDIO_DEVICE_ID;
  webSocketHeaders_ = String("Authorization: Bearer ") + AUDIO_DEVICE_TOKEN + "\r\n";
  webSocket_.setExtraHeaders(webSocketHeaders_.c_str());
  webSocket_.begin(PC_SERVER_HOST, PC_SERVER_PORT, path);
  webSocketStarted_ = true;
  webSocket_.onEvent(webSocketThunk);
  webSocket_.setReconnectInterval(config::kReconnectPeriodMs);
  webSocket_.enableHeartbeat(15000, 3000, 2);
}

void ArexVoiceAssistant::setupVoiceButton() {
  pinMode(config::kVoiceButtonPin,
          config::kVoiceButtonActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  voiceButtonRawPressed_ = readVoiceButton();
  voiceButtonStablePressed_ = voiceButtonRawPressed_;
  voiceButtonLastChangeMs_ = millis();
}

bool ArexVoiceAssistant::readVoiceButton() const {
  const bool high = digitalRead(config::kVoiceButtonPin) == HIGH;
  return config::kVoiceButtonActiveLow ? !high : high;
}

void ArexVoiceAssistant::updateVoiceButton(uint32_t nowMs) {
  const bool pressed = readVoiceButton();
  if (pressed != voiceButtonRawPressed_) {
    voiceButtonRawPressed_ = pressed;
    voiceButtonLastChangeMs_ = nowMs;
  }

  if (pressed == voiceButtonStablePressed_ ||
      nowMs - voiceButtonLastChangeMs_ < config::kButtonDebounceMs) {
    return;
  }

  voiceButtonStablePressed_ = pressed;
  if (voiceButtonStablePressed_) {
    startRecording(nowMs, RecordingTrigger::Button);
  }
}

void ArexVoiceAssistant::updateWakeListener(uint32_t nowMs) {
  if (!config::kWakeListenEnabled || !webSocketConnected_ || recording_ ||
      assistantAudioActive_) {
    return;
  }
  if (static_cast<int32_t>(nowMs - wakeCooldownUntilMs_) < 0 ||
      nowMs - lastWakePollMs_ < config::kWakePollIntervalMs) {
    return;
  }
  lastWakePollMs_ = nowMs;

  int16_t pcm[128];
  uint16_t rms = 0;
  const size_t count = audio_.readMicrophone(pcm, 128, rms);
  if (count == 0 || rms < config::kWakeRmsThreshold) {
    return;
  }

  startRecording(nowMs, RecordingTrigger::Wake);
  if (recording_) {
    heardSpeech_ = true;
    lastSpeechMs_ = nowMs;
    wakeCooldownUntilMs_ = nowMs + config::kWakeCooldownMs;
    webSocket_.sendBIN(reinterpret_cast<uint8_t*>(pcm), count * sizeof(int16_t));
  }
}

void ArexVoiceAssistant::webSocketThunk(WStype_t type, uint8_t* payload,
                                       size_t length) {
  if (instance_ != nullptr) {
    instance_->onWebSocketEvent(type, payload, length);
  }
}

void ArexVoiceAssistant::onWebSocketEvent(WStype_t type, uint8_t* payload,
                                         size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      webSocketConnected_ = true;
      display_.show(Emotion::Idle, "Arex online");
      sendHello();
      break;
    case WStype_DISCONNECTED:
      webSocketConnected_ = false;
      assistantAudioActive_ = false;
      if (recording_) {
        finishRecording(true);
      }
      display_.show(Emotion::Error, "PC disconnected");
      break;
    case WStype_TEXT:
      handleTextMessage(payload, length);
      break;
    case WStype_BIN:
      if (assistantAudioActive_) {
        audio_.writeSpeaker(payload, length);
      }
      break;
    default:
      break;
  }
}

void ArexVoiceAssistant::startRecording(uint32_t nowMs, RecordingTrigger trigger) {
  if (!webSocketConnected_ || recording_ || assistantAudioActive_) {
    display_.show(Emotion::Error, "Voice unavailable");
    return;
  }
  activeTrigger_ = trigger;
  activeSessionId_ = makeRequestId();
  recording_ = true;
  heardSpeech_ = false;
  recordingStartedMs_ = nowMs;
  lastSpeechMs_ = nowMs;
  display_.show(Emotion::Listening,
                trigger == RecordingTrigger::Wake ? "Heard sound" : "Listening");

  StaticJsonDocument<384> doc;
  doc["version"] = 1;
  doc["type"] = "audio.start";
  doc["session_id"] = activeSessionId_;
  doc["trigger"] = triggerName(activeTrigger_);
  doc["format"] = "pcm_s16le";
  doc["sample_rate_hz"] = config::kMicSampleRate;
  doc["channels"] = 1;
  String json;
  serializeJson(doc, json);
  webSocket_.sendTXT(json);
}

void ArexVoiceAssistant::updateRecording(uint32_t nowMs) {
  if (!recording_) {
    return;
  }

  int16_t pcm[256];
  uint16_t rms = 0;
  const size_t count = audio_.readMicrophone(pcm, 256, rms);
  if (count > 0) {
    webSocket_.sendBIN(reinterpret_cast<uint8_t*>(pcm), count * sizeof(int16_t));
  }
  if (rms >= config::kSpeechRmsThreshold) {
    heardSpeech_ = true;
    lastSpeechMs_ = nowMs;
  }

  const uint32_t duration = nowMs - recordingStartedMs_;
  const bool maxReached = duration >= config::kRecordingMaxMs;
  const bool silenceReached = heardSpeech_ && duration >= config::kRecordingMinMs &&
                              nowMs - lastSpeechMs_ >= config::kSilenceToStopMs;
  if (maxReached || silenceReached) {
    finishRecording(false);
  }
}

void ArexVoiceAssistant::finishRecording(bool cancelled) {
  if (!recording_) {
    return;
  }
  recording_ = false;
  sendEvent(cancelled ? "audio.cancel" : "audio.end", activeSessionId_.c_str());
  if (!cancelled) {
    display_.show(Emotion::Thinking);
  }
}

void ArexVoiceAssistant::handleTextMessage(const uint8_t* payload, size_t length) {
  StaticJsonDocument<1024> doc;
  const DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.printf("Bad JSON from PC: %s\n", error.c_str());
    return;
  }
  const char* type = doc["type"] | "";

  if (strcmp(type, "assistant.audio.start") == 0) {
    assistantAudioActive_ = true;
    display_.show(Emotion::Speaking);
  } else if (strcmp(type, "assistant.audio.end") == 0) {
    assistantAudioActive_ = false;
    display_.show(Emotion::Idle);
  } else if (strcmp(type, "wake.ignored") == 0) {
    display_.show(Emotion::Idle, "Say Arex");
    Serial.printf("Wake ignored: %s\n", doc["text"] | "");
  } else if (strcmp(type, "emotion") == 0) {
    const char* state = doc["state"] | "idle";
    if (strcmp(state, "thinking") == 0) display_.show(Emotion::Thinking);
    else if (strcmp(state, "listening") == 0) display_.show(Emotion::Listening);
    else if (strcmp(state, "speaking") == 0) display_.show(Emotion::Speaking);
    else if (strcmp(state, "error") == 0) display_.show(Emotion::Error);
    else display_.show(Emotion::Idle);
  } else if (strcmp(type, "assistant.text") == 0) {
    Serial.printf("AI: %s\n", doc["text"] | "");
  } else if (strcmp(type, "transcript") == 0) {
    Serial.printf("Heard: %s\n", doc["text"] | "");
  } else if (strcmp(type, "error") == 0) {
    display_.show(Emotion::Error, doc["message"] | "Server error");
  }
}

void ArexVoiceAssistant::sendHello() {
  StaticJsonDocument<384> doc;
  doc["version"] = 1;
  doc["type"] = "hello";
  doc["device_id"] = AUDIO_DEVICE_ID;
  doc["firmware"] = "1.0.0";
  doc["ip"] = WiFi.localIP().toString();
  String json;
  serializeJson(doc, json);
  webSocket_.sendTXT(json);
}

void ArexVoiceAssistant::sendEvent(const char* type, const char* sessionId) {
  if (!webSocketConnected_) {
    return;
  }
  StaticJsonDocument<384> doc;
  doc["version"] = 1;
  doc["type"] = type;
  if (sessionId != nullptr) {
    doc["session_id"] = sessionId;
  }
  String json;
  serializeJson(doc, json);
  webSocket_.sendTXT(json);
}

String ArexVoiceAssistant::makeRequestId() {
  char id[48];
  snprintf(id, sizeof(id), "%s-%08lx-%lu", AUDIO_DEVICE_ID,
           static_cast<unsigned long>(millis()),
           static_cast<unsigned long>(sessionCounter_ + 1));
  sessionCounter_++;
  return String(id);
}

const char* ArexVoiceAssistant::triggerName(RecordingTrigger trigger) const {
  switch (trigger) {
    case RecordingTrigger::Button:
      return "button";
    case RecordingTrigger::Wake:
      return "wake";
  }
  return "button";
}

}  // namespace arex
