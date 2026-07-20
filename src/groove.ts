import type { Groove, Lane } from "./types.js";

let idCounter = 0;
function nextId(prefix: string): string {
  idCounter += 1;
  return `${prefix}_${Date.now().toString(36)}_${idCounter}`;
}

/** For NoteEvent.id — every place a genuinely new note is created (not a
 * transform of an existing one) needs one of these. See NoteEvent's own
 * doc comment in types.ts for why this exists. */
export function nextNoteId(): string {
  return nextId("note");
}

export function createLane(
  partial: Omit<Lane, "id" | "notes" | "locked"> & { notes?: Lane["notes"] }
): Lane {
  return {
    id: nextId("lane"),
    notes: [],
    locked: false,
    ...partial,
  };
}

export function createGroove(
  partial: Omit<Groove, "id" | "lanes"> & { lanes?: Lane[] }
): Groove {
  return {
    id: nextId("groove"),
    lanes: [],
    ...partial,
  };
}

/** Deep clone — lineage nodes each hold an independent full snapshot (§3). */
export function cloneGroove(groove: Groove): Groove {
  return {
    ...groove,
    lanes: groove.lanes.map((lane) => ({
      ...lane,
      outputMapping: { ...lane.outputMapping },
      notes: lane.notes.map((note) => ({ ...note })),
    })),
  };
}

export function findLane(groove: Groove, laneId: string): Lane | undefined {
  return groove.lanes.find((lane) => lane.id === laneId);
}
