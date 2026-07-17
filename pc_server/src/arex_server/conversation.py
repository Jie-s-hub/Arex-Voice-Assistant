from __future__ import annotations

from collections import defaultdict, deque


class ConversationStore:
    def __init__(self, max_turns: int = 4) -> None:
        self._turns: dict[str, deque[tuple[str, str]]] = defaultdict(
            lambda: deque(maxlen=max_turns)
        )

    def history(self, device_id: str) -> tuple[tuple[str, str], ...]:
        return tuple(self._turns[device_id])

    def add(self, device_id: str, user: str, assistant: str) -> None:
        self._turns[device_id].append((user, assistant))

