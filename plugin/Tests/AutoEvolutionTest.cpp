// Host-transport-driven automatic evolution (DESIGN.md §11) — exercises
// LineageAudioProcessor::processBlock()'s real scheduling path (not just
// JsEngine directly) against a fake AudioPlayHead, so the bar-boundary
// detection and drainAutoEvolutionEvents() feed actually get driven the
// way a DAW would drive them.
//
// Scheduling itself (rule/running/frequency/next-due-bar) is TS-owned per
// section now (src/runtime.ts's Section.autoEvolution) — see
// JsEngine::tickAutoEvolution()'s BridgeTest.cpp coverage for the
// per-section independence guarantees. What this file verifies is the
// audio-thread integration: that a real transport crossing a bar boundary
// triggers exactly one tick, that a second block still inside the same bar
// doesn't re-tick, and that pausing/reseeding behave as the UI expects.
#include "../Source/PluginProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;

void expect(bool condition, const char* description) {
  if (!condition) {
    ++failures;
    std::printf("FAIL: %s\n", description);
  } else {
    std::printf("ok:   %s\n", description);
  }
}

class TestPlayHead final : public juce::AudioPlayHead {
public:
  void setPosition(double beat, bool playing) {
    position.setBpm(120.0);
    TimeSignature signature;
    signature.numerator = 4;
    signature.denominator = 4;
    position.setTimeSignature(signature);
    position.setPpqPosition(beat);
    position.setIsPlaying(playing);
  }

  juce::Optional<PositionInfo> getPosition() const override { return position; }

private:
  PositionInfo position;
};

void processAt(LineageAudioProcessor& processor,
               TestPlayHead& playHead,
               double beat,
               int samples,
               bool playing = true) {
  playHead.setPosition(beat, playing);
  juce::AudioBuffer<float> buffer(0, samples);
  juce::MidiBuffer midi;
  processor.processBlock(buffer, midi);
}

// Processes one block starting at `currentBeat`, sized to advance roughly
// `beatsToAdvance` (at the play head's fixed 120bpm), then moves
// `currentBeat` forward by the *exact* beats that block covered — so the
// next call's block start is contiguous with this one's end and never
// trips processBlock()'s transport-discontinuity detection (which would
// otherwise realign the schedule instead of ticking it, defeating the
// point of these bar-boundary-crossing checks).
constexpr double sampleRate = 44100.0;

void stepBeats(LineageAudioProcessor& processor,
              TestPlayHead& playHead,
              double& currentBeat,
              double beatsToAdvance) {
  const double beatsPerSample = 2.0 / sampleRate; // 120bpm fixed in TestPlayHead
  const int samples = std::max(1, static_cast<int>(std::llround(beatsToAdvance / beatsPerSample)));
  processAt(processor, playHead, currentBeat, samples);
  currentBeat += static_cast<double>(samples) * beatsPerSample;
}
} // namespace

int main() {
  LineageAudioProcessor processor;
  TestPlayHead playHead;
  processor.setPlayHead(&playHead);
  processor.setRateAndBufferSizeDetails(sampleRate, 200000);
  processor.prepareToPlay(sampleRate, 200000);

  const std::vector<JsEngine::SeedLane> seed = {
      {"kick", "Kick", 36, "", 110, {0, 8}},
      {"snare", "Snare", 38, "", 96, {4, 12}},
      {"hat", "Hat", 42, "Hats", 74, {0, 2, 4, 6, 8, 10, 12, 14}},
  };
  processor.setSeedGroove(seed);

  const JsEngine::EvolutionRule fillOnly{"fill-only", 0.0, 0.0, 1.0, 0.0};
  processor.setRulePool({{fillOnly, 1.0}});
  processor.configureAutoEvolution(true, 1);
  const auto lookAhead = processor.getPlaybackPreview(8);
  const bool containsScheduledFuture = std::any_of(
      lookAhead.events.begin(), lookAhead.events.end(), [](const auto& event) {
        return event.beatPosition >= 4.0 && (event.previewFlags & 8) != 0;
      });
  expect(containsScheduledFuture,
         "the upcoming MIDI preview includes scheduled automatic generations before they commit");

  double currentBeat = 0.0;
  // First block of playback: this only realigns the schedule (it was
  // configured before any transport position was known), it must not
  // evolve immediately.
  stepBeats(processor, playHead, currentBeat, 0.02);
  expect(processor.drainAutoEvolutionEvents().empty(),
         "starting automatic evolution schedules rather than evolving immediately");

  // Advance (contiguously) up to just past the bar 1 boundary (beat 4.0),
  // then process a block whose own start is inside bar 1 — the first block
  // at or past the scheduled bar, so it ticks exactly once.
  stepBeats(processor, playHead, currentBeat, 4.1 - currentBeat);
  stepBeats(processor, playHead, currentBeat, 0.1);
  const auto evolved = processor.drainAutoEvolutionEvents();
  expect(evolved.size() == 1 && evolved[0].operation == "fill" && evolved[0].ruleName == "fill-only",
         "crossing the configured host-bar boundary creates one automatic rule node");

  // A second block still inside bar 1 must not tick again.
  stepBeats(processor, playHead, currentBeat, 0.1);
  expect(processor.drainAutoEvolutionEvents().empty(),
         "a second block within the same already-ticked bar does not evolve again");

  processor.configureAutoEvolution(false, 1);
  stepBeats(processor, playHead, currentBeat, 8.1 - currentBeat);
  stepBeats(processor, playHead, currentBeat, 0.1);
  expect(processor.drainAutoEvolutionEvents().empty(),
         "pausing tree evolution leaves subsequent host bars unchanged");

  processor.configureAutoEvolution(true, 1);
  processor.setSeedGroove(seed);
  stepBeats(processor, playHead, currentBeat, 12.1 - currentBeat);
  stepBeats(processor, playHead, currentBeat, 0.1);
  expect(processor.drainAutoEvolutionEvents().empty(),
         "loading a seed creates a new paused tree and clears its automatic schedule");

  processor.releaseResources();
  std::printf("\n%s\n", failures == 0 ? "All automatic-evolution tests passed."
                                        : "Automatic-evolution tests FAILED.");
  return failures == 0 ? 0 : 1;
}
