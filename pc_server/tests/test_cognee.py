from aura_server.ai import AuraAI
from aura_server.config import Settings
from aura_server.cognee import CogneeMemory


def test_cognee_remembers_and_recalls_explicit_fact(tmp_path):
    memory = CogneeMemory(tmp_path / "memory.json")

    saved = memory.remember("audio-1", "my favorite color is blue")

    assert saved.text == "my favorite color is blue"
    assert memory.count("audio-1") == 1
    assert memory.recall("audio-1", "what is my favorite color")[0].text == (
        "my favorite color is blue"
    )


def test_cognee_keeps_audio_device_memories_separate(tmp_path):
    memory = CogneeMemory(tmp_path / "memory.json")

    memory.remember("audio-1", "my display is a demo OLED")
    memory.remember("audio-2", "my favorite snack is mango")

    assert "demo OLED" in memory.format_context("audio-1", "display")
    assert "mango" not in memory.format_context("audio-1", "display")


def test_cognee_forgets_matching_memory(tmp_path):
    memory = CogneeMemory(tmp_path / "memory.json")
    memory.remember("audio-1", "my favorite color is blue")
    memory.remember("audio-1", "my favorite snack is mango")

    removed = memory.forget("audio-1", "color blue")

    assert removed == 1
    assert memory.count("audio-1") == 1
    assert "mango" in memory.format_context("audio-1", "favorite snack")


def test_ai_handles_memory_commands_without_openai_call(tmp_path):
    ai = AuraAI(
        Settings(openai_api_key="sk-test", cognee_memory_path=tmp_path / "memory.json")
    )

    remember = ai.decide("remember that my demo device name is AURA", (), "audio-1")
    recall = ai.decide("what do you remember", (), "audio-1")

    assert remember.intent == "conversation"
    assert "I'll remember" in remember.reply
    assert "demo device name is AURA" in recall.reply


def test_memory_can_be_disabled(tmp_path):
    ai = AuraAI(
        Settings(
            openai_api_key="sk-test",
            enable_cognee=False,
            cognee_memory_path=tmp_path / "memory.json",
        )
    )

    assert ai.memory is None


def test_memory_clear_command_accepts_sentence_punctuation(tmp_path):
    memory = CogneeMemory(tmp_path / "memory.json")
    memory.remember("audio-1", "my device is called AURA")

    decision = memory.handle_command("audio-1", "Forget everything.")

    assert decision is not None
    assert "cleared" in decision.reply
    assert memory.count("audio-1") == 0

