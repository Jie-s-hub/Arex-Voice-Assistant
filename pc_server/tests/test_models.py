import pytest
from pydantic import ValidationError

from arex_server.models import Decision


def test_conversation_decision_is_valid():
    decision = Decision.model_validate(
        {"intent": "conversation", "reply": "Hello from Arex."}
    )

    assert decision.intent == "conversation"
    assert decision.reply == "Hello from Arex."


def test_action_fields_are_rejected():
    with pytest.raises(ValidationError):
        Decision.model_validate(
            {
                "intent": "conversation",
                "reply": "Okay.",
                "action": {"name": "unsupported_command"},
            }
        )
