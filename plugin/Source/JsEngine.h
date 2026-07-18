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
  };

  struct AutoEvolutionPreview {
    bool running = false;
    EvolutionRule rule;
    int64_t nextEvolutionBar = 0;
    int32_t frequencyBars = 4;
  };

  // Plans complete bars ahead using the exact deterministic planner used by
  // renderPlaybackBlock(). Events retain absolute beat positions for the UI.
  bool renderPlaybackPreview(std::vector<MidiEvent>& eventsOut,
                             double startBeat,
                             double beatsPerBar,
                             int32_t barCount,
                             const std::vector<std::pair<std::string, double>>& params,
                             std::string& errorOut,
                             const AutoEvolutionPreview* autoEvolution = nullptr);

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
  };
  bool getSessionInfo(SessionInfo& infoOut, std::string& errorOut);

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
  };

  // Creates a real lineage child (or sibling branch) using one weighted
  // high-level rule outcome and makes it the playback head.
  bool evolveWithRule(const EvolutionRule& rule,
                      bool branch,
                      EvolutionResult& resultOut,
                      std::string& errorOut);

private:
  JSRuntime* runtime = nullptr;
  JSContext* context = nullptr;
};
