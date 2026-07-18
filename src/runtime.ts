// Plugin runtime entry point (DESIGN.md §11) — bundled with esbuild into a
// single self-contained script and embedded in the JUCE plugin, executed
// inside QuickJS. Deliberately narrow: only the pure engine logic that has
// no Node dependency (no fs/path, so no persistence.ts/library.ts/
// jsonFileLibrary.ts/scriptMutations.ts — those are for host-side tooling,
// not the embedded runtime).
import { createGroove, createLane } from "./groove.js";
import { applyMutation } from "./mutation.js";
import { registerBuiltinMutations } from "./mutations/index.js";
import { LineageTree } from "./lineage.js";
import type { LaneType, NoteEvent } from "./types.js";

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

// Default GM-ish drum map for lanes authored via the step sequencer, which
// only sends a lane type (e.g. "kick") and not a specific MIDI note.
const DEFAULT_PITCH: Record<string, number> = { kick: 36, snare: 38, hihat: 42 };

interface SeedNote {
  laneType: string;
  step: number;
  velocity: number;
}

/**
 * Replaces the session's history with a fresh tree rooted at a groove
 * authored visually (the plugin's step-sequencer editor) — a real starting
 * point instead of an empty pattern. This is a hard reset of the session,
 * not a branch: it's "program a starting groove," not "add a generation."
 * Anything captured from live play before this point is discarded from
 * the session (not from the DAW's own undo history).
 */
function setSeedGroove(notes: SeedNote[], stepsPerBar: number, beatsPerBar: number): void {
  const beatsPerStep = stepsPerBar > 0 ? beatsPerBar / stepsPerBar : 0.25;
  const laneTypes = [...new Set(notes.map((n) => n.laneType))];

  const lanes = laneTypes.map((laneType) =>
    createLane({
      // Trusted input: the step sequencer only ever sends lane types this
      // engine knows about (kick/snare/hihat), not arbitrary external data.
      type: laneType as LaneType,
      outputMapping: { note: DEFAULT_PITCH[laneType] ?? 36, channel: 1 },
      loopLengthBars: 1,
      notes: notes
        .filter((n) => n.laneType === laneType)
        .map((n) => ({
          position: n.step * beatsPerStep,
          pitch: DEFAULT_PITCH[laneType] ?? 36,
          velocity: n.velocity,
          duration: beatsPerStep,
        })),
    })
  );

  const seedGroove = createGroove({ name: "seed", tempo: 120, referenceBarLengthBeats: beatsPerBar, lanes });
  sessionTree = new LineageTree(seedGroove);
  headNodeId = sessionTree.rootId;
  capturingBar = null;
  capturedNotes = [];
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
function getSessionInfo(): { nodeCount: number; headNodeId: string; rootNoteCount: number } {
  const root = sessionTree.getNode(sessionTree.rootId);
  const rootNoteCount = root.groove.lanes.reduce((sum, lane) => sum + lane.notes.length, 0);
  return { nodeCount: sessionTree.toJSON().nodes.length, headNodeId, rootNoteCount };
}

// --- Live block processing ------------------------------------------------

function beatToSamplePosition(beat: number, transport: BridgeTransport): number {
  const secondsPerBeat = transport.tempo > 0 ? 60 / transport.tempo : 0.5;
  return Math.round((beat - transport.blockStartBeat) * secondsPerBeat * transport.sampleRate);
}

/**
 * Renders the current lineage head into one host process block. Each lane
 * keeps its own loop length, while note positions are scaled from the
 * groove's reference meter to the host's current meter. The half-open block
 * range means an event on a block boundary is emitted exactly once, by the
 * block that starts there.
 */
function renderPlaybackBlock(transport: BridgeTransport, blockSizeSamples: number): PlaybackNoteEvent[] {
  if (transport.tempo <= 0 || transport.sampleRate <= 0 || blockSizeSamples <= 0) return [];

  const groove = sessionTree.getNode(headNodeId).groove;
  const hostBeatsPerBar = transport.beatsPerBar > 0 ? transport.beatsPerBar : 4;
  const sourceBeatsPerBar = groove.referenceBarLengthBeats > 0 ? groove.referenceBarLengthBeats : hostBeatsPerBar;
  const meterScale = hostBeatsPerBar / sourceBeatsPerBar;
  const blockEndBeat = transport.blockStartBeat +
    (blockSizeSamples / transport.sampleRate) * (transport.tempo / 60);
  const events: PlaybackNoteEvent[] = [];

  for (const lane of groove.lanes) {
    const loopLengthBeats = lane.loopLengthBars * hostBeatsPerBar;
    if (loopLengthBeats <= 0) continue;

    const firstCycle = Math.floor(transport.blockStartBeat / loopLengthBeats);
    const lastCycle = Math.floor((blockEndBeat - Number.EPSILON) / loopLengthBeats);

    for (let cycle = firstCycle; cycle <= lastCycle; cycle += 1) {
      const cycleStartBeat = cycle * loopLengthBeats;
      for (const note of lane.notes) {
        const scaledPosition = note.position * meterScale;
        const localBeat = ((scaledPosition % loopLengthBeats) + loopLengthBeats) % loopLengthBeats;
        const absoluteBeat = cycleStartBeat + localBeat;
        if (absoluteBeat < transport.blockStartBeat || absoluteBeat >= blockEndBeat) continue;

        const rawSamplePosition = beatToSamplePosition(absoluteBeat, transport);
        events.push({
          note: Math.max(0, Math.min(127, Math.round(note.pitch))),
          velocity: Math.max(1, Math.min(127, Math.round(note.velocity))),
          channel: Math.max(1, Math.min(16, Math.round(lane.outputMapping.channel))),
          samplePosition: Math.max(0, Math.min(blockSizeSamples - 1, rawSamplePosition)),
          beatPosition: absoluteBeat,
          durationBeats: Math.max(1 / 960, note.duration * meterScale),
        });
      }
    }
  }

  return events.sort((left, right) =>
    left.samplePosition - right.samplePosition || left.channel - right.channel || left.note - right.note
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
  notesIn: SeedNote[],
  stepsPerBarIn: number,
  beatsPerBarIn: number
) => setSeedGroove(notesIn, stepsPerBarIn, beatsPerBarIn);

(globalThis as Record<string, unknown>).__lineageRenderPlaybackBlock = (
  transportIn: BridgeTransport,
  blockSizeSamplesIn: number
) => renderPlaybackBlock(transportIn, blockSizeSamplesIn);
