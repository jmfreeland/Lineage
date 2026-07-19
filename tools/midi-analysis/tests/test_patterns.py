from fractions import Fraction

from lineage_midi_analysis.parser import NoteEvent
from lineage_midi_analysis.patterns import find_base_pattern, group_by_bar


def note(voice: str, start_beat: Fraction, velocity: int = 100, pitch: int = 36) -> NoteEvent:
    return NoteEvent(
        pitch=pitch,
        gm_name=voice,
        voice=voice,
        channel=9,
        velocity=velocity,
        start_tick=0,
        end_tick=0,
        start_beat=start_beat,
        duration_beats=Fraction(1, 4),
    )


def test_group_by_bar_splits_notes_correctly():
    beats_per_bar = Fraction(4)
    notes = [note("kick", Fraction(0)), note("kick", Fraction(4)), note("kick", Fraction(9, 2))]
    bars = group_by_bar(notes, beats_per_bar)
    assert set(bars.keys()) == {0, 1}
    assert len(bars[0]) == 1
    assert len(bars[1]) == 2


def test_find_base_pattern_picks_most_frequent_fingerprint():
    beats_per_bar = Fraction(4)
    bars_by_index = {
        0: [note("kick", Fraction(0)), note("snare", Fraction(1))],
        1: [note("kick", Fraction(0)), note("snare", Fraction(1))],
        2: [note("kick", Fraction(0)), note("snare", Fraction(1))],
        3: [note("kick", Fraction(0)), note("snare", Fraction(1)), note("crash", Fraction(0))],  # a fill/outlier
    }
    base = find_base_pattern(bars_by_index, beats_per_bar, grid=16)
    assert base.occurrences == 3
    assert base.bar_indices == [0, 1, 2]
    assert ("kick", Fraction(0)) in base.slots
    assert ("snare", Fraction(1)) in base.slots
    assert ("crash", Fraction(0)) not in base.slots


def test_find_base_pattern_computes_average_slot_velocity():
    beats_per_bar = Fraction(4)
    bars_by_index = {
        0: [note("kick", Fraction(0), velocity=90)],
        1: [note("kick", Fraction(0), velocity=110)],
    }
    base = find_base_pattern(bars_by_index, beats_per_bar, grid=16)
    assert base.slot_avg_velocity[("kick", Fraction(0))] == 100


def test_find_base_pattern_empty_input():
    base = find_base_pattern({}, Fraction(4), grid=16)
    assert base.occurrences == 0
    assert base.slots == frozenset()
