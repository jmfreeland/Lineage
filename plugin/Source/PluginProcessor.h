#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "JsEngine.h"

/**
 * MIDI-effect shell (DESIGN.md §11). processBlock() now bridges real MIDI
 * note-on events through the embedded engine (see JsEngine.h /
 * src/runtime.ts) rather than passing everything through untouched — an
 * MVP proof that the C++/JS bridge carries real note data through real
 * engine code (mutation.ts + velocityHumanize, unmodified) and back. There
 * is no host transport/tempo sync yet, no lineage/live-loop session state,
 * and no host-exposed parameters — this is deliberately just the bridge
 * working end to end, not the full engine surfaced.
 */
class LineageAudioProcessor : public juce::AudioProcessor {
public:
  LineageAudioProcessor();
  ~LineageAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  using AudioProcessor::processBlock;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

private:
  JsEngine jsEngine;
  bool jsEngineReady = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessor)
};
