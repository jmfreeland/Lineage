from fractions import Fraction

from lineage_midi_analysis.grid import avg_quantize_distance, detect_grid
from lineage_midi_analysis.parser import NoteEvent


def note(start_beat: Fraction) -> NoteEvent:
    return NoteEvent(
        pitch=36,
        gm_name="kick",
        voice="kick",
        channel=9,
        velocity=100,
        start_tick=0,
        end_tick=0,
        start_beat=start_beat,
        duration_beats=Fraction(1, 4),
    )


def test_detect_grid_exact_8th_notes_stays_coarse():
    notes = [note(Fraction(0)), note(Fraction(1, 2)), note(Fraction(1)), note(Fraction(3, 2))]
    assert detect_grid(notes) == 8


def test_detect_grid_exact_16th_notes_does_not_overfit_to_32():
    # Notes that land exactly on 16th-note lines score 0 error at grid=16
    # AND at grid=32 (16 divides into 32) — the naive "pick lowest error"
    # rule would be indifferent and could drift to 32; the relative-
    # improvement rule must stay at 16 since going finer buys nothing.
    notes = [note(Fraction(0)), note(Fraction(1, 4)), note(Fraction(3, 4)), note(Fraction(5, 4))]
    assert detect_grid(notes) == 16


def test_detect_grid_needs_32nd_resolution():
    notes = [note(Fraction(0)), note(Fraction(1, 8)), note(Fraction(3, 8)), note(Fraction(5, 8))]
    assert detect_grid(notes) == 32


def test_detect_grid_small_jitter_around_16th_lines_still_resolves_to_16():
    jitter = Fraction(1, 200)
    notes = [note(Fraction(0) + jitter), note(Fraction(1, 4) - jitter), note(Fraction(3, 4) + jitter)]
    assert detect_grid(notes) == 16


def test_detect_grid_empty_notes_returns_middle_default():
    assert detect_grid([]) == 16


def test_avg_quantize_distance_empty_is_zero():
    assert avg_quantize_distance([], 16) == 0.0


def test_avg_quantize_distance_exact_on_grid_is_zero():
    notes = [note(Fraction(0)), note(Fraction(1, 4))]
    assert avg_quantize_distance(notes, 16) == 0.0
