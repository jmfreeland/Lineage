#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct JSRuntime;
struct JSContext;

/**
 * Thin QuickJS wrapper (DESIGN.md §11) — deliberately not JUCE-dependent so
 * it (and the bridge behavior it exposes) can be exercised by a plain
 * standalone test binary, not just inside the real plugin.
 *
 * Not thread-safe, and QuickJS calls here are synchronous on whichever
 * thread calls processBlock() — currently the audio thread, since this is
 * an MVP. QuickJS allocates and can pause for GC, which is a real risk for
 * a hard real-time audio callback; acceptable for a personal tool with
 * generous buffer sizes for now, but worth revisiting (e.g. moving JS
 * execution off the audio thread with a lock-free handoff) if it ever
 * causes audible dropouts.
 */
class JsEngine {
public:
  JsEngine();
  ~JsEngine();

  JsEngine(const JsEngine&) = delete;
  JsEngine& operator=(const JsEngine&) = delete;

  // Evaluates top-level script source (e.g. the bundled engine runtime).
  // Returns true on success; on failure, errorOut is set and the engine
  // should not be used further.
  bool loadScript(const std::string& source, const std::string& scriptName, std::string& errorOut);

  struct MidiEvent {
    int32_t note = 0;
    int32_t velocity = 0;
    int32_t channel = 1;
    int32_t samplePosition = 0;
    // Absolute host beat position of this event (blockStartBeat + samples-
    // into-block converted via tempo/sampleRate). Input-only — the caller
    // computes it from host transport before calling processBlock(); it is
    // not meaningful on (and isn't populated in) the returned events.
    double beatPosition = 0.0;
    // Populated by renderPlaybackBlock() for notes sourced from the current
    // lineage head. Incoming live MIDI does not need to supply it.
    double durationBeats = 0.25;
    // UI metadata from the finalized planner: bit 0 ghost, bit 1 velocity-
    // humanized, bit 2 evolved head, bit 3 scheduled future evolution.
    int32_t previewFlags = 0;
  };

  // Host transport context for the block being processed, so the engine
  // can position notes against the real bar/tempo grid instead of a
  // made-up per-block scheme. blockStartBeat/sampleRate let the JS side
  // convert a note's beat position back to a sample offset — needed once
  // a mutation can change note count/timing (e.g. ghostNote), since output
  // events are no longer just the input events with velocity tweaked.
  struct Transport {
    double tempo = 120.0;
    double beatsPerBar = 4.0;
    double blockStartBeat = 0.0;
    double sampleRate = 44100.0;
  };

  // Calls globalThis.__lineageProcessBlock(events, transport, params) and
  // replaces `events` with its result — which may be a different length
  // than the input (e.g. ghostNote inserts notes). `params` is a flat set
  // of named numeric mutation parameters (e.g. host-exposed VST3 parameter
  // values) forwarded to the JS side as a plain object. Returns false
  // (leaving `events` untouched) on any JS error.
  bool processBlock(std::vector<MidiEvent>& events,
                     const Transport& transport,
                     const std::vector<std::pair<std::string, double>>& params,
                     std::string& errorOut);

  // Renders the current lineage head into the host block. Returned events
  // are note-ons with absolute beat positions and durations; the JUCE shell
  // owns note-off scheduling across process-block boundaries.
  bool renderPlaybackBlock(std::vector<MidiEvent>& eventsOut,
                           const Transport& transport,
                           int32_t blockSizeSamples,
                           const std::vector<std::pair<std::string, double>>& params,
                           std::string& errorOut);

  struct EvolutionRule {
    std::string id;
    double mutation = 0.0;
    double embellish = 0.0;
    double fill = 0.0;
    double hold = 0.0;
    // Pulls the groove back toward the section's seed rather than away
    // from it — every other weight only ever pushes further from where
    // the section started, so without this a tree can only drift, never
    // return. Defaults to 0 (existing rules are unaffected).
    double settle = 0.0;
    // Rule-specific tuning beyond the weights above (DAW testing
    // feedback: "each rule will have 0-2 params and they'll be different
    // per rule") — a flat named bag, same shape as processBlock()'s params.
    // Which keys matter is up to the JS side (mutationAmount,
    // embellishProbability, ghostVelocity, fillPeakVelocity today); an
    // absent key falls back to the pre-existing hardcoded default there.
    std::vector<std::pair<std::string, double>> params;
  };

