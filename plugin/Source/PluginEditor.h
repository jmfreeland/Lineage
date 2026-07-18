#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "StepSequencerComponent.h"

class LineageAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit LineageAudioProcessorEditor(LineageAudioProcessor&);
  ~LineageAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  LineageAudioProcessor& processorRef;
  juce::Label titleLabel;
  StepSequencerComponent stepSequencer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessorEditor)
};
