import type { NoteEvent } from "../types.js";

/**
 * Maps a loop-relative note position onto the master bar grid. Real
 * independent-length lane phasing (§4 — a 3-bar lane against a 4-bar lane)
 * needs repeat/phase-aware mapping across multiple loop cycles; this
 * simplified version checks the note's position directly against the
 * range, which is correct as long as the target range fits within a single
 * pass of the lane's own loop. Revisit once bar-range targeting needs to
 * span multiple independent-length repeats.
 */
export function isNoteInBarRange(
  note: NoteEvent,
  referenceBarLengthBeats: number,
  barRange: { start: number; end: number }
): boolean {
  const noteBar = note.position / referenceBarLengthBeats;
  return noteBar >= barRange.start && noteBar < barRange.end;
}
