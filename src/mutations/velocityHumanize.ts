import type { MutationDefinition } from "../types.js";
import { isNoteInBarRange } from "./barRange.js";

export const velocityHumanize: MutationDefinition = {
  id: "velocityHumanize",
  version: 1,
  label: "Velocity Humanize",
  description: "Nudges note velocities randomly within a range.",
  params: [
    {
      key: "amount",
      label: "Amount",
      type: "int",
      default: 12,
      min: 1,
      max: 40,
      primary: true,
      macroEligible: true,
    },
    {
      key: "probability",
      label: "Probability",
      type: "float",
      default: 0.8,
      min: 0,
      max: 1,
      step: 0.01,
      primary: true,
    },
  ],
  transform: ({ lane, barRange, referenceBarLengthBeats, rng, params }) => {
    const amount = params.amount as number;
    const probability = params.probability as number;

    return lane.notes.map((note) => {
      if (!isNoteInBarRange(note, referenceBarLengthBeats, lane.loopLengthBars, barRange)) return note;
      if (rng() >= probability) return note;

      const delta = Math.round((rng() * 2 - 1) * amount);
      const velocity = Math.min(127, Math.max(1, note.velocity + delta));
      return { ...note, velocity };
    });
  },
};
