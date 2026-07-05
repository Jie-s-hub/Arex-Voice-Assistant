from __future__ import annotations

import io
import json
import threading
from collections.abc import Sequence

from openai import OpenAI

from .config import Settings
from .intent import classify_known_home_command
from .models import DECISION_JSON_SCHEMA, Decision


SYSTEM_INSTRUCTIONS = """You are AURA, a warm and concise voice assistant on a high-school smart-living rover.
Success criteria:
- The user name is Ong You Jie, he is a 17-year-old high-school student.
- Reply in at most two short spoken sentences.
- When the user asks for recent facts, online information, news, prices, schedules, research, or anything likely to change, use web search before answering.
- For research answers, summarize clearly for speech; include source names only when they fit naturally.
- Classify supported home actions only for bedroom_light, fan, or all-off.
- Never invent a device or action.
- If the request is ambiguous, ask a brief question and return action as null.
- For ordinary conversation, return intent conversation and action null.
- Do not claim an action succeeded; the rover confirms it separately.
"""


class AuraAI:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.client = OpenAI(api_key=settings.openai_api_key)
        self._warning_pcm: bytes | None = None
        self._warning_lock = threading.Lock()

    def response_tools(self) -> list[dict[str, str]] | None:
        if not self.settings.enable_web_search:
            return None
        return [
            {
                "type": "web_search",
                "search_context_size": self.settings.web_search_context_size,
            }
        ]

    def transcribe(self, wav_bytes: bytes) -> str:
        audio = io.BytesIO(wav_bytes)
        audio.name = "aura-capture.wav"
        result = self.client.audio.transcriptions.create(
            model=self.settings.transcribe_model,
            file=audio,
            response_format="text",
            prompt="AURA Rover commands may mention a bedroom light or a fan.",
        )
        text = result if isinstance(result, str) else result.text
        return text.strip()

    def decide(self, transcript: str, history: Sequence[tuple[str, str]]) -> Decision:
        known = classify_known_home_command(transcript)
        if known is not None:
            return known

        context = "\n".join(
            f"User: {user}\nAURA: {assistant}" for user, assistant in history[-4:]
        )
        dynamic_input = (
            f"Recent conversation:\n{context or '(none)'}\n\n"
            f"Current user transcript: {transcript}"
        )
        response = self.client.responses.create(
            model=self.settings.chat_model,
            instructions=SYSTEM_INSTRUCTIONS,
            input=dynamic_input,
            tools=self.response_tools(),
            reasoning={"effort": "none"},
            text={
                "verbosity": "low",
                "format": {
                    "type": "json_schema",
                    "name": "aura_decision",
                    "strict": True,
                    "schema": DECISION_JSON_SCHEMA,
                },
            },
            store=False,
        )
        return Decision.model_validate(json.loads(response.output_text))

    def synthesize(self, text: str) -> bytes:
        with self.client.audio.speech.with_streaming_response.create(
            model=self.settings.tts_model,
            voice=self.settings.tts_voice,
            input=text,
            instructions="Speak warmly, clearly, and briefly as a friendly mobile robot.",
            response_format="pcm",
        ) as response:
            return response.read()

    def warning_pcm(self) -> bytes:
        with self._warning_lock:
            if self._warning_pcm is None:
                self._warning_pcm = self.synthesize(
                    "Obstacle detected. Vehicle stopped."
                )
            return self._warning_pcm
