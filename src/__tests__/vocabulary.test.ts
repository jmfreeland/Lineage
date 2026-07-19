import { describe, expect, it } from "vitest";
import { findVariation, normalizeVoice, parseVocabulary } from "../vocabulary.js";

function sampleJson() {
  return {
    schema_version: 1,
    source_files: ["a.mid"],
    base_patterns: [],
    variations: [
      {
        category: "timing_shift",
        voice: "snare",
        metric_position: 0.25,
        frequency: 0.6667,
        occurrences: 4,
        avg_magnitude: 7.8,
        direction: "late",
      },
      {
        category: "velocity_variation",
        voice: "closed_hihat",
        metric_position: 0.5,
        frequency: 0.3,
        occurrences: 2,
        avg_magnitude: -12,
        direction: "softer",
      },
    ],
    fills: [],
  };
}

describe("parseVocabulary", () => {
  it("parses a well-formed vocabulary.json", () => {
    const vocabulary = parseVocabulary(sampleJson());
    expect(vocabulary.schemaVersion).toBe(1);
    expect(vocabulary.variations).toHaveLength(2);
    expect(vocabulary.variations[0]).toMatchObject({
      category: "timing_shift",
      voice: "snare",
      metricPosition: 0.25,
      frequency: 0.6667,
      avgMagnitude: 7.8,
      direction: "late",
    });
  });

  it("tolerates missing optional fields", () => {
    const json = sampleJson();
    delete (json.variations[0] as Record<string, unknown>).avg_magnitude;
    delete (json.variations[0] as Record<string, unknown>).direction;
    const vocabulary = parseVocabulary(json);
    expect(vocabulary.variations[0]!.avgMagnitude).toBeUndefined();
    expect(vocabulary.variations[0]!.direction).toBeUndefined();
  });

  it("rejects non-objects", () => {
    expect(() => parseVocabulary("not an object")).toThrow(/object/);
    expect(() => parseVocabulary(null)).toThrow(/object/);
  });

  it("rejects a missing schema_version", () => {
    const json = sampleJson();
    delete (json as Record<string, unknown>).schema_version;
    expect(() => parseVocabulary(json)).toThrow(/schema_version/);
  });

  it("rejects a missing variations array", () => {
    const json = sampleJson();
    delete (json as Record<string, unknown>).variations;
    expect(() => parseVocabulary(json)).toThrow(/variations/);
  });

  it("rejects a malformed variation entry", () => {
    const json = sampleJson();
    (json.variations[0] as Record<string, unknown>).frequency = "not a number";
    expect(() => parseVocabulary(json)).toThrow(/variations\[0\]/);
  });
});

describe("normalizeVoice", () => {
  it("collapses fine-grained hi-hat articulations to hihat", () => {
    expect(normalizeVoice("closed_hihat")).toBe("hihat");
    expect(normalizeVoice("open_hihat")).toBe("hihat");
    expect(normalizeVoice("pedal_hihat")).toBe("hihat");
  });

  it("maps unrecognized voices to other", () => {
    expect(normalizeVoice("cowbell-solo")).toBe("other");
  });

  it("passes through voices that already match a LaneType", () => {
    expect(normalizeVoice("kick")).toBe("kick");
    expect(normalizeVoice("crash")).toBe("crash");
  });
});

describe("findVariation", () => {
  it("finds an exact-position match", () => {
    const vocabulary = parseVocabulary(sampleJson());
    const match = findVariation(vocabulary, "timing_shift", "snare", 0.25);
    expect(match?.avgMagnitude).toBe(7.8);
  });

  it("finds a nearby match within tolerance", () => {
    const vocabulary = parseVocabulary(sampleJson());
    const match = findVariation(vocabulary, "timing_shift", "snare", 0.27, 0.05);
    expect(match).toBeDefined();
  });

  it("returns undefined outside tolerance", () => {
    const vocabulary = parseVocabulary(sampleJson());
    const match = findVariation(vocabulary, "timing_shift", "snare", 0.5, 0.05);
    expect(match).toBeUndefined();
  });

  it("matches hi-hat articulations to a plain 'hihat' lookup", () => {
    const vocabulary = parseVocabulary(sampleJson());
    const match = findVariation(vocabulary, "velocity_variation", "hihat", 0.5);
    expect(match?.direction).toBe("softer");
  });

  it("wraps around the bar boundary", () => {
    const json = sampleJson();
    json.variations.push({
      category: "timing_shift",
      voice: "kick",
      metric_position: 0.99,
      frequency: 0.5,
      occurrences: 1,
      avg_magnitude: 3,
      direction: "early",
    });
    const vocabulary = parseVocabulary(json);
    const match = findVariation(vocabulary, "timing_shift", "kick", 0.01, 0.05);
    expect(match).toBeDefined();
    expect(match?.avgMagnitude).toBe(3);
  });

  it("returns undefined for a category/voice with no entries", () => {
    const vocabulary = parseVocabulary(sampleJson());
    expect(findVariation(vocabulary, "embellishment", "tom", 0.5)).toBeUndefined();
  });
});
