# Setup

## 1. Configure the PC server

```powershell
cd pc_server
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -e ".[dev]"
Copy-Item .env.example .env
```

Edit `pc_server/.env`:

```text
OPENAI_API_KEY=sk-...
AURA_DEVICE_TOKEN=replace-with-a-long-random-token
AURA_ENABLE_WEB_SEARCH=true
AURA_ENABLE_COGNEE=true
```

Start the server:

```powershell
aura-server
```

The health endpoint is:

```text
http://<PC-LAN-IP>:8000/health
```

## 2. Configure the ESP32

Copy:

```text
firmware/ai_audio_esp32/include/secrets.example.h
```

to:

```text
firmware/ai_audio_esp32/include/secrets.h
```

Set:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `PC_SERVER_HOST`
- `AUDIO_DEVICE_ID`
- `AUDIO_DEVICE_TOKEN`

`AUDIO_DEVICE_TOKEN` must match `AURA_DEVICE_TOKEN` on the PC.

## 3. Build and upload

```powershell
pio run -d firmware\ai_audio_esp32
pio run -d firmware\ai_audio_esp32 --target upload
pio device monitor -d firmware\ai_audio_esp32
```

## 4. Use it

Wait for the OLED to show `AI connected`, then press the voice button. The device records until speech ends or the maximum recording time is reached, sends the turn to the PC, and plays the assistant reply through the speaker.
