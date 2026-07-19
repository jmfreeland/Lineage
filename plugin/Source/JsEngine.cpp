#include "JsEngine.h"
#include "quickjs.h"

namespace {

std::string describeException(JSContext* ctx) {
  JSValue exception = JS_GetException(ctx);
  const char* str = JS_ToCString(ctx, exception);
  std::string message = str != nullptr ? str : "(unknown JS error)";
  if (str != nullptr) JS_FreeCString(ctx, str);
  JS_FreeValue(ctx, exception);
  return message;
}

JSValue makeParamsObject(JSContext* context,
                         const std::vector<std::pair<std::string, double>>& params) {
  JSValue paramsObj = JS_NewObject(context);
  for (const auto& [name, value] : params) {
    JS_SetPropertyStr(context, paramsObj, name.c_str(), JS_NewFloat64(context, value));
  }
  return paramsObj;
}

// Marshals an EvolutionRule into a fresh JS object — shared by
// evolveWithRule() and setRulePool(), since a rule pool entry carries a
// full rule definition (this engine has no independent registry of rule
// ids to look up). Caller owns and must JS_FreeValue() the result.
JSValue makeRuleObject(JSContext* context, const JsEngine::EvolutionRule& rule) {
  JSValue ruleObj = JS_NewObject(context);
  JS_SetPropertyStr(context, ruleObj, "id", JS_NewString(context, rule.id.c_str()));
  JS_SetPropertyStr(context, ruleObj, "mutation", JS_NewFloat64(context, rule.mutation));
  JS_SetPropertyStr(context, ruleObj, "embellish", JS_NewFloat64(context, rule.embellish));
  JS_SetPropertyStr(context, ruleObj, "fill", JS_NewFloat64(context, rule.fill));
  JS_SetPropertyStr(context, ruleObj, "hold", JS_NewFloat64(context, rule.hold));
  JS_SetPropertyStr(context, ruleObj, "settle", JS_NewFloat64(context, rule.settle));
  JS_SetPropertyStr(context, ruleObj, "params", makeParamsObject(context, rule.params));
  return ruleObj;
}

bool readPlaybackEvents(JSContext* context,
                        JSValue resultValue,
                        const char* functionName,
                        std::vector<JsEngine::MidiEvent>& eventsOut,
                        std::string& errorOut) {
  if (!JS_IsArray(resultValue)) {
    errorOut = std::string(functionName) + " did not return an array";
    return false;
  }

  JSValue lengthValue = JS_GetPropertyStr(context, resultValue, "length");
  int32_t resultLength = 0;
  JS_ToInt32(context, &resultLength, lengthValue);
  JS_FreeValue(context, lengthValue);

  std::vector<JsEngine::MidiEvent> renderedEvents;
  renderedEvents.reserve(static_cast<size_t>(resultLength));
  for (int32_t i = 0; i < resultLength; ++i) {
    JSValue item = JS_GetPropertyUint32(context, resultValue, static_cast<uint32_t>(i));
    JsEngine::MidiEvent event;
    JSValue field;

    field = JS_GetPropertyStr(context, item, "note");
    JS_ToInt32(context, &event.note, field);
    JS_FreeValue(context, field);

    field = JS_GetPropertyStr(context, item, "velocity");
    JS_ToInt32(context, &event.velocity, field);
    JS_FreeValue(context, field);

    field = JS_GetPropertyStr(context, item, "channel");
    JS_ToInt32(context, &event.channel, field);
    JS_FreeValue(context, field);

    field = JS_GetPropertyStr(context, item, "samplePosition");
    JS_ToInt32(context, &event.samplePosition, field);
    JS_FreeValue(context, field);

    field = JS_GetPropertyStr(context, item, "beatPosition");
    JS_ToFloat64(context, &event.beatPosition, field);
    JS_FreeValue(context, field);

    field = JS_GetPropertyStr(context, item, "durationBeats");
    JS_ToFloat64(context, &event.durationBeats, field);
    JS_FreeValue(context, field);

    field = JS_GetPropertyStr(context, item, "previewFlags");
    if (!JS_IsUndefined(field)) JS_ToInt32(context, &event.previewFlags, field);
    JS_FreeValue(context, field);

    JS_FreeValue(context, item);
    renderedEvents.push_back(event);
  }
  eventsOut = std::move(renderedEvents);
  return true;
}

} // namespace

