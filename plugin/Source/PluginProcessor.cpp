#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

#include <algorithm>
#include <cmath>

LineageAudioProcessor::LineageAudioProcessor()
    : AudioProcessor(BusesProperties()) {
  addParameter(humanizeAmountParam =
                   new juce::AudioParameterInt({"humanizeAmount", 1}, "Humanize Amount", 1, 40, 12));
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
}

void LineageAudioProcessor::releaseResources() {
  pendingPlaybackNoteOffs.clear();
  wasTransportPlaying = false;
  havePreviousBlockPosition = false;
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
    const std::vector<std::pair<std::string, double>> params = {
        {"humanizeAmount", static_cast<double>(humanizeAmountParam->get())},
        {"ghostNoteEnabled", ghostNoteEnabledParam->get() ? 1.0 : 0.0},
        {"ghostNoteProbability", static_cast<double>(ghostNoteProbabilityParam->get())}};
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
  if (isTransportPlaying && numSamples > 0) {
    std::string error;
    const juce::ScopedLock lock(jsEngineLock);
    if (!jsEngine.renderPlaybackBlock(playbackEvents, transport, numSamples, error)) {
      juce::Logger::writeToLog("Lineage: failed to render playback block: " + juce::String(error));
    }
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

  const bool transportDiscontinuity = isTransportPlaying && havePreviousBlockPosition
      && std::abs(blockStartBeat - previousBlockEndBeat) > 1.0e-4;
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

void LineageAudioProcessor::setSeedGroove(const std::vector<JsEngine::SeedLane>& lanes) {
  if (!jsEngineReady) return;
  const juce::ScopedLock lock(jsEngineLock);
  std::string error;
  if (!jsEngine.setSeedGroove(lanes, 16, 4, error)) {
    juce::Logger::writeToLog("Lineage: failed to set seed groove: " + juce::String(error));
  }
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
