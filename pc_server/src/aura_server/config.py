from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

from dotenv import load_dotenv

load_dotenv()


@dataclass(frozen=True, slots=True)
class Settings:
    openai_api_key: str = field(
        default_factory=lambda: os.getenv("OPENAI_API_KEY", "")
    )
    transcribe_model: str = os.getenv(
        "OPENAI_TRANSCRIBE_MODEL", "gpt-4o-mini-transcribe"
    )
    chat_model: str = os.getenv("OPENAI_CHAT_MODEL", "gpt-5.5")
    tts_model: str = os.getenv("OPENAI_TTS_MODEL", "gpt-4o-mini-tts")
    tts_voice: str = os.getenv("OPENAI_TTS_VOICE", "coral")
    device_token: str = os.getenv("AURA_DEVICE_TOKEN", "abcdefghijklmnop")
    host: str = os.getenv("AURA_HOST", "0.0.0.0")
    port: int = int(os.getenv("AURA_PORT", "8000"))
    log_level: str = os.getenv("AURA_LOG_LEVEL", "INFO").upper()
    max_audio_seconds: int = int(os.getenv("AURA_MAX_AUDIO_SECONDS", "8"))
    enable_web_search: bool = os.getenv("AURA_ENABLE_WEB_SEARCH", "true").lower() in {
        "1",
        "true",
        "yes",
        "on",
    }
    web_search_context_size: str = os.getenv("AURA_WEB_SEARCH_CONTEXT_SIZE", "low")
    enable_cognee: bool = os.getenv("AURA_ENABLE_COGNEE", "true").lower() in {
        "1",
        "true",
        "yes",
        "on",
    }
    cognee_memory_path: Path = field(
        default_factory=lambda: Path(
            os.getenv("AURA_COGNEE_MEMORY_PATH", "data/cognee_memory.json")
        )
    )

    def validate_runtime(self) -> None:
        if not self.openai_api_key:
            raise RuntimeError("OPENAI_API_KEY is not set")
        if self.device_token == "change-me-before-demo":
            raise RuntimeError("AURA_DEVICE_TOKEN must be changed before running the server")
        if not 1 <= self.max_audio_seconds <= 30:
            raise RuntimeError("AURA_MAX_AUDIO_SECONDS must be between 1 and 30")
        if self.web_search_context_size not in {"low", "medium", "high"}:
            raise RuntimeError(
                "AURA_WEB_SEARCH_CONTEXT_SIZE must be low, medium, or high"
            )
