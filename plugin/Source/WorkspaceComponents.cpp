#include "WorkspaceComponents.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace lineage::ui {
namespace {

constexpr float panelCornerRadius = 8.0f;

void styleCaption(juce::Label& label) {
  label.setColour(juce::Label::textColourId, mutedTextColour());
  label.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
  label.setJustificationType(juce::Justification::centred);
}

void configureMacroSlider(juce::Slider& slider) {
  slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
  slider.setRange(0.0, 100.0, 1.0);
  slider.setValue(50.0);
  slider.setDoubleClickReturnValue(true, 50.0);
}

void configureRuleSlider(juce::Slider& slider, double initialValue) {
  slider.setSliderStyle(juce::Slider::LinearHorizontal);
  slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
  slider.setRange(0.0, 1.0, 0.01);
  slider.setValue(initialValue);
}

StepSequencerComponent::SeedLane presetLane(juce::String id,
                                             juce::String name,
                                             int note,
                                             juce::String group,
                                             int velocity,
                                             std::initializer_list<int> steps) {
  return {std::move(id), std::move(name), note, std::move(group), velocity, std::vector<int>(steps)};
}

std::vector<SeedPreset> createSeedPresets() {
  return {
      {"deep-pocket", "Deep Pocket", "Straight, open and dependable",
       {presetLane("kick", "Kick", 36, {}, 110, {0, 8}),
        presetLane("snare", "Snare", 38, {}, 96, {4, 12}),
        presetLane("closed-hat", "Closed Hat", 42, "Hats", 76, {0, 2, 4, 6, 8, 10, 12}),
        presetLane("open-hat", "Open Hat", 46, "Hats", 82, {14})}},
      {"half-time", "Half Time", "Wide backbeat with late hats",
       {presetLane("kick", "Kick", 36, {}, 112, {0, 6, 10}),
        presetLane("snare", "Snare", 38, {}, 102, {8}),
        presetLane("closed-hat", "Closed Hat", 42, "Hats", 72, {0, 2, 4, 7, 10, 12}),
        presetLane("open-hat", "Open Hat", 46, "Hats", 84, {14})}},
      {"broken-hats", "Broken Hats", "Syncopated kick and hat pocket",
       {presetLane("kick", "Kick", 36, {}, 108, {0, 7, 10}),
        presetLane("snare", "Snare", 38, {}, 98, {4, 12}),
        presetLane("closed-hat", "Closed Hat", 42, "Hats", 74, {0, 3, 6, 8, 11, 13}),
        presetLane("open-hat", "Open Hat", 46, "Hats", 86, {14})}},
  };
}

RulePreset makeRulePreset(juce::String id,
                          juce::String name,
                          juce::String description,
                          double mutation,
                          double embellish,
                          double fill,
                          double hold,
                          double settle,
                          std::vector<RuleParamDef> paramDefs) {
  RulePreset preset;
  preset.id = std::move(id);
  preset.name = std::move(name);
  preset.description = std::move(description);
  preset.mutation = mutation;
  preset.embellish = embellish;
  preset.fill = fill;
  preset.hold = hold;
  preset.settle = settle;
  preset.paramDefs = std::move(paramDefs);
  for (const auto& def : preset.paramDefs) preset.paramValues[def.key] = def.defaultValue;
  return preset;
}

std::vector<RulePreset> createRulePresets() {
  return {
      makeRulePreset("pocket-keeper", "Pocket Keeper", "Mostly holds; small, tasteful movement",
                     0.22, 0.10, 0.05, 0.30, 0.33,
                     {{"mutationAmount", "Mutation Amount", 12.0, 1.0, 40.0, 1.0},
                      {"settleStrength", "Settle Strength", 0.35, 0.0, 1.0, 0.01}}),
      makeRulePreset("gentle-drift", "Gentle Drift", "Evolution first, occasional ornaments",
                     0.52, 0.25, 0.08, 0.15, 0.05,
                     {{"mutationAmount", "Mutation Amount", 22.0, 1.0, 40.0, 1.0},
                      {"embellishProbability", "Embellish Probability", 0.30, 0.0, 1.0, 0.01}}),
      makeRulePreset("fill-forward", "Fill Forward", "Pushes branches toward phrase endings",
                     0.30, 0.22, 0.40, 0.08, 0.08,
                     {{"fillPeakVelocity", "Fill Peak Velocity", 118.0, 40.0, 127.0, 1.0},
                      {"ghostVelocity", "Ghost Velocity", 40.0, 1.0, 100.0, 1.0}}),
      makeRulePreset("home-base", "Home Base", "Pulls the groove back toward its original seed",
                     0.15, 0.05, 0.05, 0.20, 0.55,
                     {{"settleStrength", "Settle Strength", 0.6, 0.0, 1.0, 0.01}}),
  };
}

} // namespace

juce::Colour backgroundColour() { return juce::Colour(0xff0b0e12); }
juce::Colour panelColour() { return juce::Colour(0xff151a21); }
juce::Colour panelBorderColour() { return juce::Colour(0xff29313c); }
juce::Colour textColour() { return juce::Colour(0xffe8edf2); }
juce::Colour mutedTextColour() { return juce::Colour(0xff8995a3); }
juce::Colour accentColour() { return juce::Colour(0xffff9d3b); }
juce::Colour secondaryAccentColour() { return juce::Colour(0xff47c6b1); }

LineageLookAndFeel::LineageLookAndFeel() {
  setColour(juce::ResizableWindow::backgroundColourId, backgroundColour());
  setColour(juce::Label::textColourId, textColour());
  setColour(juce::TextButton::textColourOffId, textColour());
  setColour(juce::TextButton::textColourOnId, backgroundColour());
  setColour(juce::Slider::textBoxTextColourId, textColour());
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff10141a));
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::ToggleButton::textColourId, textColour());
  setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff10141a));
  setColour(juce::TextEditor::textColourId, textColour());
  setColour(juce::TextEditor::outlineColourId, panelBorderColour());
  setColour(juce::TabbedComponent::backgroundColourId, backgroundColour());
  setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
  setColour(juce::TabbedButtonBar::tabTextColourId, mutedTextColour());
  setColour(juce::TabbedButtonBar::frontTextColourId, textColour());
}

void LineageLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                               juce::Button& button,
                                               const juce::Colour&,
                                               bool highlighted,
                                               bool down) {
  auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
  auto colour = button.getToggleState() ? accentColour() : juce::Colour(0xff222a34);
  if (highlighted) colour = colour.brighter(0.08f);
  if (down) colour = colour.darker(0.12f);
  g.setColour(colour);
  g.fillRoundedRectangle(bounds, 4.0f);
  g.setColour(button.getToggleState() ? accentColour().brighter(0.2f) : panelBorderColour());
  g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void LineageLookAndFeel::drawButtonText(juce::Graphics& g,
                                         juce::TextButton& button,
                                         bool,
                                         bool) {
  g.setColour(button.getToggleState() ? backgroundColour() : textColour());
  g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
  g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4), juce::Justification::centred, 1);
}

void LineageLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           float sliderPosition,
                                           float,
                                           float,
                                           juce::Slider::SliderStyle,
                                           juce::Slider&) {
  const auto track = juce::Rectangle<float>(static_cast<float>(x),
                                             static_cast<float>(y + height / 2 - 2),
                                             static_cast<float>(width),
                                             4.0f);
  g.setColour(juce::Colour(0xff2a323d));
  g.fillRoundedRectangle(track, 2.0f);
  g.setColour(secondaryAccentColour());
  g.fillRoundedRectangle(track.withWidth(std::max(0.0f, sliderPosition - static_cast<float>(x))), 2.0f);
  g.setColour(textColour());
  g.fillEllipse(sliderPosition - 5.0f, track.getCentreY() - 5.0f, 10.0f, 10.0f);
}

void LineageLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           float sliderPosition,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider&) {
  const float diameter = static_cast<float>(std::min(width, height)) - 10.0f;
  const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                              static_cast<float>(width), static_cast<float>(height))
                          .withSizeKeepingCentre(diameter, diameter);
  const float radius = diameter * 0.5f;
  const float angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
  juce::Path arc;
  arc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), radius - 3.0f, radius - 3.0f,
                    0.0f, rotaryStartAngle, rotaryEndAngle, true);
  g.setColour(juce::Colour(0xff2a323d));
  g.strokePath(arc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved));
  arc.clear();
  arc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), radius - 3.0f, radius - 3.0f,
                    0.0f, rotaryStartAngle, angle, true);
  g.setColour(accentColour());
  g.strokePath(arc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved));
  g.setColour(panelBorderColour());
  g.fillEllipse(bounds.reduced(9.0f));
  juce::Path pointer;
  pointer.addRoundedRectangle(-1.5f, -radius + 12.0f, 3.0f, radius * 0.36f, 1.5f);
  g.setColour(textColour());
  g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(bounds.getCentreX(), bounds.getCentreY()));
}

PanelComponent::PanelComponent(juce::String title, juce::String eyebrow) {
  titleLabel.setText(std::move(title), juce::dontSendNotification);
  titleLabel.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
  titleLabel.setColour(juce::Label::textColourId, textColour());
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(titleLabel);

  eyebrowLabel.setText(std::move(eyebrow), juce::dontSendNotification);
  eyebrowLabel.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
  eyebrowLabel.setColour(juce::Label::textColourId, accentColour());
  eyebrowLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(eyebrowLabel);
}

void PanelComponent::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
  g.setColour(panelColour());
  g.fillRoundedRectangle(bounds, panelCornerRadius);
  g.setColour(panelBorderColour());
  g.drawRoundedRectangle(bounds, panelCornerRadius, 1.0f);
  g.drawHorizontalLine(34, 10.0f, static_cast<float>(getWidth() - 10));
}

void PanelComponent::resized() {
  auto header = getLocalBounds().reduced(10, 0).removeFromTop(34);
  eyebrowLabel.setBounds(header.removeFromRight(std::min(110, header.getWidth() / 2)));
  titleLabel.setBounds(header);
}

juce::Rectangle<int> PanelComponent::getContentBounds() const {
  return getLocalBounds().reduced(10).withTrimmedTop(30);
}

SeedEditorPanel::SeedEditorPanel() : PanelComponent("Seed editor", "LIVE") {
  sequencer.onPatternChanged = [this](const auto& steps) {
    if (onPatternChanged != nullptr) onPatternChanged(steps);
  };
  sequencer.onCellInspectRequested = [this](const juce::String& laneId, int step) {
    if (onCellInspectRequested != nullptr) onCellInspectRequested(laneId, step);
  };
  addAndMakeVisible(sequencer);
}

void SeedEditorPanel::resized() {
  PanelComponent::resized();
  sequencer.setBounds(getContentBounds());
}

void SeedEditorPanel::sendCurrentPattern() {
  sequencer.sendCurrentPattern();
}

void SeedEditorPanel::loadPreset(const SeedPreset& preset) {
  sequencer.setLanes(preset.lanes);
}

TimelinePanel::TimelinePanel() : PanelComponent("Current + next", "8 BARS") {}

void TimelinePanel::paint(juce::Graphics& g) {
  PanelComponent::paint(g);
  auto area = getContentBounds().toFloat();
  const float labelWidth = 34.0f;
  const float rowGap = 4.0f;
  const float rowHeight = (area.getHeight() - rowGap) * 0.5f;
  const float barGap = 3.0f;
  const float barWidth = (area.getWidth() - labelWidth - barGap * 3.0f) / 4.0f;
  const double beatsPerBar = preview.beatsPerBar > 0.0 ? preview.beatsPerBar : 4.0;

  std::vector<int> pitches;
  std::array<bool, 8> scheduledEvolutionBars{};
  for (const auto& note : preview.notes) {
    if (std::find(pitches.begin(), pitches.end(), note.midiNote) == pitches.end()) pitches.push_back(note.midiNote);
    const int barIndex = static_cast<int>(
        std::floor((note.beatPosition - preview.startBeat) / beatsPerBar));
    if (barIndex >= 0 && barIndex < 8 && (note.previewFlags & 8) != 0) {
      scheduledEvolutionBars[static_cast<size_t>(barIndex)] = true;
    }
  }
  std::sort(pitches.begin(), pitches.end(), std::greater<int>());

  std::array<juce::Rectangle<float>, 8> barBounds;
  for (int bar = 0; bar < 8; ++bar) {
    const int rowIndex = bar / 4;
    const int column = bar % 4;
    const float rowY = area.getY() + static_cast<float>(rowIndex) * (rowHeight + rowGap);
    if (column == 0) {
      const auto label = juce::Rectangle<float>(area.getX(), rowY, labelWidth - 3.0f, rowHeight);
      g.setColour(rowIndex == 0 ? accentColour() : secondaryAccentColour());
      g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
      g.drawFittedText(rowIndex == 0 ? "CURR" : "NEXT", label.toNearestInt(), juce::Justification::centredLeft, 1);
    }
    const auto box = juce::Rectangle<float>(area.getX() + labelWidth + static_cast<float>(column) * (barWidth + barGap),
                                             rowY,
                                             barWidth,
                                             rowHeight);
    barBounds[static_cast<size_t>(bar)] = box;
    const auto barColour = bar < 4 ? accentColour() : secondaryAccentColour();
    g.setColour(barColour.withAlpha(bar == 0 ? 0.18f : 0.09f));
    g.fillRoundedRectangle(box, 3.0f);
    g.setColour(bar == 0 ? barColour.withAlpha(0.85f) : panelBorderColour().brighter(0.16f));
    g.drawRoundedRectangle(box, 3.0f, 1.0f);

    g.setColour(mutedTextColour().withAlpha(0.52f));
    for (int beat = 1; beat < static_cast<int>(std::ceil(beatsPerBar)); ++beat) {
      const float x = box.getX() + static_cast<float>(static_cast<double>(beat) / beatsPerBar) * box.getWidth();
      if (x < box.getRight()) g.drawVerticalLine(static_cast<int>(std::round(x)), box.getY() + 8.0f, box.getBottom() - 2.0f);
    }
    g.setColour(bar < 4 ? textColour().withAlpha(0.65f) : mutedTextColour().withAlpha(0.8f));
    g.setFont(7.5f);
    g.drawText(juce::String(bar + 1), box.withHeight(8.0f).reduced(3.0f, 0.0f).toNearestInt(),
               juce::Justification::centredLeft);
  }

  for (int bar = 0; bar < 8; ++bar) {
    const bool startsScheduledGeneration = scheduledEvolutionBars[static_cast<size_t>(bar)]
        && (bar == 0 || !scheduledEvolutionBars[static_cast<size_t>(bar - 1)]);
    if (!startsScheduledGeneration) continue;
    const auto box = barBounds[static_cast<size_t>(bar)];
    g.setColour(juce::Colour(0xffa889f0));
    g.fillRect(box.getX(), box.getY() + 1.0f, 2.0f, box.getHeight() - 2.0f);
    g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
    g.drawText("E", box.withHeight(8.0f).reduced(3.0f, 0.0f).toNearestInt(), juce::Justification::centredRight);
  }

  for (const auto& note : preview.notes) {
    const double relativeBeat = note.beatPosition - preview.startBeat;
    const int barIndex = static_cast<int>(std::floor(relativeBeat / beatsPerBar));
    if (barIndex < 0 || barIndex >= 8) continue;
    const auto box = barBounds[static_cast<size_t>(barIndex)].reduced(2.0f).withTrimmedTop(7.0f);
    const double beatInBar = relativeBeat - static_cast<double>(barIndex) * beatsPerBar;
    const float x = box.getX() + static_cast<float>(beatInBar / beatsPerBar) * box.getWidth();
    const auto pitch = std::find(pitches.begin(), pitches.end(), note.midiNote);
    const int pitchIndex = pitch != pitches.end() ? static_cast<int>(std::distance(pitches.begin(), pitch)) : 0;
    const float laneStep = pitches.size() > 1
        ? (box.getHeight() - 3.0f) / static_cast<float>(pitches.size() - 1)
        : 0.0f;
    const float y = box.getY() + static_cast<float>(pitchIndex) * laneStep;
    const float noteWidth = std::clamp(static_cast<float>(note.durationBeats / beatsPerBar) * box.getWidth(), 2.0f, 7.0f);
    auto noteColour = barIndex < 4 ? accentColour() : secondaryAccentColour();
    if ((note.previewFlags & 1) != 0) noteColour = juce::Colour(0xffa889f0);
    else if ((note.previewFlags & 8) != 0) noteColour = noteColour.interpolatedWith(juce::Colour(0xffa889f0), 0.36f);
    const float alpha = std::clamp(0.35f + static_cast<float>(note.velocity) / 190.0f, 0.4f, 1.0f);
    const auto noteRect = juce::Rectangle<float>(x, y, noteWidth, 2.6f);
    g.setColour(noteColour.withAlpha(alpha));
    g.fillRoundedRectangle(noteRect, 1.2f);
    // Humanization's effect (velocity and/or timing movement) is otherwise
    // invisible at this scale — a thin ring makes "this note was nudged"
    // legible without needing to read exact pixel offsets.
    if ((note.previewFlags & 2) != 0) {
      g.setColour(juce::Colours::white.withAlpha(0.55f));
      g.drawRoundedRectangle(noteRect.expanded(1.1f), 1.6f, 0.6f);
    }
  }

  // Explicit numeric readout of host position, independent of the moving
  // cursor below — a repeating one-bar loop can look identical bar to bar
  // at this scale, so the cursor alone is easy to mistake for "frozen"
  // even when it's tracking correctly. This makes movement unambiguous.
  const int hostBar = static_cast<int>(std::floor(preview.playheadBeat / beatsPerBar)) + 1;
  const double hostBeatInBar = preview.playheadBeat - static_cast<double>(hostBar - 1) * beatsPerBar + 1.0;
  const auto readoutText = preview.transportPlaying
      ? juce::String("BAR ") + juce::String(hostBar) + "  BEAT " + juce::String(hostBeatInBar, 2)
      : juce::String("STOPPED");
  g.setColour(preview.transportPlaying ? accentColour() : mutedTextColour());
  g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
  g.drawText(readoutText, area.withHeight(12.0f).withY(area.getY() - 13.0f),
             juce::Justification::centredRight);

  if (preview.transportPlaying) {
    const double relativePlayhead = preview.playheadBeat - preview.startBeat;
    const int playheadBar = static_cast<int>(std::floor(relativePlayhead / beatsPerBar));
    if (playheadBar >= 0 && playheadBar < 8) {
      const double beatInBar = relativePlayhead - static_cast<double>(playheadBar) * beatsPerBar;
      const auto box = barBounds[static_cast<size_t>(playheadBar)];
      const float x = box.getX() + static_cast<float>(beatInBar / beatsPerBar) * box.getWidth();
      g.setColour(juce::Colour(0xfffff2b8));
      g.drawVerticalLine(static_cast<int>(std::round(x)), box.getY() - 1.0f, box.getBottom() - 1.0f);
      g.drawVerticalLine(static_cast<int>(std::round(x)) + 1, box.getY() - 1.0f, box.getBottom() - 1.0f);
      juce::Path marker;
      marker.addTriangle(x - 3.0f, box.getY() - 1.0f, x + 3.0f, box.getY() - 1.0f, x, box.getY() + 3.0f);
      g.fillPath(marker);
    }
  }
}

