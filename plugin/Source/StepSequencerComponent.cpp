#include "StepSequencerComponent.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>

namespace {

constexpr int toolbarRowHeight = 25;
constexpr int gridRowHeight = 30;

juce::String midiNoteLabel(int note) {
  return juce::String(note) + "  " + juce::MidiMessage::getMidiNoteName(note, true, true, 3);
}

} // namespace

StepSequencerComponent::StepSequencerComponent() {
  addLaneButton.setTooltip("Add a seed lane");
  removeLaneButton.setTooltip("Remove the selected seed lane");
  addLaneButton.onClick = [this] {
    int candidateNote = 36;
    while (candidateNote < 128 && std::any_of(rows.begin(), rows.end(), [candidateNote](const auto& row) {
             return row->midiNote == candidateNote;
           })) {
      candidateNote += 1;
    }
    addLane("Lane " + juce::String(nextLaneId), std::min(candidateNote, 127));
    selectRow(static_cast<int>(rows.size()) - 1);
    notifyChange();
  };
  removeLaneButton.onClick = [this] { removeSelectedLane(); };
  addAndMakeVisible(addLaneButton);
  addAndMakeVisible(removeLaneButton);

  nameEditor.setTextToShowWhenEmpty("Lane name", juce::Colour(0xff6f7b88));
  nameEditor.setSelectAllWhenFocused(true);
  nameEditor.onTextChange = [this] {
    if (updatingEditors || selectedRow < 0 || selectedRow >= static_cast<int>(rows.size())) return;
    rows[static_cast<size_t>(selectedRow)]->name = nameEditor.getText().trim();
    updateRowSummary(*rows[static_cast<size_t>(selectedRow)]);
    notifyChange();
  };
  addAndMakeVisible(nameEditor);

  midiNoteBox.setTextWhenNothingSelected("MIDI note");
  for (int note = 0; note < 128; ++note) midiNoteBox.addItem(midiNoteLabel(note), note + 1);
  midiNoteBox.onChange = [this] {
    if (updatingEditors || selectedRow < 0 || selectedRow >= static_cast<int>(rows.size())) return;
    rows[static_cast<size_t>(selectedRow)]->midiNote = midiNoteBox.getSelectedId() - 1;
    updateRowSummary(*rows[static_cast<size_t>(selectedRow)]);
    notifyChange();
  };
  addAndMakeVisible(midiNoteBox);

  groupEditor.setTextToShowWhenEmpty("Group (e.g. Hats)", juce::Colour(0xff6f7b88));
  groupEditor.setSelectAllWhenFocused(true);
  groupEditor.onTextChange = [this] {
    if (updatingEditors || selectedRow < 0 || selectedRow >= static_cast<int>(rows.size())) return;
    rows[static_cast<size_t>(selectedRow)]->group = groupEditor.getText().trim();
    updateRowSummary(*rows[static_cast<size_t>(selectedRow)]);
    notifyChange();
  };
  addAndMakeVisible(groupEditor);

  viewport.setViewedComponent(&gridContent, false);
  viewport.setScrollBarsShown(true, false);
  viewport.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff3b4653));
  addAndMakeVisible(viewport);

  addLane("Kick", 36, {}, {0, 8});
  addLane("Snare", 38, {}, {4, 12});
  addLane("Closed Hat", 42, "Hats", {0, 2, 4, 6, 8, 10, 12});
  addLane("Open Hat", 46, "Hats", {14});
  selectRow(0);
}

