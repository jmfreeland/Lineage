import json

from lineage_midi_analysis.drum_map import load_note_map, voice_for_pitch


def test_voice_for_pitch_uses_gm_map_by_default():
    assert voice_for_pitch(36) == ("Bass Drum 1", "kick")


def test_voice_for_pitch_falls_back_to_other_for_unmapped_pitch():
    assert voice_for_pitch(10) == ("note_10", "other")


def test_voice_for_pitch_override_wins_over_gm_map():
    # Note 36 is normally "kick" — a drum library remapping it to something
    # else (e.g. a kit that puts kick elsewhere) must be respected.
    overrides = {36: "snare"}
    gm_name, voice = voice_for_pitch(36, overrides)
    assert voice == "snare"
    assert gm_name == "Bass Drum 1"  # GM name is still informative even when voice is overridden


def test_voice_for_pitch_override_covers_a_pitch_gm_has_no_opinion_on():
    # Superior Drummer-style extra articulation on a pitch GM doesn't map —
    # this is the actual gap the override mechanism exists to close.
    overrides = {100: "snare"}
    gm_name, voice = voice_for_pitch(100, overrides)
    assert voice == "snare"
    assert gm_name == "note_100"


def test_voice_for_pitch_override_dict_without_this_pitch_falls_back_to_gm():
    overrides = {100: "snare"}
    assert voice_for_pitch(36, overrides) == ("Bass Drum 1", "kick")


def test_load_note_map_parses_json_string_keys_into_int_keys(tmp_path):
    path = tmp_path / "map.json"
    path.write_text(json.dumps({"36": "kick", "100": "snare"}))

    note_map = load_note_map(path)
    assert note_map == {36: "kick", 100: "snare"}
    assert isinstance(next(iter(note_map)), int)