void TimelinePanel::setPreview(Preview nextPreview) {
  preview = std::move(nextPreview);
  repaint();
}

namespace {
constexpr int sectionRadioGroupId = 9001;
}

SectionBarComponent::SectionBarComponent() {
  addButton.setTooltip("New independent section");
  addButton.onClick = [this] {
    if (onCreateSection) onCreateSection();
  };
  addAndMakeVisible(addButton);
}

void SectionBarComponent::setSections(const std::vector<SectionInfo>& sections) {
  rebuild(sections);
  resized();
}

void SectionBarComponent::rebuild(const std::vector<SectionInfo>& sections) {
  tabs.clear();
  const bool allowDelete = sections.size() > 1;

  for (const auto& section : sections) {
    Tab tab;
    tab.id = section.id;

    tab.selectButton = std::make_unique<juce::TextButton>(section.name);
    tab.selectButton->setRadioGroupId(sectionRadioGroupId);
    tab.selectButton->setClickingTogglesState(true);
    tab.selectButton->setToggleState(section.active, juce::dontSendNotification);
    tab.selectButton->setTooltip("Switch to section " + section.name);
    const juce::String id = section.id;
    tab.selectButton->onClick = [this, id] {
      if (onSelectSection) onSelectSection(id);
    };
    addAndMakeVisible(*tab.selectButton);

    if (allowDelete) {
      tab.deleteButton = std::make_unique<juce::TextButton>("x");
      tab.deleteButton->setTooltip("Delete section " + section.name);
      tab.deleteButton->onClick = [this, id] {
        if (onDeleteSection) onDeleteSection(id);
      };
      addAndMakeVisible(*tab.deleteButton);
    }

    tabs.push_back(std::move(tab));
  }

  addAndMakeVisible(addButton);
}

void SectionBarComponent::resized() {
  auto area = getLocalBounds();
  constexpr int gap = 4;
  int x = area.getX();
  for (auto& tab : tabs) {
    const int tabWidth = tab.deleteButton ? 58 : 40;
    auto tabArea = juce::Rectangle<int>(x, area.getY(), tabWidth, area.getHeight());
    if (tab.deleteButton) tab.deleteButton->setBounds(tabArea.removeFromRight(18));
    tab.selectButton->setBounds(tabArea);
    x += tabWidth + gap;
  }
  addButton.setBounds(x, area.getY(), area.getHeight(), area.getHeight());
}

namespace {
constexpr int arrangerBarSteps[] = {1, 2, 3, 4, 6, 8, 16};
int nextArrangerBarStep(int current) {
  constexpr auto count = static_cast<int>(std::size(arrangerBarSteps));
  for (int i = 0; i < count; ++i) {
    if (arrangerBarSteps[i] == current) return arrangerBarSteps[(i + 1) % count];
  }
  return arrangerBarSteps[0];
}
} // namespace

ArrangerPanel::ArrangerPanel() : PanelComponent("Arranger", "BLOCKS") {
  // Click handlers below deliberately only ever mutate `blocks` and call
  // notifyChanged() — never rebuildControls() directly. rebuildControls()
  // destroys and recreates every block's controls, including whichever
  // one was just clicked; doing that synchronously from inside that
  // control's own onClick would destroy it while it's still on the call
  // stack (the same reentrancy hazard SectionBarComponent hit and was
  // fixed for — see PluginEditor.cpp). The visible rebuild instead happens
  // when the caller's deferred setBlocks()/setAvailableSections() call
  // lands, reflecting the engine's confirmed state.
  addButton.onClick = [this] {
    const juce::String defaultSection = !activeSectionId.isEmpty() ? activeSectionId
        : (!availableSections.empty() ? availableSections.front().id : juce::String());
    if (defaultSection.isEmpty()) return;
    blocks.push_back({defaultSection, 1});
    notifyChanged();
  };
  addAndMakeVisible(addButton);

  emptyHintLabel.setText("No arrangement yet — the active section plays on its own.", juce::dontSendNotification);
  emptyHintLabel.setColour(juce::Label::textColourId, mutedTextColour());
  emptyHintLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
  addAndMakeVisible(emptyHintLabel);
}

juce::String ArrangerPanel::nameForSectionId(const juce::String& id) const {
  const auto found = std::find_if(availableSections.begin(), availableSections.end(),
      [&id](const auto& option) { return option.id == id; });
  return found != availableSections.end() ? found->name : id;
}

void ArrangerPanel::setAvailableSections(std::vector<SectionOption> newSections) {
  availableSections = std::move(newSections);
  const auto sizeBefore = blocks.size();
  blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [this](const auto& block) {
    return std::none_of(availableSections.begin(), availableSections.end(),
        [&block](const auto& option) { return option.id == block.sectionId; });
  }), blocks.end());
  rebuildControls();
  if (blocks.size() != sizeBefore) notifyChanged();
}

void ArrangerPanel::setActiveSectionId(juce::String id) {
  activeSectionId = std::move(id);
}

void ArrangerPanel::setBlocks(std::vector<ArrangementBlockUI> newBlocks) {
  blocks = std::move(newBlocks);
  rebuildControls();
}

