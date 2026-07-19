from fractions import Fraction

from lineage_midi_analysis.distance import (
    build_distance_matrix,
    jaccard_distance,
    pattern_distance,
    velocity_profile_distance,
)
from lineage_midi_analysis.fingerprint import SlotVelocityProfile
from lineage_midi_analysis.library import DistilledPattern


def make_pattern(pattern_id: str, fingerprint: frozenset, slot_velocity: dict | None = None) -> DistilledPattern:
    return DistilledPattern(
        pattern_id=pattern_id,
        fingerprint=fingerprint,
        voices={},
        beats_per_bar=4.0,
        slot_velocity=slot_velocity or {},
    )


KICK_SNARE = frozenset({("kick", Fraction(0)), ("snare", Fraction(1))})
TOM_CRASH = frozenset({("tom", Fraction(0)), ("crash", Fraction(1))})


def test_jaccard_distance_identical_sets_is_zero():
    assert jaccard_distance(KICK_SNARE, KICK_SNARE) == 0.0


def test_jaccard_distance_disjoint_sets_is_one():
    assert jaccard_distance(KICK_SNARE, TOM_CRASH) == 1.0


def test_jaccard_distance_partial_overlap():
    a = frozenset({("kick", Fraction(0)), ("snare", Fraction(1))})
    b = frozenset({("kick", Fraction(0)), ("snare", Fraction(2))})
    # intersection={kick@0} size 1, union size 3 -> distance = 1 - 1/3
    assert abs(jaccard_distance(a, b) - (1 - 1 / 3)) < 1e-9


def test_jaccard_distance_both_empty_is_zero():
    assert jaccard_distance(frozenset(), frozenset()) == 0.0


def test_pattern_distance_identical_patterns_is_zero():
    p = make_pattern("pat_0001", KICK_SNARE)
    assert pattern_distance(p, p) == 0.0


def test_pattern_distance_isolates_structural_term_when_velocity_weight_zero():
    a = make_pattern("pat_0001", KICK_SNARE)
    b = make_pattern("pat_0002", TOM_CRASH)
    assert pattern_distance(a, b, velocity_weight=0.0) == 1.0


def test_pattern_distance_isolates_velocity_term():
    ghost_profile = {("kick", Fraction(0)): SlotVelocityProfile(tier_counts={"ghost": 10, "soft": 0, "medium": 0, "accent": 0}, avg_velocity=20)}
    accent_profile = {("kick", Fraction(0)): SlotVelocityProfile(tier_counts={"ghost": 0, "soft": 0, "medium": 0, "accent": 10}, avg_velocity=120)}
    same_shape = frozenset({("kick", Fraction(0))})

    quiet = make_pattern("pat_0001", same_shape, ghost_profile)
    loud = make_pattern("pat_0002", same_shape, accent_profile)

    assert pattern_distance(quiet, loud, velocity_weight=0.0) == 0.0
    assert pattern_distance(quiet, loud, velocity_weight=0.3) > 0.0


def test_velocity_profile_distance_identical_is_zero():
    profile = {("kick", Fraction(0)): SlotVelocityProfile(tier_counts={"ghost": 5, "soft": 0, "medium": 0, "accent": 0}, avg_velocity=20)}
    p = make_pattern("pat_0001", frozenset(), profile)
    assert velocity_profile_distance(p, p) == 0.0


def test_velocity_profile_distance_no_notes_is_zero():
    a = make_pattern("pat_0001", frozenset())
    b = make_pattern("pat_0002", frozenset())
    assert velocity_profile_distance(a, b) == 0.0


def test_build_distance_matrix_is_symmetric_with_zero_diagonal():
    patterns = [make_pattern("pat_0001", KICK_SNARE), make_pattern("pat_0002", TOM_CRASH), make_pattern("pat_0003", KICK_SNARE)]
    matrix = build_distance_matrix(patterns)

    assert matrix.shape == (3, 3)
    for i in range(3):
        assert matrix[i, i] == 0.0
    for i in range(3):
        for j in range(3):
            assert matrix[i, j] == matrix[j, i]
    assert matrix[0, 2] == 0.0  # pat_0001 and pat_0003 share the same fingerprint
