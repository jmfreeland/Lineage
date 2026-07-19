"""Corpus-wide unique-pattern distillation.

Distinct from patterns.find_base_pattern(), which finds the single most
frequent bar shape *within one file*. This module finds every distinct
bar shape *across the whole corpus*, with a real frequency count for each
— the raw material for distance/clustering (distance.py, clustering.py)
and transition analysis (transitions.py), neither of which make sense
against a single per-file "the" pattern.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass, field
from fractions import Fraction

from .fingerprint import CanonicalSlot, SlotVelocityProfile, canonical_fingerprint, velocity_profile
from .fingerprint import CANONICAL_GRID
from .parser import NoteEvent
from .quantize import steps_per_beat


@dataclass
class SourceBar:
    source_file: str
    bar_index: int
    beats_per_bar: Fraction
    notes: list[NoteEvent]


@dataclass
class DistilledPattern:
    pattern_id: str
    fingerprint: frozenset[CanonicalSlot]
    voices: dict[str, list[float]]
    beats_per_bar: float
    occurrences: int = 0
    occurrence_refs: list[tuple[str, int]] = field(default_factory=list)
    slot_velocity: dict[CanonicalSlot, SlotVelocityProfile] = field(default_factory=dict)

    @property
    def note_count(self) -> int:
        return sum(len(positions) for positions in self.voices.values())

    @property
    def density(self) -> float:
        """note_count relative to how many canonical slots this pattern's
        bar length has — a normalized "how busy is this bar" measure that's
        comparable across different time signatures."""
        canonical_slots_per_bar = float(self.beats_per_bar) * float(steps_per_beat(CANONICAL_GRID))
        if canonical_slots_per_bar == 0:
            return 0.0
        return self.note_count / canonical_slots_per_bar


def _fingerprint_to_voices(fingerprint: frozenset[CanonicalSlot]) -> dict[str, list[float]]:
    voices: dict[str, list[float]] = defaultdict(list)
    for voice, position in fingerprint:
        voices[voice].append(float(position))
    for positions in voices.values():
        positions.sort()
    return dict(voices)


def distill_patterns(bars: list[SourceBar]) -> list[DistilledPattern]:
    patterns_by_fingerprint: dict[frozenset[CanonicalSlot], DistilledPattern] = {}
    # Preserves first-occurrence order for deterministic tie-breaking below,
    # since a plain dict keyed by an unordered frozenset would otherwise
    # make "first occurrence" ambiguous across a Python version/hash-seed
    # boundary if iteration order were ever relied on instead.
    first_seen_order: list[frozenset[CanonicalSlot]] = []

    for bar in bars:
        fp = canonical_fingerprint(bar.notes, bar.beats_per_bar)
        pattern = patterns_by_fingerprint.get(fp)
        if pattern is None:
            pattern = DistilledPattern(
                pattern_id="",  # assigned after sorting, below
                fingerprint=fp,
                voices=_fingerprint_to_voices(fp),
                beats_per_bar=float(bar.beats_per_bar),
            )
            patterns_by_fingerprint[fp] = pattern
            first_seen_order.append(fp)

        pattern.occurrences += 1
        pattern.occurrence_refs.append((bar.source_file, bar.bar_index))
        for slot, profile in velocity_profile(bar.notes, bar.beats_per_bar).items():
            merged = pattern.slot_velocity.setdefault(slot, SlotVelocityProfile())
            existing_total_velocity = merged.avg_velocity * merged.hit_count
            for label, count in profile.tier_counts.items():
                merged.tier_counts[label] += count
            new_hit_count = merged.hit_count
            merged.avg_velocity = (
                (existing_total_velocity + profile.avg_velocity * profile.hit_count) / new_hit_count
                if new_hit_count
                else 0.0
            )

    first_seen_index = {fp: i for i, fp in enumerate(first_seen_order)}
    ordered = sorted(
        patterns_by_fingerprint.values(),
        key=lambda p: (-p.occurrences, first_seen_index[p.fingerprint]),
    )
    for i, pattern in enumerate(ordered):
        pattern.pattern_id = f"pat_{i + 1:04d}"

    return ordered


def bar_pattern_assignment(bars: list[SourceBar], patterns: list[DistilledPattern]) -> dict[str, dict[int, str]]:
    """source_file -> bar_index -> pattern_id, the join key transitions.py
    needs to walk a file's own bar sequence pattern-by-pattern."""
    id_by_fingerprint = {p.fingerprint: p.pattern_id for p in patterns}
    assignment: dict[str, dict[int, str]] = defaultdict(dict)
    for bar in bars:
        fp = canonical_fingerprint(bar.notes, bar.beats_per_bar)
        assignment[bar.source_file][bar.bar_index] = id_by_fingerprint[fp]
    return dict(assignment)
