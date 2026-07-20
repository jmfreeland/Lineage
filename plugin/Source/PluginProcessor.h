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
  juce::RangedAudioParameter& getHumanizeTimingEnabledParameter() noexcept { return *humanizeTimingEnabledParam; }
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

  // Rolls a weighted choice from the active section's rule pool
  // (setRulePool()) and evolves with it — what the UI's EVOLVE/BRANCH
  // buttons call now. Returns false only on a real bridge failure; an
  // empty pool is a safe no-op (resultOut.nodeId left empty, still true).
  bool evolveFromPool(bool branch, JsEngine::EvolutionResult& resultOut);

  struct AutoEvolutionEvent {
    juce::String sectionName;
    // Populated from the rule's id (e.g. "pocket-keeper"), not the
    // display-cased preset name — the fired-event bridge contract only
    // carries the id, since it can name a rule from any section, not just
    // whichever one the UI currently has selected. A cosmetic simplification,
    // not a data-loss one.
    juce::String ruleName;
    juce::String operation;
  };
  // Configures the *active* section's own automatic-evolution schedule —
  // every section remembers its own now (DAW testing feedback: "each of
  // them evolving independently"), so switching sections no longer needs
  // to pause it the way loading a new seed still does. Which rule actually
  // fires is no longer configured here — see setRulePool().
  void configureAutoEvolution(bool running, int32_t frequencyBars);
  std::vector<AutoEvolutionEvent> drainAutoEvolutionEvents();

  // The active section's weighted pool of enabled rules (DAW testing
  // feedback: "the library should have a selector tick for each rule that
  // opts it in or out for evolutions for each tree, and then there needs
  // to be a list of enabled rules in the rule controller that allows
  // setting weights for how often they occur"). evolveFromPool() and
  // automatic ticks both roll from this list.
  bool setRulePool(const std::vector<JsEngine::RulePoolEntry>& entries);
  std::vector<JsEngine::RulePoolEntry> getRulePool();

  // Traces the note at (laneId, positionBeats) in the active section's
  // current seed across the head path (DAW testing feedback: "below the
  // seed editor we need a visualizer for whatever cell we've clicked on
  // to see where it evolved to"). Message-thread only, like
  // getPlaybackPreview() above. Returns an empty vector both on a bridge
  // failure (logged) and on the safe no-op case (no note at that
  // position) — the editor doesn't need to tell those apart.
  std::vector<JsEngine::NoteEvolutionEntry> getNoteEvolution(const juce::String& laneId, double positionBeats);

  // Independent named sections ("A/B/etc that don't depend on each
  // other" — DAW testing feedback), distinct from BRANCH (still shares
  // ancestry). Exactly one section is *active* (what setSeedGroove()/
  // evolveWithRule()/the seed editor target) at a time, but playback
  // renders whichever section the arrangement resolves for the current
  // bar (see setArrangement()), and every section keeps evolving on its
  // own schedule regardless of which one is currently audible.
  JsEngine::SectionInfo createSection();
  std::vector<JsEngine::SectionInfo> listSections();
  bool selectSection(const juce::String& id);
  bool deleteSection(const juce::String& id);

  // An ordered, looping sequence of (section, bar count) blocks (DAW
  // testing feedback: "3 bars of groove and 1 with a bit more busyness,
  // another three groove, and a fill"). Empty means "no arrangement" —
  // playback then always renders the active section, the pre-arrangement
  // behavior.
  bool setArrangement(const std::vector<JsEngine::ArrangementBlock>& blocks);
  std::vector<JsEngine::ArrangementBlock> getArrangement();

  // Loads a vocabulary.json mined by tools/midi-analysis (see that tool's
  // README) — once loaded, the "mutation" rule outcome samples per-voice/
  // per-position timing and velocity variation from real performances
  // instead of a flat hardcoded amount. Independent of session/seed state;
  // this only changes how future mutations behave. Returns an empty string
  // on success, otherwise a human-readable error (malformed JSON, or a
  // schema mismatch caught by src/vocabulary.ts) meant to be shown to the
  // user directly, not just logged — this is the one bridge call whose
  // failure reason matters to whoever is picking the file.
  juce::String loadVocabulary(const juce::String& json);
  void clearVocabulary();

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

  // Auto-evolution *scheduling* (rule/running/frequency/next-due-bar) is
  // now entirely TS-owned, per section (see Section.autoEvolution in
  // src/runtime.ts) — every section remembers its own, so it keeps
  // evolving in the background regardless of which one is currently
  // audible. All the audio thread needs to track is "have we entered a new
  // host bar since the last block", to call jsEngine.tickAutoEvolution()
  // at most once per bar rather than once per block. A bar-precise (not
  // sample-precise) transition is a deliberate simplification: the old
  // code split a process block at the exact evolution beat so old/new
  // heads never shared a block; that precision bought sub-millisecond
  // accuracy for a musically bar-scale event, at the cost of the whole
  // scheduling decision needing to live in C++. Losing it means a
  // transition can land up to one block late (a few ms at typical buffer
  // sizes) — below audible/musical significance, especially next to the
  // humanization jitter already applied elsewhere.
  bool haveTickedBar = false;
  int64_t lastTickedBar = 0;
  juce::SpinLock autoEvolutionEventLock;
  std::vector<AutoEvolutionEvent> pendingAutoEvolutionEvents;

  // Ranges/defaults mirror the mutations' own param manifests
  // (mutations/velocityHumanize.ts, mutations/ghostNote.ts) — kept in sync
  // by hand for now; there's no codegen from a manifest into JUCE
  // parameters yet.
  juce::AudioParameterInt* humanizeAmountParam = nullptr;
  // Timing humanization is opt-in (see src/runtime.ts's planPlaybackRange)
  // rather than folded unconditionally into humanizeAmount, so a session
  // that never enables it keeps exactly its prior (velocity-only) behavior.
  juce::AudioParameterBool* humanizeTimingEnabledParam = nullptr;
  juce::AudioParameterBool* ghostNoteEnabledParam = nullptr;
  juce::AudioParameterFloat* ghostNoteProbabilityParam = nullptr;

  std::vector<std::pair<std::string, double>> getPlaybackParams() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LineageAudioProcessor)
};
