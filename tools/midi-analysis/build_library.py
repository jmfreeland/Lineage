#!/usr/bin/env python3
"""CLI: mine a folder of drum MIDI files into a pattern_library.json — a
corpus-wide library of unique bar patterns with frequency, distance,
clustering, and transition data. Complementary to analyze.py's
vocabulary.json (per-file base-pattern + diff-based variation stats); see
README.md's "Pattern Library pipeline" section for how the two differ.

    python3 build_library.py <folder> --out pattern_library.json \
        [--grid-candidates 8,16,32] [--velocity-weight 0.3] \
        [--max-cluster-patterns 500] [--cluster-threshold 0.4] \
        [--max-lag 8] [--include-bar-stats]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lineage_midi_analysis.clustering import DEFAULT_CLUSTER_THRESHOLD, DEFAULT_MAX_PATTERNS_FOR_CLUSTERING
from lineage_midi_analysis.distance import DEFAULT_VELOCITY_WEIGHT
from lineage_midi_analysis.grid import GRID_CANDIDATES
from lineage_midi_analysis.pattern_library import DEFAULT_OCCURRENCE_REFS_CAP, analyze_corpus, pattern_library_to_dict
from lineage_midi_analysis.transitions import DEFAULT_MAX_LAG


def _parse_grid_candidates(raw: str) -> tuple[int, ...]:
    return tuple(int(g.strip()) for g in raw.split(","))


def main() -> int:
    parser = argparse.ArgumentParser(description="Mine a folder of drum MIDI files into a pattern_library.json")
    parser.add_argument("folder", type=Path, help="Folder of .mid/.midi files (searched recursively)")
    parser.add_argument("--out", type=Path, default=Path("pattern_library.json"), help="Output JSON path")
    parser.add_argument(
        "--grid-candidates",
        type=_parse_grid_candidates,
        default=GRID_CANDIDATES,
        help=f"Comma-separated straight-grid candidates to auto-detect between (default {list(GRID_CANDIDATES)})",
    )
    parser.add_argument(
        "--velocity-weight",
        type=float,
        default=DEFAULT_VELOCITY_WEIGHT,
        help=f"Weight of velocity-profile distance vs. structural (Jaccard) distance (default {DEFAULT_VELOCITY_WEIGHT})",
    )
    parser.add_argument(
        "--max-cluster-patterns",
        type=int,
        default=DEFAULT_MAX_PATTERNS_FOR_CLUSTERING,
        help=f"Cap on how many top-occurrence patterns go into the O(n^2) distance/cluster step (default {DEFAULT_MAX_PATTERNS_FOR_CLUSTERING})",
    )
    parser.add_argument(
        "--cluster-threshold",
        type=float,
        default=DEFAULT_CLUSTER_THRESHOLD,
        help=f"Distance threshold to cut the pattern dendrogram at (default {DEFAULT_CLUSTER_THRESHOLD})",
    )
    parser.add_argument("--max-lag", type=int, default=DEFAULT_MAX_LAG, help=f"Max bar-transition lag (default {DEFAULT_MAX_LAG})")
    parser.add_argument(
        "--include-bar-stats",
        action="store_true",
        help="Include the full per-bar-slice stats array in the output (large for a big corpus; omitted by default)",
    )
    args = parser.parse_args()

    if not args.folder.is_dir():
        print(f"error: {args.folder} is not a directory", file=sys.stderr)
        return 1

    try:
        library = analyze_corpus(
            args.folder,
            grid_candidates=args.grid_candidates,
            velocity_weight=args.velocity_weight,
            max_cluster_patterns=args.max_cluster_patterns,
            cluster_threshold=args.cluster_threshold,
            max_lag=args.max_lag,
        )
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    data = pattern_library_to_dict(library, include_bar_stats=args.include_bar_stats, occurrence_refs_cap=DEFAULT_OCCURRENCE_REFS_CAP)
    args.out.write_text(json.dumps(data, indent=2))

    print(f"Analyzed {len(library.source_files)} file(s), {library.total_bars} bar(s) -> {args.out}")
    print(
        f"  {len(library.patterns)} unique pattern(s), "
        f"{len(library.clusters)} cluster(s) "
        f"({sum(1 for c in library.clusters if c.label == 'groove')} groove, "
        f"{sum(1 for c in library.clusters if c.label == 'fill')} fill, "
        f"{sum(1 for c in library.clusters if c.label == 'outlier')} outlier)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
