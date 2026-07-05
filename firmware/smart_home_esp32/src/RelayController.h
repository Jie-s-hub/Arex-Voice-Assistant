#pragma once

#include <Arduino.h>

namespace aura {

class RelayController {
 public:
  void begin();
  bool set(const String& device, const String& state);
  bool bedroomLightOn() const { return bedroomLightOn_; }
  bool fanOn() const { return fanOn_; }

 private:
  void writeRelay(uint8_t pin, bool on);
  bool bedroomLightOn_ = false;
  bool fanOn_ = false;
};

}  // namespace aura

