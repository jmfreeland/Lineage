"""Pick a pattern and a lag, see which patterns tend to follow it — reads
straight from the precomputed transitions.by_lag counts (pure/cheap
normalization to probabilities here, not a re-mine)."""

from __future__ import annotations

from typing import Any

import pandas as pd
import streamlit as st


def render(data: dict[str, Any]) -> None:
    st.header("Transition Explorer")

    pattern_ids = [p["id"] for p in data["patterns"]]
    if not pattern_ids:
        st.info("No patterns in this library.")
        return

    selected_pattern = st.selectbox("Source pattern", pattern_ids)
    max_lag = data["transitions"]["max_lag"]
    lag = st.slider("Lag (bars)", 1, max_lag, 1)

    counts = data["transitions"]["by_lag"].get(str(lag), {}).get(selected_pattern, {})
    if not counts:
        st.info(f"No observed transitions from {selected_pattern} at lag {lag}.")
        return

    total = sum(counts.values())
    rows = sorted(
        ({"pattern": pid, "probability": count / total, "count": count} for pid, count in counts.items()),
        key=lambda row: -row["probability"],
    )
    table = pd.DataFrame(rows)

    st.bar_chart(table.set_index("pattern")["probability"])
    st.dataframe(table, width="stretch")