void ArrangerPanel::rebuildControls() {
  blockControls.clear();
  for (size_t index = 0; index < blocks.size(); ++index) {
    BlockControls controls;
    controls.sectionId = blocks[index].sectionId;

    controls.sectionButton = std::make_unique<juce::TextButton>(nameForSectionId(blocks[index].sectionId));
    controls.sectionButton->setTooltip("Click to reassign this block to another section");
    controls.sectionButton->onClick = [this, index] {
      if (index >= blocks.size() || availableSections.empty()) return;
      const auto currentIt = std::find_if(availableSections.begin(), availableSections.end(),
          [this, index](const auto& option) { return option.id == blocks[index].sectionId; });
      const size_t currentIndex = currentIt != availableSections.end()
          ? static_cast<size_t>(std::distance(availableSections.begin(), currentIt)) : 0;
      const size_t nextIndex = (currentIndex + 1) % availableSections.size();
      blocks[index].sectionId = availableSections[nextIndex].id;
      notifyChanged();
    };
    addAndMakeVisible(*controls.sectionButton);

    controls.barsButton = std::make_unique<juce::TextButton>(
        juce::String(blocks[index].bars) + (blocks[index].bars == 1 ? " bar" : " bars"));
    controls.barsButton->setTooltip("Click to change how many bars this block plays");
    controls.barsButton->onClick = [this, index] {
      if (index >= blocks.size()) return;
      blocks[index].bars = nextArrangerBarStep(blocks[index].bars);
      notifyChanged();
    };
    addAndMakeVisible(*controls.barsButton);

    controls.deleteButton = std::make_unique<juce::TextButton>("x");
    controls.deleteButton->setTooltip("Remove this block");
    controls.deleteButton->onClick = [this, index] {
      if (index >= blocks.size()) return;
      blocks.erase(blocks.begin() + static_cast<long>(index));
      notifyChanged();
    };
    addAndMakeVisible(*controls.deleteButton);

    blockControls.push_back(std::move(controls));
  }
  addAndMakeVisible(addButton);
  emptyHintLabel.setVisible(blocks.empty());
  resized();
}

void ArrangerPanel::notifyChanged() {
  if (onArrangementChanged != nullptr) onArrangementChanged(blocks);
}

void ArrangerPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  emptyHintLabel.setBounds(area.withHeight(20));

  constexpr int blockWidth = 92;
  constexpr int gap = 6;
  int x = area.getX();
  const int blockHeight = std::min(area.getHeight(), 56);
  for (auto& controls : blockControls) {
    auto blockArea = juce::Rectangle<int>(x, area.getY(), blockWidth, blockHeight);
    controls.sectionButton->setBounds(blockArea.removeFromTop(blockHeight / 2).reduced(1));
    auto bottomRow = blockArea.reduced(1);
    controls.deleteButton->setBounds(bottomRow.removeFromRight(20));
    controls.barsButton->setBounds(bottomRow);
    x += blockWidth + gap;
  }
  addButton.setBounds(x, area.getY(), 96, blockHeight);
}

NoteEvolutionPanel::NoteEvolutionPanel() : PanelComponent("Note evolution", "TRACE") {
  selectionLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
  selectionLabel.setColour(juce::Label::textColourId, mutedTextColour());
  addAndMakeVisible(selectionLabel);

  viewport.setViewedComponent(&cardsContent, false);
  viewport.setScrollBarsShown(false, true);
  viewport.setColour(juce::ScrollBar::thumbColourId, panelBorderColour().brighter(0.25f));
  addAndMakeVisible(viewport);

  clearSelection();
}

void NoteEvolutionPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  selectionLabel.setBounds(area.removeFromTop(16));
  area.removeFromTop(2);
  viewport.setBounds(area);

  constexpr int cardWidth = 78;
  constexpr int gap = 4;
  cardsContent.setSize(
      std::max(viewport.getMaximumVisibleWidth(),
               static_cast<int>(cards.size()) * (cardWidth + gap)),
      viewport.getMaximumVisibleHeight());

  int x = 0;
  for (auto& card : cards) {
    auto cardArea = juce::Rectangle<int>(x, 0, cardWidth, cardsContent.getHeight());
    card.headerLabel->setBounds(cardArea.removeFromTop(cardArea.getHeight() / 2).reduced(2, 1));
    card.detailLabel->setBounds(cardArea.reduced(2, 1));
    x += cardWidth + gap;
  }
}

void NoteEvolutionPanel::setSelection(juce::String cellLabel) {
  selectionLabel.setText(cellLabel, juce::dontSendNotification);
}

void NoteEvolutionPanel::clearSelection() {
  selectionLabel.setText("Right-click a step to inspect its evolution", juce::dontSendNotification);
  rebuildCards({});
}

void NoteEvolutionPanel::setEvolution(std::vector<GenerationEntry> entries) {
  rebuildCards(entries);
}

void NoteEvolutionPanel::rebuildCards(const std::vector<GenerationEntry>& entries) {
  cards.clear();
  for (size_t index = 0; index < entries.size(); ++index) {
    const auto& entry = entries[index];
    GenerationCard card;

    card.headerLabel = std::make_unique<juce::Label>();
    card.headerLabel->setText(juce::String(static_cast<int>(index)) + " · " + entry.operation,
                              juce::dontSendNotification);
    card.headerLabel->setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    card.headerLabel->setColour(juce::Label::textColourId, entry.present ? textColour() : mutedTextColour());
    cardsContent.addAndMakeVisible(*card.headerLabel);

    card.detailLabel = std::make_unique<juce::Label>();
    card.detailLabel->setText(
        entry.present ? ("pos " + juce::String(entry.position, 2) + " · vel "
                        + juce::String(static_cast<int>(entry.velocity)))
                      : "dropped",
        juce::dontSendNotification);
    card.detailLabel->setFont(juce::Font(juce::FontOptions(9.0f)));
    card.detailLabel->setColour(juce::Label::textColourId,
                                entry.present ? accentColour() : mutedTextColour());
    cardsContent.addAndMakeVisible(*card.detailLabel);

    cards.push_back(std::move(card));
  }
  resized();
}

MacroPanel::MacroPanel() : PanelComponent("Macros", "PERFORM") {
  configureMacroSlider(complexitySlider);
  configureMacroSlider(intensitySlider);
  complexitySlider.setValue(42.0);
  intensitySlider.setValue(58.0);
  complexityLabel.setText("COMPLEXITY", juce::dontSendNotification);
  intensityLabel.setText("INTENSITY", juce::dontSendNotification);
  styleCaption(complexityLabel);
  styleCaption(intensityLabel);
  addAndMakeVisible(complexitySlider);
  addAndMakeVisible(intensitySlider);
  addAndMakeVisible(complexityLabel);
  addAndMakeVisible(intensityLabel);
}

void MacroPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  auto left = area.removeFromLeft(area.getWidth() / 2).reduced(4, 0);
  auto right = area.reduced(4, 0);
  complexityLabel.setBounds(left.removeFromBottom(18));
  intensityLabel.setBounds(right.removeFromBottom(18));
  complexitySlider.setBounds(left);
  intensitySlider.setBounds(right);
}

EvolutionCanvas::EvolutionCanvas() {
  nodes.push_back({"Deep Pocket", "Seed", "root", -1, false});
}

juce::Rectangle<float> EvolutionCanvas::getNodeBounds(int index) const {
  const float nodeWidth = std::min(230.0f, static_cast<float>(getWidth()) * 0.58f);
  const float nodeHeight = 68.0f;
  const float centreX = static_cast<float>(getWidth()) * 0.5f;
  const float branchOffset = std::min(120.0f, static_cast<float>(getWidth()) * 0.23f);
  const float x = centreX - nodeWidth * 0.5f + (nodes[static_cast<size_t>(index)].branch ? branchOffset : 0.0f);
  return {std::clamp(x, 12.0f, static_cast<float>(getWidth()) - nodeWidth - 12.0f),
          18.0f + static_cast<float>(index) * 94.0f,
          nodeWidth,
          nodeHeight};
}

