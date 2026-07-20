// Plugin runtime entry point (DESIGN.md §11) — bundled with esbuild into a
// single self-contained script and embedded in the JUCE plugin, executed
// inside QuickJS. Deliberately narrow: only the pure engine logic that has
// no Node dependency (no fs/path, so no persistence.ts/library.ts/
// jsonFileLibrary.ts/scriptMutations.ts — those are for host-side tooling,
// not the embedded runtime).
import { cloneGroove, createGroove, createLane, nextNoteId } from "./groove.js";
import { applyMutation } from "./mutation.js";
import { registerBuiltinMutations } from "./mutations/index.js";
import { createRng } from "./rng.js";
import { LineageTree } from "./lineage.js";
import { parseVocabulary, type Vocabulary } from "./vocabulary.js";
import { applyVocabularyStyle } from "./vocabularyStyle.js";
import type { Groove, LaneType, LineageNodeProvenance, NoteEvent } from "./types.js";

registerBuiltinMutations();

interface BridgeNoteEvent {
  note: number;
  velocity: number;
  channel: number;
  samplePosition: number;
  /** Absolute host beat position, computed on the C++ side from transport + samplePosition. */
  beatPosition: number;
}

interface PlaybackNoteEvent extends BridgeNoteEvent {
  /** Gate length from the stored groove, in host beats. */
  durationBeats: number;
  /** Bitfield used by the compact preview: 1 ghost, 2 humanized, 4 evolved head, 8 scheduled evolution. */
  previewFlags: number;
}

interface BridgeTransport {
  tempo: number;
  beatsPerBar: number;
  /** Beat position of sample 0 in this block — needed to convert a beat position back to a sample offset. */
  blockStartBeat: number;
  sampleRate: number;
}

type BridgeParams = Record<string, number>;

let callCounter = 0;

function wrapToBar(beat: number, bar: number, beatsPerBar: number): number {
  const local = beat - bar * beatsPerBar;
  return ((local % beatsPerBar) + beatsPerBar) % beatsPerBar;
}

// --- Rule-specific tuning beyond the five fixed weights below (DAW testing
// feedback: "each rule will have 0-2 params and they'll be different per
// rule"). A flat named bag rather than a typed manifest — same shape as the
// existing host-parameter BridgeParams — since only the plugin UI needs to
// know which keys a given rule chooses to expose, with what label/range;
// the engine just reads named values with defaults that reproduce today's
// hardcoded constants when a key is absent, so an empty/missing params bag
// is fully backward compatible.
type RuleParams = Record<string, number>;

interface EvolutionRuleInput {
  id: string;
  mutation: number;
  embellish: number;
  fill: number;
  hold: number;
  // Pulls the groove back toward the section's seed rather than away from
  // it — every other outcome only ever pushes the groove further from
  // where it started, so a tree with no settle weight can only drift, never
  // return. Defaults to 0 (existing rules are unaffected until authored
  // with a nonzero weight).
  settle: number;
  params?: RuleParams;
}

// A section's own automatic-evolution schedule (DAW testing feedback:
// arranging sections should let "each of them evolving independently" —
// this is what makes that literally true: every section carries its own
// schedule instead of one global schedule that only ever describes
// whichever section happened to be active). `nextEvolutionBar` is an
// absolute host bar number, not a relative countdown, so a transport seek
// only requires resetting it, not re-deriving relative state.
interface AutoEvolutionState {
  running: boolean;
  frequencyBars: number;
  nextEvolutionBar: number;
}

function makeAutoEvolutionState(): AutoEvolutionState {
  return { running: false, frequencyBars: 4, nextEvolutionBar: 0 };
}

// A section's weighted pool of enabled rules (DAW testing feedback: "the
// library should have a selector tick for each rule that opts it in or out
// for evolutions for each tree, and then there needs to be a list of
// enabled rules in the rule controller that allows setting weights for how
// often they occur"). Both manual evolveFromPool() and automatic
// tickAutoEvolution() roll a weighted choice from this per-section list
// rather than always applying one fixed rule — over many generations, a
// higher-frequency rule fires more often, exactly matching the request.
// Kept separate from evolveWithRule(rule, branch), which still applies one
// exact rule with no rolling — used internally by the pool roll, and left
// callable directly since a fixed, deterministic single-rule evolution is
// still a meaningful, testable operation in its own right.
interface RulePoolEntry {
  rule: EvolutionRuleInput;
  frequency: number;
}

// --- Persistent session state: independent named sections ----------------
// This module is loaded once and stays resident in the plugin's QuickJS
// context for its whole lifetime, so ordinary module-level state here IS
// the plugin's session memory across processBlock calls — the bridge is no
// longer purely stateless per block.
//
// A "section" (DAW testing feedback: "I'd kind of prefer to have A/B/etc
// sections that don't depend on each other") is a genuinely independent
// LineageTree with its own head and its own live-capture state. This is
// deliberately not the same thing as BRANCH (which creates a sibling off
// the current head's *parent* — still shared ancestry): sections share no
// history with each other at all, only the plugin instance that holds
// them. Playback renders whichever section the arrangement resolves for
// the current bar (or the active section if no arrangement is set); every
// section keeps evolving on its own schedule regardless of which one is
// currently audible.
interface Section {
  id: string;
  name: string;
  tree: LineageTree;
  headNodeId: string;
  capturingBar: number | null;
  capturedNotes: NoteEvent[];
  capturedBeatsPerBar: number;
  ruleGenerationCounter: number;
  autoEvolution: AutoEvolutionState;
  // Starts empty — rule *definitions* live entirely on the UI side (this
  // engine has no built-in notion of "Pocket Keeper"), so a freshly
  // created section has nothing to roll from until the UI pushes an
  // initial pool via setRulePool(), the same way it already pushes an
  // initial selected rule into a fresh RuleControllerPanel today.
  rulePool: RulePoolEntry[];
}

function makeDefaultGroove(): Groove {
  return createGroove({
    name: "session",
    tempo: 120,
    referenceBarLengthBeats: 4,
    lanes: [createLane({ type: "other", outputMapping: { note: 0, channel: 1 }, loopLengthBars: 1, notes: [] })],
  });
}

/** A, B, C, … Z, then falls back to a numbered label — plenty for a personal tool's realistic section count. */
function sectionNameForIndex(index: number): string {
  return index < 26 ? String.fromCharCode(65 + index) : `Section ${index + 1}`;
}

