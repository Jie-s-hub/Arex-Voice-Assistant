# Protocol

The ESP32 connects to the PC server with:

```text
ws://<PC-IP>:8000/ws/audio/<device_id>
```

Authentication is a bearer token:

```text
Authorization: Bearer <AURA_DEVICE_TOKEN>
```

For local development, the token may also be passed as `?token=...`, but the firmware uses the header.

## ESP32 to PC

Hello:

```json
{"version":1,"type":"hello","device_id":"aura-audio-01","firmware":"1.0.0","ip":"192.168.1.42"}
```

Start recording:

```json
{"version":1,"type":"audio.start","session_id":"aura-audio-01-0012abcd-1","format":"pcm_s16le","sample_rate_hz":16000,"channels":1}
```

After `audio.start`, binary WebSocket frames contain signed 16-bit little-endian mono PCM. The ESP32 ends or cancels the turn with:

```json
{"version":1,"type":"audio.end","session_id":"aura-audio-01-0012abcd-1"}
```

```json
{"version":1,"type":"audio.cancel","session_id":"aura-audio-01-0012abcd-1"}
```

## PC to ESP32

State updates:

```json
{"version":1,"type":"emotion","state":"thinking","session_id":"aura-audio-01-0012abcd-1"}
```

Transcript:

```json
{"version":1,"type":"transcript","session_id":"aura-audio-01-0012abcd-1","text":"What is the weather today?"}
```

Assistant text:

```json
{"version":1,"type":"assistant.text","session_id":"aura-audio-01-0012abcd-1","text":"Here is the short answer.","intent":"conversation"}
```

Assistant audio starts with a JSON marker, continues as binary PCM frames, and ends with another marker:

```json
{"version":1,"type":"assistant.audio.start","session_id":"aura-audio-01-0012abcd-1","format":"pcm_s16le","sample_rate_hz":24000,"channels":1}
```

```json
{"version":1,"type":"assistant.audio.end","session_id":"aura-audio-01-0012abcd-1"}
```

Errors are bounded text messages:

```json
{"version":1,"type":"error","code":"voice_pipeline_failed","message":"OPENAI_API_KEY is not set"}
```
