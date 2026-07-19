# Lineage — Evolving MIDI Grooves
### Design Document (v0.3)

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

**Scripting format (decided):** user-authored mutations are written in an embedded JS-like sandbox rather than a custom DSL or a visual node graph — no language to design/maintain, and no ramp-up cost since it's syntax the user already knows. A script is a narrow, well-defined function rather than open access to plugin internals: `(laneSnapshot(s), barRange, seededRng, params) → newNoteData`. Keeping the contract this tight means scripts stay composable, stay cheap enough to run per-pass in live loop mode (§10), and can be prototyped outside the plugin entirely (plain Node/browser JS) before being wired in. Built-in mutation types remain simple parameterized knobs for everyday use; scripting is the escape hatch for anything the built-ins don't cover. A scripted node's provenance (§3 node model) stores the script (or a versioned reference to it in the mutation library) as its `mutationType`, so genome export/replay-traceability holds even for custom scripts. A visual node-graph front end over the same underlying functions is a possible future skin, not a v1 requirement. Embeddable engine choice (e.g. QuickJS) still open.

**Constraint authoring & UI (decided):** every mutation — built-in or scripted — declares a small parameter manifest alongside its transform function (name, type, range, default, curve, `macroEligible` flag), and the UI renders that manifest generically rather than getting bespoke screens per mutation type. Overwhelm is bounded by progressive disclosure: only params marked primary show by default, the rest sit behind an "advanced" expand, and every param needs a sane default so a mutation is usable with zero tuning. Targeting (bar range, lane selection, lock state) is shared UI chrome around any mutation, not part of each mutation's own declared params, keeping each manifest small and targeting consistent everywhere. `macroEligible` params can be promoted into the live macro/MIDI-learn system (§9) instead of a separate constraint-tuning UI — day-to-day and live use only ever touches knobs deliberately promoted, and the full parameter surface only appears when tuning a specific mutation type directly.

## 3. Lineage / History

- The signature feature: a visual genealogy view — a literal branching tree of mutation history
- Click back to any ancestor state to audition or restore it
- Preset morphing/crossfade between two lineage states of the *same* groove
- Cross-groove blending — combine lanes from two different library grooves (e.g. kick from one, hats from another); a distinct operation from lineage morphing
- Export the "genome" (the lineage tree / mutation data itself), not just the resulting MIDI

**Node data model (decided):** each tree node stores a full snapshot of lane state (all lanes, notes, per-lane loop lengths) rather than a diff against its parent, plus provenance metadata (`parentId`, `mutationType`, `params`, `seed`) describing how it was derived. Snapshots give instant navigation/audition/export for any node and avoid a correctness trap where replaying an old genome through a since-changed mutation algorithm silently produces different notes than what was originally saved. Provenance metadata preserves the "genome" traceability and exportability without requiring replay to reconstruct state.

## 4. Groove & Lane Data Model (decided)

A groove is a set of **lanes**, not a single flat note stream.

Each lane:

- `id` — stable unique identifier, used for lineage/lock/mutation targeting
- `type` — semantic role (kick, snare, hihat, tom, ride, crash, perc, ...), used for mutation defaults, genre-aware fill/embellishment selection, and lock grouping
- `label` — optional user-facing name (e.g. "Kick - Sub")
- `groupId` — optional semantic link for lanes that should be targeted together (e.g. closed/open/pedal hi-hats)
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

## 10. Live Loop & Real-Time Evolution

Numerology-style live/generative use is a first-class mode, not just an offline "generate → review → commit" workflow — the goal is to be able to freeze on a section and let it keep exploring the musical space while it plays, the way you'd jam with a generative sequencer.

