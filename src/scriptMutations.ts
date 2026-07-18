import { pathToFileURL } from "node:url";
import { registerMutation } from "./mutation.js";
import type { MutationDefinition } from "./types.js";

function isValidMutationDefinition(candidate: unknown): candidate is MutationDefinition {
  if (!candidate || typeof candidate !== "object") return false;
  const def = candidate as Record<string, unknown>;
  return (
    typeof def.id === "string" &&
    typeof def.label === "string" &&
    Array.isArray(def.params) &&
    typeof def.transform === "function"
  );
}

/**
 * Loads a user-authored mutation from a JS module whose default export
 * matches MutationDefinition — the practical, Node-side form of §2's
 * "embedded JS-like sandbox" decision. This is plain dynamic import(), not
 * an isolated sandbox: fine for scripts you wrote yourself and run
 * locally, which is this engine's whole use case right now. True isolation
 * (e.g. embedding QuickJS) is a concern for when mutations run inside a
 * native plugin host, not before. Scripts must be plain JS (.mjs/.js) since
 * this relies on Node's native loader — no TypeScript compilation step.
 */
export async function loadMutationScript(filePath: string): Promise<MutationDefinition> {
  const moduleUrl = pathToFileURL(filePath).href;
  const mod = (await import(moduleUrl)) as { default?: unknown };
  const candidate = mod.default;
  if (!isValidMutationDefinition(candidate)) {
    throw new Error(
      `Mutation script "${filePath}" must default-export a MutationDefinition (id, label, params, transform)`
    );
  }
  return candidate;
}

export async function loadAndRegisterMutationScript(filePath: string): Promise<MutationDefinition> {
  const definition = await loadMutationScript(filePath);
  registerMutation(definition);
  return definition;
}
