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
  void setLanes(const std::vector<SeedLane>& lanes);

  std::function<void(const std::vector<SeedLane>&)> onPatternChanged;

  // Fired on a right-click of a step cell (DAW testing feedback: "below the
  // seed editor we need a visualizer for whatever cell we've clicked on to
  // see where it evolved to") — left-click already toggles the step on/off
  // and commits immediately, so inspection uses the secondary click instead
  // of adding new UI chrome or disturbing that behavior.
  std::function<void(const juce::String& laneId, int step)> onCellInspectRequested;

  static constexpr int numSteps = 16;

private:
  // A plain TextButton that also reports right-clicks, so a step cell can
  // keep its normal left-click-toggle behavior untouched while adding
  // inspection on the secondary click.
  class StepCellButton : public juce::TextButton {
  public:
    std::function<void()> onRightClick;
    bool inspected = false;

    void mouseDown(const juce::MouseEvent& event) override {
      if (event.mods.isPopupMenu()) {
        // Suppress the matching mouseUp too — Button::mouseUp() would
        // otherwise still fire its own click-and-toggle on release (JUCE's
        // default Button doesn't distinguish which mouse button triggered
        // the gesture), silently flipping this step off right after
        // right-clicking it to inspect it.
        suppressingRightClick = true;
        if (onRightClick != nullptr) onRightClick();
        return;
      }
      suppressingRightClick = false;
      juce::TextButton::mouseDown(event);
    }

    void mouseUp(const juce::MouseEvent& event) override {
      if (suppressingRightClick) {
        suppressingRightClick = false;
        return;
      }
      juce::TextButton::mouseUp(event);
    }

    void paintOverChildren(juce::Graphics& g) override {
      if (!inspected) return;
      g.setColour(juce::Colour(0xff47c6b1));
      g.drawRect(getLocalBounds(), 2);
    }

  private:
    bool suppressingRightClick = false;
  };

  struct RowControls {
    juce::String id;
    juce::String name;
    juce::String group;
    int midiNote = 36;
    int velocity = 100;
    std::array<bool, numSteps> activeSteps{};
    juce::TextButton selectButton;
    juce::Label groupLabel;
    std::array<StepCellButton, numSteps> cells;
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

  // The step currently under inspection (right-clicked), independent of
  // selectedRow's "editing this lane's metadata" selection. -1 means none.
  juce::String inspectedLaneId;
  int inspectedStep = -1;

  void addLane(juce::String name,
               int midiNote,
               juce::String group = {},
               const std::vector<int>& activeSteps = {},
               int velocity = 100);
  void removeSelectedLane();
  void selectRow(int index);
  void updateEditorFromSelection();
  void updateRowSummary(RowControls& row);
  void updateGridSize();
  void notifyChange();
  RowControls* findRow(const juce::String& id);
  void inspectCell(const juce::String& laneId, int step);
  void updateCellHighlight();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerComponent)
};
