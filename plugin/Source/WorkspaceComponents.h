#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "StepSequencerComponent.h"

#include <functional>
#include <memory>
#include <vector>

namespace lineage::ui {

juce::Colour backgroundColour();
juce::Colour panelColour();
juce::Colour panelBorderColour();
juce::Colour textColour();
juce::Colour mutedTextColour();
juce::Colour accentColour();
juce::Colour secondaryAccentColour();

struct SeedPreset {
  juce::String id;
  juce::String name;
  juce::String description;
  std::vector<StepSequencerComponent::SeedLane> lanes;
};

struct RulePreset {
  juce::String id;
  juce::String name;
  juce::String description;
  double mutation = 0.0;
  double embellish = 0.0;
  double fill = 0.0;
  double hold = 0.0;
};

class LineageLookAndFeel final : public juce::LookAndFeel_V4 {
public:
  LineageLookAndFeel();

  void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
  void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
  void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                        juce::Slider::SliderStyle, juce::Slider&) override;
  void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};

class PanelComponent : public juce::Component {
public:
  explicit PanelComponent(juce::String title, juce::String eyebrow = {});

  void paint(juce::Graphics&) override;
  void resized() override;

protected:
  juce::Rectangle<int> getContentBounds() const;

private:
  juce::Label titleLabel;
  juce::Label eyebrowLabel;
};

class SeedEditorPanel final : public PanelComponent {
public:
  SeedEditorPanel();
  void resized() override;
  void sendCurrentPattern();
  void loadPreset(const SeedPreset& preset);

  std::function<void(const std::vector<StepSequencerComponent::SeedLane>&)> onPatternChanged;

private:
  StepSequencerComponent sequencer;
};

class TimelinePanel final : public PanelComponent {
public:
  struct Note {
    int midiNote = 0;
    int velocity = 100;
    double beatPosition = 0.0;
    double durationBeats = 0.25;
    int previewFlags = 0;
  };

  struct Preview {
    double startBeat = 0.0;
    double beatsPerBar = 4.0;
    double playheadBeat = 0.0;
    bool transportPlaying = false;
    std::vector<Note> notes;
  };

  TimelinePanel();
  void paint(juce::Graphics&) override;
  void setPreview(Preview nextPreview);

private:
  Preview preview;
};

class ArrangerPanel final : public PanelComponent {
public:
  ArrangerPanel();
  void paint(juce::Graphics&) override;
};

class MacroPanel final : public PanelComponent {
public:
  MacroPanel();
  void resized() override;

private:
  juce::Slider complexitySlider;
  juce::Slider intensitySlider;
  juce::Label complexityLabel;
  juce::Label intensityLabel;
};

class EvolutionCanvas final : public juce::Component {
public:
  EvolutionCanvas();

  void paint(juce::Graphics&) override;
  void addEvolution(bool branch, const juce::String& ruleName, const juce::String& operation);
  void resetSeed(const juce::String& seedName);
  int getRequiredHeight() const;

private:
  struct Node {
    juce::String name;
    juce::String ruleName;
    juce::String operation;
    int parentIndex = -1;
    bool branch = false;
  };

  std::vector<Node> nodes;
  int headIndex = 0;
  juce::Rectangle<float> getNodeBounds(int index) const;
};

class EvolutionTreePanel final : public PanelComponent {
public:
  EvolutionTreePanel();
  void resized() override;
  void addEvolution(bool branch, const juce::String& ruleName, const juce::String& operation);
  void resetSeed(const juce::String& seedName);
  bool isAutoEvolutionRunning() const;
  int getEvolutionFrequencyBars() const;

  std::function<void(bool)> onEvolutionRequested;
  std::function<void(bool running, int frequencyBars)> onAutoEvolutionChanged;

private:
  juce::Viewport viewport;
  EvolutionCanvas canvas;
  juce::TextButton evolveButton{"EVOLVE"};
  juce::TextButton branchButton{"BRANCH"};
  juce::TextButton startPauseButton{"START"};
  juce::Label frequencyLabel;
  juce::ComboBox frequencyBox;

  void updateCanvasSize();
};

class LibraryPanel final : public PanelComponent {
public:
  LibraryPanel();
  void resized() override;

  RulePreset getSelectedRule() const;
  std::function<void(const SeedPreset&)> onSeedSelected;
  std::function<void(const RulePreset&)> onRuleSelected;

private:
  juce::TextEditor searchBox;
  juce::TabbedComponent presetTabs{juce::TabbedButtonBar::TabsAtTop};
  juce::Component seedPage;
  juce::Component rulePage;
  std::vector<SeedPreset> seedPresets;
  std::vector<RulePreset> rulePresets;
  std::vector<std::unique_ptr<juce::TextButton>> seedButtons;
  std::vector<std::unique_ptr<juce::TextButton>> ruleButtons;
  int selectedSeed = 0;
  int selectedRule = 0;
};

