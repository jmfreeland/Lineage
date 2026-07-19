"""Bar splitting and base-pattern detection.

Clustering is exact-match on the quantized (voice, position) fingerprint —
not fuzzy/edit-distance similarity. That's the simplest thing that could
work and it's fully deterministic/testable, but it means two bars that are
"almost" the same pattern (one extra ghost note) count as different
clusters rather than variations of one cluster. Documented as a known
limitation (see README) rather than solved here — fuzzy clustering is a
reasonable v2 improvement.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from fractions import Fraction

from .parser import NoteEvent
from .quantize import bar_index, beat_within_bar, quantize_beat

BarSlot = tuple[str, Fraction]  # (voice, quantized position within bar)


def group_by_bar(notes: list[NoteEvent], beats_per_bar: Fraction) -> dict[int, list[NoteEvent]]:
    bars: dict[int, list[NoteEvent]] = defaultdict(list)
    for note in notes:
        bars[bar_index(note.start_beat, beats_per_bar)].append(note)
    return dict(bars)


def quantized_slot(note: NoteEvent, beats_per_bar: Fraction, grid: int) -> BarSlot:
    local_beat = beat_within_bar(note.start_beat, beats_per_bar)
    return (note.voice, quantize_beat(local_beat, grid))


def fingerprint(bar_notes: list[NoteEvent], beats_per_bar: Fraction, grid: int) -> frozenset[BarSlot]:
    return frozenset(quantized_slot(n, beats_per_bar, grid) for n in bar_notes)


@dataclass
class BasePattern:
    slots: frozenset[BarSlot]
    slot_avg_velocity: dict[BarSlot, float]
    bar_indices: list[int]

    @property
    def occurrences(self) -> int:
        return len(self.bar_indices)


def find_base_pattern(bars_by_index: dict[int, list[NoteEvent]], beats_per_bar: Fraction, grid: int) -> BasePattern:
    if not bars_by_index:
        return BasePattern(slots=frozenset(), slot_avg_velocity={}, bar_indices=[])

    clusters: dict[frozenset[BarSlot], list[int]] = defaultdict(list)
    for idx in sorted(bars_by_index):
        clusters[fingerprint(bars_by_index[idx], beats_per_bar, grid)].append(idx)

    # Most frequent cluster wins; ties broken by earliest first occurrence.
    best_fp = max(clusters, key=lambda fp: (len(clusters[fp]), -clusters[fp][0]))
    bar_indices = clusters[best_fp]

    velocity_sums: dict[BarSlot, float] = defaultdict(float)
    velocity_counts: dict[BarSlot, int] = defaultdict(int)
    for idx in bar_indices:
        for note in bars_by_index[idx]:
            slot = quantized_slot(note, beats_per_bar, grid)
            velocity_sums[slot] += note.velocity
            velocity_counts[slot] += 1

    slot_avg_velocity = {slot: velocity_sums[slot] / velocity_counts[slot] for slot in velocity_sums}

    return BasePattern(slots=best_fp, slot_avg_velocity=slot_avg_velocity, bar_indices=bar_indices)
