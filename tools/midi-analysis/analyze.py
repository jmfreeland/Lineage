#!/usr/bin/env python3
"""CLI: mine a folder of drum MIDI files into a vocabulary.json.

    python3 analyze.py <folder> --out vocabulary.json [--grid 16] [--fill-threshold 0.5]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lineage_midi_analysis.diff import FILL_THRESHOLD
from lineage_midi_analysis.vocabulary import analyze_file, build_vocabulary

MIDI_EXTENSIONS = {".mid", ".midi"}


def find_midi_files(folder: Path) -> list[Path]:
    return sorted(p for p in folder.rglob("*") if p.suffix.lower() in MIDI_EXTENSIONS)


def main() -> int:
    parser = argparse.ArgumentParser(description="Mine a folder of drum MIDI files into a vocabulary.json")
    parser.add_argument("folder", type=Path, help="Folder of .mid/.midi files (searched recursively)")
    parser.add_argument("--out", type=Path, default=Path("vocabulary.json"), help="Output JSON path")
    parser.add_argument("--grid", type=int, default=16, help="Quantization grid: 16 or 32 (default 16)")
    parser.add_argument(
        "--fill-threshold",
        type=float,
        default=FILL_THRESHOLD,
        help=f"Departure score above which a bar is flagged as a fill (default {FILL_THRESHOLD})",
    )
    args = parser.parse_args()

    if not args.folder.is_dir():
        print(f"error: {args.folder} is not a directory", file=sys.stderr)
        return 1

    midi_files = find_midi_files(args.folder)
    if not midi_files:
        print(f"error: no .mid/.midi files found under {args.folder}", file=sys.stderr)
        return 1

    analyses = []
    for path in midi_files:
        try:
            analyses.append(analyze_file(str(path), args.grid, args.fill_threshold))
        except Exception as exc:  # noqa: BLE001 — one bad file shouldn't kill the whole batch
            print(f"warning: skipping {path} ({exc})", file=sys.stderr)

    if not analyses:
        print("error: no files could be analyzed", file=sys.stderr)
        return 1

    vocabulary = build_vocabulary(analyses, args.fill_threshold)
    args.out.write_text(json.dumps(vocabulary, indent=2))

    print(f"Analyzed {len(analyses)}/{len(midi_files)} file(s) -> {args.out}")
    print(f"  {len(vocabulary['base_patterns'])} base pattern(s), "
          f"{len(vocabulary['variations'])} variation type(s), "
          f"{len(vocabulary['fills'])} fill(s) flagged")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