const sections = new Map<string, Section>();
let activeSectionId = "";
let sectionCounter = 0;

// Ordered, looping sequence of (sectionId, bars) blocks — DAW testing
// feedback: "3 bars of groove and 1 with a bit more busyness, another
// three groove, and a fill". Empty means "no arrangement": every playback
// path falls back to always rendering the active section, exactly as
// before this existed.
interface ArrangementBlock {
  sectionId: string;
  bars: number;
}

let arrangement: ArrangementBlock[] = [];

function setArrangement(blocks: ArrangementBlock[]): void {
  arrangement = blocks
    .filter((block) => sections.has(block.sectionId) && block.bars > 0)
    .map((block) => ({ sectionId: block.sectionId, bars: Math.max(1, Math.round(block.bars)) }));
}

function getArrangement(): ArrangementBlock[] {
  return arrangement.map((block) => ({ ...block }));
}

/** Which section id should be audible for an absolute host bar, cycling the arrangement. Null = no arrangement configured. */
function resolveArrangementSectionId(bar: number): string | null {
  if (arrangement.length === 0) return null;
  const totalBars = arrangement.reduce((sum, block) => sum + block.bars, 0);
  if (totalBars <= 0) return null;
  const wrapped = ((Math.floor(bar) % totalBars) + totalBars) % totalBars;
  let cursor = 0;
  for (const block of arrangement) {
    if (wrapped < cursor + block.bars) return block.sectionId;
    cursor += block.bars;
  }
  return arrangement[arrangement.length - 1]!.sectionId;
}

/** The section that should actually be rendered for a given bar: the arrangement's resolved section if one is configured and still exists, else the active section. */
function sectionForBar(bar: number): Section {
  const arrangedId = resolveArrangementSectionId(bar);
  const resolved = arrangedId ? sections.get(arrangedId) : undefined;
  return resolved ?? getActiveSection();
}

function createSection(): { id: string; name: string } {
  const index = sectionCounter;
  sectionCounter += 1;
  const id = `section-${index}`;
  const name = sectionNameForIndex(index);
  const tree = new LineageTree(makeDefaultGroove());
  sections.set(id, {
    id,
    name,
    tree,
    headNodeId: tree.rootId,
    capturingBar: null,
    capturedNotes: [],
    capturedBeatsPerBar: 4,
    ruleGenerationCounter: 0,
    autoEvolution: makeAutoEvolutionState(),
    rulePool: [],
  });
  activeSectionId = id;
  return { id, name };
}

function listSections(): Array<{ id: string; name: string; active: boolean }> {
  return Array.from(sections.values()).map((section) => ({
    id: section.id,
    name: section.name,
    active: section.id === activeSectionId,
  }));
}

function selectSection(id: string): void {
  if (!sections.has(id)) throw new Error(`unknown section: ${id}`);
  activeSectionId = id;
}

function deleteSection(id: string): void {
  if (sections.size <= 1) throw new Error("cannot delete the last remaining section");
  if (!sections.has(id)) throw new Error(`unknown section: ${id}`);
  sections.delete(id);
  if (activeSectionId === id) {
    activeSectionId = sections.keys().next().value as string;
  }
  arrangement = arrangement.filter((block) => block.sectionId !== id);
}

function getActiveSection(): Section {
  const section = sections.get(activeSectionId);
  if (!section) throw new Error("no active section");
  return section;
}

createSection();

interface SeedLane {
  id: string;
  name: string;
  midiNote: number;
  group: string;
  velocity: number;
  activeSteps: number[];
}

function laneTypeForMidiNote(note: number): LaneType {
  if (note === 35 || note === 36) return "kick";
  if (note === 37 || note === 38 || note === 39 || note === 40) return "snare";
  if (note === 42 || note === 44 || note === 46) return "hihat";
  if (note >= 41 && note <= 50) return "tom";
  if (note === 51 || note === 53 || note === 59) return "ride";
  if (note === 49 || note === 52 || note === 55 || note === 57) return "crash";
  return "other";
}

/**
 * Replaces the active section's history with a fresh tree rooted at a
 * groove authored visually (the plugin's step-sequencer editor) — a real
 * starting point instead of an empty pattern. This is a hard reset of the
 * active section only, not a branch and not a cross-section operation:
 * it's "program this section's starting groove," not "add a generation."
 * Anything captured from live play into this section before this point is
 * discarded from it (not from the DAW's own undo history), and other
 * sections are untouched.
 */
function setSeedGroove(seedLanes: SeedLane[], stepsPerBar: number, beatsPerBar: number): void {
  const beatsPerStep = stepsPerBar > 0 ? beatsPerBar / stepsPerBar : 0.25;
  const lanes = seedLanes.map((seedLane) => {
    const midiNote = Math.max(0, Math.min(127, Math.round(seedLane.midiNote)));
    const lane = createLane({
      type: laneTypeForMidiNote(midiNote),
      label: seedLane.name.trim() || `MIDI ${midiNote}`,
      groupId: seedLane.group.trim() || undefined,
      outputMapping: { note: midiNote, channel: 1 },
      loopLengthBars: 1,
      notes: seedLane.activeSteps
        .filter((step) => step >= 0 && step < stepsPerBar)
        .map((step) => ({
          id: nextNoteId(),
          position: step * beatsPerStep,
          pitch: midiNote,
          velocity: Math.max(1, Math.min(127, Math.round(seedLane.velocity))),
          duration: beatsPerStep,
        })),
    });
    return seedLane.id.trim() ? { ...lane, id: seedLane.id.trim() } : lane;
  });

  const seedGroove = createGroove({ name: "seed", tempo: 120, referenceBarLengthBeats: beatsPerBar, lanes });
  const section = getActiveSection();
  section.tree = new LineageTree(seedGroove);
  section.headNodeId = section.tree.rootId;
  section.capturingBar = null;
  section.capturedNotes = [];
  section.ruleGenerationCounter = 0;
  // A fresh seed is a hard reset of this section's history — any running
  // schedule was tuned against the content that just got replaced, and its
  // nextEvolutionBar would otherwise reference a tree that no longer
  // exists. Pausing (not just rescheduling) matches setSeedGroove's own
  // "program a starting groove" framing: auto-evolution is something you
  // opt back into deliberately once you like the new seed, not something
  // that silently keeps running through it.
  section.autoEvolution = makeAutoEvolutionState();
}

