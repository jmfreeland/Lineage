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

  // The step cell currently under inspection (right-click in the seed
  // editor — DAW testing feedback: "below the seed editor we need a
  // visualizer for whatever cell we've clicked on to see where it evolved
  // to"). inspectedStep < 0 means nothing is under inspection.
  juce::String inspectedLaneId;
  int inspectedStep = -1;

  void timerCallback() override;
  void refreshTimelinePreview();
  void refreshSections();
  void refreshArranger();
  void refreshRulePool();
  void refreshNoteEvolution();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessorEditor)
};
