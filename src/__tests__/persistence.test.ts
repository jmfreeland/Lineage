import { describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { LineageTree } from "../lineage.js";
import {
  deserializeGroove,
  deserializeLineageTree,
  serializeGroove,
  serializeLineageTree,
} from "../persistence.js";

function makeGroove() {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [{ id: "test_note_13", position: 0, pitch: 36, velocity: 100, duration: 0.25 }],
  });
  return createGroove({
    name: "Test Groove",
    tempo: 120,
    referenceBarLengthBeats: 4,
    lanes: [kick],
  });
}

describe("groove persistence", () => {
  it("round-trips a groove through JSON", () => {
    const groove = makeGroove();
    const restored = deserializeGroove(serializeGroove(groove));
    expect(restored).toEqual(groove);
  });

  it("rejects an unsupported schema version", () => {
    const bad = JSON.stringify({ schemaVersion: 99, groove: makeGroove() });
    expect(() => deserializeGroove(bad)).toThrow(/schema version/i);
  });
});

describe("lineage tree persistence", () => {
  it("round-trips a tree, preserving nodes and branch lookups", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const child = tree.addChild(tree.rootId, groove, {
      type: "mutation",
      mutationId: "velocityHumanize",
      mutationVersion: 1,
      params: {},
      seed: 1,
      target: { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } },
    });

    const restored = deserializeLineageTree(serializeLineageTree(tree));

    expect(restored.rootId).toBe(tree.rootId);
    expect(restored.getNode(child.id)).toEqual(tree.getNode(child.id));
    expect(restored.getBranch(child.id).map((n) => n.id)).toEqual(
      tree.getBranch(child.id).map((n) => n.id)
    );
  });

  it("rejects an unsupported schema version", () => {
    const bad = JSON.stringify({ schemaVersion: 99, rootId: "x", nodes: [] });
    expect(() => deserializeLineageTree(bad)).toThrow(/schema version/i);
  });
});
