"""Dendrogram over the clustered patterns' precomputed linkage matrix, with
a live-adjustable distance threshold. Re-cutting the stored linkage at a
new threshold (scipy.cluster.hierarchy.fcluster) is the one legitimate live
computation in this app — cheap (O(n) on an already-materialized linkage),
unlike the O(n^2) distance matrix it was built from, which is never
recomputed here."""

from __future__ import annotations

from typing import Any

import numpy as np
import pandas as pd
import plotly.figure_factory as ff
import streamlit as st
from scipy.cluster.hierarchy import fcluster


def render(data: dict[str, Any]) -> None:
    st.header("Pattern Clusters")

    clustering = data["clustering"]
    clustered_ids = clustering["clustered_pattern_ids"]
    linkage = np.array(clustering["linkage"])

    st.caption(
        f"Distance = (1 - w) * structural (Jaccard) + w * velocity-profile, "
        f"w = {clustering['distance_metric']['velocity_weight']}. "
        f"{len(clustered_ids)} of {len(data['patterns'])} patterns were included in clustering "
        f"(the rest were too rare — see the Pattern Browser for the full frequency table)."
    )

    if linkage.shape[0] == 0 or len(clustered_ids) < 2:
        st.info("Not enough clustered patterns to render a dendrogram.")
    else:
        threshold = st.slider(
            "Cluster distance threshold", 0.0, 1.0, float(clustering["default_threshold"]), 0.01
        )

        fig = ff.create_dendrogram(
            np.zeros((len(clustered_ids), 1)),
            orientation="left",
            labels=clustered_ids,
            linkagefun=lambda _x: linkage,
            color_threshold=threshold,
        )
        fig.update_layout(height=max(400, 22 * len(clustered_ids)), margin=dict(l=10, r=10, t=10, b=10))
        st.plotly_chart(fig, width="stretch")

        live_labels = fcluster(linkage, t=threshold, criterion="distance")
        live_groups: dict[int, list[str]] = {}
        for pattern_id, label in zip(clustered_ids, live_labels):
            live_groups.setdefault(int(label), []).append(pattern_id)

        st.subheader(f"{len(live_groups)} cluster(s) at threshold {threshold:.2f}")
        for group_id in sorted(live_groups):
            st.write(f"**Cluster {group_id}:** {', '.join(live_groups[group_id])}")

    st.subheader("Precomputed clusters (from the mining run)")
    clusters = clustering["clusters"]
    if not clusters:
        st.info("No clusters.")
        return
    st.dataframe(pd.DataFrame(clusters), width="stretch")
