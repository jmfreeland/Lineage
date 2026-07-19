#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

#include <algorithm>
#include <cmath>

LineageAudioProcessor::LineageAudioProcessor()
    : AudioProcessor(BusesProperties()) {
  addParameter(humanizeAmountParam =
                   new juce::AudioParameterInt({"humanizeAmount", 1}, "Humanize Amount", 1, 40, 12));
  addParameter(humanizeTimingEnabledParam =
                   new juce::AudioParameterBool({"humanizeTimingEnabled", 1}, "Humanize Timing", false));
  addParameter(ghostNoteEnabledParam =
                   new juce::AudioParameterBool({"ghostNoteEnabled", 1}, "Ghost Notes", false));
  addParameter(ghostNoteProbabilityParam = new juce::AudioParameterFloat(
                   {"ghostNoteProbability", 1}, "Ghost Note Probability", 0.0f, 1.0f, 0.25f));

  const std::string bundleSource(BinaryData::runtime_bundle_js,
                                  static_cast<size_t>(BinaryData::runtime_bundle_jsSize));
  std::string error;
  jsEngineReady = jsEngine.loadScript(bundleSource, "runtime.bundle.js", error);
  if (!jsEngineReady) {
    juce::Logger::writeToLog("Lineage: failed to load embedded engine runtime: " + juce::String(error));
  }
}

LineageAudioProcessor::~LineageAudioProcessor() = default;

void LineageAudioProcessor::prepareToPlay(double, int) {
  pendingPlaybackNoteOffs.clear();
  wasTransportPlaying = false;
  havePreviousBlockPosition = false;
  autoEvolutionScheduleReset.store(true, std::memory_order_relaxed);
  const juce::SpinLock::ScopedLockType eventLock(autoEvolutionEventLock);
  pendingAutoEvolutionEvents.clear();
}

void LineageAudioProcessor::releaseResources() {
  pendingPlaybackNoteOffs.clear();
  wasTransportPlaying = false;
  havePreviousBlockPosition = false;
  autoEvolutionScheduleReset.store(true, std::memory_order_relaxed);
}

bool LineageAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  // No audio buses — this is a MIDI effect.
  return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled()
      && layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled();
}

void LineageAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
  if (!jsEngineReady) return; // leave midiMessages as pure passthrough if the runtime failed to load

  // Host transport (DESIGN.md §11): read tempo/time-signature/position so
  // notes can be placed against the real bar grid instead of a made-up
  // per-block scheme. Falls back to 120bpm/4-4 if the host doesn't report
  // position (e.g. some standalone/offline contexts).
  double tempo = 120.0;
  double beatsPerBar = 4.0;
  double blockStartBeat = 0.0;
  bool isTransportPlaying = false;
  if (auto* currentPlayHead = getPlayHead()) {
    if (auto position = currentPlayHead->getPosition()) {
      tempo = position->getBpm().orFallback(120.0);
      blockStartBeat = position->getPpqPosition().orFallback(0.0);
      isTransportPlaying = position->getIsPlaying();
      if (auto signature = position->getTimeSignature()) {
        beatsPerBar = static_cast<double>(signature->numerator) * (4.0 / static_cast<double>(signature->denominator));
      }
    }
  }
  const double sampleRate = getSampleRate();
  const int numSamples = buffer.getNumSamples();
  const double beatsInBlock = sampleRate > 0.0 && tempo > 0.0
      ? (static_cast<double>(numSamples) / sampleRate) * (tempo / 60.0)
      : 0.0;
  const double blockEndBeat = blockStartBeat + beatsInBlock;
  const JsEngine::Transport transport{tempo, beatsPerBar, blockStartBeat, sampleRate};
  latestBlockStartBeat.store(blockStartBeat, std::memory_order_relaxed);
  latestBeatsPerBar.store(beatsPerBar, std::memory_order_relaxed);
  latestTransportPlaying.store(isTransportPlaying, std::memory_order_relaxed);
  const bool transportDiscontinuity = isTransportPlaying && havePreviousBlockPosition
      && std::abs(blockStartBeat - previousBlockEndBeat) > 1.0e-4;

  std::vector<JsEngine::MidiEvent> noteOnEvents;
  std::vector<std::pair<juce::MidiMessage, int>> passthroughMessages;

  for (const auto metadata : midiMessages) {
    const auto message = metadata.getMessage();
    if (message.isNoteOn()) {
      const double beatsIntoBlock =
          sampleRate > 0.0 ? (static_cast<double>(metadata.samplePosition) / sampleRate) * (tempo / 60.0) : 0.0;
      JsEngine::MidiEvent event;
      event.note = message.getNoteNumber();
      event.velocity = message.getVelocity();
      event.channel = message.getChannel();
      event.samplePosition = metadata.samplePosition;
      event.beatPosition = blockStartBeat + beatsIntoBlock;
      noteOnEvents.push_back(event);
    } else {
      passthroughMessages.emplace_back(message, metadata.samplePosition);
    }
  }

  if (!noteOnEvents.empty()) {
    std::string error;
    const auto params = getPlaybackParams();
    // On failure, processBlock() leaves noteOnEvents untouched, so this
    // degrades to passthrough for note-ons rather than dropping them.
    // Locked against setSeedGroove(), which can be called concurrently
    // from the editor on the message thread — jsEngine/QuickJS isn't
    // thread-safe on its own. A lock on the audio thread isn't ideal
    // real-time practice, but it's the minimal correct fix for an outright
    // data race, consistent with JsEngine's already-documented GC-pause
    // MVP tradeoff.
    const juce::ScopedLock lock(jsEngineLock);
    jsEngine.processBlock(noteOnEvents, transport, params, error);
  }

  std::vector<JsEngine::MidiEvent> playbackEvents;
  AutoEvolutionConfig currentAutoConfig;
  {
    const juce::SpinLock::ScopedLockType configLock(autoEvolutionConfigLock);
    currentAutoConfig = autoEvolutionConfig;
  }
  bool shouldAutoEvolve = false;
  int64_t nextBarAfterAutomaticEvolution = 0;
  double autoEvolutionTransitionBeat = blockStartBeat;
  if (!isTransportPlaying || !currentAutoConfig.running) {
    autoEvolutionScheduleReset.store(true, std::memory_order_relaxed);
  } else {
    const auto currentBar = static_cast<int64_t>(std::floor(blockStartBeat / std::max(0.25, beatsPerBar)));
    const bool resetSchedule = autoEvolutionScheduleReset.exchange(false, std::memory_order_relaxed)
        || !wasTransportPlaying || transportDiscontinuity;
    if (resetSchedule) {
      nextAutoEvolutionBar.store(
          currentBar + std::max<int32_t>(1, currentAutoConfig.frequencyBars), std::memory_order_relaxed);
    } else {
      const double scheduledBeat = static_cast<double>(
          nextAutoEvolutionBar.load(std::memory_order_relaxed)) * beatsPerBar;
      if (blockStartBeat >= scheduledBeat - 1.0e-9 || blockEndBeat > scheduledBeat + 1.0e-9) {
        shouldAutoEvolve = true;
        autoEvolutionTransitionBeat = std::clamp(scheduledBeat, blockStartBeat, blockEndBeat);
        const auto transitionBar = static_cast<int64_t>(
            std::floor(autoEvolutionTransitionBeat / std::max(0.25, beatsPerBar)));
        nextBarAfterAutomaticEvolution =
            transitionBar + std::max<int32_t>(1, currentAutoConfig.frequencyBars);
      }
    }
  }

  JsEngine::EvolutionResult automaticEvolution;
  bool automaticEvolutionSucceeded = false;
  if (isTransportPlaying && numSamples > 0) {
    std::string error;
    const juce::ScopedLock lock(jsEngineLock);
    const bool splitAtEvolution = shouldAutoEvolve && numSamples > 1 && tempo > 0.0 && sampleRate > 0.0
        && autoEvolutionTransitionBeat > blockStartBeat + 1.0e-9
        && autoEvolutionTransitionBeat < blockEndBeat - 1.0e-9;
    if (splitAtEvolution) {
      const int samplesBeforeEvolution = std::clamp(
          static_cast<int>(std::llround((autoEvolutionTransitionBeat - blockStartBeat)
                                        * (60.0 / tempo) * sampleRate)),
          1,
          numSamples - 1);
      if (!jsEngine.renderPlaybackBlock(
              playbackEvents, transport, samplesBeforeEvolution, getPlaybackParams(), error)) {
        juce::Logger::writeToLog("Lineage: failed to render pre-evolution playback: " + juce::String(error));
        error.clear();
      }

      automaticEvolutionSucceeded = jsEngine.evolveWithRule(
          currentAutoConfig.rule, false, automaticEvolution, error);
      if (!automaticEvolutionSucceeded) {
        juce::Logger::writeToLog("Lineage: failed automatic evolution: " + juce::String(error));
        error.clear();
      }

      std::vector<JsEngine::MidiEvent> afterEvolution;
      const JsEngine::Transport afterTransport{
          tempo, beatsPerBar, autoEvolutionTransitionBeat, sampleRate};
      if (!jsEngine.renderPlaybackBlock(afterEvolution,
                                        afterTransport,
                                        numSamples - samplesBeforeEvolution,
                                        getPlaybackParams(),
                                        error)) {
        juce::Logger::writeToLog("Lineage: failed to render post-evolution playback: " + juce::String(error));
      } else {
        for (auto& event : afterEvolution) event.samplePosition += samplesBeforeEvolution;
        playbackEvents.insert(playbackEvents.end(), afterEvolution.begin(), afterEvolution.end());
      }
    } else {
      if (shouldAutoEvolve) {
        automaticEvolutionSucceeded = jsEngine.evolveWithRule(
            currentAutoConfig.rule, false, automaticEvolution, error);
        if (!automaticEvolutionSucceeded) {
          juce::Logger::writeToLog("Lineage: failed automatic evolution: " + juce::String(error));
          error.clear();
        }
      }
      if (!jsEngine.renderPlaybackBlock(playbackEvents, transport, numSamples, getPlaybackParams(), error)) {
        juce::Logger::writeToLog("Lineage: failed to render playback block: " + juce::String(error));
      }
    }
    if (shouldAutoEvolve) {
      nextAutoEvolutionBar.store(nextBarAfterAutomaticEvolution, std::memory_order_relaxed);
    }
  }
  if (automaticEvolutionSucceeded) {
    const juce::SpinLock::ScopedLockType eventLock(autoEvolutionEventLock);
    pendingAutoEvolutionEvents.push_back(
        {currentAutoConfig.ruleName, juce::String(automaticEvolution.operation)});
  }

  // A mutation (ghostNote) can place a note's computed sample position
  // outside this block — its offset may land before this block started or
  // after it ends. There's no cross-block MIDI scheduler yet, so those get
  // dropped here rather than misfired at a clamped, wrong position.
  juce::MidiBuffer output;
  for (const auto& event : noteOnEvents) {
    if (event.samplePosition < 0 || event.samplePosition >= numSamples) continue;
    output.addEvent(juce::MidiMessage::noteOn(event.channel, event.note, static_cast<juce::uint8>(event.velocity)),
                     event.samplePosition);
  }
  for (const auto& [message, samplePosition] : passthroughMessages) {
    output.addEvent(message, samplePosition);
  }

  struct ScheduledPlaybackMessage {
    juce::MidiMessage message;
    int samplePosition = 0;
  };
  std::vector<ScheduledPlaybackMessage> scheduledPlayback;

  if ((!isTransportPlaying && wasTransportPlaying) || transportDiscontinuity) {
    for (const auto& pending : pendingPlaybackNoteOffs) {
      scheduledPlayback.push_back({juce::MidiMessage::noteOff(pending.channel, pending.note), 0});
    }
    pendingPlaybackNoteOffs.clear();
  }

  if (isTransportPlaying) {
    auto pending = pendingPlaybackNoteOffs.begin();
    while (pending != pendingPlaybackNoteOffs.end()) {
      if (pending->beatPosition < blockEndBeat) {
        const double beatsFromStart = std::max(0.0, pending->beatPosition - blockStartBeat);
        const int samplePosition = sampleRate > 0.0 && tempo > 0.0
            ? std::clamp(static_cast<int>(std::llround(beatsFromStart * (60.0 / tempo) * sampleRate)),
                         0,
                         std::max(0, numSamples - 1))
            : 0;
        scheduledPlayback.push_back(
            {juce::MidiMessage::noteOff(pending->channel, pending->note), samplePosition});
        pending = pendingPlaybackNoteOffs.erase(pending);
      } else {
        ++pending;
      }
    }

    for (const auto& event : playbackEvents) {
      if (event.samplePosition < 0 || event.samplePosition >= numSamples) continue;
      scheduledPlayback.push_back(
          {juce::MidiMessage::noteOn(event.channel, event.note, static_cast<juce::uint8>(event.velocity)),
           event.samplePosition});

      const double noteOffBeat = event.beatPosition + std::max(event.durationBeats, 1.0 / 960.0);
      if (noteOffBeat < blockEndBeat) {
        const int noteOffSample = std::clamp(
            static_cast<int>(std::llround((noteOffBeat - blockStartBeat) * (60.0 / tempo) * sampleRate)),
            0,
            std::max(0, numSamples - 1));
        scheduledPlayback.push_back(
            {juce::MidiMessage::noteOff(event.channel, event.note), noteOffSample});
      } else {
        pendingPlaybackNoteOffs.push_back({noteOffBeat, event.note, event.channel});
      }
    }
  }

  std::stable_sort(scheduledPlayback.begin(), scheduledPlayback.end(), [](const auto& left, const auto& right) {
    if (left.samplePosition != right.samplePosition) return left.samplePosition < right.samplePosition;
    if (left.message.isNoteOff() != right.message.isNoteOff()) return left.message.isNoteOff();
    return left.message.getNoteNumber() < right.message.getNoteNumber();
  });
  for (const auto& scheduled : scheduledPlayback) {
    output.addEvent(scheduled.message, scheduled.samplePosition);
  }

  wasTransportPlaying = isTransportPlaying;
  havePreviousBlockPosition = isTransportPlaying;
  if (isTransportPlaying) previousBlockEndBeat = blockEndBeat;

  midiMessages.swapWith(output);
}

