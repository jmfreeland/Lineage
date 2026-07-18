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
  };

  // Calls globalThis.__lineageProcessBlock(events) and replaces `events`
  // with its result in place. Returns false (leaving `events` untouched)
  // on any JS error.
  bool processBlock(std::vector<MidiEvent>& events, std::string& errorOut);

private:
  JSRuntime* runtime = nullptr;
  JSContext* context = nullptr;
};
