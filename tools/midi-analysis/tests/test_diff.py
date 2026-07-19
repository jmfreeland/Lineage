from fractions import Fraction

from lineage_midi_analysis.diff import diff_bar
from lineage_midi_analysis.parser import NoteEvent, ParsedMidi
from lineage_midi_analysis.patterns import BasePattern

BEATS_PER_BAR = Fraction(4)
GRID = 16


def note(voice: str, start_beat: Fraction, velocity: int = 100, pitch: int = 36) -> NoteEvent:
    return NoteEvent(
        pitch=pitch,
        gm_name=voice,
        voice=voice,
        channel=9,
        velocity=velocity,
        start_tick=round(start_beat * 480),
        end_tick=round(start_beat * 480) + 100,
        start_beat=start_beat,
        duration_beats=Fraction(1, 8),
    )


def make_parsed() -> ParsedMidi:
    return ParsedMidi(
        source_path="test.mid",
        ticks_per_beat=480,
        beats_per_bar=BEATS_PER_BAR,
        notes=[],
        tempo_map=[(0, 500_000)],  # 120bpm
    )


def base_kick_snare() -> BasePattern:
    return BasePattern(
        slots=frozenset({("kick", Fraction(0)), ("snare", Fraction(1))}),
        slot_avg_velocity={("kick", Fraction(0)): 100.0, ("snare", Fraction(1)): 96.0},
        bar_indices=[0, 1, 2],
    )


def test_matched_slot_produces_no_diff_when_on_grid_and_on_velocity():
    bar_notes = [note("kick", Fraction(0), velocity=100), note("snare", Fraction(1), velocity=96)]
    result = diff_bar(3, bar_notes, base_kick_snare(), BEATS_PER_BAR, GRID, make_parsed())
    assert result.diffs == []
    assert not result.is_fill


def test_timing_shift_detected_when_offset_exceeds_threshold():
    # 120bpm -> 500ms/beat. A 1/32-beat offset ~ 15.6ms, above the 5ms threshold.
    bar_notes = [note("kick", Fraction(0) + Fraction(1, 32), velocity=100), note("snare", Fraction(1), velocity=96)]
    result = diff_bar(3, bar_notes, base_kick_snare(), BEATS_PER_BAR, GRID, make_parsed())
    timing_entries = [d for d in result.diffs if d.category == "timing_shift"]
    assert len(timing_entries) == 1
    assert timing_entries[0].voice == "kick"
    assert timing_entries[0].direction == "late"
    assert timing_entries[0].magnitude > 5.0


def test_velocity_variation_detected_when_delta_exceeds_threshold():
    bar_notes = [note("kick", Fraction(0), velocity=40), note("snare", Fraction(1), velocity=96)]
    result = diff_bar(3, bar_notes, base_kick_snare(), BEATS_PER_BAR, GRID, make_parsed())
    velocity_entries = [d for d in result.diffs if d.category == "velocity_variation"]
    assert len(velocity_entries) == 1
    assert velocity_entries[0].voice == "kick"
    assert velocity_entries[0].direction == "softer"


def test_omission_detected_when_base_slot_missing():
    bar_notes = [note("kick", Fraction(0), velocity=100)]  # snare dropped
    result = diff_bar(3, bar_notes, base_kick_snare(), BEATS_PER_BAR, GRID, make_parsed())
    omissions = [d for d in result.diffs if d.category == "omission"]
    assert len(omissions) == 1
    assert omissions[0].voice == "snare"


def test_embellishment_detected_for_extra_hit_at_new_position():
    bar_notes = [
        note("kick", Fraction(0), velocity=100),
        note("snare", Fraction(1), velocity=96),
        note("snare", Fraction(3, 2), velocity=40),  # ghost note, no base slot at 1.5
    ]
    result = diff_bar(3, bar_notes, base_kick_snare(), BEATS_PER_BAR, GRID, make_parsed())
    embellishments = [d for d in result.diffs if d.category == "embellishment"]
    assert len(embellishments) == 1
    assert embellishments[0].voice == "snare"
    assert embellishments[0].position_beat == Fraction(3, 2)


def test_substitution_detected_when_different_voice_at_base_position():
    bar_notes = [
        note("kick", Fraction(0), velocity=100),
        note("crash", Fraction(1), velocity=110),  # crash instead of snare, same slot
    ]
    result = diff_bar(3, bar_notes, base_kick_snare(), BEATS_PER_BAR, GRID, make_parsed())
    substitutions = [d for d in result.diffs if d.category == "substitution"]
    omissions = [d for d in result.diffs if d.category == "omission"]
    assert len(substitutions) == 1
    assert substitutions[0].voice == "crash"
    assert "snare" in substitutions[0].detail
    # The base pattern's snare slot is also unmatched, so it's a real omission too.
    assert len(omissions) == 1
    assert omissions[0].voice == "snare"


def test_density_change_detected_when_voice_hit_count_doubles():
    base = BasePattern(
        slots=frozenset({("closed_hihat", Fraction(0)), ("closed_hihat", Fraction(2))}),
        slot_avg_velocity={("closed_hihat", Fraction(0)): 70.0, ("closed_hihat", Fraction(2)): 70.0},
        bar_indices=[0, 1, 2],
    )
    bar_notes = [
        note("closed_hihat", Fraction(0), pitch=42),
        note("closed_hihat", Fraction(1, 2), pitch=42),
        note("closed_hihat", Fraction(1), pitch=42),
        note("closed_hihat", Fraction(3, 2), pitch=42),
        note("closed_hihat", Fraction(2), pitch=42),
    ]
    result = diff_bar(3, bar_notes, base, BEATS_PER_BAR, GRID, make_parsed())
    density_entries = [d for d in result.diffs if d.category == "density_change"]
    assert len(density_entries) == 1
    assert density_entries[0].direction == "increase"


def test_bar_flagged_as_fill_when_departure_score_exceeds_threshold():
    # Both base slots omitted and two new hits inserted -> high departure.
    bar_notes = [note("crash", Fraction(0), velocity=110), note("crash", Fraction(2), velocity=110)]
    result = diff_bar(3, bar_notes, base_kick_snare(), BEATS_PER_BAR, GRID, make_parsed(), fill_threshold=0.5)
    assert result.is_fill
    assert result.departure_score > 0.5
