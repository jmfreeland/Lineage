import type { MutationDefinition, NoteEvent } from "../types.js";
import { isNoteInBarRange } from "./barRange.js";

export const ghostNote: MutationDefinition = {
  id: "ghostNote",
  label: "Ghost Note",
  description: "Probabilistically inserts a quiet ghost note before existing hits.",
  params: [
    {
      key: "probability",
      label: "Probability",
      type: "float",
      default: 0.25,
      min: 0,
      max: 1,
      step: 0.01,
      primary: true,
      macroEligible: true,
    },
    {
      key: "ghostVelocity",
      label: "Ghost Velocity",
      type: "int",
      default: 30,
      min: 1,
      max: 127,
      primary: true,
    },
    {
      key: "offsetBeats",
      label: "Offset (beats)",
      type: "float",
      default: 0.125,
      min: 0.03125,
      max: 0.5,
      step: 0.03125,
    },
  ],
  transform: ({ lane, barRange, referenceBarLengthBeats, rng, params }) => {
    const probability = params.probability as number;
    const ghostVelocity = params.ghostVelocity as number;
    const offsetBeats = params.offsetBeats as number;

    const result: NoteEvent[] = [];
    for (const note of lane.notes) {
      result.push(note);
      if (!isNoteInBarRange(note, referenceBarLengthBeats, lane.loopLengthBars, barRange)) continue;
      if (rng() >= probability) continue;

      const ghostPosition = Math.max(0, note.position - offsetBeats);
      result.push({
        position: ghostPosition,
        pitch: note.pitch,
        velocity: ghostVelocity,
        duration: Math.min(offsetBeats, note.duration),
      });
    }

    return result.sort((a, b) => a.position - b.position);
  },
};
