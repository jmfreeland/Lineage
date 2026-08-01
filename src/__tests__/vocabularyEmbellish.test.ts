import { describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { parseVocabulary, type Vocabulary } from "../vocabulary.js";
import { applyVocabularyEmbellishment } from "../vocabularyEmbellish.js";

function makeVocabulary(overrides: Record<string, unknown>[] = []): Vocabulary {
  return parseVocabulary({
    schema_version: 1,
    source_files: [],
    base_patterns: [],
    variations: [
      {
        category: "embellishment",
        voice: "closed_hihat",
        metric_position: 0.5,
        frequency: 1,
        occurrences: 10,
        avg_magnitude: 40,
      },
      ...overrides,
    ],
    fills: [],
  });
}

function makeGroove(loopLengthBars = 1) {
  const hihat = createLane({
    type: "hihat",
    outputMapping: { note: 42, channel: 1 },
    loopLengthBars,
    notes: [{ id: "test_note_20", position: 0, pitch: 42, velocity: 80, duration: 0.1 }],
  });
  return createGroove({ name: "test", tempo: 120, referenceBarLengthBeats: 4, lanes: [hihat] });
}

describe("applyVocabularyEmbellishment", () => {
  it("inserts a note at the vocabulary entry's bar-fraction position", () => {
    const groove = makeGroove();
    const vocabulary = makeVocabulary();
    const laneId = groove.lanes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 1);
    const notes = result.lanes[0]!.notes;

    expect(notes).toHaveLength(2);
    expect(notes.some((n) => Math.abs(n.position - 2) < 1e-6)).toBe(true); // 0.5 * 4 beats
  });

  it("samples velocity around the entry's average magnitude", () => {
    const groove = makeGroove();
    const vocabulary = makeVocabulary();
    const laneId = groove.lanes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 1);
    const inserted = result.lanes[0]!.notes.find((n) => Math.abs(n.position - 2) < 1e-6)!;

    expect(inserted.velocity).toBeGreaterThanOrEqual(1);
    expect(inserted.velocity).toBeLessThanOrEqual(127);
    // avgMagnitude 40, sampled in [0.85, 1.15]x -> roughly [34, 46].
    expect(inserted.velocity).toBeGreaterThan(30);
    expect(inserted.velocity).toBeLessThan(50);
  });

  it("falls back to a quiet flat velocity when the entry has no avg_magnitude", () => {
    const groove = makeGroove();
    const vocabulary = parseVocabulary({
      schema_version: 1,
      source_files: [],
      base_patterns: [],
      variations: [
        { category: "embellishment", voice: "closed_hihat", metric_position: 0.5, frequency: 1, occurrences: 3 },
      ],
      fills: [],
    });
    const laneId = groove.lanes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 1);
    const inserted = result.lanes[0]!.notes.find((n) => Math.abs(n.position - 2) < 1e-6)!;

    expect(inserted.velocity).toBe(30);
  });

  it("does not double up when a note already sits at the target position", () => {
    const hihat = createLane({
      type: "hihat",
      outputMapping: { note: 42, channel: 1 },
      loopLengthBars: 1,
      notes: [{ id: "test_note_21", position: 2, pitch: 42, velocity: 80, duration: 0.1 }],
    });
    const groove = createGroove({ name: "test", tempo: 120, referenceBarLengthBeats: 4, lanes: [hihat] });
    const vocabulary = makeVocabulary();

    const result = applyVocabularyEmbellishment(groove, [hihat.id], vocabulary, 1);

    expect(result.lanes[0]!.notes).toHaveLength(1);
  });

  it("rolls the entry's frequency once per bar in a multi-bar loop", () => {
    const groove = makeGroove(2);
    const vocabulary = makeVocabulary();
    const laneId = groove.lanes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 1);
    const insertedPositions = result.lanes[0]!.notes.map((n) => n.position).filter((p) => p !== 0);

    // frequency 1 -> guaranteed hit at metric_position 0.5 in both bars: beat 2 and beat 6.
    expect(insertedPositions).toContain(2);
    expect(insertedPositions).toContain(6);
  });

  it("assigns each inserted note a fresh, distinct id", () => {
    const groove = makeGroove();
    const vocabulary = makeVocabulary();
    const laneId = groove.lanes[0]!.id;
    const originalId = groove.lanes[0]!.notes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 1);
    const ids = result.lanes[0]!.notes.map((n) => n.id);

    expect(ids).toContain(originalId);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it("never rolls a hit when frequency is 0", () => {
    const groove = makeGroove();
    const zeroFreqVocabulary = parseVocabulary({
      schema_version: 1,
      source_files: [],
      base_patterns: [],
      variations: [
        { category: "embellishment", voice: "closed_hihat", metric_position: 0.5, frequency: 0, occurrences: 0 },
      ],
      fills: [],
    });
    const laneId = groove.lanes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [laneId], zeroFreqVocabulary, 1);
    expect(result.lanes[0]!.notes).toHaveLength(1);
  });

  it("leaves lanes untouched when no embellishment entry matches their voice", () => {
    const groove = makeGroove();
    const vocabulary = makeVocabulary(); // only has a closed_hihat entry
    const kick = createLane({
      type: "kick",
      outputMapping: { note: 36, channel: 1 },
      loopLengthBars: 1,
      notes: [{ id: "test_note_22", position: 0, pitch: 36, velocity: 100, duration: 0.25 }],
    });
    groove.lanes.push(kick);

    const result = applyVocabularyEmbellishment(groove, [kick.id], vocabulary, 1);
    expect(result.lanes.find((l) => l.type === "kick")!.notes).toEqual(kick.notes);
  });

  it("only affects lanes explicitly targeted", () => {
    const groove = makeGroove();
    const vocabulary = makeVocabulary();
    const untargeted = createLane({
      type: "hihat",
      outputMapping: { note: 46, channel: 1 },
      loopLengthBars: 1,
      notes: [],
      groupId: "hats",
    });
    groove.lanes.push(untargeted);
    const targetedId = groove.lanes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [targetedId], vocabulary, 1);
    const untouchedLane = result.lanes.find((l) => l.id === untargeted.id)!;

    expect(untouchedLane.notes).toEqual([]);
  });

  it("skips locked lanes even when targeted", () => {
    const groove = makeGroove();
    groove.lanes[0]!.locked = true;
    const vocabulary = makeVocabulary();
    const laneId = groove.lanes[0]!.id;

    const result = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 1);
    expect(result.lanes[0]!.notes).toEqual(groove.lanes[0]!.notes);
  });

  it("is deterministic for a given seed (position/velocity — ids are always freshly, globally unique)", () => {
    const groove = makeGroove();
    const vocabulary = makeVocabulary();
    const laneId = groove.lanes[0]!.id;
    const stripIds = (g: ReturnType<typeof applyVocabularyEmbellishment>) =>
      g.lanes.map((lane) => lane.notes.map(({ position, velocity, pitch, duration }) => ({ position, velocity, pitch, duration })));

    const a = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 99);
    const b = applyVocabularyEmbellishment(groove, [laneId], vocabulary, 99);
    expect(stripIds(a)).toEqual(stripIds(b));
  });

  it("leaves the original groove untouched", () => {
    const groove = makeGroove();
    const original = JSON.parse(JSON.stringify(groove));
    const vocabulary = makeVocabulary();

    applyVocabularyEmbellishment(groove, groove.lanes.map((l) => l.id), vocabulary, 1);

    expect(groove).toEqual(original);
  });
});
