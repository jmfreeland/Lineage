#!/usr/bin/env python3
"""Report the raw MIDI pitches actually used across a folder of drum MIDI
files, and how each currently resolves to a voice category — the fastest
way to build a --note-map override for a drum library whose export
doesn't follow General MIDI percussion numbering beyond the core hits.

Real drum-library exports (Superior Drummer, EZdrummer, and most sample
libraries' "MIDI grooves") usually put kick/snare/hats on the familiar GM
numbers as a matter of practical convention, but their many extra
articulations — rimshots, chokes, flams, degrees of hi-hat openness — get
their own note numbers GM has no opinion about, and would otherwise all
collapse into a generic "other" bucket in analyze.py/build_library.py.

    python3 note_usage.py <folder> [--note-map existing.json] [--emit-template map.json]

Typical workflow: run this once with no --note-map to see every pitch your
library actually uses; run it again with --emit-template to get a starter
JSON file (one entry per pitch seen, pre-filled with the current GM-or-
"other" guess); edit the wrong/"other" entries by hand; pass the result
back in via --note-map to both this script and analyze.py/build_library.py.

Note names use the convention "middle C (60) = C3", matching the plugin's
own StepSequencerComponent note-name display.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lineage_midi_analysis.discovery import find_midi_files
from lineage_midi_analysis.drum_map import load_note_map, midi_note_name, voice_for_pitch
from lineage_midi_analysis.parser import parse_midi_file


def main() -> int:
    parser = argparse.ArgumentParser(description="Report raw MIDI pitch usage across a folder of drum MIDI files")
    parser.add_argument("folder", type=Path, help="Folder of .mid/.midi files (searched recursively)")
    parser.add_argument(
        "--note-map", type=Path, default=None, help="Existing pitch->voice override JSON to apply before reporting"
    )
    parser.add_argument(
        "--emit-template",
        type=Path,
        default=None,
        help="Write a starter pitch->voice JSON file, one entry per pitch actually seen, for you to edit",
    )
    args = parser.parse_args()

    if not args.folder.is_dir():
        print(f"error: {args.folder} is not a directory", file=sys.stderr)
        return 1

    midi_files = find_midi_files(args.folder)
    if not midi_files:
        print(f"error: no .mid/.midi files found under {args.folder}", file=sys.stderr)
        return 1

    note_map = load_note_map(args.note_map) if args.note_map else None

    pitch_counts: Counter[int] = Counter()
    for path in midi_files:
        try:
            parsed = parse_midi_file(path, note_map=note_map)
        except Exception as exc:  # noqa: BLE001 — one bad file shouldn't kill the whole batch
            print(f"warning: skipping {path} ({exc})", file=sys.stderr)
            continue
        pitch_counts.update(note.pitch for note in parsed.notes)

    if not pitch_counts:
        print("error: no notes found in any file", file=sys.stderr)
        return 1

    print(f"{len(midi_files)} file(s), {sum(pitch_counts.values())} note(s), {len(pitch_counts)} distinct pitch(es)\n")
    print(f"{'pitch':>5}  {'note':<5}  {'voice':<14}  source   count")
    for pitch, count in pitch_counts.most_common():
        gm_name, voice = voice_for_pitch(pitch, note_map)
        if note_map is not None and pitch in note_map:
            source = "override"
        elif voice != "other":
            source = "GM"
        else:
            source = "-"
        print(f"{pitch:>5}  {midi_note_name(pitch):<5}  {voice:<14}  {source:<8} {count}")

    if args.emit_template:
        template = {str(pitch): voice_for_pitch(pitch, note_map)[1] for pitch in sorted(pitch_counts)}
        args.emit_template.write_text(json.dumps(template, indent=2))
        print(
            f"\nWrote a starter note map ({len(template)} pitch(es)) to {args.emit_template} — "
            "edit any 'other' (or wrong) entries, then pass it back in via --note-map."
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