void EvolutionCanvas::paint(juce::Graphics& g) {
  g.fillAll(juce::Colours::transparentBlack);
  for (int index = 1; index < static_cast<int>(nodes.size()); ++index) {
    const auto child = getNodeBounds(index);
    const auto parent = getNodeBounds(nodes[static_cast<size_t>(index)].parentIndex);
    juce::Path connection;
    connection.startNewSubPath(parent.getCentreX(), parent.getBottom());
    connection.cubicTo(parent.getCentreX(), parent.getBottom() + 32.0f,
                       child.getCentreX(), child.getY() - 32.0f,
                       child.getCentreX(), child.getY());
    g.setColour(nodes[static_cast<size_t>(index)].branch ? secondaryAccentColour().withAlpha(0.7f)
                                                        : accentColour().withAlpha(0.7f));
    g.strokePath(connection, juce::PathStrokeType(1.5f));
  }

  for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
    const auto node = getNodeBounds(index);
    const auto nodeAccent = nodes[static_cast<size_t>(index)].branch ? secondaryAccentColour() : accentColour();
    g.setColour(juce::Colour(0xff1d242d));
    g.fillRoundedRectangle(node, 7.0f);
    g.setColour(index == static_cast<int>(nodes.size()) - 1 ? nodeAccent : panelBorderColour().brighter(0.2f));
    g.drawRoundedRectangle(node, 7.0f, index == static_cast<int>(nodes.size()) - 1 ? 1.8f : 1.0f);
    g.setColour(textColour());
    g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    g.drawText(nodes[static_cast<size_t>(index)].name,
               node.withTrimmedLeft(12.0f).withHeight(28.0f).toNearestInt(),
               juce::Justification::centredLeft);

    const auto& item = nodes[static_cast<size_t>(index)];
    auto chips = node.withTrimmedTop(35.0f).reduced(10.0f, 7.0f);
    auto operationChip = chips.removeFromRight(std::min(76.0f, chips.getWidth() * 0.38f));
    chips.removeFromRight(6.0f);
    g.setColour(nodeAccent.withAlpha(0.12f));
    g.fillRoundedRectangle(chips, 3.0f);
    g.fillRoundedRectangle(operationChip, 3.0f);
    g.setColour(mutedTextColour());
    g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
    g.drawFittedText(item.ruleName.toUpperCase(), chips.toNearestInt(), juce::Justification::centred, 1);
    g.setColour(nodeAccent);
    g.drawFittedText(item.operation.toUpperCase(), operationChip.toNearestInt(), juce::Justification::centred, 1);
  }
}

void EvolutionCanvas::addEvolution(bool branch,
                                    const juce::String& ruleName,
                                    const juce::String& operation) {
  const int parent = branch && nodes[static_cast<size_t>(headIndex)].parentIndex >= 0
      ? nodes[static_cast<size_t>(headIndex)].parentIndex
      : headIndex;
  const auto number = static_cast<int>(nodes.size());
  nodes.push_back({branch ? "Variation " + juce::String(number) : "Evolution " + juce::String(number),
                   ruleName,
                   operation,
                   parent,
                   branch});
  headIndex = static_cast<int>(nodes.size()) - 1;
  repaint();
}

void EvolutionCanvas::resetSeed(const juce::String& seedName) {
  nodes.clear();
  nodes.push_back({seedName.isNotEmpty() ? seedName : "Custom Seed", "Seed", "root", -1, false});
  headIndex = 0;
  repaint();
}

int EvolutionCanvas::getRequiredHeight() const {
  return 32 + static_cast<int>(nodes.size()) * 94;
}

EvolutionTreePanel::EvolutionTreePanel() : PanelComponent("Seed evolutions", "LINEAGE TREE") {
  viewport.setViewedComponent(&canvas, false);
  viewport.setScrollBarsShown(true, false);
  viewport.setColour(juce::ScrollBar::thumbColourId, panelBorderColour().brighter(0.25f));
  evolveButton.setComponentID("evolveButton");
  branchButton.setComponentID("branchButton");
  evolveButton.onClick = [this] {
    if (onEvolutionRequested != nullptr) onEvolutionRequested(false);
  };
  branchButton.onClick = [this] {
    if (onEvolutionRequested != nullptr) onEvolutionRequested(true);
  };
  startPauseButton.setClickingTogglesState(true);
  startPauseButton.setComponentID("startEvolutionButton");
  startPauseButton.setTooltip("Start or pause host-synchronised evolution for this tree");
  startPauseButton.onClick = [this] {
    const bool running = startPauseButton.getToggleState();
    startPauseButton.setButtonText(running ? "PAUSE" : "START");
    if (onAutoEvolutionChanged != nullptr) {
      onAutoEvolutionChanged(running, getEvolutionFrequencyBars());
    }
  };
  frequencyLabel.setText("EVERY", juce::dontSendNotification);
  frequencyLabel.setColour(juce::Label::textColourId, mutedTextColour());
  frequencyLabel.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
  frequencyLabel.setJustificationType(juce::Justification::centredRight);
  for (const int bars : {1, 2, 4, 8, 16}) {
    frequencyBox.addItem(juce::String(bars) + (bars == 1 ? " BAR" : " BARS"), bars);
  }
  frequencyBox.setSelectedId(4, juce::dontSendNotification);
  frequencyBox.setTooltip("Number of complete host bars between automatic evolutions");
  frequencyBox.onChange = [this] {
    if (isAutoEvolutionRunning() && onAutoEvolutionChanged != nullptr) {
      onAutoEvolutionChanged(true, getEvolutionFrequencyBars());
    }
  };
  addAndMakeVisible(viewport);
  addAndMakeVisible(evolveButton);
  addAndMakeVisible(branchButton);
  addAndMakeVisible(startPauseButton);
  addAndMakeVisible(frequencyLabel);
  addAndMakeVisible(frequencyBox);
}

void EvolutionTreePanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  auto controls = area.removeFromBottom(34);
  branchButton.setBounds(controls.removeFromRight(90).reduced(2));
  evolveButton.setBounds(controls.removeFromRight(90).reduced(2));
  controls.removeFromRight(8);
  startPauseButton.setBounds(controls.removeFromLeft(82).reduced(2));
  frequencyLabel.setBounds(controls.removeFromLeft(48).reduced(1));
  frequencyBox.setBounds(controls.removeFromLeft(92).reduced(2));
  viewport.setBounds(area.withTrimmedBottom(6));
  updateCanvasSize();
}

void EvolutionTreePanel::updateCanvasSize() {
  canvas.setSize(std::max(320, viewport.getMaximumVisibleWidth()),
                 std::max(viewport.getMaximumVisibleHeight(), canvas.getRequiredHeight()));
}

void EvolutionTreePanel::addEvolution(bool branch,
                                      const juce::String& ruleName,
                                      const juce::String& operation) {
  canvas.addEvolution(branch, ruleName, operation);
  updateCanvasSize();
  viewport.setViewPositionProportionately(0.0, 1.0);
}

void EvolutionTreePanel::resetSeed(const juce::String& seedName) {
  startPauseButton.setToggleState(false, juce::dontSendNotification);
  startPauseButton.setButtonText("START");
  frequencyBox.setSelectedId(4, juce::dontSendNotification);
  canvas.resetSeed(seedName);
  updateCanvasSize();
  viewport.setViewPosition(0, 0);
}

bool EvolutionTreePanel::isAutoEvolutionRunning() const {
  return startPauseButton.getToggleState();
}

int EvolutionTreePanel::getEvolutionFrequencyBars() const {
  return frequencyBox.getSelectedId() > 0 ? frequencyBox.getSelectedId() : 4;
}

