"""Orchestrator for the corpus-wide pattern-library pipeline — parallel to
vocabulary.py, but a second, independent pipeline (see module docstrings
throughout this package for how each stage differs from the existing
per-file base-pattern/diff pipeline). Ties together grid detection, per-bar
summary stats, canonical fingerprinting/distillation, distance/clustering,
and transitions into one PatternLibrary, and serializes it to the
pattern_library.json schema documented in the README.
"""

from __future__ import annotations

import sys
from collections import defaultdict
from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path

import numpy as np

from .clustering import (
    DEFAULT_CLUSTER_THRESHOLD,
    DEFAULT_MAX_PATTERNS_FOR_CLUSTERING,
    Cluster,
    cluster_patterns,
    linkage_matrix,
    select_patterns_for_clustering,
)
from .discovery import find_midi_files
from .distance import DEFAULT_VELOCITY_WEIGHT, build_distance_matrix
from .drum_map import NoteMap
from .fingerprint import CanonicalSlot
from .grid import GRID_CANDIDATES, detect_grid
from .library import DistilledPattern, SourceBar, bar_pattern_assignment, distill_patterns
from .parser import parse_midi_file
from .patterns import group_by_bar
from .stats import BarSliceStats, compute_bar_slice_stats, instrument_quantize_correlation, quantize_distance_distribution
from .transitions import DEFAULT_MAX_LAG, TransitionMatrix, build_transition_matrices

SCHEMA_VERSION = 1
DEFAULT_OCCURRENCE_REFS_CAP = 20


@dataclass
class PatternLibrary:
    source_files: list[str]
    grid_per_file: dict[str, int]
    total_bars: int
    patterns: list[DistilledPattern]
    bar_slices: list[BarSliceStats]
    instrument_quantize_distribution: dict
    instrument_quantize_correlation: dict
    distance_matrix: np.ndarray
    linkage: np.ndarray
    clusters: list[Cluster]
    clustered_pattern_ids: list[str]
    cluster_id_by_pattern_id: dict[str, int]
    transitions: dict[int, TransitionMatrix]
    velocity_weight: float
    cluster_threshold: float
    max_lag: int


def analyze_corpus(
    folder: Path,
    grid_candidates: tuple[int, ...] = GRID_CANDIDATES,
    velocity_weight: float = DEFAULT_VELOCITY_WEIGHT,
    max_cluster_patterns: int = DEFAULT_MAX_PATTERNS_FOR_CLUSTERING,
    cluster_threshold: float = DEFAULT_CLUSTER_THRESHOLD,
    max_lag: int = DEFAULT_MAX_LAG,
    note_map: NoteMap | None = None,
) -> PatternLibrary:
    midi_files = find_midi_files(folder)
    if not midi_files:
        raise ValueError(f"no .mid/.midi files found under {folder}")

    all_bars: list[SourceBar] = []
    grid_per_file: dict[str, int] = {}
    bar_slices: list[BarSliceStats] = []
    source_files: list[str] = []
    distances_by_voice: dict[str, list[float]] = defaultdict(list)

    for path in midi_files:
        try:
            parsed = parse_midi_file(path, note_map=note_map)
        except Exception as exc:  # noqa: BLE001 — one bad file shouldn't kill the whole batch
            print(f"warning: skipping {path} ({exc})", file=sys.stderr)
            continue

        source_path = str(path)
        source_files.append(source_path)
        detected_grid = detect_grid(parsed.notes, grid_candidates)
        grid_per_file[source_path] = detected_grid

        bars_by_index = group_by_bar(parsed.notes, parsed.beats_per_bar)
        for idx in sorted(bars_by_index):
            bar_notes = bars_by_index[idx]
            all_bars.append(SourceBar(source_path, idx, parsed.beats_per_bar, bar_notes))
            slice_stats = compute_bar_slice_stats(
                bar_notes, source_path, idx, parsed.beats_per_bar, detected_grid, parsed
            )
            bar_slices.append(slice_stats)
            for voice, distance in slice_stats.avg_quantize_distance_per_instrument_ms.items():
                distances_by_voice[voice].append(distance)

    if not source_files:
        raise ValueError("no files could be analyzed")

    total_bars = len(all_bars)
    voices = sorted(distances_by_voice)

    patterns = distill_patterns(all_bars)
    bar_pattern_by_file = bar_pattern_assignment(all_bars, patterns)

    clustered_patterns = select_patterns_for_clustering(patterns, max_cluster_patterns)
    distance_matrix = build_distance_matrix(clustered_patterns, velocity_weight)
    linkage = linkage_matrix(distance_matrix)
    clusters = cluster_patterns(clustered_patterns, distance_matrix, total_bars, cluster_threshold)

    cluster_id_by_pattern_id: dict[str, int] = {}
    for cluster in clusters:
        for pattern_id in cluster.pattern_ids:
            cluster_id_by_pattern_id[pattern_id] = cluster.cluster_id

    transitions = build_transition_matrices(bar_pattern_by_file, max_lag)

    return PatternLibrary(
        source_files=source_files,
        grid_per_file=grid_per_file,
        total_bars=total_bars,
        patterns=patterns,
        bar_slices=bar_slices,
        instrument_quantize_distribution=quantize_distance_distribution(distances_by_voice),
        instrument_quantize_correlation=instrument_quantize_correlation(bar_slices, voices),
        distance_matrix=distance_matrix,
        linkage=linkage,
        clusters=clusters,
        clustered_pattern_ids=[p.pattern_id for p in clustered_patterns],
        cluster_id_by_pattern_id=cluster_id_by_pattern_id,
        transitions=transitions,
        velocity_weight=velocity_weight,
        cluster_threshold=cluster_threshold,
        max_lag=max_lag,
    )


