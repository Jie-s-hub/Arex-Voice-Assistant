# Arex Voice Assistant

ESP32 wireless microphone and speaker for talking with an AI assistant over Wi-Fi.

Arex listens through an INMP441 I2S microphone, streams raw PCM to a FastAPI PC server over WebSocket, uses OpenAI for transcription, wake-word detection, conversation, optional web search, memory, and text-to-speech, then streams 24 kHz PCM audio back to a MAX98357A speaker amplifier.

## What is included

- Standalone ESP32 firmware for Wi-Fi, I2S microphone capture, I2S speaker playback, OLED status faces, hands-free wake listening, and button-triggered fallback turns.
- A FastAPI PC server that handles WebSocket audio, OpenAI transcription, Responses API conversation, optional hosted web search, optional local Cognee-style memory, and PCM speech output.
- Wake-word gating for hands-free audio candidates. Say `Arex, ...`; background speech without the wake word is ignored.
- JSON schemas for the audio start message and AI decision payload.
- Hardware wiring, setup, and protocol notes for the Arex Voice Assistant device.

## System at a glance

```mermaid
flowchart LR
    SOUND["Voice activity"] --> ESP32["ESP32 Arex Voice Assistant"]
    BUTTON["Fallback button<br/>BOOT / GPIO0"] --> ESP32
    MIC["INMP441 I2S mic"] --> ESP32
    ESP32 --> OLED["I2C OLED"]
    ESP32 --> AMP["MAX98357A + speaker"]
    ESP32 <-->|"WebSocket: JSON + PCM"| PC["FastAPI PC server"]
    PC <-->|"STT / Responses / TTS"| OPENAI["OpenAI API"]
```

The OpenAI API key stays on the PC. The ESP32 only knows the Wi-Fi credentials, PC LAN address, device id, and shared device token.

## Repository map

```text
Arex-Voice-Assistant/
|-- docs/
|   |-- setup.md
|   |-- wiring.md
|   \-- protocol.md
|-- firmware/
|   \-- arex_voice_assistant/
|-- pc_server/
\-- protocol/
    \-- schemas/
```

## Quick start

1. Wire the ESP32, INMP441, MAX98357A, OLED, and optional fallback voice button using [docs/wiring.md](docs/wiring.md).
2. Copy `firmware/arex_voice_assistant/include/secrets.example.h` to `firmware/arex_voice_assistant/include/secrets.h`.
3. Set `WIFI_SSID`, `WIFI_PASSWORD`, `PC_SERVER_HOST`, `AUDIO_DEVICE_ID`, and `AUDIO_DEVICE_TOKEN`.
4. On the PC:

   ```powershell
   cd pc_server
   python -m venv .venv
   .\.venv\Scripts\Activate.ps1
   pip install -e ".[dev]"
   Copy-Item .env.example .env
   ```

5. In `pc_server/.env`, set `OPENAI_API_KEY` and `AREX_DEVICE_TOKEN` to the same token used by `AUDIO_DEVICE_TOKEN`.
6. Start the server:

   ```powershell
   arex-server
   ```

7. Build and upload the ESP32 firmware:

   ```powershell
   pio run -d firmware\arex_voice_assistant
   pio run -d firmware\arex_voice_assistant --target upload
   ```

8. Open `http://<PC-IP>:8000/health`, power the ESP32, then say `Arex, what can you do?` or press the fallback button to talk.

## Default OpenAI pipeline

- Speech-to-text: `gpt-4o-mini-transcribe`
- Conversation: `gpt-5.5`
- Wake word: `Arex`, enabled by `AREX_ENABLE_WAKE_WORD=true`
- Online research: Responses API hosted `web_search`, enabled by `AREX_ENABLE_WEB_SEARCH=true`
- Long-term memory: local JSON store, enabled by `AREX_ENABLE_COGNEE=true`
- Text-to-speech: `gpt-4o-mini-tts`, raw 24 kHz mono PCM

All model IDs are environment variables in `pc_server/src/arex_server/config.py`.

## Notes

Keep speaker volume low while testing, power the amplifier from a stable supply, and keep microphone wiring short and away from the speaker amp output. Arex Voice Assistant is a prototype, not a certified consumer audio product.
