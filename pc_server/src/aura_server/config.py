from __future__ import annotations

import os
from dataclasses import dataclass

from dotenv import load_dotenv

load_dotenv()


@dataclass(frozen=True, slots=True)
class Settings:
    openai_api_key: str = os.getenv("OPENAI_API_KEY", "sk-REDACTED-OPENAI-KEY")
    transcribe_model: str = os.getenv(
        "OPENAI_TRANSCRIBE_MODEL", "gpt-4o-mini-transcribe"
    )
    chat_model: str = os.getenv("OPENAI_CHAT_MODEL", "gpt-5.5")
    tts_model: str = os.getenv("OPENAI_TTS_MODEL", "gpt-4o-mini-tts")
    tts_voice: str = os.getenv("OPENAI_TTS_VOICE", "coral")
    rover_token: str = os.getenv("ROVER_TOKEN", "abcdefghijklmnop")
    host: str = os.getenv("AURA_HOST", "0.0.0.0")
    port: int = int(os.getenv("AURA_PORT", "8000"))
    log_level: str = os.getenv("AURA_LOG_LEVEL", "INFO").upper()
    max_audio_seconds: int = int(os.getenv("AURA_MAX_AUDIO_SECONDS", "8"))

    def validate_runtime(self) -> None:
        if not self.openai_api_key:
            raise RuntimeError("OPENAI_API_KEY is not set")
        if self.rover_token == "change-me-before-demo":
            raise RuntimeError("ROVER_TOKEN must be changed before running the server")
        if not 1 <= self.max_audio_seconds <= 30:
            raise RuntimeError("AURA_MAX_AUDIO_SECONDS must be between 1 and 30")
