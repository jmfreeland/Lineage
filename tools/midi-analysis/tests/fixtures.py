"""Synthetic MIDI file construction for tests — this project's "basic test
corpus" is generated code with known ground truth, rather than real
performed drum recordings (none were available to include), so the
pipeline can be validated deterministically. See README's Limitations
section: a real corpus is still needed to produce a musically meaningful
vocabulary.json.
"""

from __future__ import annotations

from fractions import Fraction
from pathlib import Path

import mido
from mido import MetaMessage, MidiFile, MidiTrack, Message, bpm2tempo

DEFAULT_TICKS_PER_BEAT = 480


def beat_to_tick(beat: Fraction, ticks_per_beat: int = DEFAULT_TICKS_PER_BEAT) -> int:
    return round(beat * ticks_per_beat)


def write_midi(
    path: Path,
    events: list[tuple[Fraction, int, int, Fraction]],
    ticks_per_beat: int = DEFAULT_TICKS_PER_BEAT,
    tempo_bpm: float = 120.0,
    numerator: int = 4,
    denominator: int = 4,
    channel: int = 9,
    tempo_changes: list[tuple[Fraction, float]] | None = None,
) -> Path:
    """events: list of (start_beat, pitch, velocity, duration_beats), all
    as absolute beat positions from the start of the file (i.e. bar_index *
    beats_per_bar + position_within_bar)."""
    mid = MidiFile(ticks_per_beat=ticks_per_beat)
    track = MidiTrack()
    mid.tracks.append(track)

    abs_events: list[tuple[int, int, str, int, int]] = []  # (tick, priority, type, pitch, velocity)
    abs_events.append((0, 0, "tempo", 0, int(bpm2tempo(tempo_bpm))))
    abs_events.append((0, 0, "time_sig", 0, 0))
    if tempo_changes:
        for beat, bpm in tempo_changes:
            abs_events.append((beat_to_tick(beat, ticks_per_beat), 0, "tempo", 0, int(bpm2tempo(bpm))))

    for start_beat, pitch, velocity, duration_beats in events:
        start_tick = beat_to_tick(start_beat, ticks_per_beat)
        end_tick = beat_to_tick(start_beat + duration_beats, ticks_per_beat)
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
            track.append(MetaMessage("time_signature", numerator=numerator, denominator=denominator, time=delta))
        elif kind == "note_on":
            track.append(Message("note_on", note=pitch, velocity=velocity, channel=channel, time=delta))
        else:
            track.append(Message("note_off", note=pitch, velocity=0, channel=channel, time=delta))

    path.parent.mkdir(parents=True, exist_ok=True)
    mid.save(str(path))
    return path


# --- Common drum voices used across tests -----------------------------------
KICK = 36
SNARE = 38
CLOSED_HAT = 42
OPEN_HAT = 46
CRASH = 49


def basic_rock_bar(bar_start: Fraction) -> list[tuple[Fraction, int, int, Fraction]]:
    """kick on 1 & 3, snare on 2 & 4, closed hat on every 8th note."""
    events = [
        (bar_start + 0, KICK, 100, Fraction(1, 4)),
        (bar_start + 1, SNARE, 96, Fraction(1, 4)),
        (bar_start + 2, KICK, 100, Fraction(1, 4)),
        (bar_start + 3, SNARE, 96, Fraction(1, 4)),
    ]
    for eighth in range(8):
        events.append((bar_start + Fraction(eighth, 2), CLOSED_HAT, 70, Fraction(1, 8)))
    return events
