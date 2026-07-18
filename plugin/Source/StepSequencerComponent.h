#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <vector>

/**
 * Visual step sequencer for authoring a starting groove (DESIGN.md §11).
 * Fixed 3-lane (kick/snare/hihat) x 16-step (16th notes in 4/4) grid —
 * intentionally small and fixed rather than a general lane/length editor,
 * since the point right now is a way to program *a* seed pattern, not a
 * full groove-authoring UI. Every toggle immediately reports the full
 * current pattern via onPatternChanged; there's no separate "apply"
 * button.
 */
class StepSequencerComponent : public juce::Component {
public:
  struct Step {
    juce::String laneType;
    int step = 0;
    int velocity = 100;
  };

  StepSequencerComponent();

  void resized() override;

  std::function<void(const std::vector<Step>&)> onPatternChanged;

  static constexpr int numSteps = 16;
  static constexpr int numLanes = 3;

private:
  static const std::array<juce::String, numLanes> laneTypes;
  static const std::array<juce::String, numLanes> laneLabelText;

  std::array<std::array<juce::TextButton, numSteps>, numLanes> cells;
  std::array<juce::Label, numLanes> laneLabels;

  void notifyChange();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerComponent)
};
