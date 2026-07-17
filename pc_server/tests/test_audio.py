import io
import wave

import pytest

from arex_server.audio import AudioProtocolError, capture_from_start


def start_message(**overrides):
    value = {
        "session_id": "arex-audio-01-12345678",
        "format": "pcm_s16le",
        "sample_rate_hz": 16000,
        "channels": 1,
    }
    value.update(overrides)
    return value


def test_pcm_is_wrapped_as_valid_wav():
    capture = capture_from_start(start_message(), max_audio_seconds=2)
    capture.append(b"\x00\x00\x10\x00" * 100)
    with wave.open(io.BytesIO(capture.to_wav()), "rb") as wav:
        assert wav.getnchannels() == 1
        assert wav.getsampwidth() == 2
        assert wav.getframerate() == 16000
        assert wav.getnframes() == 200


def test_capture_limit_is_enforced():
    capture = capture_from_start(start_message(sample_rate_hz=8000), 1)
    with pytest.raises(AudioProtocolError, match="exceeded"):
        capture.append(bytes(16002))


def test_invalid_format_is_rejected():
    with pytest.raises(AudioProtocolError, match="pcm_s16le"):
        capture_from_start(start_message(format="mp3"), 2)


def test_wake_trigger_is_accepted():
    capture = capture_from_start(start_message(trigger="wake"), max_audio_seconds=2)

    assert capture.trigger == "wake"


def test_invalid_trigger_is_rejected():
    with pytest.raises(AudioProtocolError, match="trigger"):
        capture_from_start(start_message(trigger="timer"), 2)
