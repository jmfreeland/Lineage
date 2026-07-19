#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "WorkspaceComponents.h"

class LineageAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer {
public:
  explicit LineageAudioProcessorEditor(LineageAudioProcessor&);
  ~LineageAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  LineageAudioProcessor& processorRef;
  lineage::ui::LineageLookAndFeel lookAndFeel;
  juce::Label titleLabel;
  juce::Label statusLabel;
  lineage::ui::SectionBarComponent sectionBar;
  juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
  lineage::ui::MainWorkspaceComponent mainWorkspace;
  lineage::ui::ModulationWorkspaceComponent modulationWorkspace;

  void timerCallback() override;
  void refreshTimelinePreview();
  void refreshSections();
  void refreshArranger();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessorEditor)
};
