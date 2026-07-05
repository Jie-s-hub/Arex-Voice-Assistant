from __future__ import annotations

from enum import StrEnum

from pydantic import BaseModel, ConfigDict, Field, model_validator


class Device(StrEnum):
    BEDROOM_LIGHT = "bedroom_light"
    FAN = "fan"
    ALL = "all"


class PowerState(StrEnum):
    ON = "on"
    OFF = "off"


class HomeAction(BaseModel):
    model_config = ConfigDict(extra="forbid")

    device: Device
    state: PowerState

    @model_validator(mode="after")
    def all_only_supports_off(self) -> "HomeAction":
        if self.device is Device.ALL and self.state is not PowerState.OFF:
            raise ValueError("the 'all' device only supports the off state")
        return self


class Decision(BaseModel):
    model_config = ConfigDict(extra="forbid")

    intent: str = Field(pattern=r"^(conversation|smart_home)$")
    reply: str = Field(min_length=1, max_length=500)
    action: HomeAction | None


DECISION_JSON_SCHEMA: dict = {
    "type": "object",
    "properties": {
        "intent": {"type": "string", "enum": ["conversation", "smart_home"]},
        "reply": {"type": "string", "minLength": 1, "maxLength": 500},
        "action": {
            "anyOf": [
                {"type": "null"},
                {
                    "type": "object",
                    "properties": {
                        "device": {
                            "type": "string",
                            "enum": ["bedroom_light", "fan", "all"],
                        },
                        "state": {"type": "string", "enum": ["on", "off"]},
                    },
                    "required": ["device", "state"],
                    "additionalProperties": False,
                },
            ]
        },
    },
    "required": ["intent", "reply", "action"],
    "additionalProperties": False,
}

