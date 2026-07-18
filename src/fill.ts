import { cloneGroove } from "./groove.js";
import { JsonFileLibrary } from "./jsonFileLibrary.js";
import type { Fill, Groove } from "./types.js";

export class FillLibrary extends JsonFileLibrary<Fill> {}

/**
 * Swaps a fill's curated content into every lane whose type the fill
 * defines content for. There's no arrangement/timeline concept yet (§5 is
 * future work), so a fill isn't "inserted at bar N and then playback
 * resumes the old loop" — it simply becomes this lane's loop content for
 * this lineage node, same as a mutation's output would. Requires each
 * matching lane's loop length to equal the fill's length; a fill authored
 * for a different length needs a matching lane, not resizing on the fly.
 */
export function applyFill(groove: Groove, fill: Fill): Groove {
  const next = cloneGroove(groove);

  for (const content of fill.lanes) {
    for (const lane of next.lanes) {
      if (lane.type !== content.laneType || lane.locked) continue;
      if (lane.loopLengthBars !== fill.lengthBars) {
        throw new Error(
          `Fill "${fill.id}" is ${fill.lengthBars} bar(s) but lane "${lane.id}" loops every ${lane.loopLengthBars} bar(s)`
        );
      }
      lane.notes = content.notes.map((note) => ({ ...note }));
    }
  }

  return next;
}
