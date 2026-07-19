"""Per-bar diffing against a detected base pattern.

Thresholds (TIMING_THRESHOLD_MS, VELOCITY_THRESHOLD, FILL_THRESHOLD,
DENSITY_RATIO) are deliberately simple, hardcoded defaults rather than a
tuned model — good enough to produce a usable vocabulary, not a claim that
these are the "correct" perceptual thresholds. Revisit with real corpus
results once there's a corpus to look at.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from fractions import Fraction

from .parser import NoteEvent, ParsedMidi
from .patterns import BarSlot, BasePattern, quantized_slot
from .quantize import beat_within_bar

TIMING_THRESHOLD_MS = 5.0
VELOCITY_THRESHOLD = 8
FILL_THRESHOLD = 0.5
DENSITY_RATIO = 2.0


@dataclass
class DiffEntry:
    category: str
    voice: str
    bar_index: int
    position_beat: Fraction
    magnitude: float | None = None
    direction: str | None = None
    detail: str | None = None


@dataclass
class BarResult:
    bar_index: int
    diffs: list[DiffEntry]
    departure_score: float
    is_fill: bool


def diff_bar(
    bar_index_value: int,
    bar_notes: list[NoteEvent],
    base_pattern: BasePattern,
    beats_per_bar: Fraction,
    grid: int,
    parsed: ParsedMidi,
    fill_threshold: float = FILL_THRESHOLD,
) -> BarResult:
    actual_by_slot: dict[BarSlot, list[NoteEvent]] = defaultdict(list)
    for note in bar_notes:
        actual_by_slot[quantized_slot(note, beats_per_bar, grid)].append(note)

    diffs: list[DiffEntry] = []
    irregular_count = 0

    # Base-pattern slots: matched (timing/velocity variation) or omitted.
    for slot in base_pattern.slots:
        voice, position = slot
        notes_here = actual_by_slot.get(slot)
        if not notes_here:
            diffs.append(DiffEntry("omission", voice, bar_index_value, position))
            irregular_count += 1
            continue

        note = notes_here[0]
        local_beat = beat_within_bar(note.start_beat, beats_per_bar)
        offset_beats = local_beat - position
        offset_ms = parsed.beats_to_ms(offset_beats)
        if abs(offset_ms) > TIMING_THRESHOLD_MS:
            diffs.append(
                DiffEntry(
                    "timing_shift",
                    voice,
                    bar_index_value,
                    position,
                    magnitude=offset_ms,
                    direction="late" if offset_ms > 0 else "early",
                )
            )

        expected_velocity = base_pattern.slot_avg_velocity.get(slot, note.velocity)
        velocity_delta = note.velocity - expected_velocity
        if abs(velocity_delta) > VELOCITY_THRESHOLD:
            diffs.append(
                DiffEntry(
                    "velocity_variation",
                    voice,
                    bar_index_value,
                    position,
                    magnitude=velocity_delta,
                    direction="louder" if velocity_delta > 0 else "softer",
                )
            )

    # Slots that exist in this bar but not in the base pattern: substitution
    # if it's standing in for a base voice still missing at this position,
    # embellishment if it's layered on top of a position whose expected
    # voices are already all present. A position can have more than one
    # expected voice (e.g. kick and hi-hat both land on beat 2), so
    # "which voice does this replace" is resolved against whichever base
    # voices are *actually* unaccounted for at this position — not just an
    # arbitrary pick off the position's full expected set, which would be
    # non-deterministic (frozenset iteration order) whenever more than one
    # base voice shares a position.
    expected_voices_by_position: dict[Fraction, set[str]] = defaultdict(set)
    for voice, position in base_pattern.slots:
        expected_voices_by_position[position].add(voice)
    actual_voices_by_position: dict[Fraction, set[str]] = defaultdict(set)
    for voice, position in actual_by_slot:
        actual_voices_by_position[position].add(voice)

    for slot in actual_by_slot:
        if slot in base_pattern.slots:
            continue
        voice, position = slot
        missing_here = expected_voices_by_position.get(position, set()) - actual_voices_by_position.get(
            position, set()
        )
        if missing_here:
            diffs.append(
                DiffEntry(
                    "substitution",
                    voice,
                    bar_index_value,
                    position,
                    detail=f"replaces {', '.join(sorted(missing_here))}",
                )
            )
        else:
            diffs.append(DiffEntry("embellishment", voice, bar_index_value, position))
        irregular_count += 1

    # Whole-voice density change: a voice's hit count in this bar is far
    # above/below its count in the base pattern (e.g. hi-hats doubled to
    # 16ths). Complementary to the per-slot embellishment entries above,
    # not a replacement for them.
    base_voice_counts: dict[str, int] = defaultdict(int)
    for voice, _position in base_pattern.slots:
        base_voice_counts[voice] += 1
    actual_voice_counts: dict[str, int] = defaultdict(int)
    for voice, _position in actual_by_slot:
        actual_voice_counts[voice] += 1

    for voice, base_count in base_voice_counts.items():
        actual_count = actual_voice_counts.get(voice, 0)
        if base_count == 0:
            continue
        if actual_count >= base_count * DENSITY_RATIO:
            diffs.append(DiffEntry("density_change", voice, bar_index_value, Fraction(0), direction="increase"))
        elif actual_count <= base_count / DENSITY_RATIO and actual_count > 0:
            diffs.append(DiffEntry("density_change", voice, bar_index_value, Fraction(0), direction="decrease"))

    departure_score = irregular_count / max(1, len(base_pattern.slots))
    is_fill = departure_score > fill_threshold

    return BarResult(bar_index=bar_index_value, diffs=diffs, departure_score=departure_score, is_fill=is_fill)