// --- Mined vocabulary (tools/midi-analysis) -------------------------------
// Optional, session-lifetime style influence: when loaded, the rule
// engine's "mutation" operation (see applyRuleGeneration below) samples
// per-voice/per-position timing and velocity variation from real
// performance statistics instead of a single flat hardcoded amount.
// Independent of section/seed state — loading a vocabulary doesn't touch
// any lineage tree, it only changes how future mutations behave, for
// whichever section they run against.
let loadedVocabulary: Vocabulary | null = null;

function setVocabulary(json: string): boolean {
  loadedVocabulary = parseVocabulary(JSON.parse(json));
  return true;
}

function clearVocabulary(): void {
  loadedVocabulary = null;
}

type RuleOperation = "mutation" | "embellish" | "fill" | "hold" | "settle";

function chooseRuleOperation(rule: EvolutionRuleInput, rng: () => number): RuleOperation {
  const weights: Array<[RuleOperation, number]> = [
    ["mutation", Math.max(0, rule.mutation)],
    ["embellish", Math.max(0, rule.embellish)],
    ["fill", Math.max(0, rule.fill)],
    ["hold", Math.max(0, rule.hold)],
    ["settle", Math.max(0, rule.settle)],
  ];
  const total = weights.reduce((sum, [, weight]) => sum + weight, 0);
  if (total <= 0) return "hold";
  let roll = rng() * total;
  for (const [operation, weight] of weights) {
    roll -= weight;
    if (roll <= 0) return operation;
  }
  return "hold";
}

/**
 * Pulls `source` back toward `seedGroove` (the section's root) by `strength`
 * (0-1). For each note that has a close match in the same lane's seed notes,
 * nudges its position/velocity a `strength` fraction of the way toward that
 * seed note; a note with no seed counterpart — a prior embellish/fill
 * addition — is probabilistically dropped instead, since "pulling back
 * toward the seed" means undoing additions, not just softening them.
 * Matches lanes by id, which stays stable across generations (mutations
 * only ever rewrite a lane's notes, never its id).
 *
 * The match threshold is deliberately tight (0.1 beats) rather than "half a
 * beat": a ghost note sits only 0.125 beats before the real hit it
 * ornaments (applyRuleGeneration's fixed embellish offset), so a looser
 * threshold would treat that ghost as "matching" its neighbor's seed note
 * instead of recognizing it as an unmatched addition to remove. Ordinary
 * timing humanization/vocabulary-driven movement is well under this (a few
 * hundredths of a beat at most), so genuinely-humanized notes still match.
 */
function applySettle(source: Groove, seedGroove: Groove, strength: number, rng: () => number): Groove {
  const result = cloneGroove(source);
  const seedLaneById = new Map(seedGroove.lanes.map((lane) => [lane.id, lane]));
  const matchThresholdBeats = 0.1;

  for (const lane of result.lanes) {
    const seedLane = seedLaneById.get(lane.id);
    if (!seedLane) continue;

    const kept: NoteEvent[] = [];
    for (const note of lane.notes) {
      let nearest: NoteEvent | undefined;
      let nearestDistance = Infinity;
      for (const seedNote of seedLane.notes) {
        const distance = Math.abs(note.position - seedNote.position);
        if (distance < nearestDistance) {
          nearestDistance = distance;
          nearest = seedNote;
        }
      }

      if (nearest && nearestDistance <= matchThresholdBeats) {
        if (rng() < strength) {
          kept.push({
            ...note,
            position: note.position + (nearest.position - note.position) * strength,
            velocity: Math.max(1, Math.min(127, Math.round(note.velocity + (nearest.velocity - note.velocity) * strength))),
          });
        } else {
          kept.push(note);
        }
      } else if (rng() >= strength) {
        kept.push(note);
      }
    }
    lane.notes = kept.sort((left, right) => left.position - right.position);
  }

  return result;
}

function applyBasicFill(source: Groove, peakVelocity: number): Groove {
  const groove = cloneGroove(source);
  const target = groove.lanes.find((lane) => lane.type === "snare")
    ?? groove.lanes.find((lane) => lane.type === "tom")
    ?? groove.lanes.find((lane) => lane.type !== "kick");
  if (!target) return groove;

  const barLength = Math.max(1, groove.referenceBarLengthBeats);
  const start = Math.max(0, barLength - 1);
  const pitch = target.outputMapping.note;
  // Same 4-hit crescendo shape as always; peakVelocity (rule-tunable) scales
  // it proportionally instead of using the fixed [72, 84, 96, 112] array.
  const clampedPeak = Math.max(1, Math.min(127, peakVelocity));
  const shape = [0.643, 0.75, 0.857, 1.0];
  for (let step = 0; step < 4; step += 1) {
    const position = start + step * 0.25;
    if (!target.notes.some((note) => Math.abs(note.position - position) < 1 / 960)) {
      const velocity = Math.max(1, Math.min(127, Math.round(clampedPeak * shape[step]!)));
      target.notes.push({id: nextNoteId(), position, pitch, velocity, duration: 0.2});
    }
  }
  target.notes.sort((left, right) => left.position - right.position);
  return groove;
}