void StepSequencerComponent::addLane(juce::String name,
                                     int midiNote,
                                     juce::String group,
                                     const std::vector<int>& activeStepList,
                                     int velocity) {
  auto row = std::make_unique<RowControls>();
  row->id = "seed-lane-" + juce::String(nextLaneId++);
  row->name = std::move(name);
  row->group = std::move(group);
  row->midiNote = std::clamp(midiNote, 0, 127);
  row->velocity = std::clamp(velocity, 1, 127);
  for (const int step : activeStepList) {
    if (step >= 0 && step < numSteps) row->activeSteps[static_cast<size_t>(step)] = true;
  }

  const auto rowId = row->id;
  row->selectButton.onClick = [this, rowId] {
    for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
      if (rows[static_cast<size_t>(index)]->id == rowId) {
        selectRow(index);
        return;
      }
    }
  };
  gridContent.addAndMakeVisible(row->selectButton);

  row->groupLabel.setJustificationType(juce::Justification::centred);
  row->groupLabel.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
  row->groupLabel.setColour(juce::Label::textColourId, juce::Colour(0xff47c6b1));
  row->groupLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a2e2d));
  gridContent.addAndMakeVisible(row->groupLabel);

  for (int step = 0; step < numSteps; ++step) {
    auto& cell = row->cells[static_cast<size_t>(step)];
    cell.setClickingTogglesState(true);
    cell.setToggleState(row->activeSteps[static_cast<size_t>(step)], juce::dontSendNotification);
    cell.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff242c36));
    cell.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff9d3b));
    cell.onClick = [this, rowId, step] {
      if (auto* editedRow = findRow(rowId)) {
        editedRow->activeSteps[static_cast<size_t>(step)] =
            editedRow->cells[static_cast<size_t>(step)].getToggleState();
        notifyChange();
      }
    };
    gridContent.addAndMakeVisible(cell);
  }

  updateRowSummary(*row);
  rows.push_back(std::move(row));
  updateGridSize();
}

void StepSequencerComponent::removeSelectedLane() {
  if (selectedRow < 0 || selectedRow >= static_cast<int>(rows.size())) return;
  rows.erase(rows.begin() + selectedRow);
  selectedRow = rows.empty() ? -1 : std::min(selectedRow, static_cast<int>(rows.size()) - 1);
  updateEditorFromSelection();
  updateGridSize();
  notifyChange();
}

void StepSequencerComponent::selectRow(int index) {
  selectedRow = index >= 0 && index < static_cast<int>(rows.size()) ? index : -1;
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    rows[static_cast<size_t>(rowIndex)]->selectButton.setToggleState(rowIndex == selectedRow,
                                                                    juce::dontSendNotification);
  }
  updateEditorFromSelection();
}

void StepSequencerComponent::updateEditorFromSelection() {
  updatingEditors = true;
  const bool hasSelection = selectedRow >= 0 && selectedRow < static_cast<int>(rows.size());
  nameEditor.setEnabled(hasSelection);
  midiNoteBox.setEnabled(hasSelection);
  groupEditor.setEnabled(hasSelection);
  removeLaneButton.setEnabled(hasSelection);
  if (hasSelection) {
    const auto& row = *rows[static_cast<size_t>(selectedRow)];
    nameEditor.setText(row.name, false);
    midiNoteBox.setSelectedId(row.midiNote + 1, juce::dontSendNotification);
    groupEditor.setText(row.group, false);
  } else {
    nameEditor.clear();
    midiNoteBox.setSelectedId(0, juce::dontSendNotification);
    groupEditor.clear();
  }
  updatingEditors = false;
}

void StepSequencerComponent::updateRowSummary(RowControls& row) {
  const auto displayName = row.name.isNotEmpty() ? row.name : "Untitled";
  row.selectButton.setButtonText(displayName + "  " + juce::String(row.midiNote));
  const auto groupName = row.group.trim().toUpperCase();
  row.groupLabel.setText(groupName.isNotEmpty() ? groupName.substring(0, 4) : "-", juce::dontSendNotification);
  row.groupLabel.setTooltip(groupName.isNotEmpty() ? "Linked group: " + row.group : "No linked group");
}

void StepSequencerComponent::updateGridSize() {
  gridContent.setSize(std::max(260, viewport.getMaximumVisibleWidth()),
                      std::max(viewport.getMaximumVisibleHeight(),
                               static_cast<int>(rows.size()) * gridRowHeight));
  resized();
}

