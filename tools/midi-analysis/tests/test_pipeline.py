"""End-to-end pipeline tests: real (synthetic) MIDI files in, vocabulary
dict out. Complements the isolated unit tests in test_diff.py/test_patterns.py
by proving the stages compose correctly."""

import json
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

from fixtures import CLOSED_HAT, CRASH, KICK, SNARE, basic_rock_bar, write_midi

from lineage_midi_analysis.vocabulary import analyze_file, build_vocabulary

ANALYZE_SCRIPT = Path(__file__).resolve().parent.parent / "analyze.py"


def _fill_bar_events(bar_start: Fraction) -> list[tuple[Fraction, int, int, Fraction]]:
    """3 bars of basic_rock_bar establish the base pattern; this 4th bar
    departs from it: a kick->crash substitution, two ghost-snare
    embellishments (on 16th-note offsets the base pattern never uses, so
    they're genuinely free positions rather than a substitution against the
    base hi-hat's 8th-note grid), and half the hi-hat 8ths dropped
    (omissions + an overall density decrease for that voice)."""
    return [
        (bar_start + 0, KICK, 100, Fraction(1, 4)),
        (bar_start + 2, CRASH, 110, Fraction(1, 4)),  # replaces the base pattern's second kick
        (bar_start + 1, SNARE, 96, Fraction(1, 4)),
        (bar_start + 3, SNARE, 96, Fraction(1, 4)),
        (bar_start + Fraction(5, 4), SNARE, 40, Fraction(1, 8)),  # ghost, off the hi-hat's 8th-note grid
        (bar_start + Fraction(13, 4), SNARE, 40, Fraction(1, 8)),  # ghost, off the hi-hat's 8th-note grid
        (bar_start + 0, CLOSED_HAT, 70, Fraction(1, 8)),
        (bar_start + 1, CLOSED_HAT, 70, Fraction(1, 8)),
        (bar_start + 2, CLOSED_HAT, 70, Fraction(1, 8)),
        (bar_start + 3, CLOSED_HAT, 70, Fraction(1, 8)),
    ]


def _build_four_bar_fixture(path: Path) -> Path:
    events: list[tuple[Fraction, int, int, Fraction]] = []
    for bar in range(3):
        events.extend(basic_rock_bar(Fraction(bar * 4)))
    events.extend(_fill_bar_events(Fraction(3 * 4)))
    return write_midi(path, events)


def test_base_pattern_is_the_three_repeated_bars(tmp_path):
    path = _build_four_bar_fixture(tmp_path / "groove.mid")
    analysis = analyze_file(str(path), grid=16, fill_threshold=0.5)

    assert analysis.base_pattern.occurrences == 3
    assert analysis.base_pattern.bar_indices == [0, 1, 2]
    assert ("kick", Fraction(0)) in analysis.base_pattern.slots
    assert ("kick", Fraction(2)) in analysis.base_pattern.slots
    assert ("snare", Fraction(1)) in analysis.base_pattern.slots


def test_fourth_bar_is_flagged_as_a_fill(tmp_path):
    path = _build_four_bar_fixture(tmp_path / "groove.mid")
    analysis = analyze_file(str(path), grid=16, fill_threshold=0.5)

    fill_bars = [b for b in analysis.bar_results if b.is_fill]
    assert [b.bar_index for b in fill_bars] == [3]
    assert fill_bars[0].departure_score > 0.5


def test_fill_bar_contains_expected_diff_categories(tmp_path):
    path = _build_four_bar_fixture(tmp_path / "groove.mid")
    analysis = analyze_file(str(path), grid=16, fill_threshold=0.5)

    fill_bar = next(b for b in analysis.bar_results if b.bar_index == 3)
    categories = {d.category for d in fill_bar.diffs}
    assert "substitution" in categories
    assert "embellishment" in categories
    assert "omission" in categories
    assert "density_change" in categories

    substitution = next(d for d in fill_bar.diffs if d.category == "substitution")
    assert substitution.voice == "crash"
    assert "kick" in substitution.detail


def test_vocabulary_excludes_fill_bars_from_variation_frequency(tmp_path):
    path = _build_four_bar_fixture(tmp_path / "groove.mid")
    analysis = analyze_file(str(path), grid=16, fill_threshold=0.5)
    vocabulary = build_vocabulary([analysis], fill_threshold=0.5)

    assert vocabulary["schema_version"] == 1
    assert len(vocabulary["fills"]) == 1
    assert vocabulary["fills"][0]["bar_index"] == 3
    assert vocabulary["fills"][0]["phrase_boundary"] is True  # bar 3 is the 4th bar of the phrase

    # Bars 0-2 are exact repeats of the base pattern, so with no fill bars
    # counted, there should be no leftover variation noise from bar 3.
    fill_only_categories = {"substitution", "embellishment"}
    assert not any(v["category"] in fill_only_categories for v in vocabulary["variations"])


def test_cli_end_to_end_produces_valid_json(tmp_path):
    corpus_dir = tmp_path / "corpus"
    _build_four_bar_fixture(corpus_dir / "groove.mid")
    out_path = tmp_path / "vocabulary.json"

    result = subprocess.run(
        [sys.executable, str(ANALYZE_SCRIPT), str(corpus_dir), "--out", str(out_path)],
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    assert out_path.exists()
    data = json.loads(out_path.read_text())
    assert data["schema_version"] == 1
    assert len(data["base_patterns"]) == 1
    assert data["base_patterns"][0]["occurrences"] == 3


def test_cli_reports_error_for_empty_folder(tmp_path):
    empty_dir = tmp_path / "empty"
    empty_dir.mkdir()
    result = subprocess.run(
        [sys.executable, str(ANALYZE_SCRIPT), str(empty_dir)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode != 0
    assert "no .mid" in result.stderr
