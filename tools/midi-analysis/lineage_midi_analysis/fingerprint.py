"""Canonical, cross-file-comparable bar fingerprinting for corpus-wide
pattern distillation (library.py) — distinct from patterns.py's per-FILE
fingerprint() (which quantizes against whatever grid the CLI was told to
use, fine for finding one file's own dominant pattern, not fine for
comparing patterns across files that may have different natural
resolutions).

Every bar is fingerprinted at a single fixed CANONICAL_GRID regardless of
the file's own detected grid (grid.py) — a 16-grid file and a 32-grid file
must be able to produce identical fingerprints for musically-identical
bars. This is safe rather than lossy specifically because CANONICAL_GRID
(32) is a multiple of every straight grid this tool detects (8, 16): every
grid-8 or grid-16 position lands exactly on a grid-32 line too, so
canonicalizing at 32 never introduces rounding ambiguity or false splits.

Pattern IDENTITY (the fingerprint itself) is presence-only — (voice, step)
— matching patterns.py's existing exact-match philosophy. Velocity is
real-world-noisy enough that folding it into identity would mean almost no
two real bars ever match, defeating the point of counting frequency at
all. Velocity is not thrown away, though: it's captured separately as a
per-slot tier histogram (velocity_profile below), which feeds both the
pattern-distance metric (distance.py) and — the actual reason this
matters — makes each distilled pattern's *feel*, not just its shape,
available for later embellishment/humanization use, unlike the existing
diff pipeline's fills list (which only flags that a bar was a fill).
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass, field
from fractions import Fraction

from .parser import NoteEvent
from .quantize import beat_within_bar, quantize_beat

CANONICAL_GRID = 32

# (label, min_velocity, max_velocity) — inclusive bounds spanning the full
# MIDI velocity range 0-127. Four tiers is coarse by design: fine enough to
# distinguish a ghost note from an accent, coarse enough that ordinary
# performance/humanize jitter doesn't bounce a note between tiers.
VELOCITY_TIERS: tuple[tuple[str, int, int], ...] = (
    ("ghost", 0, 39),
    ("soft", 40, 74),
    ("medium", 75, 104),
    ("accent", 105, 127),
)

CanonicalSlot = tuple[str, Fraction]  # (voice, canonical-quantized position within bar)


def velocity_tier(velocity: int) -> str:
    # Tiers span the full 0-127 MIDI range with no gaps, so this only falls
    # through for a malformed out-of-range value, clamped to the nearest end.
    for label, low, high in VELOCITY_TIERS:
        if low <= velocity <= high:
            return label
    return VELOCITY_TIERS[0][0] if velocity < 0 else VELOCITY_TIERS[-1][0]


def canonical_slot(note: NoteEvent, beats_per_bar: Fraction) -> CanonicalSlot:
    local_beat = beat_within_bar(note.start_beat, beats_per_bar)
    return (note.voice, quantize_beat(local_beat, CANONICAL_GRID))


def canonical_fingerprint(bar_notes: list[NoteEvent], beats_per_bar: Fraction) -> frozenset[CanonicalSlot]:
    return frozenset(canonical_slot(n, beats_per_bar) for n in bar_notes)


@dataclass
class SlotVelocityProfile:
    tier_counts: dict[str, int] = field(default_factory=lambda: {label: 0 for label, _, _ in VELOCITY_TIERS})
    avg_velocity: float = 0.0

    @property
    def hit_count(self) -> int:
        return sum(self.tier_counts.values())


def velocity_profile(
    bar_notes: list[NoteEvent], beats_per_bar: Fraction
) -> dict[CanonicalSlot, SlotVelocityProfile]:
    velocities_by_slot: dict[CanonicalSlot, list[int]] = defaultdict(list)
    for note in bar_notes:
        velocities_by_slot[canonical_slot(note, beats_per_bar)].append(note.velocity)

    profiles: dict[CanonicalSlot, SlotVelocityProfile] = {}
    for slot, velocities in velocities_by_slot.items():
        profile = SlotVelocityProfile(avg_velocity=sum(velocities) / len(velocities))
        for velocity in velocities:
            profile.tier_counts[velocity_tier(velocity)] += 1
        profiles[slot] = profile
    return profiles
