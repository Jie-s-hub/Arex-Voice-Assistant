from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TEXT_SUFFIXES = {
    ".cpp",
    ".example",
    ".gitattributes",
    ".gitignore",
    ".h",
    ".ini",
    ".ino",
    ".json",
    ".md",
    ".py",
    ".toml",
    ".txt",
    ".yaml",
    ".yml",
}
SKIP_PARTS = {
    ".git",
    ".pio",
    ".pytest_cache",
    ".venv",
    "__pycache__",
}
MOJIBAKE_MARKERS = tuple(
    chr(codepoint)
    for codepoint in (
        0x9225,
        0x9239,
        0xFFFD,
        0x6522,
        0x6DE5,
        0x6DE9,
        0x8133,
        0x60DF,
        0x922B,
        0x922D,
        0x6DED,
    )
) + (
    "\N{LATIN SMALL LETTER A WITH CIRCUMFLEX}\N{EURO SIGN}",
    "\N{LATIN CAPITAL LETTER I WITH CIRCUMFLEX}\N{COPYRIGHT SIGN}",
    "\N{LATIN CAPITAL LETTER A WITH TILDE}\N{MULTIPLICATION SIGN}",
    "\N{LATIN SMALL LETTER A WITH CIRCUMFLEX}\N{DAGGER}",
    "\N{LATIN SMALL LETTER A WITH CIRCUMFLEX}\N{MODIFIER LETTER CIRCUMFLEX ACCENT}",
)


def iter_text_files():
    for path in REPO_ROOT.rglob("*"):
        if any(part in SKIP_PARTS for part in path.parts):
            continue
        if path.is_file() and path.suffix.lower() in TEXT_SUFFIXES:
            yield path


def test_repository_text_files_decode_as_utf8():
    for path in iter_text_files():
        path.read_text(encoding="utf-8")


def test_repository_text_files_do_not_contain_known_mojibake():
    for path in iter_text_files():
        text = path.read_text(encoding="utf-8")
        assert not any(marker in text for marker in MOJIBAKE_MARKERS), path
