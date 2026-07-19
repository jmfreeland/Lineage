"""Headless Streamlit smoke tests via streamlit.testing.v1.AppTest — no
browser involved. Not a claim of full UI coverage (this repo has no other
UI-testing precedent), just a guard against every page raising an
exception on both a normal multi-pattern library and a degenerate
single-clustered-pattern one (where the dendrogram has nothing to draw).

AppTest doesn't expose a documented way to pass argv to a script under
test (there's no `args=` on `from_file`), so `sys.argv` is set directly
before `.run()` — the app script executes in-process and reads
`sys.argv[1:]` itself, same as it would from a real `streamlit run` CLI
invocation."""

from __future__ import annotations

import subprocess
import sys
from fractions import Fraction
from pathlib import Path

import pytest
from fixtures import basic_rock_bar, write_midi
from streamlit.testing.v1 import AppTest

APP_SCRIPT = Path(__file__).resolve().parent.parent / "app.py"
BUILD_LIBRARY_SCRIPT = Path(__file__).resolve().parent.parent / "build_library.py"


def _build_library_json(tmp_path: Path, max_cluster_patterns: int | None = None) -> Path:
    corpus_dir = tmp_path / "corpus"
    events: list[tuple[Fraction, int, int, Fraction]] = []
    for bar in range(3):
        events.extend(basic_rock_bar(Fraction(bar * 4)))
    write_midi(corpus_dir / "a.mid", events)

    out_path = tmp_path / "pattern_library.json"
    args = [sys.executable, str(BUILD_LIBRARY_SCRIPT), str(corpus_dir), "--out", str(out_path)]
    if max_cluster_patterns is not None:
        args += ["--max-cluster-patterns", str(max_cluster_patterns)]
    result = subprocess.run(args, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    return out_path


@pytest.fixture
def library_path(tmp_path) -> Path:
    return _build_library_json(tmp_path)


def _run_app(monkeypatch, library_path: Path) -> AppTest:
    monkeypatch.setattr(sys, "argv", ["streamlit", "--library", str(library_path)])
    at = AppTest.from_file(str(APP_SCRIPT), default_timeout=30)
    at.run()
    return at


def test_app_loads_library_and_shows_nav(monkeypatch, library_path):
    at = _run_app(monkeypatch, library_path)
    assert not at.exception
    assert at.sidebar.radio[0].options == [
        "Overview", "Pattern Browser", "Clusters", "Instrument Correlation", "Transitions",
    ]


def test_app_without_a_library_shows_upload_prompt(monkeypatch):
    monkeypatch.setattr(sys, "argv", ["streamlit"])
    at = AppTest.from_file(str(APP_SCRIPT), default_timeout=30)
    at.run()
    assert not at.exception
    assert any("upload" in info.value.lower() for info in at.info)


@pytest.mark.parametrize(
    "page", ["Overview", "Pattern Browser", "Clusters", "Instrument Correlation", "Transitions"]
)
def test_every_page_renders_without_exception(monkeypatch, library_path, page):
    at = _run_app(monkeypatch, library_path)
    at.sidebar.radio[0].set_value(page).run()
    assert not at.exception


def test_pattern_browser_widgets_do_not_raise(monkeypatch, library_path):
    at = _run_app(monkeypatch, library_path)
    at.sidebar.radio[0].set_value("Pattern Browser").run()

    at.text_input[0].set_value("kick").run()
    assert not at.exception

    # An impossible filter should hit the "no patterns match" branch cleanly.
    at.number_input[0].set_value(999999).run()
    assert not at.exception
    assert any("no patterns match" in info.value.lower() for info in at.info)


def test_cluster_view_handles_a_single_clustered_pattern(monkeypatch, tmp_path):
    # max_cluster_patterns=1 forces exactly one pattern into clustering —
    # the linkage matrix is then empty (nothing to link), which the
    # dendrogram branch must handle without raising.
    path = _build_library_json(tmp_path, max_cluster_patterns=1)
    at = _run_app(monkeypatch, path)
    at.sidebar.radio[0].set_value("Clusters").run()
    assert not at.exception
    assert any("not enough clustered patterns" in info.value.lower() for info in at.info)
