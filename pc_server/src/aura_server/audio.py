from __future__ import annotations

import io
import wave
from dataclasses import dataclass, field


class AudioProtocolError(ValueError):
    pass


@dataclass(slots=True)
class AudioCapture:
    session_id: str
    sample_rate_hz: int
    channels: int
    max_bytes: int
    pcm: bytearray = field(default_factory=bytearray)

    def append(self, chunk: bytes) -> None:
        if len(self.pcm) + len(chunk) > self.max_bytes:
            raise AudioProtocolError("audio capture exceeded configured limit")
        self.pcm.extend(chunk)

    def to_wav(self) -> bytes:
        if not self.pcm:
            raise AudioProtocolError("audio capture is empty")
        if len(self.pcm) % 2:
            raise AudioProtocolError("16-bit PCM byte count must be even")
        output = io.BytesIO()
        with wave.open(output, "wb") as wav:
            wav.setnchannels(self.channels)
            wav.setsampwidth(2)
            wav.setframerate(self.sample_rate_hz)
            wav.writeframes(self.pcm)
        return output.getvalue()


def capture_from_start(message: dict, max_audio_seconds: int) -> AudioCapture:
    if message.get("format") != "pcm_s16le":
        raise AudioProtocolError("only pcm_s16le input is supported")
    sample_rate = int(message.get("sample_rate_hz", 0))
    channels = int(message.get("channels", 0))
    session_id = str(message.get("session_id", ""))
    if not session_id or not 8000 <= sample_rate <= 48000 or channels != 1:
        raise AudioProtocolError("invalid session_id, sample rate, or channels")
    return AudioCapture(
        session_id=session_id,
        sample_rate_hz=sample_rate,
        channels=channels,
        max_bytes=sample_rate * channels * 2 * max_audio_seconds,
    )

