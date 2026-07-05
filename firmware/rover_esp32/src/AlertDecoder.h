#pragma once

#include <Arduino.h>

namespace aura {

enum class UnoEvent : uint8_t { None, VoiceRequest, Obstacle };

class AlertDecoder {
 public:
  explicit AlertDecoder(uint8_t pin) : pin_(pin) {}
  void begin();
  UnoEvent update(uint32_t nowMs);
  bool obstacleActive() const { return obstacleEmitted_ && wasHigh_; }

 private:
  static constexpr uint16_t kLongPulseMs = 150;
  static constexpr uint16_t kShortMaxMs = 100;
  static constexpr uint16_t kSecondPulseWindowMs = 220;

  uint8_t pin_;
  bool wasHigh_ = false;
  bool obstacleEmitted_ = false;
  uint8_t shortPulseCount_ = 0;
  uint32_t highSinceMs_ = 0;
  uint32_t firstShortEndedMs_ = 0;
};

}  // namespace aura
