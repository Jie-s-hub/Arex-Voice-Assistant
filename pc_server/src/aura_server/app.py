from __future__ import annotations

import asyncio
import json
import logging
import secrets
from contextlib import suppress
from dataclasses import dataclass, field
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, status

from .ai import AuraAI
from .audio import AudioCapture, AudioProtocolError, capture_from_start
from .config import Settings
from .conversation import ConversationStore
from .models import Decision

logger = logging.getLogger(__name__)


class AudioDeviceConnection:
    def __init__(self, websocket: WebSocket, device_id: str) -> None:
        self.websocket = websocket
        self.device_id = device_id
        self.capture: AudioCapture | None = None
        self.send_lock = asyncio.Lock()
        self.turn_tasks: set[asyncio.Task[Any]] = set()

    async def send_json(self, message: dict[str, Any]) -> None:
        async with self.send_lock:
            await self.websocket.send_json({"version": 1, **message})

    async def send_pcm(self, pcm: bytes, session_id: str) -> None:
        # One lock covers the start marker, binary frames, and end marker so
        # another task cannot interleave a different audio stream.
        async with self.send_lock:
            await self.websocket.send_json(
                {
                    "version": 1,
                    "type": "assistant.audio.start",
                    "session_id": session_id,
                    "format": "pcm_s16le",
                    "sample_rate_hz": 24000,
                    "channels": 1,
                }
            )
            try:
                for offset in range(0, len(pcm), 4096):
                    await self.websocket.send_bytes(pcm[offset : offset + 4096])
                    await asyncio.sleep(0)
            finally:
                await self.websocket.send_json(
                    {
                        "version": 1,
                        "type": "assistant.audio.end",
                        "session_id": session_id,
                    }
                )

    def start_task(self, coroutine: Any) -> asyncio.Task[Any]:
        task = asyncio.create_task(coroutine)
        self.turn_tasks.add(task)
        task.add_done_callback(self.turn_tasks.discard)
        return task

    def cancel_turns(self) -> None:
        for task in tuple(self.turn_tasks):
            task.cancel()


@dataclass(slots=True)
class AppState:
    settings: Settings
    conversations: ConversationStore = field(default_factory=ConversationStore)
    connections: dict[str, AudioDeviceConnection] = field(default_factory=dict)
    _ai: AuraAI | None = None

    @property
    def ai(self) -> AuraAI:
        if self._ai is None:
            self.settings.validate_runtime()
            self._ai = AuraAI(self.settings)
        return self._ai


async def process_turn(
    state: AppState, connection: AudioDeviceConnection, capture: AudioCapture
) -> None:
    session_id = capture.session_id
    try:
        await connection.send_json(
            {"type": "emotion", "state": "thinking", "session_id": session_id}
        )
        wav = capture.to_wav()
        transcript = await asyncio.to_thread(state.ai.transcribe, wav)
        if not transcript:
            raise ValueError("I could not hear any speech")
        await connection.send_json(
            {"type": "transcript", "session_id": session_id, "text": transcript}
        )

        history = state.conversations.history(connection.device_id)
        decision: Decision = await asyncio.to_thread(
            state.ai.decide, transcript, history, connection.device_id
        )
        state.conversations.add(connection.device_id, transcript, decision.reply)

        await connection.send_json(
            {
                "type": "assistant.text",
                "session_id": session_id,
                "text": decision.reply,
                "intent": decision.intent,
            }
        )
        pcm = await asyncio.to_thread(state.ai.synthesize, decision.reply)
        await connection.send_pcm(pcm, session_id)
    except asyncio.CancelledError:
        logger.info("Turn %s cancelled for %s", session_id, connection.device_id)
        raise
    except Exception as exc:
        logger.exception("Voice turn failed for %s", connection.device_id)
        with suppress(Exception):
            await connection.send_json(
                {
                    "type": "error",
                    "session_id": session_id,
                    "code": "voice_pipeline_failed",
                    "message": str(exc)[:120],
                }
            )


def create_app(settings: Settings | None = None) -> FastAPI:
    app = FastAPI(title="AURA Wireless AI Audio Server", version="1.0.0")
    state = AppState(settings or Settings())
    app.state.aura = state

    @app.get("/health")
    async def health() -> dict[str, Any]:
        return {
            "ok": True,
            "service": "aura-audio-server",
            "connected_devices": sorted(state.connections),
            "openai_key_configured": bool(state.settings.openai_api_key),
            "cognee_enabled": state.settings.enable_cognee,
        }

    @app.websocket("/ws/audio/{device_id}")
    async def audio_socket(websocket: WebSocket, device_id: str) -> None:
        authorization = websocket.headers.get("authorization", "")
        supplied_token = (
            authorization.removeprefix("Bearer ")
            if authorization.startswith("Bearer ")
            else websocket.query_params.get("token", "")  # v0 compatibility
        )
        if not secrets.compare_digest(supplied_token, state.settings.device_token):
            await websocket.close(code=status.WS_1008_POLICY_VIOLATION)
            return

        await websocket.accept()
        connection = AudioDeviceConnection(websocket, device_id)
        old = state.connections.get(device_id)
        if old is not None:
            await old.websocket.close(code=status.WS_1001_GOING_AWAY)
        state.connections[device_id] = connection
        logger.info("Audio device connected: %s", device_id)

        try:
            while True:
                event = await websocket.receive()
                if event["type"] == "websocket.disconnect":
                    break
                if event.get("bytes") is not None:
                    if connection.capture is None:
                        raise AudioProtocolError("binary frame without audio.start")
                    connection.capture.append(event["bytes"])
                    continue

                raw_text = event.get("text")
                if raw_text is None:
                    continue
                if len(raw_text) > 4096:
                    raise AudioProtocolError("control frame exceeded 4096 bytes")
                message = json.loads(raw_text)
                if message.get("version") != 1:
                    raise AudioProtocolError("unsupported protocol version")
                message_type = message.get("type")

                if message_type == "audio.start":
                    if connection.capture is not None:
                        raise AudioProtocolError("audio capture already active")
                    connection.capture = capture_from_start(
                        message, state.settings.max_audio_seconds
                    )
                elif message_type == "audio.end":
                    capture = connection.capture
                    connection.capture = None
                    if capture is None or capture.session_id != message.get("session_id"):
                        raise AudioProtocolError("audio.end session mismatch")
                    connection.start_task(process_turn(state, connection, capture))
                elif message_type == "audio.cancel":
                    connection.capture = None
                elif message_type in {"hello", "pong"}:
                    logger.debug("Audio device event from %s: %s", device_id, message_type)
                else:
                    logger.warning("Unknown audio device message: %s", message_type)
        except (WebSocketDisconnect, RuntimeError):
            pass
        except (AudioProtocolError, json.JSONDecodeError) as exc:
            logger.warning("Protocol error from %s: %s", device_id, exc)
            with suppress(Exception):
                await connection.send_json(
                    {"type": "error", "code": "protocol_error", "message": str(exc)}
                )
                await websocket.close(code=status.WS_1003_UNSUPPORTED_DATA)
        finally:
            connection.cancel_turns()
            if state.connections.get(device_id) is connection:
                state.connections.pop(device_id, None)
            logger.info("Audio device disconnected: %s", device_id)

    return app


app = create_app()
