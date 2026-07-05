#include <Arduino.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>

#if defined(AURA_USE_EXAMPLE_SECRETS)
#include "secrets.example.h"
#elif __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and configure it."
#endif

// ================================================================
// AURA Rover ESP32 standalone bench test
//
// Tests without an Uno, rover motors, PC server, or OpenAI account:
//   O - OLED and all six emotion faces
//   M - INMP441 microphone and live RMS level
//   S - MAX98357A speaker tones
//   W - ESP32 Wi-Fi radio scan
//   A - GPIO25 obstacle-alert input
//   R - Run the main tests in sequence
//   C - Connect to the AURA PC server
//   V - Complete microphone -> OpenAI -> speaker voice test
// ================================================================

namespace pins {
constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;
constexpr uint8_t ALERT_INPUT = 25;

constexpr uint8_t MIC_BCLK = 26;
constexpr uint8_t MIC_WS = 27;
constexpr uint8_t MIC_DATA = 34;

constexpr uint8_t SPEAKER_BCLK = 14;
constexpr uint8_t SPEAKER_LRC = 13;
constexpr uint8_t SPEAKER_DATA = 32;
}  // namespace pins

constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t MIC_SAMPLE_RATE = 16000;
constexpr uint32_t SPEAKER_SAMPLE_RATE = 24000;
constexpr uint8_t MIC_RIGHT_SHIFT = 14;

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
WebSocketsClient webSocket;

bool microphoneReady = false;
bool speakerReady = false;
bool pcConnected = false;
bool assistantAudioActive = false;
bool voiceRoundTripDone = false;
bool voiceRoundTripFailed = false;
String webSocketHeaders;
String activeVoiceSession;

void showScreen(const char* face, const char* label);
void showMessage(const char* line1, const char* line2 = nullptr);
void printMenu();
bool initializeMicrophone();
bool initializeSpeaker();
uint16_t readMicrophoneRms();
size_t readMicrophonePcm(int16_t* output, size_t maxSamples, uint16_t& rms);
void testOled();
void testMicrophone(uint32_t durationMs = 8000);
void testSpeaker();
void testWifi();
void testAlertInput(uint32_t timeoutMs = 12000);
void runAllTests();
void playToneHz(float frequency, uint16_t durationMs, int16_t amplitude = 4500);
bool connectToPc();
void testPcConnection();
void testOpenAiVoiceRoundTrip();
void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
void handlePcText(const uint8_t* payload, size_t length);
void sendAudioMarker(const char* type, const String& sessionId);

void setup() {
  Serial.begin(115200);
  delay(250);

  pinMode(pins::ALERT_INPUT, INPUT_PULLDOWN);

  Wire.begin(pins::OLED_SDA, pins::OLED_SCL);
  oled.setI2CAddress(OLED_ADDRESS << 1);
  oled.begin();
  showScreen("(^_^)", "ESP32 test ready");

  microphoneReady = initializeMicrophone();
  speakerReady = initializeSpeaker();

  Serial.println();
  Serial.println("AURA Rover - ESP32 standalone bench test");
  Serial.println("No Uno, PC server, API key, or motors are required.");
  Serial.print("Microphone initialization: ");
  Serial.println(microphoneReady ? "PASS" : "FAIL");
  Serial.print("Speaker initialization: ");
  Serial.println(speakerReady ? "PASS" : "FAIL");
  printMenu();
}

void loop() {
  webSocket.loop();
  if (!Serial.available()) {
    delay(5);
    return;
  }

  const char command = static_cast<char>(toupper(Serial.read()));
  while (Serial.available()) {
    Serial.read();
  }

  switch (command) {
    case 'O': testOled(); break;
    case 'M': testMicrophone(); break;
    case 'S': testSpeaker(); break;
    case 'W': testWifi(); break;
    case 'A': testAlertInput(); break;
    case 'R': runAllTests(); break;
    case 'C': testPcConnection(); break;
    case 'V': testOpenAiVoiceRoundTrip(); break;
    case 'H':
    case '?': printMenu(); break;
    default:
      Serial.println("Unknown command. Enter H for help.");
      break;
  }
}

void printMenu() {
  Serial.println();
  Serial.println("========== TEST MENU ==========");
  Serial.println("O  OLED emotion-face test");
  Serial.println("M  Microphone level test (8 seconds)");
  Serial.println("S  Speaker tone test");
  Serial.println("W  Wi-Fi network scan");
  Serial.println("A  GPIO25 alert-input test");
  Serial.println("R  Run main tests in sequence");
  Serial.println("C  Connect to AURA PC server");
  Serial.println("V  Full OpenAI voice round-trip test");
  Serial.println("H  Show this menu");
  Serial.println("===============================");
  Serial.println("Enter one letter and press Send/Enter.");
}

