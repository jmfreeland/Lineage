# Lineage — Evolving MIDI Grooves
### Design Document (v0.2)

## Concept

Lineage takes a simple, reasonably basic drum groove and brings it to life over time — evolving it, embellishing it, making it imperfect in pleasing ways, and eventually reacting to the jam happening around it. The name reflects the core mechanic: every pattern has ancestors. Each generation of a groove descends from the last through traceable, controllable mutation.

This is a personal tool first — built to curate a private library of grooves, mutations, and embellishments in service of making interesting music, not a commercial product. The scope below is intentionally aspirational; Phase 1 in practice will be smaller and more personal (get a basic groove in, get a couple of mutation types working end-to-end, get save/recall working, and start actually using it).

Visual/brand direction: professional. Superior Drummer meets Bitwig.

---

## 1. Core Libraries

- **Groove library** — visible and accessible across any preset, not siloed per-patch
- **Mutation library** — scriptable, so new mutation types can be authored quickly rather than working only within a fixed built-in set
- **Fill library**
- **Embellishment library**
- **Preset-level pool** — a per-preset working set of jams and mutations pulled from the libraries above

## 2. Mutation System

- Multiple mutation *types*, each applicable independently
- Mutations apply per-bar, or to a chosen range of bars — not necessarily the whole pattern at once
- Musical guardrails/constraints per mutation type (weighted/probabilistic) so evolution stays musically coherent rather than drifting into noise — more a "tasteful vs. chaotic" dial than a strict correctness check, since happy accidents are part of the point
- Freeze/lock individual lanes (e.g. lock the kick, let hi-hats and fills keep evolving) so mutation can be scoped to part of the kit
- Genre-aware fill/embellishment logic — a jazz fill and a metal fill shouldn't be drawn from the same pool
- Preview mutations before committing them to the lineage, rather than every generated child automatically becoming a permanent node

## 3. Lineage / History

- The signature feature: a visual genealogy view — a literal branching tree of mutation history
- Click back to any ancestor state to audition or restore it
- Preset morphing/crossfade between two lineage states of the *same* groove
- Cross-groove blending — combine lanes from two different library grooves (e.g. kick from one, hats from another); a distinct operation from lineage morphing
- Export the "genome" (the lineage tree / mutation data itself), not just the resulting MIDI

## 4. Groove & Lane Data Model (decided)

A groove is a set of **lanes**, not a single flat note stream.

Each lane:

- `id` — stable unique identifier, used for lineage/lock/mutation targeting
- `type` — semantic role (kick, snare, hihat, tom, ride, crash, perc, ...), used for mutation defaults, genre-aware fill/embellishment selection, and lock grouping
- `label` — optional user-facing name (e.g. "Kick - Sub")
- `outputMapping` — MIDI note/channel or plugin output target, kept independent of `type`
- `loopLengthBars` — the lane's own cycle length
- `notes` — the note events for this lane
- `lockState`, `mutationState`

Notes on the model:

- Multiple lanes can share a `type` for layering (e.g. a "Kick - Sub" lane and a "Kick - Click" lane both typed `kick`), each with its own `outputMapping`. `type` and `id` together support this: `type` groups lanes for musical/UI purposes, `id` addresses one specifically.
- Lanes are not required to share a loop length. A shared master timeline/tempo + reference bar length anchors all lanes; mutation operations are addressed in master-grid bar coordinates and mapped onto whatever portion of each lane falls in that range.
- Independent lane lengths are a deliberate feature: a 3-bar hi-hat lane against a 4-bar kick lane produces a slowly drifting phase relationship (Reich-style phasing) as a cheap, musically rich source of evolution — complementary to, not a replacement for, explicit drift/modulation mutations.

## 5. Modulation & Evolution Paths

- Drift-style modulation in various flavors
- Freeform or point-based drawing of an evolution path across bars
- Complex linked modulation tools for advanced users
- Arrangement awareness: sync to DAW timeline/song position so evolution behaves differently across sections (tighter verses, looser choruses/breakdowns)
- An energy/intensity macro that sweeps density + embellishment together, useful for build-ups

## 6. Humanization & Feel

- Deep options for humanization of feel and groove (timing, velocity, micro-timing drift, etc.)

## 7. Reproducibility & Workflow

- Random seed selection when reproducibility is desired
- MIDI recorded from the last play, drag-and-droppable to a clip
- Quick save to library
- Bounce to standard MIDI file for DAW handoff

## 8. Listening & Extraction (later phase)

- Listen to the last X bars of output — from the main bus or other tracks — and use that data to drive evolution
- Drag in MIDI or audio clips and extract useful information from them: feel, embellishments, fills
- Flagged as the most engineering-heavy area (real-time onset/rhythm analysis) — treat as its own later milestone rather than bundling with core v1 features

## 9. Performance-Oriented Controls

- Live macro controls for real-time nudging (e.g. a "chaos" knob, a "density" knob) separate from the deeper modulation tools
- Momentary vs. latched triggers for mutations, useful in a live set
- MIDI learn / hardware controller mapping

---

## Suggested Phasing

**Phase 1 — Core Engine**
Lane-based groove/mutation/fill/embellishment libraries, per-bar mutation application, basic humanization, seed-based reproducibility, quick save, MIDI drag-out.

**Phase 2 — Lineage & Modulation**
Genealogy visualization, freeze/lock lanes, drift modulation, freeform/point-based evolution paths, arrangement sync, energy macro.

**Phase 3 — Performance & Interop**
Live macros, MIDI learn, genome export/import, preset morphing, cross-groove blending.

**Phase 4 — Listening & Extraction**
Audio/MIDI analysis of external input (own output, other tracks, dragged-in clips) to drive evolution and extract feel/fills/embellishments.

---

## Open Design Questions

- ~~Data model for the lineage tree (how mutation history is stored/versioned)~~ — resolved for the groove/lane layer, see §4. Still open: how lineage tree *nodes* reference lane-level state (full snapshot per node vs. diff/patch per node).
- Scripting format for user-authored mutations (DSL vs. JS-like sandbox vs. visual node graph)
- How "constraints" per mutation type are authored and surfaced in the UI without overwhelming the user
