from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field


class Decision(BaseModel):
    model_config = ConfigDict(extra="forbid")

    intent: str = Field(pattern=r"^conversation$")
    reply: str = Field(min_length=1, max_length=500)


DECISION_JSON_SCHEMA: dict = {
    "type": "object",
    "properties": {
        "intent": {"type": "string", "enum": ["conversation"]},
        "reply": {"type": "string", "minLength": 1, "maxLength": 500},
    },
    "required": ["intent", "reply"],
    "additionalProperties": False,
}
