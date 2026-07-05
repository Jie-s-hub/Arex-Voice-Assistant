#include <Arduino.h>
#include <AuraSafety.h>

// Compile/bench harness only. It proves the reusable safety module without
// pretending to know or replace the real rover's existing mecanum pin map.
constexpr uint8_t kBenchStopLed = LED_BUILTIN;
constexpr uint8_t kBenchVoiceButton = A0;  // button from A0 to GND

void benchStop() { digitalWrite(kBenchStopLed, HIGH); }

aura::SafetyConfig config;
aura::SafetyController safety(config, benchStop);

void setup() {
  pinMode(kBenchStopLed, OUTPUT);
  pinMode(kBenchVoiceButton, INPUT_PULLUP);
  safety.begin();
}

void loop() {
  digitalWrite(kBenchStopLed, LOW);
  safety.update(digitalRead(kBenchVoiceButton) == LOW);
}