LibraryPanel::LibraryPanel()
    : PanelComponent("Library", "SEEDS + RULES"),
      seedPresets(createSeedPresets()),
      rulePresets(createRulePresets()) {
  searchBox.setTextToShowWhenEmpty("Search current tab", mutedTextColour());
  searchBox.setFont(juce::Font(juce::FontOptions(11.0f)));
  addAndMakeVisible(searchBox);

  presetTabs.addTab("SEEDS", panelColour(), &seedPage, false);
  presetTabs.addTab("RULES", panelColour(), &rulePage, false);
  presetTabs.setComponentID("libraryPresetTabs");
  presetTabs.setTabBarDepth(25);
  presetTabs.setOutline(0);
  addAndMakeVisible(presetTabs);

  for (int index = 0; index < static_cast<int>(seedPresets.size()); ++index) {
    auto button = std::make_unique<juce::TextButton>(seedPresets[static_cast<size_t>(index)].name);
    button->setRadioGroupId(4101);
    button->setClickingTogglesState(true);
    button->setToggleState(index == selectedSeed, juce::dontSendNotification);
    button->setTooltip(seedPresets[static_cast<size_t>(index)].description);
    button->onClick = [this, index] {
      selectedSeed = index;
      if (onSeedSelected != nullptr) onSeedSelected(seedPresets[static_cast<size_t>(index)]);
    };
    seedPage.addAndMakeVisible(*button);
    seedButtons.push_back(std::move(button));
  }

  for (int index = 0; index < static_cast<int>(rulePresets.size()); ++index) {
    auto enableBox = std::make_unique<juce::ToggleButton>();
    enableBox->setTooltip("Include " + rulePresets[static_cast<size_t>(index)].name + " in this tree's evolution pool");
    juce::ToggleButton* enableBoxPtr = enableBox.get();
    enableBox->onClick = [this, index, enableBoxPtr] {
      if (onRuleEnabledChanged != nullptr) {
        onRuleEnabledChanged(rulePresets[static_cast<size_t>(index)], enableBoxPtr->getToggleState());
      }
    };
    rulePage.addAndMakeVisible(*enableBox);
    ruleEnableBoxes.push_back(std::move(enableBox));

    auto button = std::make_unique<juce::TextButton>(rulePresets[static_cast<size_t>(index)].name);
    button->setRadioGroupId(4102);
    button->setClickingTogglesState(true);
    button->setToggleState(index == selectedRule, juce::dontSendNotification);
    button->setTooltip(rulePresets[static_cast<size_t>(index)].description);
    button->onClick = [this, index] {
      selectedRule = index;
      if (onRuleSelected != nullptr) onRuleSelected(rulePresets[static_cast<size_t>(index)]);
    };
    rulePage.addAndMakeVisible(*button);
    ruleButtons.push_back(std::move(button));
  }
}

void LibraryPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  searchBox.setBounds(area.removeFromTop(28));
  area.removeFromTop(6);
  presetTabs.setBounds(area);

  auto seedArea = seedPage.getLocalBounds().reduced(4);
  for (auto& button : seedButtons) button->setBounds(seedArea.removeFromTop(36).reduced(0, 3));
  auto ruleArea = rulePage.getLocalBounds().reduced(4);
  for (size_t index = 0; index < ruleButtons.size(); ++index) {
    auto row = ruleArea.removeFromTop(36).reduced(0, 3);
    ruleEnableBoxes[index]->setBounds(row.removeFromLeft(28));
    ruleButtons[index]->setBounds(row);
  }
}

RulePreset LibraryPanel::getSelectedRule() const {
  if (selectedRule >= 0 && selectedRule < static_cast<int>(rulePresets.size())) {
    return rulePresets[static_cast<size_t>(selectedRule)];
  }
  return {};
}

RulePreset LibraryPanel::findRuleById(const juce::String& id) const {
  for (const auto& preset : rulePresets) {
    if (preset.id == id) return preset;
  }
  RulePreset fallback;
  fallback.id = id;
  fallback.name = id;
  return fallback;
}

void LibraryPanel::setEnabledRuleIds(const std::vector<juce::String>& ids) {
  for (size_t index = 0; index < rulePresets.size() && index < ruleEnableBoxes.size(); ++index) {
    const bool enabled = std::find(ids.begin(), ids.end(), rulePresets[index].id) != ids.end();
    ruleEnableBoxes[index]->setToggleState(enabled, juce::dontSendNotification);
  }
}

RuleControllerPanel::RuleControllerPanel() : PanelComponent("Rule controller", "TREE WEIGHTS") {
  const juce::StringArray names{"Mutation", "Embellish", "Fill", "Hold", "Settle"};
  const double initialValues[] = {0.68, 0.42, 0.24, 0.55, 0.0};
  activeRuleLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
  activeRuleLabel.setColour(juce::Label::textColourId, accentColour());
  activeRuleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(activeRuleLabel);
  for (int index = 0; index < names.size(); ++index) {
    auto label = std::make_unique<juce::Label>();
    label->setText(names[index], juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, index == 3 ? secondaryAccentColour() : textColour());
    label->setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(*label);
    labels.push_back(std::move(label));

    auto slider = std::make_unique<juce::Slider>();
    configureRuleSlider(*slider, initialValues[index]);
    slider->onValueChange = [this] {
      if (updatingRule || sliders.size() < 5) return;
      currentRule.mutation = sliders[0]->getValue();
      currentRule.embellish = sliders[1]->getValue();
      currentRule.fill = sliders[2]->getValue();
      currentRule.hold = sliders[3]->getValue();
      currentRule.settle = sliders[4]->getValue();
      if (onRuleChanged != nullptr) onRuleChanged(currentRule);
    };
    addAndMakeVisible(*slider);
    sliders.push_back(std::move(slider));
  }

  poolHeaderLabel.setText("ENABLED RULES POOL", juce::dontSendNotification);
  poolHeaderLabel.setColour(juce::Label::textColourId, mutedTextColour());
  poolHeaderLabel.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
  addAndMakeVisible(poolHeaderLabel);
}

void RuleControllerPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  activeRuleLabel.setBounds(area.removeFromTop(26));

  // Reserve the pool section's space from the bottom first — otherwise the
  // per-rule weight sliders above (sized to evenly fill whatever's left)
  // would swallow the entire column before the pool gets a look-in.
  const int poolRowHeight = 26;
  const int poolSectionHeight = 22 + static_cast<int>(poolSliders.size()) * poolRowHeight;
  auto poolArea = area.removeFromBottom(poolSectionHeight);

  const int totalRows = static_cast<int>(sliders.size() + paramSliders.size());
  const int rowHeight = totalRows > 0 ? std::max(22, area.getHeight() / totalRows) : 30;
  for (size_t index = 0; index < sliders.size(); ++index) {
    auto row = area.removeFromTop(rowHeight);
    labels[index]->setBounds(row.removeFromLeft(72));
    sliders[index]->setBounds(row.reduced(2, 3));
  }
  if (!paramSliders.empty()) area.removeFromTop(6);
  for (size_t index = 0; index < paramSliders.size(); ++index) {
    auto row = area.removeFromTop(rowHeight);
    paramLabels[index]->setBounds(row.removeFromLeft(120));
    paramSliders[index]->setBounds(row.reduced(2, 3));
  }

  poolArea.removeFromTop(6);
  poolHeaderLabel.setBounds(poolArea.removeFromTop(16));
  for (size_t index = 0; index < poolSliders.size(); ++index) {
    auto row = poolArea.removeFromTop(poolRowHeight);
    poolLabels[index]->setBounds(row.removeFromLeft(120));
    poolSliders[index]->setBounds(row.reduced(2, 2));
  }
}

void RuleControllerPanel::rebuildParamControls() {
  paramLabels.clear();
  paramSliders.clear();
  for (const auto& def : currentRule.paramDefs) {
    auto label = std::make_unique<juce::Label>();
    label->setText(def.label, juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, mutedTextColour());
    label->setFont(juce::Font(juce::FontOptions(10.0f)));
    addAndMakeVisible(*label);

    auto slider = std::make_unique<juce::Slider>();
    slider->setSliderStyle(juce::Slider::LinearHorizontal);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
    slider->setRange(def.minValue, def.maxValue, def.step);
    const auto found = currentRule.paramValues.find(def.key);
    slider->setValue(found != currentRule.paramValues.end() ? found->second : def.defaultValue,
                     juce::dontSendNotification);

    juce::Slider* sliderPtr = slider.get();
    const juce::String key = def.key;
    slider->onValueChange = [this, sliderPtr, key] {
      if (updatingRule) return;
      currentRule.paramValues[key] = sliderPtr->getValue();
      if (onRuleChanged != nullptr) onRuleChanged(currentRule);
    };
    addAndMakeVisible(*slider);

    paramLabels.push_back(std::move(label));
    paramSliders.push_back(std::move(slider));
  }
  resized();
}

