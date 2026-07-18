// Plugin runtime entry point (DESIGN.md §11) — bundled with esbuild into a
// single self-contained script and embedded in the JUCE plugin, executed
// inside QuickJS. Deliberately narrow: only the pure engine logic that has
// no Node dependency (no fs/path, so no persistence.ts/library.ts/
// jsonFileLibrary.ts/scriptMutations.ts — those are for host-side tooling,
// not the embedded runtime).
import { createGroove, createLane } from "./groove.js";
import { applyMutation } from "./mutation.js";
import { registerBuiltinMutations } from "./mutations/index.js";

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
}

type BridgeParams = Record<string, number>;

let callCounter = 0;

/**
 * MVP bridge (DESIGN.md §11): wraps a block's incoming note-on events as a
 * throwaway single-bar-lane groove and runs them through the real
 * velocityHumanize mutation — proof that the C++/JS bridge executes actual
 * engine code (mutation.ts + mutations/velocityHumanize.ts, unmodified)
 * inside the plugin process, not a stand-in.
 *
 * Notes are positioned within their actual current bar using the host's
 * real tempo/time-signature (via `transport` and each event's
 * `beatPosition`), not a made-up per-block index — so bar-range-aware
 * mutation targeting means something real. There is still no persistent
 * multi-bar arrangement/lineage/live-loop session living in the plugin;
 * each block is processed independently.
 */
function processBlock(
  events: BridgeNoteEvent[],
  transport: BridgeTransport,
  params: BridgeParams
): BridgeNoteEvent[] {
  if (events.length === 0) return events;

  const beatsPerBar = transport.beatsPerBar > 0 ? transport.beatsPerBar : 4;

  const lane = createLane({
    type: "other",
    outputMapping: { note: 0, channel: 1 },
    loopLengthBars: 1,
    notes: events.map((event) => ({
      position: ((event.beatPosition % beatsPerBar) + beatsPerBar) % beatsPerBar,
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
  const mutationParams = { probability: 1, amount: params.amount ?? 20 };
  const result = applyMutation(groove, "velocityHumanize", target, mutationParams, callCounter);
  const outNotes = result.lanes[0]!.notes;

  return events.map((event, index) => ({ ...event, velocity: outNotes[index]!.velocity }));
}

(globalThis as Record<string, unknown>).__lineageProcessBlock = (
  eventsIn: BridgeNoteEvent[],
  transportIn: BridgeTransport,
  paramsIn: BridgeParams
) => processBlock(eventsIn, transportIn, paramsIn);
