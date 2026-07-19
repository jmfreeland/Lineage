"""End-to-end pattern-library pipeline tests: real (synthetic) MIDI files
in, PatternLibrary/pattern_library.json out. Mirrors test_pipeline.py's
convention for the existing vocabulary pipeline, proving this second,
independent pipeline's stages compose correctly."""

import json
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

from fixtures import CLOSED_HAT, KICK, SNARE, basic_rock_bar, write_midi

from lineage_midi_analysis.pattern_library import analyze_corpus, pattern_library_to_dict

BUILD_LIBRARY_SCRIPT = Path(__file__).resolve().parent.parent / "build_library.py"

TOM = 45


def _fill_bar(bar_start: Fraction) -> list[tuple[Fraction, int, int, Fraction]]:
    """A busy tom fill, structurally distinct from basic_rock_bar's shape."""
    return [
        (bar_start + 0, TOM, 90, Fraction(1, 8)),
        (bar_start + Fraction(1, 2), TOM, 95, Fraction(1, 8)),
        (bar_start + 1, TOM, 100, Fraction(1, 8)),
        (bar_start + Fraction(3, 2), TOM, 105, Fraction(1, 8)),
        (bar_start + 2, TOM, 110, Fraction(1, 8)),
        (bar_start + Fraction(5, 2), TOM, 115, Fraction(1, 8)),
        (bar_start + 3, TOM, 120, Fraction(1, 8)),
    ]


def _build_corpus(corpus_dir: Path) -> None:
    file_a_events: list[tuple[Fraction, int, int, Fraction]] = []
    for bar in range(3):
        file_a_events.extend(basic_rock_bar(Fraction(bar * 4)))
    write_midi(corpus_dir / "a.mid", file_a_events)

    file_b_events: list[tuple[Fraction, int, int, Fraction]] = []
    for bar in range(2):
        file_b_events.extend(basic_rock_bar(Fraction(bar * 4)))
    file_b_events.extend(_fill_bar(Fraction(2 * 4)))
    write_midi(corpus_dir / "b.mid", file_b_events)


def test_analyze_corpus_shape(tmp_path):
    corpus_dir = tmp_path / "corpus"
    _build_corpus(corpus_dir)

    library = analyze_corpus(corpus_dir)

    assert len(library.source_files) == 2
    assert library.total_bars == 6
    # basic_rock_bar's notes (quarter-note kick/snare, 8th-note hats) all
    # land exactly on 8th-note grid lines, so an 8th-note grid genuinely
    # fits without overfitting to a finer resolution.
    assert set(library.grid_per_file.values()) == {8}

    # The rock-bar pattern occurs 5 times (3 in a.mid, 2 in b.mid); the
    # fill occurs once. Distillation must find both as distinct patterns.
    occurrence_counts = sorted(p.occurrences for p in library.patterns)
    assert occurrence_counts == [1, 5]

    dominant = next(p for p in library.patterns if p.occurrences == 5)
    assert dominant.voices["kick"] == [0.0, 2.0]
    assert dominant.voices["snare"] == [1.0, 3.0]

    assert set(library.instrument_quantize_correlation["voices"]) >= {"kick", "snare", "closed_hihat"}

    dominant_cluster_id = library.cluster_id_by_pattern_id[dominant.pattern_id]
    dominant_cluster = next(c for c in library.clusters if c.cluster_id == dominant_cluster_id)
    assert dominant_cluster.label == "groove"

    assert set(library.transitions.keys()) == set(range(1, 9))
    # a.mid contributes two rock->rock lag-1 transitions (bar0->1, bar1->2).
    assert library.transitions[1].counts[dominant.pattern_id][dominant.pattern_id] >= 2


def test_pattern_library_to_dict_is_json_serializable(tmp_path):
    corpus_dir = tmp_path / "corpus"
    _build_corpus(corpus_dir)
    library = analyze_corpus(corpus_dir)

    data = pattern_library_to_dict(library)
    serialized = json.dumps(data)  # must not raise

    reloaded = json.loads(serialized)
    assert reloaded["schema_version"] == 1
    assert reloaded["total_bars"] == 6
    assert reloaded["bar_slices"] is None
    assert len(reloaded["patterns"]) == 2


def test_cli_end_to_end_produces_valid_json(tmp_path):
    corpus_dir = tmp_path / "corpus"
    _build_corpus(corpus_dir)
    out_path = tmp_path / "pattern_library.json"

    result = subprocess.run(
        [sys.executable, str(BUILD_LIBRARY_SCRIPT), str(corpus_dir), "--out", str(out_path), "--include-bar-stats"],
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    assert out_path.exists()
    data = json.loads(out_path.read_text())
    assert data["schema_version"] == 1
    assert data["total_bars"] == 6
    assert len(data["patterns"]) == 2
    assert data["bar_slices"] is not None
    assert len(data["bar_slices"]) == 6
    assert data["clustering"]["clusters"]


def test_cli_reports_error_for_empty_folder(tmp_path):
    empty_dir = tmp_path / "empty"
    empty_dir.mkdir()
    result = subprocess.run(
        [sys.executable, str(BUILD_LIBRARY_SCRIPT), str(empty_dir)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode != 0
    assert "no .mid" in result.stderr


def test_cli_reports_error_for_missing_folder(tmp_path):
    result = subprocess.run(
        [sys.executable, str(BUILD_LIBRARY_SCRIPT), str(tmp_path / "does-not-exist")],
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode != 0
    assert "not a directory" in result.stderr
