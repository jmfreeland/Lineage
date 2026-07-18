import { createRng } from "./rng.js";
import { cloneGroove, findLane } from "./groove.js";
import type {
  Groove,
  MutationDefinition,
  MutationParamValues,
  MutationTarget,
} from "./types.js";

const registry = new Map<string, MutationDefinition>();

export function registerMutation(definition: MutationDefinition): void {
  if (registry.has(definition.id)) {
    throw new Error(`Mutation "${definition.id}" is already registered`);
  }
  registry.set(definition.id, definition);
}

export function getMutation(id: string): MutationDefinition {
  const def = registry.get(id);
  if (!def) throw new Error(`Unknown mutation "${id}"`);
  return def;
}

export function listMutations(): MutationDefinition[] {
  return [...registry.values()];
}

/** Fills in any params the caller omitted with the mutation's declared defaults. */
export function resolveParams(
  definition: MutationDefinition,
  params: MutationParamValues = {}
): MutationParamValues {
  const resolved: MutationParamValues = {};
  for (const param of definition.params) {
    resolved[param.key] = params[param.key] ?? param.default;
  }
  return resolved;
}

/**
 * Applies a mutation to the targeted lanes/bar-range of a groove and returns
 * a new groove (targeted lanes replaced, everything else copied through
 * untouched — locked lanes, per §2, are never included in a target).
 */
export function applyMutation(
  groove: Groove,
  mutationId: string,
  target: MutationTarget,
  params: MutationParamValues,
  seed: number
): Groove {
  const definition = getMutation(mutationId);
  const resolvedParams = resolveParams(definition, params);
  const next = cloneGroove(groove);
  const rng = createRng(seed);

  for (const laneId of target.laneIds) {
    const lane = findLane(next, laneId);
    if (!lane) throw new Error(`Mutation target references unknown lane "${laneId}"`);
    if (lane.locked) continue;
    lane.notes = definition.transform({
      lane,
      barRange: target.barRange,
      referenceBarLengthBeats: next.referenceBarLengthBeats,
      rng,
      params: resolvedParams,
    });
  }

  return next;
}
