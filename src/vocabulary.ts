// Consumes the vocabulary.json produced by tools/midi-analysis (see that
// tool's README for the mining side). This module only reads the data —
// nothing here writes it. Deliberately no Node dependency (no fs), so it's
// usable from src/runtime.ts inside the embedded plugin runtime, not just
// host-side tooling: the plugin receives vocabulary JSON as a string over
// the bridge and parses it here.
import type { LaneType } from "./types.js";

export interface VocabularyVariation {
  category: string;
  /** Raw voice string from the analysis tool (e.g. "closed_hihat"), not yet normalized to a LaneType. */
  voice: string;
  /** Fraction through the bar, 0 (inclusive) to 1 (exclusive). */
  metricPosition: number;
  frequency: number;
  occurrences: number;
  avgMagnitude?: number;
  direction?: string;
}

export interface Vocabulary {
  schemaVersion: number;
  variations: VocabularyVariation[];
}

/**
 * tools/midi-analysis's voice categories are finer-grained than the
 * engine's LaneType (e.g. closed/open/pedal hi-hat all collapse to
 * "hihat" here) — this is where that collapsing happens, once, rather
 * than at every lookup call site.
 */
const VOICE_TO_LANE_TYPE: Record<string, LaneType> = {
  kick: "kick",
  snare: "snare",
  closed_hihat: "hihat",
  open_hihat: "hihat",
  pedal_hihat: "hihat",
  tom: "tom",
  ride: "ride",
  crash: "crash",
  clap: "perc",
  perc: "perc",
  other: "other",
};

export function normalizeVoice(voice: string): LaneType {
  return VOICE_TO_LANE_TYPE[voice] ?? "other";
}

class VocabularyParseError extends Error {}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

/** Throws VocabularyParseError on anything that doesn't look like a vocabulary.json. */
export function parseVocabulary(data: unknown): Vocabulary {
  if (!isRecord(data)) throw new VocabularyParseError("vocabulary must be an object");
  if (typeof data.schema_version !== "number") {
    throw new VocabularyParseError("vocabulary is missing a numeric schema_version");
  }
  if (!Array.isArray(data.variations)) {
    throw new VocabularyParseError("vocabulary is missing a variations array");
  }

  const variations: VocabularyVariation[] = data.variations.map((entry, index) => {
    if (!isRecord(entry)) throw new VocabularyParseError(`variations[${index}] is not an object`);
    const category = entry.category;
    const voice = entry.voice;
    const metricPosition = entry.metric_position;
    const frequency = entry.frequency;
    if (typeof category !== "string" || typeof voice !== "string") {
      throw new VocabularyParseError(`variations[${index}] is missing category/voice`);
    }
    if (typeof metricPosition !== "number" || typeof frequency !== "number") {
      throw new VocabularyParseError(`variations[${index}] is missing metric_position/frequency`);
    }
    return {
      category,
      voice,
      metricPosition,
      frequency,
      occurrences: typeof entry.occurrences === "number" ? entry.occurrences : 0,
      avgMagnitude: typeof entry.avg_magnitude === "number" ? entry.avg_magnitude : undefined,
      direction: typeof entry.direction === "string" ? entry.direction : undefined,
    };
  });

  return { schemaVersion: data.schema_version, variations };
}

/**
 * Nearest-match lookup: the strongest (bar-fraction-)closest entry for a
 * given category and lane type, within `tolerance` of positionFraction.
 * Ties broken by highest frequency, so a well-attested variation wins over
 * a rarely-seen one at a similar position.
 */
export function findVariation(
  vocabulary: Vocabulary,
  category: string,
  laneType: LaneType,
  positionFraction: number,
  tolerance = 0.05
): VocabularyVariation | undefined {
  let best: VocabularyVariation | undefined;
  let bestDistance = Infinity;

  for (const entry of vocabulary.variations) {
    if (entry.category !== category) continue;
    if (normalizeVoice(entry.voice) !== laneType) continue;

    const raw = Math.abs(entry.metricPosition - positionFraction);
    const distance = Math.min(raw, 1 - raw); // bar wraps: position 0.98 is close to 0.02
    if (distance > tolerance) continue;

    if (distance < bestDistance || (distance === bestDistance && entry.frequency > (best?.frequency ?? -1))) {
      best = entry;
      bestDistance = distance;
    }
  }

  return best;
}
