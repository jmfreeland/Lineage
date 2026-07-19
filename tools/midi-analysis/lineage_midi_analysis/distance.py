"""Pairwise distance between distilled patterns (library.py), the input to
clustering.py.

distance = (1 - w) * jaccard_distance(structure) + w * velocity_profile_distance(feel)

Jaccard distance on the (voice, canonical-step) slot sets is a standard,
simple, bounded [0,1], symmetric similarity measure for sparse binary step
patterns — two patterns that share every slot are 0 apart, two with no
slots in common are maximally 1 apart. Velocity is treated as a secondary
"feel" signal layered on top of structural shape, not the primary
determinant of "is this the same groove" — hence the small default weight.
Like diff.py's thresholds, DEFAULT_VELOCITY_WEIGHT is a simple hardcoded
starting point, not tuned against real corpus data.
"""

from __future__ import annotations

import numpy as np

from .fingerprint import VELOCITY_TIERS
from .library import DistilledPattern

DEFAULT_VELOCITY_WEIGHT = 0.3

_TIER_ORDER = tuple(label for label, _, _ in VELOCITY_TIERS)


def jaccard_distance(a: frozenset, b: frozenset) -> float:
    if not a and not b:
        return 0.0
    union = a | b
    if not union:
        return 0.0
    return 1.0 - len(a & b) / len(union)


def _pooled_velocity_tier_distribution(pattern: DistilledPattern) -> np.ndarray:
    """The pattern's total hit count per velocity tier, pooled across every
    slot and normalized to sum to 1 — a coarse "how hard does this pattern
    hit overall" fingerprint, independent of exactly which slot each hit
    landed on (slot-level detail is what `structure`/Jaccard already
    covers)."""
    counts = np.zeros(len(_TIER_ORDER))
    for profile in pattern.slot_velocity.values():
        for i, tier in enumerate(_TIER_ORDER):
            counts[i] += profile.tier_counts.get(tier, 0)
    total = counts.sum()
    if total == 0:
        return counts
    return counts / total


def velocity_profile_distance(a: DistilledPattern, b: DistilledPattern) -> float:
    """Normalized L1 (total variation) distance between two patterns'
    pooled velocity-tier distributions, in [0, 1]."""
    dist_a = _pooled_velocity_tier_distribution(a)
    dist_b = _pooled_velocity_tier_distribution(b)
    return float(np.abs(dist_a - dist_b).sum() / 2.0)


def pattern_distance(
    a: DistilledPattern, b: DistilledPattern, velocity_weight: float = DEFAULT_VELOCITY_WEIGHT
) -> float:
    structural = jaccard_distance(a.fingerprint, b.fingerprint)
    velocity = velocity_profile_distance(a, b)
    return (1 - velocity_weight) * structural + velocity_weight * velocity


def build_distance_matrix(
    patterns: list[DistilledPattern], velocity_weight: float = DEFAULT_VELOCITY_WEIGHT
) -> np.ndarray:
    n = len(patterns)
    matrix = np.zeros((n, n))
    for i in range(n):
        for j in range(i + 1, n):
            d = pattern_distance(patterns[i], patterns[j], velocity_weight)
            matrix[i, j] = d
            matrix[j, i] = d
    return matrix
