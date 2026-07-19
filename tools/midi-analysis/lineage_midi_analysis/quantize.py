"""Grid quantization. Uses exact Fraction arithmetic throughout — pattern
clustering (patterns.py) hashes quantized positions, and float rounding
error there would silently split one real pattern into multiple "distinct"
fingerprints.
"""

from __future__ import annotations

from fractions import Fraction


def steps_per_beat(grid: int) -> Fraction:
    """`grid` is the note value defining the quantization resolution — 16
    for 16th notes, 32 for 32nd notes. A quarter-note beat contains
    grid/4 of them."""
    if grid % 4 != 0:
        raise ValueError(f"grid must be a multiple of 4 (got {grid}); e.g. 16 or 32")
    return Fraction(grid, 4)


def quantize_beat(beat: Fraction, grid: int) -> Fraction:
    step = steps_per_beat(grid)
    return round(beat * step) / step


def quantize_distance(beat: Fraction, grid: int) -> Fraction:
    """Absolute distance from `beat` to its nearest grid line, in beats."""
    return abs(beat - quantize_beat(beat, grid))


def bar_index(beat: Fraction, beats_per_bar: Fraction) -> int:
    return int(beat // beats_per_bar)


def beat_within_bar(beat: Fraction, beats_per_bar: Fraction) -> Fraction:
    return beat - bar_index(beat, beats_per_bar) * beats_per_bar
