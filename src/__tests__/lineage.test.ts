import { beforeEach, describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { applyMutation, listMutations } from "../mutation.js";
import { registerBuiltinMutations } from "../mutations/index.js";
import { LineageTree } from "../lineage.js";

function makeGroove() {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [{ position: 0, pitch: 36, velocity: 100, duration: 0.25 }],
  });
  return createGroove({
    name: "Test Groove",
    tempo: 120,
    referenceBarLengthBeats: 4,
    lanes: [kick],
  });
}

beforeEach(() => {
  if (listMutations().length === 0) registerBuiltinMutations();
});

describe("LineageTree", () => {
  it("creates a root node with null provenance", () => {
    const tree = new LineageTree(makeGroove());
    const root = tree.getNode(tree.rootId);

    expect(root.parentId).toBeNull();
    expect(root.provenance).toBeNull();
  });

  it("records provenance and parentage for a mutated child", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };
    const params = { probability: 1 };
    const seed = 99;

    const mutated = applyMutation(groove, "velocityHumanize", target, params, seed);
    const child = tree.addChild(tree.rootId, mutated, {
      type: "mutation",
      mutationId: "velocityHumanize",
      mutationVersion: 1,
      params,
      seed,
      target,
    });

    expect(child.parentId).toBe(tree.rootId);
    expect(child.provenance).toEqual({
      type: "mutation",
      mutationId: "velocityHumanize",
      mutationVersion: 1,
      params,
      seed,
      target,
    });
  });

  it("returns the root-to-node branch in order", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };

    const gen1 = applyMutation(groove, "velocityHumanize", target, {}, 1);
    const node1 = tree.addChild(tree.rootId, gen1, {
      type: "mutation",
      mutationId: "velocityHumanize",
      mutationVersion: 1,
      params: {},
      seed: 1,
      target,
    });

    const gen2 = applyMutation(gen1, "ghostNote", target, {}, 2);
    const node2 = tree.addChild(node1.id, gen2, {
      type: "mutation",
      mutationId: "ghostNote",
      mutationVersion: 1,
      params: {},
      seed: 2,
      target,
    });

    const branch = tree.getBranch(node2.id);

    expect(branch.map((n) => n.id)).toEqual([tree.rootId, node1.id, node2.id]);
  });

  it("keeps each node's snapshot independent of later mutations", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };

    const mutated = applyMutation(groove, "velocityHumanize", target, { probability: 1 }, 5);
    const child = tree.addChild(tree.rootId, mutated, {
      type: "mutation",
      mutationId: "velocityHumanize",
      mutationVersion: 1,
      params: { probability: 1 },
      seed: 5,
      target,
    });

    mutated.lanes[0]!.notes[0]!.velocity = 1;

    expect(tree.getNode(child.id).groove.lanes[0]!.notes[0]!.velocity).not.toBe(1);
  });
});
