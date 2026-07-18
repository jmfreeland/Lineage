import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { FillLibrary } from "../fill.js";
import { EmbellishmentLibrary } from "../embellishment.js";
import type { Fill, Embellishment } from "../types.js";

describe("FillLibrary / EmbellishmentLibrary (JsonFileLibrary)", () => {
  let dir: string;

  beforeEach(async () => {
    dir = await mkdtemp(join(tmpdir(), "lineage-content-library-"));
  });

  afterEach(async () => {
    await rm(dir, { recursive: true, force: true });
  });

  it("saves, loads, lists, and deletes fills", async () => {
    const library = new FillLibrary(dir);
    const fill: Fill = {
      id: "straightFill",
      name: "Straight Fill",
      lengthBars: 1,
      lanes: [{ laneType: "snare", notes: [{ position: 0, pitch: 38, velocity: 110, duration: 0.2 }] }],
    };

    expect(await library.list()).toEqual([]);
    await library.save(fill);
    expect(await library.load(fill.id)).toEqual(fill);
    expect(await library.list()).toEqual([{ id: fill.id, name: fill.name }]);

    await library.delete(fill.id);
    expect(await library.list()).toEqual([]);
  });

  it("saves, loads, lists, and deletes embellishments, independent of the fill library", async () => {
    const library = new EmbellishmentLibrary(dir);
    const embellishment: Embellishment = {
      id: "hatFlutter",
      name: "Hat Flutter",
      laneType: "hihat",
      notes: [{ position: 0.5, pitch: 42, velocity: 40, duration: 0.05 }],
    };

    await library.save(embellishment);
    expect(await library.load(embellishment.id)).toEqual(embellishment);

    const entries = await library.list();
    expect(entries).toEqual([{ id: embellishment.id, name: embellishment.name }]);
  });
});
