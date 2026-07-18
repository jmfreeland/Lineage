#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "JsEngine.h"

/**
 * MIDI-effect shell (DESIGN.md §11). processBlock() bridges real MIDI
 * note-on events through the embedded engine (see JsEngine.h /
 * src/runtime.ts): notes are positioned against the host's real
 * tempo/time-signature (via getPlayHead()), two mutations' macroEligible
 * params are real host-automatable VST3 parameters, and the JS runtime
 * keeps a persistent session lineage tree that grows as you play (module-
 * level state inside src/runtime.ts, not recreated per block). There is
 * still no playback driven from that captured history — it's memory, not
 * yet a loop — and most of what the engine can do (fills, embellishments,
 * other mutation types, the live-loop session, lineage branching) isn't
 * reachable from inside a DAW yet.
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

  // Ranges/defaults mirror the mutations' own param manifests
  // (mutations/velocityHumanize.ts, mutations/ghostNote.ts) — kept in sync
  // by hand for now; there's no codegen from a manifest into JUCE
  // parameters yet.
  juce::AudioParameterInt* humanizeAmountParam = nullptr;
  juce::AudioParameterBool* ghostNoteEnabledParam = nullptr;
  juce::AudioParameterFloat* ghostNoteProbabilityParam = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessor)
};