  // Plans complete bars ahead using the exact deterministic planner used by
  // renderPlaybackBlock(). Events retain absolute beat positions for the UI.
  // Internally resolves the arrangement (setArrangement()) and simulates
  // every relevant section's own auto-evolution schedule forward without
  // committing anything — the JS side owns all of that per-section
  // scheduling state now, so this call needs nothing beyond the transport
  // window and host params.
  bool renderPlaybackPreview(std::vector<MidiEvent>& eventsOut,
                             double startBeat,
                             double beatsPerBar,
                             int32_t barCount,
                             const std::vector<std::pair<std::string, double>>& params,
                             std::string& errorOut);

  // Queries the persistent session lineage tree living inside the JS
  // runtime (DESIGN.md §11) — how many nodes it has captured so far, and
  // the id of the most recently committed one. Exists mainly so session
  // persistence across processBlock() calls is observable/testable, not
  // (yet) surfaced to the host UI.
  struct SessionInfo {
    int32_t nodeCount = 0;
    std::string headNodeId;
    int32_t rootNoteCount = 0;
    int32_t rootLaneCount = 0;
    int32_t groupedLaneCount = 0;
    std::string sectionId;
    std::string sectionName;
  };
  bool getSessionInfo(SessionInfo& infoOut, std::string& errorOut);

  // A genuinely independent lineage tree (DAW testing feedback: "A/B/etc
  // sections that don't depend on each other"), distinct from BRANCH (which
  // still shares ancestry with the current head's parent). Exactly one
  // section is active/audible at a time; setSeedGroove(), evolveWithRule(),
  // and playback all operate on whichever section is currently active.
  struct SectionInfo {
    std::string id;
    std::string name;
    bool active = false;
  };

  // Creates a new, independent, auto-named section (A, B, C, …) rooted at
  // an empty placeholder groove, and makes it the active section.
  bool createSection(SectionInfo& infoOut, std::string& errorOut);

  // Lists every section that currently exists, in creation order, with
  // `active` marking whichever one setSeedGroove()/evolveWithRule()/
  // playback currently target.
  bool listSections(std::vector<SectionInfo>& sectionsOut, std::string& errorOut);

  // Makes an existing section active. Does not evolve, mutate, or discard
  // any section, including the one being switched away from.
  bool selectSection(const std::string& id, std::string& errorOut);

  // Deletes a section. Fails (via errorOut) rather than deleting the last
  // remaining section. Deleting the active section switches the active
  // section to another remaining one.
  bool deleteSection(const std::string& id, std::string& errorOut);

  // An ordered, looping sequence of (section, bar count) blocks (DAW
  // testing feedback: "3 bars of groove and 1 with a bit more busyness,
  // another three groove, and a fill"). Empty means "no arrangement" —
  // every playback path then simply always renders the active section, the
  // pre-arrangement behavior.
  struct ArrangementBlock {
    std::string sectionId;
    int32_t bars = 1;
  };
  bool setArrangement(const std::vector<ArrangementBlock>& blocks, std::string& errorOut);
  bool getArrangement(std::vector<ArrangementBlock>& blocksOut, std::string& errorOut);

  // Configures the *active* section's own automatic-evolution schedule.
  // currentBar anchors a schedule change to "now" — the JS side has no
  // transport awareness of its own. Mirrors the UI's existing START/PAUSE +
  // frequency controls; each section remembers its own schedule now, so
  // switching sections no longer needs to pause it. Which rule actually
  // fires each time is no longer configured here — see setRulePool().
  bool configureAutoEvolution(bool running, int32_t frequencyBars, int64_t currentBar, std::string& errorOut);

  // The active section's weighted pool of enabled rules (DAW testing
  // feedback: "the library should have a selector tick for each rule that
  // opts it in or out for evolutions for each tree, and then there needs
  // to be a list of enabled rules in the rule controller that allows
  // setting weights for how often they occur"). evolveFromPool() and
  // automatic ticks both roll a weighted choice from this list rather than
  // always applying one fixed rule.
  struct RulePoolEntry {
    EvolutionRule rule;
    double frequency = 1.0;
  };
  bool setRulePool(const std::vector<RulePoolEntry>& entries, std::string& errorOut);
  bool getRulePool(std::vector<RulePoolEntry>& entriesOut, std::string& errorOut);

  // Realigns every currently-running section's schedule to "next due N
  // bars from now" — call this on a transport start, seek, or loop so a
  // schedule computed against a stale bar number doesn't fire early/late or
  // (after a big jump) fire a burst of catch-up generations.
  bool resetAutoEvolutionSchedules(int64_t currentBar, std::string& errorOut);

