"""Per-file analysis and cross-file aggregation into the vocabulary schema."""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from fractions import Fraction

from .diff import BarResult, DiffEntry, diff_bar
from .parser import ParsedMidi, parse_midi_file
from .patterns import BasePattern, find_base_pattern, group_by_bar

SCHEMA_VERSION = 1


@dataclass
class FileAnalysis:
    source_path: str
    parsed: ParsedMidi
    base_pattern: BasePattern
    bar_results: list[BarResult]


def analyze_file(path: str, grid: int, fill_threshold: float) -> FileAnalysis:
    parsed = parse_midi_file(path)
    bars_by_index = group_by_bar(parsed.notes, parsed.beats_per_bar)
    base_pattern = find_base_pattern(bars_by_index, parsed.beats_per_bar, grid)

    bar_results = [
        diff_bar(idx, bars_by_index[idx], base_pattern, parsed.beats_per_bar, grid, parsed, fill_threshold)
        for idx in sorted(bars_by_index)
    ]

    return FileAnalysis(source_path=path, parsed=parsed, base_pattern=base_pattern, bar_results=bar_results)


def _base_pattern_to_dict(analysis: FileAnalysis, pattern_id: str) -> dict:
    voices: dict[str, list[float]] = defaultdict(list)
    for voice, position in sorted(analysis.base_pattern.slots, key=lambda s: (s[0], s[1])):
        voices[voice].append(float(position))
    return {
        "id": pattern_id,
        "source_file": analysis.source_path,
        "beats_per_bar": float(analysis.parsed.beats_per_bar),
        "voices": dict(voices),
        "occurrences": analysis.base_pattern.occurrences,
    }


def _variation_key(entry: DiffEntry, beats_per_bar: Fraction) -> tuple[str, str, Fraction]:
    # Position normalized to fraction-through-the-bar so entries from files
    # in different time signatures aggregate meaningfully together.
    position_fraction = entry.position_beat / beats_per_bar if beats_per_bar else Fraction(0)
    return (entry.category, entry.voice, position_fraction)


def build_vocabulary(analyses: list[FileAnalysis], fill_threshold: float) -> dict:
    base_patterns = [_base_pattern_to_dict(a, f"bp_{i + 1:02d}") for i, a in enumerate(analyses)]

    variation_groups: dict[tuple[str, str, Fraction], list[DiffEntry]] = defaultdict(list)
    non_fill_bar_count = 0
    fills: list[dict] = []

    for analysis in analyses:
        beats_per_bar = analysis.parsed.beats_per_bar
        for bar_result in analysis.bar_results:
            if bar_result.is_fill:
                fills.append(
                    {
                        "source_file": analysis.source_path,
                        "bar_index": bar_result.bar_index,
                        "departure_score": round(bar_result.departure_score, 3),
                        "phrase_boundary": bar_result.bar_index % 4 == 3,
                    }
                )
                continue
            non_fill_bar_count += 1
            for entry in bar_result.diffs:
                variation_groups[_variation_key(entry, beats_per_bar)].append(entry)

    variations = []
    for (category, voice, position_fraction), entries in sorted(
        variation_groups.items(), key=lambda kv: (kv[0][0], kv[0][1], float(kv[0][2]))
    ):
        magnitudes = [e.magnitude for e in entries if e.magnitude is not None]
        variation: dict = {
            "category": category,
            "voice": voice,
            "metric_position": round(float(position_fraction), 4),
            "frequency": round(len(entries) / max(1, non_fill_bar_count), 4),
            "occurrences": len(entries),
        }
        if magnitudes:
            variation["avg_magnitude"] = round(sum(magnitudes) / len(magnitudes), 3)
        directions = [e.direction for e in entries if e.direction]
        if directions:
            # Most common direction, e.g. mostly "late" vs mostly "early".
            variation["direction"] = max(set(directions), key=directions.count)
        details = {e.detail for e in entries if e.detail}
        if details:
            variation["detail"] = sorted(details)
        variations.append(variation)

    return {
        "schema_version": SCHEMA_VERSION,
        "source_files": [a.source_path for a in analyses],
        "base_patterns": base_patterns,
        "variations": variations,
        "fills": fills,
    }
