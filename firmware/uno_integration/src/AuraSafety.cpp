#include "AuraSafety.h"

namespace aura {

SafetyController::SafetyController(const SafetyConfig& config,
                                   StopCallback stopCallback)
    : config_(config),
      stopCallback_(stopCallback),
      sonar_(config.triggerPin, config.echoPin, config.maxDistanceCm) {}

void SafetyController::begin() {
  pinMode(config_.alertPin, OUTPUT);
  setAlert(false);
  lastSampleMs_ = millis() - config_.samplePeriodMs;
}

void SafetyController::update(bool voiceButtonPressed) {
  const uint32_t nowMs = millis();
  updateSonar(nowMs);

  // Rising-edge only: holding the PS2 button cannot queue repeated recordings.
  if (voiceButtonPressed && !voiceButtonWasPressed_ && !obstacleActive_) {
    requestVoiceSignal(nowMs);
  }
  voiceButtonWasPressed_ = voiceButtonPressed;

  if (obstacleActive_ && stopCallback_ != nullptr) {
    // Keep enforcing zero motor output while the object remains too close.
    stopCallback_();
  }
  updateSignal(nowMs);
}

void SafetyController::updateSonar(uint32_t nowMs) {
  if (static_cast<uint32_t>(nowMs - lastSampleMs_) < config_.samplePeriodMs) {
    return;
  }
  lastSampleMs_ = nowMs;

  // NewPing returns zero for no echo. Zero is therefore not treated as a
  // nearby object; wiring failures must be detected during the pre-run check.
  lastDistanceCm_ = sonar_.ping_cm();
  const bool tooClose =
      lastDistanceCm_ > 0 && lastDistanceCm_ <= config_.stopDistanceCm;

  if (tooClose) {
    clearSamples_ = 0;
    if (stopCallback_ != nullptr) {
      stopCallback_();  // First close reading immediately reaches the motors.
    }
    if (!obstacleActive_) {
      obstacleActive_ = true;
      requestObstacleSignal(nowMs);
    }
    return;
  }

  if (!obstacleActive_) {
    return;
  }

  // Require three clear readings beyond a hysteresis band before releasing
  // the stop latch. The existing PS2 loop must still issue a new drive command.
  if (lastDistanceCm_ == 0 || lastDistanceCm_ >= config_.clearDistanceCm) {
    if (++clearSamples_ >= 3) {
      obstacleActive_ = false;
      clearSamples_ = 0;
    }
  } else {
    clearSamples_ = 0;
  }
}

void SafetyController::requestVoiceSignal(uint32_t nowMs) {
  if (signalState_ != SignalState::Idle) {
    return;
  }
  signalState_ = SignalState::VoiceHigh1;
  signalSinceMs_ = nowMs;
  setAlert(true);
}

void SafetyController::requestObstacleSignal(uint32_t nowMs) {
  // Safety always preempts an in-progress voice pattern.
  signalState_ = SignalState::ObstacleHigh;
  signalSinceMs_ = nowMs;
  setAlert(true);
}

void SafetyController::updateSignal(uint32_t nowMs) {
  const uint32_t elapsed = nowMs - signalSinceMs_;
  switch (signalState_) {
    case SignalState::Idle:
      break;
    case SignalState::VoiceHigh1:
      if (elapsed >= kVoiceHighMs) {
        setAlert(false);
        signalState_ = SignalState::VoiceGap;
        signalSinceMs_ = nowMs;
      }
      break;
    case SignalState::VoiceGap:
      if (elapsed >= kVoiceGapMs) {
        setAlert(true);
        signalState_ = SignalState::VoiceHigh2;
        signalSinceMs_ = nowMs;
      }
      break;
    case SignalState::VoiceHigh2:
      if (elapsed >= kVoiceHighMs) {
        setAlert(false);
        signalState_ = SignalState::Idle;
      }
      break;
    case SignalState::ObstacleHigh:
      if (elapsed >= kObstacleHighMs && !obstacleActive_) {
        setAlert(false);
        signalState_ = SignalState::Idle;
      }
      break;
  }
}

void SafetyController::setAlert(bool high) {
  digitalWrite(config_.alertPin, high ? HIGH : LOW);
}

}  // namespace aura
