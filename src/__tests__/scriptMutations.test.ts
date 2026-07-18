import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { applyMutation } from "../mutation.js";
import { loadAndRegisterMutationScript, loadMutationScript } from "../scriptMutations.js";

const VALID_SCRIPT = `
export default {
  id: "scriptedFlatten",
  label: "Scripted Flatten",
  params: [{ key: "velocity", label: "Velocity", type: "int", default: 60, min: 1, max: 127 }],
  transform: ({ lane, params }) => lane.notes.map((n) => ({ ...n, velocity: params.velocity })),
};
`;

const INVALID_SCRIPT = `export default { id: "missingTransform", label: "Nope", params: [] };`;

function makeGroove() {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [{ position: 0, pitch: 36, velocity: 100, duration: 0.25 }],
  });
  return createGroove({ name: "Test", tempo: 120, referenceBarLengthBeats: 4, lanes: [kick] });
}

describe("loadMutationScript", () => {
  let dir: string;

  beforeEach(async () => {
    dir = await mkdtemp(join(tmpdir(), "lineage-scripts-"));
  });

  afterEach(async () => {
    await rm(dir, { recursive: true, force: true });
  });

  it("loads a valid mutation script's default export", async () => {
    const filePath = join(dir, "flatten.mjs");
    await writeFile(filePath, VALID_SCRIPT, "utf8");

    const definition = await loadMutationScript(filePath);

    expect(definition.id).toBe("scriptedFlatten");
    expect(typeof definition.transform).toBe("function");
  });

  it("rejects a script missing required fields", async () => {
    const filePath = join(dir, "invalid.mjs");
    await writeFile(filePath, INVALID_SCRIPT, "utf8");

    await expect(loadMutationScript(filePath)).rejects.toThrow(/MutationDefinition/);
  });

  it("loads, registers, and can immediately apply a scripted mutation", async () => {
    const filePath = join(dir, "flatten2.mjs");
    await writeFile(filePath, VALID_SCRIPT.replace("scriptedFlatten", "scriptedFlatten2"), "utf8");

    await loadAndRegisterMutationScript(filePath);

    const groove = makeGroove();
    const target = { laneIds: [groove.lanes[0]!.id], barRange: { start: 0, end: 1 } };
    const result = applyMutation(groove, "scriptedFlatten2", target, { velocity: 42 }, 1);

    expect(result.lanes[0]!.notes[0]!.velocity).toBe(42);
  });
});
