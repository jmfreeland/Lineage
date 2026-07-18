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

  std::function<void(const std::vector<StepSequencerComponent::SeedLane>&)> onPatternChanged;

private:
  StepSequencerComponent sequencer;
};

class TimelinePanel final : public PanelComponent {
public:
  TimelinePanel();
  void paint(juce::Graphics&) override;
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
  void addEvolution(bool branch);
  int getRequiredHeight() const;

private:
  struct Node {
    juce::String name;
    int parentIndex = -1;
    bool branch = false;
  };

  std::vector<Node> nodes;
  juce::Rectangle<float> getNodeBounds(int index) const;
};

class EvolutionTreePanel final : public PanelComponent {
public:
  EvolutionTreePanel();
  void resized() override;

private:
  juce::Viewport viewport;
  EvolutionCanvas canvas;
  juce::TextButton evolveButton{"EVOLVE"};
  juce::TextButton branchButton{"BRANCH"};

  void updateCanvasSize();
};

class LibraryPanel final : public PanelComponent {
public:
  LibraryPanel();
  void resized() override;

private:
  juce::TextEditor searchBox;
  std::vector<std::unique_ptr<juce::ToggleButton>> entries;
};

class RuleControllerPanel final : public PanelComponent {
public:
  RuleControllerPanel();
  void resized() override;

private:
  std::vector<std::unique_ptr<juce::Label>> labels;
  std::vector<std::unique_ptr<juce::Slider>> sliders;
};

class MainWorkspaceComponent final : public juce::Component {
public:
  MainWorkspaceComponent();
  void resized() override;
  void sendCurrentSeed();

  std::function<void(const std::vector<StepSequencerComponent::SeedLane>&)> onSeedPatternChanged;

private:
  SeedEditorPanel seedEditor;
  TimelinePanel timeline;
  ArrangerPanel arranger;
  MacroPanel macros;
  EvolutionTreePanel evolutionTree;
  LibraryPanel library;
  RuleControllerPanel rules;
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
                    juce::RangedAudioParameter& ghostEnabled,
                    juce::RangedAudioParameter& ghostProbability);
  void resized() override;

private:
  juce::Slider humanizeSlider;
  juce::Slider ghostProbabilitySlider;
  juce::ToggleButton ghostEnabledButton{"Enable ghost notes"};
  juce::Label humanizeLabel;
  juce::Label ghostProbabilityLabel;
  juce::SliderParameterAttachment humanizeAttachment;
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
                               juce::RangedAudioParameter& ghostEnabled,
                               juce::RangedAudioParameter& ghostProbability);
  void resized() override;

private:
  ModulationPanel modulation;
  HumanizationPanel humanization;
  SilencerPanel silencer;
};

} // namespace lineage::ui
