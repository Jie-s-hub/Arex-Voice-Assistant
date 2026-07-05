#include "AlertDecoder.h"

namespace aura {

void AlertDecoder::begin() {
  // The divider's 20 kOhm lower resistor is also the external pull-down.
  pinMode(pin_, INPUT);
  wasHigh_ = digitalRead(pin_) == HIGH;
  highSinceMs_ = millis();
}

UnoEvent AlertDecoder::update(uint32_t nowMs) {
  const bool high = digitalRead(pin_) == HIGH;

  if (high && !wasHigh_) {
    highSinceMs_ = nowMs;
    obstacleEmitted_ = false;
  }

  // A long pulse is classified before it ends. This adds at most 150 ms to the
  // ESP32 warning only; the Uno motor stop already happened synchronously.
  if (high && !obstacleEmitted_ &&
      static_cast<uint32_t>(nowMs - highSinceMs_) >= kLongPulseMs) {
    obstacleEmitted_ = true;
    shortPulseCount_ = 0;
    wasHigh_ = high;
    return UnoEvent::Obstacle;
  }

  if (!high && wasHigh_ && !obstacleEmitted_) {
    const uint32_t widthMs = nowMs - highSinceMs_;
    if (widthMs <= kShortMaxMs) {
      if (shortPulseCount_ == 0) {
        shortPulseCount_ = 1;
        firstShortEndedMs_ = nowMs;
      } else if (nowMs - firstShortEndedMs_ <= kSecondPulseWindowMs) {
        shortPulseCount_ = 0;
        wasHigh_ = high;
        return UnoEvent::VoiceRequest;
      }
    } else {
      shortPulseCount_ = 0;
    }
  }

  if (shortPulseCount_ == 1 &&
      nowMs - firstShortEndedMs_ > kSecondPulseWindowMs) {
    shortPulseCount_ = 0;
  }

  wasHigh_ = high;
  return UnoEvent::None;
}

}  // namespace aura
