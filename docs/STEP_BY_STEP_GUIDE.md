# AURA Rover — Step-by-Step Build Guide

This guide explains how to turn the existing PS2-controlled mecanum rover into AURA without redesigning its drive system.

> **Code-specific update:** The supplied `mecanum_car_v2.3.ino` already uses D7, D8, and D9 for the right-front motor. Do not use the earlier generic D8/D9 sonar plan. Use the integrated `mecanum_car_v2.4_aura.ino`, move only the right-front IN1 wire from D7 to D3, and connect the HC-SR04 in one-pin mode on D11 as described below.

Work through the stages in order. Do not connect mains-voltage appliances during development. Use a low-voltage lamp and fan for the competition prototype.

## 1. Prepare the tools

### Software to install

Install these on the PC:

1. [Visual Studio Code](https://code.visualstudio.com/)
2. The **PlatformIO IDE** extension in Visual Studio Code
3. Python 3.11 or newer
4. Git, if you want version control

You also need:

- A data-capable USB cable for each ESP32
- The Arduino Uno USB cable
- A multimeter
- Breadboard and jumper wires
- 10 kΩ and 20 kΩ resistors
- A regulated 5 V buck converter rated for at least 2 A
- A safe low-voltage lamp and fan for relay testing

## 2. Back up the existing rover

Before changing anything:

1. Open the current Arduino Uno mecanum sketch.
2. Save a copy named something like `mecanum_rover_original.ino`.
3. Photograph all existing Uno, PS2, L298N, and motor connections.
4. Write down every occupied Uno pin.
5. Upload the original program and confirm the rover still drives correctly.

Do not continue until the untouched backup works.

## 3. Find the existing motor-stop code

The AURA safety module must call the rover's existing stop function.

Look for code similar to:

```cpp
analogWrite(motor1Enable, 0);
analogWrite(motor2Enable, 0);
analogWrite(motor3Enable, 0);
analogWrite(motor4Enable, 0);
```

It may instead be called:

```cpp
stopMotors();
stopAllMotors();
motorStop();
```

If there is no function yet, place the existing tested stop statements inside one:

```cpp
void stopAllMotorsForSafety() {
  // Paste the existing commands that stop all four motors here.
}
```

Do not change the mecanum calculations or motor direction logic.

## 4. Make the required pin change

Your original sketch uses D7 for the right-front L298N `IN1` signal. Move that one physical wire:

```text
Before: Uno D7 -> right-front L298N IN1
After:  Uno D3 -> right-front L298N IN1
```

The final added-function pins are:

| Function | Uno pin |
|---|---:|
| Right-front L298N IN1 | D3, moved from D7 |
| HC-SR04 shared trigger/echo | D11 |
| ESP32 alert output | D7 |

Leave all other existing motor and PS2 wires unchanged.

## 5. Wire the HC-SR04 to the Uno

The Uno has only one safe pin left, so use NewPing's one-pin mode:

| HC-SR04 | Connection |
|---|---|
| VCC | Uno 5V |
| GND | Uno GND |
| TRIG | Uno D11 directly |
| ECHO | Uno D11 through a 2.2 kΩ resistor |

Do not connect ECHO directly to the shared D11 node without the resistor. Keep the sensor facing forward and away from the spinning wheels.

## 6. Build the Uno-to-ESP32 voltage divider

Never connect the Uno's 5 V D7 output directly to an ESP32 pin.

Build this divider:

```text
Uno D7 -------- 10 kΩ --------+-------- ESP32 GPIO25
                              |
                            20 kΩ
                              |
Shared GND -------------------+
```

Before connecting GPIO25:

1. Power the Uno.
2. Temporarily make D7 HIGH with a test sketch if necessary.
3. Measure the divider output with a multimeter.
4. It should be approximately 3.3 V.
5. Connect the divider output to ESP32 GPIO25 only after confirming the voltage.

Connect Uno GND and rover ESP32 GND together.

## 7. Upload the integrated Uno sketch

Your directly integrated code is here:

```text
firmware/uno_mecanum_aura/mecanum_car_v2.4_aura/
  mecanum_car_v2.4_aura.ino
```

1. Install the **PS2X** and **NewPing** libraries in Arduino IDE.
2. Open `mecanum_car_v2.4_aura.ino`.
3. Select Arduino Uno and the correct COM port.
4. Upload the sketch.
5. Open Serial Monitor at 9600 baud.

The original movement buttons are unchanged. The previously unused **L1** button is the dedicated AURA voice button.

Full details of every pin and code change are in `docs/YOUR_MECANUM_CODE_INTEGRATION.md`.

## 8. Test obstacle stopping before adding AI

Raise the rover so none of the wheels touches the floor.

1. Upload the modified Uno sketch.
2. Command forward movement with the PS2 controller.
3. Place a flat object within 25 cm of the HC-SR04.
4. Confirm all four motors stop.
5. Keep holding the PS2 movement control. The motors must remain stopped.
6. Remove the object.
7. Confirm the rover does not move until a new permitted command is issued.
8. Repeat for reverse, strafing, and rotation.

Do not place the rover on the floor until this test passes consistently.

## 9. Prepare the rover ESP32 power supply

Use the rover battery only through a regulated buck converter:

```text
7.4 V battery → fuse/switch → 5 V buck converter
```

Connect the regulated 5 V output to:

- ESP32 `5V` or `VIN`
- MAX98357A `VIN`

Use the ESP32's `3V3` pin for:

- INMP441 microphone
- OLED display

Do not power the ESP32 from the L298N's onboard 5 V output.

Connect all logic grounds at a common ground point.

## 10. Wire the OLED

| OLED | Rover ESP32 |
|---|---:|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

The firmware expects address `0x3C` and an SH1106 128×64 display.

If the screen stays blank:

1. Run an I2C scanner.
2. Check whether the address is `0x3C` or `0x3D`.
3. Check whether the controller is SH1106 or SSD1306.
4. Follow the OLED note in `docs/wiring.md` if it is SSD1306.

## 11. Wire the INMP441 microphone

| INMP441 | Rover ESP32 |
|---|---:|
| VDD | 3V3 |
| GND | GND |
| SCK/BCLK | GPIO26 |
| WS/LRCL | GPIO27 |
| SD | GPIO34 |
| L/R | GND |

Keep microphone wires short and away from the motors and L298N wiring.

## 12. Wire the MAX98357A and speaker

| MAX98357A | Rover ESP32 / power |
|---|---:|
| VIN | Regulated 5V |
| GND | GND |
| BCLK | GPIO14 |
| LRC | GPIO13 |
| DIN | GPIO32 |
| SPK+ | Speaker + |
| SPK− | Speaker − |

Important: neither speaker wire connects to ground. The MAX98357A uses a bridged speaker output.

Start with a low amplifier gain or low volume.

## 13. Configure the rover ESP32 firmware

Open this folder in PlatformIO:

```text
firmware/rover_esp32
```

Copy:

```text
include/secrets.example.h
```

to:

```text
include/secrets.h
```

Edit `secrets.h`:

```cpp
constexpr char WIFI_SSID[] = "YOUR_WIFI_NAME";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

constexpr char PC_SERVER_HOST[] = "192.168.1.50";
constexpr uint16_t PC_SERVER_PORT = 8000;
constexpr char ROVER_DEVICE_ID[] = "aura-rover-01";
constexpr char ROVER_TOKEN[] = "use-a-long-random-token";

constexpr char HOME_NODE_BASE_URL[] = "http://192.168.1.60";
constexpr char HOME_NODE_TOKEN[] = "use-another-long-random-token";
```

Replace the IP addresses with the real PC and smart-home ESP32 addresses.

To find the PC's address in PowerShell:

```powershell
ipconfig
```

Use the IPv4 address of the Wi-Fi adapter. Do not use `127.0.0.1` or `localhost` in ESP32 firmware.

## 14. Build and upload the rover ESP32 firmware

Using PlatformIO in Visual Studio Code:

1. Connect the rover ESP32 by USB.
2. Open `firmware/rover_esp32`.
3. Click **Build**.
4. Click **Upload**.
5. Open the Serial Monitor at `115200` baud.

Or use PowerShell:

```powershell
cd firmware\rover_esp32
pio run
pio run --target upload
pio device monitor --baud 115200
```

Expected behavior:

- OLED starts with `(^_^)`.
- Pressing the selected PS2 button shows `(O_O)`.
- An obstacle shows `(>_<)` and plays the local warning tone.

The PC is not required for the OLED obstacle warning or local tone.

## 15. Wire the smart-home ESP32

Use a two-channel relay module.

| Relay module | Smart-home ESP32 |
|---|---:|
| IN1 | GPIO26 |
| IN2 | GPIO27 |
| GND | GND |
| VCC/JD-VCC | Supply required by the relay board |

Logical assignments:

- IN1: bedroom light
- IN2: fan

Leave the appliance terminals disconnected during the first test.

The firmware assumes an active-LOW relay module. If the relays turn on during boot, disconnect the loads immediately and review `kRelayActiveLow` in:

```text
firmware/smart_home_esp32/include/AppConfig.h
```

## 16. Configure and upload the smart-home firmware

Open:

```text
firmware/smart_home_esp32
```

Copy `include/secrets.example.h` to `include/secrets.h`, then edit:

```cpp
constexpr char WIFI_SSID[] = "YOUR_WIFI_NAME";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char HOME_NODE_TOKEN[] = "use-another-long-random-token";
```

`HOME_NODE_TOKEN` must exactly match the value in the rover ESP32 firmware.

Build and upload:

```powershell
cd firmware\smart_home_esp32
pio run
pio run --target upload
pio device monitor --baud 115200
```

The Serial Monitor prints the node's IP address. Put that address into the rover's `HOME_NODE_BASE_URL`, then rebuild and upload the rover firmware if it changed.

## 17. Test the relay node directly

Check its health from PowerShell:

```powershell
Invoke-RestMethod -Uri "http://192.168.1.60/health"
```

Replace the IP address with the actual node address.

Test the bedroom light:

```powershell
$headers = @{ Authorization = "Bearer use-another-long-random-token" }
$body = @{
  version = 1
  request_id = "manual-test-0001"
  source = "test-pc"
  command = "set"
  device = "bedroom_light"
  state = "on"
} | ConvertTo-Json

Invoke-RestMethod `
  -Method Post `
  -Uri "http://192.168.1.60/api/v1/commands" `
  -Headers $headers `
  -ContentType "application/json" `
  -Body $body
```

Repeat with `state = "off"`. Test only with LEDs or safe low-voltage loads.

## 18. Create the PC Python environment

Open PowerShell in the project and run:

```powershell
cd pc_server
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -e ".[dev]"
```

If PowerShell blocks activation, run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\.venv\Scripts\Activate.ps1
```

## 19. Configure the OpenAI API

Create the PC configuration:

```powershell
Copy-Item .env.example .env
```

Open `.env` and configure:

```dotenv
OPENAI_API_KEY=sk-your-real-key
OPENAI_TRANSCRIBE_MODEL=gpt-4o-mini-transcribe
OPENAI_CHAT_MODEL=gpt-5.5
OPENAI_TTS_MODEL=gpt-4o-mini-tts
OPENAI_TTS_VOICE=coral

ROVER_TOKEN=use-a-long-random-token

AURA_HOST=0.0.0.0
AURA_PORT=8000
AURA_LOG_LEVEL=INFO
AURA_MAX_AUDIO_SECONDS=8
```

`ROVER_TOKEN` must exactly match the rover ESP32 value.

Never place `OPENAI_API_KEY` in Arduino or ESP32 code.

## 20. Run the PC tests

With the virtual environment active:

```powershell
python -m pytest
```

Expected result:

```text
13 passed
```

Do not proceed until all tests pass.

## 21. Start the AURA PC server

Run:

```powershell
aura-server
```

Leave that PowerShell window open.

Open this address in a browser on the PC:

```text
http://127.0.0.1:8000/health
```

You should see JSON similar to:

```json
{
  "ok": true,
  "service": "aura-rover-server",
  "connected_rovers": [],
  "openai_key_configured": true
}
```

If Windows Firewall asks for permission, allow Python on private networks only.

## 22. Connect the rover to the PC

1. Confirm the PC and both ESP32 boards use the same Wi-Fi network.
2. Start the PC server.
3. Power or reset the rover ESP32.
4. Watch the rover Serial Monitor.
5. Refresh `/health`.

Expected result:

```json
"connected_rovers": ["aura-rover-01"]
```

The OLED should show `AI connected` and then return to `(^_^)`.

If it does not connect:

- Check the PC IP in `PC_SERVER_HOST`.
- Check `ROVER_TOKEN` on both sides.
- Check Windows Firewall.
- Confirm port `8000` is not being used by another program.
- Confirm the ESP32 and PC can reach each other on the Wi-Fi network.

## 23. Test the complete voice workflow

Keep the rover's wheels raised.

1. Press the selected PS2 voice button once.
2. Confirm the OLED displays `(O_O)`.
3. Say: “Turn on bedroom light.”
4. Stop speaking.
5. Confirm the OLED changes to `(-_-)`.
6. Confirm the correct relay turns on.
7. Confirm AURA speaks its response while showing `(^o^)`.
8. Confirm the OLED returns to `(^_^)`.

Test every required command:

- Turn on bedroom light
- Turn off bedroom light
- Turn on fan
- Turn off fan
- Turn everything off

Also ask a normal question. A normal conversation must not activate a relay.

## 24. Test the exact obstacle workflow

With the wheels still raised:

1. Command the rover to move.
2. Place an object within the stop threshold.
3. Confirm the motors stop immediately.
4. Confirm the OLED shows `(>_<)`.
5. Confirm the local warning tone plays.
6. With the PC connected, confirm AURA says:

   ```text
   Obstacle detected. Vehicle stopped.
   ```

7. Repeat while AURA is Listening, Thinking, and Speaking.
8. Turn off the PC and repeat. The motors must still stop, and the OLED/tone must still work.

If stopping depends on the PC or Wi-Fi, the integration is incorrect.

## 25. Tune the rover

### Stop distance

Change these values in the Uno `SafetyConfig`:

```cpp
25,  // stop distance
32,  // clear distance
```

Use a larger stop distance if the rover moves quickly or the floor is slippery.

### Microphone sensitivity

Edit:

```text
firmware/rover_esp32/include/AppConfig.h
```

Tune:

```cpp
constexpr uint16_t kSpeechRmsThreshold = 450;
```

- Increase it if motor noise is detected as speech.
- Decrease it if quiet speech is ignored.

### Recording time

The default maximum is six seconds:

```cpp
constexpr uint32_t kRecordingMaxMs = 6000;
```

Keep responses and commands short for the competition demonstration.

## 26. Prepare for the competition

1. Use a dedicated travel router if possible.
2. Reserve fixed IP addresses for the PC and both ESP32 boards.
3. Label every wire.
4. Secure boards and cables so mecanum vibration cannot loosen them.
5. Carry spare USB cables, resistors, an HC-SR04, and an ESP32.
6. Disable PC sleep during the demonstration.
7. Start the PC server before powering the rover.
8. Perform three complete cold-start rehearsals.
9. Prepare an offline demonstration using PS2 driving, sonar stopping, OLED warning, and the local tone.

## Final checklist

- [ ] Original mecanum controls still work.
- [ ] Uno stops all four motors without Wi-Fi or PC.
- [ ] D7 divider output is approximately 3.3 V.
- [ ] All six OLED faces display correctly.
- [ ] Microphone detects speech without constant motor-noise triggers.
- [ ] Speaker is clear and does not reset the ESP32.
- [ ] Both relays are OFF during boot.
- [ ] Invalid relay tokens and commands are rejected.
- [ ] All five voice commands work.
- [ ] Normal conversation activates no relay.
- [ ] Exact obstacle warning is spoken when the PC is available.
- [ ] Local warning face and tone work when the PC is unavailable.
- [ ] Three cold-start rehearsals pass.

For deeper technical details, also read:

- `docs/architecture.md`
- `docs/wiring.md`
- `docs/protocols.md`
- `docs/risk-and-testing.md`
