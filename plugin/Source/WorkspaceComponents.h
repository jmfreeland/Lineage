#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "StepSequencerComponent.h"

#include <functional>
#include <map>
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

// A rule-specific tunable beyond the four weights below — 0-2 per rule,
// different keys per rule (DAW testing feedback: "each rule will have 0-2
// params and they'll be different per rule"). Declarative like a
// mutation's own MutationParam manifest (DESIGN.md §2), so
// RuleControllerPanel can render an appropriate slider without knowing in
// advance which keys any given rule exposes.
struct RuleParamDef {
  juce::String key;
  juce::String label;
  double defaultValue = 0.0;
  double minValue = 0.0;
  double maxValue = 1.0;
  double step = 0.01;
};

struct RulePreset {
  juce::String id;
  juce::String name;
  juce::String description;
  double mutation = 0.0;
  double embellish = 0.0;
  double fill = 0.0;
  double hold = 0.0;
  // Pulls the groove back toward the section's seed rather than away from
  // it — everything else only ever pushes further from where the section
  // started, so without this a tree can only drift, never return.
  double settle = 0.0;
  std::vector<RuleParamDef> paramDefs;
  std::map<juce::String, double> paramValues;
};

// One entry in a section's weighted rule pool (DAW testing feedback: "the
// library should have a selector tick for each rule that opts it in or out
// for evolutions for each tree, and then there needs to be a list of
// enabled rules in the rule controller that allows setting weights for how
// often they occur"). Carries the full rule (a snapshot taken at the
// moment it was enabled, or round-tripped from the engine) rather than
// just an id, since it's what PluginEditor needs to build a
// JsEngine::RulePoolEntry — distinct from JsEngine::RulePoolEntry itself
// the same way RulePreset is distinct from JsEngine::EvolutionRule.
struct RulePoolEntryUI {
  RulePreset rule;
  double frequency = 1.0;
};

// Result of an EVOLVE/BRANCH request. ruleName is populated from the
// chosen rule's id (mirrors AutoEvolutionEvent's own display-name
// simplification — see PluginProcessor.h) since the bridge only reports
// the id, not the display-cased preset name. Both fields empty means the
// pool was empty (a safe no-op) or the bridge call failed.
struct EvolutionOutcome {
  juce::String ruleName;
  juce::String operation;
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

// One (section, bar count) block in the arrangement — an ordered, looping
// sequence mixing independent sections into a timeline (DAW testing
// feedback: "3 bars of groove and 1 with a bit more busyness, another
// three groove, and a fill"). Distinct from JsEngine::ArrangementBlock
// (that one's the bridge-marshaled form); PluginEditor converts between
// them the same way it does for RulePreset/JsEngine::EvolutionRule.
struct ArrangementBlockUI {
  juce::String sectionId;
  int bars = 1;
};

class ArrangerPanel final : public PanelComponent {
public:
  struct SectionOption {
    juce::String id;
    juce::String name;
  };

  ArrangerPanel();
  void resized() override;

  // The sections a block can be assigned to (cycled through on click) and
  // how their ids resolve to display names. Blocks referencing a section
  // no longer in this list are dropped from the display (the engine drops
  // them from playback too — see JsEngine::setArrangement()).
  void setAvailableSections(std::vector<SectionOption> newSections);
  // Which section a newly-added block defaults to.
  void setActiveSectionId(juce::String id);
  // Reflects the arrangement's current state, e.g. after the engine
  // confirms a change. Rebuilds the block controls; see the reentrancy
  // note in PluginEditor.cpp on why callers must defer this out of a
  // block control's own click handler.
  void setBlocks(std::vector<ArrangementBlockUI> newBlocks);

  std::function<void(const std::vector<ArrangementBlockUI>&)> onArrangementChanged;

private:
  struct BlockControls {
    juce::String sectionId;
    std::unique_ptr<juce::TextButton> sectionButton;
    std::unique_ptr<juce::TextButton> barsButton;
    std::unique_ptr<juce::TextButton> deleteButton;
  };

  std::vector<ArrangementBlockUI> blocks;
  std::vector<SectionOption> availableSections;
  juce::String activeSectionId;
  std::vector<BlockControls> blockControls;
  juce::TextButton addButton{"+ ADD BLOCK"};
  juce::Label emptyHintLabel;

  juce::String nameForSectionId(const juce::String& id) const;
  void rebuildControls();
  void notifyChanged();
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

  std::function<void(bool branch)> onEvolutionRequested;
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
  // Looks up a rule preset by id (e.g. to recover its display name/
  // description/paramDefs after a round trip through the engine, which
  // only knows ids). Falls back to a bare {id, id} preset if unknown —
  // still usable for display, just without the curated metadata.
  RulePreset findRuleById(const juce::String& id) const;
  // Reflects a section's confirmed rule-pool membership (e.g. after
  // switching sections) by ticking/unticking each rule's enable box
  // without notification.
  void setEnabledRuleIds(const std::vector<juce::String>& ids);

