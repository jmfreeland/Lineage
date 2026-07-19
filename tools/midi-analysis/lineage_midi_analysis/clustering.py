"""Hierarchical clustering of distilled patterns (library.py) into
groove/fill/outlier groups, plus the scalability cap that keeps the O(n^2)
distance/cluster step tractable on a large corpus.

Clustering only groups patterns by structural/velocity similarity — it has
no notion of "fill" on its own. Cluster labels are assigned afterward by a
deterministic two-signal heuristic (how often the cluster's patterns occur,
and how dense they are relative to the corpus's groove baseline), the same
class of hardcoded, explicitly-unvalidated heuristic as diff.py's
DENSITY_RATIO — a starting point, not a claim these thresholds are
perceptually "correct."
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
from scipy.cluster.hierarchy import fcluster, linkage
from scipy.spatial.distance import squareform

from .library import DistilledPattern

DEFAULT_CLUSTER_THRESHOLD = 0.4
DEFAULT_MAX_PATTERNS_FOR_CLUSTERING = 500

# A cluster whose patterns collectively occur in fewer than this share of
# all bars is too rare to characterize as a groove OR a fill with any
# confidence — probably a one-off/noise bar.
MIN_OCCURRENCE_SHARE_FOR_OUTLIER = 0.005
# A cluster whose average density is this many times the corpus's dominant
# (most-frequent) cluster's density reads as a fill (busier than the
# baseline groove) rather than a groove variant. Deliberately anchored to
# the single most-frequent cluster rather than a median/average across
# several "common enough" clusters — a genuinely frequent fill would
# otherwise pollute its own baseline and dodge detection.
FILL_DENSITY_RATIO = 1.5


def select_patterns_for_clustering(
    patterns: list[DistilledPattern], max_patterns: int = DEFAULT_MAX_PATTERNS_FOR_CLUSTERING
) -> list[DistilledPattern]:
    """Top-N most frequent patterns, deterministic tie-break by pattern_id.
    `patterns` is already sorted by descending occurrence (library.py's
    distill_patterns() ordering) but re-sorts explicitly here rather than
    relying on that invariant holding at the call site."""
    return sorted(patterns, key=lambda p: (-p.occurrences, p.pattern_id))[:max_patterns]


def linkage_matrix(distance_matrix: np.ndarray, method: str = "average") -> np.ndarray:
    n = distance_matrix.shape[0]
    if n < 2:
        return np.zeros((0, 4))
    condensed = squareform(distance_matrix, checks=False)
    return linkage(condensed, method=method)


@dataclass
class Cluster:
    cluster_id: int
    pattern_ids: list[str]
    label: str  # "groove" | "fill" | "outlier"
    avg_density: float
    total_occurrences: int


def cluster_patterns(
    patterns: list[DistilledPattern],
    distance_matrix: np.ndarray,
    total_bars: int,
    threshold: float = DEFAULT_CLUSTER_THRESHOLD,
) -> list[Cluster]:
    if not patterns:
        return []

    if len(patterns) == 1:
        labels = [1]
    else:
        z = linkage_matrix(distance_matrix)
        labels = fcluster(z, t=threshold, criterion="distance")

    members_by_label: dict[int, list[DistilledPattern]] = defaultdict(list)
    for pattern, label in zip(patterns, labels):
        members_by_label[int(label)].append(pattern)

    raw_groups = []
    for label in sorted(members_by_label):
        members = members_by_label[label]
        total_occurrences = sum(p.occurrences for p in members)
        avg_density = sum(p.density for p in members) / len(members)
        occurrence_share = total_occurrences / total_bars if total_bars else 0.0
        raw_groups.append(
            {
                "pattern_ids": [p.pattern_id for p in members],
                "avg_density": avg_density,
                "total_occurrences": total_occurrences,
                "occurrence_share": occurrence_share,
            }
        )

    groove_baseline_density = max(raw_groups, key=lambda g: g["total_occurrences"])["avg_density"]

    clusters = []
    for cluster_id, group in enumerate(raw_groups):
        if group["occurrence_share"] < MIN_OCCURRENCE_SHARE_FOR_OUTLIER:
            group_label = "outlier"
        elif (
            groove_baseline_density > 0
            and group["avg_density"] >= FILL_DENSITY_RATIO * groove_baseline_density
        ):
            group_label = "fill"
        else:
            group_label = "groove"
        clusters.append(
            Cluster(
                cluster_id=cluster_id,
                pattern_ids=group["pattern_ids"],
                label=group_label,
                avg_density=group["avg_density"],
                total_occurrences=group["total_occurrences"],
            )
        )
    return clusters