function applyRuleGeneration(
  source: Groove,
  rule: EvolutionRuleInput,
  generation: number,
  seedGroove: Groove
): {
  groove: Groove;
  operation: RuleOperation;
  seed: number;
  weights: {mutation: number; embellish: number; fill: number; hold: number; settle: number};
} {
  const seed = (hashString(rule.id) ^ Math.imul(generation, 0x9e3779b1)) >>> 0;
  const operation = chooseRuleOperation(rule, createRng(seed));
  const params = rule.params ?? {};
  let result = cloneGroove(source);

  if (operation === "mutation") {
    // With a mined vocabulary loaded, style each note per its own lane's
    // voice and bar position from real performance statistics; otherwise
    // fall back to flat humanization at the rule's own tunable amount
    // (default 18 reproduces the original hardcoded constant).
    result = loadedVocabulary
      ? applyVocabularyStyle(source, source.lanes.map((lane) => lane.id), loadedVocabulary, seed)
      : applyMutation(
          source,
          "velocityHumanize",
          {laneIds: source.lanes.map((lane) => lane.id), barRange: {start: 0, end: 1}},
          {probability: 1, amount: params.mutationAmount ?? 18},
          seed
        );
  } else if (operation === "embellish") {
    const preferred = source.lanes.filter((lane) => lane.type === "snare" || lane.type === "hihat");
    const laneIds = (preferred.length > 0 ? preferred : source.lanes).map((lane) => lane.id);
    result = applyMutation(
      source,
      "ghostNote",
      {laneIds, barRange: {start: 0, end: 1}},
      {
        probability: params.embellishProbability ?? 0.42,
        ghostVelocity: params.ghostVelocity ?? 34,
        offsetBeats: 0.125,
      },
      seed
    );
  } else if (operation === "fill") {
    result = applyBasicFill(source, params.fillPeakVelocity ?? 112);
  } else if (operation === "settle") {
    const strength = Math.max(0, Math.min(1, params.settleStrength ?? 0.5));
    result = applySettle(source, seedGroove, strength, createRng(seed));
  }

  result = {...result, name: `${source.name} · ${rule.id} ${generation}`};
  return {
    groove: result,
    operation,
    seed,
    weights: {
      mutation: Math.max(0, rule.mutation),
      embellish: Math.max(0, rule.embellish),
      fill: Math.max(0, rule.fill),
      hold: Math.max(0, rule.hold),
      settle: Math.max(0, rule.settle),
    },
  };
}

/** Shared body for evolving one explicit section (manual EVOLVE/BRANCH and automatic ticking both funnel through this). */
function evolveSectionWithRule(section: Section, rule: EvolutionRuleInput, branch: boolean): {
  nodeId: string;
  parentId: string;
  operation: RuleOperation;
} {
  const current = section.tree.getNode(section.headNodeId);
  const parent = branch && current.parentId !== null ? section.tree.getNode(current.parentId) : current;
  const seedGroove = section.tree.getNode(section.tree.rootId).groove;
  section.ruleGenerationCounter += 1;
  const evolved = applyRuleGeneration(parent.groove, rule, section.ruleGenerationCounter, seedGroove);
  const node = section.tree.addChild(parent.id, evolved.groove, {
    type: "rule",
    ruleId: rule.id,
    operation: evolved.operation,
    seed: evolved.seed,
    weights: evolved.weights,
  });
  section.headNodeId = node.id;
  return {nodeId: node.id, parentId: parent.id, operation: evolved.operation};
}

function evolveWithRule(rule: EvolutionRuleInput, branch: boolean): {
  nodeId: string;
  parentId: string;
  operation: RuleOperation;
} {
  return evolveSectionWithRule(getActiveSection(), rule, branch);
}

function setRulePool(entries: RulePoolEntry[]): void {
  getActiveSection().rulePool = entries.map((entry) => ({rule: entry.rule, frequency: Math.max(0, entry.frequency)}));
}

function getRulePool(): RulePoolEntry[] {
  return getActiveSection().rulePool.map((entry) => ({rule: entry.rule, frequency: entry.frequency}));
}

/** Weighted pick from a rule pool; null if the pool is empty or every entry has zero/negative frequency. */
function chooseFromPool(pool: RulePoolEntry[], rng: () => number): EvolutionRuleInput | null {
  const total = pool.reduce((sum, entry) => sum + Math.max(0, entry.frequency), 0);
  if (total <= 0) return null;
  let roll = rng() * total;
  for (const entry of pool) {
    const weight = Math.max(0, entry.frequency);
    roll -= weight;
    if (roll <= 0) return entry.rule;
  }
  return pool[pool.length - 1]!.rule;
}

/** Deterministic per-generation seed for "which rule in the pool fires", independent of that rule's own applyRuleGeneration seed. */
function poolRollSeed(sectionId: string, generation: number): number {
  return (hashString(sectionId) ^ Math.imul(generation, 0x27220a95)) >>> 0;
}

/**
 * Rolls a weighted choice from `section`'s own rule pool and evolves with
 * whichever rule was chosen (DAW testing feedback: enabled rules should
 * have "weights for how often they occur"). Returns null (no-op, no tree
 * node created) if the pool is empty or has no positive-frequency
 * entries; the UI is expected to keep at least one rule enabled, but this
 * stays a safe no-op rather than throwing if that invariant is ever
 * violated. Takes an explicit section — not just "the active one" — so
 * tickAutoEvolution() can use it for a background section too.
 */
function evolveSectionFromPool(section: Section, branch: boolean): {
  nodeId: string;
  parentId: string;
  operation: RuleOperation;
  ruleId: string;
} | null {
  const seed = poolRollSeed(section.id, section.ruleGenerationCounter + 1);
  const chosenRule = chooseFromPool(section.rulePool, createRng(seed));
  if (!chosenRule) return null;
  const result = evolveSectionWithRule(section, chosenRule, branch);
  return {...result, ruleId: chosenRule.id};
}

/** What the UI's EVOLVE/BRANCH buttons call — evolveSectionFromPool() on whichever section is currently active. */
function evolveFromPool(branch: boolean): {
  nodeId: string;
  parentId: string;
  operation: RuleOperation;
  ruleId: string;
} | null {
  return evolveSectionFromPool(getActiveSection(), branch);
}

/**
 * Configures the active section's own automatic-evolution schedule
 * (DAW-controlled START/PAUSE + frequency). Which rule fires each time is
 * no longer part of this config — it's rolled from the section's rule pool
 * (setRulePool()) at each due generation, same as manual evolveFromPool().
 * `currentBar` is the host's current absolute bar, supplied by the C++
 * side so a schedule change can be anchored to "now" without this module
 * needing its own transport awareness. Only resets `nextEvolutionBar` when
 * running/frequency actually changed, mirroring the previous C++-side
 * behavior — nudging an unrelated parameter shouldn't restart the count.
 */
function configureAutoEvolution(running: boolean, frequencyBars: number, currentBar: number): void {
  const section = getActiveSection();
  const safeFrequency = Math.max(1, Math.min(64, Math.round(frequencyBars)));
  const scheduleChanged = section.autoEvolution.running !== running
    || section.autoEvolution.frequencyBars !== safeFrequency;
  section.autoEvolution.running = running;
  section.autoEvolution.frequencyBars = safeFrequency;
  if (scheduleChanged) {
    section.autoEvolution.nextEvolutionBar = Math.floor(currentBar) + safeFrequency;
  }
}

