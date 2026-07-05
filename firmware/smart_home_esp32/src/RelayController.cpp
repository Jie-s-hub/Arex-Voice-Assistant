#include "RelayController.h"

#include "AppConfig.h"

namespace aura {

void RelayController::begin() {
  // Write the inactive level before switching the pins to outputs to minimize
  // relay chatter during boot.
  digitalWrite(home_config::kBedroomLightRelayPin,
               home_config::kRelayActiveLow ? HIGH : LOW);
  digitalWrite(home_config::kFanRelayPin,
               home_config::kRelayActiveLow ? HIGH : LOW);
  pinMode(home_config::kBedroomLightRelayPin, OUTPUT);
  pinMode(home_config::kFanRelayPin, OUTPUT);
  set("all", "off");
}

bool RelayController::set(const String& device, const String& state) {
  if (state != "on" && state != "off") {
    return false;
  }
  const bool on = state == "on";
  if (device == "bedroom_light") {
    bedroomLightOn_ = on;
    writeRelay(home_config::kBedroomLightRelayPin, on);
    return true;
  }
  if (device == "fan") {
    fanOn_ = on;
    writeRelay(home_config::kFanRelayPin, on);
    return true;
  }
  if (device == "all" && !on) {
    bedroomLightOn_ = false;
    fanOn_ = false;
    writeRelay(home_config::kBedroomLightRelayPin, false);
    writeRelay(home_config::kFanRelayPin, false);
    return true;
  }
  return false;
}

void RelayController::writeRelay(uint8_t pin, bool on) {
  const bool pinHigh = home_config::kRelayActiveLow ? !on : on;
  digitalWrite(pin, pinHigh ? HIGH : LOW);
}

}  // namespace aura

