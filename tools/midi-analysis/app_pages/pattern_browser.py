"""Sortable/filterable pattern table plus a piano-roll view (voice lanes x
canonical step, marker size/color by average velocity) and full stats for
whichever pattern is selected."""

from __future__ import annotations

from typing import Any

import pandas as pd
import plotly.graph_objects as go
import streamlit as st

# Rendering order for voice lanes in the piano-roll — low drums to high
# cymbals, roughly how a kit would read top-to-bottom on a real chart.
VOICE_ORDER = [
    "kick", "snare", "clap", "tom", "pedal_hihat", "closed_hihat", "open_hihat", "ride", "crash", "perc", "other",
]


def _voice_sort_key(voice: str) -> tuple[int, str]:
    if voice in VOICE_ORDER:
        return (VOICE_ORDER.index(voice), voice)
    return (len(VOICE_ORDER), voice)


def render(data: dict[str, Any]) -> None:
    st.header("Pattern Browser")

    cluster_label_by_id = {c["cluster_id"]: c["label"] for c in data["clustering"]["clusters"]}

    rows = []
    for pattern in data["patterns"]:
        rows.append(
            {
                "id": pattern["id"],
                "occurrences": pattern["occurrences"],
                "occurrence_share": pattern["occurrence_share"],
                "density": pattern["density"],
                "voices": ", ".join(sorted(pattern["voices"].keys())),
                "cluster": cluster_label_by_id.get(pattern["cluster_id"], "-"),
            }
        )
    table = pd.DataFrame(rows)

    filter_col, min_col = st.columns(2)
    voice_filter = filter_col.text_input("Voice contains", "")
    min_occurrences = min_col.number_input("Min occurrences", min_value=0, value=0, step=1)

    filtered = table
    if voice_filter:
        filtered = filtered[filtered["voices"].str.contains(voice_filter, case=False)]
    if min_occurrences:
        filtered = filtered[filtered["occurrences"] >= min_occurrences]

    st.dataframe(filtered, width="stretch", height=min(400, 40 + 35 * len(filtered)))

    if filtered.empty:
        st.info("No patterns match the current filters.")
        return

    selected_id = st.selectbox("Inspect pattern", filtered["id"].tolist())
    pattern = next(p for p in data["patterns"] if p["id"] == selected_id)

    st.subheader(f"{pattern['id']} — {pattern['occurrences']} occurrences ({pattern['occurrence_share'] * 100:.1f}% of bars)")
    _render_piano_roll(pattern)

    stat_cols = st.columns(3)
    stat_cols[0].metric("Note count", pattern["note_count"])
    stat_cols[1].metric("Density", f"{pattern['density']:.3f}")
    stat_cols[2].metric("Cluster", cluster_label_by_id.get(pattern["cluster_id"], "not clustered"))

    with st.expander("Occurrences (source files / bar indices)"):
        refs_df = pd.DataFrame(pattern["occurrence_refs"])
        st.dataframe(refs_df, width="stretch")
        if pattern["occurrence_refs_truncated"]:
            st.caption(f"Showing the first {len(refs_df)} of {pattern['occurrences']} occurrences.")


def _render_piano_roll(pattern: dict[str, Any]) -> None:
    voices = sorted(pattern["voices"].keys(), key=_voice_sort_key)
    fig = go.Figure()
    for lane_index, voice in enumerate(voices):
        positions = pattern["voices"][voice]
        avg_velocities = []
        for position in positions:
            profile = pattern["slot_velocity"].get(f"{voice}@{position}")
            avg_velocities.append(profile["avg_velocity"] if profile else 100.0)
        fig.add_trace(
            go.Scatter(
                x=positions,
                y=[lane_index] * len(positions),
                mode="markers",
                marker=dict(
                    size=[8 + v / 127 * 16 for v in avg_velocities],
                    color=avg_velocities,
                    colorscale="Oranges",
                    cmin=0,
                    cmax=127,
                    showscale=(lane_index == 0),
                    colorbar=dict(title="velocity") if lane_index == 0 else None,
                ),
                name=voice,
                hovertemplate=f"{voice} @ %{{x}}<br>avg velocity: %{{marker.color:.0f}}<extra></extra>",
            )
        )
    fig.update_yaxes(tickmode="array", tickvals=list(range(len(voices))), ticktext=voices, title=None)
    fig.update_xaxes(title="Beat", range=[-0.25, pattern["beats_per_bar"] + 0.25])
    fig.update_layout(height=max(160, 50 + 40 * len(voices)), showlegend=False, margin=dict(l=10, r=10, t=10, b=40))
    st.plotly_chart(fig, width="stretch")