void RuleControllerPanel::setRulePreset(const RulePreset& preset) {
  currentRule = preset;
  activeRuleLabel.setText(preset.name + "  ·  " + preset.description, juce::dontSendNotification);
  if (sliders.size() < 5) return;
  updatingRule = true;
  sliders[0]->setValue(preset.mutation, juce::dontSendNotification);
  sliders[1]->setValue(preset.embellish, juce::dontSendNotification);
  sliders[2]->setValue(preset.fill, juce::dontSendNotification);
  sliders[3]->setValue(preset.hold, juce::dontSendNotification);
  sliders[4]->setValue(preset.settle, juce::dontSendNotification);
  updatingRule = false;
  // Each rule can expose a different set of extra params (0-2, different
  // keys), so the control set itself — not just slider values — needs
  // rebuilding per rule. This only ever fires here, in response to a
  // different rule being selected elsewhere, never from inside one of
  // these sliders' own callback.
  rebuildParamControls();
}

void RuleControllerPanel::setPool(std::vector<RulePoolEntryUI> entries) {
  pool = std::move(entries);
  rebuildPoolControls();
}

void RuleControllerPanel::rebuildPoolControls() {
  poolLabels.clear();
  poolSliders.clear();
  for (size_t index = 0; index < pool.size(); ++index) {
    auto label = std::make_unique<juce::Label>();
    label->setText(pool[index].rule.name, juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, textColour());
    label->setFont(juce::Font(juce::FontOptions(10.0f)));
    addAndMakeVisible(*label);

    auto slider = std::make_unique<juce::Slider>();
    slider->setSliderStyle(juce::Slider::LinearHorizontal);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
    slider->setRange(0.0, 4.0, 0.1);
    slider->setValue(pool[index].frequency, juce::dontSendNotification);
    slider->onValueChange = [this, index] {
      if (index >= pool.size()) return;
      pool[index].frequency = poolSliders[index]->getValue();
      if (onPoolChanged != nullptr) onPoolChanged(pool);
    };
    addAndMakeVisible(*slider);

    poolLabels.push_back(std::move(label));
    poolSliders.push_back(std::move(slider));
  }
  resized();
}

MainWorkspaceComponent::MainWorkspaceComponent() {
  arranger.onArrangementChanged = [this](const std::vector<ArrangementBlockUI>& blocks) {
    if (onArrangementChanged != nullptr) onArrangementChanged(blocks);
  };
  seedEditor.onPatternChanged = [this](const auto& steps) {
    if (!loadingSeedPreset) evolutionTree.resetSeed("Custom Seed");
    if (onSeedPatternChanged != nullptr) onSeedPatternChanged(steps);
  };
  seedEditor.onCellInspectRequested = [this](const juce::String& laneId, int step) {
    if (onNoteInspectRequested != nullptr) onNoteInspectRequested(laneId, step);
  };
  library.onSeedSelected = [this](const SeedPreset& preset) {
    loadingSeedPreset = true;
    seedEditor.loadPreset(preset);
    loadingSeedPreset = false;
    evolutionTree.resetSeed(preset.name);
  };
  library.onRuleSelected = [this](const RulePreset& preset) {
    selectedRule = preset;
    rules.setRulePreset(selectedRule);
  };
  // Enabling a rule snapshots whichever version is "current" for it — if
  // it's the rule presently open in the weights editor (possibly tweaked
  // there), that edited copy is used; otherwise the library's stock
  // preset. Disabling just drops it from the pool.
  library.onRuleEnabledChanged = [this](const RulePreset& preset, bool enabled) {
    const RulePreset& source = (selectedRule.id == preset.id) ? selectedRule : preset;
    const auto existing = std::find_if(rulePool.begin(), rulePool.end(), [&](const RulePoolEntryUI& entry) {
      return entry.rule.id == preset.id;
    });
    if (enabled) {
      if (existing == rulePool.end()) rulePool.push_back({source, 1.0});
    } else if (existing != rulePool.end()) {
      rulePool.erase(existing);
    }
    rules.setPool(rulePool);
    if (onPoolChanged != nullptr) onPoolChanged(rulePool);
  };
  rules.onRuleChanged = [this](const RulePreset& rule) {
    selectedRule = rule;
  };
  rules.onPoolChanged = [this](const std::vector<RulePoolEntryUI>& entries) {
    rulePool = entries;
    if (onPoolChanged != nullptr) onPoolChanged(rulePool);
  };
  evolutionTree.onEvolutionRequested = [this](bool branch) {
    if (onEvolutionRequested == nullptr) return;
    const auto outcome = onEvolutionRequested(branch);
    if (outcome.operation.isNotEmpty()) evolutionTree.addEvolution(branch, outcome.ruleName, outcome.operation);
  };
  evolutionTree.onAutoEvolutionChanged = [this](bool running, int frequencyBars) {
    if (onAutoEvolutionChanged != nullptr) onAutoEvolutionChanged(running, frequencyBars);
  };
  selectedRule = library.getSelectedRule();
  rules.setRulePreset(selectedRule);
  addAndMakeVisible(seedEditor);
  addAndMakeVisible(timeline);
  addAndMakeVisible(arranger);
  addAndMakeVisible(noteEvolution);
  addAndMakeVisible(macros);
  addAndMakeVisible(evolutionTree);
  addAndMakeVisible(library);
  addAndMakeVisible(rules);
}

void MainWorkspaceComponent::resized() {
  constexpr int gap = 8;
  auto area = getLocalBounds().reduced(10);
  // The seed editor is the one panel in this rail that's actually
  // functional today (arranger/macros are still inert scaffolding — see
  // DESIGN.md §11), so it gets the width/height budget: a wider left
  // column overall, and most of that column's height, at the arranger's
  // expense rather than shrinking anything that does something.
  const int leftWidth = std::clamp(static_cast<int>(static_cast<float>(area.getWidth()) * 0.32f), 320, 420);
  const int rightWidth = std::clamp(static_cast<int>(static_cast<float>(area.getWidth()) * 0.24f), 245, 330);
  auto left = area.removeFromLeft(leftWidth);
  area.removeFromLeft(gap);
  auto right = area.removeFromRight(rightWidth);
  area.removeFromRight(gap);
  auto centre = area;

  const int seedHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.42f), 260, 320);
  const int noteEvolutionHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.14f), 90, 110);
  const int timelineHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.16f), 105, 130);
  const int arrangerHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.16f), 90, 130);
  seedEditor.setBounds(left.removeFromTop(seedHeight));
  left.removeFromTop(gap);
  noteEvolution.setBounds(left.removeFromTop(noteEvolutionHeight));
  left.removeFromTop(gap);
  timeline.setBounds(left.removeFromTop(timelineHeight));
  left.removeFromTop(gap);
  arranger.setBounds(left.removeFromTop(arrangerHeight));
  left.removeFromTop(gap);
  macros.setBounds(left);

  evolutionTree.setBounds(centre);

  library.setBounds(right.removeFromTop((right.getHeight() - gap) / 2));
  right.removeFromTop(gap);
  rules.setBounds(right);
}

void MainWorkspaceComponent::sendCurrentSeed() {
  loadingSeedPreset = true;
  seedEditor.sendCurrentPattern();
  loadingSeedPreset = false;
}

void MainWorkspaceComponent::setTimelinePreview(TimelinePanel::Preview preview) {
  timeline.setPreview(std::move(preview));
}

void MainWorkspaceComponent::addAutomaticEvolution(const juce::String& ruleName,
                                                    const juce::String& operation) {
  evolutionTree.addEvolution(false, ruleName, operation);
}

void MainWorkspaceComponent::notifySectionChanged(const juce::String& sectionLabel) {
  evolutionTree.resetSeed(sectionLabel);
  noteEvolution.clearSelection();
}

void MainWorkspaceComponent::setNoteSelection(juce::String cellLabel) {
  noteEvolution.setSelection(std::move(cellLabel));
}

void MainWorkspaceComponent::setNoteEvolution(std::vector<NoteEvolutionPanel::GenerationEntry> entries) {
  noteEvolution.setEvolution(std::move(entries));
}

void MainWorkspaceComponent::setArrangerSections(std::vector<ArrangerPanel::SectionOption> sections) {
  arranger.setAvailableSections(std::move(sections));
}

void MainWorkspaceComponent::setArrangerActiveSectionId(juce::String id) {
  arranger.setActiveSectionId(std::move(id));
}

