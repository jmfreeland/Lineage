# Lineage MIDI Groove-Variation Analysis Tool

Offline corpus-mining tool: point it at a folder of drum MIDI files and it
produces a `vocabulary.json` describing how real performances vary a base
pattern — timing shifts, velocity variation, embellishments, omissions,
substitutions, density changes, and fills. This is meant to give Lineage's
mutation/rule engine (see `../../DESIGN.md` §2 and the plugin's rule-driven
auto-evolution) a *sampled-from-real-performance* source of variation to
draw from, as an alternative to purely synthetic/random mutation.

This tool is standalone — plain Python, its own dependencies, decoupled
from the JUCE plugin build. Nothing here runs inside the plugin; it only
ever produces the JSON file, which is meant to be consumed separately
(that consumption step — wiring `vocabulary.json` into the C++/TS rule
engine — isn't built yet; this is just the mining side).

## Setup

```
cd tools/midi-analysis
pip install -r requirements.txt
```

Uses `mido` (pure Python, no native build step) rather than `pretty_midi` —
`pretty_midi` failed to build from source in the environment this was
developed in (an old-setuptools/distutils incompatibility unrelated to
this project). `mido` is lower-level (you get raw MIDI messages, not
`pretty_midi`'s note/instrument objects) but the parser here does its own
note-on/note-off pairing and tempo-map handling, so nothing was lost.

## Usage

```
python3 analyze.py <folder> --out vocabulary.json [--grid 16] [--fill-threshold 0.5]
```

- `<folder>` — searched recursively for `.mid`/`.midi` files.
- `--grid` — quantization resolution: `16` (16th notes, default) or `32`
  (32nd notes). Must be a multiple of 4.
- `--fill-threshold` — a bar's departure score (irregularities / base
  pattern size) above which it's flagged as a fill instead of counted
  toward variation-frequency stats. Default `0.5`.

A generated example is checked in at `sample_corpus/` — run
`python3 make_sample_corpus.py` to regenerate `rock_groove.mid`, or just
`python3 analyze.py sample_corpus/ --out /tmp/vocabulary.json` to see the
pipeline run against it immediately.

## Pipeline

1. **Parse** (`parser.py`) — load with `mido`, pair note-on/note-off per
   channel, map GM percussion pitches to voice categories (`drum_map.py`).
   Prefers channel 10 (GM percussion); falls back to all channels if
   channel 10 has no notes, since many exported drum-only loops don't
   bother setting it.
2. **Quantize** (`quantize.py`) — snap each note's beat position to the
   nearest grid line using exact `Fraction` arithmetic (not floats — the
   clustering step hashes quantized positions, and float rounding error
   there would silently split one real pattern into several "distinct"
   ones).
3. **Find the base pattern** (`patterns.py`) — split into bars, fingerprint
   each bar as its set of quantized `(voice, position)` slots, and cluster
   by **exact** fingerprint match. The most frequent cluster is the base
   pattern. This is deliberately the simplest thing that works, not fuzzy/
   edit-distance similarity — see Limitations.
4. **Diff each bar against the base pattern** (`diff.py`) — for every
   base-pattern slot: matched → check timing offset (`timing_shift`, >5ms)
   and velocity delta (`velocity_variation`, >8) against that slot's
   average velocity across the base-pattern bars; missing → `omission`.
   For every actual slot *not* in the base pattern: if it's standing in
   for a base voice still missing at that position → `substitution`
   (detail lists exactly which base voice(s) are missing there — a
   position can have more than one expected voice, e.g. kick and hi-hat
   both on beat 2, so "which voice does this replace" is resolved against
   what's *actually* unaccounted for, not an arbitrary pick); otherwise →
   `embellishment`. Per-voice hit-count doubling/halving vs. the base
   pattern → `density_change`. A bar's departure score (irregularities /
   base pattern size) above `--fill-threshold` flags it as a `fill`
   instead of a normal variation bar.
5. **Aggregate** (`vocabulary.py`) — group diff entries across all
   non-fill bars by `(category, voice, metric_position)`, where
   `metric_position` is normalized to *fraction through the bar* (not
   absolute beat) so files in different time signatures aggregate
   meaningfully together. `frequency` = occurrences / non-fill bars
   considered. Fill bars are reported separately, each flagged with
   whether it lands on a likely phrase boundary (`bar_index % 4 == 3`).

## Output schema

```jsonc
{
  "schema_version": 1,
  "source_files": ["..."],
  "base_patterns": [
    {
      "id": "bp_01",
      "source_file": "...",
      "beats_per_bar": 4.0,
      "voices": { "kick": [0.0, 2.0], "snare": [1.0, 3.0], "closed_hihat": [0.0, 0.5, ...] },
      "occurrences": 6
    }
  ],
  "variations": [
    {
      "category": "timing_shift",
      "voice": "snare",
      "metric_position": 0.25,       // fraction through the bar, not absolute beat
      "frequency": 0.6667,
      "occurrences": 4,
      "avg_magnitude": 7.812,        // ms for timing_shift, velocity delta for velocity_variation
      "direction": "late"
    }
  ],
  "fills": [
    { "source_file": "...", "bar_index": 3, "departure_score": 1.333, "phrase_boundary": true }
  ]
}
```

This is close to but not identical to the draft schema in the original
design doc — `metric_position` became a bar-fraction rather than a named
slot (e.g. `"beat_2"`) once cross-file, cross-time-signature aggregation
was actually implemented, and a `density_change` category and per-file
`base_patterns` (one per file, not merged) were added since diffing needed
them.

## Pattern Library pipeline

A second, independent pipeline, living alongside everything above rather
than replacing it — this one distills a whole corpus into a browsable
library of **unique 1-bar patterns** with frequency, distance, clustering,
and transition information, instead of one file's single dominant "base
pattern" plus diffs against it. It exists to give richer, sampled content
(not just "a bar was flagged as a fill") for future embellishment/
humanization and probabilistic seed/arrangement work; `vocabulary.json`
keeps feeding the plugin's mutation engine exactly as before.

```
python3 build_library.py <folder> --out pattern_library.json \
    [--grid-candidates 8,16,32] [--velocity-weight 0.3] \
    [--max-cluster-patterns 500] [--cluster-threshold 0.4] \
    [--max-lag 8] [--include-bar-stats]
```

### Pipeline

1. **Auto-detect a straight quantization grid** per file (`grid.py`) —
   8th/16th/32nd notes only (no triplet/swing detection yet, see
   Limitations). Naively minimizing quantize error always prefers the
   finest grid, so instead this starts at the coarsest candidate and only
   moves to a finer one when it cuts average error by at least 20%
   (`MIN_RELATIVE_IMPROVEMENT`) — a simple elbow rule that avoids
   overfitting every file to 32nd notes.
2. **Per-bar-slice summary stats** (`stats.py`), measured against each
   file's own detected grid: total notes, notes/velocity per step, avg/min/
   max velocity per instrument, avg quantize distance per instrument and
   per note (each note weighted equally, distinct from the per-instrument
   average), plus a corpus-level quantize-distance distribution and
   pairwise instrument correlation (Pearson, on pairwise-complete per-bar
   observations).
3. **Canonical fingerprinting** (`fingerprint.py`) — every bar is
   fingerprinted at a fixed 32nd-note **canonical grid**, independent of
   the file's own detected grid, so patterns from a 16-grid file and a
   32-grid file compare equal (16 divides 32 exactly — never a false
   split). Identity is presence-only `(voice, step)`, matching the existing
   pipeline's exact-match philosophy; velocity isn't part of identity, but
   is captured separately as a per-slot 4-tier histogram (ghost/soft/
   medium/accent).
4. **Distill unique patterns** (`library.py`) — group every bar in the
   corpus by canonical fingerprint; each distinct shape becomes one
   `DistilledPattern` with a real occurrence count, not just "the most
   common one per file."
5. **Distance + clustering** (`distance.py`, `clustering.py`) — pattern
   distance blends structural (Jaccard on slot sets) and velocity-profile
   difference, weighted `(1-w)*structural + w*velocity` (default `w=0.3`).
   Only the top `--max-cluster-patterns` (default 500) most frequent
   patterns go into the O(n²) distance matrix and
   `scipy.cluster.hierarchy` average-linkage clustering — rarer patterns
   stay fully counted in the frequency table but aren't clustered. Clusters
   are labeled **groove / fill / outlier** by a density/occurrence-share
   heuristic, not purely by shape: a cluster occurring in fewer than 0.5%
   of bars is an `outlier`; one whose average density is 1.5x the corpus's
   most-frequent cluster's density is a `fill`; otherwise `groove`.
6. **Transitions** (`transitions.py`) — per lag 1-8, per source file, over
   the file's own bar sequence: which pattern tends to follow which. A
   transition at lag `L` only counts if *every* bar in between is present
   (non-empty) — a gap breaks the chain rather than silently skipping over it.

### Output schema

```jsonc
{
  "schema_version": 1,
  "source_files": ["..."],
  "grid_per_file": { "path/to/file.mid": 16 },
  "canonical_grid": 32,
  "total_bars": 842,
  "patterns": [
    {
      "id": "pat_0001",
      "voices": { "kick": [0.0, 2.0], "snare": [1.0, 3.0] },
      "beats_per_bar": 4.0,
      "occurrences": 214,
      "occurrence_share": 0.254,
      "note_count": 10,
      "density": 0.3125,
      "slot_velocity": { "kick@0.0": { "ghost": 0, "soft": 3, "medium": 180, "accent": 31, "avg_velocity": 101.2 } },
      "occurrence_refs": [{ "source_file": "...", "bar_index": 0 }],
      "occurrence_refs_truncated": true,
      "cluster_id": 3,
      "included_in_clustering": true
    }
  ],
  "instrument_quantize_distance": {
    "distributions": { "kick": { "mean": 4.2, "median": 2.1, "p10": 0.0, "p90": 9.8, "bin_edges": [...], "counts": [...] } },
    "correlation_matrix": { "voices": ["kick", "snare"], "matrix": [[1.0, 0.42], [0.42, 1.0]] }
  },
  "clustering": {
    "distance_metric": { "velocity_weight": 0.3, "structural": "jaccard" },
    "clustered_pattern_ids": ["pat_0001"],
    "distance_matrix": [[0.0]],
    "linkage": [],
    "default_threshold": 0.4,
    "clusters": [{ "cluster_id": 0, "label": "groove", "pattern_ids": ["pat_0001"], "avg_density": 0.28, "total_occurrences": 402 }]
  },
  "transitions": { "max_lag": 8, "by_lag": { "1": { "pat_0001": { "pat_0001": 180 } } } },
  "bar_slices": null
}
```
`occurrence_refs` is capped (default 20 per pattern) to keep file size
bounded for a large corpus; `bar_slices` (full per-bar stats) is omitted
unless `--include-bar-stats` is passed, since everything the overview/
browser UI needs is already in the aggregates above.

### Browsing: Streamlit GUI

```
streamlit run app.py -- --library pattern_library.json
```
Reads the precomputed JSON only — no MIDI parsing, no re-running the
pipeline live (the one exception: the cluster view re-cuts the stored
`linkage` matrix at an adjustable threshold, which is cheap since the
linkage itself is already computed). Pages: corpus overview, a filterable
pattern browser with a piano-roll view per pattern, a dendrogram/cluster
view, an instrument quantize-distance correlation heatmap, and a
transition explorer.

### Limitations (Pattern Library pipeline)

- **Straight grids only** — no triplet/swing detection, same deferred item
  as the rest of this tool (see Limitations above).
- **Presence-only pattern identity** — velocity affects distance/cluster
  labeling, not whether two bars count as "the same pattern," the same
  tradeoff class as the existing base-pattern pipeline's exact-match
  clustering.
- **Clustering thresholds are hardcoded, unvalidated defaults**
  (`DEFAULT_CLUSTER_THRESHOLD`, `FILL_DENSITY_RATIO`,
  `MIN_OCCURRENCE_SHARE_FOR_OUTLIER`, `DEFAULT_VELOCITY_WEIGHT`) — same
  "revisit with real data" posture as `diff.py`'s thresholds.
- **Clustering is capped** to the top `--max-cluster-patterns` most
  frequent patterns on a large corpus; rarer patterns keep full frequency
  stats but aren't assigned a cluster.
- **Transitions require every intermediate bar to be non-empty** for a lag
  to count — a sparse/quiet file will show fewer long-lag transitions,
  which may or may not reflect a real musical property of the source.
- **Synthetic-fixture-only test coverage**, same caveat as the rest of this
  tool — no real performed MIDI was available to validate against.

## Testing

```
pip install -r requirements.txt   # includes pytest
python3 -m pytest
```

101 tests total. 33 cover the original vocabulary pipeline: parsing
(tick/beat math, tempo-change handling, channel fallback), quantization,
base-pattern clustering, every diff category, and two end-to-end pipeline
tests including a CLI subprocess invocation. 59 cover the Pattern Library
pipeline: grid detection, per-bar-slice stats and correlation, canonical
fingerprinting, corpus-wide distillation, distance/clustering (including
the scalability cap and the groove/fill/outlier split), transitions
(including the "gap breaks the chain" contiguity rule), and its own
end-to-end/CLI tests. The remaining 9 are headless Streamlit smoke tests
(`streamlit.testing.v1.AppTest`, no browser) proving every page renders
without an exception against both a normal library and a degenerate
single-clustered-pattern one — not a claim of full UI coverage, this repo
has no other UI-testing precedent. All fixtures are **synthetic, generated
MIDI with known ground truth** (`tests/fixtures.py`) — see Limitations
below for what that does and doesn't validate.

## Limitations

- **No real performed MIDI was available to include or test against.**
  Every fixture and the checked-in `sample_corpus/rock_groove.mid` is
  programmatically generated with deliberately injected variations, not a
  real drum performance. This validates that the pipeline correctly
  detects the categories of variation it's designed to detect — it does
  *not* validate that those categories/thresholds match what real
  performances actually look like, or produce a musically meaningful
  vocabulary. That needs a real corpus, from you.
- **Exact-match clustering, not fuzzy.** Two bars that are "almost" the
  base pattern (one extra ghost note) form their own cluster rather than
  being recognized as a variation of the dominant one — they'd need to
  recur often enough to become the majority cluster themselves, or they
  just don't get counted toward `occurrences`. A real corpus with genuine
  performance noise will likely need fuzzy/edit-distance clustering to
  find a base pattern at all; this is the main thing to revisit once real
  files are in hand.
- **Tempo changes**: bar/quantization math is entirely tick-based (a beat
  is `tick / ticks_per_beat` by MIDI's own definition, independent of
  tempo), so discrete tempo changes don't distort *where* things land on
  the grid. Only millisecond-offset reporting needs tempo, and that's
  computed via full tempo-map integration (`ParsedMidi.tick_to_seconds`) —
  correctly. A continuous tempo *ramp* isn't distinguished from a
  performer's own timing drift, though; both just show up as gradually
  increasing `timing_shift` offsets.
- **Swing/shuffle grids** aren't handled specially — a swung file
  quantized against a straight 16th-note grid will show every off-beat
  16th as a consistent, large `timing_shift`, which is technically
  correct but not a useful characterization of "this file is swung."
  Detecting and quantizing against a swing grid is unbuilt.
- **Thresholds** (5ms timing, 8-velocity, 0.5 departure score, 2x density
  ratio) are simple hardcoded defaults, not tuned against real data —
  there's no real data yet to tune them against.
