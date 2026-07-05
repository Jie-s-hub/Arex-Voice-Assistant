import pytest

from aura_server.intent import classify_known_home_command
from aura_server.models import Device, PowerState


@pytest.mark.parametrize(
    ("phrase", "device", "state"),
    [
        ("Turn on bedroom light", Device.BEDROOM_LIGHT, PowerState.ON),
        ("Turn the bedroom light off!", Device.BEDROOM_LIGHT, PowerState.OFF),
        ("Turn on fan", Device.FAN, PowerState.ON),
        ("Turn the fan off.", Device.FAN, PowerState.OFF),
        ("Turn everything off", Device.ALL, PowerState.OFF),
    ],
)
def test_known_commands(phrase, device, state):
    decision = classify_known_home_command(phrase)
    assert decision is not None
    assert decision.intent == "smart_home"
    assert decision.action is not None
    assert decision.action.device is device
    assert decision.action.state is state


def test_conversation_is_not_misclassified():
    assert classify_known_home_command("Why do fans have blades?") is None


def test_unsupported_all_on_is_not_classified():
    assert classify_known_home_command("Turn everything on") is None

