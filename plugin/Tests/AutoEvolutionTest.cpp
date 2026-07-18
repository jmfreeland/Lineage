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
} // namespace

int main() {
  constexpr double sampleRate = 44100.0;
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
  processor.configureAutoEvolution(fillOnly, "Fill Only", true, 1);
  const auto lookAhead = processor.getPlaybackPreview(8);
  const bool containsScheduledFuture = std::any_of(
      lookAhead.events.begin(), lookAhead.events.end(), [](const auto& event) {
        return event.beatPosition >= 4.0 && (event.previewFlags & 8) != 0;
      });
  expect(containsScheduledFuture,
         "the upcoming MIDI preview includes scheduled automatic generations before they commit");

  constexpr int firstBlockSamples = 512;
  processAt(processor, playHead, 0.0, firstBlockSamples);
  expect(processor.drainAutoEvolutionEvents().empty(),
         "starting automatic evolution schedules rather than evolving immediately");

  const double secondBlockStart = (static_cast<double>(firstBlockSamples) / sampleRate) * 2.0;
  processAt(processor, playHead, secondBlockStart, 180000);
  const auto evolved = processor.drainAutoEvolutionEvents();
  expect(evolved.size() == 1 && evolved[0].operation == "fill" && evolved[0].ruleName == "Fill Only",
         "crossing the configured host-bar boundary creates one automatic rule node");

  processor.configureAutoEvolution(fillOnly, "Fill Only", false, 1);
  const double thirdBlockStart = secondBlockStart + (180000.0 / sampleRate) * 2.0;
  processAt(processor, playHead, thirdBlockStart, 180000);
  expect(processor.drainAutoEvolutionEvents().empty(),
         "pausing tree evolution leaves subsequent host bars unchanged");

  processor.configureAutoEvolution(fillOnly, "Fill Only", true, 1);
  processor.setSeedGroove(seed);
  const double fourthBlockStart = thirdBlockStart + (180000.0 / sampleRate) * 2.0;
  processAt(processor, playHead, fourthBlockStart, 180000);
  expect(processor.drainAutoEvolutionEvents().empty(),
         "loading a seed creates a new paused tree and clears its automatic schedule");

  processor.releaseResources();
  std::printf("\n%s\n", failures == 0 ? "All automatic-evolution tests passed."
                                        : "Automatic-evolution tests FAILED.");
  return failures == 0 ? 0 : 1;
}
