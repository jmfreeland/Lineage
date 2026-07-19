#include "PluginEditor.h"

namespace {
JsEngine::EvolutionRule toEngineRule(const lineage::ui::RulePreset& rule) {
  JsEngine::EvolutionRule engineRule;
  engineRule.id = rule.id.toStdString();
  engineRule.mutation = rule.mutation;
  engineRule.embellish = rule.embellish;
  engineRule.fill = rule.fill;
  engineRule.hold = rule.hold;
  engineRule.settle = rule.settle;
  engineRule.params.reserve(rule.paramValues.size());
  for (const auto& [key, value] : rule.paramValues) engineRule.params.emplace_back(key.toStdString(), value);
  return engineRule;
}
} // namespace

LineageAudioProcessorEditor::LineageAudioProcessorEditor(LineageAudioProcessor& processorIn)
    : AudioProcessorEditor(&processorIn),
      processorRef(processorIn),
      modulationWorkspace(processorIn.getHumanizeAmountParameter(),
                          processorIn.getHumanizeTimingEnabledParameter(),
                          processorIn.getGhostNoteEnabledParameter(),
                          processorIn.getGhostNoteProbabilityParameter()) {
  setLookAndFeel(&lookAndFeel);

  titleLabel.setText("LINEAGE", juce::dontSendNotification);
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  titleLabel.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
  titleLabel.setColour(juce::Label::textColourId, lineage::ui::textColour());
  addAndMakeVisible(titleLabel);

  statusLabel.setText("v" + juce::String(JucePlugin_VersionString) + "   |   AUTO EVOLUTION   |   HOST BARS",
                       juce::dontSendNotification);
  statusLabel.setJustificationType(juce::Justification::centredRight);
  statusLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
  statusLabel.setColour(juce::Label::textColourId, lineage::ui::secondaryAccentColour());
  addAndMakeVisible(statusLabel);

  mainWorkspace.onSeedPatternChanged = [this](const std::vector<StepSequencerComponent::SeedLane>& lanes) {
    std::vector<JsEngine::SeedLane> seedLanes;
    seedLanes.reserve(lanes.size());
    for (const auto& lane : lanes) {
      JsEngine::SeedLane seedLane;
      seedLane.id = lane.id.toStdString();
      seedLane.name = lane.name.toStdString();
      seedLane.midiNote = lane.midiNote;
      seedLane.group = lane.group.toStdString();
      seedLane.velocity = lane.velocity;
      seedLane.activeSteps.reserve(lane.activeSteps.size());
      for (const int step : lane.activeSteps) seedLane.activeSteps.push_back(step);
      seedLanes.push_back(std::move(seedLane));
    }
    processorRef.setSeedGroove(seedLanes);
  };
  mainWorkspace.onEvolutionRequested = [this](const lineage::ui::RulePreset& rule, bool branch) {
    JsEngine::EvolutionResult result;
    if (!processorRef.evolveWithRule(toEngineRule(rule), branch, result)) return juce::String();
    refreshTimelinePreview();
    return juce::String(result.operation);
  };
  mainWorkspace.onAutoEvolutionChanged = [this](const lineage::ui::RulePreset& rule,
                                                 bool running,
                                                 int frequencyBars) {
    processorRef.configureAutoEvolution(toEngineRule(rule), rule.name, running, frequencyBars);
    refreshTimelinePreview();
  };

  tabs.addTab("LINEAGE", lineage::ui::backgroundColour(), &mainWorkspace, false);
  tabs.addTab("MODULATION", lineage::ui::backgroundColour(), &modulationWorkspace, false);
  tabs.setComponentID("workspaceTabs");
  tabs.setTabBarDepth(32);
  tabs.setOutline(0);
  addAndMakeVisible(tabs);

  // SectionBarComponent rebuilds (destroys and recreates) its tab buttons
  // whenever the section list changes — including the very button that was
  // just clicked. Doing that synchronously from inside that button's own
  // onClick would destroy it while it's still on the call stack, so the
  // rebuild is deferred to the next message-loop iteration via callAsync,
  // guarded by a SafePointer in case the editor closes in the meantime.
  sectionBar.onCreateSection = [this] {
    processorRef.createSection();
    juce::Component::SafePointer<LineageAudioProcessorEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
      if (safeThis == nullptr) return;
      safeThis->refreshSections();
      safeThis->refreshTimelinePreview();
    });
  };
  sectionBar.onSelectSection = [this](const juce::String& id) {
    if (!processorRef.selectSection(id)) return;
    juce::Component::SafePointer<LineageAudioProcessorEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
      if (safeThis == nullptr) return;
      safeThis->refreshSections();
      safeThis->refreshTimelinePreview();
    });
  };
  sectionBar.onDeleteSection = [this](const juce::String& id) {
    processorRef.deleteSection(id);
    juce::Component::SafePointer<LineageAudioProcessorEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
      if (safeThis == nullptr) return;
      safeThis->refreshSections();
      safeThis->refreshTimelinePreview();
    });
  };
  addAndMakeVisible(sectionBar);

  setResizable(true, true);
  setResizeLimits(900, 620, 1800, 1200);
  setSize(1240, 760);
  mainWorkspace.sendCurrentSeed();
  refreshSections();
  refreshTimelinePreview();
  startTimerHz(5);
}

