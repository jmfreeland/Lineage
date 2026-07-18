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
                                      std::string& errorOut,
                                      const AutoEvolutionPreview* autoEvolution) {
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
  JSValue autoObj = JS_NewObject(context);
  JS_SetPropertyStr(context,
                    autoObj,
                    "running",
                    JS_NewBool(context, autoEvolution != nullptr && autoEvolution->running));
  JSValue ruleObj = JS_NewObject(context);
  if (autoEvolution != nullptr) {
    JS_SetPropertyStr(context, ruleObj, "id", JS_NewString(context, autoEvolution->rule.id.c_str()));
    JS_SetPropertyStr(context, ruleObj, "mutation", JS_NewFloat64(context, autoEvolution->rule.mutation));
    JS_SetPropertyStr(context, ruleObj, "embellish", JS_NewFloat64(context, autoEvolution->rule.embellish));
    JS_SetPropertyStr(context, ruleObj, "fill", JS_NewFloat64(context, autoEvolution->rule.fill));
    JS_SetPropertyStr(context, ruleObj, "hold", JS_NewFloat64(context, autoEvolution->rule.hold));
    JS_SetPropertyStr(context,
                      autoObj,
                      "nextEvolutionBar",
                      JS_NewFloat64(context, static_cast<double>(autoEvolution->nextEvolutionBar)));
    JS_SetPropertyStr(context, autoObj, "frequencyBars", JS_NewInt32(context, autoEvolution->frequencyBars));
  } else {
    JS_SetPropertyStr(context, ruleObj, "id", JS_NewString(context, ""));
    JS_SetPropertyStr(context, autoObj, "nextEvolutionBar", JS_NewInt32(context, 0));
    JS_SetPropertyStr(context, autoObj, "frequencyBars", JS_NewInt32(context, 4));
  }
  JS_SetPropertyStr(context, autoObj, "rule", ruleObj);
  JSValueConst argv[] = {startValue, beatsValue, barCountValue, paramsObj, autoObj};
  JSValue resultValue = JS_Call(context, func, global, 5, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else {
    ok = readPlaybackEvents(context, resultValue, "__lineageRenderPlaybackPreview", eventsOut, errorOut);
  }

  JS_FreeValue(context, resultValue);
  JS_FreeValue(context, autoObj);
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

  JSValue ruleObj = JS_NewObject(context);
  JS_SetPropertyStr(context, ruleObj, "id", JS_NewString(context, rule.id.c_str()));
  JS_SetPropertyStr(context, ruleObj, "mutation", JS_NewFloat64(context, rule.mutation));
  JS_SetPropertyStr(context, ruleObj, "embellish", JS_NewFloat64(context, rule.embellish));
  JS_SetPropertyStr(context, ruleObj, "fill", JS_NewFloat64(context, rule.fill));
  JS_SetPropertyStr(context, ruleObj, "hold", JS_NewFloat64(context, rule.hold));
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
