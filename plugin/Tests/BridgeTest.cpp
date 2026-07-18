// Standalone smoke test for the C++/JS bridge (DESIGN.md §11) — verifies
// the embedded engine bundle loads, that real mutations from the TS engine
// (velocityHumanize, ghostNote) actually execute inside QuickJS, that host
// parameters reach them, and that the persistent session lineage tree
// accumulates across calls — without needing a DAW to load a VST3 into.
#include "../Source/JsEngine.h"
#include "BinaryData.h"

#include <cmath>
#include <cstdio>
#include <utility>
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

  // beatPosition/samplePosition are mutually consistent (120bpm, 44.1kHz,
  // blockStartBeat 0) so the "position preserved" check below is exact.
  std::vector<JsEngine::MidiEvent> events = {
      {36, 100, 1, 0, 0.0},
      {38, 100, 1, 11025, 0.5},
      {42, 100, 1, 22050, 1.0},
  };
  const auto original = events;

  const JsEngine::Transport transport{120.0, 4.0, 0.0, 44100.0};
  const std::vector<std::pair<std::string, double>> params = {{"humanizeAmount", 20.0}};

  bool ok = engine.processBlock(events, transport, params, error);
  expect(ok, "processBlock() succeeds");
  if (!ok) {
    std::printf("  error: %s\n", error.c_str());
    return 1;
  }

  expect(events.size() == original.size(), "output event count matches input when ghost notes are disabled");

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
  expect(allFieldsPreservedExceptVelocity, "note/channel/samplePosition round-trip exactly through the beat conversion");
  expect(allVelocitiesInMidiRange, "mutated velocities stay within MIDI range [1, 127]");

  // Empty block should be a safe no-op.
  std::vector<JsEngine::MidiEvent> empty;
  ok = engine.processBlock(empty, transport, params, error);
  expect(ok && empty.empty(), "an empty block is a safe no-op");

  // A near-zero "amount" param should barely move velocities — proves the
  // params argument actually reaches the mutation, not just events/transport.
  std::vector<JsEngine::MidiEvent> gentle = original;
  const std::vector<std::pair<std::string, double>> gentleParams = {{"humanizeAmount", 1.0}};
  ok = engine.processBlock(gentle, transport, gentleParams, error);
  bool allWithinOne = true;
  for (size_t i = 0; i < gentle.size() && i < original.size(); ++i) {
    if (std::abs(gentle[i].velocity - original[i].velocity) > 1) allWithinOne = false;
  }
  expect(ok && allWithinOne, "a small host 'amount' param produces small velocity changes");

  // --- Ghost notes: variable-length output -------------------------------
  std::vector<JsEngine::MidiEvent> withGhosts = original;
  const std::vector<std::pair<std::string, double>> ghostParams = {
      {"humanizeAmount", 1.0}, {"ghostNoteEnabled", 1.0}, {"ghostNoteProbability", 1.0}};
  ok = engine.processBlock(withGhosts, transport, ghostParams, error);
  expect(ok && withGhosts.size() > original.size(),
         "enabling ghost notes actually grows the event count (proves variable-length output round-trips)");
  bool allGhostVelocitiesInRange = true;
  for (const auto& event : withGhosts) {
    if (event.velocity < 1 || event.velocity > 127) allGhostVelocitiesInRange = false;
  }
  expect(allGhostVelocitiesInRange, "ghost-note-inflated output still stays within MIDI velocity range");

  // --- Session persistence -------------------------------------------------
  JsEngine::SessionInfo infoBefore;
  ok = engine.getSessionInfo(infoBefore, error);
  expect(ok, "getSessionInfo() succeeds");

  // bar 0, then bar 1 — crossing the bar boundary should commit bar 0 to
  // the session lineage tree (module-level JS state, persisting across
  // these separate processBlock() calls on the same JsEngine instance).
  std::vector<JsEngine::MidiEvent> bar0 = {{36, 100, 1, 0, 0.0}, {38, 100, 1, 11025, 0.5}};
  std::vector<JsEngine::MidiEvent> bar1 = {{36, 100, 1, 0, 4.0}};
  engine.processBlock(bar0, transport, params, error);
  engine.processBlock(bar1, transport, params, error);

  JsEngine::SessionInfo infoAfter;
  ok = engine.getSessionInfo(infoAfter, error);
  expect(ok && infoAfter.nodeCount > infoBefore.nodeCount,
         "playing across a bar boundary grows the persistent session lineage tree");
  expect(!infoAfter.headNodeId.empty(), "session has a valid head node id");

  // --- Visual step-sequencer seed groove ----------------------------------
  const std::vector<JsEngine::SeedNote> seedNotes = {
      {"kick", 0, 110}, {"kick", 8, 100}, {"snare", 4, 90}, {"snare", 12, 90}, {"hihat", 0, 70}, {"hihat", 2, 70},
  };
  ok = engine.setSeedGroove(seedNotes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds");
  if (!ok) std::printf("  error: %s\n", error.c_str());

  JsEngine::SessionInfo infoAfterSeed;
  ok = engine.getSessionInfo(infoAfterSeed, error);
  expect(ok && infoAfterSeed.rootNoteCount == static_cast<int32_t>(seedNotes.size()),
         "seeding a groove makes it the session's new root, with all authored notes present");
  expect(infoAfterSeed.headNodeId != infoAfter.headNodeId,
         "seeding resets the session to a fresh tree (new head), not a branch off the old one");

  std::printf("\n%s\n", failures == 0 ? "All bridge tests passed." : "Bridge tests FAILED.");
  return failures == 0 ? 0 : 1;
}
