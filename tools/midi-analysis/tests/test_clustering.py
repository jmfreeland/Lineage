from fractions import Fraction

from lineage_midi_analysis.clustering import (
    DEFAULT_MAX_PATTERNS_FOR_CLUSTERING,
    cluster_patterns,
    linkage_matrix,
    select_patterns_for_clustering,
)
from lineage_midi_analysis.distance import build_distance_matrix
from lineage_midi_analysis.library import DistilledPattern

GROOVE_A = frozenset({("kick", Fraction(0)), ("snare", Fraction(1))})
GROOVE_B = frozenset({("kick", Fraction(0)), ("snare", Fraction(1)), ("closed_hihat", Fraction(1, 2))})
FILL_A = frozenset(
    {("tom", Fraction(0)), ("tom", Fraction(1, 4)), ("tom", Fraction(1, 2)), ("tom", Fraction(3, 4)), ("crash", Fraction(0))}
)
ONE_OFF = frozenset({("ride", Fraction(3, 8))})


def make_pattern(pattern_id: str, fingerprint: frozenset, occurrences: int) -> DistilledPattern:
    voices: dict[str, list[float]] = {}
    for voice, position in fingerprint:
        voices.setdefault(voice, []).append(float(position))
    return DistilledPattern(
        pattern_id=pattern_id,
        fingerprint=fingerprint,
        voices=voices,
        beats_per_bar=4.0,
        occurrences=occurrences,
    )


def test_select_patterns_for_clustering_keeps_top_n_by_occurrence():
    patterns = [make_pattern(f"pat_{i:04d}", frozenset({("kick", Fraction(i))}), occurrences=i) for i in range(1, 11)]
    selected = select_patterns_for_clustering(patterns, max_patterns=3)
    assert [p.occurrences for p in selected] == [10, 9, 8]


def test_select_patterns_for_clustering_deterministic_tie_break():
    patterns = [
        make_pattern("pat_0002", frozenset({("kick", Fraction(0))}), occurrences=5),
        make_pattern("pat_0001", frozenset({("snare", Fraction(0))}), occurrences=5),
    ]
    selected = select_patterns_for_clustering(patterns, max_patterns=1)
    assert selected[0].pattern_id == "pat_0001"


def test_linkage_matrix_shape():
    patterns = [
        make_pattern("pat_0001", GROOVE_A, 10),
        make_pattern("pat_0002", GROOVE_B, 8),
        make_pattern("pat_0003", FILL_A, 3),
    ]
    matrix = build_distance_matrix(patterns)
    z = linkage_matrix(matrix)
    assert z.shape == (2, 4)


def test_linkage_matrix_fewer_than_two_patterns_is_empty():
    assert linkage_matrix(build_distance_matrix([make_pattern("pat_0001", GROOVE_A, 1)])).shape == (0, 4)


def test_cluster_patterns_separates_groove_fill_and_outlier():
    patterns = [
        make_pattern("pat_0001", GROOVE_A, 400),
        make_pattern("pat_0002", GROOVE_B, 380),
        make_pattern("pat_0003", FILL_A, 30),
        make_pattern("pat_0004", ONE_OFF, 1),
    ]
    total_bars = sum(p.occurrences for p in patterns)
    matrix = build_distance_matrix(patterns)
    clusters = cluster_patterns(patterns, matrix, total_bars, threshold=0.5)

    labels_by_pattern = {pid: c.label for c in clusters for pid in c.pattern_ids}
    assert labels_by_pattern["pat_0001"] == "groove"
    assert labels_by_pattern["pat_0002"] == "groove"
    assert labels_by_pattern["pat_0003"] == "fill"
    assert labels_by_pattern["pat_0004"] == "outlier"


def test_cluster_patterns_empty_input():
    assert cluster_patterns([], build_distance_matrix([]), total_bars=0) == []


def test_cluster_patterns_single_pattern():
    patterns = [make_pattern("pat_0001", GROOVE_A, 10)]
    clusters = cluster_patterns(patterns, build_distance_matrix(patterns), total_bars=10)
    assert len(clusters) == 1
    assert clusters[0].pattern_ids == ["pat_0001"]


def test_cluster_patterns_caps_via_selection_helper():
    patterns = [
        make_pattern(f"pat_{i:04d}", frozenset({("kick", Fraction(i % 4))}), occurrences=1)
        for i in range(DEFAULT_MAX_PATTERNS_FOR_CLUSTERING + 50)
    ]
    selected = select_patterns_for_clustering(patterns)
    assert len(selected) == DEFAULT_MAX_PATTERNS_FOR_CLUSTERING