void StepSequencerComponent::resized() {
  auto area = getLocalBounds();
  auto firstToolbarRow = area.removeFromTop(toolbarRowHeight);
  addLaneButton.setBounds(firstToolbarRow.removeFromLeft(28).reduced(1));
  removeLaneButton.setBounds(firstToolbarRow.removeFromLeft(28).reduced(1));
  nameEditor.setBounds(firstToolbarRow.reduced(2, 1));

  auto secondToolbarRow = area.removeFromTop(toolbarRowHeight);
  midiNoteBox.setBounds(secondToolbarRow.removeFromLeft(std::min(112, secondToolbarRow.getWidth() / 2)).reduced(2, 1));
  groupEditor.setBounds(secondToolbarRow.reduced(2, 1));
  area.removeFromTop(4);
  viewport.setBounds(area);

  const int contentWidth = std::max(260, viewport.getMaximumVisibleWidth());
  if (gridContent.getWidth() != contentWidth
      || gridContent.getHeight() != std::max(viewport.getMaximumVisibleHeight(),
                                             static_cast<int>(rows.size()) * gridRowHeight)) {
    gridContent.setSize(contentWidth,
                        std::max(viewport.getMaximumVisibleHeight(),
                                 static_cast<int>(rows.size()) * gridRowHeight));
  }

  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    auto rowArea = juce::Rectangle<int>(0, rowIndex * gridRowHeight, gridContent.getWidth(), gridRowHeight).reduced(0, 2);
    auto& row = *rows[static_cast<size_t>(rowIndex)];
    row.selectButton.setBounds(rowArea.removeFromLeft(82).reduced(1));
    row.groupLabel.setBounds(rowArea.removeFromLeft(34).reduced(2, 3));

    constexpr int cellGap = 1;
    constexpr int beatGap = 3;
    const int groupGaps = (numSteps / 4) - 1;
    const int cellWidth = std::max(5, (rowArea.getWidth() - (numSteps - 1) * cellGap - groupGaps * beatGap) / numSteps);
    int x = rowArea.getX();
    for (int step = 0; step < numSteps; ++step) {
      row.cells[static_cast<size_t>(step)].setBounds(x, rowArea.getY(), cellWidth, rowArea.getHeight());
      x += cellWidth + cellGap;
      if (step % 4 == 3 && step < numSteps - 1) x += beatGap;
    }
  }
}

void StepSequencerComponent::sendCurrentPattern() {
  notifyChange();
}

void StepSequencerComponent::setLanes(const std::vector<SeedLane>& lanes) {
  rows.clear();
  selectedRow = -1;
  for (const auto& lane : lanes) {
    addLane(lane.name, lane.midiNote, lane.group, lane.activeSteps, lane.velocity);
  }
  selectRow(rows.empty() ? -1 : 0);
  updateGridSize();
  notifyChange();
}

void StepSequencerComponent::notifyChange() {
  if (onPatternChanged == nullptr) return;
  std::vector<SeedLane> seedLanes;
  seedLanes.reserve(rows.size());
  for (const auto& row : rows) {
    SeedLane lane;
    lane.id = row->id;
    lane.name = row->name;
    lane.midiNote = row->midiNote;
    lane.group = row->group;
    lane.velocity = row->velocity;
    for (int step = 0; step < numSteps; ++step) {
      if (row->activeSteps[static_cast<size_t>(step)]) lane.activeSteps.push_back(step);
    }
    seedLanes.push_back(std::move(lane));
  }
  onPatternChanged(seedLanes);
}

StepSequencerComponent::RowControls* StepSequencerComponent::findRow(const juce::String& id) {
  const auto match = std::find_if(rows.begin(), rows.end(), [&id](const auto& row) { return row->id == id; });
  return match != rows.end() ? match->get() : nullptr;
}
