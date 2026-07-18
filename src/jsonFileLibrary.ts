import { mkdir, readdir, readFile, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";

interface NamedRecord {
  id: string;
  name: string;
}

export interface LibraryEntry {
  id: string;
  name: string;
}

/**
 * Shared directory-of-JSON-files storage for the content libraries (Fill,
 * Embellishment) — one file per record, versioned envelope, plain and
 * git-friendly. Not used for Groove, which already has its own richer
 * (de)serialization in persistence.ts independent of any library.
 */
export class JsonFileLibrary<T extends NamedRecord> {
  constructor(
    private readonly directory: string,
    private readonly schemaVersion: number = 1
  ) {}

  private pathFor(id: string): string {
    return join(this.directory, `${id}.json`);
  }

  async save(record: T): Promise<void> {
    await mkdir(this.directory, { recursive: true });
    const payload = { schemaVersion: this.schemaVersion, record };
    await writeFile(this.pathFor(record.id), JSON.stringify(payload, null, 2), "utf8");
  }

  async load(id: string): Promise<T> {
    const json = await readFile(this.pathFor(id), "utf8");
    const payload = JSON.parse(json) as { schemaVersion: number; record: T };
    if (payload.schemaVersion !== this.schemaVersion) {
      throw new Error(`Unsupported schema version for "${id}": ${payload.schemaVersion}`);
    }
    return payload.record;
  }

  async list(): Promise<LibraryEntry[]> {
    let files: string[];
    try {
      files = await readdir(this.directory);
    } catch (err) {
      if ((err as NodeJS.ErrnoException).code === "ENOENT") return [];
      throw err;
    }

    const entries: LibraryEntry[] = [];
    for (const file of files) {
      if (!file.endsWith(".json")) continue;
      const record = await this.load(file.slice(0, -".json".length));
      entries.push({ id: record.id, name: record.name });
    }
    return entries;
  }

  async delete(id: string): Promise<void> {
    await rm(this.pathFor(id), { force: true });
  }
}