void MainWorkspaceComponent::setArrangerBlocks(std::vector<ArrangementBlockUI> arrangerBlocks) {
  arranger.setBlocks(std::move(arrangerBlocks));
}

void MainWorkspaceComponent::setRulePool(std::vector<RulePoolEntryUI> entries) {
  // A pool round-tripped through the engine only carries an id (the
  // bridge doesn't know about display names/descriptions) — recover the
  // curated name from the Library's presets where possible so the pool
  // list doesn't just show raw ids.
  for (auto& entry : entries) {
    const auto known = library.findRuleById(entry.rule.id);
    if (known.name.isNotEmpty()) entry.rule.name = known.name;
  }
  rulePool = std::move(entries);
  std::vector<juce::String> ids;
  ids.reserve(rulePool.size());
  for (const auto& entry : rulePool) ids.push_back(entry.rule.id);
  library.setEnabledRuleIds(ids);
  rules.setPool(rulePool);
}

ModulationPanel::ModulationPanel() : PanelComponent("Modulation editor", "STOCHASTIC + DRAWN") {
  addButton.onClick = [this] {
    modulatorCount += 1;
    repaint();
  };
  addAndMakeVisible(addButton);
}

void ModulationPanel::paint(juce::Graphics& g) {
  PanelComponent::paint(g);
  auto area = getContentBounds().toFloat().withTrimmedBottom(40.0f);
  const float sidebarWidth = std::min(150.0f, area.getWidth() * 0.24f);
  auto sidebar = area.removeFromLeft(sidebarWidth);
  area.removeFromLeft(12.0f);

  g.setFont(10.5f);
  for (int index = 0; index < modulatorCount; ++index) {
    const auto card = juce::Rectangle<float>(sidebar.getX(), sidebar.getY() + static_cast<float>(index) * 38.0f,
                                              sidebar.getWidth(), 30.0f);
    if (card.getBottom() > sidebar.getBottom()) break;
    g.setColour(index == 0 ? accentColour().withAlpha(0.17f) : secondaryAccentColour().withAlpha(0.11f));
    g.fillRoundedRectangle(card, 4.0f);
    g.setColour(index == 0 ? accentColour() : mutedTextColour());
    g.drawRoundedRectangle(card, 4.0f, 1.0f);
    g.drawText(index % 3 == 0 ? "Brownian drift " + juce::String(index + 1)
                              : index % 3 == 1 ? "Hand curve " + juce::String(index + 1)
                                               : "Probability walk " + juce::String(index + 1),
               card.reduced(8.0f).toNearestInt(), juce::Justification::centredLeft);
  }

  g.setColour(juce::Colour(0xff1b222b));
  g.fillRoundedRectangle(area, 5.0f);
  g.setColour(panelBorderColour());
  for (int line = 1; line < 8; ++line) {
    const float x = area.getX() + area.getWidth() * static_cast<float>(line) / 8.0f;
    g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
  }
  for (int line = 1; line < 4; ++line) {
    const float y = area.getY() + area.getHeight() * static_cast<float>(line) / 4.0f;
    g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
  }

  for (int curve = 0; curve < std::min(modulatorCount, 4); ++curve) {
    juce::Path path;
    for (int point = 0; point <= 120; ++point) {
      const float t = static_cast<float>(point) / 120.0f;
      const float curveIndex = static_cast<float>(curve);
      const float stochastic = std::sin(t * (7.0f + curveIndex * 2.0f) + curveIndex * 0.8f) * 0.18f
          + std::sin(t * (23.0f + curveIndex * 3.0f)) * 0.07f;
      const float drawn = 0.58f - t * 0.18f + stochastic + curveIndex * 0.045f;
      const float x = area.getX() + t * area.getWidth();
      const float y = area.getY() + std::clamp(drawn, 0.08f, 0.92f) * area.getHeight();
      if (point == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }
    g.setColour((curve % 2 == 0 ? accentColour() : secondaryAccentColour())
                    .withAlpha(0.92f - static_cast<float>(curve) * 0.13f));
    g.strokePath(path, juce::PathStrokeType(curve == 0 ? 2.2f : 1.4f, juce::PathStrokeType::curved));
  }
}

void ModulationPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  addButton.setBounds(area.removeFromBottom(30).removeFromLeft(150));
}

HumanizationPanel::HumanizationPanel(juce::RangedAudioParameter& humanizeAmount,
                                     juce::RangedAudioParameter& humanizeTimingEnabled,
                                     juce::RangedAudioParameter& ghostEnabled,
                                     juce::RangedAudioParameter& ghostProbability)
    : PanelComponent("Humanization", "LIVE PARAMETERS"),
      humanizeAttachment(humanizeAmount, humanizeSlider, nullptr),
      humanizeTimingAttachment(humanizeTimingEnabled, humanizeTimingButton, nullptr),
      ghostEnabledAttachment(ghostEnabled, ghostEnabledButton, nullptr),
      ghostProbabilityAttachment(ghostProbability, ghostProbabilitySlider, nullptr) {
  humanizeLabel.setText("VELOCITY + TIMING MOVEMENT", juce::dontSendNotification);
  ghostProbabilityLabel.setText("GHOST PROBABILITY", juce::dontSendNotification);
  styleCaption(humanizeLabel);
  styleCaption(ghostProbabilityLabel);
  humanizeLabel.setJustificationType(juce::Justification::centredLeft);
  ghostProbabilityLabel.setJustificationType(juce::Justification::centredLeft);

  humanizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  humanizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
  humanizeSlider.setRange(1.0, 40.0, 1.0);
  ghostProbabilitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
  ghostProbabilitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
  ghostProbabilitySlider.setRange(0.0, 1.0, 0.01);
  humanizeTimingButton.setTooltip("Also nudge note timing, scaled by the amount above — off by default so "
                                  "existing sessions keep their exact prior (velocity-only) feel.");

  addAndMakeVisible(humanizeLabel);
  addAndMakeVisible(humanizeSlider);
  addAndMakeVisible(humanizeTimingButton);
  addAndMakeVisible(ghostEnabledButton);
  addAndMakeVisible(ghostProbabilityLabel);
  addAndMakeVisible(ghostProbabilitySlider);
}

void HumanizationPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  humanizeLabel.setBounds(area.removeFromTop(20));
  humanizeSlider.setBounds(area.removeFromTop(34));
  area.removeFromTop(4);
  humanizeTimingButton.setBounds(area.removeFromTop(24));
  area.removeFromTop(4);
  ghostEnabledButton.setBounds(area.removeFromTop(28));
  ghostProbabilityLabel.setBounds(area.removeFromTop(20));
  ghostProbabilitySlider.setBounds(area.removeFromTop(34));
}

SilencerPanel::SilencerPanel() : PanelComponent("Silencer", "RESERVED") {}

void SilencerPanel::paint(juce::Graphics& g) {
  PanelComponent::paint(g);
  const auto area = getContentBounds();
  g.setColour(mutedTextColour().withAlpha(0.7f));
  g.setFont(11.0f);
  g.drawFittedText("Reserved for future performance and selective-muting tools.",
                   area.reduced(10), juce::Justification::centred, 3);
}

ModulationWorkspaceComponent::ModulationWorkspaceComponent(juce::RangedAudioParameter& humanizeAmount,
                                                           juce::RangedAudioParameter& humanizeTimingEnabled,
                                                           juce::RangedAudioParameter& ghostEnabled,
                                                           juce::RangedAudioParameter& ghostProbability)
    : humanization(humanizeAmount, humanizeTimingEnabled, ghostEnabled, ghostProbability) {
  addAndMakeVisible(modulation);
  addAndMakeVisible(humanization);
  addAndMakeVisible(silencer);
}

void ModulationWorkspaceComponent::resized() {
  constexpr int gap = 8;
  auto area = getLocalBounds().reduced(10);
  auto right = area.removeFromRight(
      std::clamp(static_cast<int>(static_cast<float>(area.getWidth()) * 0.34f), 300, 430));
  area.removeFromRight(gap);
  modulation.setBounds(area);
  humanization.setBounds(right.removeFromTop((right.getHeight() - gap) / 2));
  right.removeFromTop(gap);
  silencer.setBounds(right);
}

} // namespace lineage::ui