/**
 * Realigns every running section's schedule to "next due `frequencyBars`
 * bars from now" — called when the host transport starts, seeks, or loops,
 * so a schedule computed against a stale bar number doesn't fire early/late
 * or (after a big jump) fire a burst of catch-up generations.
 */
function resetAutoEvolutionSchedules(currentBar: number): void {
  for (const section of sections.values()) {
    if (!section.autoEvolution.running) continue;
    section.autoEvolution.nextEvolutionBar = Math.floor(currentBar) + section.autoEvolution.frequencyBars;
  }
}

/**
 * Called once per detected host bar change while the transport is playing.
 * Evolves every section whose own schedule is due — independent of which
 * section is currently audible, which is what makes background sections in
 * an arrangement actually keep evolving on their own (DAW testing
 * feedback: "each of them evolving independently"). One generation per due
 * section per call, not a catch-up loop; a section left behind by a big
 * transport jump gets exactly one generation and its schedule re-anchored
 * to `currentBar`, rather than firing everything it missed at once.
 */
function tickAutoEvolution(currentBar: number): Array<{
  sectionId: string;
  sectionName: string;
  ruleId: string;
  operation: RuleOperation;
}> {
  const bar = Math.floor(currentBar);
  const fired: Array<{sectionId: string; sectionName: string; ruleId: string; operation: RuleOperation}> = [];
  for (const section of sections.values()) {
    const auto = section.autoEvolution;
    if (!auto.running || bar < auto.nextEvolutionBar) continue;
    const evolved = evolveSectionFromPool(section, false);
    auto.nextEvolutionBar = bar + auto.frequencyBars;
    if (evolved) {
      fired.push({sectionId: section.id, sectionName: section.name, ruleId: evolved.ruleId, operation: evolved.operation});
    }
  }
  return fired;
}

function commitCapturedBar(section: Section): void {
  if (section.capturedNotes.length === 0) return;
  const lane = createLane({
    type: "other",
    outputMapping: { note: 0, channel: 1 },
    loopLengthBars: 1,
    notes: section.capturedNotes,
  });
  const groove = createGroove({
    name: `bar ${section.capturingBar ?? 0}`,
    tempo: 120,
    referenceBarLengthBeats: section.capturedBeatsPerBar,
    lanes: [lane],
  });
  const node = section.tree.addChild(section.headNodeId, groove, { type: "recorded", capturedAtMs: Date.now() });
  section.headNodeId = node.id;
  section.capturedNotes = [];
}

function captureEvents(section: Section, events: BridgeNoteEvent[], beatsPerBar: number): void {
  for (const event of events) {
    const bar = Math.floor(event.beatPosition / beatsPerBar);
    if (section.capturingBar !== null && bar !== section.capturingBar) commitCapturedBar(section);
    section.capturingBar = bar;
    section.capturedBeatsPerBar = beatsPerBar;
    section.capturedNotes.push({
      id: nextNoteId(),
      position: wrapToBar(event.beatPosition, bar, beatsPerBar),
      pitch: event.note,
      velocity: event.velocity,
      duration: 0.25,
    });
  }
}

/** Host-side introspection — not part of live processing, just makes session persistence observable/testable. */
function getSessionInfo(): {
  nodeCount: number;
  headNodeId: string;
  rootNoteCount: number;
  rootLaneCount: number;
  groupedLaneCount: number;
  sectionId: string;
  sectionName: string;
} {
  const section = getActiveSection();
  const root = section.tree.getNode(section.tree.rootId);
  const rootNoteCount = root.groove.lanes.reduce((sum, lane) => sum + lane.notes.length, 0);
  const groupedLaneCount = root.groove.lanes.filter((lane) => lane.groupId !== undefined).length;
  return {
    nodeCount: section.tree.toJSON().nodes.length,
    headNodeId: section.headNodeId,
    rootNoteCount,
    rootLaneCount: root.groove.lanes.length,
    groupedLaneCount,
    sectionId: section.id,
    sectionName: section.name,
  };
}

// --- Live block processing ------------------------------------------------

function beatToSamplePosition(beat: number, transport: BridgeTransport): number {
  const secondsPerBeat = transport.tempo > 0 ? 60 / transport.tempo : 0.5;
  return Math.round((beat - transport.blockStartBeat) * secondsPerBeat * transport.sampleRate);
}

const previewFlagGhost = 1;
const previewFlagHumanized = 2;
const previewFlagEvolved = 4;
const previewFlagScheduledEvolution = 8;