  std::function<void(const SeedPreset&)> onSeedSelected;
  std::function<void(const RulePreset&)> onRuleSelected;
  // Fired when a rule's pool-enable checkbox is toggled (DAW testing
  // feedback: "the library should have a selector tick for each rule that
  // opts it in or out for evolutions for each tree").
  std::function<void(const RulePreset&, bool enabled)> onRuleEnabledChanged;

private:
  juce::TextEditor searchBox;
  juce::TabbedComponent presetTabs{juce::TabbedButtonBar::TabsAtTop};
  juce::Component seedPage;
  juce::Component rulePage;
  std::vector<SeedPreset> seedPresets;
  std::vector<RulePreset> rulePresets;
  std::vector<std::unique_ptr<juce::TextButton>> seedButtons;
  std::vector<std::unique_ptr<juce::TextButton>> ruleButtons;
  std::vector<std::unique_ptr<juce::ToggleButton>> ruleEnableBoxes;
  int selectedSeed = 0;
  int selectedRule = 0;
};

class RuleControllerPanel final : public PanelComponent {
public:
  RuleControllerPanel();
  void resized() override;
  void setRulePreset(const RulePreset& preset);
  // Reflects the active section's confirmed rule pool. Rebuilds the pool's
  // frequency-slider controls; see rebuildParamControls()'s reentrancy
  // note — this only ever fires from a *different* component (the
  // Library's enable checkboxes) being toggled, never from inside one of
  // these sliders' own onValueChange.
  void setPool(std::vector<RulePoolEntryUI> entries);

  std::function<void(const RulePreset&)> onRuleChanged;
  // Fired when a pool entry's frequency slider moves. Only ever mutates
  // local state and calls this — never rebuilds — so it's safe to call
  // from inside the slider's own callback.
  std::function<void(const std::vector<RulePoolEntryUI>&)> onPoolChanged;

private:
  juce::Label activeRuleLabel;
  std::vector<std::unique_ptr<juce::Label>> labels;
  std::vector<std::unique_ptr<juce::Slider>> sliders;
  // Rule-specific extra-param controls (0-2, keys vary per rule). Rebuilt
  // only from setRulePreset() — i.e. only in response to a *different*
  // component (the Library's rule buttons) being clicked, never from
  // inside one of these sliders' own onValueChange — so there's no risk
  // of a control destroying itself mid-callback.
  std::vector<std::unique_ptr<juce::Label>> paramLabels;
  std::vector<std::unique_ptr<juce::Slider>> paramSliders;
  RulePreset currentRule;
  bool updatingRule = false;

  juce::Label poolHeaderLabel;
  std::vector<RulePoolEntryUI> pool;
  std::vector<std::unique_ptr<juce::Label>> poolLabels;
  std::vector<std::unique_ptr<juce::Slider>> poolSliders;

  void rebuildParamControls();
  void rebuildPoolControls();
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

  // Pass-through to the Arranger panel (DAW testing feedback: "3 bars of
  // groove and 1 with a bit more busyness, another three groove, and a
  // fill"). See ArrangerPanel's own doc comments for the reentrancy
  // constraint on when setArrangerBlocks()/setArrangerSections() may be
  // called.
  void setArrangerSections(std::vector<ArrangerPanel::SectionOption> sections);
  void setArrangerActiveSectionId(juce::String id);
  void setArrangerBlocks(std::vector<ArrangementBlockUI> arrangerBlocks);

  // Reflects the active section's confirmed rule pool (e.g. after
  // switching sections) — pushes down to both the Library's enable
  // checkboxes and the Rule Controller's frequency-slider list.
  void setRulePool(std::vector<RulePoolEntryUI> entries);

  std::function<void(const std::vector<StepSequencerComponent::SeedLane>&)> onSeedPatternChanged;
  // Rolls a weighted choice from the pool and evolves with it — the pool
  // replaces the old "whichever rule is selected in the Library" targeting
  // now that rules are opted in/out via checkbox instead.
  std::function<EvolutionOutcome(bool branch)> onEvolutionRequested;
  std::function<void(bool running, int frequencyBars)> onAutoEvolutionChanged;
  std::function<void(const std::vector<ArrangementBlockUI>&)> onArrangementChanged;
  std::function<void(const std::vector<RulePoolEntryUI>&)> onPoolChanged;

private:
  SeedEditorPanel seedEditor;
  TimelinePanel timeline;
  ArrangerPanel arranger;
  MacroPanel macros;
  EvolutionTreePanel evolutionTree;
  LibraryPanel library;
  RuleControllerPanel rules;
  RulePreset selectedRule;
  std::vector<RulePoolEntryUI> rulePool;
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
