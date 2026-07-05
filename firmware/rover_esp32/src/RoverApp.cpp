#include "RoverApp.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "AppConfig.h"

namespace aura {

RoverApp* RoverApp::instance_ = nullptr;

void RoverApp::begin() {
  instance_ = this;
  Serial.begin(115200);
  display_.begin();
  alert_.begin();

  if (!audio_.begin()) {
    display_.show(Emotion::Error, "I2S failed");
    Serial.println("FATAL: failed to initialize I2S");
  }

  connectWifi();
  startWebSocket();
}

void RoverApp::loop() {
  const uint32_t nowMs = millis();

  if (WiFi.status() == WL_CONNECTED && !webSocketStarted_) {
    Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    startWebSocket();
  } else if (WiFi.status() != WL_CONNECTED &&
      nowMs - lastReconnectAttemptMs_ >= config::kReconnectPeriodMs) {
    connectWifi();
  }

  webSocket_.loop();
  handleUnoEvent(alert_.update(nowMs), nowMs);
  updateRecording(nowMs);
  executePendingHomeCommand();

  if (warningUntilMs_ != 0 && static_cast<int32_t>(nowMs - warningUntilMs_) >= 0 &&
      !alert_.obstacleActive() && !recording_ && !assistantAudioActive_) {
    warningUntilMs_ = 0;
    display_.show(Emotion::Idle);
  }
}

void RoverApp::connectWifi() {
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

void RoverApp::startWebSocket() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  const String path = String("/ws/rover/") + ROVER_DEVICE_ID;
  webSocketHeaders_ = String("Authorization: Bearer ") + ROVER_TOKEN + "\r\n";
  webSocket_.setExtraHeaders(webSocketHeaders_.c_str());
  webSocket_.begin(PC_SERVER_HOST, PC_SERVER_PORT, path);
  webSocketStarted_ = true;
  webSocket_.onEvent(webSocketThunk);
  webSocket_.setReconnectInterval(config::kReconnectPeriodMs);
  webSocket_.enableHeartbeat(15000, 3000, 2);
}

void RoverApp::webSocketThunk(WStype_t type, uint8_t* payload, size_t length) {
  if (instance_ != nullptr) {
    instance_->onWebSocketEvent(type, payload, length);
  }
}

void RoverApp::onWebSocketEvent(WStype_t type, uint8_t* payload,
                                size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      webSocketConnected_ = true;
      display_.show(Emotion::Idle, "AI connected");
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

void RoverApp::handleUnoEvent(UnoEvent event, uint32_t nowMs) {
  if (event == UnoEvent::None) {
    return;
  }
  if (event == UnoEvent::Obstacle) {
    if (recording_) {
      finishRecording(true);
    }
    // Ignore any queued conversational PCM; the safety warning has priority.
    assistantAudioActive_ = false;
    warningUntilMs_ = nowMs + config::kWarningDisplayMs;
    display_.show(Emotion::Warning, "Vehicle stopped");
    audio_.playWarningTone();
    sendEvent("safety.obstacle");
    return;
  }
  if (event == UnoEvent::VoiceRequest) {
    startRecording(nowMs);
  }
}

void RoverApp::startRecording(uint32_t nowMs) {
  if (!webSocketConnected_ || recording_ || assistantAudioActive_) {
    display_.show(Emotion::Error, "Voice unavailable");
    return;
  }
  activeSessionId_ = makeRequestId();
  recording_ = true;
  heardSpeech_ = false;
  recordingStartedMs_ = nowMs;
  lastSpeechMs_ = nowMs;
  display_.show(Emotion::Listening);

  StaticJsonDocument<384> doc;
  doc["version"] = 1;
  doc["type"] = "audio.start";
  doc["session_id"] = activeSessionId_;
  doc["format"] = "pcm_s16le";
  doc["sample_rate_hz"] = config::kMicSampleRate;
  doc["channels"] = 1;
  String json;
  serializeJson(doc, json);
  webSocket_.sendTXT(json);
}

void RoverApp::updateRecording(uint32_t nowMs) {
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

void RoverApp::finishRecording(bool cancelled) {
  if (!recording_) {
    return;
  }
  recording_ = false;
  sendEvent(cancelled ? "audio.cancel" : "audio.end", activeSessionId_.c_str());
  if (!cancelled) {
    display_.show(Emotion::Thinking);
  }
}

void RoverApp::handleTextMessage(const uint8_t* payload, size_t length) {
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
    if (warningUntilMs_ == 0) {
      display_.show(Emotion::Idle);
    }
  } else if (strcmp(type, "emotion") == 0) {
    const char* state = doc["state"] | "idle";
    if (strcmp(state, "thinking") == 0) display_.show(Emotion::Thinking);
    else if (strcmp(state, "listening") == 0) display_.show(Emotion::Listening);
    else if (strcmp(state, "speaking") == 0) display_.show(Emotion::Speaking);
    else if (strcmp(state, "warning") == 0) display_.show(Emotion::Warning);
    else if (strcmp(state, "error") == 0) display_.show(Emotion::Error);
    else display_.show(Emotion::Idle);
  } else if (strcmp(type, "home.command") == 0 && !homeCommandPending_) {
    pendingHomeCommand_.requestId = doc["request_id"].as<String>();
    pendingHomeCommand_.device = doc["device"].as<String>();
    pendingHomeCommand_.state = doc["state"].as<String>();
    homeCommandPending_ = true;
  } else if (strcmp(type, "error") == 0) {
    display_.show(Emotion::Error, doc["message"] | "Server error");
  }
}

void RoverApp::executePendingHomeCommand() {
  if (!homeCommandPending_) {
    return;
  }
  homeCommandPending_ = false;
  const HomeResult result = home_.execute(pendingHomeCommand_);

  StaticJsonDocument<512> doc;
  doc["version"] = 1;
  doc["type"] = "home.result";
  doc["request_id"] = pendingHomeCommand_.requestId;
  doc["ok"] = result.ok;
  doc["status_code"] = result.statusCode;
  doc["detail"] = result.response;
  String json;
  serializeJson(doc, json);
  if (webSocketConnected_) {
    webSocket_.sendTXT(json);
  }
}

void RoverApp::sendHello() {
  StaticJsonDocument<384> doc;
  doc["version"] = 1;
  doc["type"] = "hello";
  doc["device_id"] = ROVER_DEVICE_ID;
  doc["firmware"] = "1.0.0";
  doc["ip"] = WiFi.localIP().toString();
  String json;
  serializeJson(doc, json);
  webSocket_.sendTXT(json);
}

void RoverApp::sendEvent(const char* type, const char* sessionId) {
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

String RoverApp::makeRequestId() {
  char id[48];
  snprintf(id, sizeof(id), "%s-%08lx-%lu", ROVER_DEVICE_ID,
           static_cast<unsigned long>(millis()),
           static_cast<unsigned long>(sessionCounter_ + 1));
  sessionCounter_++;
  return String(id);
}

}  // namespace aura
