#include "PluginEditor.h"

LineageAudioProcessorEditor::LineageAudioProcessorEditor(LineageAudioProcessor& processorIn)
    : AudioProcessorEditor(&processorIn), processorRef(processorIn) {
  titleLabel.setText("Lineage", juce::dontSendNotification);
  titleLabel.setJustificationType(juce::Justification::centred);
  titleLabel.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
  addAndMakeVisible(titleLabel);

  stepSequencer.onPatternChanged = [this](const std::vector<StepSequencerComponent::Step>& steps) {
    std::vector<JsEngine::SeedNote> notes;
    notes.reserve(steps.size());
    for (const auto& step : steps) {
      notes.push_back({step.laneType.toStdString(), step.step, step.velocity});
    }
    processorRef.setSeedGroove(notes);
  };
  addAndMakeVisible(stepSequencer);

  setSize(640, 180);
}

LineageAudioProcessorEditor::~LineageAudioProcessorEditor() = default;

void LineageAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void LineageAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();
  titleLabel.setBounds(bounds.removeFromTop(30));
  stepSequencer.setBounds(bounds.reduced(8));
}
