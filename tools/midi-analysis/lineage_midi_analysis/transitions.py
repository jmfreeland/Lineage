"""Bar-to-bar pattern transition matrices, per lag, across a source file's
own contiguous bar sequence.

Unlike distance.py/clustering.py, this is O(bars) not O(patterns^2) — every
pattern (not just the top-N selected for clustering, see library.py's
distill_patterns()/select_patterns_for_clustering()) participates, and
there's no scalability cap to worry about.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass, field

DEFAULT_MAX_LAG = 8


@dataclass
class TransitionMatrix:
    lag: int
    counts: dict[str, dict[str, int]] = field(default_factory=lambda: defaultdict(lambda: defaultdict(int)))


def _is_contiguous_run(bars_by_index: dict[int, str], start: int, end: int) -> bool:
    return all(i in bars_by_index for i in range(start, end + 1))


def build_transition_matrices(
    bar_pattern_by_file: dict[str, dict[int, str]], max_lag: int = DEFAULT_MAX_LAG
) -> dict[int, TransitionMatrix]:
    """For each lag L in 1..max_lag: source_pattern -> target_pattern ->
    count, counting a transition only when EVERY bar in [idx, idx+L] is
    present (non-empty) for that source file — not just the two endpoints.
    A gap anywhere in between (an empty/silent bar) breaks contiguity, so a
    transition is never counted across a skipped bar."""
    matrices = {lag: TransitionMatrix(lag=lag) for lag in range(1, max_lag + 1)}

    for bars_by_index in bar_pattern_by_file.values():
        for lag in range(1, max_lag + 1):
            matrix = matrices[lag]
            for idx in sorted(bars_by_index):
                target_idx = idx + lag
                if target_idx not in bars_by_index:
                    continue
                if not _is_contiguous_run(bars_by_index, idx, target_idx):
                    continue
                matrix.counts[bars_by_index[idx]][bars_by_index[target_idx]] += 1

    return matrices


def transition_probabilities(matrix: TransitionMatrix) -> dict[str, dict[str, float]]:
    probabilities: dict[str, dict[str, float]] = {}
    for source_id, targets in matrix.counts.items():
        total = sum(targets.values())
        if total == 0:
            continue
        probabilities[source_id] = {target_id: count / total for target_id, count in targets.items()}
    return probabilities