void showScreen(const char* face, const char* label) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_courB18_tf);
  const int16_t faceWidth = oled.getStrWidth(face);
  oled.drawStr((128 - faceWidth) / 2, 30, face);

  oled.setFont(u8g2_font_6x10_tf);
  const int16_t labelWidth = oled.getStrWidth(label);
  const int16_t x = (128 - labelWidth) / 2;
  oled.drawStr(x > 0 ? x : 0, 55, label);
  oled.sendBuffer();
}

void showMessage(const char* line1, const char* line2) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(2, 25, line1);
  if (line2 != nullptr) {
    oled.drawStr(2, 43, line2);
  }
  oled.sendBuffer();
}

bool initializeMicrophone() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = MIC_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 6,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };

  const i2s_pin_config_t pinConfig = {
      .bck_io_num = pins::MIC_BCLK,
      .ws_io_num = pins::MIC_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = pins::MIC_DATA,
  };

  return i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) == ESP_OK &&
         i2s_set_pin(I2S_NUM_0, &pinConfig) == ESP_OK;
}

bool initializeSpeaker() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SPEAKER_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
  };

  const i2s_pin_config_t pinConfig = {
      .bck_io_num = pins::SPEAKER_BCLK,
      .ws_io_num = pins::SPEAKER_LRC,
      .data_out_num = pins::SPEAKER_DATA,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  return i2s_driver_install(I2S_NUM_1, &config, 0, nullptr) == ESP_OK &&
         i2s_set_pin(I2S_NUM_1, &pinConfig) == ESP_OK &&
         i2s_zero_dma_buffer(I2S_NUM_1) == ESP_OK;
}

uint16_t readMicrophoneRms() {
  int16_t pcm[256];
  uint16_t rms = 0;
  readMicrophonePcm(pcm, 256, rms);
  return rms;
}

size_t readMicrophonePcm(int16_t* output, size_t maxSamples, uint16_t& rms) {
  static int32_t raw[256];
  const size_t wanted = min(maxSamples, static_cast<size_t>(256));
  size_t bytesRead = 0;
  if (i2s_read(I2S_NUM_0, raw, wanted * sizeof(int32_t), &bytesRead,
               pdMS_TO_TICKS(30)) != ESP_OK) {
    rms = 0;
    return 0;
  }

  const size_t count = bytesRead / sizeof(int32_t);
  if (count == 0) {
    rms = 0;
    return 0;
  }

  uint64_t energy = 0;
  for (size_t i = 0; i < count; ++i) {
    int32_t sample = raw[i] >> MIC_RIGHT_SHIFT;
    sample = constrain(sample, -32768, 32767);
    output[i] = static_cast<int16_t>(sample);
    energy += static_cast<int64_t>(sample) * sample;
  }
  rms = static_cast<uint16_t>(sqrt(energy / count));
  return count;
}

void testOled() {
  Serial.println();
  Serial.println("OLED TEST: confirm all six faces are visible and centered.");
  const char* faces[] = {
      "(^_^)", "(O_O)", "(-_-)", "(^o^)", "(>_<)", "(X_X)"};
  const char* labels[] = {
      "Idle", "Listening", "Thinking", "Speaking", "Warning", "Error"};

  for (uint8_t i = 0; i < 6; ++i) {
    Serial.print(labels[i]);
    Serial.print(": ");
    Serial.println(faces[i]);
    showScreen(faces[i], labels[i]);
    delay(1200);
  }
  showScreen("(^_^)", "OLED test done");
  Serial.println("OLED TEST COMPLETE - visual confirmation required.");
}