JsEngine::JsEngine() {
  runtime = JS_NewRuntime();
  context = JS_NewContext(runtime);
}

JsEngine::~JsEngine() {
  if (context != nullptr) JS_FreeContext(context);
  if (runtime != nullptr) JS_FreeRuntime(runtime);
}

bool JsEngine::loadScript(const std::string& source, const std::string& scriptName, std::string& errorOut) {
  JSValue result = JS_Eval(context, source.c_str(), source.size(), scriptName.c_str(), JS_EVAL_TYPE_GLOBAL);
  bool ok = !JS_IsException(result);
  if (!ok) errorOut = describeException(context);
  JS_FreeValue(context, result);
  return ok;
}

bool JsEngine::processBlock(std::vector<MidiEvent>& events,
                             const Transport& transport,
                             const std::vector<std::pair<std::string, double>>& params,
                             std::string& errorOut) {
  if (events.empty()) return true;

  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageProcessBlock");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageProcessBlock is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue inputArray = JS_NewArray(context);
  for (size_t i = 0; i < events.size(); ++i) {
    JSValue obj = JS_NewObject(context);
    JS_SetPropertyStr(context, obj, "note", JS_NewInt32(context, events[i].note));
    JS_SetPropertyStr(context, obj, "velocity", JS_NewInt32(context, events[i].velocity));
    JS_SetPropertyStr(context, obj, "channel", JS_NewInt32(context, events[i].channel));
    JS_SetPropertyStr(context, obj, "samplePosition", JS_NewInt32(context, events[i].samplePosition));
    JS_SetPropertyStr(context, obj, "beatPosition", JS_NewFloat64(context, events[i].beatPosition));
    JS_SetPropertyUint32(context, inputArray, static_cast<uint32_t>(i), obj);
  }

  JSValue transportObj = JS_NewObject(context);
  JS_SetPropertyStr(context, transportObj, "tempo", JS_NewFloat64(context, transport.tempo));
  JS_SetPropertyStr(context, transportObj, "beatsPerBar", JS_NewFloat64(context, transport.beatsPerBar));
  JS_SetPropertyStr(context, transportObj, "blockStartBeat", JS_NewFloat64(context, transport.blockStartBeat));
  JS_SetPropertyStr(context, transportObj, "sampleRate", JS_NewFloat64(context, transport.sampleRate));

  JSValue paramsObj = JS_NewObject(context);
  for (const auto& [key, value] : params) {
    JS_SetPropertyStr(context, paramsObj, key.c_str(), JS_NewFloat64(context, value));
  }

  JSValueConst argv[] = {inputArray, transportObj, paramsObj};
  JSValue resultValue = JS_Call(context, func, global, 3, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else if (!JS_IsArray(resultValue)) {
    ok = false;
    errorOut = "__lineageProcessBlock did not return an array";
  } else {
    // The result array's own length, not events.size() — mutations like
    // ghostNote can change the note count, so input/output lengths may
    // legitimately differ.
    JSValue lengthValue = JS_GetPropertyStr(context, resultValue, "length");
    int32_t resultLength = 0;
    JS_ToInt32(context, &resultLength, lengthValue);
    JS_FreeValue(context, lengthValue);

    std::vector<MidiEvent> outEvents;
    outEvents.reserve(static_cast<size_t>(resultLength));
    for (int32_t i = 0; i < resultLength; ++i) {
      JSValue item = JS_GetPropertyUint32(context, resultValue, static_cast<uint32_t>(i));
      MidiEvent event;
      JSValue field;

      field = JS_GetPropertyStr(context, item, "note");
      JS_ToInt32(context, &event.note, field);
      JS_FreeValue(context, field);

      field = JS_GetPropertyStr(context, item, "velocity");
      JS_ToInt32(context, &event.velocity, field);
      JS_FreeValue(context, field);

      field = JS_GetPropertyStr(context, item, "channel");
      JS_ToInt32(context, &event.channel, field);
      JS_FreeValue(context, field);

      field = JS_GetPropertyStr(context, item, "samplePosition");
      JS_ToInt32(context, &event.samplePosition, field);
      JS_FreeValue(context, field);

      JS_FreeValue(context, item);
      outEvents.push_back(event);
    }
    events = std::move(outEvents);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, paramsObj);
  JS_FreeValue(context, transportObj);
  JS_FreeValue(context, inputArray);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::renderPlaybackBlock(std::vector<MidiEvent>& eventsOut,
                                    const Transport& transport,
                                    int32_t blockSizeSamples,
                                    const std::vector<std::pair<std::string, double>>& params,
                                    std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageRenderPlaybackBlock");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageRenderPlaybackBlock is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue transportObj = JS_NewObject(context);
  JS_SetPropertyStr(context, transportObj, "tempo", JS_NewFloat64(context, transport.tempo));
  JS_SetPropertyStr(context, transportObj, "beatsPerBar", JS_NewFloat64(context, transport.beatsPerBar));
  JS_SetPropertyStr(context, transportObj, "blockStartBeat", JS_NewFloat64(context, transport.blockStartBeat));
  JS_SetPropertyStr(context, transportObj, "sampleRate", JS_NewFloat64(context, transport.sampleRate));

  JSValue blockSizeValue = JS_NewInt32(context, blockSizeSamples);
  JSValue paramsObj = makeParamsObject(context, params);
  JSValueConst argv[] = {transportObj, blockSizeValue, paramsObj};
  JSValue resultValue = JS_Call(context, func, global, 3, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else {
    ok = readPlaybackEvents(context, resultValue, "__lineageRenderPlaybackBlock", eventsOut, errorOut);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, paramsObj);
  JS_FreeValue(context, blockSizeValue);
  JS_FreeValue(context, transportObj);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::renderPlaybackPreview(std::vector<MidiEvent>& eventsOut,
                                      double startBeat,
                                      double beatsPerBar,
                                      int32_t barCount,
                                      const std::vector<std::pair<std::string, double>>& params,
                                      std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageRenderPlaybackPreview");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageRenderPlaybackPreview is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue startValue = JS_NewFloat64(context, startBeat);
  JSValue beatsValue = JS_NewFloat64(context, beatsPerBar);
  JSValue barCountValue = JS_NewInt32(context, barCount);
  JSValue paramsObj = makeParamsObject(context, params);
  JSValueConst argv[] = {startValue, beatsValue, barCountValue, paramsObj};
  JSValue resultValue = JS_Call(context, func, global, 4, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else {
    ok = readPlaybackEvents(context, resultValue, "__lineageRenderPlaybackPreview", eventsOut, errorOut);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, paramsObj);
  JS_FreeValue(context, barCountValue);
  JS_FreeValue(context, beatsValue);
  JS_FreeValue(context, startValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::getSessionInfo(SessionInfo& infoOut, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageGetSessionInfo");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageGetSessionInfo is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue resultValue = JS_Call(context, func, global, 0, nullptr);
  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else {
    JSValue nodeCountValue = JS_GetPropertyStr(context, resultValue, "nodeCount");
    JS_ToInt32(context, &infoOut.nodeCount, nodeCountValue);
    JS_FreeValue(context, nodeCountValue);

    JSValue headNodeIdValue = JS_GetPropertyStr(context, resultValue, "headNodeId");
    const char* headNodeIdStr = JS_ToCString(context, headNodeIdValue);
    infoOut.headNodeId = headNodeIdStr != nullptr ? headNodeIdStr : "";
    if (headNodeIdStr != nullptr) JS_FreeCString(context, headNodeIdStr);
    JS_FreeValue(context, headNodeIdValue);

    JSValue rootNoteCountValue = JS_GetPropertyStr(context, resultValue, "rootNoteCount");
    JS_ToInt32(context, &infoOut.rootNoteCount, rootNoteCountValue);
    JS_FreeValue(context, rootNoteCountValue);

    JSValue rootLaneCountValue = JS_GetPropertyStr(context, resultValue, "rootLaneCount");
    JS_ToInt32(context, &infoOut.rootLaneCount, rootLaneCountValue);
    JS_FreeValue(context, rootLaneCountValue);

    JSValue groupedLaneCountValue = JS_GetPropertyStr(context, resultValue, "groupedLaneCount");
    JS_ToInt32(context, &infoOut.groupedLaneCount, groupedLaneCountValue);
    JS_FreeValue(context, groupedLaneCountValue);

    JSValue sectionIdValue = JS_GetPropertyStr(context, resultValue, "sectionId");
    const char* sectionIdStr = JS_ToCString(context, sectionIdValue);
    infoOut.sectionId = sectionIdStr != nullptr ? sectionIdStr : "";
    if (sectionIdStr != nullptr) JS_FreeCString(context, sectionIdStr);
    JS_FreeValue(context, sectionIdValue);

    JSValue sectionNameValue = JS_GetPropertyStr(context, resultValue, "sectionName");
    const char* sectionNameStr = JS_ToCString(context, sectionNameValue);
    infoOut.sectionName = sectionNameStr != nullptr ? sectionNameStr : "";
    if (sectionNameStr != nullptr) JS_FreeCString(context, sectionNameStr);
    JS_FreeValue(context, sectionNameValue);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::setSeedGroove(const std::vector<SeedLane>& lanes,
                              int32_t stepsPerBar,
                              int32_t beatsPerBar,
                              std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageSetSeedGroove");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageSetSeedGroove is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue lanesArray = JS_NewArray(context);
  for (size_t i = 0; i < lanes.size(); ++i) {
    JSValue obj = JS_NewObject(context);
    JS_SetPropertyStr(context, obj, "id", JS_NewString(context, lanes[i].id.c_str()));
    JS_SetPropertyStr(context, obj, "name", JS_NewString(context, lanes[i].name.c_str()));
    JS_SetPropertyStr(context, obj, "midiNote", JS_NewInt32(context, lanes[i].midiNote));
    JS_SetPropertyStr(context, obj, "group", JS_NewString(context, lanes[i].group.c_str()));
    JS_SetPropertyStr(context, obj, "velocity", JS_NewInt32(context, lanes[i].velocity));

    JSValue stepsArray = JS_NewArray(context);
    for (size_t stepIndex = 0; stepIndex < lanes[i].activeSteps.size(); ++stepIndex) {
      JS_SetPropertyUint32(context,
                           stepsArray,
                           static_cast<uint32_t>(stepIndex),
                           JS_NewInt32(context, lanes[i].activeSteps[stepIndex]));
    }
    JS_SetPropertyStr(context, obj, "activeSteps", stepsArray);
    JS_SetPropertyUint32(context, lanesArray, static_cast<uint32_t>(i), obj);
  }

  JSValueConst argv[] = {lanesArray, JS_NewInt32(context, stepsPerBar), JS_NewInt32(context, beatsPerBar)};
  JSValue resultValue = JS_Call(context, func, global, 3, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, argv[2]);
  JS_FreeValue(context, argv[1]);
  JS_FreeValue(context, lanesArray);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::evolveWithRule(const EvolutionRule& rule,
                              bool branch,
                              EvolutionResult& resultOut,
                              std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageEvolveWithRule");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageEvolveWithRule is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue ruleObj = makeRuleObject(context, rule);
  JSValue branchValue = JS_NewBool(context, branch);
  JSValueConst argv[] = {ruleObj, branchValue};
  JSValue resultValue = JS_Call(context, func, global, 2, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else {
    auto readString = [this, resultValue](const char* fieldName) {
      JSValue value = JS_GetPropertyStr(context, resultValue, fieldName);
      const char* text = JS_ToCString(context, value);
      std::string result = text != nullptr ? text : "";
      if (text != nullptr) JS_FreeCString(context, text);
      JS_FreeValue(context, value);
      return result;
    };
    resultOut.nodeId = readString("nodeId");
    resultOut.parentId = readString("parentId");
    resultOut.operation = readString("operation");
    if (resultOut.nodeId.empty() || resultOut.operation.empty()) {
      ok = false;
      errorOut = "__lineageEvolveWithRule returned an incomplete result";
    }
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, branchValue);
  JS_FreeValue(context, ruleObj);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::evolveFromPool(bool branch, EvolutionResult& resultOut, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageEvolveFromPool");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageEvolveFromPool is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue branchValue = JS_NewBool(context, branch);
  JSValueConst argv[] = {branchValue};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else if (JS_IsNull(resultValue) || JS_IsUndefined(resultValue)) {
    // An empty pool (nothing enabled) — a safe no-op, not a bridge error.
    resultOut = EvolutionResult{};
  } else {
    auto readString = [this, resultValue](const char* fieldName) {
      JSValue value = JS_GetPropertyStr(context, resultValue, fieldName);
      const char* text = JS_ToCString(context, value);
      std::string result = text != nullptr ? text : "";
      if (text != nullptr) JS_FreeCString(context, text);
      JS_FreeValue(context, value);
      return result;
    };
    resultOut.nodeId = readString("nodeId");
    resultOut.parentId = readString("parentId");
    resultOut.operation = readString("operation");
    resultOut.ruleId = readString("ruleId");
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, branchValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::setVocabulary(const std::string& json, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageSetVocabulary");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageSetVocabulary is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue jsonValue = JS_NewString(context, json.c_str());
  JSValueConst argv[] = {jsonValue};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, jsonValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::clearVocabulary(std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageClearVocabulary");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageClearVocabulary is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue resultValue = JS_Call(context, func, global, 0, nullptr);
  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

namespace {
JsEngine::SectionInfo readSectionInfo(JSContext* context, JSValue value) {
  JsEngine::SectionInfo info;

  JSValue idValue = JS_GetPropertyStr(context, value, "id");
  const char* idStr = JS_ToCString(context, idValue);
  info.id = idStr != nullptr ? idStr : "";
  if (idStr != nullptr) JS_FreeCString(context, idStr);
  JS_FreeValue(context, idValue);

  JSValue nameValue = JS_GetPropertyStr(context, value, "name");
  const char* nameStr = JS_ToCString(context, nameValue);
  info.name = nameStr != nullptr ? nameStr : "";
  if (nameStr != nullptr) JS_FreeCString(context, nameStr);
  JS_FreeValue(context, nameValue);

  JSValue activeValue = JS_GetPropertyStr(context, value, "active");
  info.active = JS_ToBool(context, activeValue) != 0;
  JS_FreeValue(context, activeValue);

  return info;
}
} // namespace

bool JsEngine::createSection(SectionInfo& infoOut, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageCreateSection");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageCreateSection is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue resultValue = JS_Call(context, func, global, 0, nullptr);
  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else {
    infoOut = readSectionInfo(context, resultValue);
    infoOut.active = true;
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::listSections(std::vector<SectionInfo>& sectionsOut, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageListSections");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageListSections is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue resultValue = JS_Call(context, func, global, 0, nullptr);
  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else if (!JS_IsArray(resultValue)) {
    ok = false;
    errorOut = "__lineageListSections did not return an array";
  } else {
    JSValue lengthValue = JS_GetPropertyStr(context, resultValue, "length");
    int32_t length = 0;
    JS_ToInt32(context, &length, lengthValue);
    JS_FreeValue(context, lengthValue);

    std::vector<SectionInfo> sections;
    sections.reserve(static_cast<size_t>(length));
    for (int32_t i = 0; i < length; ++i) {
      JSValue item = JS_GetPropertyUint32(context, resultValue, static_cast<uint32_t>(i));
      sections.push_back(readSectionInfo(context, item));
      JS_FreeValue(context, item);
    }
    sectionsOut = std::move(sections);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::selectSection(const std::string& id, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageSelectSection");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageSelectSection is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue idValue = JS_NewString(context, id.c_str());
  JSValueConst argv[] = {idValue};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, idValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::deleteSection(const std::string& id, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageDeleteSection");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageDeleteSection is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue idValue = JS_NewString(context, id.c_str());
  JSValueConst argv[] = {idValue};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, idValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::setArrangement(const std::vector<ArrangementBlock>& blocks, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageSetArrangement");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageSetArrangement is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue blocksArray = JS_NewArray(context);
  for (size_t i = 0; i < blocks.size(); ++i) {
    JSValue obj = JS_NewObject(context);
    JS_SetPropertyStr(context, obj, "sectionId", JS_NewString(context, blocks[i].sectionId.c_str()));
    JS_SetPropertyStr(context, obj, "bars", JS_NewInt32(context, blocks[i].bars));
    JS_SetPropertyUint32(context, blocksArray, static_cast<uint32_t>(i), obj);
  }
  JSValueConst argv[] = {blocksArray};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, blocksArray);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::getArrangement(std::vector<ArrangementBlock>& blocksOut, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageGetArrangement");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageGetArrangement is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue resultValue = JS_Call(context, func, global, 0, nullptr);
  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else if (!JS_IsArray(resultValue)) {
    ok = false;
    errorOut = "__lineageGetArrangement did not return an array";
  } else {
    JSValue lengthValue = JS_GetPropertyStr(context, resultValue, "length");
    int32_t length = 0;
    JS_ToInt32(context, &length, lengthValue);
    JS_FreeValue(context, lengthValue);

    std::vector<ArrangementBlock> blocks;
    blocks.reserve(static_cast<size_t>(length));
    for (int32_t i = 0; i < length; ++i) {
      JSValue item = JS_GetPropertyUint32(context, resultValue, static_cast<uint32_t>(i));
      ArrangementBlock block;

      JSValue idValue = JS_GetPropertyStr(context, item, "sectionId");
      const char* idStr = JS_ToCString(context, idValue);
      block.sectionId = idStr != nullptr ? idStr : "";
      if (idStr != nullptr) JS_FreeCString(context, idStr);
      JS_FreeValue(context, idValue);

      JSValue barsValue = JS_GetPropertyStr(context, item, "bars");
      JS_ToInt32(context, &block.bars, barsValue);
      JS_FreeValue(context, barsValue);

      JS_FreeValue(context, item);
      blocks.push_back(std::move(block));
    }
    blocksOut = std::move(blocks);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::configureAutoEvolution(bool running,
                                      int32_t frequencyBars,
                                      int64_t currentBar,
                                      std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageConfigureAutoEvolution");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageConfigureAutoEvolution is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue runningValue = JS_NewBool(context, running);
  JSValue frequencyValue = JS_NewInt32(context, frequencyBars);
  JSValue currentBarValue = JS_NewFloat64(context, static_cast<double>(currentBar));
  JSValueConst argv[] = {runningValue, frequencyValue, currentBarValue};
  JSValue resultValue = JS_Call(context, func, global, 3, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, currentBarValue);
  JS_FreeValue(context, frequencyValue);
  JS_FreeValue(context, runningValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::setRulePool(const std::vector<RulePoolEntry>& entries, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageSetRulePool");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageSetRulePool is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue entriesArray = JS_NewArray(context);
  for (size_t i = 0; i < entries.size(); ++i) {
    JSValue entryObj = JS_NewObject(context);
    JS_SetPropertyStr(context, entryObj, "rule", makeRuleObject(context, entries[i].rule));
    JS_SetPropertyStr(context, entryObj, "frequency", JS_NewFloat64(context, entries[i].frequency));
    JS_SetPropertyUint32(context, entriesArray, static_cast<uint32_t>(i), entryObj);
  }
  JSValueConst argv[] = {entriesArray};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, entriesArray);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::getRulePool(std::vector<RulePoolEntry>& entriesOut, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageGetRulePool");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageGetRulePool is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue resultValue = JS_Call(context, func, global, 0, nullptr);
  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else if (!JS_IsArray(resultValue)) {
    ok = false;
    errorOut = "__lineageGetRulePool did not return an array";
  } else {
    JSValue lengthValue = JS_GetPropertyStr(context, resultValue, "length");
    int32_t length = 0;
    JS_ToInt32(context, &length, lengthValue);
    JS_FreeValue(context, lengthValue);

    std::vector<RulePoolEntry> entries;
    entries.reserve(static_cast<size_t>(length));
    for (int32_t i = 0; i < length; ++i) {
      JSValue item = JS_GetPropertyUint32(context, resultValue, static_cast<uint32_t>(i));
      RulePoolEntry entry;

      JSValue ruleValue = JS_GetPropertyStr(context, item, "rule");
      JSValue idValue = JS_GetPropertyStr(context, ruleValue, "id");
      const char* idStr = JS_ToCString(context, idValue);
      entry.rule.id = idStr != nullptr ? idStr : "";
      if (idStr != nullptr) JS_FreeCString(context, idStr);
      JS_FreeValue(context, idValue);

      auto readRuleDouble = [this, ruleValue](const char* fieldName) {
        JSValue value = JS_GetPropertyStr(context, ruleValue, fieldName);
        double result = 0.0;
        JS_ToFloat64(context, &result, value);
        JS_FreeValue(context, value);
        return result;
      };
      entry.rule.mutation = readRuleDouble("mutation");
      entry.rule.embellish = readRuleDouble("embellish");
      entry.rule.fill = readRuleDouble("fill");
      entry.rule.hold = readRuleDouble("hold");
      entry.rule.settle = readRuleDouble("settle");

      JSValue paramsValue = JS_GetPropertyStr(context, ruleValue, "params");
      JSPropertyEnum* paramNames = nullptr;
      uint32_t paramCount = 0;
      if (JS_GetOwnPropertyNames(context, &paramNames, &paramCount, paramsValue,
                                 JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
        for (uint32_t p = 0; p < paramCount; ++p) {
          const char* paramName = JS_AtomToCString(context, paramNames[p].atom);
          JSValue paramValue = JS_GetPropertyStr(context, paramsValue, paramName);
          double paramNumber = 0.0;
          JS_ToFloat64(context, &paramNumber, paramValue);
          entry.rule.params.emplace_back(paramName != nullptr ? paramName : "", paramNumber);
          JS_FreeValue(context, paramValue);
          if (paramName != nullptr) JS_FreeCString(context, paramName);
          JS_FreeAtom(context, paramNames[p].atom);
        }
        js_free(context, paramNames);
      }
      JS_FreeValue(context, paramsValue);
      JS_FreeValue(context, ruleValue);

      JSValue frequencyValue = JS_GetPropertyStr(context, item, "frequency");
      JS_ToFloat64(context, &entry.frequency, frequencyValue);
      JS_FreeValue(context, frequencyValue);

      JS_FreeValue(context, item);
      entries.push_back(std::move(entry));
    }
    entriesOut = std::move(entries);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::resetAutoEvolutionSchedules(int64_t currentBar, std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageResetAutoEvolutionSchedules");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageResetAutoEvolutionSchedules is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue currentBarValue = JS_NewFloat64(context, static_cast<double>(currentBar));
  JSValueConst argv[] = {currentBarValue};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) errorOut = describeException(context);

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, currentBarValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}

bool JsEngine::tickAutoEvolution(int64_t currentBar,
                                 std::vector<AutoEvolutionFiredEvent>& eventsOut,
                                 std::string& errorOut) {
  JSValue global = JS_GetGlobalObject(context);
  JSValue func = JS_GetPropertyStr(context, global, "__lineageTickAutoEvolution");
  if (!JS_IsFunction(context, func)) {
    errorOut = "__lineageTickAutoEvolution is not defined — was the runtime bundle loaded?";
    JS_FreeValue(context, func);
    JS_FreeValue(context, global);
    return false;
  }

  JSValue currentBarValue = JS_NewFloat64(context, static_cast<double>(currentBar));
  JSValueConst argv[] = {currentBarValue};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else if (!JS_IsArray(resultValue)) {
    ok = false;
    errorOut = "__lineageTickAutoEvolution did not return an array";
  } else {
    JSValue lengthValue = JS_GetPropertyStr(context, resultValue, "length");
    int32_t length = 0;
    JS_ToInt32(context, &length, lengthValue);
    JS_FreeValue(context, lengthValue);

    std::vector<AutoEvolutionFiredEvent> events;
    events.reserve(static_cast<size_t>(length));
    for (int32_t i = 0; i < length; ++i) {
      JSValue item = JS_GetPropertyUint32(context, resultValue, static_cast<uint32_t>(i));
      AutoEvolutionFiredEvent event;

      auto readString = [this, item](const char* fieldName) {
        JSValue value = JS_GetPropertyStr(context, item, fieldName);
        const char* text = JS_ToCString(context, value);
        std::string result = text != nullptr ? text : "";
        if (text != nullptr) JS_FreeCString(context, text);
        JS_FreeValue(context, value);
        return result;
      };
      event.sectionId = readString("sectionId");
      event.sectionName = readString("sectionName");
      event.ruleId = readString("ruleId");
      event.operation = readString("operation");

      JS_FreeValue(context, item);
      events.push_back(std::move(event));
    }
    eventsOut = std::move(events);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, currentBarValue);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}
