// Applies vocabulary-informed timing/velocity variation to a groove — the
// engine-side counterpart to tools/midi-analysis's mined vocabulary.json.
// Deliberately not registered as a MutationDefinition (mutation.ts's
// registry): a Vocabulary is a whole dataset, not a flat MutationParam
// value, so it doesn't fit that contract. This sits alongside applyMutation
// as its own pure transform, the same way fills are already a bespoke
// function rather than a registered mutation.
import { cloneGroove } from "./groove.js";
import { createRng } from "./rng.js";
import { findVariation, type Vocabulary } from "./vocabulary.js";
import type { Groove } from "./types.js";

const DEFAULT_TEMPO_FALLBACK_BPM = 120;

/**
 * For each note in the targeted lanes, looks up the nearest timing_shift
 * and velocity_variation vocabulary entries for that lane's voice and bar
 * position, and — with probability equal to the entry's observed
 * frequency — nudges the note by a magnitude sampled around (not fixed to)
 * the entry's average magnitude, in the entry's observed direction. Falls
 * through untouched when no matching entry exists, so lanes/voices the
 * vocabulary has no data for are simply unaffected rather than erroring.
 */
export function applyVocabularyStyle(groove: Groove, laneIds: string[], vocabulary: Vocabulary, seed: number): Groove {
  const next = cloneGroove(groove);
  const rng = createRng(seed);
  const barLength = next.referenceBarLengthBeats > 0 ? next.referenceBarLengthBeats : 4;
  const beatsPerSecond = next.tempo > 0 ? next.tempo / 60 : DEFAULT_TEMPO_FALLBACK_BPM / 60;

  for (const lane of next.lanes) {
    if (!laneIds.includes(lane.id) || lane.locked) continue;
    const loopLengthBeats = lane.loopLengthBars > 0 ? lane.loopLengthBars * barLength : barLength;

    lane.notes = lane.notes.map((note) => {
      const localPosition = ((note.position % barLength) + barLength) % barLength;
      const positionFraction = localPosition / barLength;

      let position = note.position;
      let velocity = note.velocity;

      const timing = findVariation(vocabulary, "timing_shift", lane.type, positionFraction);
      if (timing?.avgMagnitude !== undefined && rng() < timing.frequency) {
        const magnitudeMs = Math.abs(timing.avgMagnitude) * (0.5 + rng());
        const signedMs = timing.direction === "early" ? -magnitudeMs : magnitudeMs;
        const offsetBeats = (signedMs / 1000) * beatsPerSecond;
        position = ((note.position + offsetBeats) % loopLengthBeats + loopLengthBeats) % loopLengthBeats;
      }

      const velocityVariation = findVariation(vocabulary, "velocity_variation", lane.type, positionFraction);
      if (velocityVariation?.avgMagnitude !== undefined && rng() < velocityVariation.frequency) {
        const magnitude = Math.abs(velocityVariation.avgMagnitude) * (0.5 + rng());
        const signed = velocityVariation.direction === "softer" ? -magnitude : magnitude;
        velocity = Math.max(1, Math.min(127, Math.round(note.velocity + signed)));
      }

      return { ...note, position, velocity };
    });
  }

  return next;
}
