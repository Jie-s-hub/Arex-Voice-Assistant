# Step-by-step implementation roadmap

Each stage has an exit condition. Do not combine stages during first bring-up; small, boring victories are how robots stay out of walls.

## Stage 0 — Preserve the working rover

1. Save the current Uno sketch and photograph every existing connection.
2. Record the PS2X, L298N, PWM, and direction pin map.
3. Put the rover on a stand and confirm all existing motions still work.
4. Identify the existing function or statements that set all four motors to zero.

Exit: the untouched backup builds, uploads, and drives correctly.

## Stage 1 — Power and grounding

1. Install the fused 5 V buck supply with no ESP32 connected.
2. Verify 5.0 V at idle and while all wheels change direction.
3. Add the star ground and local capacitors.
4. Scope or meter the rail for resets/dips if equipment is available.

Exit: stable 5 V under motor transients and no hot components.

## Stage 2 — Uno sonar safety only

1. Install NewPing and copy `AuraSafety.*` into the existing project or Arduino libraries folder.
2. Connect only the HC-SR04 to the Uno.
3. Replace the example stop callback with the real existing stop function.
4. Call `auraSafety.update(...)` every loop and prevent non-zero motor writes while `obstacleActive()` is true.
5. Print distance temporarily and tune 25 cm/32 cm for the rover's stopping distance.

Exit: at ten approach angles and speeds, the first close reading stops every motor; removing the object never causes uncommanded motion.

## Stage 3 — One-wire link

1. Build the 10 kΩ/20 kΩ divider and verify 3.3 V at its node.
2. Connect D7 to GPIO25 through the divider.
3. Upload rover ESP32 firmware with microphone/amplifier temporarily disconnected.
4. Press the chosen PS2 button and confirm `(O_O)`; present an obstacle and confirm `(>_<)`.
5. If available, capture both waveforms with a logic analyzer.

Exit: 100 consecutive voice triggers and 50 obstacle triggers classify correctly, with no cross-classifications.

## Stage 4 — OLED and microphone

1. Verify OLED address with an I2C scanner.
2. Confirm all six faces are centered and readable.
3. Connect the INMP441, keeping wires short and away from motors.
4. Watch RMS values temporarily over Serial and tune `kSpeechRmsThreshold` in the actual room.

Exit: voice starts reliably, silence ends a turn, and motor noise alone does not constantly count as speech.

## Stage 5 — Speaker

1. Connect MAX98357A to regulated 5 V and its separate I2S pins.
2. Start at low acoustic gain; verify the two-tone warning.
3. Confirm there is no ESP32 brownout at maximum planned volume.

Exit: warning is audible without resets, severe distortion, or hot parts.

## Stage 6 — Smart-home node with safe loads

1. Configure its Wi-Fi and token, then flash it with relay loads disconnected.
2. Confirm both relay outputs are OFF during reset and boot.
3. Use a low-voltage lamp/fan and POST each command with PowerShell or curl.
4. Test invalid token, malformed JSON, invalid device, `all/on`, and duplicate request IDs.

Exit: only authenticated allowlisted commands operate the safe test loads.

## Stage 7 — PC server without the rover

1. Create the virtual environment and install `.[dev]`.
2. Run `pytest`.
3. Fill `.env`; open `/health`.
4. Use a WebSocket test client or recorded PCM fixture to exercise start/binary/end framing.
5. Verify the OpenAI account can access the configured models before demo day.

Exit: a recorded command produces transcript, valid decision JSON, PCM speech, and no API key in logs.

## Stage 8 — Full integration

1. Set the PC LAN IP and matching tokens in the rover firmware.
2. Keep wheels raised. Exercise all five commands, general conversation, silence, Wi-Fi loss, and an obstacle during each voice state.
3. Lower the rover only after the safety tests pass.
4. Measure end-to-end latency and tune capture silence, Wi-Fi placement, and TTS volume.

Exit: all acceptance tests in `risk-and-testing.md` pass.

## Stage 9 — Competition hardening

1. Use an isolated travel router with fixed DHCP reservations.
2. Label cables and carry tested spares: USB cables, buck converter, HC-SR04, ESP32, divider resistors.
3. Create a one-command PC startup shortcut and disable sleep/automatic updates during the demo window.
4. Run a cold-start rehearsal from batteries and a powered-off PC.
5. Prepare a graceful offline demonstration: PS2 drive, sonar stop, OLED warning, and relay-node local HTTP test.

Exit: three complete cold-start rehearsals succeed without editing code or wiring.

