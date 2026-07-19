// Plugin runtime entry point (DESIGN.md §11) — bundled with esbuild into a
// single self-contained script and embedded in the JUCE plugin, executed
// inside QuickJS. Deliberately narrow: only the pure engine logic that has
// no Node dependency (no fs/path, so no persistence.ts/library.ts/
// jsonFileLibrary.ts/scriptMutations.ts — those are for host-side tooling,
// not the embedded runtime).
import { cloneGroove, createGroove, createLane } from "./groove.js";
import { applyMutation } from "./mutation.js";
import { registerBuiltinMutations } from "./mutations/index.js";
import { createRng } from "./rng.js";
import { LineageTree } from "./lineage.js";
import { parseVocabulary, type Vocabulary } from "./vocabulary.js";
import { applyVocabularyStyle } from "./vocabularyStyle.js";
import type { Groove, LaneType, NoteEvent } from "./types.js";

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
  /** Bitfield used by the compact preview: 1 ghost, 2 humanized, 4 evolved head. */
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

// --- Persistent session state --------------------------------------------
// This module is loaded once and stays resident in the plugin's QuickJS
// context for its whole lifetime, so ordinary module-level state here IS
// the plugin's session memory across processBlock calls — the bridge is no
// longer purely stateless per block. Captures what's actually played, bar
// by bar, into a real lineage tree (§3), so the plugin builds up a genuine
// history of a session as you play into it. This does not yet drive
// playback (looping/evolving a captured bar back out) — that's a separate,
// larger piece of work (real cross-block MIDI scheduling) left for later.
let sessionTree = new LineageTree(
  createGroove({
    name: "session",
    tempo: 120,
    referenceBarLengthBeats: 4,
    lanes: [createLane({ type: "other", outputMapping: { note: 0, channel: 1 }, loopLengthBars: 1, notes: [] })],
  })
);
let headNodeId = sessionTree.rootId;
let capturingBar: number | null = null;
let capturedNotes: NoteEvent[] = [];
let capturedBeatsPerBar = 4;
let ruleGenerationCounter = 0;

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
 * Replaces the session's history with a fresh tree rooted at a groove
 * authored visually (the plugin's step-sequencer editor) — a real starting
 * point instead of an empty pattern. This is a hard reset of the session,
 * not a branch: it's "program a starting groove," not "add a generation."
 * Anything captured from live play before this point is discarded from
 * the session (not from the DAW's own undo history).
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
          position: step * beatsPerStep,
          pitch: midiNote,
          velocity: Math.max(1, Math.min(127, Math.round(seedLane.velocity))),
          duration: beatsPerStep,
        })),
    });
    return seedLane.id.trim() ? { ...lane, id: seedLane.id.trim() } : lane;
  });

  const seedGroove = createGroove({ name: "seed", tempo: 120, referenceBarLengthBeats: beatsPerBar, lanes });
  sessionTree = new LineageTree(seedGroove);
  headNodeId = sessionTree.rootId;
  capturingBar = null;
  capturedNotes = [];
  ruleGenerationCounter = 0;
}

// --- Mined vocabulary (tools/midi-analysis) -------------------------------
// Optional, session-lifetime style influence: when loaded, the rule
// engine's "mutation" operation (see applyRuleGeneration below) samples
// per-voice/per-position timing and velocity variation from real
// performance statistics instead of a single flat hardcoded amount.
// Independent of seed/session state — loading a vocabulary doesn't touch
// the lineage tree, it only changes how future mutations behave.
let loadedVocabulary: Vocabulary | null = null;

function setVocabulary(json: string): boolean {
  loadedVocabulary = parseVocabulary(JSON.parse(json));
  return true;
}

function clearVocabulary(): void {
  loadedVocabulary = null;
}

interface EvolutionRuleInput {
  id: string;
  mutation: number;
  embellish: number;
  fill: number;
  hold: number;
}

