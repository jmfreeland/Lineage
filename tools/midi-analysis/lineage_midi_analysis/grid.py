"""Per-file straight-grid auto-detection.

Only straight grids (8th/16th/32nd notes) are considered — triplet/swing
grid detection is a real algorithmic subproblem of its own and is
deliberately deferred until there's a real (non-synthetic) corpus to
validate it against, matching this tool's existing "revisit with real data"
posture (see diff.py's threshold docstring).
"""

from __future__ import annotations

from .parser import NoteEvent
from .quantize import quantize_distance

GRID_CANDIDATES: tuple[int, ...] = (8, 16, 32)

# A finer grid's quantization points are always a superset of a coarser
# grid's (32nd-note lines include every 16th-note line), so naively picking
# whichever grid minimizes average distance always picks the finest
# candidate — every file would "detect" as 32, which says nothing about the
# file's actual content. Instead, only move to a finer grid if it cuts
# average error by a meaningful relative amount; otherwise stay coarse.
MIN_RELATIVE_IMPROVEMENT = 0.2


def avg_quantize_distance(notes: list[NoteEvent], grid: int) -> float:
    if not notes:
        return 0.0
    return float(sum(quantize_distance(n.start_beat, grid) for n in notes)) / len(notes)


def detect_grid(notes: list[NoteEvent], candidates: tuple[int, ...] = GRID_CANDIDATES) -> int:
    """Picks the coarsest candidate grid that fits well, only refining to a
    finer one when it meaningfully reduces average quantize distance.
    Candidates are tried from coarsest to finest regardless of input order.
    An empty note list returns the middle candidate as a harmless default.
    """
    if not notes:
        sorted_candidates = sorted(candidates)
        return sorted_candidates[len(sorted_candidates) // 2]

    sorted_candidates = sorted(candidates)
    best = sorted_candidates[0]
    best_score = avg_quantize_distance(notes, best)
    for candidate in sorted_candidates[1:]:
        score = avg_quantize_distance(notes, candidate)
        if score < best_score * (1 - MIN_RELATIVE_IMPROVEMENT):
            best, best_score = candidate, score
    return best
