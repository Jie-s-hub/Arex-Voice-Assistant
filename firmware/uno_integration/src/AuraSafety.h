#pragma once

#include <Arduino.h>
#include <NewPing.h>

namespace aura {

using StopCallback = void (*)();

struct SafetyConfig {
  uint8_t triggerPin = 8;
  uint8_t echoPin = 9;
  uint8_t alertPin = 7;
  uint16_t stopDistanceCm = 25;
  uint16_t clearDistanceCm = 32;
  uint16_t maxDistanceCm = 200;
  uint16_t samplePeriodMs = 50;
};

// One-wire encoding on alertPin:
//   voice request: 40 ms HIGH, 40 ms LOW, 40 ms HIGH
//   obstacle:      HIGH for at least 500 ms and until the obstacle clears
// The ESP32 recognizes a long HIGH as an obstacle. The Uno stops the motors
// immediately; the wire is notification, not part of the stop path.
class SafetyController {
 public:
  SafetyController(const SafetyConfig& config, StopCallback stopCallback);

  void begin();
  void update(bool voiceButtonPressed);

  bool obstacleActive() const { return obstacleActive_; }
  uint16_t lastDistanceCm() const { return lastDistanceCm_; }

 private:
  enum class SignalState : uint8_t {
    Idle,
    VoiceHigh1,
    VoiceGap,
    VoiceHigh2,
    ObstacleHigh,
  };

  void updateSonar(uint32_t nowMs);
  void updateSignal(uint32_t nowMs);
  void requestVoiceSignal(uint32_t nowMs);
  void requestObstacleSignal(uint32_t nowMs);
  void setAlert(bool high);

  static constexpr uint16_t kVoiceHighMs = 40;
  static constexpr uint16_t kVoiceGapMs = 40;
  static constexpr uint16_t kObstacleHighMs = 500;

  SafetyConfig config_;
  StopCallback stopCallback_;
  NewPing sonar_;

  uint32_t lastSampleMs_ = 0;
  uint32_t signalSinceMs_ = 0;
  uint16_t lastDistanceCm_ = 0;
  uint8_t clearSamples_ = 0;
  bool obstacleActive_ = false;
  bool voiceButtonWasPressed_ = false;
  SignalState signalState_ = SignalState::Idle;
};

}  // namespace aura