void LineageAudioProcessor::resetAutoEvolutionForContextChange() {
  {
    const juce::SpinLock::ScopedLockType configLock(autoEvolutionConfigLock);
    autoEvolutionConfig.running = false;
    autoEvolutionConfig.frequencyBars = 4;
  }
  autoEvolutionScheduleReset.store(true, std::memory_order_relaxed);
  nextAutoEvolutionBar.store(0, std::memory_order_relaxed);
  {
    const juce::SpinLock::ScopedLockType eventLock(autoEvolutionEventLock);
    pendingAutoEvolutionEvents.clear();
  }
}

void LineageAudioProcessor::setSeedGroove(const std::vector<JsEngine::SeedLane>& lanes) {
  if (!jsEngineReady) return;
  resetAutoEvolutionForContextChange();
  {
    const juce::ScopedLock lock(jsEngineLock);
    std::string error;
    if (!jsEngine.setSeedGroove(lanes, 16, 4, error)) {
      juce::Logger::writeToLog("Lineage: failed to set seed groove: " + juce::String(error));
    }
  }
}

JsEngine::SectionInfo LineageAudioProcessor::createSection() {
  JsEngine::SectionInfo info;
  if (!jsEngineReady) return info;
  resetAutoEvolutionForContextChange();
  const juce::ScopedLock lock(jsEngineLock);
  std::string error;
  if (!jsEngine.createSection(info, error)) {
    juce::Logger::writeToLog("Lineage: failed to create section: " + juce::String(error));
  }
  return info;
}

