import { describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { applyFill } from "../fill.js";
import type { Fill } from "../types.js";

function makeGroove() {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [{ position: 0, pitch: 36, velocity: 100, duration: 0.25 }],
  });
  const hihat = createLane({
    type: "hihat",
    outputMapping: { note: 42, channel: 1 },
    loopLengthBars: 1,
    notes: [{ position: 0, pitch: 42, velocity: 80, duration: 0.1 }],
  });
  return createGroove({
    name: "Test Groove",
    tempo: 120,
    referenceBarLengthBeats: 4,
    lanes: [kick, hihat],
  });
}

describe("applyFill", () => {
  it("replaces content only for lanes the fill defines", () => {
    const groove = makeGroove();
    const fill: Fill = {
      id: "kickFill",
      name: "Kick Fill",
      lengthBars: 1,
      lanes: [
        {
          laneType: "kick",
          notes: [
            { position: 0, pitch: 36, velocity: 120, duration: 0.25 },
            { position: 2, pitch: 36, velocity: 120, duration: 0.25 },
            { position: 3, pitch: 36, velocity: 120, duration: 0.25 },
          ],
        },
      ],
    };

    const result = applyFill(groove, fill);

    expect(result.lanes.find((l) => l.type === "kick")!.notes).toHaveLength(3);
    // hihat wasn't part of the fill, so it's untouched.
    expect(result.lanes.find((l) => l.type === "hihat")!.notes).toEqual(
      groove.lanes.find((l) => l.type === "hihat")!.notes
    );
  });

  it("skips locked lanes", () => {
    const groove = makeGroove();
    groove.lanes.find((l) => l.type === "kick")!.locked = true;
    const fill: Fill = {
      id: "kickFill",
      name: "Kick Fill",
      lengthBars: 1,
      lanes: [{ laneType: "kick", notes: [{ position: 0, pitch: 36, velocity: 1, duration: 0.1 }] }],
    };

    const result = applyFill(groove, fill);

    expect(result.lanes.find((l) => l.type === "kick")!.notes).toEqual(
      groove.lanes.find((l) => l.type === "kick")!.notes
    );
  });

  it("throws when the fill length doesn't match the lane's loop length", () => {
    const groove = makeGroove();
    const fill: Fill = {
      id: "twoBarFill",
      name: "Two Bar Fill",
      lengthBars: 2,
      lanes: [{ laneType: "kick", notes: [] }],
    };

    expect(() => applyFill(groove, fill)).toThrow(/bar/i);
  });

  it("leaves the original groove untouched", () => {
    const groove = makeGroove();
    const original = JSON.parse(JSON.stringify(groove));
    const fill: Fill = {
      id: "kickFill",
      name: "Kick Fill",
      lengthBars: 1,
      lanes: [{ laneType: "kick", notes: [{ position: 0, pitch: 36, velocity: 1, duration: 0.1 }] }],
    };

    applyFill(groove, fill);

    expect(groove).toEqual(original);
  });
});
