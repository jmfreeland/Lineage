from fractions import Fraction

from lineage_midi_analysis.fingerprint import (
    canonical_fingerprint,
    canonical_slot,
    velocity_profile,
    velocity_tier,
)
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


def test_velocity_tier_boundaries():
    assert velocity_tier(0) == "ghost"
    assert velocity_tier(39) == "ghost"
    assert velocity_tier(40) == "soft"
    assert velocity_tier(74) == "soft"
    assert velocity_tier(75) == "medium"
    assert velocity_tier(104) == "medium"
    assert velocity_tier(105) == "accent"
    assert velocity_tier(127) == "accent"


def test_canonical_fingerprint_ignores_which_grid_a_file_was_detected_at():
    # canonical_fingerprint() always quantizes raw beat positions straight
    # to CANONICAL_GRID (32) — it never consults a file's own detected
    # grid (grid.py) at all, unlike stats.py's per-instrument quantize-
    # distance measurements, which deliberately DO use the detected grid.
    # Two notes near the same intended 16th-note hit, jittered by
    # different (independently plausible) amounts — as if drawn from a
    # file that would auto-detect as grid=16 and one that would
    # auto-detect as grid=32 — still land on the identical canonical slot,
    # which is what makes patterns from either kind of file comparable.
    bar_a = [note("kick", Fraction(0)), note("snare", Fraction(1, 4) + Fraction(1, 100))]
    bar_b = [note("kick", Fraction(0)), note("snare", Fraction(1, 4) - Fraction(1, 250))]

    fp_a = canonical_fingerprint(bar_a, BEATS_PER_BAR)
    fp_b = canonical_fingerprint(bar_b, BEATS_PER_BAR)
    assert fp_a == fp_b
    assert ("kick", Fraction(0)) in fp_a
    assert ("snare", Fraction(1, 4)) in fp_a


def test_canonical_fingerprint_slightly_off_grid_note_snaps_to_same_canonical_slot():
    # Real timing jitter around an intended 16th-note hit should still
    # canonicalize to that 16th-note's 32nd-grid position, not spuriously
    # land on a neighboring 32nd-grid line.
    exact = [note("kick", Fraction(1, 4))]
    jittered = [note("kick", Fraction(1, 4) + Fraction(1, 100))]
    assert canonical_fingerprint(exact, BEATS_PER_BAR) == canonical_fingerprint(jittered, BEATS_PER_BAR)


def test_canonical_slot_wraps_to_bar_local_position():
    slot = canonical_slot(note("kick", Fraction(9, 2)), BEATS_PER_BAR)  # bar 1, beat 0.5 locally
    assert slot == ("kick", Fraction(1, 2))


def test_velocity_profile_aggregates_tier_counts_per_slot():
    bar_notes = [
        note("kick", Fraction(0), velocity=20),   # ghost
        note("kick", Fraction(0), velocity=110),  # accent (a second, different-velocity kick at the same slot)
        note("snare", Fraction(1), velocity=80),  # medium
    ]
    profile = velocity_profile(bar_notes, BEATS_PER_BAR)

    kick_slot = ("kick", Fraction(0))
    assert profile[kick_slot].tier_counts["ghost"] == 1
    assert profile[kick_slot].tier_counts["accent"] == 1
    assert profile[kick_slot].hit_count == 2
    assert profile[kick_slot].avg_velocity == (20 + 110) / 2

    snare_slot = ("snare", Fraction(1))
    assert profile[snare_slot].tier_counts["medium"] == 1
    assert profile[snare_slot].avg_velocity == 80