type RuleOperation = "mutation" | "embellish" | "fill" | "hold";

function chooseRuleOperation(rule: EvolutionRuleInput, rng: () => number): RuleOperation {
  const weights: Array<[RuleOperation, number]> = [
    ["mutation", Math.max(0, rule.mutation)],
    ["embellish", Math.max(0, rule.embellish)],
    ["fill", Math.max(0, rule.fill)],
    ["hold", Math.max(0, rule.hold)],
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

function applyBasicFill(source: Groove): Groove {
  const groove = cloneGroove(source);
  const target = groove.lanes.find((lane) => lane.type === "snare")
    ?? groove.lanes.find((lane) => lane.type === "tom")
    ?? groove.lanes.find((lane) => lane.type !== "kick");
  if (!target) return groove;

  const barLength = Math.max(1, groove.referenceBarLengthBeats);
  const start = Math.max(0, barLength - 1);
  const pitch = target.outputMapping.note;
  const velocities = [72, 84, 96, 112];
  for (let step = 0; step < 4; step += 1) {
    const position = start + step * 0.25;
    if (!target.notes.some((note) => Math.abs(note.position - position) < 1 / 960)) {
      target.notes.push({position, pitch, velocity: velocities[step]!, duration: 0.2});
    }
  }
  target.notes.sort((left, right) => left.position - right.position);
  return groove;
}

function applyRuleGeneration(source: Groove, rule: EvolutionRuleInput, generation: number): {
  groove: Groove;
  operation: RuleOperation;
  seed: number;
  weights: {mutation: number; embellish: number; fill: number; hold: number};
} {
  const seed = (hashString(rule.id) ^ Math.imul(generation, 0x9e3779b1)) >>> 0;
  const operation = chooseRuleOperation(rule, createRng(seed));
  let result = cloneGroove(source);

  if (operation === "mutation") {
    // With a mined vocabulary loaded, style each note per its own lane's
    // voice and bar position from real performance statistics; otherwise
    // fall back to the original flat, uniform humanization.
    result = loadedVocabulary
      ? applyVocabularyStyle(source, source.lanes.map((lane) => lane.id), loadedVocabulary, seed)
      : applyMutation(
          source,
          "velocityHumanize",
          {laneIds: source.lanes.map((lane) => lane.id), barRange: {start: 0, end: 1}},
          {probability: 1, amount: 18},
          seed
        );
  } else if (operation === "embellish") {
    const preferred = source.lanes.filter((lane) => lane.type === "snare" || lane.type === "hihat");
    const laneIds = (preferred.length > 0 ? preferred : source.lanes).map((lane) => lane.id);
    result = applyMutation(
      source,
      "ghostNote",
      {laneIds, barRange: {start: 0, end: 1}},
      {probability: 0.42, ghostVelocity: 34, offsetBeats: 0.125},
      seed
    );
  } else if (operation === "fill") {
    result = applyBasicFill(source);
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
    },
  };
}

function evolveWithRule(rule: EvolutionRuleInput, branch: boolean): {
  nodeId: string;
  parentId: string;
  operation: RuleOperation;
} {
  const current = sessionTree.getNode(headNodeId);
  const parent = branch && current.parentId !== null ? sessionTree.getNode(current.parentId) : current;
  ruleGenerationCounter += 1;
  const evolved = applyRuleGeneration(parent.groove, rule, ruleGenerationCounter);
  const node = sessionTree.addChild(parent.id, evolved.groove, {
    type: "rule",
    ruleId: rule.id,
    operation: evolved.operation,
    seed: evolved.seed,
    weights: evolved.weights,
  });
  headNodeId = node.id;
  return {nodeId: node.id, parentId: parent.id, operation: evolved.operation};
}

function commitCapturedBar(): void {
  if (capturedNotes.length === 0) return;
  const lane = createLane({
    type: "other",
    outputMapping: { note: 0, channel: 1 },
    loopLengthBars: 1,
    notes: capturedNotes,
  });
  const groove = createGroove({
    name: `bar ${capturingBar ?? 0}`,
    tempo: 120,
    referenceBarLengthBeats: capturedBeatsPerBar,
    lanes: [lane],
  });
  const node = sessionTree.addChild(headNodeId, groove, { type: "recorded", capturedAtMs: Date.now() });
  headNodeId = node.id;
  capturedNotes = [];
}

function captureEvents(events: BridgeNoteEvent[], beatsPerBar: number): void {
  for (const event of events) {
    const bar = Math.floor(event.beatPosition / beatsPerBar);
    if (capturingBar !== null && bar !== capturingBar) commitCapturedBar();
    capturingBar = bar;
    capturedBeatsPerBar = beatsPerBar;
    capturedNotes.push({
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
} {
  const root = sessionTree.getNode(sessionTree.rootId);
  const rootNoteCount = root.groove.lanes.reduce((sum, lane) => sum + lane.notes.length, 0);
  const groupedLaneCount = root.groove.lanes.filter((lane) => lane.groupId !== undefined).length;
  return {
    nodeCount: sessionTree.toJSON().nodes.length,
    headNodeId,
    rootNoteCount,
    rootLaneCount: root.groove.lanes.length,
    groupedLaneCount,
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
 * beat range. Playback blocks and the eight-bar UI preview both call this
 * planner, so stochastic choices cannot disagree between what is drawn and
 * what the DAW receives. Future fills/embellishments/evolution rules should
 * enter here (or update the lineage head snapshot consumed here), never in
 * a parallel visual-only path.
 */
function planPlaybackRange(
  startBeat: number,
  endBeat: number,
  hostBeatsPerBar: number,
  params: BridgeParams,
  grooveOverride?: Groove,
  forceEvolved = false,
  extraPreviewFlags = 0
): PlaybackNoteEvent[] {
  if (endBeat <= startBeat || hostBeatsPerBar <= 0) return [];

  const headNode = sessionTree.getNode(headNodeId);
  const groove = grooveOverride ?? headNode.groove;
  const sourceBeatsPerBar = groove.referenceBarLengthBeats > 0
    ? groove.referenceBarLengthBeats
    : hostBeatsPerBar;
  const meterScale = hostBeatsPerBar / sourceBeatsPerBar;
  const humanizeAmount = Math.max(0, Math.min(40, params.humanizeAmount ?? 12));
  const ghostEnabled = (params.ghostNoteEnabled ?? 0) >= 1;
  const ghostProbability = Math.max(0, Math.min(1, params.ghostNoteProbability ?? 0.25));
  const evolvedFlag = forceEvolved || headNode.provenance?.type === "mutation"
      || headNode.provenance?.type === "liveSession"
      || headNode.provenance?.type === "rule"
    ? previewFlagEvolved
    : 0;
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
        const commonFlags = evolvedFlag | extraPreviewFlags | (delta !== 0 ? previewFlagHumanized : 0);

        if (absoluteBeat >= startBeat && absoluteBeat < endBeat) {
          events.push({
            note: Math.max(0, Math.min(127, Math.round(note.pitch))),
            velocity,
            channel: Math.max(1, Math.min(16, Math.round(lane.outputMapping.channel))),
            samplePosition: 0,
            beatPosition: absoluteBeat,
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

  return events.sort((left, right) =>
    left.beatPosition - right.beatPosition || left.channel - right.channel || left.note - right.note
  );
}

/**
 * Renders the current lineage head into one host process block. Each lane
 * keeps its own loop length, while note positions are scaled from the
 * groove's reference meter to the host's current meter. The half-open block
 * range means an event on a block boundary is emitted exactly once, by the
 * block that starts there.
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
  const events = planPlaybackRange(transport.blockStartBeat, blockEndBeat, hostBeatsPerBar, params);
  for (const event of events) {
    const rawSamplePosition = beatToSamplePosition(event.beatPosition, transport);
    event.samplePosition = Math.max(0, Math.min(blockSizeSamples - 1, rawSamplePosition));
  }
  return events.sort((left, right) =>
    left.samplePosition - right.samplePosition || left.channel - right.channel || left.note - right.note
  );
}

function renderPlaybackPreview(
  startBeat: number,
  beatsPerBar: number,
  barCount: number,
  params: BridgeParams,
  autoEvolution?: {
    running: boolean;
    rule: EvolutionRuleInput;
    nextEvolutionBar: number;
    frequencyBars: number;
  }
): PlaybackNoteEvent[] {
  const safeBeatsPerBar = beatsPerBar > 0 ? beatsPerBar : 4;
  const safeBarCount = Math.max(1, Math.min(32, Math.round(barCount)));
  const endBeat = startBeat + safeBeatsPerBar * safeBarCount;
  if (!autoEvolution?.running || !autoEvolution.rule.id) {
    return planPlaybackRange(startBeat, endBeat, safeBeatsPerBar, params);
  }

  const head = sessionTree.getNode(headNodeId);
  let groove = head.groove;
  let evolved = head.provenance?.type === "mutation"
      || head.provenance?.type === "liveSession"
      || head.provenance?.type === "rule";
  let generation = ruleGenerationCounter;
  let scheduledEvolutionApplied = false;
  let nextEvolutionBar = Math.round(autoEvolution.nextEvolutionBar);
  const frequencyBars = Math.max(1, Math.round(autoEvolution.frequencyBars));
  let cursor = startBeat;
  const events: PlaybackNoteEvent[] = [];

  while (cursor < endBeat - 1.0e-9) {
    const boundaryBeat = nextEvolutionBar * safeBeatsPerBar;
    if (boundaryBeat <= cursor + 1.0e-9) {
      generation += 1;
      groove = applyRuleGeneration(groove, autoEvolution.rule, generation).groove;
      evolved = true;
      scheduledEvolutionApplied = true;
      nextEvolutionBar += frequencyBars;
      continue;
    }
    const segmentEnd = Math.min(endBeat, boundaryBeat);
    events.push(...planPlaybackRange(cursor,
                                     segmentEnd,
                                     safeBeatsPerBar,
                                     params,
                                     groove,
                                     evolved,
                                     scheduledEvolutionApplied ? previewFlagScheduledEvolution : 0));
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
 * into the session lineage tree above, independent of what gets echoed
 * back to the host.
 */
function processBlock(events: BridgeNoteEvent[], transport: BridgeTransport, params: BridgeParams): BridgeNoteEvent[] {
  if (events.length === 0) return events;

  const beatsPerBar = transport.beatsPerBar > 0 ? transport.beatsPerBar : 4;
  const currentBar = Math.floor(events[0]!.beatPosition / beatsPerBar);
  const channel = events[0]!.channel;

  captureEvents(events, beatsPerBar);

  const lane = createLane({
    type: "other",
    outputMapping: { note: 0, channel: 1 },
    loopLengthBars: 1,
    notes: events.map((event) => ({
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
  paramsIn: BridgeParams,
  autoEvolutionIn?: {
    running: boolean;
    rule: EvolutionRuleInput;
    nextEvolutionBar: number;
    frequencyBars: number;
  }
) => renderPlaybackPreview(startBeatIn, beatsPerBarIn, barCountIn, paramsIn, autoEvolutionIn);

(globalThis as Record<string, unknown>).__lineageEvolveWithRule = (
  ruleIn: EvolutionRuleInput,
  branchIn: boolean
) => evolveWithRule(ruleIn, branchIn);

(globalThis as Record<string, unknown>).__lineageSetVocabulary = (jsonIn: string) => setVocabulary(jsonIn);

(globalThis as Record<string, unknown>).__lineageClearVocabulary = () => clearVocabulary();
