from fractions import Fraction

from lineage_midi_analysis.parser import NoteEvent, ParsedMidi
from lineage_midi_analysis.stats import (
    BarSliceStats,
    compute_bar_slice_stats,
    instrument_quantize_correlation,
    quantize_distance_distribution,
)

BEATS_PER_BAR = Fraction(4)


def note(voice: str, start_beat: Fraction, velocity: int = 100) -> NoteEvent:
    return NoteEvent(
        pitch=36,
        gm_name=voice,
        voice=voice,
        channel=9,
        velocity=velocity,
        start_tick=0,
        end_tick=0,
        start_beat=start_beat,
        duration_beats=Fraction(1, 4),
    )


def parsed_at_120bpm() -> ParsedMidi:
    return ParsedMidi(
        source_path="test.mid",
        ticks_per_beat=480,
        beats_per_bar=BEATS_PER_BAR,
        notes=[],
        tempo_map=[(0, 500_000)],  # 120bpm
    )


def test_compute_bar_slice_stats_empty_bar_is_all_zero():
    stats = compute_bar_slice_stats([], "f.mid", 0, BEATS_PER_BAR, 16, parsed_at_120bpm())
    assert stats == BarSliceStats(source_file="f.mid", bar_index=0, grid=16, total_notes=0)


def test_compute_bar_slice_stats_basic_metrics():
    notes = [
        note("kick", Fraction(0), velocity=100),
        note("kick", Fraction(2), velocity=120),
        note("snare", Fraction(1), velocity=80),
    ]
    stats = compute_bar_slice_stats(notes, "f.mid", 0, BEATS_PER_BAR, 16, parsed_at_120bpm())

    assert stats.total_notes == 3
    assert stats.notes_per_step[Fraction(0)] == 1
    assert stats.avg_velocity == (100 + 120 + 80) / 3
    assert stats.avg_velocity_per_instrument["kick"] == 110
    assert stats.avg_velocity_per_instrument["snare"] == 80
    assert stats.min_velocity_per_instrument["kick"] == 100
    assert stats.max_velocity_per_instrument["kick"] == 120


def test_compute_bar_slice_stats_quantize_distance_zero_when_exactly_on_grid():
    notes = [note("kick", Fraction(0)), note("kick", Fraction(1, 4))]
    stats = compute_bar_slice_stats(notes, "f.mid", 0, BEATS_PER_BAR, 16, parsed_at_120bpm())
    assert stats.avg_quantize_distance_ms == 0.0
    assert stats.avg_quantize_distance_per_instrument_ms["kick"] == 0.0


def test_compute_bar_slice_stats_quantize_distance_nonzero_when_offset():
    # A 16th-note grid line is every 0.25 beats; at 120bpm a quarter beat is
    # 500ms, so a 1/16-beat offset (a quarter of a grid step) is ~31.25ms.
    notes = [note("kick", Fraction(0)), note("kick", Fraction(1, 4) + Fraction(1, 16))]
    stats = compute_bar_slice_stats(notes, "f.mid", 0, BEATS_PER_BAR, 16, parsed_at_120bpm())
    assert stats.avg_quantize_distance_per_instrument_ms["kick"] > 0
    assert round(stats.avg_quantize_distance_per_instrument_ms["kick"], 2) == round(500 * (1 / 16) / 2, 2)


def test_quantize_distance_distribution_percentiles_and_bins():
    distances = {"kick": [0.0, 10.0, 20.0]}
    dist = quantize_distance_distribution(distances, bin_count=2)
    assert dist["kick"]["n"] == 3
    assert dist["kick"]["mean"] == 10.0
    assert dist["kick"]["median"] == 10.0
    assert sum(dist["kick"]["counts"]) == 3
    assert len(dist["kick"]["bin_edges"]) == 3


def test_quantize_distance_distribution_skips_empty_voice():
    dist = quantize_distance_distribution({"kick": []})
    assert "kick" not in dist


def test_instrument_quantize_correlation_perfectly_correlated():
    bars = [
        BarSliceStats(
            source_file="f.mid", bar_index=i, grid=16, total_notes=2,
            avg_quantize_distance_per_instrument_ms={"kick": float(i), "snare": float(i)},
        )
        for i in range(5)
    ]
    result = instrument_quantize_correlation(bars, ["kick", "snare"])
    assert result["voices"] == ["kick", "snare"]
    assert result["matrix"][0][1] == 1.0
    assert result["matrix"][1][0] == 1.0
    assert result["matrix"][0][0] == 1.0


def test_instrument_quantize_correlation_uncorrelated():
    bars = [
        BarSliceStats(
            source_file="f.mid", bar_index=0, grid=16, total_notes=2,
            avg_quantize_distance_per_instrument_ms={"kick": 0.0, "snare": 10.0},
        ),
        BarSliceStats(
            source_file="f.mid", bar_index=1, grid=16, total_notes=2,
            avg_quantize_distance_per_instrument_ms={"kick": 10.0, "snare": 10.0},
        ),
        BarSliceStats(
            source_file="f.mid", bar_index=2, grid=16, total_notes=2,
            avg_quantize_distance_per_instrument_ms={"kick": 5.0, "snare": 0.0},
        ),
        BarSliceStats(
            source_file="f.mid", bar_index=3, grid=16, total_notes=2,
            avg_quantize_distance_per_instrument_ms={"kick": 5.0, "snare": 20.0},
        ),
    ]
    result = instrument_quantize_correlation(bars, ["kick", "snare"])
    assert abs(result["matrix"][0][1]) < 0.5


def test_instrument_quantize_correlation_missing_voice_in_some_bars_does_not_crash():
    bars = [
        BarSliceStats(
            source_file="f.mid", bar_index=0, grid=16, total_notes=1,
            avg_quantize_distance_per_instrument_ms={"kick": 0.0},
        ),
        BarSliceStats(
            source_file="f.mid", bar_index=1, grid=16, total_notes=1,
            avg_quantize_distance_per_instrument_ms={"snare": 5.0},
        ),
    ]
    result = instrument_quantize_correlation(bars, ["kick", "snare"])
    assert result["matrix"][0][1] is None


def test_instrument_quantize_correlation_needs_at_least_two_overlapping_bars():
    bars = [
        BarSliceStats(
            source_file="f.mid", bar_index=0, grid=16, total_notes=2,
            avg_quantize_distance_per_instrument_ms={"kick": 0.0, "snare": 0.0},
        ),
    ]
    result = instrument_quantize_correlation(bars, ["kick", "snare"])
    assert result["matrix"][0][1] is None
