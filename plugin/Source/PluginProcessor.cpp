#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

LineageAudioProcessor::LineageAudioProcessor()
    : AudioProcessor(BusesProperties()) {
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

void LineageAudioProcessor::processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages) {
  if (!jsEngineReady) return; // leave midiMessages as pure passthrough if the runtime failed to load

  std::vector<JsEngine::MidiEvent> noteOnEvents;
  std::vector<std::pair<juce::MidiMessage, int>> passthroughMessages;

  for (const auto metadata : midiMessages) {
    const auto message = metadata.getMessage();
    if (message.isNoteOn()) {
      noteOnEvents.push_back({message.getNoteNumber(), message.getVelocity(), message.getChannel(),
                               metadata.samplePosition});
    } else {
      passthroughMessages.emplace_back(message, metadata.samplePosition);
    }
  }

  if (!noteOnEvents.empty()) {
    std::string error;
    // On failure, processBlock() leaves noteOnEvents untouched, so this
    // degrades to passthrough for note-ons rather than dropping them.
    jsEngine.processBlock(noteOnEvents, error);
  }

  juce::MidiBuffer output;
  for (const auto& event : noteOnEvents) {
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
