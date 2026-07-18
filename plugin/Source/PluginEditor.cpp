#include "PluginEditor.h"

LineageAudioProcessorEditor::LineageAudioProcessorEditor(LineageAudioProcessor& processorIn)
    : AudioProcessorEditor(&processorIn), processorRef(processorIn) {
  titleLabel.setText("Lineage", juce::dontSendNotification);
  titleLabel.setJustificationType(juce::Justification::centred);
  titleLabel.setFont(juce::Font(juce::FontOptions(24.0f).withStyle("Bold")));
  addAndMakeVisible(titleLabel);

  setSize(400, 300);
}

LineageAudioProcessorEditor::~LineageAudioProcessorEditor() = default;

void LineageAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void LineageAudioProcessorEditor::resized() {
  titleLabel.setBounds(getLocalBounds());
}
