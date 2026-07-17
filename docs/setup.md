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
AREX_DEVICE_TOKEN=replace-with-a-long-random-token
AREX_ENABLE_WAKE_WORD=true
AREX_WAKE_WORD=Arex
AREX_WAKE_WORD_ALIASES=arex,a rex,hey arex,hey a rex,alex
AREX_WAKE_WORD_REQUIRED_FOR_BUTTON=false
AREX_ENABLE_WEB_SEARCH=true
AREX_ENABLE_COGNEE=true
```

Start the server:

```powershell
arex-server
```

The health endpoint is:

```text
http://<PC-LAN-IP>:8000/health
```

## 2. Configure the ESP32

Copy:

```text
firmware/arex_voice_assistant/include/secrets.example.h
```

to:

```text
firmware/arex_voice_assistant/include/secrets.h
```

Set:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `PC_SERVER_HOST`
- `AUDIO_DEVICE_ID`
- `AUDIO_DEVICE_TOKEN`

`AUDIO_DEVICE_TOKEN` must match `AREX_DEVICE_TOKEN` on the PC.

## 3. Build and upload

```powershell
pio run -d firmware\arex_voice_assistant
pio run -d firmware\arex_voice_assistant --target upload
pio device monitor -d firmware\arex_voice_assistant
```

## 4. Use it

Wait for the OLED to show `Arex online`, then say `Arex, ...` followed by your request. The ESP32 records a wake candidate when it hears sound; the PC transcribes it and ignores it unless the wake word is detected. The fallback button still starts a turn without requiring the wake word unless `AREX_WAKE_WORD_REQUIRED_FOR_BUTTON=true`.
