import { mkdir, readdir, readFile, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import type { Groove } from "./types.js";
import { deserializeGroove, serializeGroove } from "./persistence.js";

export interface GrooveLibraryEntry {
  id: string;
  name: string;
}

/**
 * File-based groove library (§1 — visible/accessible across any preset,
 * not siloed per-patch). One JSON file per groove: plain, inspectable, and
 * git-friendly for a personal collection.
 */
export class GrooveLibrary {
  constructor(private readonly directory: string) {}

  private pathFor(id: string): string {
    return join(this.directory, `${id}.json`);
  }

  async save(groove: Groove): Promise<void> {
    await mkdir(this.directory, { recursive: true });
    await writeFile(this.pathFor(groove.id), serializeGroove(groove), "utf8");
  }

  async load(id: string): Promise<Groove> {
    const json = await readFile(this.pathFor(id), "utf8");
    return deserializeGroove(json);
  }

  async list(): Promise<GrooveLibraryEntry[]> {
    let files: string[];
    try {
      files = await readdir(this.directory);
    } catch (err) {
      if ((err as NodeJS.ErrnoException).code === "ENOENT") return [];
      throw err;
    }

    const entries: GrooveLibraryEntry[] = [];
    for (const file of files) {
      if (!file.endsWith(".json")) continue;
      const groove = await this.load(file.slice(0, -".json".length));
      entries.push({ id: groove.id, name: groove.name });
    }
    return entries;
  }

  async delete(id: string): Promise<void> {
    await rm(this.pathFor(id), { force: true });
  }
}
