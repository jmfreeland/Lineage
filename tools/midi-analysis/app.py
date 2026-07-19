#!/usr/bin/env python3
"""Streamlit GUI for browsing a pattern_library.json built by
build_library.py.

    streamlit run app.py -- --library pattern_library.json

Only ever deserializes the precomputed JSON (app_pages/loader.py) — no MIDI
parsing, no re-running the mining pipeline. The one legitimate live
computation is the cluster view re-cutting its stored linkage matrix at an
adjustable threshold (app_pages/cluster_view.py), which is cheap.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import streamlit as st

sys.path.insert(0, str(Path(__file__).resolve().parent))

from app_pages import cluster_view, correlation_view, loader, overview, pattern_browser, transition_explorer

PAGES = {
    "Overview": overview,
    "Pattern Browser": pattern_browser,
    "Clusters": cluster_view,
    "Instrument Correlation": correlation_view,
    "Transitions": transition_explorer,
}


def _parse_args() -> argparse.Namespace:
    # Streamlit strips its own CLI args before "--"; anything after that is
    # this app's own argv, so argparse can be used normally here.
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path, default=None, help="Path to a pattern_library.json")
    args, _unknown = parser.parse_known_args(sys.argv[1:])
    return args


def main() -> None:
    st.set_page_config(page_title="Lineage Pattern Library", layout="wide")
    args = _parse_args()

    st.sidebar.title("Lineage Pattern Library")

    library_path = args.library
    uploaded_file = None
    if library_path is None or not library_path.exists():
        uploaded_file = st.sidebar.file_uploader("pattern_library.json", type="json")
        if uploaded_file is None:
            st.title("Lineage Pattern Library")
            st.info(
                "Provide a library with `streamlit run app.py -- --library pattern_library.json`, "
                "or upload one in the sidebar."
            )
            return

    data = loader.load_library(uploaded_file if uploaded_file is not None else library_path)

    st.sidebar.caption(f"{data['total_bars']} bars · {len(data['patterns'])} unique patterns")
    page_name = st.sidebar.radio("View", list(PAGES.keys()))

    PAGES[page_name].render(data)


if __name__ == "__main__":
    main()