function hashString(value: string): number {
  let hash = 2166136261;
  for (let index = 0; index < value.length; index += 1) {
    hash ^= value.charCodeAt(index);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function eventSeed(laneId: string, absoluteBeat: number, salt: number): number {
  const tick = Math.round(absoluteBeat * 960);
  return (hashString(laneId) ^ Math.imul(tick, 0x45d9f3b) ^ salt) >>> 0;
}

/**
 * Produces the finalized, deterministic MIDI plan for an arbitrary host-
 * beat range against one explicit groove — pure, no section/arrangement
 * lookup of its own. `planArrangedRange` below (the arrangement-aware
 * wrapper) and the auto-evolution look-ahead simulation are the two
 * callers; both resolve which groove applies to which sub-range themselves
 * so this stays a single shared implementation of the actual note-planning
 * math (humanize, ghosts, meter scaling) that can never disagree between
 * what's drawn and what the DAW receives.
 */
function planPlaybackRangeForGroove(
  startBeat: number,
  endBeat: number,
  hostBeatsPerBar: number,
  params: BridgeParams,
  groove: Groove,
  forceEvolved: boolean,
  extraPreviewFlags: number
): PlaybackNoteEvent[] {
  if (endBeat <= startBeat || hostBeatsPerBar <= 0) return [];

  const sourceBeatsPerBar = groove.referenceBarLengthBeats > 0
    ? groove.referenceBarLengthBeats
    : hostBeatsPerBar;
  const meterScale = hostBeatsPerBar / sourceBeatsPerBar;
  const humanizeAmount = Math.max(0, Math.min(40, params.humanizeAmount ?? 12));
  // Timing is opt-in (DAW testing feedback: "humanization should probably
  // have timing") rather than baked unconditionally into the existing
  // amount knob, so a host that never enables it keeps byte-identical
  // positions to before this existed — the knob's magnitude still governs
  // how far notes move, just gated by this separate toggle.
  const timingHumanizeEnabled = (params.humanizeTimingEnabled ?? 0) >= 1;
  const ghostEnabled = (params.ghostNoteEnabled ?? 0) >= 1;
  const ghostProbability = Math.max(0, Math.min(1, params.ghostNoteProbability ?? 0.25));
  const evolvedFlag = forceEvolved ? previewFlagEvolved : 0;
  const events: PlaybackNoteEvent[] = [];

  for (const lane of groove.lanes) {
    const loopLengthBeats = lane.loopLengthBars * hostBeatsPerBar;
    if (loopLengthBeats <= 0) continue;

    // Look one ghost-note offset beyond the requested range so a ghost just
    // inside the range is retained even when its parent hit is just outside.
    const ghostOffsetBeats = 0.125 * meterScale;
    const firstCycle = Math.floor(startBeat / loopLengthBeats);
    const lastCycle = Math.floor((endBeat + ghostOffsetBeats - Number.EPSILON) / loopLengthBeats);

    for (let cycle = firstCycle; cycle <= lastCycle; cycle += 1) {
      const cycleStartBeat = cycle * loopLengthBeats;
      for (const note of lane.notes) {
        const scaledPosition = note.position * meterScale;
        const localBeat = ((scaledPosition % loopLengthBeats) + loopLengthBeats) % loopLengthBeats;
        const absoluteBeat = cycleStartBeat + localBeat;
        const humanizeRng = createRng(eventSeed(lane.id, absoluteBeat, 0x6d2b79f5));
        const delta = humanizeAmount > 0 ? Math.round((humanizeRng() * 2 - 1) * humanizeAmount) : 0;
        const velocity = Math.max(1, Math.min(127, Math.round(note.velocity + delta)));

        // Small deterministic timing nudge, seeded independently of the
        // velocity RNG above but from the same lane+beat identity so it
        // reproduces identically between this preview and the later audio
        // block. Scaled by the same amount knob (0-40) so one control
        // governs overall humanized "feel". Capped well inside the
        // ghost-note lookahead window below, and deliberately left out of
        // the startBeat/endBeat membership test that follows (which still
        // uses the unshifted absoluteBeat) so a jittered note can never be
        // dropped at a block boundary — worst case it renders up to one
        // block early/late relative to its unjittered grid time, the same
        // class of boundary simplification already accepted for ghosts.
        const timingRng = createRng(eventSeed(lane.id, absoluteBeat, 0x27d4eb2f));
        const maxTimingJitterBeats = 0.03 * meterScale;
        const timingDelta = timingHumanizeEnabled && humanizeAmount > 0
          ? (timingRng() * 2 - 1) * maxTimingJitterBeats * (humanizeAmount / 40)
          : 0;
        const commonFlags = evolvedFlag | extraPreviewFlags
          | (delta !== 0 || timingDelta !== 0 ? previewFlagHumanized : 0);

        if (absoluteBeat >= startBeat && absoluteBeat < endBeat) {
          events.push({
            note: Math.max(0, Math.min(127, Math.round(note.pitch))),
            velocity,
            channel: Math.max(1, Math.min(16, Math.round(lane.outputMapping.channel))),
            samplePosition: 0,
            beatPosition: absoluteBeat + timingDelta,
            durationBeats: Math.max(1 / 960, note.duration * meterScale),
            previewFlags: commonFlags,
          });
        }

        if (ghostEnabled) {
          const ghostRng = createRng(eventSeed(lane.id, absoluteBeat, 0x1b873593));
          if (ghostRng() < ghostProbability) {
            const ghostBeat = Math.max(cycleStartBeat, absoluteBeat - ghostOffsetBeats);
            if (ghostBeat >= startBeat && ghostBeat < endBeat) {
              events.push({
                note: Math.max(0, Math.min(127, Math.round(note.pitch))),
                velocity: 30,
                channel: Math.max(1, Math.min(16, Math.round(lane.outputMapping.channel))),
                samplePosition: 0,
                beatPosition: ghostBeat,
                durationBeats: Math.max(1 / 960, Math.min(ghostOffsetBeats, note.duration * meterScale)),
                previewFlags: commonFlags | previewFlagGhost,
              });
            }
          }
        }
      }
    }
  }

  return events;
}

function isEvolvedProvenance(provenance: {type: string} | null | undefined): boolean {
  return provenance?.type === "mutation" || provenance?.type === "liveSession" || provenance?.type === "rule";
}

/**
 * Arrangement-aware wrapper around planPlaybackRangeForGroove: splits the
 * requested range at every bar boundary where the resolved section changes
 * (or, with no arrangement configured, doesn't split at all — a single
 * call against the active section, exactly the pre-arrangement behavior),
 * and renders each segment from that section's own *current, committed*
 * head. Used for real audio output and for the default (non-look-ahead)
 * preview; the auto-evolution look-ahead simulation below additionally
 * simulates *future* generations on top of this same per-bar resolution.
 */
function planArrangedRange(
  startBeat: number,
  endBeat: number,
  hostBeatsPerBar: number,
  params: BridgeParams
): PlaybackNoteEvent[] {
  if (endBeat <= startBeat || hostBeatsPerBar <= 0) return [];

  const events: PlaybackNoteEvent[] = [];
  let cursor = startBeat;
  while (cursor < endBeat - 1.0e-9) {
    const bar = Math.floor(cursor / hostBeatsPerBar);
    const barEndBeat = (bar + 1) * hostBeatsPerBar;
    const segmentEnd = Math.min(endBeat, barEndBeat);
    const section = sectionForBar(bar);
    const headNode = section.tree.getNode(section.headNodeId);
    events.push(...planPlaybackRangeForGroove(
      cursor, segmentEnd, hostBeatsPerBar, params, headNode.groove, isEvolvedProvenance(headNode.provenance), 0
    ));
    cursor = segmentEnd;
  }

  return events.sort((left, right) =>
    left.beatPosition - right.beatPosition || left.channel - right.channel || left.note - right.note
  );
}

/**
 * Renders whichever section the arrangement resolves for this block's bar
 * (or the active section with no arrangement configured) into one host
 * process block. Each lane keeps its own loop length, while note positions
 * are scaled from the groove's reference meter to the host's current
 * meter. The half-open block range means an event on a block boundary is
 * emitted exactly once, by the block that starts there.
 */
function renderPlaybackBlock(
  transport: BridgeTransport,
  blockSizeSamples: number,
  params: BridgeParams
): PlaybackNoteEvent[] {
  if (transport.tempo <= 0 || transport.sampleRate <= 0 || blockSizeSamples <= 0) return [];

  const hostBeatsPerBar = transport.beatsPerBar > 0 ? transport.beatsPerBar : 4;
  const blockEndBeat = transport.blockStartBeat +
    (blockSizeSamples / transport.sampleRate) * (transport.tempo / 60);
  const events = planArrangedRange(transport.blockStartBeat, blockEndBeat, hostBeatsPerBar, params);
  for (const event of events) {
    const rawSamplePosition = beatToSamplePosition(event.beatPosition, transport);
    event.samplePosition = Math.max(0, Math.min(blockSizeSamples - 1, rawSamplePosition));
  }
  return events.sort((left, right) =>
    left.samplePosition - right.samplePosition || left.channel - right.channel || left.note - right.note
  );
}

/**
 * The "current + next" look-ahead: for every bar in the requested horizon,
 * resolves which section the arrangement puts there (DAW testing feedback:
 * arranging sections "will make current + next more useful too" — this is
 * what makes each bar show its real upcoming section instead of one
 * repeating loop), and *simulates* that section's own auto-evolution
 * schedule forward — independently per section, never committing a node —
 * so a background section's next scheduled generation is visible before it
 * actually fires. Purely a function of already-committed state (each
 * section's real head + real schedule); nothing here mutates a tree.
 */
function renderPlaybackPreview(
  startBeat: number,
  beatsPerBar: number,
  barCount: number,
  params: BridgeParams
): PlaybackNoteEvent[] {
  const safeBeatsPerBar = beatsPerBar > 0 ? beatsPerBar : 4;
  const safeBarCount = Math.max(1, Math.min(32, Math.round(barCount)));
  const endBeat = startBeat + safeBeatsPerBar * safeBarCount;
  if (endBeat <= startBeat) return [];

  interface SimState {
    groove: Groove;
    generation: number;
    nextEvolutionBar: number;
    evolved: boolean;
    aheadOfCommitted: boolean;
  }
  const simState = new Map<string, SimState>();
  const simFor = (section: Section): SimState => {
    let sim = simState.get(section.id);
    if (!sim) {
      const head = section.tree.getNode(section.headNodeId);
      sim = {
        groove: head.groove,
        generation: section.ruleGenerationCounter,
        nextEvolutionBar: section.autoEvolution.nextEvolutionBar,
        evolved: isEvolvedProvenance(head.provenance),
        aheadOfCommitted: false,
      };
      simState.set(section.id, sim);
    }
    return sim;
  };

  const events: PlaybackNoteEvent[] = [];
  let cursor = startBeat;
  while (cursor < endBeat - 1.0e-9) {
    const bar = Math.floor(cursor / safeBeatsPerBar);
    const barEndBeat = (bar + 1) * safeBeatsPerBar;
    const segmentEnd = Math.min(endBeat, barEndBeat);
    const section = sectionForBar(bar);
    const sim = simFor(section);

    while (section.autoEvolution.running && sim.nextEvolutionBar <= bar) {
      sim.generation += 1;
      // Same weighted-pool roll evolveSectionFromPool() will make for real
      // once this bar actually arrives, seeded identically (section id +
      // generation) so the preview never shows a different rule than the
      // one that ends up committing.
      const rolledRule = chooseFromPool(section.rulePool, createRng(poolRollSeed(section.id, sim.generation)));
      if (!rolledRule) break; // nothing enabled to roll — stop simulating forward, not an error
      const seedGroove = section.tree.getNode(section.tree.rootId).groove;
      sim.groove = applyRuleGeneration(sim.groove, rolledRule, sim.generation, seedGroove).groove;
      sim.evolved = true;
      sim.aheadOfCommitted = true;
      sim.nextEvolutionBar += section.autoEvolution.frequencyBars;
    }

    events.push(...planPlaybackRangeForGroove(
      cursor, segmentEnd, safeBeatsPerBar, params, sim.groove, sim.evolved,
      sim.aheadOfCommitted ? previewFlagScheduledEvolution : 0
    ));
    cursor = segmentEnd;
  }

  return events.sort((left, right) =>
    left.beatPosition - right.beatPosition || left.channel - right.channel || left.note - right.note
  );
}

/**
 * MVP bridge (§11): runs a block's incoming note-ons through the real
 * mutation pipeline — velocityHumanize always, ghostNote additionally when
 * host-enabled — and echoes the result back. A ghost note's computed
 * sample position can fall outside the current block (its offset may land
 * before this block started or after it ends); those are left for the
 * C++ side to drop rather than scheduled into a future/past block, since
 * there's no cross-block MIDI scheduler yet. Also captures played notes
 * into the active section's lineage tree above, independent of what gets
 * echoed back to the host.
 */
function processBlock(events: BridgeNoteEvent[], transport: BridgeTransport, params: BridgeParams): BridgeNoteEvent[] {
  if (events.length === 0) return events;

  const beatsPerBar = transport.beatsPerBar > 0 ? transport.beatsPerBar : 4;
  const currentBar = Math.floor(events[0]!.beatPosition / beatsPerBar);
  const channel = events[0]!.channel;

  captureEvents(getActiveSection(), events, beatsPerBar);

  const lane = createLane({
    type: "other",
    outputMapping: { note: 0, channel: 1 },
    loopLengthBars: 1,
    notes: events.map((event) => ({
      id: nextNoteId(),
      position: wrapToBar(event.beatPosition, currentBar, beatsPerBar),
      pitch: event.note,
      velocity: event.velocity,
      duration: 0.25,
    })),
  });
  const groove = createGroove({
    name: "block",
    tempo: transport.tempo > 0 ? transport.tempo : 120,
    referenceBarLengthBeats: beatsPerBar,
    lanes: [lane],
  });
  const target = { laneIds: [lane.id], barRange: { start: 0, end: 1 } };

  callCounter += 1;
  let result = applyMutation(
    groove,
    "velocityHumanize",
    target,
    { probability: 1, amount: params.humanizeAmount ?? 20 },
    callCounter
  );

  if ((params.ghostNoteEnabled ?? 0) >= 1) {
    callCounter += 1;
    result = applyMutation(result, "ghostNote", target, { probability: params.ghostNoteProbability ?? 0.25 }, callCounter);
  }

  return result.lanes[0]!.notes.map((note) => {
    const absoluteBeat = currentBar * beatsPerBar + note.position;
    return {
      note: note.pitch,
      velocity: note.velocity,
      channel,
      samplePosition: beatToSamplePosition(absoluteBeat, transport),
      beatPosition: absoluteBeat,
    };
  });
}

// --- Per-note evolution (DAW testing feedback: "below the seed editor we
// need a visualizer for whatever cell we've clicked on to see where it
// evolved to") -----------------------------------------------------------
// Traces one note — identified by its position in the active section's
// CURRENT seed (root node), re-resolved fresh on every call rather than a
// captured id with its own lifetime — across the head path only
// (tree.getBranch(headNodeId), root-first). Deliberately not every branch
// in the tree: this matches "see where it evolved to" as a single linear
// story, not a second tree diagram. A note that gets dropped along the
// way (e.g. settle removing an unmatched embellishment) simply reports
// present: false for every generation from that point on, rather than
// disappearing from the array — the caller can see exactly when/why.
interface NoteEvolutionEntry {
  nodeId: string;
  generation: number;
  operation: string;
  present: boolean;
  position: number | null;
  velocity: number | null;
}

function findNoteInGroove(
  groove: Groove,
  laneId: string,
  predicate: (note: NoteEvent) => boolean
): NoteEvent | undefined {
  const lane = groove.lanes.find((l) => l.id === laneId);
  return lane?.notes.find(predicate);
}

function describeProvenance(provenance: LineageNodeProvenance | null): string {
  if (!provenance) return "root";
  return provenance.type === "rule" ? provenance.operation : provenance.type;
}

function getNoteEvolutionForCell(laneId: string, positionBeats: number): NoteEvolutionEntry[] {
  const section = getActiveSection();
  const root = section.tree.getNode(section.tree.rootId);
  const seedNote = findNoteInGroove(root.groove, laneId, (n) => Math.abs(n.position - positionBeats) < 1e-6);
  if (!seedNote) return [];

  const branch = section.tree.getBranch(section.headNodeId);
  return branch.map((node, index) => {
    const match = findNoteInGroove(node.groove, laneId, (n) => n.id === seedNote.id);
    return {
      nodeId: node.id,
      generation: index,
      operation: describeProvenance(node.provenance),
      present: match !== undefined,
      position: match?.position ?? null,
      velocity: match?.velocity ?? null,
    };
  });
}

(globalThis as Record<string, unknown>).__lineageProcessBlock = (
  eventsIn: BridgeNoteEvent[],
  transportIn: BridgeTransport,
  paramsIn: BridgeParams
) => processBlock(eventsIn, transportIn, paramsIn);

(globalThis as Record<string, unknown>).__lineageGetSessionInfo = () => getSessionInfo();

(globalThis as Record<string, unknown>).__lineageSetSeedGroove = (
  lanesIn: SeedLane[],
  stepsPerBarIn: number,
  beatsPerBarIn: number
) => setSeedGroove(lanesIn, stepsPerBarIn, beatsPerBarIn);

(globalThis as Record<string, unknown>).__lineageRenderPlaybackBlock = (
  transportIn: BridgeTransport,
  blockSizeSamplesIn: number,
  paramsIn: BridgeParams
) => renderPlaybackBlock(transportIn, blockSizeSamplesIn, paramsIn);

(globalThis as Record<string, unknown>).__lineageRenderPlaybackPreview = (
  startBeatIn: number,
  beatsPerBarIn: number,
  barCountIn: number,
  paramsIn: BridgeParams
) => renderPlaybackPreview(startBeatIn, beatsPerBarIn, barCountIn, paramsIn);

(globalThis as Record<string, unknown>).__lineageEvolveWithRule = (
  ruleIn: EvolutionRuleInput,
  branchIn: boolean
) => evolveWithRule(ruleIn, branchIn);

(globalThis as Record<string, unknown>).__lineageSetVocabulary = (jsonIn: string) => setVocabulary(jsonIn);

(globalThis as Record<string, unknown>).__lineageClearVocabulary = () => clearVocabulary();

(globalThis as Record<string, unknown>).__lineageCreateSection = () => createSection();

(globalThis as Record<string, unknown>).__lineageListSections = () => listSections();

(globalThis as Record<string, unknown>).__lineageSelectSection = (idIn: string) => selectSection(idIn);

(globalThis as Record<string, unknown>).__lineageDeleteSection = (idIn: string) => deleteSection(idIn);

(globalThis as Record<string, unknown>).__lineageSetArrangement = (blocksIn: ArrangementBlock[]) => setArrangement(blocksIn);

(globalThis as Record<string, unknown>).__lineageGetArrangement = () => getArrangement();

(globalThis as Record<string, unknown>).__lineageConfigureAutoEvolution = (
  runningIn: boolean,
  frequencyBarsIn: number,
  currentBarIn: number
) => configureAutoEvolution(runningIn, frequencyBarsIn, currentBarIn);

(globalThis as Record<string, unknown>).__lineageResetAutoEvolutionSchedules = (currentBarIn: number) =>
  resetAutoEvolutionSchedules(currentBarIn);

(globalThis as Record<string, unknown>).__lineageTickAutoEvolution = (currentBarIn: number) =>
  tickAutoEvolution(currentBarIn);

(globalThis as Record<string, unknown>).__lineageSetRulePool = (entriesIn: RulePoolEntry[]) => setRulePool(entriesIn);

(globalThis as Record<string, unknown>).__lineageGetRulePool = () => getRulePool();

(globalThis as Record<string, unknown>).__lineageEvolveFromPool = (branchIn: boolean) => evolveFromPool(branchIn);

(globalThis as Record<string, unknown>).__lineageGetNoteEvolution = (laneIdIn: string, positionBeatsIn: number) =>
  getNoteEvolutionForCell(laneIdIn, positionBeatsIn);
