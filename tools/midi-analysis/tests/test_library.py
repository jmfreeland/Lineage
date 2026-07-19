from fractions import Fraction

from lineage_midi_analysis.library import SourceBar, bar_pattern_assignment, distill_patterns
from lineage_midi_analysis.parser import NoteEvent

BEATS_PER_BAR = Fraction(4)


def note(voice: str, start_beat: Fraction, velocity: int = 100) -> NoteEvent:
    return NoteEvent(
        pitch=36,
        gm_name=voice,
        voice=voice,
        channel=9,
        velocity=velocity,
        start_tick=0,
        end_tick=0,
        start_beat=start_beat,
        duration_beats=Fraction(1, 4),
    )


def rock_bar() -> list[NoteEvent]:
    return [note("kick", Fraction(0)), note("kick", Fraction(2)), note("snare", Fraction(1)), note("snare", Fraction(3))]


def fill_bar() -> list[NoteEvent]:
    return [note("tom", Fraction(0)), note("tom", Fraction(1, 2)), note("tom", Fraction(1)), note("crash", Fraction(0))]


def test_distill_patterns_merges_identical_bars_across_files():
    bars = [
        SourceBar("a.mid", 0, BEATS_PER_BAR, rock_bar()),
        SourceBar("a.mid", 1, BEATS_PER_BAR, rock_bar()),
        SourceBar("b.mid", 0, BEATS_PER_BAR, rock_bar()),
    ]
    patterns = distill_patterns(bars)

    assert len(patterns) == 1
    assert patterns[0].occurrences == 3
    assert set(patterns[0].occurrence_refs) == {("a.mid", 0), ("a.mid", 1), ("b.mid", 0)}
    assert patterns[0].voices["kick"] == [0.0, 2.0]
    assert patterns[0].voices["snare"] == [1.0, 3.0]


def test_distill_patterns_distinct_shapes_form_separate_patterns():
    bars = [
        SourceBar("a.mid", 0, BEATS_PER_BAR, rock_bar()),
        SourceBar("a.mid", 1, BEATS_PER_BAR, rock_bar()),
        SourceBar("a.mid", 2, BEATS_PER_BAR, fill_bar()),
    ]
    patterns = distill_patterns(bars)

    assert len(patterns) == 2
    # Ordered by descending occurrence: the rock bar (2x) before the
    # singleton fill (1x).
    assert patterns[0].occurrences == 2
    assert patterns[1].occurrences == 1


def test_distill_patterns_ids_are_sequential_and_deterministic():
    bars = [SourceBar("a.mid", 0, BEATS_PER_BAR, rock_bar()), SourceBar("a.mid", 1, BEATS_PER_BAR, fill_bar())]
    patterns = distill_patterns(bars)
    assert {p.pattern_id for p in patterns} == {"pat_0001", "pat_0002"}


def test_distill_patterns_note_count_and_density():
    bars = [SourceBar("a.mid", 0, BEATS_PER_BAR, rock_bar())]
    pattern = distill_patterns(bars)[0]
    assert pattern.note_count == 4
    # 4 beats/bar * 8 canonical (32nd-grid) steps/beat = 32 canonical slots.
    assert pattern.density == 4 / 32


def test_distill_patterns_merges_velocity_profile_across_occurrences():
    quiet_bar = [note("kick", Fraction(0), velocity=20)]
    loud_bar = [note("kick", Fraction(0), velocity=120)]
    bars = [SourceBar("a.mid", 0, BEATS_PER_BAR, quiet_bar), SourceBar("a.mid", 1, BEATS_PER_BAR, loud_bar)]
    pattern = distill_patterns(bars)[0]

    slot = ("kick", Fraction(0))
    assert pattern.slot_velocity[slot].tier_counts["ghost"] == 1
    assert pattern.slot_velocity[slot].tier_counts["accent"] == 1
    assert pattern.slot_velocity[slot].avg_velocity == (20 + 120) / 2


def test_bar_pattern_assignment_maps_every_bar_to_its_pattern_id():
    bars = [SourceBar("a.mid", 0, BEATS_PER_BAR, rock_bar()), SourceBar("a.mid", 1, BEATS_PER_BAR, fill_bar())]
    patterns = distill_patterns(bars)
    assignment = bar_pattern_assignment(bars, patterns)

    rock_id = next(p.pattern_id for p in patterns if p.occurrences == 1 and p.voices.get("kick"))
    fill_id = next(p.pattern_id for p in patterns if "tom" in p.voices)
    assert assignment["a.mid"][0] == rock_id
    assert assignment["a.mid"][1] == fill_id


def test_distill_patterns_empty_input():
    assert distill_patterns([]) == []
