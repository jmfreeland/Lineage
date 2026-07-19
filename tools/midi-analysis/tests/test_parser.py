from fractions import Fraction

from fixtures import CLOSED_HAT, KICK, SNARE, write_midi

from lineage_midi_analysis.parser import parse_midi_file


def test_parses_notes_with_exact_beat_positions(tmp_path):
    path = write_midi(
        tmp_path / "simple.mid",
        events=[
            (Fraction(0), KICK, 100, Fraction(1, 4)),
            (Fraction(1), SNARE, 96, Fraction(1, 4)),
        ],
    )
    parsed = parse_midi_file(path)

    assert len(parsed.notes) == 2
    assert parsed.notes[0].pitch == KICK
    assert parsed.notes[0].start_beat == Fraction(0)
    assert parsed.notes[0].voice == "kick"
    assert parsed.notes[1].start_beat == Fraction(1)
    assert parsed.notes[1].voice == "snare"


def test_default_time_signature_is_four_four(tmp_path):
    path = write_midi(tmp_path / "no_sig.mid", events=[(Fraction(0), KICK, 100, Fraction(1, 4))])
    parsed = parse_midi_file(path)
    assert parsed.beats_per_bar == Fraction(4)


def test_reads_explicit_time_signature(tmp_path):
    path = write_midi(
        tmp_path / "three_four.mid",
        events=[(Fraction(0), KICK, 100, Fraction(1, 4))],
        numerator=3,
        denominator=4,
    )
    parsed = parse_midi_file(path)
    assert parsed.beats_per_bar == Fraction(3)


def test_tick_to_beat_conversion_is_tempo_independent(tmp_path):
    """A beat is tick/ticks_per_beat by MIDI's own definition — a tempo
    change partway through must not shift where later notes land on the
    beat grid, only how many real-world ms they correspond to."""
    path = write_midi(
        tmp_path / "tempo_change.mid",
        events=[
            (Fraction(0), KICK, 100, Fraction(1, 4)),
            (Fraction(4), SNARE, 100, Fraction(1, 4)),  # after the tempo change
        ],
        tempo_bpm=120,
        tempo_changes=[(Fraction(2), 200.0)],
    )
    parsed = parse_midi_file(path)
    assert parsed.notes[0].start_beat == Fraction(0)
    assert parsed.notes[1].start_beat == Fraction(4)


def test_tick_to_seconds_accounts_for_tempo_changes(tmp_path):
    path = write_midi(
        tmp_path / "tempo_change_seconds.mid",
        events=[(Fraction(0), KICK, 100, Fraction(1, 4))],
        tempo_bpm=120,
        tempo_changes=[(Fraction(2), 240.0)],  # doubles tempo at beat 2
    )
    parsed = parse_midi_file(path)
    # First 2 beats at 120bpm = 1s, next 2 beats at 240bpm = 0.5s -> beat 4 at 1.5s.
    seconds_at_beat_4 = parsed.tick_to_seconds(parsed.ticks_per_beat * 4)
    assert abs(seconds_at_beat_4 - 1.5) < 1e-6


def test_falls_back_to_all_channels_when_percussion_channel_empty(tmp_path):
    path = write_midi(
        tmp_path / "wrong_channel.mid",
        events=[(Fraction(0), KICK, 100, Fraction(1, 4))],
        channel=0,
    )
    parsed = parse_midi_file(path)
    assert len(parsed.notes) == 1
    assert parsed.notes[0].channel == 0


def test_unmapped_pitch_gets_other_voice(tmp_path):
    path = write_midi(tmp_path / "weird_pitch.mid", events=[(Fraction(0), 10, 100, Fraction(1, 4))])
    parsed = parse_midi_file(path)
    assert parsed.notes[0].voice == "other"
    assert parsed.notes[0].gm_name == "note_10"


def test_closed_hat_maps_correctly():
    from lineage_midi_analysis.drum_map import voice_for_pitch

    assert voice_for_pitch(CLOSED_HAT) == ("Closed Hi-Hat", "closed_hihat")


def test_parse_midi_file_applies_note_map_override(tmp_path):
    # Pitch 100 is outside GM's percussion range (would otherwise be
    # "other") — a drum library's extra articulation on that note should
    # resolve to whatever voice the override says, once one is supplied.
    path = write_midi(tmp_path / "custom_kit.mid", events=[(Fraction(0), 100, 100, Fraction(1, 4))])

    unmapped = parse_midi_file(path)
    assert unmapped.notes[0].voice == "other"

    mapped = parse_midi_file(path, note_map={100: "snare"})
    assert mapped.notes[0].voice == "snare"
