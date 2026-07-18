#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "JsEngine.h"

#include <atomic>

/**
 * MIDI-effect shell (DESIGN.md §11). processBlock() bridges real MIDI
 * note-on events through the embedded engine (see JsEngine.h /
 * src/runtime.ts): notes are positioned against the host's real
 * tempo/time-signature (via getPlayHead()), two mutations' macroEligible
 * params are real host-automatable VST3 parameters, and the JS runtime
 * keeps a persistent session lineage tree that grows as you play (module-
 * level state inside src/runtime.ts, not recreated per block). The editor
 * also offers a visual step sequencer (StepSequencerComponent) to author a
 * starting groove, which becomes that session's seed via setSeedGroove().
 * The current lineage head is rendered as host-synchronised MIDI while the
 * transport runs. Most of what the engine can do (fills,
 * embellishments, other mutation types, the live-loop session, lineage
 * branching) isn't reachable from inside a DAW yet.
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

  juce::RangedAudioParameter& getHumanizeAmountParameter() noexcept { return *humanizeAmountParam; }
  juce::RangedAudioParameter& getGhostNoteEnabledParameter() noexcept { return *ghostNoteEnabledParam; }
  juce::RangedAudioParameter& getGhostNoteProbabilityParameter() noexcept { return *ghostNoteProbabilityParam; }

  // Called from the editor (message thread) when the step sequencer
  // changes — replaces the session's seed groove. Guarded by jsEngineLock
  // since jsEngine is otherwise only ever touched from processBlock() on
  // the audio thread; JsEngine itself isn't thread-safe.
  void setSeedGroove(const std::vector<JsEngine::SeedLane>& lanes);

  struct PlaybackPreview {
    double startBeat = 0.0;
    double beatsPerBar = 4.0;
    double playheadBeat = 0.0;
    bool transportPlaying = false;
    std::vector<JsEngine::MidiEvent> events;
  };

  // Message-thread look-ahead for the editor. The JS planner is shared with
  // real playback, so every displayed stochastic choice is reproducible in
  // the audio blocks that later cover the same beat range.
  PlaybackPreview getPlaybackPreview(int32_t barCount = 8);
  bool evolveWithRule(const JsEngine::EvolutionRule& rule,
                      bool branch,
                      JsEngine::EvolutionResult& resultOut);

  struct AutoEvolutionEvent {
    juce::String ruleName;
    juce::String operation;
  };
  void configureAutoEvolution(const JsEngine::EvolutionRule& rule,
                              juce::String ruleName,
                              bool running,
                              int32_t frequencyBars);
  std::vector<AutoEvolutionEvent> drainAutoEvolutionEvents();

private:
  struct PendingNoteOff {
    double beatPosition = 0.0;
    int note = 0;
    int channel = 1;
  };

  JsEngine jsEngine;
  bool jsEngineReady = false;
  juce::CriticalSection jsEngineLock;
  std::vector<PendingNoteOff> pendingPlaybackNoteOffs;
  bool wasTransportPlaying = false;
  bool havePreviousBlockPosition = false;
  double previousBlockEndBeat = 0.0;
  std::atomic<double> latestBlockStartBeat{0.0};
  std::atomic<double> latestBeatsPerBar{4.0};
  std::atomic<bool> latestTransportPlaying{false};

  struct AutoEvolutionConfig {
    JsEngine::EvolutionRule rule;
    juce::String ruleName;
    bool running = false;
    int32_t frequencyBars = 4;
  };
  juce::SpinLock autoEvolutionConfigLock;
  AutoEvolutionConfig autoEvolutionConfig;
  std::atomic<bool> autoEvolutionScheduleReset{true};
  std::atomic<int64_t> nextAutoEvolutionBar{0};
  juce::SpinLock autoEvolutionEventLock;
  std::vector<AutoEvolutionEvent> pendingAutoEvolutionEvents;

  // Ranges/defaults mirror the mutations' own param manifests
  // (mutations/velocityHumanize.ts, mutations/ghostNote.ts) — kept in sync
  // by hand for now; there's no codegen from a manifest into JUCE
  // parameters yet.
  juce::AudioParameterInt* humanizeAmountParam = nullptr;
  juce::AudioParameterBool* ghostNoteEnabledParam = nullptr;
  juce::AudioParameterFloat* ghostNoteProbabilityParam = nullptr;

  std::vector<std::pair<std::string, double>> getPlaybackParams() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessor)
};