void testMicrophone(uint32_t durationMs) {
  Serial.println();
  if (!microphoneReady) {
    Serial.println("MICROPHONE TEST FAIL: I2S initialization failed.");
    showScreen("(X_X)", "Mic init failed");
    return;
  }

  Serial.println("MICROPHONE TEST: speak, clap, and remain quiet.");
  Serial.println("Live RMS values should rise clearly when you make a sound.");
  uint16_t minimumRms = UINT16_MAX;
  uint16_t maximumRms = 0;
  const uint32_t started = millis();
  uint32_t lastDisplayMs = 0;

  while (millis() - started < durationMs) {
    const uint16_t rms = readMicrophoneRms();
    minimumRms = min(minimumRms, rms);
    maximumRms = max(maximumRms, rms);

    if (millis() - lastDisplayMs >= 120) {
      lastDisplayMs = millis();
      Serial.print("Mic RMS: ");
      Serial.println(rms);

      oled.clearBuffer();
      oled.setFont(u8g2_font_6x10_tf);
      oled.drawStr(2, 12, "INMP441 live level");
      char value[24];
      snprintf(value, sizeof(value), "RMS: %u", rms);
      oled.drawStr(2, 28, value);
      const uint32_t scaledWidth = rms / 20;
      const uint8_t barWidth = scaledWidth < 124 ? scaledWidth : 124;
      oled.drawFrame(2, 40, 124, 16);
      oled.drawBox(4, 42, barWidth, 12);
      oled.sendBuffer();
    }
  }

  const uint16_t range = maximumRms - minimumRms;
  Serial.print("Minimum RMS: "); Serial.println(minimumRms);
  Serial.print("Maximum RMS: "); Serial.println(maximumRms);
  Serial.print("RMS range:   "); Serial.println(range);
  if (maximumRms > 100 && range > 100) {
    Serial.println("MICROPHONE TEST PASS: changing audio signal detected.");
    showScreen("(^_^)", "Mic signal PASS");
  } else {
    Serial.println("MICROPHONE TEST CHECK: signal did not change enough.");
    showScreen("(X_X)", "Check mic wiring");
  }
}

void playToneHz(float frequency, uint16_t durationMs, int16_t amplitude) {
  if (!speakerReady) {
    return;
  }

  int16_t samples[120];
  const uint32_t totalSamples =
      static_cast<uint32_t>(SPEAKER_SAMPLE_RATE) * durationMs / 1000;
  uint32_t generated = 0;

  while (generated < totalSamples) {
    const uint32_t remaining = totalSamples - generated;
    const size_t blockSamples = remaining < 120 ? remaining : 120;
    for (size_t i = 0; i < blockSamples; ++i) {
      const float phase =
          2.0f * PI * frequency * (generated + i) / SPEAKER_SAMPLE_RATE;
      samples[i] = static_cast<int16_t>(amplitude * sinf(phase));
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_1, samples, blockSamples * sizeof(int16_t),
              &bytesWritten, pdMS_TO_TICKS(100));
    generated += blockSamples;
  }
}

void testSpeaker() {
  Serial.println();
  if (!speakerReady) {
    Serial.println("SPEAKER TEST FAIL: I2S initialization failed.");
    showScreen("(X_X)", "Speaker init fail");
    return;
  }

  Serial.println("SPEAKER TEST: you should hear three clear tones.");
  showScreen("(^o^)", "Speaker test");
  playToneHz(440.0f, 350);
  delay(100);
  playToneHz(660.0f, 350);
  delay(100);
  playToneHz(880.0f, 500);
  i2s_zero_dma_buffer(I2S_NUM_1);
  showScreen("(^_^)", "Did you hear it?");
  Serial.println("SPEAKER TEST COMPLETE - confirm the tones by listening.");
}

void testWifi() {
  Serial.println();
  Serial.println("WI-FI TEST: scanning nearby networks; no password required.");
  showScreen("(-_-)", "Scanning Wi-Fi");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(150);
  const int count = WiFi.scanNetworks(false, true);

  if (count <= 0) {
    Serial.println("WI-FI TEST CHECK: no networks found.");
    showScreen("(X_X)", "No Wi-Fi found");
    WiFi.scanDelete();
    return;
  }

  Serial.print("WI-FI TEST PASS: found ");
  Serial.print(count);
  Serial.println(" network(s).");
  for (int i = 0; i < count; ++i) {
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(WiFi.SSID(i));
    Serial.print("  RSSI ");
    Serial.print(WiFi.RSSI(i));
    Serial.println(" dBm");
  }

  char label[28];
  snprintf(label, sizeof(label), "Wi-Fi PASS: %d found", count);
  showScreen("(^_^)", label);
  WiFi.scanDelete();
}

void testAlertInput(uint32_t timeoutMs) {
  Serial.println();
  Serial.println("GPIO25 ALERT TEST:");
  Serial.println("Within 12 seconds, connect GPIO25 to 3V3 through a 1 kOhm resistor.");
  Serial.println("Do not connect GPIO25 to 5 V.");
  showMessage("GPIO25 alert test", "Apply safe 3.3 V");

  const uint32_t started = millis();
  uint32_t highStartedMs = 0;
  bool wasHigh = false;
  while (millis() - started < timeoutMs) {
    const bool high = digitalRead(pins::ALERT_INPUT) == HIGH;
    if (high && !wasHigh) {
      highStartedMs = millis();
    }
    if (high && millis() - highStartedMs >= 150) {
      Serial.println("GPIO25 ALERT TEST PASS: long HIGH detected.");
      showScreen("(>_<)", "Alert input PASS");
      playToneHz(880.0f, 200);
      playToneHz(660.0f, 250);
      return;
    }
    wasHigh = high;
    delay(2);
  }

  Serial.println("GPIO25 ALERT TEST CHECK: no long HIGH was detected.");
  showScreen("(X_X)", "No GPIO25 alert");
}

