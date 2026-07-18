// Standalone smoke test for the C++/JS bridge (DESIGN.md §11) — verifies
// the embedded engine bundle loads and that a real mutation from the TS
// engine (velocityHumanize) actually executes inside QuickJS, without
// needing a DAW to load a VST3 into.
#include "../Source/JsEngine.h"
#include "BinaryData.h"

#include <cstdio>
#include <vector>

namespace {
int failures = 0;

void expect(bool condition, const char* description) {
  if (!condition) {
    failures += 1;
    std::printf("FAIL: %s\n", description);
  } else {
    std::printf("ok:   %s\n", description);
  }
}
} // namespace

int main() {
  JsEngine engine;
  std::string error;

  std::string bundleSource(BinaryData::runtime_bundle_js, static_cast<size_t>(BinaryData::runtime_bundle_jsSize));
  bool loaded = engine.loadScript(bundleSource, "runtime.bundle.js", error);
  expect(loaded, "runtime bundle loads without a JS error");
  if (!loaded) {
    std::printf("  error: %s\n", error.c_str());
    return 1;
  }

  std::vector<JsEngine::MidiEvent> events = {
      {36, 100, 1, 0},
      {38, 100, 1, 512},
      {42, 100, 1, 1024},
  };
  const auto original = events;

  bool ok = engine.processBlock(events, error);
  expect(ok, "processBlock() succeeds");
  if (!ok) {
    std::printf("  error: %s\n", error.c_str());
    return 1;
  }

  expect(events.size() == original.size(), "output event count matches input");

  bool anyVelocityChanged = false;
  bool allFieldsPreservedExceptVelocity = true;
  bool allVelocitiesInMidiRange = true;
  for (size_t i = 0; i < events.size() && i < original.size(); ++i) {
    if (events[i].velocity != original[i].velocity) anyVelocityChanged = true;
    if (events[i].note != original[i].note || events[i].channel != original[i].channel ||
        events[i].samplePosition != original[i].samplePosition) {
      allFieldsPreservedExceptVelocity = false;
    }
    if (events[i].velocity < 1 || events[i].velocity > 127) allVelocitiesInMidiRange = false;
  }

  expect(anyVelocityChanged, "at least one velocity was actually mutated (not a no-op passthrough)");
  expect(allFieldsPreservedExceptVelocity, "note/channel/samplePosition are preserved exactly");
  expect(allVelocitiesInMidiRange, "mutated velocities stay within MIDI range [1, 127]");

  // Empty block should be a safe no-op.
  std::vector<JsEngine::MidiEvent> empty;
  ok = engine.processBlock(empty, error);
  expect(ok && empty.empty(), "an empty block is a safe no-op");

  std::printf("\n%s\n", failures == 0 ? "All bridge tests passed." : "Bridge tests FAILED.");
  return failures == 0 ? 0 : 1;
}
