"""MIDI loading and normalization.

Deliberately keeps bar/beat math tempo-independent: a "beat" is
tick / ticks_per_beat, which MIDI defines as fixed for the whole file
regardless of tempo changes. Tempo only matters for turning a beat position
into a real-world millisecond offset (for human-readable timing-shift
reporting) — quantization, bar boundaries, and pattern clustering never
need it. That sidesteps most of the "what about tempo changes" problem
raised in the design doc rather than solving it in general; a file with a
tempo *ramp* (as opposed to discrete changes) would still report accurate
ms offsets (tick_to_seconds integrates the whole tempo map), but that's
about as far as this goes.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path

import mido

from .drum_map import NoteMap, voice_for_pitch

DEFAULT_TEMPO_USEC = 500_000  # 120 BPM, MIDI's own default when unspecified
GM_PERCUSSION_CHANNEL = 9  # "channel 10" in 1-based DAW terminology


@dataclass
class NoteEvent:
    pitch: int
    gm_name: str
    voice: str
    channel: int
    velocity: int
    start_tick: int
    end_tick: int
    start_beat: Fraction
    duration_beats: Fraction


@dataclass
class ParsedMidi:
    source_path: str
    ticks_per_beat: int
    beats_per_bar: Fraction
    notes: list[NoteEvent] = field(default_factory=list)
    tempo_map: list[tuple[int, int]] = field(default_factory=list)  # (tick, usec_per_beat)

    def tick_to_seconds(self, tick: int) -> float:
        seconds = 0.0
        prev_tick, prev_tempo = self.tempo_map[0]
        for next_tick, next_tempo in self.tempo_map[1:]:
            if tick <= next_tick:
                break
            seconds += (next_tick - prev_tick) * prev_tempo / (self.ticks_per_beat * 1_000_000)
            prev_tick, prev_tempo = next_tick, next_tempo
        seconds += (tick - prev_tick) * prev_tempo / (self.ticks_per_beat * 1_000_000)
        return seconds

    def beats_to_ms(self, beats: Fraction) -> float:
        """Duration in ms of a beat-length span, at the tempo in effect at tick 0.
        Used only for reporting small relative offsets (e.g. quantization
        deltas), where "which tempo" rarely matters in practice — a file
        with wildly different tempos at different points would need
        per-instance conversion instead, which callers can still do via
        tick_to_seconds on the two absolute ticks."""
        _, tempo = self.tempo_map[0]
        seconds_per_beat = tempo / 1_000_000
        return float(beats) * seconds_per_beat * 1000.0


def _build_tempo_map(merged: list[mido.Message]) -> list[tuple[int, int]]:
    tempo_map: list[tuple[int, int]] = []
    absolute_tick = 0
    for msg in merged:
        absolute_tick += msg.time
        if msg.is_meta and msg.type == "set_tempo":
            tempo_map.append((absolute_tick, msg.tempo))
    if not tempo_map or tempo_map[0][0] != 0:
        tempo_map.insert(0, (0, DEFAULT_TEMPO_USEC))
    return tempo_map


def _time_signature(merged: list[mido.Message]) -> Fraction:
    absolute_tick = 0
    for msg in merged:
        absolute_tick += msg.time
        if msg.is_meta and msg.type == "time_signature":
            # Beats-per-bar in quarter-note terms, matching the convention
            # used in the plugin (PluginProcessor.cpp): numerator * 4/denominator.
            return Fraction(msg.numerator) * Fraction(4, msg.denominator)
    return Fraction(4)  # default 4/4


def parse_midi_file(
    path: str | Path,
    prefer_channel: int | None = GM_PERCUSSION_CHANNEL,
    note_map: NoteMap | None = None,
) -> ParsedMidi:
    """Parses one MIDI file into normalized NoteEvents.

    If `prefer_channel` has any note events, only that channel is used
    (the standard GM percussion channel). Otherwise falls back to every
    channel in the file — many exported drum-only loops don't bother
    setting channel 10, since there's nothing else in the file to
    disambiguate from.

    `note_map` (see drum_map.load_note_map()) overrides the built-in GM
    pitch->voice mapping — needed for drum libraries (Superior Drummer,
    EZdrummer, ...) whose extra articulations don't follow GM numbering.
    """
    midi_file = mido.MidiFile(str(path))
    merged = list(mido.merge_tracks(midi_file.tracks))
    ticks_per_beat = midi_file.ticks_per_beat

    tempo_map = _build_tempo_map(merged)
    beats_per_bar = _time_signature(merged)

    all_notes = _extract_notes(merged, ticks_per_beat, note_map)

    notes = [n for n in all_notes if n.channel == prefer_channel] if prefer_channel is not None else all_notes
    if not notes:
        notes = all_notes

    return ParsedMidi(
        source_path=str(path),
        ticks_per_beat=ticks_per_beat,
        beats_per_bar=beats_per_bar,
        notes=notes,
        tempo_map=tempo_map,
    )


def _extract_notes(
    merged: list[mido.Message], ticks_per_beat: int, note_map: NoteMap | None = None
) -> list[NoteEvent]:
    notes: list[NoteEvent] = []
    open_notes: dict[tuple[int, int], tuple[int, int]] = {}  # (channel, pitch) -> (start_tick, velocity)
    absolute_tick = 0

    for msg in merged:
        absolute_tick += msg.time
        if msg.is_meta:
            continue

        if msg.type == "note_on" and msg.velocity > 0:
            open_notes[(msg.channel, msg.note)] = (absolute_tick, msg.velocity)
        elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
            key = (msg.channel, msg.note)
            opened = open_notes.pop(key, None)
            if opened is None:
                continue
            start_tick, velocity = opened
            gm_name, voice = voice_for_pitch(msg.note, note_map)
            notes.append(
                NoteEvent(
                    pitch=msg.note,
                    gm_name=gm_name,
                    voice=voice,
                    channel=msg.channel,
                    velocity=velocity,
                    start_tick=start_tick,
                    end_tick=absolute_tick,
                    start_beat=Fraction(start_tick, ticks_per_beat),
                    duration_beats=Fraction(absolute_tick - start_tick, ticks_per_beat),
                )
            )

    notes.sort(key=lambda n: (n.start_beat, n.pitch))
    return notes
