// Vocabulary-informed embellishment — the engine-side counterpart to
// tools/midi-analysis's "embellishment" category entries (an extra note
// layered on top of an already-fully-present base-pattern position,
// captured per lane voice/bar-position with an observed frequency and,
// once diff.py started recording it, an average velocity). Deliberately
// its own pure transform rather than a registered MutationDefinition or a
// reuse of the ghostNote mutation: ghostNote inserts relative to an
// existing note's own position (offsetBeats before it), while vocabulary
// embellishment inserts at an absolute per-bar position independent of
// any specific existing note — exactly matching how the mined data was
// captured (diff.py's diff_bar() looks at whole-position slots, not
// individual notes). runtime.ts falls back to the ghostNote mutation
// unchanged when no vocabulary is loaded.
import { cloneGroove, nextNoteId } from "./groove.js";
import { createRng } from "./rng.js";
import { normalizeVoice, type Vocabulary } from "./vocabulary.js";
import type { Groove } from "./types.js";

// "Embellishment" vocabulary entries never carry avg_magnitude — diff.py
// treats them as "an extra hit happened here" rather than a variation from
// an expected value, so there's nothing to average against. This is the
// same flavor of quiet accent ghostNote's own default targets.
const DEFAULT_EMBELLISH_VELOCITY = 30;

/**
 * For each targeted lane, and for every "embellishment" vocabulary entry
 * matching that lane's voice, rolls the entry's own observed frequency
 * once per bar in the lane's loop; on a hit, inserts a new note at that
 * entry's bar-fraction position (skipped if a note already sits there —
 * don't double up on an already-present hit) at a velocity sampled around
 * the entry's observed average when available, or a quiet flat default
 * otherwise. Falls through untouched when no matching entry exists, so
 * lanes/voices the vocabulary has no embellishment data for are simply
 * unaffected rather than erroring.
 */
export function applyVocabularyEmbellishment(
  groove: Groove,
  laneIds: string[],
  vocabulary: Vocabulary,
  seed: number
): Groove {
  const next = cloneGroove(groove);
  const rng = createRng(seed);
  const barLength = next.referenceBarLengthBeats > 0 ? next.referenceBarLengthBeats : 4;
  const embellishmentEntries = vocabulary.variations.filter((entry) => entry.category === "embellishment");
  if (embellishmentEntries.length === 0) return next;

  for (const lane of next.lanes) {
    if (!laneIds.includes(lane.id) || lane.locked) continue;
    const matching = embellishmentEntries.filter((entry) => normalizeVoice(entry.voice) === lane.type);
    if (matching.length === 0) continue;

    const loopLengthBeats = lane.loopLengthBars > 0 ? lane.loopLengthBars * barLength : barLength;
    const barCount = Math.max(1, Math.round(loopLengthBeats / barLength));

    for (let bar = 0; bar < barCount; bar += 1) {
      for (const entry of matching) {
        if (rng() >= entry.frequency) continue;
        const position = bar * barLength + entry.metricPosition * barLength;
        if (lane.notes.some((note) => Math.abs(note.position - position) < 1e-3)) continue;

        const velocity =
          entry.avgMagnitude !== undefined
            ? Math.max(1, Math.min(127, Math.round(entry.avgMagnitude * (0.85 + rng() * 0.3))))
            : DEFAULT_EMBELLISH_VELOCITY;
        lane.notes.push({
          id: nextNoteId(),
          position,
          pitch: lane.outputMapping.note,
          velocity,
          duration: 0.1,
        });
      }
    }
    lane.notes.sort((a, b) => a.position - b.position);
  }

  return next;
}
