from fractions import Fraction

import pytest

from lineage_midi_analysis.quantize import bar_index, beat_within_bar, quantize_beat, steps_per_beat


def test_steps_per_beat_16th_grid():
    assert steps_per_beat(16) == Fraction(4)


def test_steps_per_beat_32nd_grid():
    assert steps_per_beat(32) == Fraction(8)


def test_steps_per_beat_rejects_non_multiple_of_four():
    with pytest.raises(ValueError):
        steps_per_beat(15)


def test_quantize_beat_snaps_to_nearest_16th():
    # 0.23 beats should snap to the nearest 1/4-beat (16th note) slot: 0.25.
    assert quantize_beat(Fraction(23, 100), 16) == Fraction(1, 4)


def test_quantize_beat_exact_on_grid_is_unchanged():
    assert quantize_beat(Fraction(3, 4), 16) == Fraction(3, 4)


def test_quantize_beat_32nd_grid_resolution():
    # 1/8 beat is exactly on a 32nd-note grid line.
    assert quantize_beat(Fraction(1, 8) + Fraction(1, 100), 32) == Fraction(1, 8)


def test_bar_index_and_beat_within_bar():
    beats_per_bar = Fraction(4)
    assert bar_index(Fraction(0), beats_per_bar) == 0
    assert bar_index(Fraction(3, 1), beats_per_bar) == 0
    assert bar_index(Fraction(4), beats_per_bar) == 1
    assert bar_index(Fraction(9, 2), beats_per_bar) == 1
    assert beat_within_bar(Fraction(9, 2), beats_per_bar) == Fraction(1, 2)
