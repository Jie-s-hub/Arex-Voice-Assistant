#include <PS2X_lib.h>
#include <AuraSafety.h>

// This file demonstrates the ONLY additions required in the working rover
// sketch. Keep the existing PS2 setup, motor pin map, and mecanum calculations.

extern PS2X ps2x;  // Use the PS2X instance already present in the rover sketch.

void stopAllMotorsForSafety() {
  // Replace this body with the existing project's tested stop function.
  // It must set all four motor PWM/enable outputs to zero immediately.
}

aura::SafetyConfig safetyConfig{
    8,    // HC-SR04 TRIG: change if this conflicts with the existing rover
    9,    // HC-SR04 ECHO
    7,    // Uno -> divider -> ESP32 GPIO25
    25,   // stop distance in centimetres
    32,   // clear distance (hysteresis)
    200,  // maximum measured distance
    50,   // sonar sample period
};

aura::SafetyController auraSafety(safetyConfig, stopAllMotorsForSafety);

void setup() {
  // Existing PS2 and motor setup remains here.
  auraSafety.begin();
}

void loop() {
  // Existing ps2x.read_gamepad(...) and mecanum update remain here.

  // Choose one otherwise-unused PS2 button. This example uses TRIANGLE.
  const bool voicePressed = ps2x.Button(PSB_TRIANGLE);
  auraSafety.update(voicePressed);

  // Existing drive loop continues. If obstacleActive() is true, do not apply
  // new non-zero drive outputs; the module also calls the stop callback every
  // loop as a final guard.
  if (auraSafety.obstacleActive()) {
    return;
  }
}