  struct AutoEvolutionFiredEvent {
    std::string sectionId;
    std::string sectionName;
    std::string ruleId;
    std::string operation;
  };

  // Call once per detected host bar change while the transport is playing.
  // Evolves every section whose own schedule is due — independent of which
  // section is currently audible, which is what makes background sections
  // in an arrangement keep evolving on their own (DAW testing feedback:
  // "each of them evolving independently").
  bool tickAutoEvolution(int64_t currentBar,
                        std::vector<AutoEvolutionFiredEvent>& eventsOut,
                        std::string& errorOut);

  // A complete authored row from the visual seed editor. Rows are retained
  // even when they contain no active steps so lane naming/grouping survives.
  struct SeedLane {
    std::string id;
    std::string name;
    int32_t midiNote = 36;
    std::string group;
    int32_t velocity = 100;
    std::vector<int32_t> activeSteps;
  };

  // Replaces the session's history with a fresh tree rooted at a groove
  // built from these lanes (DESIGN.md §11's visual groove editor) — a hard
  // reset, not a branch. stepsPerBar/beatsPerBar describe the grid (16
  // steps over 4 beats = 16th-note resolution in 4/4).
  bool setSeedGroove(const std::vector<SeedLane>& lanes,
                      int32_t stepsPerBar,
                      int32_t beatsPerBar,
                      std::string& errorOut);

  struct EvolutionResult {
    std::string nodeId;
    std::string parentId;
    std::string operation;
    // Only populated by evolveFromPool() below — evolveWithRule()'s caller
    // already knows the rule id, since they passed it in.
    std::string ruleId;
  };

  // Creates a real lineage child (or sibling branch) using one weighted
  // high-level rule outcome and makes it the playback head.
  bool evolveWithRule(const EvolutionRule& rule,
                      bool branch,
                      EvolutionResult& resultOut,
                      std::string& errorOut);

  // Rolls a weighted choice from the active section's rule pool
  // (setRulePool()) and evolves (or branches) with whichever rule was
  // chosen — what the UI's EVOLVE/BRANCH buttons call now. A bridge call
  // failure (returns false) is a real error; an *empty* pool is not — it
  // leaves resultOut.nodeId empty (a safe no-op) while still returning
  // true, since nothing about the bridge call itself failed.
  bool evolveFromPool(bool branch, EvolutionResult& resultOut, std::string& errorOut);

  // One tree node's worth of a single traced note (DAW testing feedback:
  // "below the seed editor we need a visualizer for whatever cell we've
  // clicked on to see where it evolved to"). `present` false means the
  // note was dropped at or before this node (e.g. settle removing an
  // unmatched embellishment) — position/velocity are meaningless then.
  // parentNodeId is empty for the root; the full array — every node in
  // the tree, not just one branch — forms a flat parent-linked forest of
  // exactly one tree, which the caller reconstructs into root-to-leaf
  // paths. isHeadPath marks the nodes on the currently-audible branch.
  struct NoteEvolutionEntry {
    std::string nodeId;
    std::string parentNodeId;
    int32_t generation = 0;
    std::string operation;
    bool present = false;
    double position = 0.0;
    double velocity = 0.0;
    bool isHeadPath = false;
  };

  // Traces the note at (laneId, positionBeats) in the active section's
  // CURRENT seed (root node) across every node in the tree, not just the
  // head path — re-resolved fresh on every call from the seed's current
  // content, not a captured note identity with its own lifetime. An empty
  // result means no note exists at that position in the current seed;
  // that's a safe no-op, not a bridge error.
  bool getNoteEvolution(const std::string& laneId,
                        double positionBeats,
                        std::vector<NoteEvolutionEntry>& entriesOut,
                        std::string& errorOut);

  // Loads a vocabulary.json produced by tools/midi-analysis (raw JSON
  // text — parsing/validation happens on the JS side via src/vocabulary.ts).
  // Once loaded, evolveWithRule()'s "mutation" outcome samples per-voice/
  // per-position timing and velocity variation from it instead of a flat
  // hardcoded amount. Independent of the session/seed groove — loading a
  // vocabulary only changes how future mutations behave, not tree state.
  bool setVocabulary(const std::string& json, std::string& errorOut);
  bool clearVocabulary(std::string& errorOut);

private:
  JSRuntime* runtime = nullptr;
  JSContext* context = nullptr;
};
