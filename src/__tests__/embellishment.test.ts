import { describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { applyEmbellishment } from "../embellishment.js";
import type { Embellishment } from "../types.js";

function makeGroove() {
  const hihat = createLane({
    type: "hihat",
    outputMapping: { note: 42, channel: 1 },
    loopLengthBars: 1,
    notes: [{ id: "test_note_2", position: 0, pitch: 42, velocity: 80, duration: 0.1 }],
  });
  return createGroove({
    name: "Test Groove",
    tempo: 120,
    referenceBarLengthBeats: 4,
    lanes: [hihat],
  });
}

describe("applyEmbellishment", () => {
  it("adds notes without removing existing ones", () => {
    const groove = makeGroove();
    const laneId = groove.lanes[0]!.id;
    const embellishment: Embellishment = {
      id: "hatRoll",
      name: "Hat Roll",
      laneType: "hihat",
      notes: [
        { position: 0.5, pitch: 42, velocity: 50, duration: 0.05 },
        { position: 0.75, pitch: 42, velocity: 50, duration: 0.05 },
      ],
    };

    const result = applyEmbellishment(groove, embellishment, laneId, 0);
    const lane = result.lanes[0]!;

    expect(lane.notes).toHaveLength(3);
    expect(lane.notes.map((n) => n.position)).toEqual([0, 0.5, 0.75]);
  });

  it("offsets embellishment notes by the target bar", () => {
    const groove = makeGroove();
    const laneId = groove.lanes[0]!.id;
    const embellishment: Embellishment = {
      id: "hatRoll",
      name: "Hat Roll",
      laneType: "hihat",
      notes: [{ position: 0, pitch: 42, velocity: 50, duration: 0.05 }],
    };

    // referenceBarLengthBeats is 4, so bar 2 starts at beat 8.
    const result = applyEmbellishment(groove, embellishment, laneId, 2);

    expect(result.lanes[0]!.notes.map((n) => n.position)).toContain(8);
  });

  it("assigns each inserted note a fresh, distinct id, and doesn't disturb the existing note's id", () => {
    const groove = makeGroove();
    const laneId = groove.lanes[0]!.id;
    const existingId = groove.lanes[0]!.notes[0]!.id;
    const embellishment: Embellishment = {
      id: "hatRoll",
      name: "Hat Roll",
      laneType: "hihat",
      notes: [
        { position: 0.5, pitch: 42, velocity: 50, duration: 0.05 },
        { position: 0.75, pitch: 42, velocity: 50, duration: 0.05 },
      ],
    };

    const result = applyEmbellishment(groove, embellishment, laneId, 0);
    const ids = result.lanes[0]!.notes.map((n) => n.id);

    expect(ids).toContain(existingId);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it("throws on lane type mismatch", () => {
    const groove = makeGroove();
    const laneId = groove.lanes[0]!.id;
    const embellishment: Embellishment = {
      id: "kickThing",
      name: "Kick Thing",
      laneType: "kick",
      notes: [],
    };

    expect(() => applyEmbellishment(groove, embellishment, laneId, 0)).toThrow(/hihat/i);
  });

  it("is a no-op on a locked lane", () => {
    const groove = makeGroove();
    groove.lanes[0]!.locked = true;
    const laneId = groove.lanes[0]!.id;
    const embellishment: Embellishment = {
      id: "hatRoll",
      name: "Hat Roll",
      laneType: "hihat",
      notes: [{ position: 0.5, pitch: 42, velocity: 50, duration: 0.05 }],
    };

    const result = applyEmbellishment(groove, embellishment, laneId, 0);

    expect(result.lanes[0]!.notes).toEqual(groove.lanes[0]!.notes);
  });
});