LineageAudioProcessorEditor::~LineageAudioProcessorEditor() {
  stopTimer();
  tabs.clearTabs();
  setLookAndFeel(nullptr);
}

void LineageAudioProcessorEditor::timerCallback() {
  for (const auto& event : processorRef.drainAutoEvolutionEvents()) {
    mainWorkspace.addAutomaticEvolution(event.ruleName, event.operation);
  }
  refreshTimelinePreview();
}

void LineageAudioProcessorEditor::refreshSections() {
  const auto engineSections = processorRef.listSections();
  std::vector<lineage::ui::SectionBarComponent::SectionInfo> uiSections;
  uiSections.reserve(engineSections.size());
  juce::String activeName;
  for (const auto& section : engineSections) {
    uiSections.push_back({juce::String(section.id), juce::String(section.name), section.active});
    if (section.active) activeName = juce::String(section.name);
  }
  sectionBar.setSections(uiSections);
  if (activeName.isNotEmpty()) mainWorkspace.notifySectionChanged(activeName);
}

void LineageAudioProcessorEditor::refreshTimelinePreview() {
  const auto processorPreview = processorRef.getPlaybackPreview(8);
  lineage::ui::TimelinePanel::Preview preview;
  preview.startBeat = processorPreview.startBeat;
  preview.beatsPerBar = processorPreview.beatsPerBar;
  preview.playheadBeat = processorPreview.playheadBeat;
  preview.transportPlaying = processorPreview.transportPlaying;
  preview.notes.reserve(processorPreview.events.size());
  for (const auto& event : processorPreview.events) {
    preview.notes.push_back({event.note,
                             event.velocity,
                             event.beatPosition,
                             event.durationBeats,
                             event.previewFlags});
  }
  mainWorkspace.setTimelinePreview(std::move(preview));
}

void LineageAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(lineage::ui::backgroundColour());
  juce::ColourGradient headerGradient(juce::Colour(0xff171c23), 0.0f, 0.0f,
                                      lineage::ui::backgroundColour(), static_cast<float>(getWidth()), 0.0f, false);
  g.setGradientFill(headerGradient);
  g.fillRect(getLocalBounds().removeFromTop(42));
}

void LineageAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();
  auto header = bounds.removeFromTop(42).reduced(14, 0);
  titleLabel.setBounds(header.removeFromLeft(180));
  statusLabel.setBounds(header.removeFromRight(300));
  sectionBar.setBounds(header.reduced(0, 7));
  tabs.setBounds(bounds);
}
