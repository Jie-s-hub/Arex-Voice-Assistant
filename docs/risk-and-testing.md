# Risk analysis and testing plan

## Risk register

| Risk | Likelihood | Impact | Control / mitigation |
|---|---|---:|---|
| HC-SR04 misses soft, angled, or very near object | Medium | High | Conservative threshold, pre-run check, slow demo speed, adult spotter; future bumper/second sensor |
| Existing drive loop overwrites the stop output | Medium during integration | Critical | Invoke tested stop callback and guard all non-zero motor writes while `obstacleActive()` |
| Motor noise resets ESP32 or corrupts audio | High without power work | High | Dedicated 5 V buck, star ground, bulk/ceramic decoupling, short I2S wiring, Wi-Fi RSSI check |
| 5 V Uno signal damages ESP32 | High if miswired | High | Mandatory 10 kΩ/20 kΩ divider; multimeter verification before connection |
| Single wire confuses voice and obstacle | Low after test | High | Widely separated pulse lengths, obstacle preemption, 100/50-event classification test; future separate wire |
| Wi-Fi or PC disappears | Medium | Medium | Uno safety is local; OLED/tone fallback; reconnect state machine; rehearsed offline mode |
| OpenAI latency/rate/API failure | Medium | Medium | Time-bounded capture, concise model settings, configurable models, deterministic known commands, visible error state |
| Hallucinated or unsafe smart-home action | Low | High | Strict schema, fixed device allowlist, Pydantic validation, node revalidation, `all/on` forbidden |
| Duplicate relay operation after retry | Medium | Low for set commands | Request IDs and eight-entry idempotency cache |
| Relay energizes at boot | Medium with active-low boards | High | Inactive level written before output mode, boot test with load disconnected, `all/off` initialization |
| Exposed API or node token | Medium | Medium | `.gitignore`, PC-only OpenAI key, separate random tokens, isolated WLAN; future TLS/certificates |
| Mains shock/fire | Avoidable | Critical | Prefer low-voltage demo loads; certified enclosure/fusing and qualified adult for any mains work |
| Speaker overcurrent/brownout | Medium | Medium | Regulated supply rated at least 2 A, start at low gain, thermal and voltage test |
| Battery misuse or short circuit | Low with supervision | Critical | Fuse, insulated connectors, correct charger/BMS, no unreviewed pack series/parallel connection |

## Automated PC tests

From `pc_server`:

```powershell
python -m pytest
```

The included tests verify:

- all five required phrases map to the correct allowlisted action;
- conversational wording is not accidentally treated as a device command;
- `all/on` and extra model-produced fields are rejected;
- PCM is wrapped into a standards-compliant mono WAV;
- maximum audio size and format constraints are enforced.

Add recorded noisy-room WAV fixtures later and run them against the configured transcription model as a separate, API-using evaluation; keep those out of the fast unit suite.

## Firmware static/build checks

Run in each PlatformIO project:

```powershell
pio run -d firmware/rover_esp32
pio run -d firmware/smart_home_esp32
```

Compile the Uno's real preserved sketch after adding `AuraSafety`; the example intentionally cannot know the existing motor function and pin map.

The reusable Uno module also has a compile/bench harness:

```powershell
pio run -d firmware/uno_integration
```

The directly integrated supplied mecanum sketch builds with:

```powershell
pio run -d firmware/uno_mecanum_aura
```

## Hardware-in-the-loop tests

### Obstacle safety matrix

With wheels raised first, repeat for forward, reverse, strafe left/right, and both rotations:

1. Command motion.
2. Present a flat obstacle inside the threshold.
3. Confirm all four outputs go to zero.
4. Hold the controller command; motors must remain stopped.
5. Remove the object; motors must not move until a new permitted operator command.

Repeat with the PC off and Wi-Fi off. The stop result must be identical.

Target measurements:

- Uno stop: no later than one 50 ms sample interval plus sonar echo time after the obstacle becomes measurable.
- ESP32 warning face/tone: within 250 ms of the Uno event rising.
- Zero false voice triggers during 50 obstacle events.

### One-wire classification

- 100 PS2 voice-button presses: 100 Listening states, zero Warning states.
- 50 obstacle insertions: 50 Warning states, zero Listening states.
- Insert an obstacle during the 40 ms voice pattern: Warning wins and motors remain stopped.
- Reset either controller with the line connected: no uncommanded motor motion.

### Audio

- Quiet speech at 0.5 m, normal speech at 1 m, and speech with motors idling.
- Silence produces no relay command.
- Six-second maximum always closes the capture.
- Speaker response is intelligible at the demo distance with no reset.
- Obstacle during Listening/Thinking/Speaking cancels the ordinary interaction and prioritizes warning.

### Smart home

Test request matrix:

| Case | Expected |
|---|---|
| Valid light/fan on/off | HTTP 200 and correct relay |
| `all/off` | Both off |
| `all/on` | HTTP 422, no change |
| Unknown device | HTTP 422, no change |
| Missing/wrong token | HTTP 401, no change |
| Malformed or >1 KB body | HTTP 400, no change |
| Same request ID twice | Second response has `duplicate: true` |
| Node reset | Both relays remain/go off |

## End-to-end acceptance tests

The project is demo-ready only when all of these pass on battery power:

- Existing PS2 mecanum actions behave exactly as before integration.
- Each required spoken command actuates only its intended relay.
- “Turn everything off” de-energizes both outputs.
- A general question receives an intelligible answer and actuates nothing.
- All six OLED expressions appear in their correct states.
- Every close obstacle stops the rover with the PC disconnected.
- The exact spoken warning is heard when the PC/API is available; the local tone remains when it is not.
- Bad Wi-Fi, bad token, OpenAI error, silent audio, and home-node outage each fail visibly without unsafe actuation.
- Three cold-start demo rehearsals pass consecutively.
