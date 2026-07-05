#pragma once

#include <Arduino.h>

#if defined(AURA_USE_EXAMPLE_SECRETS)
#include "secrets.example.h"
#elif __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and configure it."
#endif

namespace aura::home_config {

constexpr uint8_t kBedroomLightRelayPin = 26;
constexpr uint8_t kFanRelayPin = 27;

// Most low-level-trigger relay boards are active LOW. Change this only after
// checking the relay module with no mains load connected.
constexpr bool kRelayActiveLow = true;
constexpr uint16_t kHttpPort = 80;
constexpr uint32_t kWifiRetryMs = 5000;

}  // namespace aura::home_config
