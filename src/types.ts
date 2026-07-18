// Core data model — see DESIGN.md §4 (lanes) and §3 (lineage tree).

export type LaneType =
  | "kick"
  | "snare"
  | "hihat"
  | "tom"
  | "ride"
  | "crash"
  | "perc"
  | "other";

export interface NoteEvent {
  /** Position in beats, relative to the start of the lane's own loop. */
  position: number;
  /** MIDI note number, or lane-defined pitch for non-drum lanes. */
  pitch: number;
  velocity: number; // 0-127
  duration: number; // in beats
}

export interface OutputMapping {
  note: number;
  channel: number;
}

export interface Lane {
  id: string;
  type: LaneType;
  label?: string;
  /** Optional semantic link for joint targeting (e.g. closed/open hats). */
  groupId?: string;
  outputMapping: OutputMapping;
  loopLengthBars: number;
  notes: NoteEvent[];
  locked: boolean;
}

export interface Groove {
  id: string;
  name: string;
  tempo: number; // BPM
  /** Master reference bar length; mutation bar ranges are addressed against this grid. */
  referenceBarLengthBeats: number;
  lanes: Lane[];
}

/** Which lanes and which bar range (in master-grid bars) a mutation applies to. */
export interface MutationTarget {
  laneIds: string[];
  barRange: { start: number; end: number };
}

export type ParamType = "float" | "int" | "bool" | "enum";

export interface MutationParam {
  key: string;
  label: string;
  type: ParamType;
  default: number | boolean | string;
  min?: number;
  max?: number;
  step?: number;
  options?: string[]; // for enum
  /** Whether this param can be promoted to a live macro/MIDI-learn control (§9). */
  macroEligible?: boolean;
  /** Show in the default (non-advanced) view. */
  primary?: boolean;
}

export type MutationParamValues = Record<string, number | boolean | string>;

/**
 * A mutation transform: pure given its inputs, so the same seed always
 * produces the same output (§7 reproducibility).
 */
export type MutationTransform = (args: {
  lane: Lane;
  barRange: { start: number; end: number };
  referenceBarLengthBeats: number;
  rng: () => number;
  params: MutationParamValues;
}) => NoteEvent[];

export interface MutationDefinition {
  id: string;
  /**
   * Bumped whenever `transform`'s behavior changes for the same inputs.
   * Recorded in provenance (MutationProvenance/LiveSessionPass) so a node's
   * genome always names the exact mutation version that produced it —
   * without this, editing a mutation's logic later would silently change
   * what an old lineage node "means" (§2/§3's replay-drift concern).
   */
  version: number;
  label: string;
  description?: string;
  params: MutationParam[];
  transform: MutationTransform;
}

export interface MutationProvenance {
  type: "mutation";
  mutationId: string;
  mutationVersion: number;
  params: MutationParamValues;
  seed: number;
  target: MutationTarget;
}

/** One mutation pass applied during a live-loop session (§10). */
export interface LiveSessionPass {
  mutationId: string;
  mutationVersion: number;
  params: MutationParamValues;
  seed: number;
  target: MutationTarget;
}

/**
 * A node committed from a live-loop session (§10) can be the result of many
 * mutation passes since the anchor, not one — recording the full sequence
 * keeps genome traceability honest instead of collapsing it to "last pass
 * only."
 */
export interface LiveSessionProvenance {
  type: "liveSession";
  anchorNodeId: string;
  passes: LiveSessionPass[];
}

/**
 * A node captured directly from live-played MIDI (§11's plugin bridge) —
 * not derived from a parent via a mutation or live-session evolution, just
 * "this is what got played." Distinct provenance kind so the genome stays
 * honest about where a node actually came from.
 */
export interface RecordedProvenance {
  type: "recorded";
  capturedAtMs: number;
}

/** A tree step chosen from the four high-level weighted rule outcomes. */
export interface RuleProvenance {
  type: "rule";
  ruleId: string;
  operation: "mutation" | "embellish" | "fill" | "hold";
  seed: number;
  weights: {
    mutation: number;
    embellish: number;
    fill: number;
    hold: number;
  };
}

export type LineageNodeProvenance =
  | MutationProvenance
  | LiveSessionProvenance
  | RecordedProvenance
  | RuleProvenance;

export interface LineageNode {
  id: string;
  parentId: string | null;
  groove: Groove; // full snapshot, see DESIGN.md §3
  provenance: LineageNodeProvenance | null; // null for the root node
  createdAt: number;
}

/**
 * Curated (not generated) full-kit content, typically swapped in for a
 * phrase-ending bar or two — distinct from a mutation, which transforms
 * existing notes rather than supplying fixed content (§1, §2).
 */
export interface FillLaneContent {
  laneType: LaneType;
  notes: NoteEvent[];
}

export interface Fill {
  id: string;
  name: string;
  genre?: string;
  lengthBars: number;
  lanes: FillLaneContent[];
}

/**
 * Curated, single-lane ornamentation layered onto existing notes at a
 * chosen insertion point — additive, unlike a Fill's full-content swap.
 */
export interface Embellishment {
  id: string;
  name: string;
  genre?: string;
  laneType: LaneType;
  /** Loop-relative to the embellishment's own insertion point (0 = the target bar). */
  notes: NoteEvent[];
}
