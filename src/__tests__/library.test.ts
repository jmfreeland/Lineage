import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { GrooveLibrary } from "../library.js";

function makeGroove(name: string) {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [{ id: "test_note_5", position: 0, pitch: 36, velocity: 100, duration: 0.25 }],
  });
  return createGroove({ name, tempo: 120, referenceBarLengthBeats: 4, lanes: [kick] });
}

describe("GrooveLibrary", () => {
  let dir: string;
  let library: GrooveLibrary;

  beforeEach(async () => {
    dir = await mkdtemp(join(tmpdir(), "lineage-library-"));
    library = new GrooveLibrary(dir);
  });

  afterEach(async () => {
    await rm(dir, { recursive: true, force: true });
  });

  it("returns an empty list before anything is saved", async () => {
    expect(await library.list()).toEqual([]);
  });

  it("saves and loads a groove by id", async () => {
    const groove = makeGroove("My Groove");
    await library.save(groove);

    expect(await library.load(groove.id)).toEqual(groove);
  });

  it("lists saved grooves by id and name", async () => {
    const a = makeGroove("Groove A");
    const b = makeGroove("Groove B");
    await library.save(a);
    await library.save(b);

    const entries = await library.list();
    expect(entries).toHaveLength(2);
    expect(entries).toEqual(
      expect.arrayContaining([
        { id: a.id, name: "Groove A" },
        { id: b.id, name: "Groove B" },
      ])
    );
  });

  it("deletes a groove", async () => {
    const groove = makeGroove("Temporary");
    await library.save(groove);
    await library.delete(groove.id);

    expect(await library.list()).toEqual([]);
  });
});
