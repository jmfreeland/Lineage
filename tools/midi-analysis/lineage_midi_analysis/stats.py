"""Per-bar-slice summary statistics, plus corpus-level quantize-distance
distribution and cross-instrument correlation.

Distinct from grid.py: stats here are always measured against a bar's own
(already-detected) grid — "how far off was this note from the resolution
the performer meant to hit" only means something once that resolution is
known. Distinct from fingerprint.py's canonical-grid identity, which exists
for a completely different purpose (cross-file pattern comparability, not
per-instrument sloppiness measurement) — see fingerprint.py's docstring.
"""

from __future__ import annotations

import math
from collections import defaultdict
from dataclasses import dataclass, field
from fractions import Fraction

from .parser import NoteEvent, ParsedMidi
from .patterns import quantized_slot
from .quantize import quantize_distance


@dataclass
class BarSliceStats:
    source_file: str
    bar_index: int
    grid: int
    total_notes: int
    notes_per_step: dict[Fraction, int] = field(default_factory=dict)
    avg_velocity: float = 0.0
    avg_velocity_per_step: dict[Fraction, float] = field(default_factory=dict)
    avg_velocity_per_instrument: dict[str, float] = field(default_factory=dict)
    min_velocity_per_instrument: dict[str, int] = field(default_factory=dict)
    max_velocity_per_instrument: dict[str, int] = field(default_factory=dict)
    # ms, not beats — matches diff.py's convention of reporting timing in
    # human-readable ms rather than raw Fraction beats.
    avg_quantize_distance_per_instrument_ms: dict[str, float] = field(default_factory=dict)
    # Per-note average (each note weighted equally, not per-instrument
    # averaged-of-averages) — distinct from the per-instrument dict above,
    # which would over-weight an instrument with few notes relative to one
    # with many if simply averaged together.
    avg_quantize_distance_ms: float = 0.0


def compute_bar_slice_stats(
    bar_notes: list[NoteEvent],
    source_file: str,
    bar_index: int,
    beats_per_bar: Fraction,
    grid: int,
    parsed: ParsedMidi,
) -> BarSliceStats:
    if not bar_notes:
        return BarSliceStats(source_file=source_file, bar_index=bar_index, grid=grid, total_notes=0)

    notes_per_step: dict[Fraction, int] = defaultdict(int)
    velocity_sum_per_step: dict[Fraction, int] = defaultdict(int)
    velocities_per_instrument: dict[str, list[int]] = defaultdict(list)
    distances_per_instrument_ms: dict[str, list[float]] = defaultdict(list)
    all_distances_ms: list[float] = []
    all_velocities: list[int] = []

    for note in bar_notes:
        _, position = quantized_slot(note, beats_per_bar, grid)
        notes_per_step[position] += 1
        velocity_sum_per_step[position] += note.velocity
        velocities_per_instrument[note.voice].append(note.velocity)
        all_velocities.append(note.velocity)

        distance_ms = parsed.beats_to_ms(quantize_distance(note.start_beat, grid))
        distances_per_instrument_ms[note.voice].append(distance_ms)
        all_distances_ms.append(distance_ms)

    avg_velocity_per_step = {
        step: velocity_sum_per_step[step] / notes_per_step[step] for step in notes_per_step
    }
    avg_velocity_per_instrument = {
        voice: sum(vels) / len(vels) for voice, vels in velocities_per_instrument.items()
    }
    min_velocity_per_instrument = {voice: min(vels) for voice, vels in velocities_per_instrument.items()}
    max_velocity_per_instrument = {voice: max(vels) for voice, vels in velocities_per_instrument.items()}
    avg_quantize_distance_per_instrument_ms = {
        voice: sum(dists) / len(dists) for voice, dists in distances_per_instrument_ms.items()
    }

    return BarSliceStats(
        source_file=source_file,
        bar_index=bar_index,
        grid=grid,
        total_notes=len(bar_notes),
        notes_per_step=dict(notes_per_step),
        avg_velocity=sum(all_velocities) / len(all_velocities),
        avg_velocity_per_step=avg_velocity_per_step,
        avg_velocity_per_instrument=avg_velocity_per_instrument,
        min_velocity_per_instrument=min_velocity_per_instrument,
        max_velocity_per_instrument=max_velocity_per_instrument,
        avg_quantize_distance_per_instrument_ms=avg_quantize_distance_per_instrument_ms,
        avg_quantize_distance_ms=sum(all_distances_ms) / len(all_distances_ms),
    )


