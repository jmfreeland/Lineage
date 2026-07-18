#include "StepSequencerComponent.h"

const std::array<juce::String, StepSequencerComponent::numLanes> StepSequencerComponent::laneTypes = {"kick", "snare",
                                                                                                        "hihat"};
const std::array<juce::String, StepSequencerComponent::numLanes> StepSequencerComponent::laneLabelText = {
    "Kick", "Snare", "Hi-Hat"};

StepSequencerComponent::StepSequencerComponent() {
  for (int lane = 0; lane < numLanes; ++lane) {
    laneLabels[static_cast<size_t>(lane)].setText(laneLabelText[static_cast<size_t>(lane)],
                                                    juce::dontSendNotification);
    laneLabels[static_cast<size_t>(lane)].setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(laneLabels[static_cast<size_t>(lane)]);

    for (int step = 0; step < numSteps; ++step) {
      auto& cell = cells[static_cast<size_t>(lane)][static_cast<size_t>(step)];
      cell.setClickingTogglesState(true);
      cell.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
      cell.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
      cell.onClick = [this] { notifyChange(); };
      addAndMakeVisible(cell);
    }
  }
}

void StepSequencerComponent::resized() {
  constexpr int labelWidth = 70;
  constexpr int rowHeight = 32;
  constexpr int cellGap = 2;
  constexpr int beatGap = 8; // extra gap every 4 steps, so the grid reads as bars of 4

  auto bounds = getLocalBounds();
  const int gridWidth = bounds.getWidth() - labelWidth;
  const int beatGroups = numSteps / 4;
  const int cellWidth = (gridWidth - (numSteps - 1) * cellGap - (beatGroups - 1) * beatGap) / numSteps;

  for (int lane = 0; lane < numLanes; ++lane) {
    const int y = lane * rowHeight;
    laneLabels[static_cast<size_t>(lane)].setBounds(0, y, labelWidth, rowHeight);

    int x = labelWidth;
    for (int step = 0; step < numSteps; ++step) {
      cells[static_cast<size_t>(lane)][static_cast<size_t>(step)].setBounds(x, y, cellWidth, rowHeight - cellGap);
      x += cellWidth + cellGap;
      if (step % 4 == 3) x += beatGap - cellGap;
    }
  }
}

void StepSequencerComponent::notifyChange() {
  if (onPatternChanged == nullptr) return;

  std::vector<Step> steps;
  for (int lane = 0; lane < numLanes; ++lane) {
    for (int step = 0; step < numSteps; ++step) {
      if (cells[static_cast<size_t>(lane)][static_cast<size_t>(step)].getToggleState()) {
        steps.push_back({laneTypes[static_cast<size_t>(lane)], step, 100});
      }
    }
  }
  onPatternChanged(steps);
}
