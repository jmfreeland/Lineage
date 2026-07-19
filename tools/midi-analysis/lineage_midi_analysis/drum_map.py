"""General MIDI percussion key map, collapsed to a smaller set of canonical
"voice" categories the diffing/aggregation stages actually reason about.

The raw GM name is kept alongside the voice category so vocabulary output
stays traceable to the source pitch, but grouping (e.g. every hi-hat
articulation) happens on `voice`.

Real drum-library exports (Superior Drummer, EZdrummer, and most sample
libraries' "MIDI grooves") usually put their CORE hits on the familiar GM
numbers (kick=36, snare=38, hats=42/46, ...) as a matter of practical
convention, but their many extra articulations — rimshots, chokes, flams,
degrees of hi-hat openness — get their own note numbers GM has no opinion
about, and would otherwise all collapse into a generic "other" bucket.
`voice_for_pitch()`'s optional `overrides` (built via `load_note_map()`,
easiest built by first running `note_usage.py` against your own corpus —
see that script's docstring) exists specifically for that gap.
"""

from __future__ import annotations

import json
from pathlib import Path

# pitch -> (gm_name, voice)
GM_DRUM_MAP: dict[int, tuple[str, str]] = {
    35: ("Acoustic Bass Drum", "kick"),
    36: ("Bass Drum 1", "kick"),
    37: ("Side Stick", "snare"),
    38: ("Acoustic Snare", "snare"),
    39: ("Hand Clap", "clap"),
    40: ("Electric Snare", "snare"),
    41: ("Low Floor Tom", "tom"),
    42: ("Closed Hi-Hat", "closed_hihat"),
    43: ("High Floor Tom", "tom"),
    44: ("Pedal Hi-Hat", "pedal_hihat"),
    45: ("Low Tom", "tom"),
    46: ("Open Hi-Hat", "open_hihat"),
    47: ("Low-Mid Tom", "tom"),
    48: ("Hi-Mid Tom", "tom"),
    49: ("Crash Cymbal 1", "crash"),
    50: ("High Tom", "tom"),
    51: ("Ride Cymbal 1", "ride"),
    52: ("Chinese Cymbal", "crash"),
    53: ("Ride Bell", "ride"),
    54: ("Tambourine", "perc"),
    55: ("Splash Cymbal", "crash"),
    56: ("Cowbell", "perc"),
    57: ("Crash Cymbal 2", "crash"),
    58: ("Vibraslap", "perc"),
    59: ("Ride Cymbal 2", "ride"),
    60: ("Hi Bongo", "perc"),
    61: ("Low Bongo", "perc"),
    62: ("Mute Hi Conga", "perc"),
    63: ("Open Hi Conga", "perc"),
    64: ("Low Conga", "perc"),
    65: ("High Timbale", "perc"),
    66: ("Low Timbale", "perc"),
    67: ("High Agogo", "perc"),
    68: ("Low Agogo", "perc"),
    69: ("Cabasa", "perc"),
    70: ("Maracas", "perc"),
    71: ("Short Whistle", "perc"),
    72: ("Long Whistle", "perc"),
    73: ("Short Guiro", "perc"),
    74: ("Long Guiro", "perc"),
    75: ("Claves", "perc"),
    76: ("Hi Wood Block", "perc"),
    77: ("Low Wood Block", "perc"),
    78: ("Mute Cuica", "perc"),
    79: ("Open Cuica", "perc"),
    80: ("Mute Triangle", "perc"),
    81: ("Open Triangle", "perc"),
}


NoteMap = dict[int, str]  # pitch -> voice, an override on top of GM_DRUM_MAP


def load_note_map(path: str | Path) -> NoteMap:
    """Loads a `{"<pitch>": "voice"}` JSON override file (keys are strings
    since JSON object keys must be strings) into a `{pitch: voice}` dict.
    See note_usage.py --emit-template for the easiest way to produce one."""
    raw = json.loads(Path(path).read_text())
    return {int(pitch): voice for pitch, voice in raw.items()}


def voice_for_pitch(pitch: int, overrides: NoteMap | None = None) -> tuple[str, str]:
    """Returns (gm_name, voice) for a MIDI pitch. `overrides` (see
    load_note_map()) takes priority over GM_DRUM_MAP when both have an
    opinion about a pitch — needed for drum libraries whose extra
    articulations don't follow GM numbering (see module docstring).
    Falls back to a generic 'other' voice for anything neither maps."""
    if overrides is not None and pitch in overrides:
        gm_name = GM_DRUM_MAP.get(pitch, (f"note_{pitch}", ""))[0]
        return (gm_name, overrides[pitch])
    entry = GM_DRUM_MAP.get(pitch)
    if entry is not None:
        return entry
    return (f"note_{pitch}", "other")
