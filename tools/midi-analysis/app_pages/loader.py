"""The sole read boundary for the Streamlit app — every page renders from
the dict this returns, never touching MIDI files or re-running the mining
pipeline. Keeps "mining" (build_library.py, batch, reproducible, tested)
and "browsing" (this app, interactive) separate, matching this project's
existing analyze.py/src-vocabulary.ts split."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import streamlit as st


@st.cache_data
def load_library(source: Path | Any) -> dict[str, Any]:
    if isinstance(source, Path):
        return json.loads(source.read_text())
    return json.loads(source.getvalue())