void runAllTests() {
  Serial.println();
  Serial.println("RUNNING ESP32 BENCH TESTS");
  Serial.println("Follow the OLED, Serial Monitor, and sound instructions.");
  testOled();
  testSpeaker();
  testMicrophone(6000);
  testWifi();
  Serial.println();
  Serial.println("Automatic sequence complete.");
  Serial.println("Run A separately when ready to test GPIO25.");
  showScreen("(^_^)", "Main tests done");
  printMenu();
}

bool connectToPc() {
  if (pcConnected) {
    return true;
  }

  Serial.println();
  Serial.println("PC CONNECTION TEST");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(TEST_WIFI_SSID);
    showScreen("(-_-)", "Connecting Wi-Fi");

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
    const uint32_t wifiStarted = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStarted < 15000) {
      delay(100);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("PC CONNECTION FAIL: Wi-Fi did not connect.");
    showScreen("(X_X)", "Wi-Fi failed");
    return false;
  }

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Connecting to PC: ws://");
  Serial.print(TEST_PC_HOST);
  Serial.print(':');
  Serial.println(TEST_PC_PORT);
  showScreen("(-_-)", "Connecting to PC");

  webSocketHeaders =
      String("Authorization: Bearer ") + TEST_ROVER_TOKEN + "\r\n";
  webSocket.setExtraHeaders(webSocketHeaders.c_str());
  const String path = String("/ws/rover/") + TEST_DEVICE_ID;
  webSocket.begin(TEST_PC_HOST, TEST_PC_PORT, path);
  webSocket.onEvent(onWebSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);

  const uint32_t socketStarted = millis();
  while (!pcConnected && millis() - socketStarted < 12000) {
    webSocket.loop();
    delay(5);
  }

  if (!pcConnected) {
    Serial.println("PC CONNECTION FAIL: WebSocket did not connect.");
    Serial.println("Check PC IP, server, token, firewall, and Wi-Fi network.");
    showScreen("(X_X)", "PC connection fail");
    return false;
  }
  return true;
}

void testPcConnection() {
  if (connectToPc()) {
    Serial.println("PC CONNECTION TEST PASS.");
    showScreen("(^_^)", "PC server PASS");
  }
}

void testOpenAiVoiceRoundTrip() {
  Serial.println();
  Serial.println("FULL OPENAI VOICE TEST");
  if (!microphoneReady || !speakerReady) {
    Serial.println("TEST FAIL: microphone or speaker I2S initialization failed.");
    showScreen("(X_X)", "Check audio I2S");
    return;
  }
  if (!connectToPc()) {
    return;
  }

  activeVoiceSession =
      String(TEST_DEVICE_ID) + "-test-" + String(millis(), HEX);
  voiceRoundTripDone = false;
  voiceRoundTripFailed = false;

  StaticJsonDocument<384> start;
  start["version"] = 1;
  start["type"] = "audio.start";
  start["session_id"] = activeVoiceSession;
  start["format"] = "pcm_s16le";
  start["sample_rate_hz"] = MIC_SAMPLE_RATE;
  start["channels"] = 1;
  String startJson;
  serializeJson(start, startJson);
  webSocket.sendTXT(startJson);
  webSocket.loop();

  Serial.println("Speak now. Recording for five seconds...");
  Serial.println("Try: What is a mecanum wheel?");
  showScreen("(O_O)", "Speak now - 5 sec");

  const uint32_t recordingStarted = millis();
  uint32_t lastLevelPrint = 0;
  while (millis() - recordingStarted < 5000) {
    webSocket.loop();
    if (!pcConnected) {
      Serial.println("FULL OPENAI VOICE TEST FAIL: WebSocket disconnected during recording.");
      showScreen("(X_X)", "PC disconnected");
      return;
    }
    int16_t pcm[256];
    uint16_t rms = 0;
    const size_t count = readMicrophonePcm(pcm, 256, rms);
    if (count > 0) {
      webSocket.sendBIN(
          reinterpret_cast<uint8_t*>(pcm), count * sizeof(int16_t));
      webSocket.loop();
    }
    if (millis() - lastLevelPrint >= 500) {
      lastLevelPrint = millis();
      Serial.print("Recording RMS: ");
      Serial.println(rms);
    }
  }

  if (!pcConnected) {
    Serial.println("FULL OPENAI VOICE TEST FAIL: WebSocket disconnected before audio.end.");
    showScreen("(X_X)", "PC disconnected");
    return;
  }
  sendAudioMarker("audio.end", activeVoiceSession);
  webSocket.loop();
  showScreen("(-_-)", "OpenAI thinking");
  Serial.println("Audio sent. Waiting up to 45 seconds for the response...");

  const uint32_t responseStarted = millis();
  while (!voiceRoundTripDone && !voiceRoundTripFailed &&
         millis() - responseStarted < 45000) {
    webSocket.loop();
    if (!pcConnected && !voiceRoundTripDone && !voiceRoundTripFailed) {
      Serial.println("FULL OPENAI VOICE TEST FAIL: WebSocket disconnected while waiting for OpenAI.");
      showScreen("(X_X)", "PC disconnected");
      return;
    }
    delay(2);
  }

  if (voiceRoundTripDone) {
    Serial.println("FULL OPENAI VOICE TEST PASS.");
    Serial.println("The key, transcription, AI response, TTS, and speaker path worked.");
    showScreen("(^_^)", "OpenAI test PASS");
  } else if (voiceRoundTripFailed) {
    Serial.println("FULL OPENAI VOICE TEST FAIL: server returned an error.");
  } else {
    Serial.println("FULL OPENAI VOICE TEST FAIL: response timed out.");
    showScreen("(X_X)", "OpenAI timeout");
  }
}

