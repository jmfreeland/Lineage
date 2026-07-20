import { describe, expect, it } from "vitest";
import { isNoteInBarRange } from "../mutations/barRange.js";
import type { NoteEvent } from "../types.js";

describe("isNoteInBarRange", () => {
  const referenceBarLengthBeats = 4;
  const loopLengthBars = 3;
  // Local bar 0 of a 3-bar-loop lane repeats at absolute bars 0, 3, 6, 9, ...
  const note: NoteEvent = { id: "test_note_1", position: 0, pitch: 42, velocity: 100, duration: 0.25 };

  it("matches a repeat occurrence that falls inside the target range", () => {
    expect(isNoteInBarRange(note, referenceBarLengthBeats, loopLengthBars, { start: 3, end: 4 })).toBe(true);
    expect(isNoteInBarRange(note, referenceBarLengthBeats, loopLengthBars, { start: 6, end: 7 })).toBe(true);
  });

  it("does not match a range that falls between repeat occurrences", () => {
    // Occurrences are at 0, 3, 6, 9 — bar 5 lands between the 3 and 6 occurrences.
    expect(isNoteInBarRange(note, referenceBarLengthBeats, loopLengthBars, { start: 5, end: 6 })).toBe(false);
  });

  it("matches the first occurrence at bar 0", () => {
    expect(isNoteInBarRange(note, referenceBarLengthBeats, loopLengthBars, { start: 0, end: 1 })).toBe(true);
  });

  it("matches when a wide range spans multiple repeats", () => {
    expect(isNoteInBarRange(note, referenceBarLengthBeats, loopLengthBars, { start: 4, end: 10 })).toBe(true);
  });
});
