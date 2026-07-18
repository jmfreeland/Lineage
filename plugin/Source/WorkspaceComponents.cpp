#include "WorkspaceComponents.h"

#include <algorithm>
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

TimelinePanel::TimelinePanel() : PanelComponent("Current + next", "8 BARS") {}

void TimelinePanel::paint(juce::Graphics& g) {
  PanelComponent::paint(g);
  auto area = getContentBounds().toFloat();
  const float labelWidth = 54.0f;
  const float gap = 4.0f;
  const float barWidth = (area.getWidth() - labelWidth - gap * 7.0f) / 8.0f;
  g.setFont(10.0f);
  g.setColour(mutedTextColour());
  g.drawText("NOW", area.removeFromLeft(labelWidth).toNearestInt(), juce::Justification::centredLeft);
  for (int bar = 0; bar < 8; ++bar) {
    const auto box = juce::Rectangle<float>(area.getX() + static_cast<float>(bar) * (barWidth + gap), area.getY() + 5.0f,
                                             barWidth, area.getHeight() - 10.0f);
    g.setColour(bar < 4 ? accentColour().withAlpha(0.18f) : secondaryAccentColour().withAlpha(0.12f));
    g.fillRoundedRectangle(box, 3.0f);
    g.setColour(bar < 4 ? accentColour().withAlpha(0.7f) : panelBorderColour().brighter(0.2f));
    g.drawRoundedRectangle(box, 3.0f, 1.0f);
    g.setColour(bar < 4 ? textColour() : mutedTextColour());
    g.drawText(juce::String(bar + 1), box.toNearestInt(), juce::Justification::centred);
  }
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
  nodes.push_back({"Seed A", -1, false});
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

    const juce::StringArray lanes{"KICK", "SNARE", "HATS"};
    const float chipWidth = (node.getWidth() - 32.0f) / 3.0f;
    for (int lane = 0; lane < lanes.size(); ++lane) {
      const auto chip = juce::Rectangle<float>(node.getX() + 10.0f + static_cast<float>(lane) * (chipWidth + 6.0f),
                                                node.getY() + 36.0f, chipWidth, 20.0f);
      g.setColour(nodeAccent.withAlpha(0.10f + static_cast<float>(lane) * 0.04f));
      g.fillRoundedRectangle(chip, 3.0f);
      g.setColour(mutedTextColour());
      g.setFont(8.5f);
      g.drawText(lanes[lane], chip.toNearestInt(), juce::Justification::centred);
    }
  }
}

void EvolutionCanvas::addEvolution(bool branch) {
  const int parent = std::max(0, static_cast<int>(nodes.size()) - (branch ? 2 : 1));
  const auto number = static_cast<int>(nodes.size());
  nodes.push_back({branch ? "Variation " + juce::String(number) : "Evolution " + juce::String(number),
                   parent,
                   branch});
  repaint();
}

int EvolutionCanvas::getRequiredHeight() const {
  return 32 + static_cast<int>(nodes.size()) * 94;
}

EvolutionTreePanel::EvolutionTreePanel() : PanelComponent("Seed evolutions", "LINEAGE TREE") {
  viewport.setViewedComponent(&canvas, false);
  viewport.setScrollBarsShown(true, false);
  viewport.setColour(juce::ScrollBar::thumbColourId, panelBorderColour().brighter(0.25f));
  evolveButton.onClick = [this] {
    canvas.addEvolution(false);
    updateCanvasSize();
    viewport.setViewPositionProportionately(0.0, 1.0);
  };
  branchButton.onClick = [this] {
    canvas.addEvolution(true);
    updateCanvasSize();
    viewport.setViewPositionProportionately(0.0, 1.0);
  };
  addAndMakeVisible(viewport);
  addAndMakeVisible(evolveButton);
  addAndMakeVisible(branchButton);
}

void EvolutionTreePanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  auto controls = area.removeFromBottom(34);
  branchButton.setBounds(controls.removeFromRight(90).reduced(2));
  evolveButton.setBounds(controls.removeFromRight(90).reduced(2));
  viewport.setBounds(area.withTrimmedBottom(6));
  updateCanvasSize();
}

void EvolutionTreePanel::updateCanvasSize() {
  canvas.setSize(std::max(320, viewport.getMaximumVisibleWidth()),
                 std::max(viewport.getMaximumVisibleHeight(), canvas.getRequiredHeight()));
}

LibraryPanel::LibraryPanel() : PanelComponent("Library", "PROJECT") {
  searchBox.setTextToShowWhenEmpty("Search grooves + evolutions", mutedTextColour());
  searchBox.setFont(juce::Font(juce::FontOptions(11.0f)));
  addAndMakeVisible(searchBox);

  const juce::StringArray names{"Deep pocket 01", "Broken hats", "Half-time pull  | evolution",
                                 "Ghost-note lift  | evolution"};
  for (int index = 0; index < names.size(); ++index) {
    auto entry = std::make_unique<juce::ToggleButton>(names[index]);
    entry->setToggleState(index != 3, juce::dontSendNotification);
    entry->setTooltip(index < 2 ? "Groove available to the project" : "Include this evolution in the project");
    addAndMakeVisible(*entry);
    entries.push_back(std::move(entry));
  }
}

void LibraryPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  searchBox.setBounds(area.removeFromTop(28));
  area.removeFromTop(6);
  for (auto& entry : entries) entry->setBounds(area.removeFromTop(30));
}

RuleControllerPanel::RuleControllerPanel() : PanelComponent("Rule controller", "TREE WEIGHTS") {
  const juce::StringArray names{"Mutation", "Embellish", "Fill", "Hold"};
  const double initialValues[] = {0.68, 0.42, 0.24, 0.55};
  for (int index = 0; index < names.size(); ++index) {
    auto label = std::make_unique<juce::Label>();
    label->setText(names[index], juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, index == 3 ? secondaryAccentColour() : textColour());
    label->setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(*label);
    labels.push_back(std::move(label));

    auto slider = std::make_unique<juce::Slider>();
    configureRuleSlider(*slider, initialValues[index]);
    addAndMakeVisible(*slider);
    sliders.push_back(std::move(slider));
  }
}

void RuleControllerPanel::resized() {
  PanelComponent::resized();
  auto area = getContentBounds();
  const int rowHeight = std::max(30, area.getHeight() / static_cast<int>(sliders.size()));
  for (size_t index = 0; index < sliders.size(); ++index) {
    auto row = area.removeFromTop(rowHeight);
    labels[index]->setBounds(row.removeFromLeft(72));
    sliders[index]->setBounds(row.reduced(2, 3));
  }
}

MainWorkspaceComponent::MainWorkspaceComponent() {
  seedEditor.onPatternChanged = [this](const auto& steps) {
    if (onSeedPatternChanged != nullptr) onSeedPatternChanged(steps);
  };
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
  const int leftWidth = std::clamp(static_cast<int>(static_cast<float>(area.getWidth()) * 0.27f), 270, 360);
  const int rightWidth = std::clamp(static_cast<int>(static_cast<float>(area.getWidth()) * 0.24f), 245, 330);
  auto left = area.removeFromLeft(leftWidth);
  area.removeFromLeft(gap);
  auto right = area.removeFromRight(rightWidth);
  area.removeFromRight(gap);
  auto centre = area;

  const int seedHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.29f), 170, 220);
  const int timelineHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.16f), 105, 130);
  const int arrangerHeight = std::clamp(static_cast<int>(static_cast<float>(left.getHeight()) * 0.25f), 140, 190);
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
  seedEditor.sendCurrentPattern();
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
