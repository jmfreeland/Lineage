"""Shared MIDI file discovery for the Pattern Library pipeline and its
tooling (build_library.py, note_usage.py, the Note Mapper GUI page).
analyze.py (the original vocabulary pipeline) deliberately keeps its own
local copy rather than importing this, so this module never needs to
touch that file."""

from __future__ import annotations

from pathlib import Path

MIDI_EXTENSIONS = {".mid", ".midi"}


def find_midi_files(folder: Path) -> list[Path]:
    return sorted(p for p in folder.rglob("*") if p.suffix.lower() in MIDI_EXTENSIONS)
