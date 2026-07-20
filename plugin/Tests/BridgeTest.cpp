// Standalone smoke test for the C++/JS bridge (DESIGN.md §11) — verifies
// the embedded engine bundle loads, that real mutations from the TS engine
// (velocityHumanize, ghostNote) actually execute inside QuickJS, that host
// parameters reach them, and that the persistent session lineage tree
// accumulates across calls — without needing a DAW to load a VST3 into.
#include "../Source/JsEngine.h"
#include "BinaryData.h"

#include <algorithm>
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

  // --- Dynamic visual seed-editor groove ----------------------------------
  const std::vector<JsEngine::SeedLane> seedLanes = {
      {"kick", "Kick", 36, "", 110, {0, 8}},
      {"snare", "Snare", 38, "", 90, {4, 12}},
      {"closed-hat", "Closed Hat", 42, "Hats", 70, {0, 2}},
      {"open-hat", "Open Hat", 46, "Hats", 76, {}},
  };
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds");
  if (!ok) std::printf("  error: %s\n", error.c_str());

  JsEngine::SessionInfo infoAfterSeed;
  ok = engine.getSessionInfo(infoAfterSeed, error);
  expect(ok && infoAfterSeed.rootNoteCount == 6,
         "seeding a groove makes it the session's new root, with all authored notes present");
  expect(infoAfterSeed.rootLaneCount == static_cast<int32_t>(seedLanes.size()),
         "seed lanes are retained even when a lane has no active notes");
  expect(infoAfterSeed.groupedLaneCount == 2,
         "linked seed lanes preserve their shared semantic group");
  expect(infoAfterSeed.headNodeId != infoAfter.headNodeId,
         "seeding resets the session to a fresh tree (new head), not a branch off the old one");

  // --- Host-synchronised playback of the current lineage head ------------
  std::vector<JsEngine::MidiEvent> playback;
  const JsEngine::Transport firstHalfBeat{120.0, 4.0, 0.0, 44100.0};
  ok = engine.renderPlaybackBlock(playback, firstHalfBeat, 11025, params, error);
  expect(ok, "renderPlaybackBlock() succeeds");
  const auto firstKick = std::find_if(playback.begin(), playback.end(), [](const auto& event) {
    return event.note == 36 && event.samplePosition == 0;
  });
  const auto firstHiHat = std::find_if(playback.begin(), playback.end(), [](const auto& event) {
    return event.note == 42 && event.samplePosition == 0;
  });
  expect(playback.size() == 2 && firstKick != playback.end() && firstHiHat != playback.end(),
         "the first half-beat block renders simultaneous seed notes at their exact host sample");
  expect(std::all_of(playback.begin(), playback.end(), [](const auto& event) {
           return std::abs(event.durationBeats - 0.25) < 1.0e-9;
         }),
         "rendered seed notes preserve their authored gate length");

  const JsEngine::Transport secondHalfBeat{120.0, 4.0, 0.5, 44100.0};
  ok = engine.renderPlaybackBlock(playback, secondHalfBeat, 11025, params, error);
  expect(ok && playback.size() == 1 && playback[0].note == 42 && playback[0].samplePosition == 0,
         "an event on a process-block boundary is rendered by the block that starts there");

  const JsEngine::Transport nextBar{120.0, 4.0, 4.0, 44100.0};
  ok = engine.renderPlaybackBlock(playback, nextBar, 11025, params, error);
  const auto loopedKick = std::find_if(playback.begin(), playback.end(), [](const auto& event) {
    return event.note == 36 && event.samplePosition == 0;
  });
  expect(ok && playback.size() == 2 && loopedKick != playback.end(),
         "the current lineage head loops on the next host bar");

  // --- Finalized eight-bar look-ahead ------------------------------------
  std::vector<JsEngine::MidiEvent> preview;
  ok = engine.renderPlaybackPreview(preview, 0.0, 4.0, 8, params, error);
  expect(ok && preview.size() == 48,
         "the preview plans all authored notes across the current and next four bars");
  expect(std::all_of(preview.begin(), preview.end(), [](const auto& event) {
           return event.beatPosition >= 0.0 && event.beatPosition < 32.0;
         }),
         "preview events retain absolute beat positions inside the requested horizon");
  const auto plannedLoopedKick = std::find_if(preview.begin(), preview.end(), [](const auto& event) {
    return event.note == 36 && std::abs(event.beatPosition - 4.0) < 1.0e-9;
  });
  expect(loopedKick != playback.end() && plannedLoopedKick != preview.end()
             && loopedKick->velocity == plannedLoopedKick->velocity
             && loopedKick->previewFlags == plannedLoopedKick->previewFlags,
         "preview and audio-block playback share the same finalized stochastic event");

  std::vector<JsEngine::MidiEvent> ghostPreview;
  ok = engine.renderPlaybackPreview(ghostPreview, 0.0, 4.0, 8, ghostParams, error);
  const bool containsMarkedGhost = std::any_of(ghostPreview.begin(), ghostPreview.end(), [](const auto& event) {
    return (event.previewFlags & 1) != 0;
  });
  expect(ok && ghostPreview.size() > preview.size() && containsMarkedGhost,
         "the same preview planner includes and identifies deterministic ghost-note randomization");

  const JsEngine::EvolutionRule fillRule{"fill-only", 0.0, 0.0, 1.0, 0.0};
  ok = engine.setRulePool({{fillRule, 1.0}}, error);
  expect(ok, "setRulePool() succeeds ahead of auto-evolution checks");
  ok = engine.configureAutoEvolution(true, 1, 0, error);
  expect(ok, "configureAutoEvolution() succeeds");
  std::vector<JsEngine::MidiEvent> scheduledPreview;
  ok = engine.renderPlaybackPreview(scheduledPreview, 0.0, 4.0, 8, params, error);
  const bool futureBarsAreEvolved = std::any_of(
      scheduledPreview.begin(), scheduledPreview.end(), [](const auto& event) {
        return event.beatPosition >= 4.0 && (event.previewFlags & 4) != 0
            && (event.previewFlags & 8) != 0;
      });
  JsEngine::SessionInfo infoAfterScheduledPreview;
  engine.getSessionInfo(infoAfterScheduledPreview, error);
  expect(ok && scheduledPreview.size() > preview.size() && futureBarsAreEvolved,
         "look-ahead simulates scheduled rule generations inside the upcoming eight bars");
  expect(infoAfterScheduledPreview.nodeCount == infoAfterSeed.nodeCount,
         "planning future automatic evolution does not commit tree nodes early");

  // Auto-evolution configured but not yet ticked must not have touched the
  // real head/tree either — only tickAutoEvolution() commits anything.
  ok = engine.configureAutoEvolution(false, 4, 0, error);
  expect(ok, "configureAutoEvolution() can pause a running schedule");

  // --- Weighted rule-driven lineage growth -------------------------------
  JsEngine::EvolutionResult evolution;
  ok = engine.evolveWithRule(fillRule, false, evolution, error);
  expect(ok && evolution.operation == "fill" && !evolution.nodeId.empty(),
         "a weighted rule creates a real lineage child with its selected operation");
  JsEngine::SessionInfo infoAfterEvolution;
  engine.getSessionInfo(infoAfterEvolution, error);
  expect(infoAfterEvolution.nodeCount == infoAfterSeed.nodeCount + 1,
         "rule evolution grows the persistent runtime tree");

  std::vector<JsEngine::MidiEvent> evolvedPreview;
  ok = engine.renderPlaybackPreview(evolvedPreview, 0.0, 4.0, 8, params, error);
  const bool containsEvolvedEvent = std::any_of(evolvedPreview.begin(), evolvedPreview.end(), [](const auto& event) {
    return (event.previewFlags & 4) != 0;
  });
  expect(ok && evolvedPreview.size() > preview.size() && containsEvolvedEvent,
         "the evolved tree head changes both finalized playback and the upcoming-bars preview");

  JsEngine::EvolutionResult branchEvolution;
  ok = engine.evolveWithRule(fillRule, true, branchEvolution, error);
  JsEngine::SessionInfo infoAfterBranch;
  engine.getSessionInfo(infoAfterBranch, error);
  expect(ok && infoAfterBranch.nodeCount == infoAfterEvolution.nodeCount + 1
             && branchEvolution.parentId != evolution.nodeId,
         "branching creates a sibling variation from the current head's parent");

  // --- Rule-specific tunable params, 0-2 per rule beyond the four weights
  // (DAW testing feedback: "each rule will have 0-2 params and they'll be
  // different per rule") -----------------------------------------------
  const std::vector<std::pair<std::string, double>> noHumanizeParams = {{"humanizeAmount", 0.0}};

  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds ahead of rule-param checks");

  JsEngine::EvolutionRule fillCustomPeak{"fill-custom-peak", 0.0, 0.0, 1.0, 0.0};
  fillCustomPeak.params = {{"fillPeakVelocity", 50.0}};
  JsEngine::EvolutionResult fillCustomResult;
  ok = engine.evolveWithRule(fillCustomPeak, false, fillCustomResult, error);
  expect(ok && fillCustomResult.operation == "fill",
         "a rule with only a custom fillPeakVelocity param still evolves via the fill operation");

  std::vector<JsEngine::MidiEvent> fillCustomPreview;
  engine.renderPlaybackPreview(fillCustomPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const bool hasScaledFillPeak = std::any_of(fillCustomPreview.begin(), fillCustomPreview.end(), [](const auto& event) {
    return event.velocity == 50 && std::abs(event.beatPosition - 3.75) < 1.0e-6;
  });
  expect(hasScaledFillPeak, "a rule's custom fillPeakVelocity param scales the generated fill's peak note exactly");

  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  JsEngine::EvolutionRule embellishCustom{"embellish-custom", 0.0, 1.0, 0.0, 0.0};
  embellishCustom.params = {{"embellishProbability", 1.0}, {"ghostVelocity", 77.0}};
  JsEngine::EvolutionResult embellishCustomResult;
  ok = engine.evolveWithRule(embellishCustom, false, embellishCustomResult, error);
  expect(ok && embellishCustomResult.operation == "embellish",
         "a rule with custom embellishProbability/ghostVelocity params evolves via the embellish operation");

  std::vector<JsEngine::MidiEvent> embellishCustomPreview;
  engine.renderPlaybackPreview(embellishCustomPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const bool hasCustomGhostVelocity = std::any_of(
      embellishCustomPreview.begin(), embellishCustomPreview.end(),
      [](const auto& event) { return event.velocity == 77; });
  expect(hasCustomGhostVelocity, "a rule's custom ghostVelocity param sets the exact velocity of generated ghost notes");

  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  JsEngine::EvolutionRule mutationSmallAmount{"mutation-small", 1.0, 0.0, 0.0, 0.0};
  mutationSmallAmount.params = {{"mutationAmount", 3.0}};
  JsEngine::EvolutionResult smallAmountResult;
  ok = engine.evolveWithRule(mutationSmallAmount, false, smallAmountResult, error);
  std::vector<JsEngine::MidiEvent> smallAmountPreview;
  engine.renderPlaybackPreview(smallAmountPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const auto originalVelocity = [](const auto& event) {
    if (event.note == 36) return 110;
    if (event.note == 38) return 90;
    if (event.note == 42) return 70;
    return event.velocity;
  };
  const bool allWithinSmallAmount = std::all_of(
      smallAmountPreview.begin(), smallAmountPreview.end(),
      [&](const auto& event) { return std::abs(event.velocity - originalVelocity(event)) <= 3; });
  expect(ok && allWithinSmallAmount,
         "a rule's small custom mutationAmount param keeps velocity deltas within that bound");

  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  JsEngine::EvolutionRule mutationLargeAmount{"mutation-large", 1.0, 0.0, 0.0, 0.0};
  mutationLargeAmount.params = {{"mutationAmount", 40.0}};
  JsEngine::EvolutionResult largeAmountResult;
  ok = engine.evolveWithRule(mutationLargeAmount, false, largeAmountResult, error);
  std::vector<JsEngine::MidiEvent> largeAmountPreview;
  engine.renderPlaybackPreview(largeAmountPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const bool anyExceedsSmallBound = std::any_of(
      largeAmountPreview.begin(), largeAmountPreview.end(),
      [&](const auto& event) { return std::abs(event.velocity - originalVelocity(event)) > 3; });
  expect(ok && anyExceedsSmallBound,
         "a rule's larger custom mutationAmount param produces a bigger velocity swing than a small one");

  // --- Settle: pulls the groove back toward the section's seed rather
  // than away from it ------------------------------------------------------
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds ahead of settle checks");

  JsEngine::EvolutionRule settleOnlyRule{"settle-only", 0.0, 0.0, 0.0, 0.0, 1.0};
  settleOnlyRule.params = {{"settleStrength", 1.0}};
  JsEngine::EvolutionResult settleResult;
  ok = engine.evolveWithRule(settleOnlyRule, false, settleResult, error);
  expect(ok && settleResult.operation == "settle",
         "a rule with only a settle weight evolves via the settle operation");

  std::vector<JsEngine::MidiEvent> settledFreshPreview;
  engine.renderPlaybackPreview(settledFreshPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const bool freshVelocitiesUnchanged = std::all_of(
      settledFreshPreview.begin(), settledFreshPreview.end(),
      [&](const auto& event) { return event.velocity == originalVelocity(event); });
  expect(freshVelocitiesUnchanged, "settling a groove that already matches its seed changes nothing");

  // Diverge from the seed with a heavy mutation, then settle at full
  // strength and confirm the divergence is undone.
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  JsEngine::EvolutionRule diverge{"diverge", 1.0, 0.0, 0.0, 0.0, 0.0};
  diverge.params = {{"mutationAmount", 40.0}};
  JsEngine::EvolutionResult divergeResult;
  ok = engine.evolveWithRule(diverge, false, divergeResult, error);
  std::vector<JsEngine::MidiEvent> divergedPreview;
  engine.renderPlaybackPreview(divergedPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const bool divergedFromSeed = std::any_of(
      divergedPreview.begin(), divergedPreview.end(),
      [&](const auto& event) { return event.velocity != originalVelocity(event); });
  expect(ok && divergedFromSeed,
         "a heavy mutation actually moves velocities away from the seed (settle test setup sanity check)");

  ok = engine.evolveWithRule(settleOnlyRule, false, settleResult, error);
  expect(ok && settleResult.operation == "settle", "settling after a divergence evolves via the settle operation");
  std::vector<JsEngine::MidiEvent> settledPreview;
  engine.renderPlaybackPreview(settledPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const bool settledBackToSeed = std::all_of(
      settledPreview.begin(), settledPreview.end(),
      [&](const auto& event) { return event.velocity == originalVelocity(event); });
  expect(settledBackToSeed, "settling at full strength pulls a diverged groove's velocities exactly back to the seed");

  // A ghost note added by embellish has no seed counterpart, so a full-
  // strength settle should remove it rather than just soften it.
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  JsEngine::EvolutionRule embellishHeavy{"embellish-heavy", 0.0, 1.0, 0.0, 0.0, 0.0};
  embellishHeavy.params = {{"embellishProbability", 1.0}, {"ghostVelocity", 30.0}};
  JsEngine::EvolutionResult embellishResult;
  ok = engine.evolveWithRule(embellishHeavy, false, embellishResult, error);
  expect(ok && embellishResult.operation == "embellish", "the embellish-heavy setup rule evolves via embellish");
  std::vector<JsEngine::MidiEvent> embellishedPreview;
  engine.renderPlaybackPreview(embellishedPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  const size_t countWithGhosts = embellishedPreview.size();

  JsEngine::EvolutionResult settleAfterEmbellish;
  ok = engine.evolveWithRule(settleOnlyRule, false, settleAfterEmbellish, error);
  std::vector<JsEngine::MidiEvent> settledAfterEmbellishPreview;
  engine.renderPlaybackPreview(settledAfterEmbellishPreview, 0.0, 4.0, 1, noHumanizeParams, error);
  expect(ok && settledAfterEmbellishPreview.size() < countWithGhosts,
         "settling at full strength removes embellishment notes that have no seed counterpart");

  // --- Mined vocabulary (tools/midi-analysis) informs rule-driven mutation ---
  // Fresh seed so the snare backbeat is at a known, exact position again.
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  const JsEngine::EvolutionRule mutationOnlyRule{"mutation-only", 1.0, 0.0, 0.0, 0.0};

  JsEngine::EvolutionResult noVocabEvolution;
  ok = engine.evolveWithRule(mutationOnlyRule, false, noVocabEvolution, error);
  std::vector<JsEngine::MidiEvent> noVocabPreview;
  engine.renderPlaybackPreview(noVocabPreview, 0.0, 4.0, 1, params, error);
  const auto noVocabSnare = std::find_if(noVocabPreview.begin(), noVocabPreview.end(),
                                         [](const auto& event) { return event.note == 38; });
  expect(ok && noVocabSnare != noVocabPreview.end() && std::abs(noVocabSnare->beatPosition - 1.0) < 1.0e-9,
         "without a vocabulary, plain rule-driven mutation never moves a note's timing");

  const std::string vocabularyJson = R"({
    "schema_version": 1,
    "source_files": [],
    "base_patterns": [],
    "variations": [
      {"category": "timing_shift", "voice": "snare", "metric_position": 0.25,
       "frequency": 1.0, "occurrences": 10, "avg_magnitude": 40.0, "direction": "late"}
    ],
    "fills": []
  })";
  ok = engine.setVocabulary(vocabularyJson, error);
  expect(ok, "setVocabulary() succeeds");
  if (!ok) std::printf("  error: %s\n", error.c_str());

  JsEngine::EvolutionResult vocabEvolution;
  ok = engine.evolveWithRule(mutationOnlyRule, false, vocabEvolution, error);
  std::vector<JsEngine::MidiEvent> vocabPreview;
  engine.renderPlaybackPreview(vocabPreview, 0.0, 4.0, 1, params, error);
  const auto vocabSnare = std::find_if(vocabPreview.begin(), vocabPreview.end(),
                                       [](const auto& event) { return event.note == 38; });
  expect(ok && vocabSnare != vocabPreview.end() && vocabSnare->beatPosition > 1.02,
         "a loaded vocabulary measurably shifts a matched note's timing during rule-driven mutation");

  ok = engine.clearVocabulary(error);
  expect(ok, "clearVocabulary() succeeds");

  // --- Timing humanization (opt-in — DAW testing feedback: "humanization
  // should probably have timing") -----------------------------------------
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds ahead of timing-humanization checks");

  const std::vector<std::pair<std::string, double>> timingOffParams = {{"humanizeAmount", 40.0}};
  std::vector<JsEngine::MidiEvent> timingOffPreview;
  engine.renderPlaybackPreview(timingOffPreview, 0.0, 4.0, 1, timingOffParams, error);
  const bool allExactWithTimingOff = std::all_of(
      timingOffPreview.begin(), timingOffPreview.end(), [](const auto& event) {
        return std::abs(event.beatPosition - std::round(event.beatPosition * 4.0) / 4.0) < 1.0e-9;
      });
  expect(allExactWithTimingOff,
         "with humanizeTimingEnabled unset, a high humanizeAmount still never moves a note's timing");

  const std::vector<std::pair<std::string, double>> timingOnParams = {
      {"humanizeAmount", 40.0}, {"humanizeTimingEnabled", 1.0}};
  std::vector<JsEngine::MidiEvent> timingOnPreview;
  engine.renderPlaybackPreview(timingOnPreview, 0.0, 4.0, 1, timingOnParams, error);
  const bool anyNoteMoved = std::any_of(timingOnPreview.begin(), timingOnPreview.end(), [](const auto& event) {
    return std::abs(event.beatPosition - std::round(event.beatPosition * 4.0) / 4.0) > 1.0e-6;
  });
  expect(anyNoteMoved, "enabling humanizeTimingEnabled measurably moves note timing at a high amount");

  const bool anyHumanizedFlagSet = std::any_of(timingOnPreview.begin(), timingOnPreview.end(), [](const auto& event) {
    return (event.previewFlags & 2) != 0;
  });
  expect(anyHumanizedFlagSet, "timing-shifted notes are marked with the humanized preview flag");

  // --- Independent named sections (DAW testing feedback: "A/B/etc sections
  // that don't depend on each other") -------------------------------------
  std::vector<JsEngine::SectionInfo> sections;
  ok = engine.listSections(sections, error);
  expect(ok && sections.size() == 1 && sections[0].active,
         "a fresh engine starts with exactly one active default section");
  const std::string sectionAId = sections[0].id;

  JsEngine::SessionInfo infoSectionABeforeSwitch;
  engine.getSessionInfo(infoSectionABeforeSwitch, error);
  expect(infoSectionABeforeSwitch.sectionId == sectionAId,
         "getSessionInfo() reports the active section's id");

  JsEngine::SectionInfo created;
  ok = engine.createSection(created, error);
  expect(ok && !created.id.empty() && created.id != sectionAId && created.active,
         "createSection() creates a new, distinct, immediately-active section");

  ok = engine.listSections(sections, error);
  expect(ok && sections.size() == 2, "listSections() reflects the newly created section");
  const bool onlySecondIsActive =
      sections.size() == 2 && !sections[0].active && sections[1].active;
  expect(onlySecondIsActive, "creating a section makes it the sole active one");

  JsEngine::SessionInfo infoNewSection;
  engine.getSessionInfo(infoNewSection, error);
  expect(infoNewSection.sectionId == created.id && infoNewSection.nodeCount == 1,
         "the new section starts with its own fresh single-node tree, not section A's history");

  // Seed and evolve the new section — section A must be completely unaffected.
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() on the new section succeeds");
  JsEngine::EvolutionResult evolutionOnNewSection;
  ok = engine.evolveWithRule(fillRule, false, evolutionOnNewSection, error);
  expect(ok, "evolveWithRule() on the new section succeeds");

  JsEngine::SessionInfo infoNewSectionAfterWork;
  engine.getSessionInfo(infoNewSectionAfterWork, error);

  ok = engine.selectSection(sectionAId, error);
  expect(ok, "selectSection() switches back to section A");
  JsEngine::SessionInfo infoSectionAAfterSwitchBack;
  engine.getSessionInfo(infoSectionAAfterSwitchBack, error);
  expect(infoSectionAAfterSwitchBack.sectionId == sectionAId
             && infoSectionAAfterSwitchBack.headNodeId == infoSectionABeforeSwitch.headNodeId
             && infoSectionAAfterSwitchBack.nodeCount == infoSectionABeforeSwitch.nodeCount,
         "section A's tree is untouched by seeding/evolving the other section");
  expect(infoSectionAAfterSwitchBack.headNodeId != infoNewSectionAfterWork.headNodeId,
         "the two sections have genuinely independent, non-matching heads");

  ok = engine.selectSection("no-such-section", error);
  expect(!ok, "selectSection() with an unknown id fails rather than silently no-op'ing");

  ok = engine.selectSection(created.id, error);
  expect(ok, "selectSection() switches to the second section again");
  ok = engine.deleteSection(sectionAId, error);
  expect(ok, "deleteSection() removes a non-active section");
  ok = engine.listSections(sections, error);
  expect(ok && sections.size() == 1 && sections[0].id == created.id,
         "the deleted section is gone and the remaining section is unaffected");

  ok = engine.deleteSection(created.id, error);
  expect(!ok, "deleteSection() refuses to remove the last remaining section");

  JsEngine::SectionInfo anotherSection;
  engine.createSection(anotherSection, error);
  ok = engine.deleteSection(anotherSection.id, error);
  ok = engine.listSections(sections, error);
  expect(ok && sections.size() == 1 && sections[0].active,
         "deleting the active section falls back to another remaining section as active");

  // --- Arrangement: mixing sections into a timeline, each still evolving
  // independently in the background (DAW testing feedback: "3 bars of
  // groove and 1 with a bit more busyness, another three groove, and a
  // fill... each of them evolving independently") -------------------------
  const std::string arrangerSectionAId = sections[0].id;
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds for section A ahead of arrangement checks");

  JsEngine::SectionInfo sectionBInfo;
  ok = engine.createSection(sectionBInfo, error);
  expect(ok, "createSection() succeeds for section B ahead of arrangement checks");
  const std::vector<JsEngine::SeedLane> tomOnlyLanes = {{"tom", "Tom", 45, "", 100, {0, 4, 8, 12}}};
  ok = engine.setSeedGroove(tomOnlyLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds for section B with content distinct from A");

  ok = engine.selectSection(arrangerSectionAId, error);
  expect(ok, "selectSection() switches back to A before configuring the arrangement");

  const std::vector<JsEngine::ArrangementBlock> arrangementBlocks = {
      {arrangerSectionAId, 2}, {sectionBInfo.id, 1}};
  ok = engine.setArrangement(arrangementBlocks, error);
  expect(ok, "setArrangement() succeeds");

  std::vector<JsEngine::ArrangementBlock> roundTrippedBlocks;
  ok = engine.getArrangement(roundTrippedBlocks, error);
  expect(ok && roundTrippedBlocks.size() == 2
             && roundTrippedBlocks[0].sectionId == arrangerSectionAId && roundTrippedBlocks[0].bars == 2
             && roundTrippedBlocks[1].sectionId == sectionBInfo.id && roundTrippedBlocks[1].bars == 1,
         "getArrangement() reflects the configured blocks in order");

  std::vector<JsEngine::MidiEvent> arrangedPreview;
  ok = engine.renderPlaybackPreview(arrangedPreview, 0.0, 4.0, 4, noHumanizeParams, error);
  const bool bar0HasSectionAKick = std::any_of(arrangedPreview.begin(), arrangedPreview.end(), [](const auto& e) {
    return e.note == 36 && e.beatPosition < 4.0;
  });
  const bool bar2IsSectionBOnly = std::all_of(arrangedPreview.begin(), arrangedPreview.end(), [](const auto& e) {
    return !(e.beatPosition >= 8.0 && e.beatPosition < 12.0) || e.note == 45;
  });
  const bool bar2HasSectionBTom = std::any_of(arrangedPreview.begin(), arrangedPreview.end(), [](const auto& e) {
    return e.note == 45 && e.beatPosition >= 8.0 && e.beatPosition < 12.0;
  });
  const bool bar3WrapsBackToSectionA = std::any_of(arrangedPreview.begin(), arrangedPreview.end(), [](const auto& e) {
    return e.note == 36 && e.beatPosition >= 12.0 && e.beatPosition < 16.0;
  });
  expect(ok && bar0HasSectionAKick && bar2IsSectionBOnly && bar2HasSectionBTom && bar3WrapsBackToSectionA,
         "the arrangement plays section A for its bars and section B for its bar, then wraps back to A");

  // Configure BOTH sections' own auto-evolution schedules — B's while B is
  // NOT the currently-active/audible section — and confirm ticking evolves
  // both independently, not just whichever happens to be active.
  const JsEngine::EvolutionRule fillOnlyRule{"fill-bg", 0.0, 0.0, 1.0, 0.0};
  ok = engine.setRulePool({{fillOnlyRule, 1.0}}, error);
  expect(ok, "setRulePool() configures section A's (the active section's) own pool");
  ok = engine.configureAutoEvolution(true, 1, 0, error);
  expect(ok, "configureAutoEvolution() configures section A's (the active section's) own schedule");

  ok = engine.selectSection(sectionBInfo.id, error);
  expect(ok, "selectSection() switches to B to configure its own, separate schedule");
  JsEngine::EvolutionRule embellishOnlyRule{"embellish-bg", 0.0, 1.0, 0.0, 0.0, 0.0};
  embellishOnlyRule.params = {{"embellishProbability", 1.0}};
  ok = engine.setRulePool({{embellishOnlyRule, 1.0}}, error);
  expect(ok, "setRulePool() configures section B's own pool independently of A's");
  ok = engine.configureAutoEvolution(true, 1, 0, error);
  expect(ok, "configureAutoEvolution() configures section B's own schedule independently of A's");

  ok = engine.selectSection(arrangerSectionAId, error);
  expect(ok, "selectSection() switches back to A without disturbing B's just-configured schedule");

  JsEngine::SessionInfo infoABeforeTick;
  engine.getSessionInfo(infoABeforeTick, error);
  engine.selectSection(sectionBInfo.id, error);
  JsEngine::SessionInfo infoBBeforeTick;
  engine.getSessionInfo(infoBBeforeTick, error);
  engine.selectSection(arrangerSectionAId, error);

  std::vector<JsEngine::AutoEvolutionFiredEvent> firedEvents;
  ok = engine.tickAutoEvolution(1, firedEvents, error);
  expect(ok && firedEvents.size() == 2, "tickAutoEvolution() evolves every due section in one call, not just the active one");
  const bool sectionAFired = std::any_of(firedEvents.begin(), firedEvents.end(),
      [&](const auto& e) { return e.sectionId == arrangerSectionAId; });
  const bool sectionBFired = std::any_of(firedEvents.begin(), firedEvents.end(),
      [&](const auto& e) { return e.sectionId == sectionBInfo.id; });
  expect(sectionAFired && sectionBFired,
         "both the currently-active section and a background section evolve on their own schedule");

  JsEngine::SessionInfo infoAAfterTick;
  engine.getSessionInfo(infoAAfterTick, error);
  expect(infoAAfterTick.nodeCount == infoABeforeTick.nodeCount + 1, "ticking grows section A's tree by exactly one node");

  engine.selectSection(sectionBInfo.id, error);
  JsEngine::SessionInfo infoBAfterTick;
  engine.getSessionInfo(infoBAfterTick, error);
  expect(infoBAfterTick.nodeCount == infoBBeforeTick.nodeCount + 1,
         "ticking grows section B's tree by one node too, even though B was never the active/audible section");

  ok = engine.resetAutoEvolutionSchedules(100, error);
  expect(ok, "resetAutoEvolutionSchedules() succeeds");
  std::vector<JsEngine::AutoEvolutionFiredEvent> noFireYet;
  engine.tickAutoEvolution(100, noFireYet, error);
  expect(noFireYet.empty(),
         "resetAutoEvolutionSchedules() realigns the next-due bar so a tick at the reset bar itself doesn't immediately fire");

  // --- Weighted rule pool: round-trip, weighted rolling, empty-pool
  // no-op, and per-section independence (DAW testing feedback: "the
  // library should have a selector tick for each rule that opts it in or
  // out for evolutions for each tree, and then there needs to be a list
  // of enabled rules in the rule controller that allows setting weights
  // for how often they occur") --------------------------------------------
  JsEngine::SectionInfo poolSectionA;
  ok = engine.createSection(poolSectionA, error);
  expect(ok, "createSection() succeeds ahead of rule-pool checks");
  ok = engine.setSeedGroove(seedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds for the rule-pool section");

  const JsEngine::EvolutionRule poolFillRule{"pool-fill", 0.0, 0.0, 1.0, 0.0};
  JsEngine::EvolutionRule poolEmbellishRule{"pool-embellish", 0.0, 1.0, 0.0, 0.0};
  poolEmbellishRule.params = {{"embellishProbability", 1.0}};

  ok = engine.setRulePool({{poolFillRule, 3.0}, {poolEmbellishRule, 1.0}}, error);
  expect(ok, "setRulePool() succeeds");

  std::vector<JsEngine::RulePoolEntry> roundTripped;
  ok = engine.getRulePool(roundTripped, error);
  expect(ok && roundTripped.size() == 2, "getRulePool() round-trips the pool with the right number of entries");
  const auto foundFill = std::find_if(roundTripped.begin(), roundTripped.end(),
      [](const auto& e) { return e.rule.id == "pool-fill"; });
  const auto foundEmbellish = std::find_if(roundTripped.begin(), roundTripped.end(),
      [](const auto& e) { return e.rule.id == "pool-embellish"; });
  expect(foundFill != roundTripped.end() && foundFill->frequency == 3.0 && foundFill->rule.fill == 1.0,
         "getRulePool() round-trips a rule's id, weights, and frequency exactly");
  expect(foundEmbellish != roundTripped.end() && foundEmbellish->frequency == 1.0
             && !foundEmbellish->rule.params.empty()
             && foundEmbellish->rule.params[0].first == "embellishProbability"
             && foundEmbellish->rule.params[0].second == 1.0,
         "getRulePool() round-trips a rule's arbitrary-keyed params object exactly");

  // A heavily-weighted rule should be chosen far more often than a
  // lightly-weighted one across many rolls. branch=true so each roll is an
  // independent sibling rather than compounding onto a growing head.
  ok = engine.setRulePool({{poolFillRule, 100.0}, {poolEmbellishRule, 0.001}}, error);
  expect(ok, "setRulePool() succeeds for the weighted-rolling check");
  int fillCount = 0;
  int embellishCount = 0;
  for (int i = 0; i < 30; ++i) {
    JsEngine::EvolutionResult rolled;
    ok = engine.evolveFromPool(true, rolled, error);
    if (!ok) break;
    if (rolled.operation == "fill") ++fillCount;
    if (rolled.operation == "embellish") ++embellishCount;
  }
  expect(ok && fillCount > embellishCount && fillCount >= 25,
         "evolveFromPool() rolls the heavily-weighted rule far more often than the lightly-weighted one");

  // Empty pool is a safe no-op, not an error.
  ok = engine.setRulePool({}, error);
  expect(ok, "setRulePool() accepts an empty pool");
  JsEngine::SessionInfo infoBeforeEmptyRoll;
  engine.getSessionInfo(infoBeforeEmptyRoll, error);
  JsEngine::EvolutionResult emptyRollResult;
  ok = engine.evolveFromPool(false, emptyRollResult, error);
  JsEngine::SessionInfo infoAfterEmptyRoll;
  engine.getSessionInfo(infoAfterEmptyRoll, error);
  expect(ok && emptyRollResult.nodeId.empty() && infoAfterEmptyRoll.nodeCount == infoBeforeEmptyRoll.nodeCount,
         "evolveFromPool() on an empty pool is a safe no-op, not a bridge error");

  // Per-section independence: a second section's pool must not see the
  // first section's entries (or lack thereof).
  JsEngine::SectionInfo poolSectionB;
  ok = engine.createSection(poolSectionB, error);
  expect(ok, "createSection() succeeds for the second rule-pool section");
  ok = engine.setRulePool({{poolEmbellishRule, 1.0}}, error);
  expect(ok, "setRulePool() configures section B's own pool");

  ok = engine.selectSection(poolSectionA.id, error);
  expect(ok, "selectSection() switches back to section A");
  std::vector<JsEngine::RulePoolEntry> sectionAPoolAfterBConfigured;
  ok = engine.getRulePool(sectionAPoolAfterBConfigured, error);
  expect(ok && sectionAPoolAfterBConfigured.empty(),
         "section A's pool (emptied above) is unaffected by configuring section B's own pool");

  ok = engine.selectSection(poolSectionB.id, error);
  expect(ok, "selectSection() switches to section B");
  std::vector<JsEngine::RulePoolEntry> sectionBPool;
  ok = engine.getRulePool(sectionBPool, error);
  expect(ok && sectionBPool.size() == 1 && sectionBPool[0].rule.id == "pool-embellish",
         "section B's pool is genuinely independent of section A's");

  // --- Per-note evolution: trace one note across the head path (DAW
  // testing feedback: "below the seed editor we need a visualizer for
  // whatever cell we've clicked on to see where it evolved to") ---------
  JsEngine::SectionInfo noteEvoSection;
  ok = engine.createSection(noteEvoSection, error);
  expect(ok, "createSection() succeeds ahead of note-evolution checks");

  const std::vector<JsEngine::SeedLane> noteEvoSeedLanes = {
      {"kick", "Kick", 36, "", 100, {0, 8}},
      {"snare", "Snare", 38, "", 96, {4, 12}},
  };
  ok = engine.setSeedGroove(noteEvoSeedLanes, 16, 4, error);
  expect(ok, "setSeedGroove() succeeds for the note-evolution section");

  std::vector<JsEngine::NoteEvolutionEntry> rootOnly;
  ok = engine.getNoteEvolution("kick", 0.0, rootOnly, error);
  expect(ok && rootOnly.size() == 1 && rootOnly[0].operation == "root" && rootOnly[0].present
             && std::abs(rootOnly[0].position - 0.0) < 1e-6,
         "getNoteEvolution() on a freshly-seeded section reports just the root generation");

  std::vector<JsEngine::NoteEvolutionEntry> noNoteHere;
  ok = engine.getNoteEvolution("kick", 3.75, noNoteHere, error);
  expect(ok && noNoteHere.empty(),
         "getNoteEvolution() at a seed position with no note is a safe empty result, not an error");

  const JsEngine::EvolutionRule holdOnlyRule{"hold-only", 0.0, 0.0, 0.0, 1.0};
  JsEngine::EvolutionResult holdResult;
  ok = engine.evolveWithRule(holdOnlyRule, false, holdResult, error);
  expect(ok && holdResult.operation == "hold",
         "hold-only rule evolves via hold, ahead of note-evolution checks");

  std::vector<JsEngine::NoteEvolutionEntry> afterHold;
  ok = engine.getNoteEvolution("kick", 0.0, afterHold, error);
  expect(ok && afterHold.size() == 2 && afterHold[1].operation == "hold" && afterHold[1].present
             && std::abs(afterHold[1].position - 0.0) < 1e-6,
         "getNoteEvolution() grows by one generation after a hold evolution, note still present and unmoved");

  JsEngine::EvolutionRule mutationOnlyEvoRule{"mutation-evo", 1.0, 0.0, 0.0, 0.0};
  mutationOnlyEvoRule.params = {{"mutationAmount", 40.0}};
  JsEngine::EvolutionResult mutationResult;
  ok = engine.evolveWithRule(mutationOnlyEvoRule, false, mutationResult, error);
  expect(ok && mutationResult.operation == "mutation",
         "mutation-only rule evolves via mutation, ahead of note-evolution checks");

  std::vector<JsEngine::NoteEvolutionEntry> afterMutation;
  ok = engine.getNoteEvolution("kick", 0.0, afterMutation, error);
  expect(ok && afterMutation.size() == 3 && afterMutation[2].operation == "mutation"
             && afterMutation[2].present && std::abs(afterMutation[2].position - 0.0) < 1e-6
             && afterMutation[2].velocity >= 1.0 && afterMutation[2].velocity <= 127.0,
         "getNoteEvolution() reflects a velocity-only mutation: note stays present, position "
         "unchanged, velocity a valid MIDI value (not necessarily different — the mutation's own "
         "probability param, not tested here, may leave any single note unchanged)");

  JsEngine::EvolutionRule embellishOnlyEvoRule{"embellish-evo", 0.0, 1.0, 0.0, 0.0};
  embellishOnlyEvoRule.params = {{"embellishProbability", 1.0}};
  JsEngine::EvolutionResult embellishEvoResult;
  ok = engine.evolveWithRule(embellishOnlyEvoRule, false, embellishEvoResult, error);
  expect(ok && embellishEvoResult.operation == "embellish",
         "embellish-only rule evolves via embellish, ahead of note-evolution checks");

  std::vector<JsEngine::NoteEvolutionEntry> afterEmbellish;
  ok = engine.getNoteEvolution("kick", 0.0, afterEmbellish, error);
  expect(ok && afterEmbellish.size() == 4 && afterEmbellish[3].operation == "embellish"
             && afterEmbellish[3].present,
         "the original seed note is untouched (still present) by an embellish generation that only "
         "inserts a new ghost note — embellish never removes an existing note");

  // BRANCH creates a sibling of the current head (child of the head's
  // parent) and that sibling becomes the new head — so getNoteEvolution()
  // should now report the *whole tree*, not just one branch: both the old
  // (now non-head) embellish node and the new (head) sibling, both
  // children of the same mutation-generation parent.
  JsEngine::EvolutionResult branchResult;
  ok = engine.evolveWithRule(holdOnlyRule, true, branchResult, error);
  expect(ok && branchResult.operation == "hold" && branchResult.parentId == mutationResult.nodeId,
         "BRANCH evolves via hold, forking off the mutation generation (embellish's parent), not "
         "off embellish itself");

  std::vector<JsEngine::NoteEvolutionEntry> afterBranch;
  ok = engine.getNoteEvolution("kick", 0.0, afterBranch, error);
  expect(ok && afterBranch.size() == 5,
         "getNoteEvolution() after a BRANCH reports every node in the tree (5), not just one "
         "branch's worth (4)");

  const auto embellishEntry = std::find_if(afterBranch.begin(), afterBranch.end(),
      [](const JsEngine::NoteEvolutionEntry& e) { return e.operation == "embellish"; });
  const auto branchEntry = std::find_if(afterBranch.begin(), afterBranch.end(), [&](const JsEngine::NoteEvolutionEntry& e) {
    return e.operation == "hold" && e.nodeId == branchResult.nodeId;
  });
  expect(embellishEntry != afterBranch.end() && branchEntry != afterBranch.end()
             && embellishEntry->parentNodeId == mutationResult.nodeId
             && branchEntry->parentNodeId == mutationResult.nodeId
             && embellishEntry->generation == branchEntry->generation,
         "the old embellish node and the new branch node are true siblings — same parent, same depth");
  expect(embellishEntry != afterBranch.end() && branchEntry != afterBranch.end()
             && !embellishEntry->isHeadPath && branchEntry->isHeadPath,
         "isHeadPath correctly follows the head to the new branch, off the now-abandoned embellish node");

  const auto rootEntry = std::find_if(afterBranch.begin(), afterBranch.end(),
      [](const JsEngine::NoteEvolutionEntry& e) { return e.operation == "root"; });
  expect(rootEntry != afterBranch.end() && rootEntry->isHeadPath,
         "shared ancestry (root, and everything up to the branch point) stays on the head path for "
         "both branches");

  ok = engine.selectSection(arrangerSectionAId, error);
  expect(ok, "selectSection() restores A as active for test-cleanliness");

  std::printf("\n%s\n", failures == 0 ? "All bridge tests passed." : "Bridge tests FAILED.");
  return failures == 0 ? 0 : 1;
}
