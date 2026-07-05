# Integration notes for `mecanum_car_v2.3.ino`

The supplied sketch has been integrated into:

```text
firmware/uno_mecanum_aura/mecanum_car_v2.4_aura/
  mecanum_car_v2.4_aura.ino
```

The mecanum equations, speed settings, smoothing, motor function, and original PS2 movement buttons are preserved.

## Why the original pin plan could not be used

The supplied sketch already uses:

| Original pin | Existing use |
|---:|---|
| D7 | Right-front motor `RF_IN1` |
| D8 | Right-front motor `RF_IN2` |
| D9 | Right-front motor PWM `RF_EN` |

Therefore, connecting D7 directly to the ESP32 or putting the sonar on D8/D9 would interfere with the right-front motor.

The sketch also uses D2, D4–D10, D12, D13, and A0–A5. Only D3 and D11 are safely available while keeping USB Serial on D0/D1.

## Exact hardware changes

### 1. Move one existing motor wire

Move the right-front L298N `IN1` control wire:

```text
Before: Uno D7 -> right-front L298N IN1
After:  Uno D3 -> right-front L298N IN1
```

Do not move the right-front `IN2` or enable wires:

```text
Uno D8 -> right-front L298N IN2
Uno D9 -> right-front L298N EN/PWM
```

### 2. Use D7 for the ESP32 alert

```text
Uno D7 ---- 10 kΩ ----+---- ESP32 GPIO25
                      |
                    20 kΩ
                      |
Shared GND -----------+
```

Measure approximately 3.3 V at the divider output when D7 is HIGH before connecting GPIO25.

### 3. Connect the HC-SR04 in one-pin mode

NewPing supports one-pin mode when its trigger and echo arguments are the same pin. This is necessary because only D11 remains free.

Wire the standard four-pin HC-SR04 like this:

```text
HC-SR04 VCC  -> Uno 5V
HC-SR04 GND  -> Uno GND
HC-SR04 TRIG -> Uno D11 directly
HC-SR04 ECHO -> one end of a 2.2 kΩ resistor
resistor other end -> Uno D11
```

The resistor limits current while the shared line changes between trigger output and echo input. Do not connect ECHO directly to the shared D11 node without the resistor.

The code uses:

```cpp
NewPing sonar(SONAR_PIN, SONAR_PIN, MAX_DISTANCE_CM);
```

### 4. Voice button

The original movement buttons are unchanged. PS2 `L1` was unused and is now the dedicated AURA voice button:

```cpp
const bool voicePressed = ps2x.Button(PSB_L1);
```

## Final Uno pin map

| Pin | Final use |
|---:|---|
| D0 | USB Serial RX |
| D1 | USB Serial TX |
| D2 | Right-rear IN1 |
| D3 | Right-front IN1 — moved from D7 |
| D4 | Right-rear IN2 |
| D5 | Left-front PWM |
| D6 | Left-rear PWM |
| D7 | AURA alert through divider |
| D8 | Right-front IN2 |
| D9 | Right-front PWM |
| D10 | Right-rear PWM |
| D11 | HC-SR04 one-pin trigger/echo |
| D12 | PS2 SEL |
| D13 | PS2 CLK |
| A0 | Left-front IN1 |
| A1 | Left-front IN2 |
| A2 | Left-rear IN1 |
| A3 | Left-rear IN2 |
| A4 | PS2 CMD |
| A5 | PS2 DAT |

## Added safety behavior

- The sonar is checked every 50 ms.
- A valid distance of 25 cm or less immediately calls the existing `stopAllMotors()` function.
- The stop remains latched until three clear readings at or beyond 32 cm.
- After clearing, the driver must release movement controls once before motion is allowed again.
- The D7 obstacle signal remains HIGH while the stop latch is active.
- Pressing L1 sends the short-double-pulse voice request to the rover ESP32.
- Sonar safety continues operating if the PS2 controller disconnects.

## Upload procedure

### Arduino IDE

1. Install the `PS2X` and `NewPing` libraries.
2. Open `mecanum_car_v2.4_aura.ino`.
3. Select **Arduino Uno** and the correct COM port.
4. Upload.
5. Open Serial Monitor at 9600 baud.

### PlatformIO

From the AURA project root:

```powershell
pio run -d firmware/uno_mecanum_aura
pio run -d firmware/uno_mecanum_aura --target upload
pio device monitor --baud 9600
```

## First test

Keep the wheels raised.

1. Verify every original movement direction.
2. Pay special attention to the right-front wheel after moving its IN1 wire to D3.
3. Press L1 and confirm the ESP32 enters Listening mode.
4. Move forward and place an object within 25 cm.
5. Confirm all four motors stop.
6. Keep holding the movement control while removing the object.
7. Confirm the rover remains stopped until the movement control is released and pressed again.

