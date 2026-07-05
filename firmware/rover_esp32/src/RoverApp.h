#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>

#include "AppConfig.h"
#include "AlertDecoder.h"
#include "AudioIO.h"
#include "EmotionDisplay.h"
#include "HomeClient.h"

namespace aura {

class RoverApp {
 public:
  void begin();
  void loop();

 private:
  static RoverApp* instance_;
  static void webSocketThunk(WStype_t type, uint8_t* payload, size_t length);
  void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);

  void connectWifi();
  void startWebSocket();
  void handleUnoEvent(UnoEvent event, uint32_t nowMs);
  void startRecording(uint32_t nowMs);
  void updateRecording(uint32_t nowMs);
  void finishRecording(bool cancelled);
  void handleTextMessage(const uint8_t* payload, size_t length);
  void executePendingHomeCommand();
  void sendHello();
  void sendEvent(const char* type, const char* sessionId = nullptr);
  String makeRequestId();

  WebSocketsClient webSocket_;
  EmotionDisplay display_;
  AlertDecoder alert_{config::kAlertPin};
  AudioIO audio_;
  HomeClient home_;

  bool webSocketConnected_ = false;
  bool webSocketStarted_ = false;
  bool wifiStarted_ = false;
  bool recording_ = false;
  bool heardSpeech_ = false;
  bool assistantAudioActive_ = false;
  bool homeCommandPending_ = false;
  uint32_t recordingStartedMs_ = 0;
  uint32_t lastSpeechMs_ = 0;
  uint32_t warningUntilMs_ = 0;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t sessionCounter_ = 0;
  String activeSessionId_;
  String webSocketHeaders_;
  HomeCommand pendingHomeCommand_;
};

}  // namespace aura
