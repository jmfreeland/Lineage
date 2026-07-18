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

bool JsEngine::processBlock(std::vector<MidiEvent>& events, std::string& errorOut) {
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
    JS_SetPropertyUint32(context, inputArray, static_cast<uint32_t>(i), obj);
  }

  JSValueConst argv[] = {inputArray};
  JSValue resultValue = JS_Call(context, func, global, 1, argv);

  bool ok = !JS_IsException(resultValue);
  if (!ok) {
    errorOut = describeException(context);
  } else if (!JS_IsArray(resultValue)) {
    ok = false;
    errorOut = "__lineageProcessBlock did not return an array";
  } else {
    std::vector<MidiEvent> outEvents;
    outEvents.reserve(events.size());
    for (size_t i = 0; i < events.size(); ++i) {
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
  JS_FreeValue(context, inputArray);
  JS_FreeValue(context, func);
  JS_FreeValue(context, global);
  return ok;
}
