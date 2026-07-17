#include "EmotionDisplay.h"

#include <Wire.h>

#include "AppConfig.h"

namespace arex {

EmotionDisplay::EmotionDisplay()
    : oled_(U8G2_R0, U8X8_PIN_NONE) {}

void EmotionDisplay::begin() {
  Wire.begin(config::kOledSdaPin, config::kOledSclPin);
  oled_.setI2CAddress(config::kOledAddress << 1);  // U8g2 uses 8-bit address.
  oled_.begin();
  show(Emotion::Idle, "Arex ready");
}

void EmotionDisplay::show(Emotion emotion, const char* detail) {
  current_ = emotion;
  oled_.clearBuffer();

  oled_.setFont(u8g2_font_courB18_tf);
  const char* face = faceFor(emotion);
  const int16_t faceWidth = oled_.getStrWidth(face);
  oled_.drawStr((128 - faceWidth) / 2, 30, face);

  oled_.setFont(u8g2_font_6x10_tf);
  const char* label = detail != nullptr ? detail : labelFor(emotion);
  const int16_t labelWidth = oled_.getStrWidth(label);
  const int16_t labelX = (128 - labelWidth) / 2;
  oled_.drawStr(labelX > 0 ? labelX : 0, 55, label);
  oled_.sendBuffer();
}

const char* EmotionDisplay::faceFor(Emotion emotion) const {
  switch (emotion) {
    case Emotion::Idle: return "(^_^)";
    case Emotion::Listening: return "(O_O)";
    case Emotion::Thinking: return "(-_-)";
    case Emotion::Speaking: return "(^o^)";
    case Emotion::Error: return "(X_X)";
  }
  return "(X_X)";
}

const char* EmotionDisplay::labelFor(Emotion emotion) const {
  switch (emotion) {
    case Emotion::Idle: return "Idle";
    case Emotion::Listening: return "Listening";
    case Emotion::Thinking: return "Thinking";
    case Emotion::Speaking: return "Speaking";
    case Emotion::Error: return "Error";
  }
  return "Error";
}

}  // namespace arex
