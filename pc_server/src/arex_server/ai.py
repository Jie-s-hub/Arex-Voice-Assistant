from __future__ import annotations

import io
import json
import re
from collections.abc import Sequence

from openai import OpenAI

from .config import Settings
from .cognee import CogneeMemory
from .models import DECISION_JSON_SCHEMA, Decision


SYSTEM_INSTRUCTIONS = """You are Arex, a warm and concise voice assistant running through an ESP32 wireless microphone and speaker.
Success criteria:
- The user name is Ong You Jie, he is a 17-year-old high-school student.
- Reply in at most two short spoken sentences.
- When the user asks for recent facts, online information, news, prices, schedules, research, or anything likely to change, use web search before answering.
- For research answers, summarize clearly for speech; include source names only when they fit naturally.
- Use the Cognee long-term memory section when it is relevant.
- Never invent memories. If a fact is not in the current transcript, recent conversation, or Cognee memory, say you do not remember it.
- Return intent conversation for every reply.
"""


def split_wake_word(transcript: str, aliases: Sequence[str]) -> tuple[bool, str]:
    """Return whether the transcript starts with a wake word and the command."""

    text = re.sub(r"[-_]+", " ", transcript).strip()
    if not text:
        return False, ""

    normalized_aliases = sorted(
        {alias.strip().lower() for alias in aliases if alias.strip()},
        key=len,
        reverse=True,
    )
    for alias in normalized_aliases:
        alias_pattern = r"\s+".join(re.escape(part) for part in alias.split())
        pattern = rf"^\s*(?:hey\s+|ok(?:ay)?\s+)?{alias_pattern}(?:\b|[\s,.:;!?]+)(.*)$"
        match = re.match(pattern, text, flags=re.IGNORECASE)
        if match:
            command = match.group(1).strip(" \t\r\n,.:;!?")
            return True, command
    return False, ""


class ArexAI:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.client = OpenAI(api_key=settings.openai_api_key)
        self.memory = (
            CogneeMemory(settings.cognee_memory_path) if settings.enable_cognee else None
        )

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
        audio.name = "arex-capture.wav"
        result = self.client.audio.transcriptions.create(
            model=self.settings.transcribe_model,
            file=audio,
            response_format="text",
            prompt="Short voice notes and questions captured by an ESP32 wireless microphone. The wake word is Arex.",
        )
        text = result if isinstance(result, str) else result.text
        return text.strip()

    def split_wake_word(self, transcript: str) -> tuple[bool, str]:
        aliases = (self.settings.wake_word, *self.settings.wake_word_aliases)
        return split_wake_word(transcript, aliases)

    def decide(
        self, transcript: str, history: Sequence[tuple[str, str]], device_id: str
    ) -> Decision:
        if self.memory is not None:
            memory_decision = self.memory.handle_command(device_id, transcript)
            if memory_decision is not None:
                return memory_decision

        context = "\n".join(
            f"User: {user}\nArex: {assistant}" for user, assistant in history[-4:]
        )
        memory_context = (
            self.memory.format_context(device_id, transcript)
            if self.memory is not None
            else "(memory disabled)"
        )
        dynamic_input = (
            f"Cognee long-term memory for this audio device:\n{memory_context}\n\n"
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
                    "name": "arex_decision",
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
            instructions="Speak warmly, clearly, and briefly as Arex, a friendly AI audio assistant.",
            response_format="pcm",
        ) as response:
            return response.read()
