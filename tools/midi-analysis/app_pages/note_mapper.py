"""Interactive drum note-map builder: scan a folder of MIDI files, see
every raw pitch actually in use and how it currently resolves to a voice
category, edit voices inline, and save/download the resulting
pitch->voice override JSON for --note-map (analyze.py/build_library.py).

Standalone from the rest of this app on purpose — it doesn't read or write
a pattern_library.json, since building a note map is a pre-processing step
that normally runs BEFORE build_library.py, not something you browse
after the fact. app.py renders this page without requiring a library to
be loaded first.
"""

from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
import streamlit as st

from lineage_midi_analysis.discovery import find_midi_files
from lineage_midi_analysis.drum_map import KNOWN_VOICES, midi_note_name, voice_for_pitch
from lineage_midi_analysis.parser import parse_midi_file


@st.cache_data(show_spinner="Scanning MIDI files...")
def _scan_folder(folder: str, note_map_items: tuple[tuple[int, str], ...]) -> pd.DataFrame:
    note_map = dict(note_map_items) if note_map_items else None
    midi_files = find_midi_files(Path(folder))

    counts: dict[int, int] = {}
    for path in midi_files:
        try:
            parsed = parse_midi_file(path, note_map=note_map)
        except Exception:  # noqa: BLE001 — one bad file shouldn't block the scan
            continue
        for note in parsed.notes:
            counts[note.pitch] = counts.get(note.pitch, 0) + 1

    rows = [
        {"pitch": pitch, "note": midi_note_name(pitch), "count": count, "voice": voice_for_pitch(pitch, note_map)[1]}
        for pitch, count in sorted(counts.items(), key=lambda kv: -kv[1])
    ]
    return pd.DataFrame(rows, columns=["pitch", "note", "count", "voice"])


def render() -> None:
    st.header("Note Mapper")
    st.caption(
        "Scan a folder of drum MIDI files, see every raw pitch in use and how it currently "
        "resolves to a voice, and edit the **voice** column to build a `--note-map` override for "
        "libraries (Superior Drummer, EZdrummer, ...) whose extra articulations — rimshots, "
        "chokes, flams, degrees of hi-hat openness — don't follow GM percussion numbering."
    )

    folder_col, upload_col = st.columns([2, 1])
    folder = folder_col.text_input("MIDI folder", placeholder="/path/to/your/midi/folder")
    starting_map_file = upload_col.file_uploader("Start from an existing map (optional)", type="json")

    starting_note_map: dict[int, str] = {}
    if starting_map_file is not None:
        starting_note_map = {int(k): v for k, v in json.loads(starting_map_file.getvalue()).items()}

    if not folder:
        st.info("Enter a folder path above to scan it.")
        return
    if not Path(folder).is_dir():
        st.error(f"{folder} is not a directory.")
        return

    table = _scan_folder(folder, tuple(sorted(starting_note_map.items())))
    if table.empty:
        st.error("No .mid/.midi files found (or no notes in them) under that folder.")
        return

    st.write(f"{len(table)} distinct pitch(es), {int(table['count'].sum())} note(s) total.")

    edited = st.data_editor(
        table,
        column_config={
            "pitch": st.column_config.NumberColumn("Pitch", disabled=True),
            "note": st.column_config.TextColumn("Note", disabled=True),
            "count": st.column_config.NumberColumn("Count", disabled=True),
            "voice": st.column_config.TextColumn(
                "Voice", help=f"Typical categories: {', '.join(KNOWN_VOICES)} — or your own."
            ),
        },
        hide_index=True,
        width="stretch",
        height=min(560, 40 + 36 * len(table)),
        key=f"note_mapper_editor::{folder}",
    )

    note_map_out = {
        str(int(row["pitch"])): row["voice"]
        for _, row in edited.iterrows()
        if isinstance(row["voice"], str) and row["voice"].strip()
    }
    json_text = json.dumps(note_map_out, indent=2)

    st.download_button("Download note_map.json", data=json_text, file_name="note_map.json", mime="application/json")

    save_path_col, save_button_col = st.columns([3, 1])
    save_path = save_path_col.text_input("...or save directly to a path", value=str(Path(folder) / "note_map.json"))
    if save_button_col.button("Save"):
        Path(save_path).write_text(json_text)
        st.success(f"Saved to {save_path}")

    st.caption(f"Once you're happy with this map, pass it to the mining tools: `--note-map {save_path}`")