void sendAudioMarker(const char* type, const String& sessionId) {
  StaticJsonDocument<256> document;
  document["version"] = 1;
  document["type"] = type;
  document["session_id"] = sessionId;
  String json;
  serializeJson(document, json);
  webSocket.sendTXT(json);
}

void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      pcConnected = true;
      Serial.println("WebSocket connected to AURA PC server.");
      StaticJsonDocument<256> hello;
      hello["version"] = 1;
      hello["type"] = "hello";
      hello["device_id"] = TEST_DEVICE_ID;
      hello["firmware"] = "esp32-bench-1.0";
      hello["ip"] = WiFi.localIP().toString();
      String json;
      serializeJson(hello, json);
      webSocket.sendTXT(json);
      break;
    }
    case WStype_DISCONNECTED:
      pcConnected = false;
      assistantAudioActive = false;
      Serial.println("WebSocket disconnected from PC server.");
      break;
    case WStype_TEXT:
      handlePcText(payload, length);
      break;
    case WStype_BIN:
      if (assistantAudioActive && speakerReady) {
        size_t written = 0;
        i2s_write(I2S_NUM_1, payload, length, &written, pdMS_TO_TICKS(200));
      }
      break;
    default:
      break;
  }
}

void handlePcText(const uint8_t* payload, size_t length) {
  StaticJsonDocument<1024> document;
  const DeserializationError error = deserializeJson(document, payload, length);
  if (error) {
    Serial.print("Bad PC JSON: ");
    Serial.println(error.c_str());
    return;
  }

  const char* type = document["type"] | "";
  if (strcmp(type, "emotion") == 0) {
    const char* state = document["state"] | "idle";
    if (strcmp(state, "thinking") == 0) showScreen("(-_-)", "OpenAI thinking");
    else if (strcmp(state, "speaking") == 0) showScreen("(^o^)", "AI speaking");
    else if (strcmp(state, "error") == 0) showScreen("(X_X)", "Server error");
  } else if (strcmp(type, "transcript") == 0) {
    Serial.print("TRANSCRIPT: ");
    Serial.println(document["text"] | "");
  } else if (strcmp(type, "assistant.text") == 0) {
    Serial.print("AURA RESPONSE: ");
    Serial.println(document["text"] | "");
  } else if (strcmp(type, "assistant.audio.start") == 0) {
    assistantAudioActive = true;
    showScreen("(^o^)", "AI speaking");
  } else if (strcmp(type, "assistant.audio.end") == 0) {
    assistantAudioActive = false;
    i2s_zero_dma_buffer(I2S_NUM_1);
    voiceRoundTripDone = true;
  } else if (strcmp(type, "error") == 0) {
    assistantAudioActive = false;
    voiceRoundTripFailed = true;
    Serial.print("SERVER ERROR [");
    Serial.print(document["code"] | "unknown");
    Serial.print("]: ");
    Serial.println(document["message"] | "no detail");
    showScreen("(X_X)", "OpenAI/server error");
  }
}
