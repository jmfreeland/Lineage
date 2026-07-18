import { cloneGroove, findLane } from "./groove.js";
import { JsonFileLibrary } from "./jsonFileLibrary.js";
import type { Embellishment, Groove } from "./types.js";

export class EmbellishmentLibrary extends JsonFileLibrary<Embellishment> {}

/**
 * Layers an embellishment's notes onto an existing lane at a chosen bar,
 * additively — unlike applyFill, existing notes are kept, not replaced.
 * That's the meaningful difference between the two: a fill is a moment's
 * full content, an embellishment is an ornament added on top of whatever's
 * already there.
 */
export function applyEmbellishment(
  groove: Groove,
  embellishment: Embellishment,
  laneId: string,
  atBar: number
): Groove {
  const next = cloneGroove(groove);
  const lane = findLane(next, laneId);
  if (!lane) throw new Error(`Unknown lane "${laneId}"`);
  if (lane.type !== embellishment.laneType) {
    throw new Error(
      `Embellishment "${embellishment.id}" is for ${embellishment.laneType} lanes, but "${laneId}" is ${lane.type}`
    );
  }
  if (lane.locked) return next;

  const offsetBeats = atBar * next.referenceBarLengthBeats;
  const inserted = embellishment.notes.map((note) => ({
    ...note,
    position: note.position + offsetBeats,
  }));

  lane.notes = [...lane.notes, ...inserted].sort((a, b) => a.position - b.position);
  return next;
}