- **Freeze & loop**: pin playback to a bar range or a specific lineage branch and loop it, independent of host arrangement position — the base mechanic for jamming with a groove live rather than reviewing it after the fact.
- **Branch walk while looping**: optionally advance along a lineage branch on each loop pass (or every N passes) so looping *the tree* becomes a way to hear a pattern's committed history unfold over repeats, not just audit it statically.
- **Live mutation mode**: independent of branch walking, allow continuous real-time mutation while looping — each pass (or sub-interval) nudges the pattern per the active mutation types/intensity, audible immediately. This is the live extension of §2's mutation system, running continuously instead of as a one-shot generate step.
- Live mutation operates on a transient/ephemeral working state, not the persisted lineage tree directly — otherwise every loop pass would flood the tree with near-duplicate nodes and defeat the point of a curated history.
- Quick-save during a live session commits the *current* live state as a new permanent node, child of the branch point where freeze/loop began — the live-and-continuous counterpart to the "preview before commit" idea in §2.
- Optional auto-capture: periodically commit snapshots during a live session (e.g. every N loops) so nothing worth keeping is lost if you're not babysitting the save button while jamming.
- Freeze/loop, branch walk, and live mutation are independent toggles — loop a fixed frozen pattern with nothing moving, walk a branch with no live mutation, or combine all three for continuously-evolving playback.

## 11. Target Platform (decided)

Primary hosts are **Bitwig Studio** and **Ableton Live**. The format both share cleanly is **VST3**: Bitwig also supports CLAP (nice-to-have, not required) and runs on Linux, where Ableton doesn't; AU is Mac-only and doesn't help reach either host on Windows or Bitwig-on-Linux. VST3 is the one format that covers both hosts on every platform either of them runs on.

Lineage is a **MIDI effect/generator**, not an audio effect — it needs to be declared as a MIDI-only VST3 plugin (no audio buses), a distinct category from instruments/audio-FX that both hosts support but that isn't the default template path in most plugin frameworks.

**Engine/plugin bridge (decided, MVP built):** the core engine (this repo, §1–§10) is TypeScript and stays that way rather than being ported to C++. The plugin shell embeds a JS runtime (QuickJS-ng) and runs the compiled engine inside it, rather than reimplementing the engine natively. This is a direct extension of §2's "embedded JS-like sandbox" decision — instead of only user mutation scripts running in an embedded JS context, the whole engine does, so nothing gets built twice. It's viable specifically *because* this is MIDI generation, not sample-accurate audio DSP: MIDI's timing tolerances are loose enough that a JS engine in the processing path is workable, where it wouldn't be for audio-rate work.

The bridge is real, not just planned: `src/runtime.ts` is a curated engine entry point (the pure logic — groove/mutation/lineage — with no Node dependency, so no persistence/library modules) bundled by esbuild (`npm run build:runtime`) into a single script, embedded into the plugin binary via JUCE's binary-data mechanism, and executed by `plugin/Source/JsEngine`, a thin QuickJS wrapper. `PluginProcessor::processBlock()` currently converts incoming MIDI note-ons into JS values, runs them through the real `velocityHumanize` mutation (unmodified engine code, not a stand-in), and writes the mutated velocities back — proof the C++/JS boundary carries real note data through real engine code correctly. `plugin/Tests/BridgeTest.cpp` is a standalone executable that exercises this without needing a DAW.

**Transport sync and host parameters (decided, MVP built):** two more pieces of the bridge are real now, not just planned.

- *Transport sync*: `PluginProcessor::processBlock()` reads the host's tempo, time signature, and playhead position via `getPlayHead()->getPosition()` (falling back to 120bpm/4-4 if a host doesn't report position) and computes each note-on's absolute beat position from its sample offset within the block. `src/runtime.ts` uses that — plus the real `beatsPerBar` — to place each note at its actual position within the current bar, instead of the earlier placeholder (notes indexed 0, 1, 2… against a made-up per-block "bar length"). Bar-range-aware mutation targeting now means something real.
- *Host parameters*: `velocityHumanize`'s `macroEligible` "amount" param (§2) is a real `juce::AudioParameterInt` (range/default mirror the mutation's own manifest, kept in sync by hand — there's no codegen from the manifest to JUCE parameters yet) — host-automatable and MIDI-learnable like any other VST3 parameter, not a hardcoded constant. `JsEngine::processBlock()`'s bridge contract grew a generic `params` argument (a flat named-value map) to carry it across, rather than a bespoke field, so wiring up more mutations' params later doesn't need another contract change.

