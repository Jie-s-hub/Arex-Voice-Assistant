#pragma once

#include <Arduino.h>

#if defined(AREX_USE_EXAMPLE_SECRETS)
#include "secrets.example.h"
#elif __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and configure it."
#endif

namespace arex::config {

// OLED / I2C
constexpr uint8_t kOledSdaPin = 21;
constexpr uint8_t kOledSclPin = 22;
constexpr uint8_t kOledAddress = 0x3C;

// Manual fallback trigger. The hands-free wake-word gate runs on the PC after
// this firmware streams a voice candidate.
constexpr uint8_t kVoiceButtonPin = 0;
constexpr bool kVoiceButtonActiveLow = true;
constexpr uint32_t kButtonDebounceMs = 35;
constexpr bool kWakeListenEnabled = true;
constexpr uint32_t kWakePollIntervalMs = 120;
constexpr uint16_t kWakeRmsThreshold = 700;
constexpr uint32_t kWakeCooldownMs = 2500;

// INMP441 on I2S_NUM_0 (receive)
constexpr uint8_t kMicBclkPin = 26;
constexpr uint8_t kMicWsPin = 27;
constexpr uint8_t kMicDataPin = 34;
constexpr uint32_t kMicSampleRate = 16000;
constexpr uint8_t kMicRightShift = 14;

// MAX98357A on I2S_NUM_1 (transmit)
constexpr uint8_t kSpeakerBclkPin = 14;
constexpr uint8_t kSpeakerLrcPin = 13;
constexpr uint8_t kSpeakerDataPin = 32;
constexpr uint32_t kSpeakerSampleRate = 24000;

constexpr uint32_t kRecordingMaxMs = 6000;
constexpr uint32_t kRecordingMinMs = 900;
constexpr uint32_t kSilenceToStopMs = 1100;
constexpr uint16_t kSpeechRmsThreshold = 450;
constexpr uint32_t kReconnectPeriodMs = 5000;

}  // namespace arex::config
