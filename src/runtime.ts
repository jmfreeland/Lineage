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
}

let callCounter = 0;

/**
 * MVP bridge (DESIGN.md §11): wraps a block's incoming note-on events as a
 * throwaway single-lane groove and runs them through the real
 * velocityHumanize mutation — proof that the C++/JS bridge executes actual
 * engine code (mutation.ts + mutations/velocityHumanize.ts, unmodified)
 * inside the plugin process, not a stand-in. Superseded once host
 * transport sync and the full lineage/live-loop session are wired in;
 * there is no bar/tempo mapping here yet, just enough structure for
 * applyMutation's targeting to have something to operate on.
 */
function processBlock(events: BridgeNoteEvent[]): BridgeNoteEvent[] {
  if (events.length === 0) return events;

  const lane = createLane({
    type: "other",
    outputMapping: { note: 0, channel: 1 },
    loopLengthBars: 1,
    notes: events.map((event, index) => ({
      position: index,
      pitch: event.note,
      velocity: event.velocity,
      duration: 0.25,
    })),
  });
  const groove = createGroove({
    name: "block",
    tempo: 120,
    referenceBarLengthBeats: events.length,
    lanes: [lane],
  });
  const target = { laneIds: [lane.id], barRange: { start: 0, end: 1 } };

  callCounter += 1;
  const result = applyMutation(groove, "velocityHumanize", target, { probability: 1, amount: 20 }, callCounter);
  const outNotes = result.lanes[0]!.notes;

  return events.map((event, index) => ({ ...event, velocity: outNotes[index]!.velocity }));
}

(globalThis as Record<string, unknown>).__lineageProcessBlock = (eventsIn: BridgeNoteEvent[]) =>
  processBlock(eventsIn);
