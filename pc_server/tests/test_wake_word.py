from arex_server.ai import split_wake_word


def test_wake_word_detects_arex_prefix():
    detected, command = split_wake_word(
        "Arex, what is the weather?", ("Arex", "a rex")
    )

    assert detected is True
    assert command == "what is the weather"


def test_wake_word_detects_common_alias():
    detected, command = split_wake_word("Hey Alex set a timer", ("Arex", "Alex"))

    assert detected is True
    assert command == "set a timer"


def test_wake_word_rejects_background_speech():
    detected, command = split_wake_word("what is the weather?", ("Arex", "a rex"))

    assert detected is False
    assert command == ""
