#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

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

void LineageAudioProcessor::prepareToPlay(double, int) {}

void LineageAudioProcessor::releaseResources() {}

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
  if (auto* currentPlayHead = getPlayHead()) {
    if (auto position = currentPlayHead->getPosition()) {
      tempo = position->getBpm().orFallback(120.0);
      blockStartBeat = position->getPpqPosition().orFallback(0.0);
      if (auto signature = position->getTimeSignature()) {
        beatsPerBar = static_cast<double>(signature->numerator) * (4.0 / static_cast<double>(signature->denominator));
      }
    }
  }
  const double sampleRate = getSampleRate();

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
    const JsEngine::Transport transport{tempo, beatsPerBar, blockStartBeat, sampleRate};
    const std::vector<std::pair<std::string, double>> params = {
        {"humanizeAmount", static_cast<double>(humanizeAmountParam->get())},
        {"ghostNoteEnabled", ghostNoteEnabledParam->get() ? 1.0 : 0.0},
        {"ghostNoteProbability", static_cast<double>(ghostNoteProbabilityParam->get())}};
    // On failure, processBlock() leaves noteOnEvents untouched, so this
    // degrades to passthrough for note-ons rather than dropping them.
    jsEngine.processBlock(noteOnEvents, transport, params, error);
  }

  // A mutation (ghostNote) can place a note's computed sample position
  // outside this block — its offset may land before this block started or
  // after it ends. There's no cross-block MIDI scheduler yet, so those get
  // dropped here rather than misfired at a clamped, wrong position.
  const int numSamples = buffer.getNumSamples();
  juce::MidiBuffer output;
  for (const auto& event : noteOnEvents) {
    if (event.samplePosition < 0 || event.samplePosition >= numSamples) continue;
    output.addEvent(juce::MidiMessage::noteOn(event.channel, event.note, static_cast<juce::uint8>(event.velocity)),
                     event.samplePosition);
  }
  for (const auto& [message, samplePosition] : passthroughMessages) {
    output.addEvent(message, samplePosition);
  }

  midiMessages.swapWith(output);
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
