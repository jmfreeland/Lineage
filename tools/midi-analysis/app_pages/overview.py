"""Corpus overview page: file/bar counts, detected grid per file, and how
much of the corpus each cluster label (groove/fill/outlier) accounts for."""

from __future__ import annotations

from typing import Any

import pandas as pd
import streamlit as st


def render(data: dict[str, Any]) -> None:
    st.header("Corpus Overview")

    col1, col2, col3, col4 = st.columns(4)
    col1.metric("Files", len(data["source_files"]))
    col2.metric("Total bars", data["total_bars"])
    col3.metric("Unique patterns", len(data["patterns"]))
    col4.metric("Canonical grid", f"1/{data['canonical_grid']}")

    st.subheader("Detected grid per file")
    grid_df = pd.DataFrame(
        [{"file": file, "grid": f"1/{grid}"} for file, grid in data["grid_per_file"].items()]
    )
    st.dataframe(grid_df, width="stretch", height=min(400, 40 + 35 * len(grid_df)))

    st.subheader("Cluster label breakdown (share of bars)")
    clusters = data["clustering"]["clusters"]
    if not clusters:
        st.info("No clusters — corpus may be too small, or every pattern was a singleton.")
        return

    label_occurrences: dict[str, int] = {}
    for cluster in clusters:
        label_occurrences[cluster["label"]] = label_occurrences.get(cluster["label"], 0) + cluster["total_occurrences"]
    label_df = pd.DataFrame(
        [{"label": label, "occurrences": count} for label, count in sorted(label_occurrences.items())]
    )
    st.bar_chart(label_df.set_index("label"))
