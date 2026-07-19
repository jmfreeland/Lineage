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
  const JsEngine::AutoEvolutionPreview scheduledFill{true, fillRule, 1, 4};
  std::vector<JsEngine::MidiEvent> scheduledPreview;
  ok = engine.renderPlaybackPreview(scheduledPreview, 0.0, 4.0, 8, params, error, &scheduledFill);
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

  std::printf("\n%s\n", failures == 0 ? "All bridge tests passed." : "Bridge tests FAILED.");
  return failures == 0 ? 0 : 1;
}
