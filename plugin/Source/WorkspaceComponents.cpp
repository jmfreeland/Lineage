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

std::vector<RulePreset> createRulePresets() {
  return {
      {"pocket-keeper", "Pocket Keeper", "Mostly holds; small, tasteful movement", 0.26, 0.12, 0.05, 0.57},
      {"gentle-drift", "Gentle Drift", "Evolution first, occasional ornaments", 0.52, 0.25, 0.08, 0.15},
      {"fill-forward", "Fill Forward", "Pushes branches toward phrase endings", 0.30, 0.22, 0.40, 0.08},
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
    g.setColour(noteColour.withAlpha(alpha));
    g.fillRoundedRectangle(juce::Rectangle<float>(x, y, noteWidth, 2.6f), 1.2f);
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

ArrangerPanel::ArrangerPanel() : PanelComponent("Arranger", "BLOCKS") {}

void ArrangerPanel::paint(juce::Graphics& g) {
  PanelComponent::paint(g);
  auto area = getContentBounds().toFloat();
  const juce::StringArray names{"A", "A'", "FILL", "B"};
  const float gap = 5.0f;
  const float widths[] = {0.28f, 0.22f, 0.16f, 0.28f};
  float x = area.getX();
  for (int index = 0; index < names.size(); ++index) {
    const float width = area.getWidth() * widths[index];
    const auto block = juce::Rectangle<float>(x, area.getY() + 6.0f, width,
                                               std::min(42.0f, area.getHeight() * 0.42f));
    g.setColour(index == 2 ? secondaryAccentColour().withAlpha(0.22f) : accentColour().withAlpha(0.13f));
    g.fillRoundedRectangle(block, 4.0f);
    g.setColour(index == 2 ? secondaryAccentColour() : panelBorderColour().brighter(0.25f));
    g.drawRoundedRectangle(block, 4.0f, 1.0f);
    g.setColour(textColour());
    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    g.drawText(names[index], block.toNearestInt(), juce::Justification::centred);
    x += width + gap;
  }
  const auto hint = area.withTrimmedTop(std::min(52.0f, area.getHeight() * 0.5f));
  g.setColour(mutedTextColour());
  g.setFont(10.0f);
  g.drawFittedText("Group lineage subtrees into reusable song blocks", hint.toNearestInt(),
                   juce::Justification::centredLeft, 2);
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
  for (auto& button : ruleButtons) button->setBounds(ruleArea.removeFromTop(36).reduced(0, 3));
}

RulePreset LibraryPanel::getSelectedRule() const {
  if (selectedRule >= 0 && selectedRule < static_cast<int>(rulePresets.size())) {
    return rulePresets[static_cast<size_t>(selectedRule)];
  }
  return {};
}

RuleControllerPanel::RuleControllerPanel() : PanelComponent("Rule controller", "TREE WEIGHTS") {
  const juce::StringArray names{"Mutation", "Embellish", "Fill", "Hold"};
  const double initialValues[] = {0.68, 0.42, 0.24, 0.55};
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
      if (updatingRule || sliders.size() < 4) return;
      currentRule.mutation = sliders[0]->getValue();
      currentRule.embellish = sliders[1]->getValue();
      currentRule.fill = sliders[2]->getValue();
      currentRule.hold = sliders[3]->getValue();
      if (onRuleChanged != nullptr) onRuleChanged(currentRule);
    };
    addAndMakeVisible(*slider);
    sliders.push_back(std::move(slider));
  }
}

void RuleControllerPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  activeRuleLabel.setBounds(area.removeFromTop(26));
  const int rowHeight = std::max(30, area.getHeight() / static_cast<int>(sliders.size()));
  for (size_t index = 0; index < sliders.size(); ++index) {
    auto row = area.removeFromTop(rowHeight);
    labels[index]->setBounds(row.removeFromLeft(72));
    sliders[index]->setBounds(row.reduced(2, 3));
  }
}

void RuleControllerPanel::setRulePreset(const RulePreset& preset) {
  currentRule = preset;
  activeRuleLabel.setText(preset.name + "  ·  " + preset.description, juce::dontSendNotification);
  if (sliders.size() < 4) return;
  updatingRule = true;
  sliders[0]->setValue(preset.mutation, juce::dontSendNotification);
  sliders[1]->setValue(preset.embellish, juce::dontSendNotification);
  sliders[2]->setValue(preset.fill, juce::dontSendNotification);
  sliders[3]->setValue(preset.hold, juce::dontSendNotification);
  updatingRule = false;
}

MainWorkspaceComponent::MainWorkspaceComponent() {
  seedEditor.onPatternChanged = [this](const auto& steps) {
    if (!loadingSeedPreset) evolutionTree.resetSeed("Custom Seed");
    if (onSeedPatternChanged != nullptr) onSeedPatternChanged(steps);
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
    if (evolutionTree.isAutoEvolutionRunning() && onAutoEvolutionChanged != nullptr) {
      onAutoEvolutionChanged(selectedRule, true, evolutionTree.getEvolutionFrequencyBars());
    }
  };
  rules.onRuleChanged = [this](const RulePreset& rule) {
    selectedRule = rule;
    if (evolutionTree.isAutoEvolutionRunning() && onAutoEvolutionChanged != nullptr) {
      onAutoEvolutionChanged(selectedRule, true, evolutionTree.getEvolutionFrequencyBars());
    }
  };
  evolutionTree.onEvolutionRequested = [this](bool branch) {
    if (onEvolutionRequested == nullptr) return;
    const auto operation = onEvolutionRequested(selectedRule, branch);
    if (operation.isNotEmpty()) evolutionTree.addEvolution(branch, selectedRule.name, operation);
  };
  evolutionTree.onAutoEvolutionChanged = [this](bool running, int frequencyBars) {
    if (onAutoEvolutionChanged != nullptr) onAutoEvolutionChanged(selectedRule, running, frequencyBars);
  };
  selectedRule = library.getSelectedRule();
  rules.setRulePreset(selectedRule);
  addAndMakeVisible(seedEditor);
  addAndMakeVisible(timeline);
  addAndMakeVisible(arranger);
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
  const int timelineHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.16f), 105, 130);
  const int arrangerHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.16f), 90, 130);
  seedEditor.setBounds(left.removeFromTop(seedHeight));
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
                                     juce::RangedAudioParameter& ghostEnabled,
                                     juce::RangedAudioParameter& ghostProbability)
    : PanelComponent("Humanization", "LIVE PARAMETERS"),
      humanizeAttachment(humanizeAmount, humanizeSlider, nullptr),
      ghostEnabledAttachment(ghostEnabled, ghostEnabledButton, nullptr),
      ghostProbabilityAttachment(ghostProbability, ghostProbabilitySlider, nullptr) {
  humanizeLabel.setText("VELOCITY MOVEMENT", juce::dontSendNotification);
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

  addAndMakeVisible(humanizeLabel);
  addAndMakeVisible(humanizeSlider);
  addAndMakeVisible(ghostEnabledButton);
  addAndMakeVisible(ghostProbabilityLabel);
  addAndMakeVisible(ghostProbabilitySlider);
}

void HumanizationPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  humanizeLabel.setBounds(area.removeFromTop(20));
  humanizeSlider.setBounds(area.removeFromTop(34));
  area.removeFromTop(8);
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
                                                           juce::RangedAudioParameter& ghostEnabled,
                                                           juce::RangedAudioParameter& ghostProbability)
    : humanization(humanizeAmount, ghostEnabled, ghostProbability) {
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
