#include <Arduino.h>

#include "AudioAssistantApp.h"

aura::AudioAssistantApp app;

void setup() { app.begin(); }

void loop() { app.loop(); }
