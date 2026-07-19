from lineage_midi_analysis.transitions import build_transition_matrices, transition_probabilities


def test_build_transition_matrices_lag_1_hand_counted():
    # Sequence A, A, A, B across bars 0-3.
    bar_pattern_by_file = {"a.mid": {0: "A", 1: "A", 2: "A", 3: "B"}}
    matrices = build_transition_matrices(bar_pattern_by_file, max_lag=1)

    counts = matrices[1].counts
    assert counts["A"]["A"] == 2  # 0->1, 1->2
    assert counts["A"]["B"] == 1  # 2->3
    assert "B" not in counts or not counts["B"]  # B is never a source (no bar 4)


def test_build_transition_matrices_skips_across_a_gap():
    # Bar 2 is missing (silent/empty) — the lag-3 transition from bar 0 to
    # bar 3 must NOT be counted, since it would silently skip over bar 2.
    # Checking only the two endpoints (0 and 3, both present) would wrongly
    # count this; the contiguity check must span the whole [0,3] range.
    bar_pattern_by_file = {"a.mid": {0: "A", 1: "A", 3: "B"}}
    matrices = build_transition_matrices(bar_pattern_by_file, max_lag=3)

    assert matrices[3].counts.get("A", {}).get("B", 0) == 0
    # Lag-1 from bar 0->1 (both present, contiguous) still counts.
    assert matrices[1].counts["A"]["A"] == 1
    # Lag-2 from bar 1->3 requires bar 2 present too -> not counted.
    assert matrices[2].counts.get("A", {}).get("B", 0) == 0


def test_build_transition_matrices_respects_max_lag():
    bar_pattern_by_file = {"a.mid": {0: "A", 1: "A", 2: "A"}}
    matrices = build_transition_matrices(bar_pattern_by_file, max_lag=2)
    assert set(matrices.keys()) == {1, 2}


def test_build_transition_matrices_aggregates_across_files():
    bar_pattern_by_file = {
        "a.mid": {0: "A", 1: "B"},
        "b.mid": {0: "A", 1: "B"},
    }
    matrices = build_transition_matrices(bar_pattern_by_file, max_lag=1)
    assert matrices[1].counts["A"]["B"] == 2


def test_build_transition_matrices_empty_input():
    matrices = build_transition_matrices({}, max_lag=8)
    assert len(matrices) == 8
    assert all(len(m.counts) == 0 for m in matrices.values())


def test_transition_probabilities_rows_sum_to_one():
    bar_pattern_by_file = {"a.mid": {0: "A", 1: "A", 2: "A", 3: "B", 4: "A", 5: "B"}}
    matrices = build_transition_matrices(bar_pattern_by_file, max_lag=1)
    probs = transition_probabilities(matrices[1])

    for source_id, targets in probs.items():
        assert abs(sum(targets.values()) - 1.0) < 1e-9

    # A -> A twice (0->1, 1->2), A -> B twice (2->3, 4->5): 50/50.
    assert abs(probs["A"]["A"] - 0.5) < 1e-9
    assert abs(probs["A"]["B"] - 0.5) < 1e-9


def test_transition_probabilities_empty_matrix():
    from lineage_midi_analysis.transitions import TransitionMatrix

    assert transition_probabilities(TransitionMatrix(lag=1)) == {}