std::vector<JsEngine::SectionInfo> LineageAudioProcessor::listSections() {
  std::vector<JsEngine::SectionInfo> sections;
  if (!jsEngineReady) return sections;
  const juce::ScopedLock lock(jsEngineLock);
  std::string error;
  if (!jsEngine.listSections(sections, error)) {
    juce::Logger::writeToLog("Lineage: failed to list sections: " + juce::String(error));
  }
  return sections;
}

bool LineageAudioProcessor::selectSection(const juce::String& id) {
  if (!jsEngineReady) return false;
  resetAutoEvolutionForContextChange();
  const juce::ScopedLock lock(jsEngineLock);
  std::string error;
  if (!jsEngine.selectSection(id.toStdString(), error)) {
    juce::Logger::writeToLog("Lineage: failed to select section: " + juce::String(error));
    return false;
  }
  return true;
}

bool LineageAudioProcessor::deleteSection(const juce::String& id) {
  if (!jsEngineReady) return false;
  // Deleting the active section switches the active section on the JS
  // side; the schedule may no longer describe the section now active, so
  // reset it unconditionally rather than tracking which id was active.
  resetAutoEvolutionForContextChange();
  const juce::ScopedLock lock(jsEngineLock);
  std::string error;
  if (!jsEngine.deleteSection(id.toStdString(), error)) {
    juce::Logger::writeToLog("Lineage: failed to delete section: " + juce::String(error));
    return false;
  }
  return true;
}

std::vector<std::pair<std::string, double>> LineageAudioProcessor::getPlaybackParams() const {
  return {{"humanizeAmount", static_cast<double>(humanizeAmountParam->get())},
          {"humanizeTimingEnabled", humanizeTimingEnabledParam->get() ? 1.0 : 0.0},
          {"ghostNoteEnabled", ghostNoteEnabledParam->get() ? 1.0 : 0.0},
          {"ghostNoteProbability", static_cast<double>(ghostNoteProbabilityParam->get())}};
}

