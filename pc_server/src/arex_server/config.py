from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

from dotenv import load_dotenv

load_dotenv()


def _env(name: str, default: str) -> str:
    return os.getenv(name, default)


def _env_bool(name: str, default: str) -> bool:
    return _env(name, default).lower() in {"1", "true", "yes", "on"}


def _env_csv(name: str, default: str) -> tuple[str, ...]:
    return tuple(
        item.strip()
        for item in _env(name, default).split(",")
        if item.strip()
    )


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
    device_token: str = field(
        default_factory=lambda: _env("AREX_DEVICE_TOKEN", "abcdefghijklmnop")
    )
    host: str = field(default_factory=lambda: _env("AREX_HOST", "0.0.0.0"))
    port: int = field(
        default_factory=lambda: int(_env("AREX_PORT", "8000"))
    )
    log_level: str = field(
        default_factory=lambda: _env("AREX_LOG_LEVEL", "INFO").upper()
    )
    max_audio_seconds: int = field(
        default_factory=lambda: int(
            _env("AREX_MAX_AUDIO_SECONDS", "8")
        )
    )
    enable_web_search: bool = field(
        default_factory=lambda: _env_bool("AREX_ENABLE_WEB_SEARCH", "true")
    )
    web_search_context_size: str = field(
        default_factory=lambda: _env("AREX_WEB_SEARCH_CONTEXT_SIZE", "low")
    )
    enable_cognee: bool = field(
        default_factory=lambda: _env_bool("AREX_ENABLE_COGNEE", "true")
    )
    cognee_memory_path: Path = field(
        default_factory=lambda: Path(
            _env("AREX_COGNEE_MEMORY_PATH", "data/cognee_memory.json")
        )
    )
    enable_wake_word: bool = field(
        default_factory=lambda: _env_bool("AREX_ENABLE_WAKE_WORD", "true")
    )
    wake_word: str = field(
        default_factory=lambda: _env("AREX_WAKE_WORD", "Arex")
    )
    wake_word_aliases: tuple[str, ...] = field(
        default_factory=lambda: _env_csv(
            "AREX_WAKE_WORD_ALIASES",
            "arex,a rex,hey arex,hey a rex,alex",
        )
    )
    wake_word_required_for_button: bool = field(
        default_factory=lambda: _env_bool(
            "AREX_WAKE_WORD_REQUIRED_FOR_BUTTON",
            "false",
        )
    )

    def validate_runtime(self) -> None:
        if not self.openai_api_key:
            raise RuntimeError("OPENAI_API_KEY is not set")
        if self.device_token == "change-me-before-demo":
            raise RuntimeError(
                "AREX_DEVICE_TOKEN must be changed before running the server"
            )
        if not 1 <= self.max_audio_seconds <= 30:
            raise RuntimeError("AREX_MAX_AUDIO_SECONDS must be between 1 and 30")
        if self.web_search_context_size not in {"low", "medium", "high"}:
            raise RuntimeError(
                "AREX_WEB_SEARCH_CONTEXT_SIZE must be low, medium, or high"
            )
        if not self.wake_word.strip():
            raise RuntimeError("AREX_WAKE_WORD must not be empty")
