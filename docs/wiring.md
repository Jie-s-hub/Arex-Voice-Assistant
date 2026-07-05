# Wiring

Default pins are defined in `firmware/ai_audio_esp32/include/AppConfig.h`.

## ESP32 pin map

| Part | Signal | ESP32 pin | Notes |
|---|---:|---:|---|
| Voice button | Button input | GPIO0 | Uses the common BOOT button by default, active LOW |
| OLED | SDA | GPIO21 | I2C address defaults to `0x3C` |
| OLED | SCL | GPIO22 | Shared I2C bus |
| INMP441 mic | SCK / BCLK | GPIO26 | I2S receive bus |
| INMP441 mic | WS / LRCL | GPIO27 | I2S receive bus |
| INMP441 mic | SD | GPIO34 | Input-only pin |
| MAX98357A amp | BCLK | GPIO14 | I2S transmit bus |
| MAX98357A amp | LRC | GPIO13 | I2S transmit bus |
| MAX98357A amp | DIN | GPIO32 | I2S transmit bus |

## Power and grounding

- Power the ESP32 from USB or a stable 5 V supply.
- Power the INMP441 and OLED from 3.3 V.
- Power the MAX98357A according to the board rating, usually 3.3 V to 5 V.
- Connect all grounds together.
- Keep microphone wires short and physically separated from speaker output wires.
- Start with low speaker volume to avoid feedback into the microphone.

## Button behavior

The default voice trigger is the ESP32 dev-board BOOT button on GPIO0. Press it after the device has already booted. To use another button, change `kVoiceButtonPin` in `AppConfig.h`; wire one side to the selected GPIO and the other side to ground when `kVoiceButtonActiveLow` is `true`.
