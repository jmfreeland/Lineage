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
import type { NoteEvent } from "./types.js";

registerBuiltinMutations();

interface BridgeNoteEvent {
  note: number;
  velocity: number;
  channel: number;
  samplePosition: number;
  /** Absolute host beat position, computed on the C++ side from transport + samplePosition. */
  beatPosition: number;
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
const sessionTree = new LineageTree(
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
function getSessionInfo(): { nodeCount: number; headNodeId: string } {
  return { nodeCount: sessionTree.toJSON().nodes.length, headNodeId };
}

// --- Live block processing ------------------------------------------------

function beatToSamplePosition(beat: number, transport: BridgeTransport): number {
  const secondsPerBeat = transport.tempo > 0 ? 60 / transport.tempo : 0.5;
  return Math.round((beat - transport.blockStartBeat) * secondsPerBeat * transport.sampleRate);
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
