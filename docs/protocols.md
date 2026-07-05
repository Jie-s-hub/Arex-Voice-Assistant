# Communication protocols

All JSON protocols use UTF-8, snake_case names, and integer `version: 1`. Unknown versions are rejected instead of guessed.

## Uno-to-rover one-wire protocol

Electrical path: `Uno D7 -> 10 kΩ/20 kΩ divider -> ESP32 GPIO25`, plus shared ground.

| Event | D7 waveform | Decoder rule |
|---|---|---|
| Voice request | HIGH 40 ms, LOW 40 ms, HIGH 40 ms, then LOW | Two completed pulses no longer than 100 ms, second within 220 ms |
| Obstacle | HIGH for at least 500 ms and until the obstacle clears | HIGH continuously for at least 150 ms |
| Idle | LOW | No event |

The waveform scheduler is non-blocking. An obstacle preempts an in-progress voice pattern. The 150 ms classification delay affects only the ESP32 display/audio; the Uno has already stopped the motors.

## Rover-to-PC WebSocket

Endpoint:

```text
ws://<PC-IP>:8000/ws/rover/<device_id>
Authorization: Bearer <ROVER_TOKEN>
```

Use an isolated/trusted competition WLAN because this first version uses plaintext LAN WebSockets. The token is a rover credential, not the OpenAI key. A production deployment should move to WSS and per-device certificates.

### Framing

- Control/event frames are WebSocket text containing one JSON object.
- Audio frames are WebSocket binary containing headerless signed 16-bit little-endian mono PCM.
- Binary frames are legal only between the appropriate `audio.start` and `audio.end` markers.
- Only one audio stream in each direction may be active on a connection.
- Rover capture is bounded to 8 seconds by the server, even if faulty firmware keeps sending.

### Rover messages

Audio capture start:

```json
{
  "version": 1,
  "type": "audio.start",
  "session_id": "aura-rover-01-0012abcd-7",
  "format": "pcm_s16le",
  "sample_rate_hz": 16000,
  "channels": 1
}
```

After binary frames:

```json
{
  "version": 1,
  "type": "audio.end",
  "session_id": "aura-rover-01-0012abcd-7"
}
```

Other rover events:

```json
{"version":1,"type":"audio.cancel","session_id":"aura-rover-01-0012abcd-7"}
{"version":1,"type":"safety.obstacle"}
{"version":1,"type":"hello","device_id":"aura-rover-01","firmware":"1.0.0","ip":"192.168.1.42"}
{"version":1,"type":"home.result","request_id":"aura-rover-01-0012abcd-7","ok":true,"status_code":200,"detail":"{...}"}
```

### PC messages

Emotion:

```json
{"version":1,"type":"emotion","state":"thinking","session_id":"aura-rover-01-0012abcd-7"}
```

Allowed states are `idle`, `listening`, `thinking`, `speaking`, `warning`, and `error`.

Smart-home action:

```json
{
  "version": 1,
  "type": "home.command",
  "request_id": "aura-rover-01-0012abcd-7",
  "device": "bedroom_light",
  "state": "on"
}
```

Assistant PCM uses:

```json
{"version":1,"type":"assistant.audio.start","session_id":"aura-rover-01-0012abcd-7","format":"pcm_s16le","sample_rate_hz":24000,"channels":1}
```

then binary PCM frames, then:

```json
{"version":1,"type":"assistant.audio.end","session_id":"aura-rover-01-0012abcd-7"}
```

Error:

```json
{
  "version": 1,
  "type": "error",
  "session_id": "aura-rover-01-0012abcd-7",
  "code": "voice_pipeline_failed",
  "message": "bounded human-readable detail"
}
```

## PC AI decision object

The AI does not emit arbitrary relay JSON. Its output must validate against `protocol/schemas/ai-decision.schema.json` and the Pydantic model.

Conversation:

```json
{
  "intent": "conversation",
  "reply": "Hello! What can I help you with?",
  "action": null
}
```

Home command:

```json
{
  "intent": "smart_home",
  "reply": "Turning off the fan.",
  "action": {"device":"fan","state":"off"}
}
```

Allowlist rules:

- `bedroom_light`: `on` or `off`
- `fan`: `on` or `off`
- `all`: `off` only

## Rover-to-smart-home HTTP API

Endpoint:

```text
POST http://<HOME-NODE-IP>/api/v1/commands
Authorization: Bearer <HOME_NODE_TOKEN>
Content-Type: application/json
```

Command:

```json
{
  "version": 1,
  "request_id": "aura-rover-01-0012abcd-7",
  "source": "aura-rover-01",
  "command": "set",
  "device": "bedroom_light",
  "state": "on"
}
```

Success:

```json
{
  "ok": true,
  "request_id": "aura-rover-01-0012abcd-7",
  "duplicate": false,
  "devices": {"bedroom_light":"on","fan":"off"}
}
```

Errors use HTTP `400` for malformed JSON, `401` for a bad token, and `422` for a valid JSON envelope with unsupported values. The node remembers eight recent request IDs; retrying one returns `duplicate: true` and does not create a new actuation event.

Health endpoint:

```text
GET http://<HOME-NODE-IP>/health
```

The machine-readable schema is `protocol/schemas/home-command.schema.json`.
