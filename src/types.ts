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
  label: string;
  description?: string;
  params: MutationParam[];
  transform: MutationTransform;
}

export interface LineageNodeProvenance {
  mutationId: string;
  params: MutationParamValues;
  seed: number;
  target: MutationTarget;
}

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