def _step_keyed_dict_to_json(values: dict[Fraction, float | int]) -> dict[str, float | int]:
    return {str(float(step)): value for step, value in sorted(values.items())}


def _slot_key(slot: CanonicalSlot) -> str:
    voice, position = slot
    return f"{voice}@{float(position)}"


def _pattern_to_dict(pattern: DistilledPattern, total_bars: int, cluster_id_by_pattern_id: dict[str, int], occurrence_refs_cap: int) -> dict:
    refs = [{"source_file": f, "bar_index": idx} for f, idx in pattern.occurrence_refs]
    truncated = occurrence_refs_cap is not None and len(refs) > occurrence_refs_cap
    if truncated:
        refs = refs[:occurrence_refs_cap]

    cluster_id = cluster_id_by_pattern_id.get(pattern.pattern_id)
    return {
        "id": pattern.pattern_id,
        "voices": pattern.voices,
        "beats_per_bar": pattern.beats_per_bar,
        "occurrences": pattern.occurrences,
        "occurrence_share": round(pattern.occurrences / total_bars, 4) if total_bars else 0.0,
        "note_count": pattern.note_count,
        "density": round(pattern.density, 4),
        "slot_velocity": {
            _slot_key(slot): {**profile.tier_counts, "avg_velocity": round(profile.avg_velocity, 2)}
            for slot, profile in pattern.slot_velocity.items()
        },
        "occurrence_refs": refs,
        "occurrence_refs_truncated": truncated,
        "cluster_id": cluster_id,
        "included_in_clustering": cluster_id is not None,
    }


def _bar_slice_to_dict(slice_stats: BarSliceStats) -> dict:
    return {
        "source_file": slice_stats.source_file,
        "bar_index": slice_stats.bar_index,
        "grid": slice_stats.grid,
        "total_notes": slice_stats.total_notes,
        "notes_per_step": _step_keyed_dict_to_json(slice_stats.notes_per_step),
        "avg_velocity": round(slice_stats.avg_velocity, 2),
        "avg_velocity_per_step": _step_keyed_dict_to_json(slice_stats.avg_velocity_per_step),
        "avg_velocity_per_instrument": {k: round(v, 2) for k, v in slice_stats.avg_velocity_per_instrument.items()},
        "min_velocity_per_instrument": slice_stats.min_velocity_per_instrument,
        "max_velocity_per_instrument": slice_stats.max_velocity_per_instrument,
        "avg_quantize_distance_per_instrument_ms": {
            k: round(v, 3) for k, v in slice_stats.avg_quantize_distance_per_instrument_ms.items()
        },
        "avg_quantize_distance_ms": round(slice_stats.avg_quantize_distance_ms, 3),
    }


def pattern_library_to_dict(
    library: PatternLibrary, include_bar_stats: bool = False, occurrence_refs_cap: int = DEFAULT_OCCURRENCE_REFS_CAP
) -> dict:
    patterns = [
        _pattern_to_dict(p, library.total_bars, library.cluster_id_by_pattern_id, occurrence_refs_cap)
        for p in library.patterns
    ]

    clusters = [
        {
            "cluster_id": c.cluster_id,
            "label": c.label,
            "pattern_ids": c.pattern_ids,
            "avg_density": round(c.avg_density, 4),
            "total_occurrences": c.total_occurrences,
        }
        for c in library.clusters
    ]

    transitions_by_lag = {
        str(lag): {source: dict(targets) for source, targets in matrix.counts.items()}
        for lag, matrix in library.transitions.items()
    }

    return {
        "schema_version": SCHEMA_VERSION,
        "source_files": library.source_files,
        "grid_per_file": library.grid_per_file,
        "canonical_grid": 32,
        "total_bars": library.total_bars,
        "patterns": patterns,
        "instrument_quantize_distance": {
            "distributions": library.instrument_quantize_distribution,
            "correlation_matrix": library.instrument_quantize_correlation,
        },
        "clustering": {
            "distance_metric": {"velocity_weight": library.velocity_weight, "structural": "jaccard"},
            "clustered_pattern_ids": library.clustered_pattern_ids,
            "distance_matrix": library.distance_matrix.tolist(),
            "linkage": library.linkage.tolist(),
            "default_threshold": library.cluster_threshold,
            "clusters": clusters,
        },
        "transitions": {"max_lag": library.max_lag, "by_lag": transitions_by_lag},
        "bar_slices": [_bar_slice_to_dict(s) for s in library.bar_slices] if include_bar_stats else None,
    }
