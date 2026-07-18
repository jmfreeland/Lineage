import type { Groove } from "./types.js";
import { LineageTree, type SerializedLineageTree } from "./lineage.js";

export interface SerializedGroove {
  schemaVersion: 1;
  groove: Groove;
}

/** Quick-save format for a single groove (§7 — plain JSON, easy to inspect or hand-edit). */
export function serializeGroove(groove: Groove): string {
  const payload: SerializedGroove = { schemaVersion: 1, groove };
  return JSON.stringify(payload, null, 2);
}

export function deserializeGroove(json: string): Groove {
  const payload = JSON.parse(json) as SerializedGroove;
  if (payload.schemaVersion !== 1) {
    throw new Error(`Unsupported groove schema version: ${payload.schemaVersion}`);
  }
  return payload.groove;
}

/** Exports a full lineage tree — the "genome" (§3), not just the resulting MIDI. */
export function serializeLineageTree(tree: LineageTree): string {
  return JSON.stringify(tree.toJSON(), null, 2);
}

export function deserializeLineageTree(json: string): LineageTree {
  const payload = JSON.parse(json) as SerializedLineageTree;
  if (payload.schemaVersion !== 1) {
    throw new Error(`Unsupported lineage tree schema version: ${payload.schemaVersion}`);
  }
  return LineageTree.fromJSON(payload);
}
