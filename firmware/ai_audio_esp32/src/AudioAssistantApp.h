#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>

#include "AppConfig.h"
#include "AudioIO.h"
#include "EmotionDisplay.h"

namespace aura {

class AudioAssistantApp {
 public:
  void begin();
  void loop();

 private:
  static AudioAssistantApp* instance_;
  static void webSocketThunk(WStype_t type, uint8_t* payload, size_t length);
  void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);

  void connectWifi();
  void startWebSocket();
  void setupVoiceButton();
  bool readVoiceButton() const;
  void updateVoiceButton(uint32_t nowMs);
  void startRecording(uint32_t nowMs);
  void updateRecording(uint32_t nowMs);
  void finishRecording(bool cancelled);
  void handleTextMessage(const uint8_t* payload, size_t length);
  void sendHello();
  void sendEvent(const char* type, const char* sessionId = nullptr);
  String makeRequestId();

  WebSocketsClient webSocket_;
  EmotionDisplay display_;
  AudioIO audio_;

  bool webSocketConnected_ = false;
  bool webSocketStarted_ = false;
  bool wifiStarted_ = false;
  bool recording_ = false;
  bool heardSpeech_ = false;
  bool assistantAudioActive_ = false;
  bool voiceButtonRawPressed_ = false;
  bool voiceButtonStablePressed_ = false;
  uint32_t recordingStartedMs_ = 0;
  uint32_t lastSpeechMs_ = 0;
  uint32_t voiceButtonLastChangeMs_ = 0;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t sessionCounter_ = 0;
  String activeSessionId_;
  String webSocketHeaders_;
};

}  // namespace aura
