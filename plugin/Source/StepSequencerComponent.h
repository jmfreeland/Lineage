#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

/**
 * Dynamic lane-based seed editor. Rows are real seed lanes rather than a
 * fixed kit template: each has a name, MIDI note, optional semantic group,
 * and a 16-step pattern. Selecting a row exposes its metadata in the compact
 * editor above the scrollable grid.
 */
class StepSequencerComponent : public juce::Component {
public:
  struct SeedLane {
    juce::String id;
    juce::String name;
    int midiNote = 36;
    juce::String group;
    int velocity = 100;
    std::vector<int> activeSteps;
  };

  StepSequencerComponent();

  void resized() override;
  void sendCurrentPattern();

  std::function<void(const std::vector<SeedLane>&)> onPatternChanged;

  static constexpr int numSteps = 16;

private:
  struct RowControls {
    juce::String id;
    juce::String name;
    juce::String group;
    int midiNote = 36;
    int velocity = 100;
    std::array<bool, numSteps> activeSteps{};
    juce::TextButton selectButton;
    juce::Label groupLabel;
    std::array<juce::TextButton, numSteps> cells;
  };

  juce::TextButton addLaneButton{"+"};
  juce::TextButton removeLaneButton{"-"};
  juce::TextEditor nameEditor;
  juce::ComboBox midiNoteBox;
  juce::TextEditor groupEditor;
  juce::Viewport viewport;
  juce::Component gridContent;
  std::vector<std::unique_ptr<RowControls>> rows;
  int selectedRow = -1;
  int nextLaneId = 1;
  bool updatingEditors = false;

  void addLane(juce::String name,
               int midiNote,
               juce::String group = {},
               std::initializer_list<int> activeSteps = {});
  void removeSelectedLane();
  void selectRow(int index);
  void updateEditorFromSelection();
  void updateRowSummary(RowControls& row);
  void updateGridSize();
  void notifyChange();
  RowControls* findRow(const juce::String& id);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerComponent)
};