LineageAudioProcessor::PlaybackPreview LineageAudioProcessor::getPlaybackPreview(int32_t barCount) {
  PlaybackPreview preview;
  preview.beatsPerBar = std::max(0.25, latestBeatsPerBar.load(std::memory_order_relaxed));
  preview.playheadBeat = latestBlockStartBeat.load(std::memory_order_relaxed);
  preview.transportPlaying = latestTransportPlaying.load(std::memory_order_relaxed);
  preview.startBeat = std::floor(preview.playheadBeat / preview.beatsPerBar) * preview.beatsPerBar;
  if (!jsEngineReady) return preview;

  AutoEvolutionConfig currentAutoConfig;
  {
    const juce::SpinLock::ScopedLockType configLock(autoEvolutionConfigLock);
    currentAutoConfig = autoEvolutionConfig;
  }
  JsEngine::AutoEvolutionPreview autoPreview;
  autoPreview.running = currentAutoConfig.running;
  autoPreview.rule = currentAutoConfig.rule;
  autoPreview.nextEvolutionBar = nextAutoEvolutionBar.load(std::memory_order_relaxed);
  autoPreview.frequencyBars = currentAutoConfig.frequencyBars;

  std::string error;
  const juce::ScopedLock lock(jsEngineLock);
  if (!jsEngine.renderPlaybackPreview(preview.events,
                                      preview.startBeat,
                                      preview.beatsPerBar,
                                      std::clamp(barCount, 1, 32),
                                      getPlaybackParams(),
                                      error,
                                      currentAutoConfig.running ? &autoPreview : nullptr)) {
    juce::Logger::writeToLog("Lineage: failed to render playback preview: " + juce::String(error));
    preview.events.clear();
  }
  return preview;
}

bool LineageAudioProcessor::evolveWithRule(const JsEngine::EvolutionRule& rule,
                                            bool branch,
                                            JsEngine::EvolutionResult& resultOut) {
  if (!jsEngineReady) return false;
  std::string error;
  const juce::ScopedLock lock(jsEngineLock);
  if (!jsEngine.evolveWithRule(rule, branch, resultOut, error)) {
    juce::Logger::writeToLog("Lineage: failed to evolve with rule: " + juce::String(error));
    return false;
  }
  return true;
}

void LineageAudioProcessor::configureAutoEvolution(const JsEngine::EvolutionRule& rule,
                                                    juce::String ruleName,
                                                    bool running,
                                                    int32_t frequencyBars) {
  bool scheduleChanged = false;
  {
    const juce::SpinLock::ScopedLockType configLock(autoEvolutionConfigLock);
    const int32_t safeFrequency = std::clamp(frequencyBars, 1, 64);
    scheduleChanged = autoEvolutionConfig.running != running
        || autoEvolutionConfig.frequencyBars != safeFrequency;
    autoEvolutionConfig.rule = rule;
    autoEvolutionConfig.ruleName = std::move(ruleName);
    autoEvolutionConfig.running = running;
    autoEvolutionConfig.frequencyBars = safeFrequency;
  }
  if (scheduleChanged) {
    const double beatsPerBar = std::max(0.25, latestBeatsPerBar.load(std::memory_order_relaxed));
    const auto currentBar = static_cast<int64_t>(
        std::floor(latestBlockStartBeat.load(std::memory_order_relaxed) / beatsPerBar));
    nextAutoEvolutionBar.store(
        currentBar + std::max<int32_t>(1, frequencyBars), std::memory_order_relaxed);
    autoEvolutionScheduleReset.store(true, std::memory_order_relaxed);
  }
}

std::vector<LineageAudioProcessor::AutoEvolutionEvent>
LineageAudioProcessor::drainAutoEvolutionEvents() {
  const juce::SpinLock::ScopedLockType eventLock(autoEvolutionEventLock);
  std::vector<AutoEvolutionEvent> events;
  events.swap(pendingAutoEvolutionEvents);
  return events;
}

juce::AudioProcessorEditor* LineageAudioProcessor::createEditor() {
  return new LineageAudioProcessorEditor(*this);
}

bool LineageAudioProcessor::hasEditor() const { return true; }

const juce::String LineageAudioProcessor::getName() const { return "Lineage"; }

bool LineageAudioProcessor::acceptsMidi() const { return true; }
bool LineageAudioProcessor::producesMidi() const { return true; }
bool LineageAudioProcessor::isMidiEffect() const { return true; }
double LineageAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int LineageAudioProcessor::getNumPrograms() { return 1; }
int LineageAudioProcessor::getCurrentProgram() { return 0; }
void LineageAudioProcessor::setCurrentProgram(int) {}
const juce::String LineageAudioProcessor::getProgramName(int) { return {}; }
void LineageAudioProcessor::changeProgramName(int, const juce::String&) {}

void LineageAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void LineageAudioProcessor::setStateInformation(const void*, int) {}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new LineageAudioProcessor();
}
