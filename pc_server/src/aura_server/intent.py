from __future__ import annotations

import re

from .models import Decision, Device, HomeAction, PowerState


def _normalize(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", " ", text.lower()).strip()


def classify_known_home_command(text: str) -> Decision | None:
    """Recognize the five competition commands without an AI round trip.

    The AI structured-output path remains available for natural variants, but
    exact commands are deterministic, fast, and cannot hallucinate a device.
    """

    normalized = _normalize(text)
    patterns: tuple[tuple[set[str], HomeAction, str], ...] = (
        (
            {"turn everything off", "switch everything off", "all off"},
            HomeAction(device=Device.ALL, state=PowerState.OFF),
            "Turning everything off.",
        ),
        (
            {"turn on bedroom light", "turn the bedroom light on"},
            HomeAction(device=Device.BEDROOM_LIGHT, state=PowerState.ON),
            "Turning on the bedroom light.",
        ),
        (
            {"turn off bedroom light", "turn the bedroom light off"},
            HomeAction(device=Device.BEDROOM_LIGHT, state=PowerState.OFF),
            "Turning off the bedroom light.",
        ),
        (
            {"turn on fan", "turn the fan on"},
            HomeAction(device=Device.FAN, state=PowerState.ON),
            "Turning on the fan.",
        ),
        (
            {"turn off fan", "turn the fan off"},
            HomeAction(device=Device.FAN, state=PowerState.OFF),
            "Turning off the fan.",
        ),
    )
    for phrases, action, reply in patterns:
        if normalized in phrases:
            return Decision(intent="smart_home", reply=reply, action=action)
    return None

