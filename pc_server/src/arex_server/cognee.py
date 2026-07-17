from __future__ import annotations

import json
import re
import uuid
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path

from .models import Decision


_WORD_RE = re.compile(r"[a-z0-9]+")


@dataclass(frozen=True, slots=True)
class Memory:
    id: str
    device_id: str
    text: str
    source: str
    created_at: str
    updated_at: str

    @classmethod
    def from_dict(cls, data: dict[str, str]) -> "Memory":
        return cls(
            id=data["id"],
            device_id=data["device_id"],
            text=data["text"],
            source=data.get("source", "explicit"),
            created_at=data["created_at"],
            updated_at=data.get("updated_at", data["created_at"]),
        )

    def to_dict(self) -> dict[str, str]:
        return {
            "id": self.id,
            "device_id": self.device_id,
            "text": self.text,
            "source": self.source,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
        }


class CogneeMemory:
    """Small local long-term memory store for the Arex audio device.

    Cognee intentionally stores only explicit memories: the user has to say
    "remember that ...". This avoids silently collecting personal information
    during a school demonstration.
    """

    def __init__(self, path: Path, *, max_memories_per_device: int = 80) -> None:
        self.path = path
        self.max_memories_per_device = max_memories_per_device

    def handle_command(self, device_id: str, transcript: str) -> Decision | None:
        cleaned = _clean(transcript)
        command = cleaned.lower().strip(" .!?")

        remember_match = re.match(
            r"^(?:please\s+)?remember\s+(?:that\s+)?(.+)$", command, re.IGNORECASE
        )
        if remember_match:
            fact = cleaned[remember_match.start(1) :].strip(" .!?")
            memory = self.remember(device_id, fact)
            return Decision(
                intent="conversation",
                reply=f"I'll remember that {memory.text}.",
            )

        note_match = re.match(
            r"^(?:please\s+)?(?:save|store)\s+(?:this\s+)?(?:memory|note):?\s+(.+)$",
            command,
            re.IGNORECASE,
        )
        if note_match:
            fact = cleaned[note_match.start(1) :].strip(" .!?")
            memory = self.remember(device_id, fact)
            return Decision(
                intent="conversation",
                reply=f"Saved in Cognee memory: {memory.text}.",
            )

        if re.match(
            r"^(?:what\s+do\s+you\s+remember|what\s+do\s+you\s+know\s+about\s+me|tell\s+me\s+what\s+you\s+remember)\??$",
            command,
        ):
            memories = self.recall(device_id, limit=6)
            if not memories:
                reply = "I don't have any saved memories yet. Say, remember that my favorite color is blue."
            else:
                facts = "; ".join(memory.text for memory in memories)
                reply = f"I remember: {facts}."
            return Decision(intent="conversation", reply=reply)

        if re.match(
            r"^(?:forget|delete|clear)\s+(?:everything|my\s+memory|all\s+memories)$",
            command,
        ):
            removed = self.clear(device_id)
            reply = (
                "I cleared your Cognee memory."
                if removed
                else "Cognee memory is already empty."
            )
            return Decision(intent="conversation", reply=reply)

        forget_match = re.match(r"^(?:please\s+)?forget\s+(?:that\s+)?(.+)$", command)
        if forget_match:
            target = cleaned[forget_match.start(1) :].strip(" .!?")
            removed = self.forget(device_id, target)
            reply = (
                f"I forgot {removed} matching memory."
                if removed == 1
                else f"I forgot {removed} matching memories."
                if removed
                else "I could not find a matching memory to forget."
            )
            return Decision(intent="conversation", reply=reply)

        return None

    def remember(self, device_id: str, text: str, *, source: str = "explicit") -> Memory:
        text = _clean(text).strip(" .!?")
        if not text:
            raise ValueError("memory text is empty")

        memories = self._load()
        now = _now()
        normalized = _normalize(text)
        updated: Memory | None = None
        next_memories: list[Memory] = []

        for memory in memories:
            if memory.device_id == device_id and _normalize(memory.text) == normalized:
                updated = Memory(
                    id=memory.id,
                    device_id=memory.device_id,
                    text=text,
                    source=source,
                    created_at=memory.created_at,
                    updated_at=now,
                )
                next_memories.append(updated)
            else:
                next_memories.append(memory)

        if updated is None:
            updated = Memory(
                id=f"mem-{uuid.uuid4().hex[:12]}",
                device_id=device_id,
                text=text,
                source=source,
                created_at=now,
                updated_at=now,
            )
            next_memories.append(updated)

        next_memories = self._trim_device(next_memories, device_id)
        self._save(next_memories)
        return updated

    def recall(
        self, device_id: str, query: str = "", *, limit: int = 5
    ) -> list[Memory]:
        memories = [memory for memory in self._load() if memory.device_id == device_id]
        if not query:
            return sorted(memories, key=lambda memory: memory.updated_at, reverse=True)[
                :limit
            ]

        query_tokens = set(_tokens(query))
        scored: list[tuple[int, str, Memory]] = []
        for memory in memories:
            memory_tokens = set(_tokens(memory.text))
            score = len(query_tokens & memory_tokens)
            if score:
                scored.append((score, memory.updated_at, memory))
        scored.sort(key=lambda item: (item[0], item[1]), reverse=True)
        return [item[2] for item in scored[:limit]]

    def format_context(self, device_id: str, query: str, *, limit: int = 5) -> str:
        memories = self.recall(device_id, query, limit=limit)
        if not memories:
            memories = self.recall(device_id, limit=limit)
        if not memories:
            return "(none)"
        return "\n".join(f"- {memory.text}" for memory in memories)

    def forget(self, device_id: str, query: str) -> int:
        query_normalized = _normalize(query)
        query_tokens = set(_tokens(query))
        kept: list[Memory] = []
        removed = 0
        for memory in self._load():
            if memory.device_id != device_id:
                kept.append(memory)
                continue
            memory_normalized = _normalize(memory.text)
            memory_tokens = set(_tokens(memory.text))
            is_match = (
                query_normalized in memory_normalized
                or memory_normalized in query_normalized
                or len(query_tokens & memory_tokens) >= max(1, min(3, len(query_tokens)))
            )
            if is_match:
                removed += 1
            else:
                kept.append(memory)
        if removed:
            self._save(kept)
        return removed

    def clear(self, device_id: str) -> int:
        kept: list[Memory] = []
        removed = 0
        for memory in self._load():
            if memory.device_id == device_id:
                removed += 1
            else:
                kept.append(memory)
        if removed:
            self._save(kept)
        return removed

    def count(self, device_id: str | None = None) -> int:
        memories = self._load()
        if device_id is None:
            return len(memories)
        return sum(1 for memory in memories if memory.device_id == device_id)

    def _load(self) -> list[Memory]:
        if not self.path.exists():
            return []
        data = json.loads(self.path.read_text(encoding="utf-8"))
        return [Memory.from_dict(item) for item in data.get("memories", [])]

    def _save(self, memories: list[Memory]) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "version": 1,
            "memories": [memory.to_dict() for memory in memories],
        }
        temp_path = self.path.with_suffix(self.path.suffix + ".tmp")
        temp_path.write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        temp_path.replace(self.path)

    def _trim_device(self, memories: list[Memory], device_id: str) -> list[Memory]:
        device_memories = [memory for memory in memories if memory.device_id == device_id]
        if len(device_memories) <= self.max_memories_per_device:
            return memories

        keep_ids = {
            memory.id
            for memory in sorted(
                device_memories, key=lambda memory: memory.updated_at, reverse=True
            )[: self.max_memories_per_device]
        }
        return [
            memory
            for memory in memories
            if memory.device_id != device_id or memory.id in keep_ids
        ]


def _clean(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def _normalize(text: str) -> str:
    return " ".join(_tokens(text))


def _tokens(text: str) -> list[str]:
    return _WORD_RE.findall(text.lower())


def _now() -> str:
    return datetime.now(UTC).replace(microsecond=0).isoformat()