def quantize_distance_distribution(distances_by_voice: dict[str, list[float]], bin_count: int = 20) -> dict:
    """A simple equal-width histogram per voice, plus a few order statistics
    — enough for a GUI to render a distribution without shipping every raw
    sample. Sample lists are assumed already-sorted-agnostic (sorted here).
    """
    result: dict[str, dict] = {}
    for voice, distances in distances_by_voice.items():
        if not distances:
            continue
        sorted_distances = sorted(distances)
        n = len(sorted_distances)
        lo, hi = sorted_distances[0], sorted_distances[-1]
        if hi == lo:
            bin_edges = [lo, lo + 1.0]
            counts = [n]
        else:
            bin_width = (hi - lo) / bin_count
            bin_edges = [lo + i * bin_width for i in range(bin_count + 1)]
            counts = [0] * bin_count
            for value in sorted_distances:
                index = min(bin_count - 1, int((value - lo) / bin_width))
                counts[index] += 1
        result[voice] = {
            "n": n,
            "mean": sum(sorted_distances) / n,
            "median": _percentile(sorted_distances, 0.5),
            "p10": _percentile(sorted_distances, 0.1),
            "p90": _percentile(sorted_distances, 0.9),
            "bin_edges": bin_edges,
            "counts": counts,
        }
    return result


def _percentile(sorted_values: list[float], fraction: float) -> float:
    if len(sorted_values) == 1:
        return sorted_values[0]
    index = fraction * (len(sorted_values) - 1)
    lower = math.floor(index)
    upper = math.ceil(index)
    if lower == upper:
        return sorted_values[int(index)]
    weight = index - lower
    return sorted_values[lower] * (1 - weight) + sorted_values[upper] * weight


def instrument_quantize_correlation(bar_slices: list[BarSliceStats], voices: list[str]) -> dict:
    """Pairwise Pearson correlation between instruments' per-bar average
    quantize distance — "when this instrument is sloppy in a bar, is that
    other instrument also sloppy in the same bar." Uses pairwise-complete
    observations (only bars where BOTH voices appear in that pair), since
    not every voice appears in every bar; a pair with fewer than 2
    overlapping bars can't have a defined correlation and reports null
    rather than raising or fabricating a value.
    """
    per_voice_by_bar: dict[str, dict[tuple[str, int], float]] = {voice: {} for voice in voices}
    for slice_stats in bar_slices:
        key = (slice_stats.source_file, slice_stats.bar_index)
        for voice, distance in slice_stats.avg_quantize_distance_per_instrument_ms.items():
            if voice in per_voice_by_bar:
                per_voice_by_bar[voice][key] = distance

    matrix: list[list[float | None]] = []
    for row_voice in voices:
        row: list[float | None] = []
        for col_voice in voices:
            row.append(_pearson_correlation(per_voice_by_bar[row_voice], per_voice_by_bar[col_voice]))
        matrix.append(row)

    return {"voices": list(voices), "matrix": matrix}


def _pearson_correlation(a: dict[tuple[str, int], float], b: dict[tuple[str, int], float]) -> float | None:
    shared_keys = a.keys() & b.keys()
    if len(shared_keys) < 2:
        return None
    xs = [a[k] for k in shared_keys]
    ys = [b[k] for k in shared_keys]
    n = len(xs)
    mean_x = sum(xs) / n
    mean_y = sum(ys) / n
    cov = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
    var_x = sum((x - mean_x) ** 2 for x in xs)
    var_y = sum((y - mean_y) ** 2 for y in ys)
    if var_x == 0 or var_y == 0:
        return None
    return cov / math.sqrt(var_x * var_y)
