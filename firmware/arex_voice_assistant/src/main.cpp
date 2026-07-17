#include <Arduino.h>

#include "ArexVoiceAssistant.h"

arex::ArexVoiceAssistant app;

void setup() { app.begin(); }

void loop() { app.loop(); }
