import pytest
from pydantic import ValidationError

from aura_server.models import Decision


def test_all_on_is_rejected_even_from_model_output():
    with pytest.raises(ValidationError):
        Decision.model_validate(
            {
                "intent": "smart_home",
                "reply": "Turning everything on.",
                "action": {"device": "all", "state": "on"},
            }
        )


def test_extra_action_fields_are_rejected():
    with pytest.raises(ValidationError):
        Decision.model_validate(
            {
                "intent": "smart_home",
                "reply": "Okay.",
                "action": {
                    "device": "fan",
                    "state": "on",
                    "duration": "forever",
                },
            }
        )

