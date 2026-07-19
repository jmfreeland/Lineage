import json
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

from fixtures import KICK, SNARE, write_midi

NOTE_USAGE_SCRIPT = Path(__file__).resolve().parent.parent / "note_usage.py"

# Outside GM's percussion range — the same kind of "extra articulation"
# note a real drum library export would use for e.g. a rimshot.
UNMAPPED_PITCH = 100


def _build_corpus(corpus_dir: Path) -> None:
    write_midi(
        corpus_dir / "a.mid",
        events=[
            (Fraction(0), KICK, 100, Fraction(1, 4)),
            (Fraction(1), SNARE, 96, Fraction(1, 4)),
            (Fraction(2), UNMAPPED_PITCH, 80, Fraction(1, 4)),
            (Fraction(3), UNMAPPED_PITCH, 80, Fraction(1, 4)),
        ],
    )


def test_reports_pitch_usage_and_current_mapping(tmp_path):
    corpus_dir = tmp_path / "corpus"
    _build_corpus(corpus_dir)

    result = subprocess.run(
        [sys.executable, str(NOTE_USAGE_SCRIPT), str(corpus_dir)],
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    assert "3 distinct pitch" in result.stdout
    assert str(KICK) in result.stdout
    assert "kick" in result.stdout
    assert str(UNMAPPED_PITCH) in result.stdout
    assert "other" in result.stdout


def test_note_map_flag_changes_reported_voice(tmp_path):
    corpus_dir = tmp_path / "corpus"
    _build_corpus(corpus_dir)
    note_map_path = tmp_path / "note_map.json"
    note_map_path.write_text(json.dumps({str(UNMAPPED_PITCH): "snare"}))

    result = subprocess.run(
        [sys.executable, str(NOTE_USAGE_SCRIPT), str(corpus_dir), "--note-map", str(note_map_path)],
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    assert "override" in result.stdout
    lines = [line for line in result.stdout.splitlines() if line.strip().startswith(str(UNMAPPED_PITCH))]
    assert len(lines) == 1
    assert "snare" in lines[0]
    assert "other" not in lines[0]


def test_emit_template_writes_one_entry_per_pitch_seen(tmp_path):
    corpus_dir = tmp_path / "corpus"
    _build_corpus(corpus_dir)
    template_path = tmp_path / "template.json"

    result = subprocess.run(
        [sys.executable, str(NOTE_USAGE_SCRIPT), str(corpus_dir), "--emit-template", str(template_path)],
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert result.returncode == 0, result.stderr
    assert template_path.exists()
    template = json.loads(template_path.read_text())
    assert template == {str(KICK): "kick", str(SNARE): "snare", str(UNMAPPED_PITCH): "other"}


def test_reports_error_for_empty_folder(tmp_path):
    empty_dir = tmp_path / "empty"
    empty_dir.mkdir()
    result = subprocess.run(
        [sys.executable, str(NOTE_USAGE_SCRIPT), str(empty_dir)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode != 0
    assert "no .mid" in result.stderr
