# ESP32-only hardware test

This test checks the rover ESP32 and its new components without connecting the Arduino Uno or motors. It can also test the complete PC server and OpenAI voice path.

Test firmware:

```text
firmware/rover_esp32_bench_test
```

## Hardware required

- ESP32-WROOM-32 development board
- 1.3-inch I2C OLED
- INMP441 microphone
- MAX98357A amplifier
- 3 W, 4 Ω speaker
- Regulated 5 V supply
- USB data cable
- Optional 1 kΩ resistor/jumper for the GPIO25 test

The HC-SR04 remains an Uno-only sensor and is not used in this ESP32 bench test.

## Wiring

### OLED

| OLED | ESP32 |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

### INMP441

| INMP441 | ESP32 |
|---|---:|
| VDD | 3V3 |
| GND | GND |
| SCK/BCLK | GPIO26 |
| WS/LRCL | GPIO27 |
| SD | GPIO34 |
| L/R | GND |

### MAX98357A

| MAX98357A | ESP32 / supply |
|---|---:|
| VIN | Regulated 5V |
| GND | GND |
| BCLK | GPIO14 |
| LRC | GPIO13 |
| DIN | GPIO32 |
| SPK+ | Speaker + |
| SPK− | Speaker − |

Neither speaker wire connects to ground.

## Build and upload

First copy:

```text
firmware/rover_esp32_bench_test/include/secrets.example.h
```

to:

```text
firmware/rover_esp32_bench_test/include/secrets.h
```

Set the Wi-Fi name/password, PC LAN IPv4 address, and rover token. The token must match `ROVER_TOKEN` in `pc_server/.env`.

Open PowerShell in the AURA project directory:

```powershell
pio run -d firmware/rover_esp32_bench_test
pio run -d firmware/rover_esp32_bench_test --target upload
pio device monitor --baud 115200
```

Or open `firmware/rover_esp32_bench_test` in PlatformIO and use **Build**, **Upload**, and **Serial Monitor**.

## Serial test menu

Enter one letter in the Serial Monitor:

| Command | Test |
|---|---|
| `O` | Cycles through all six OLED faces |
| `M` | Shows and prints the INMP441 RMS level for eight seconds |
| `S` | Plays 440 Hz, 660 Hz, and 880 Hz speaker tones |
| `W` | Scans nearby Wi-Fi networks without needing a password |
| `A` | Waits for a safe 3.3 V HIGH on GPIO25 |
| `R` | Runs OLED, speaker, microphone, and Wi-Fi tests |
| `C` | Connects to Wi-Fi and tests the PC WebSocket server |
| `V` | Records five seconds and runs the complete OpenAI voice round trip |
| `H` | Prints the menu again |

## Expected results

### OLED

All faces should be readable and centered:

```text
Idle       (^_^)
Listening  (O_O)
Thinking   (-_-)
Speaking   (^o^)
Warning    (>_<)
Error      (X_X)
```

If the display is blank, check its I2C address and whether it uses SH1106 or SSD1306.

### Microphone

Run `M`, then alternate between silence, speaking, and clapping.

- The RMS number should change substantially.
- The OLED bar should rise with louder sound.
- A constant zero usually means incorrect SD, WS, SCK, power, or L/R wiring.
- Random maximum values usually indicate a floating/miswired microphone data line.

### Speaker

Run `S`. You should hear three ascending tones. If not:

- confirm MAX98357A VIN receives regulated 5 V;
- confirm ESP32 and amplifier grounds are connected;
- confirm BCLK, LRC, and DIN pins;
- confirm the speaker connects only between SPK+ and SPK−.

### Wi-Fi

Run `W`. Finding at least one network confirms that the ESP32 Wi-Fi radio can scan. This test does not connect to a network or require credentials.

### GPIO25 alert input

Run `A`, then connect:

```text
ESP32 3V3 -> 1 kΩ resistor -> GPIO25
```

Hold it for at least 0.2 seconds. The display should show `(>_<)` and the speaker should play the warning tones.

Never connect GPIO25 to 5 V. When the Uno is added later, its D7 signal must pass through the documented 10 kΩ/20 kΩ divider.

## Test the PC server and OpenAI key

### 1. Configure the PC

From the project directory:

```powershell
cd pc_server
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -e ".[dev]"
Copy-Item .env.example .env
```

Edit `.env`:

```dotenv
OPENAI_API_KEY=sk-your-real-openai-key
OPENAI_TRANSCRIBE_MODEL=gpt-4o-mini-transcribe
OPENAI_CHAT_MODEL=gpt-5.5
OPENAI_TTS_MODEL=gpt-4o-mini-tts
OPENAI_TTS_VOICE=coral
ROVER_TOKEN=the-same-long-token-used-in-esp32-secrets
AURA_HOST=0.0.0.0
AURA_PORT=8000
```

The OpenAI key stays on the PC. Never copy it into ESP32 firmware.

### 2. Verify the server locally

Run:

```powershell
python -m pytest
aura-server
```

Open:

```text
http://127.0.0.1:8000/health
```

`openai_key_configured` should be `true`. This confirms that a value is configured, but only a real voice request proves the key is valid and has model access.

### 3. Test the ESP32-to-PC connection

In the ESP32 Serial Monitor, enter:

```text
C
```

Expected result:

```text
WebSocket connected to AURA PC server.
PC CONNECTION TEST PASS.
```

The `/health` result should list `aura-esp32-bench` in `connected_rovers`.

If it fails, check:

- the PC and ESP32 are on the same Wi-Fi network;
- `TEST_PC_HOST` is the PC's LAN IPv4 address, not `127.0.0.1`;
- the tokens match exactly;
- the PC server is running;
- Windows Firewall allows Python on the private network.

### 4. Test the real OpenAI key and voice pipeline

Enter:

```text
V
```

When the OLED shows `(O_O)`, say a short question such as:

```text
What is a mecanum wheel?
```

The test records for five seconds. Expected sequence:

1. Serial Monitor prints changing microphone RMS values.
2. OLED changes from Listening `(O_O)` to Thinking `(-_-)`.
3. Serial Monitor prints `TRANSCRIPT:` with what you said.
4. Serial Monitor prints `AURA RESPONSE:`.
5. OLED shows Speaking `(^o^)`.
6. The response plays through the MAX98357A speaker.
7. Serial Monitor prints `FULL OPENAI VOICE TEST PASS`.

This successful test verifies:

- ESP32 microphone capture;
- Wi-Fi and WebSocket communication;
- PC server authentication;
- the OpenAI API key;
- speech-to-text model access;
- Responses API model access;
- text-to-speech model access;
- return audio and ESP32 speaker playback.

If the key is invalid or a model is unavailable, the Serial Monitor prints the bounded server error returned by the PC.
