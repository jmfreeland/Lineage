import { cloneGroove } from "./groove.js";
import { applyMutation } from "./mutation.js";
import { randomSeed } from "./rng.js";
import { LineageTree } from "./lineage.js";
import type {
  Groove,
  LineageNode,
  LiveSessionPass,
  MutationParamValues,
  MutationTarget,
} from "./types.js";

export interface LiveMutationSpec {
  mutationId: string;
  target: MutationTarget;
  params?: MutationParamValues;
}

export interface LiveLoopSessionOptions {
  tree: LineageTree;
  /** The lineage node playback is frozen on — quick-save commits become children of this node. */
  anchorNodeId: string;
  /** Explicit sequence of node ids to step through when branch-walking. */
  walkPath?: string[];
  /** Convenience: derive walkPath as the branch from anchorNodeId to this descendant. */
  walkTargetNodeId?: string;
}

/**
 * Numerology-style live/generative playback (§10): freeze on a branch,
 * loop it, and optionally let it keep evolving while it plays. advance()
 * represents one loop pass and is caller-driven — this class has no
 * real-time clock of its own. Deciding *when* a pass completes (host
 * transport, a standalone player's scheduler, etc.) is a separate concern
 * layered on top; this is just the state machine.
 */
export class LiveLoopSession {
  readonly tree: LineageTree;
  readonly anchorNodeId: string;
  currentGroove: Groove;

  /** Independent toggles, per §10 — combine freely. */
  branchWalkEnabled = false;
  liveMutationEnabled = false;
  liveMutations: LiveMutationSpec[] = [];

  private walkPath: string[];
  private walkIndex = 0;
  private passes: LiveSessionPass[] = [];

  constructor(options: LiveLoopSessionOptions) {
    this.tree = options.tree;
    this.anchorNodeId = options.anchorNodeId;
    this.walkPath = LiveLoopSession.resolveWalkPath(options);
    this.currentGroove = cloneGroove(this.tree.getNode(this.anchorNodeId).groove);
  }

  private static resolveWalkPath(options: LiveLoopSessionOptions): string[] {
    if (options.walkPath) return options.walkPath;
    if (options.walkTargetNodeId) {
      const branch = options.tree.getBranch(options.walkTargetNodeId);
      const anchorIndex = branch.findIndex((node) => node.id === options.anchorNodeId);
      if (anchorIndex === -1) {
        throw new Error(
          `Walk target "${options.walkTargetNodeId}" is not a descendant of anchor "${options.anchorNodeId}"`
        );
      }
      return branch.slice(anchorIndex).map((node) => node.id);
    }
    return [options.anchorNodeId];
  }

  /** One loop pass: branch-walk step (if enabled), then live mutation (if enabled), on top of whatever that leaves. */
  advance(): void {
    if (this.branchWalkEnabled) {
      this.walkIndex = (this.walkIndex + 1) % this.walkPath.length;
      const node = this.tree.getNode(this.walkPath[this.walkIndex]!);
      this.currentGroove = cloneGroove(node.groove);
    }

    if (this.liveMutationEnabled) {
      for (const spec of this.liveMutations) {
        const seed = randomSeed();
        const params = spec.params ?? {};
        this.currentGroove = applyMutation(this.currentGroove, spec.mutationId, spec.target, params, seed);
        this.passes.push({ mutationId: spec.mutationId, params, seed, target: spec.target });
      }
    }
  }

  /** Quick-save: commits the current live state as a permanent child of the anchor node (§10). */
  commit(): LineageNode {
    const node = this.tree.addChild(this.anchorNodeId, this.currentGroove, {
      type: "liveSession",
      anchorNodeId: this.anchorNodeId,
      passes: [...this.passes],
    });
    this.passes = [];
    return node;
  }
}
