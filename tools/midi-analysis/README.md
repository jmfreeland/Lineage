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

## Testing

```
pip install -r requirements.txt   # includes pytest
python3 -m pytest
```

33 tests across parsing (tick/beat math, tempo-change handling, channel
fallback), quantization, base-pattern clustering, every diff category, and
two end-to-end pipeline tests including a CLI subprocess invocation. All
fixtures are **synthetic, generated MIDI with known ground truth**
(`tests/fixtures.py`) — see Limitations below for what that does and
doesn't validate.

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
