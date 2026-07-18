import { beforeEach, describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { applyMutation, listMutations } from "../mutation.js";
import { registerBuiltinMutations } from "../mutations/index.js";
import { LineageTree } from "../lineage.js";
import { LiveLoopSession } from "../liveLoopSession.js";
import type { Groove } from "../types.js";

function makeGroove(): Groove {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [
      { position: 0, pitch: 36, velocity: 100, duration: 0.25 },
      { position: 2, pitch: 36, velocity: 100, duration: 0.25 },
    ],
  });
  return createGroove({ name: "Test", tempo: 120, referenceBarLengthBeats: 4, lanes: [kick] });
}

beforeEach(() => {
  if (listMutations().length === 0) registerBuiltinMutations();
});

describe("LiveLoopSession", () => {
  it("with everything disabled, advance() is a no-op — a static frozen loop", () => {
    const tree = new LineageTree(makeGroove());
    const session = new LiveLoopSession({ tree, anchorNodeId: tree.rootId });
    const before = JSON.stringify(session.currentGroove);

    session.advance();
    session.advance();

    expect(JSON.stringify(session.currentGroove)).toBe(before);
  });

  it("branch walk cycles through the resolved path and wraps around", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };
    const gen1 = applyMutation(groove, "velocityHumanize", target, { probability: 1 }, 1);
    const node1 = tree.addChild(tree.rootId, gen1, {
      type: "mutation",
      mutationId: "velocityHumanize",
      mutationVersion: 1,
      params: { probability: 1 },
      seed: 1,
      target,
    });

    const session = new LiveLoopSession({
      tree,
      anchorNodeId: tree.rootId,
      walkTargetNodeId: node1.id,
    });
    session.branchWalkEnabled = true;

    expect(session.currentGroove).toEqual(tree.getNode(tree.rootId).groove);
    session.advance();
    expect(session.currentGroove).toEqual(node1.groove);
    session.advance(); // wraps back to the anchor
    expect(session.currentGroove).toEqual(tree.getNode(tree.rootId).groove);
  });

  it("live mutation mode evolves the working state without touching the tree", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const session = new LiveLoopSession({ tree, anchorNodeId: tree.rootId });
    session.liveMutationEnabled = true;
    session.liveMutations = [
      {
        mutationId: "velocityHumanize",
        target: { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } },
        params: { probability: 1 },
      },
    ];

    session.advance();
    session.advance();
    session.advance();

    expect(session.currentGroove.lanes[0]!.notes).not.toEqual(groove.lanes[0]!.notes);
    // Nothing was committed — flooding the tree with near-duplicate nodes is exactly what §10 avoids.
    expect(tree.getChildren(tree.rootId)).toHaveLength(0);
  });

  it("commit() records every pass since the anchor as liveSession provenance", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const session = new LiveLoopSession({ tree, anchorNodeId: tree.rootId });
    session.liveMutationEnabled = true;
    session.liveMutations = [
      {
        mutationId: "velocityHumanize",
        target: { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } },
        params: { probability: 1 },
      },
    ];

    session.advance();
    session.advance();
    const node = session.commit();

    expect(node.parentId).toBe(tree.rootId);
    expect(node.provenance).toMatchObject({ type: "liveSession", anchorNodeId: tree.rootId });
    if (node.provenance?.type === "liveSession") {
      expect(node.provenance.passes).toHaveLength(2);
    }
    expect(node.groove).toEqual(session.currentGroove);
  });

  it("commit() clears the pass buffer so a second commit only records new passes", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const session = new LiveLoopSession({ tree, anchorNodeId: tree.rootId });
    session.liveMutationEnabled = true;
    session.liveMutations = [
      {
        mutationId: "velocityHumanize",
        target: { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } },
        params: { probability: 1 },
      },
    ];

    session.advance();
    session.commit();
    session.advance();
    const secondNode = session.commit();

    if (secondNode.provenance?.type === "liveSession") {
      expect(secondNode.provenance.passes).toHaveLength(1);
    }
  });

  it("throws when walkTargetNodeId is not a descendant of the anchor", () => {
    const groove = makeGroove();
    const tree = new LineageTree(groove);
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };

    // A sibling branch off the root — not a descendant of `sibling`, which we'll anchor on.
    const sibling = tree.addChild(tree.rootId, applyMutation(groove, "velocityHumanize", target, {}, 1), {
      type: "mutation",
      mutationId: "velocityHumanize",
      mutationVersion: 1,
      params: {},
      seed: 1,
      target,
    });
    const other = tree.addChild(tree.rootId, applyMutation(groove, "ghostNote", target, {}, 2), {
      type: "mutation",
      mutationId: "ghostNote",
      mutationVersion: 1,
      params: {},
      seed: 2,
      target,
    });

    expect(
      () =>
        new LiveLoopSession({
          tree,
          anchorNodeId: sibling.id,
          walkTargetNodeId: other.id,
        })
    ).toThrow(/descendant/);
  });
});
