import { beforeEach, describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { applyMutation, listMutations } from "../mutation.js";
import { registerBuiltinMutations } from "../mutations/index.js";
import type { Groove } from "../types.js";

function makeGroove(): Groove {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [
      { position: 0, pitch: 36, velocity: 100, duration: 0.25 },
      { position: 1, pitch: 36, velocity: 100, duration: 0.25 },
      { position: 2, pitch: 36, velocity: 100, duration: 0.25 },
      { position: 3, pitch: 36, velocity: 100, duration: 0.25 },
    ],
  });
  return createGroove({
    name: "Test Groove",
    tempo: 120,
    referenceBarLengthBeats: 4,
    lanes: [kick],
  });
}

beforeEach(() => {
  // registerMutation throws on duplicate ids, and the registry is
  // module-level — only register once per process.
  if (listMutations().length === 0) registerBuiltinMutations();
});

describe("applyMutation", () => {
  it("is deterministic for a given seed (§7 reproducibility)", () => {
    const groove = makeGroove();
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };

    const a = applyMutation(groove, "velocityHumanize", target, {}, 42);
    const b = applyMutation(groove, "velocityHumanize", target, {}, 42);

    expect(a.lanes[0]!.notes).toEqual(b.lanes[0]!.notes);
  });

  it("produces different results for different seeds", () => {
    const groove = makeGroove();
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };

    const a = applyMutation(groove, "velocityHumanize", target, {}, 1);
    const b = applyMutation(groove, "velocityHumanize", target, {}, 2);

    expect(a.lanes[0]!.notes).not.toEqual(b.lanes[0]!.notes);
  });

  it("leaves the original groove untouched", () => {
    const groove = makeGroove();
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };
    const original = JSON.parse(JSON.stringify(groove.lanes[0]!.notes));

    applyMutation(groove, "velocityHumanize", target, { probability: 1 }, 7);

    expect(groove.lanes[0]!.notes).toEqual(original);
  });

  it("skips locked lanes (§2)", () => {
    const groove = makeGroove();
    groove.lanes[0]!.locked = true;
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };

    const result = applyMutation(groove, "velocityHumanize", target, { probability: 1 }, 7);

    expect(result.lanes[0]!.notes).toEqual(groove.lanes[0]!.notes);
  });

  it("ghostNote inserts notes only within the target bar range", () => {
    const groove = makeGroove();
    // Only bar 0 (beats 0-4) targeted; note at position 3 is the last hit in range.
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };

    const result = applyMutation(
      groove,
      "ghostNote",
      target,
      { probability: 1 },
      7
    );

    // 4 original notes, all in range -> up to 4 ghosts inserted.
    expect(result.lanes[0]!.notes.length).toBeGreaterThan(4);
    expect(result.lanes[0]!.notes.length).toBeLessThanOrEqual(8);
  });
});
