#!/usr/bin/env python3
"""Generates sample_corpus/rock_groove.mid — a synthetic 8-bar loop (6 base
bars + 2 fill bars at the phrase boundaries) checked in as the "basic test
corpus" this tool's design doc calls for.

This is NOT a substitute for real performed drum MIDI. It exists so
`analyze.py sample_corpus/` produces something immediately, and so the
pipeline's behavior is demonstrated on a file you can open and read the
code for. The vocabulary a real corpus would produce — actual human timing
and velocity variation, real fill vocabulary — needs real recordings; see
README.md's Limitations section.
"""

from __future__ import annotations

from fractions import Fraction
from pathlib import Path

from mido import MetaMessage, MidiFile, MidiTrack, Message, bpm2tempo

TICKS_PER_BEAT = 480
KICK, SNARE, CLOSED_HAT, CRASH = 36, 38, 42, 49


def beat_to_tick(beat: Fraction) -> int:
    return round(beat * TICKS_PER_BEAT)


def basic_bar(bar_start: Fraction, bar_number: int = 0) -> list[tuple[Fraction, int, int, Fraction]]:
    # A little deterministic per-bar micro-timing/velocity jitter on the
    # backbeat snare — small enough (and quantized-position-preserving
    # enough) to stay clustered as the same base pattern, but large enough
    # on 2 of every 3 bars to cross the diff thresholds and show up as real
    # timing_shift/velocity_variation entries. Without this the sample
    # corpus is too robotically perfect to demonstrate the interesting part
    # of the output.
    snare_jitter_ticks = (bar_number % 3) * 4  # 0, 4, 8 ticks -> 0ms, ~5.2ms, ~10.4ms late at 96bpm
    snare_jitter_beats = Fraction(snare_jitter_ticks, TICKS_PER_BEAT)
    snare_velocity = 94 - (bar_number % 3) * 5

    events = [
        (bar_start + 0, KICK, 100, Fraction(1, 4)),
        (bar_start + 1 + snare_jitter_beats, SNARE, snare_velocity, Fraction(1, 4)),
        (bar_start + 2, KICK, 102, Fraction(1, 4)),
        (bar_start + 3, SNARE, 94, Fraction(1, 4)),
    ]
    for eighth in range(8):
        # A touch of natural-looking velocity variation on the hats so the
        # sample isn't perfectly robotic — still well under the diff
        # thresholds, so it doesn't affect base-pattern detection.
        velocity = 68 if eighth % 2 else 74
        events.append((bar_start + Fraction(eighth, 2), CLOSED_HAT, velocity, Fraction(1, 8)))
    return events


def fill_bar(bar_start: Fraction) -> list[tuple[Fraction, int, int, Fraction]]:
    return [
        (bar_start + 0, KICK, 100, Fraction(1, 4)),
        (bar_start + Fraction(3, 4), KICK, 90, Fraction(1, 8)),  # extra kick, off-grid embellishment
        (bar_start + 1, SNARE, 96, Fraction(1, 4)),
        (bar_start + Fraction(5, 4), SNARE, 40, Fraction(1, 8)),  # ghost note
        (bar_start + Fraction(3, 2), SNARE, 100, Fraction(1, 8)),
        (bar_start + 2, SNARE, 105, Fraction(1, 4)),
        (bar_start + Fraction(5, 2), SNARE, 100, Fraction(1, 8)),
        (bar_start + 3, CRASH, 115, Fraction(1, 2)),  # crash replaces the usual snare backbeat
    ]


def build_events() -> list[tuple[Fraction, int, int, Fraction]]:
    events: list[tuple[Fraction, int, int, Fraction]] = []
    for bar in range(8):
        bar_start = Fraction(bar * 4)
        if bar % 4 == 3:  # bars 3 and 7: last bar of each 4-bar phrase
            events.extend(fill_bar(bar_start))
        else:
            events.extend(basic_bar(bar_start, bar_number=bar))
    return events


def write_midi(path: Path, events: list[tuple[Fraction, int, int, Fraction]], tempo_bpm: float = 96.0) -> None:
    mid = MidiFile(ticks_per_beat=TICKS_PER_BEAT)
    track = MidiTrack()
    mid.tracks.append(track)

    abs_events: list[tuple[int, int, str, int, int]] = [
        (0, 0, "tempo", 0, int(bpm2tempo(tempo_bpm))),
        (0, 0, "time_sig", 0, 0),
    ]
    for start_beat, pitch, velocity, duration_beats in events:
        start_tick = beat_to_tick(start_beat)
        end_tick = beat_to_tick(start_beat + duration_beats)
        abs_events.append((start_tick, 1, "note_on", pitch, velocity))
        abs_events.append((end_tick, -1, "note_off", pitch, 0))
    abs_events.sort(key=lambda e: (e[0], e[1]))

    prev_tick = 0
    for tick, _priority, kind, pitch, velocity in abs_events:
        delta = tick - prev_tick
        prev_tick = tick
        if kind == "tempo":
            track.append(MetaMessage("set_tempo", tempo=velocity, time=delta))
        elif kind == "time_sig":
            track.append(MetaMessage("time_signature", numerator=4, denominator=4, time=delta))
        elif kind == "note_on":
            track.append(Message("note_on", note=pitch, velocity=velocity, channel=9, time=delta))
        else:
            track.append(Message("note_off", note=pitch, velocity=0, channel=9, time=delta))

    path.parent.mkdir(parents=True, exist_ok=True)
    mid.save(str(path))


if __name__ == "__main__":
    out = Path(__file__).resolve().parent / "sample_corpus" / "rock_groove.mid"
    write_midi(out, build_events())
    print(f"wrote {out}")
