import { describe, expect, it } from "vitest";
import { createGroove, createLane } from "../groove.js";
import { parseVocabulary, type Vocabulary } from "../vocabulary.js";
import { applyVocabularyStyle } from "../vocabularyStyle.js";

function makeVocabulary(overrides: Record<string, unknown>[] = []): Vocabulary {
  return parseVocabulary({
    schema_version: 1,
    source_files: [],
    base_patterns: [],
    variations: [
      {
        category: "timing_shift",
        voice: "snare",
        metric_position: 0.25,
        frequency: 1,
        occurrences: 10,
        avg_magnitude: 20,
        direction: "late",
      },
      {
        category: "velocity_variation",
        voice: "kick",
        metric_position: 0,
        frequency: 1,
        occurrences: 10,
        avg_magnitude: 15,
        direction: "louder",
      },
      ...overrides,
    ],
    fills: [],
  });
}

function makeGroove(tempo = 120) {
  const kick = createLane({
    type: "kick",
    outputMapping: { note: 36, channel: 1 },
    loopLengthBars: 1,
    notes: [{ position: 0, pitch: 36, velocity: 100, duration: 0.25 }],
  });
  const snare = createLane({
    type: "snare",
    outputMapping: { note: 38, channel: 1 },
    loopLengthBars: 1,
    notes: [{ position: 1, pitch: 38, velocity: 96, duration: 0.25 }],
  });
  return createGroove({ name: "test", tempo, referenceBarLengthBeats: 4, lanes: [kick, snare] });
}

describe("applyVocabularyStyle", () => {
  it("shifts a note late by roughly the vocabulary's average magnitude, converted to beats", () => {
    const groove = makeGroove(120);
    const vocabulary = makeVocabulary();
    const snareLane = groove.lanes.find((l) => l.type === "snare")!;

    const result = applyVocabularyStyle(groove, [snareLane.id], vocabulary, 1);
    const resultSnare = result.lanes.find((l) => l.type === "snare")!;

    // 120bpm -> 0.5s/beat. 20ms average -> up to 0.5-1.5x spread -> 0.02-0.06 beats late.
    const delta = resultSnare.notes[0]!.position - 1;
    expect(delta).toBeGreaterThan(0);
    expect(delta).toBeLessThan(0.1);
  });

  it("increases velocity for a matched velocity_variation entry", () => {
    const groove = makeGroove(120);
    const vocabulary = makeVocabulary();
    const kickLane = groove.lanes.find((l) => l.type === "kick")!;

    const result = applyVocabularyStyle(groove, [kickLane.id], vocabulary, 2);
    const resultKick = result.lanes.find((l) => l.type === "kick")!;

    expect(resultKick.notes[0]!.velocity).toBeGreaterThan(100);
    expect(resultKick.notes[0]!.velocity).toBeLessThanOrEqual(127);
  });

  it("leaves notes untouched when no vocabulary entry matches their voice", () => {
    const groove = makeGroove(120);
    const vocabulary = makeVocabulary(); // only has snare/kick entries
    const hihat = createLane({
      type: "hihat",
      outputMapping: { note: 42, channel: 1 },
      loopLengthBars: 1,
      notes: [{ position: 2, pitch: 42, velocity: 70, duration: 0.1 }],
    });
    groove.lanes.push(hihat);

    const result = applyVocabularyStyle(groove, [hihat.id], vocabulary, 3);
    const resultHat = result.lanes.find((l) => l.type === "hihat")!;

    expect(resultHat.notes[0]).toEqual(hihat.notes[0]);
  });

  it("only affects lanes explicitly targeted", () => {
    const groove = makeGroove(120);
    const vocabulary = makeVocabulary();
    const kickLane = groove.lanes.find((l) => l.type === "kick")!;
    const snareLane = groove.lanes.find((l) => l.type === "snare")!;

    const result = applyVocabularyStyle(groove, [kickLane.id], vocabulary, 4);
    const resultSnare = result.lanes.find((l) => l.type === "snare")!;

    expect(resultSnare.notes).toEqual(snareLane.notes);
  });

  it("skips locked lanes even when targeted", () => {
    const groove = makeGroove(120);
    groove.lanes.find((l) => l.type === "kick")!.locked = true;
    const vocabulary = makeVocabulary();
    const kickLane = groove.lanes.find((l) => l.type === "kick")!;

    const result = applyVocabularyStyle(groove, [kickLane.id], vocabulary, 5);
    expect(result.lanes.find((l) => l.type === "kick")!.notes).toEqual(kickLane.notes);
  });

  it("is deterministic for a given seed", () => {
    const groove = makeGroove(120);
    const vocabulary = makeVocabulary();
    const laneIds = groove.lanes.map((l) => l.id);

    const a = applyVocabularyStyle(groove, laneIds, vocabulary, 42);
    const b = applyVocabularyStyle(groove, laneIds, vocabulary, 42);
    expect(a).toEqual(b);
  });

  it("leaves the original groove untouched", () => {
    const groove = makeGroove(120);
    const original = JSON.parse(JSON.stringify(groove));
    const vocabulary = makeVocabulary();

    applyVocabularyStyle(groove, groove.lanes.map((l) => l.id), vocabulary, 6);

    expect(groove).toEqual(original);
  });

  it("wraps a position shifted past the lane's loop boundary", () => {
    const kick = createLane({
      type: "kick",
      outputMapping: { note: 36, channel: 1 },
      loopLengthBars: 1,
      notes: [{ position: 3.99, pitch: 36, velocity: 100, duration: 0.1 }],
    });
    const groove = createGroove({ name: "wrap-test", tempo: 120, referenceBarLengthBeats: 4, lanes: [kick] });
    const vocabulary = parseVocabulary({
      schema_version: 1,
      source_files: [],
      base_patterns: [],
      variations: [
        {
          category: "timing_shift",
          voice: "kick",
          metric_position: 0.9975, // matches localPosition 3.99 / 4
          frequency: 1,
          occurrences: 5,
          avg_magnitude: 200, // large enough to push past the loop boundary
          direction: "late",
        },
      ],
      fills: [],
    });

    const result = applyVocabularyStyle(groove, [kick.id], vocabulary, 7);
    const position = result.lanes[0]!.notes[0]!.position;
    expect(position).toBeGreaterThanOrEqual(0);
    expect(position).toBeLessThan(4);
  });
});
