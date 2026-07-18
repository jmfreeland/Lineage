import type { NoteEvent } from "../types.js";

/**
 * A lane's notes are stored once, loop-relative, but the lane repeats every
 * `loopLengthBars` on the master grid — so a note occurs at every
 * `localBar + k * loopLengthBars` for integer k (§4 independent lane
 * lengths, e.g. a 3-bar hi-hat lane against a 4-bar target range). A note is
 * "in range" if any of its repeat occurrences falls in the target range.
 */
export function isNoteInBarRange(
  note: NoteEvent,
  referenceBarLengthBeats: number,
  loopLengthBars: number,
  barRange: { start: number; end: number }
): boolean {
  const localBar = note.position / referenceBarLengthBeats;
  const kMin = Math.floor((barRange.start - localBar) / loopLengthBars);
  const kMax = Math.ceil((barRange.end - localBar) / loopLengthBars);

  for (let k = kMin; k <= kMax; k += 1) {
    const absoluteBar = k * loopLengthBars + localBar;
    if (absoluteBar >= barRange.start && absoluteBar < barRange.end) return true;
  }
  return false;
}