**Persistent session state and broader mutation/param coverage (decided, MVP built):**

- *Persistent session state and playback*: `src/runtime.ts` is a JS module loaded once and resident in the plugin's QuickJS context for its whole lifetime — module-level state there IS the plugin's session memory across `processBlock()` calls now, not recreated per block. Played notes are captured bar-by-bar (using the same real transport info as above) into a genuine `LineageTree` that grows as you play. A `RecordedProvenance` variant (§3's provenance union) marks these nodes as "this is what got played," distinct from a mutation- or live-session-derived node. The current lineage head is now rendered against each host process block while transport is running, respecting per-lane loop lengths; the JUCE shell emits note-ons and carries note-offs across block boundaries. `JsEngine::getSessionInfo()` and `__lineageGetSessionInfo()` make persistence observable/testable, while `renderPlaybackBlock()` verifies host-synchronised playback without a DAW.
- *Broader coverage*: `ghostNote` is now chained after `velocityHumanize` in the incoming-MIDI bridge pipeline, gated by a new `juce::AudioParameterBool` ("Ghost Notes", default off since it changes note count) plus its own `macroEligible` "probability" param as a `juce::AudioParameterFloat`. Wiring in a note-count-changing mutation surfaced (and fixed) a real bug: `JsEngine::processBlock()`'s read-back loop was bounded by the *input* event count, silently dropping any notes past that if a mutation's output array was longer — it now reads the actual returned array length. Incoming live notes still cannot be forecast and a mutation-created ghost that falls outside their current audio block is dropped rather than misfired. Autonomous lineage playback no longer has that limitation: it is planned deterministically over arbitrary beat ranges, so boundary-crossing generated events are available to the correct block before it starts.

What's still not covered, and still a real next step rather than a formality: cross-block scheduling for mutation-created ghosts derived from unpredictable live MIDI input (generated lineage playback and its note-offs are scheduled), the curated fill/embellishment libraries and deeper mutation catalog, and the live-loop session itself. The first rule-driven mutation/ornament/fill operations are reachable in the DAW now, but they are deliberately small starting behaviours rather than the eventual content system.

**Visual seed-groove editor and local build tooling (decided, MVP built):**

- *Seed editor*: `plugin/Source/StepSequencerComponent` — a dynamic lane-based 16-step editor. Rows can be added or removed, named freely, assigned any MIDI note, and linked with a shared semantic group such as `Hats`; empty rows are retained so metadata is not lost. Every edit calls `LineageAudioProcessor::setSeedGroove()` immediately (no separate "apply"), which calls `__lineageSetSeedGroove`: `src/runtime.ts` builds a real `Groove` from the authored lanes and **resets** the session to a fresh `LineageTree` rooted at it — a hard reset ("program a starting groove"), not a branch off whatever was captured before. That current head loops autonomously against the DAW transport and is emitted as MIDI with cross-block note-off scheduling. Grouping currently establishes joint-targeting metadata; choke/exclusivity behavior remains a later rule layered over the same group ID.
- *Thread safety*: this is the first time something other than `processBlock()` (running on the audio thread) touches `jsEngine` — the editor's step-sequencer callback runs on the message thread. `JsEngine`/QuickJS isn't thread-safe, so `LineageAudioProcessor` now guards every `jsEngine` call with a `juce::CriticalSection`. A lock on the audio thread isn't ideal real-time practice, but it's the minimal correct fix for an outright data race, consistent with the already-documented GC-pause MVP tradeoff on the audio thread.
- *Editor workspace shell*: the plugin editor now follows the intended two-tab information architecture. The Lineage tab has a left workflow rail (working dynamic seed editor, eight-bar preview, arranger scaffold, macros), a scrollable middle evolution tree, and a right library/rule rail. The Modulation tab has an extensible stochastic/hand-drawn modulation visualization, the working humanization and ghost-note parameters, and a reserved Silencer panel. Seed editing, rule-driven tree growth, and humanization are wired to the engine/host today; arranger blocks, macros, and additional modulators remain interactive UI scaffolding until their engine contracts exist.
- *Seeds vs rules*: the Library explicitly tabs these as different kinds of reusable material. The initial seed set is **Deep Pocket**, **Half Time**, and **Broken Hats**; choosing one loads its complete named/MIDI-mapped/grouped lanes into the editor and hard-resets the tree. The initial rule set is **Pocket Keeper**, **Gentle Drift**, and **Fill Forward**. Each rule owns editable weights for mutation, embellishment, fill, and hold outcomes; weights are normalized at selection time rather than required to sum to one in the UI.
- *Rule-driven trees*: `EVOLVE` applies the selected weighted rule to the current head and commits a child; `BRANCH` applies it to the current head's parent and commits a sibling variation. The first concrete operations are velocity mutation, ghost-note ornamentation, a compact phrase-ending fill, and an unchanged hold. Every child stores a `RuleProvenance` record containing rule ID, selected operation, seed, and weights, and becomes the playback head immediately. Tree cards display the rule and realized operation, while the finalized eight-bar preview updates from the new groove. These operations are intentionally basic versions—the weighted/provenance contract is the durable part that richer content will extend.
- *Mined vocabulary informs mutation (decided, MVP built)*: `tools/midi-analysis` (its own README) mines a folder of drum MIDI into `vocabulary.json` — per-voice, per-bar-position timing/velocity statistics sampled from real performances. `src/vocabulary.ts` parses and validates that JSON (collapsing the tool's finer-grained voices, e.g. `closed_hihat`/`open_hihat`, to the engine's `LaneType`); `src/vocabularyStyle.ts`'s `applyVocabularyStyle()` is a new pure transform — not a registered `MutationDefinition`, since a whole vocabulary dataset doesn't fit the flat-`MutationParam` contract — that nudges each note's timing and velocity toward the nearest matching vocabulary entry for its lane's voice and bar-position, with probability equal to that entry's observed frequency and magnitude sampled around (not fixed to) its average. When a vocabulary is loaded (`__lineageSetVocabulary`, plumbed through `JsEngine::setVocabulary()`), `applyRuleGeneration()`'s "mutation" outcome uses this instead of the flat, uniform `velocityHumanize(amount: 18)` — informed variation instead of one hand-tuned constant. No vocabulary loaded → unchanged flat-humanize behavior, so this is additive, not a behavior change by default. Not yet built: a plugin UI to load a vocabulary file (this was verified end-to-end via `JsEngine::setVocabulary()`/`BridgeTest.cpp` and a Node sanity check of the bundled runtime, not through any file-picker — that's deliberately deferred to pair with real DAW testing rather than build unverifiable native-dialog UI headlessly), and embellishment/fill sampling from the vocabulary (the current schema doesn't capture fill *content*, only that a bar was flagged as a fill, so there's nothing yet to sample fills from).
- *Per-tree automatic evolution*: each tree footer has a `START`/`PAUSE` toggle and a host-bar interval selector (1, 2, 4, 8, or 16 bars; default 4). Starting schedules the first child after that many bar boundaries while MIDI playback continues independently; pausing stops growth, not sound. The selected/edited rule remains live while the tree is running. Scheduling follows DAW PPQ position rather than a wall-clock UI timer, resets on transport starts/jumps, and splits a process block when an evolution boundary lands inside it so the old head renders before the exact boundary and the new head renders after it. The eight-bar look-ahead uses the same pure rule-generation function to simulate any scheduled generations inside its horizon without committing them early; a purple `E` boundary and tinted notes identify those future states. Completed automatic nodes are passed back to the message thread for the visible tree. Loading or editing a seed creates a new tree and resets its automation to paused/every 4 bars, making the control state genuinely tree-scoped rather than global plugin state.
- *Final-MIDI look-ahead*: “Current + next” is a live two-row piano-roll miniature: four orange current bars and four teal following bars, aligned to the host's current bar and refreshed from the editor. It is not a visualization-only approximation. `src/runtime.ts` owns one deterministic arbitrary-range playback planner; both `renderPlaybackBlock()` and `renderPlaybackPreview()` query it with the same host meter and parameters. Random humanization and ghost insertion are seeded from lane identity + absolute beat, so repeated UI refreshes and later audio blocks reproduce identical events. Velocity is visible through note opacity and generated ghosts have their own colour. As fills, embellishments, and evolution/arrangement decisions become reachable in the plugin, they must feed this planner (or the head groove snapshot it reads), which makes them appear in the preview automatically rather than requiring separate UI logic.
- *Local build tooling*: `scripts/build.sh` — `npm install` (for the esbuild runtime bundle), configure, build the VST3 target, and (by default) install straight into the local VST3 folder (`~/Library/Audio/Plug-Ins/VST3` on macOS, `~/.vst3` on Linux) so it shows up in Bitwig/Ableton without manual copying. `--no-install` to skip that, `--clean` to wipe the build dir. The VST3 build has been verified headlessly on Linux and macOS; Windows remains unverified — see the note below.

**Known gap:** everything in this document has only been verified headlessly, on Linux and macOS with no audio hardware or DAW — `Lineage_VST3` compiles and `LineageBridgeTest` passes, but the plugin has never actually been loaded into Bitwig or Ableton. That's the real next validation step, on real hardware, not something further sandbox work can substitute for.

**Known simplification, flagged in code:** `JsEngine::processBlock()` runs synchronously on whichever thread calls it — currently the audio thread. QuickJS allocates and can pause for GC, which is a real risk for a hard real-time callback. Acceptable for a personal tool with generous buffer sizes for now; revisit (e.g. moving JS execution off the audio thread with a lock-free handoff) if it causes audible dropouts.

---

## Suggested Phasing

**Phase 1 — Core Engine**
Lane-based groove/mutation/fill/embellishment libraries, per-bar mutation application, basic humanization, seed-based reproducibility, quick save, MIDI drag-out.

**Phase 2 — Lineage & Modulation**
Genealogy visualization, freeze/lock lanes, drift modulation, freeform/point-based evolution paths, arrangement sync, energy macro.

**Phase 3 — Performance & Interop**
Live macros, MIDI learn, genome export/import, preset morphing, cross-groove blending, freeze/loop + live real-time evolution mode.

**Phase 4 — Listening & Extraction**
Audio/MIDI analysis of external input (own output, other tracks, dragged-in clips) to drive evolution and extract feel/fills/embellishments.

---

## Open Design Questions

- ~~Data model for the lineage tree (how mutation history is stored/versioned)~~ — resolved: groove/lane layer in §4, tree node structure (snapshot + provenance) in §3.
- ~~Scripting format for user-authored mutations~~ — resolved: embedded JS-like sandbox, see §2. Still open: which embeddable engine (e.g. QuickJS).
- ~~How "constraints" per mutation type are authored and surfaced in the UI without overwhelming the user~~ — resolved: declarative parameter manifest per mutation, generically rendered with progressive disclosure, see §2.

---

## Related / Future Projects

- **Canvas** (working title) — a harmonic counterpart to Lineage, applying the same lineage/mutation/evolution concepts to harmonic/melodic material instead of rhythmic grooves. Not in scope for this document; noted here so the naming and conceptual relationship are on record when that exploration starts.
