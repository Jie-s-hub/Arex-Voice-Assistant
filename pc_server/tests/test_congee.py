from aura_server.ai import AuraAI
from aura_server.config import Settings
from aura_server.congee import CongeeMemory


def test_congee_remembers_and_recalls_explicit_fact(tmp_path):
    memory = CongeeMemory(tmp_path / "memory.json")

    saved = memory.remember("rover-1", "my favorite color is blue")

    assert saved.text == "my favorite color is blue"
    assert memory.count("rover-1") == 1
    assert memory.recall("rover-1", "what is my favorite color")[0].text == (
        "my favorite color is blue"
    )


def test_congee_keeps_rover_memories_separate(tmp_path):
    memory = CongeeMemory(tmp_path / "memory.json")

    memory.remember("rover-1", "my bedroom light is a demo LED")
    memory.remember("rover-2", "my favorite snack is mango")

    assert "demo LED" in memory.format_context("rover-1", "bedroom light")
    assert "mango" not in memory.format_context("rover-1", "bedroom light")


def test_congee_forgets_matching_memory(tmp_path):
    memory = CongeeMemory(tmp_path / "memory.json")
    memory.remember("rover-1", "my favorite color is blue")
    memory.remember("rover-1", "my favorite snack is mango")

    removed = memory.forget("rover-1", "color blue")

    assert removed == 1
    assert memory.count("rover-1") == 1
    assert "mango" in memory.format_context("rover-1", "favorite snack")


def test_ai_handles_memory_commands_without_openai_call(tmp_path):
    ai = AuraAI(
        Settings(openai_api_key="sk-test", memory_path=tmp_path / "memory.json")
    )

    remember = ai.decide("remember that my demo robot name is AURA", (), "rover-1")
    recall = ai.decide("what do you remember", (), "rover-1")

    assert remember.intent == "conversation"
    assert "I'll remember" in remember.reply
    assert "demo robot name is AURA" in recall.reply


def test_memory_can_be_disabled(tmp_path):
    ai = AuraAI(
        Settings(
            openai_api_key="sk-test",
            enable_memory=False,
            memory_path=tmp_path / "memory.json",
        )
    )

    assert ai.memory is None


def test_memory_clear_command_accepts_sentence_punctuation(tmp_path):
    memory = CongeeMemory(tmp_path / "memory.json")
    memory.remember("rover-1", "my robot is called AURA")

    decision = memory.handle_command("rover-1", "Forget everything.")

    assert decision is not None
    assert "cleared" in decision.reply
    assert memory.count("rover-1") == 0
