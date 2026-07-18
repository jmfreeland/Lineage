import { cloneGroove } from "./groove.js";
import type { Groove, LineageNode, LineageNodeProvenance } from "./types.js";

let idCounter = 0;
function nextId(): string {
  idCounter += 1;
  return `node_${Date.now().toString(36)}_${idCounter}`;
}

export interface SerializedLineageTree {
  schemaVersion: 1;
  rootId: string;
  nodes: LineageNode[];
}

/**
 * Each node holds a full groove snapshot plus provenance describing how it
 * was derived — see DESIGN.md §3. Snapshots give instant access to any
 * ancestor; provenance keeps the mutation history ("genome") traceable
 * without requiring replay to reconstruct state.
 */
export class LineageTree {
  private nodes = new Map<string, LineageNode>();
  readonly rootId: string;

  constructor(source: Groove | Omit<SerializedLineageTree, "schemaVersion">) {
    if ("lanes" in source) {
      const root: LineageNode = {
        id: nextId(),
        parentId: null,
        groove: cloneGroove(source),
        provenance: null,
        createdAt: Date.now(),
      };
      this.nodes.set(root.id, root);
      this.rootId = root.id;
    } else {
      for (const node of source.nodes) this.nodes.set(node.id, node);
      this.rootId = source.rootId;
    }
  }

  /** Exports the full tree ("genome" — DESIGN.md §3) for save/load or sharing. */
  toJSON(): SerializedLineageTree {
    return { schemaVersion: 1, rootId: this.rootId, nodes: [...this.nodes.values()] };
  }

  static fromJSON(data: SerializedLineageTree): LineageTree {
    return new LineageTree(data);
  }

  addChild(parentId: string, groove: Groove, provenance: LineageNodeProvenance): LineageNode {
    if (!this.nodes.has(parentId)) {
      throw new Error(`Unknown lineage node "${parentId}"`);
    }
    const node: LineageNode = {
      id: nextId(),
      parentId,
      groove: cloneGroove(groove),
      provenance,
      createdAt: Date.now(),
    };
    this.nodes.set(node.id, node);
    return node;
  }

  getNode(id: string): LineageNode {
    const node = this.nodes.get(id);
    if (!node) throw new Error(`Unknown lineage node "${id}"`);
    return node;
  }

  getChildren(id: string): LineageNode[] {
    return [...this.nodes.values()].filter((node) => node.parentId === id);
  }

  /** Root-to-node path — the branch leading to this node. */
  getBranch(id: string): LineageNode[] {
    const branch: LineageNode[] = [];
    let current: LineageNode | undefined = this.getNode(id);
    while (current) {
      branch.unshift(current);
      current = current.parentId ? this.nodes.get(current.parentId) : undefined;
    }
    return branch;
  }
}