class RuleControllerPanel final : public PanelComponent {
public:
  RuleControllerPanel();
  void resized() override;
  void setRulePreset(const RulePreset& preset);

  std::function<void(const RulePreset&)> onRuleChanged;

private:
  juce::Label activeRuleLabel;
  std::vector<std::unique_ptr<juce::Label>> labels;
  std::vector<std::unique_ptr<juce::Slider>> sliders;
  RulePreset currentRule;
  bool updatingRule = false;
};

// Independent named "A/B/etc" trees (DAW testing feedback) — distinct from
// BRANCH, which still shares ancestry with the current head's parent.
// Exactly one section is active/audible at a time; this bar switches which
// one that is and creates/deletes sections, but never evolves or mutates
// any of them itself.
class SectionBarComponent final : public juce::Component {
public:
  struct SectionInfo {
    juce::String id;
    juce::String name;
    bool active = false;
  };

  SectionBarComponent();
  void resized() override;
  void setSections(const std::vector<SectionInfo>& sections);

  std::function<void()> onCreateSection;
  std::function<void(const juce::String& id)> onSelectSection;
  std::function<void(const juce::String& id)> onDeleteSection;

private:
  struct Tab {
    juce::String id;
    std::unique_ptr<juce::TextButton> selectButton;
    std::unique_ptr<juce::TextButton> deleteButton;
  };

  std::vector<Tab> tabs;
  juce::TextButton addButton{"+"};

  void rebuild(const std::vector<SectionInfo>& sections);
};

class MainWorkspaceComponent final : public juce::Component {
public:
  MainWorkspaceComponent();
  void resized() override;
  void sendCurrentSeed();
  void setTimelinePreview(TimelinePanel::Preview preview);
  void addAutomaticEvolution(const juce::String& ruleName, const juce::String& operation);

  // Called after switching the active section (SectionBarComponent). Resets
  // only the evolution tree's local visual history — it never touches the
  // engine, so it cannot discard a section's real lineage tree. The seed
  // editor grid is deliberately left as-is: clearing or reloading it here
  // would fire onSeedPatternChanged and hard-reset whichever section is now
  // active, which is safe for a brand-new empty section but would destroy
  // an existing section's real content when merely switching to view it.
  void notifySectionChanged(const juce::String& sectionLabel);

  std::function<void(const std::vector<StepSequencerComponent::SeedLane>&)> onSeedPatternChanged;
  std::function<juce::String(const RulePreset&, bool branch)> onEvolutionRequested;
  std::function<void(const RulePreset&, bool running, int frequencyBars)> onAutoEvolutionChanged;

private:
  SeedEditorPanel seedEditor;
  TimelinePanel timeline;
  ArrangerPanel arranger;
  MacroPanel macros;
  EvolutionTreePanel evolutionTree;
  LibraryPanel library;
  RuleControllerPanel rules;
  RulePreset selectedRule;
  bool loadingSeedPreset = false;
};

class ModulationPanel final : public PanelComponent {
public:
  ModulationPanel();
  void paint(juce::Graphics&) override;
  void resized() override;

private:
  juce::TextButton addButton{"+ ADD MODULATOR"};
  int modulatorCount = 2;
};

class HumanizationPanel final : public PanelComponent {
public:
  HumanizationPanel(juce::RangedAudioParameter& humanizeAmount,
                    juce::RangedAudioParameter& humanizeTimingEnabled,
                    juce::RangedAudioParameter& ghostEnabled,
                    juce::RangedAudioParameter& ghostProbability);
  void resized() override;

private:
  juce::Slider humanizeSlider;
  juce::Slider ghostProbabilitySlider;
  juce::ToggleButton humanizeTimingButton{"Humanize timing"};
  juce::ToggleButton ghostEnabledButton{"Enable ghost notes"};
  juce::Label humanizeLabel;
  juce::Label ghostProbabilityLabel;
  juce::SliderParameterAttachment humanizeAttachment;
  juce::ButtonParameterAttachment humanizeTimingAttachment;
  juce::ButtonParameterAttachment ghostEnabledAttachment;
  juce::SliderParameterAttachment ghostProbabilityAttachment;
};

class SilencerPanel final : public PanelComponent {
public:
  SilencerPanel();
  void paint(juce::Graphics&) override;
};

class ModulationWorkspaceComponent final : public juce::Component {
public:
  ModulationWorkspaceComponent(juce::RangedAudioParameter& humanizeAmount,
                               juce::RangedAudioParameter& humanizeTimingEnabled,
                               juce::RangedAudioParameter& ghostEnabled,
                               juce::RangedAudioParameter& ghostProbability);
  void resized() override;

private:
  ModulationPanel modulation;
  HumanizationPanel humanization;
  SilencerPanel silencer;
};

} // namespace lineage::ui
