import { registerMutation } from "../mutation.js";
import { velocityHumanize } from "./velocityHumanize.js";
import { ghostNote } from "./ghostNote.js";

export const BUILTIN_MUTATIONS = [velocityHumanize, ghostNote];

export function registerBuiltinMutations(): void {
  for (const mutation of BUILTIN_MUTATIONS) {
    registerMutation(mutation);
  }
}
