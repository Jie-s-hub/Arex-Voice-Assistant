from arex_server.ai import ArexAI
from arex_server.config import Settings


def test_web_search_tool_is_enabled_by_default():
    ai = ArexAI(Settings(openai_api_key="sk-test"))

    assert ai.response_tools() == [
        {"type": "web_search", "search_context_size": "low"}
    ]


def test_web_search_tool_can_be_disabled():
    ai = ArexAI(Settings(openai_api_key="sk-test", enable_web_search=False))

    assert ai.response_tools() is None


def test_source_code_has_no_default_openai_key(monkeypatch):
    monkeypatch.delenv("OPENAI_API_KEY", raising=False)

    assert Settings().openai_api_key == ""
