"""Instrument quantize-distance correlation heatmap, plus a per-instrument
quantize-distance distribution histogram."""

from __future__ import annotations

from typing import Any

import numpy as np
import pandas as pd
import plotly.express as px
import streamlit as st


def render(data: dict[str, Any]) -> None:
    st.header("Instrument Quantize-Distance Correlation")
    st.caption(
        "Pairwise correlation of each instrument's average per-bar quantize distance — "
        "\"when this instrument is sloppy in a bar, is that other instrument also sloppy in the same bar.\""
    )

    correlation = data["instrument_quantize_distance"]["correlation_matrix"]
    voices = correlation["voices"]
    if not voices:
        st.info("No instrument quantize-distance data in this library.")
        return

    matrix = np.array([[v if v is not None else np.nan for v in row] for row in correlation["matrix"]])
    fig = px.imshow(
        matrix, x=voices, y=voices, color_continuous_scale="RdBu_r", zmin=-1, zmax=1, text_auto=".2f",
        aspect="auto",
    )
    fig.update_layout(height=max(350, 60 + 40 * len(voices)))
    st.plotly_chart(fig, width="stretch")

    st.subheader("Quantize-distance distribution per instrument")
    distributions = data["instrument_quantize_distance"]["distributions"]
    if not distributions:
        st.info("No per-instrument distribution data in this library.")
        return

    selected_voice = st.selectbox("Instrument", sorted(distributions.keys()))
    distribution = distributions[selected_voice]

    bin_edges = distribution["bin_edges"]
    counts = distribution["counts"]
    centers = [round((bin_edges[i] + bin_edges[i + 1]) / 2, 2) for i in range(len(counts))]
    histogram = pd.DataFrame({"quantize_distance_ms": centers, "count": counts})
    st.bar_chart(histogram.set_index("quantize_distance_ms"))

    stat_cols = st.columns(4)
    stat_cols[0].metric("Mean (ms)", f"{distribution['mean']:.2f}")
    stat_cols[1].metric("Median (ms)", f"{distribution['median']:.2f}")
    stat_cols[2].metric("P10 (ms)", f"{distribution['p10']:.2f}")
    stat_cols[3].metric("P90 (ms)", f"{distribution['p90']:.2f}")
