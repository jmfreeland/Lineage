#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "JsEngine.h"

/**
 * MIDI-effect shell (DESIGN.md §11). processBlock() bridges real MIDI
 * note-on events through the embedded engine (see JsEngine.h /
 * src/runtime.ts): notes are positioned against the host's real
 * tempo/time-signature (via getPlayHead()), and the "amount" mutation
 * parameter is a real, host-automatable VST3 parameter rather than a
 * hardcoded constant. There is still no persisted lineage/live-loop
 * session state in the plugin, and only one mutation's one macroEligible
 * param is wired up so far — everything else the engine can do is not yet
 * reachable from inside a DAW.
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

  // Range/default mirror mutations/velocityHumanize.ts's "amount" param
  // manifest (min 1, max 40, default 12) — kept in sync by hand for now;
  // there's no codegen from the manifest into JUCE parameters yet.
  juce::AudioParameterInt* humanizeAmountParam = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessor)
};
