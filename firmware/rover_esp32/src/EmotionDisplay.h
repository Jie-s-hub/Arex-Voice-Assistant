#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

namespace aura {

enum class Emotion : uint8_t {
  Idle,
  Listening,
  Thinking,
  Speaking,
  Warning,
  Error,
};

class EmotionDisplay {
 public:
  EmotionDisplay();
  void begin();
  void show(Emotion emotion, const char* detail = nullptr);
  Emotion current() const { return current_; }

 private:
  const char* faceFor(Emotion emotion) const;
  const char* labelFor(Emotion emotion) const;

  U8G2_SH1106_128X64_NONAME_F_HW_I2C oled_;
  Emotion current_ = Emotion::Idle;
};

}  // namespace aura

