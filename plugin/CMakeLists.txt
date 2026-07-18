cmake_minimum_required(VERSION 3.22)

project(Lineage VERSION 0.1.4)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.9
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Embedded JS runtime (DESIGN.md §11) — the engine (../src) runs inside
# this instead of being reimplemented in C++.
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_CLI_STATIC OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG v0.9.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(quickjs)
# The VST3 target links as a shared module, so its static dependencies
# (qjs is built as a static lib by default) must be position-independent.
set_target_properties(qjs PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# Bundles src/runtime.ts (+ everything it imports) into one self-contained
# script via esbuild, run at configure time so the file exists before
# juce_add_binary_data reads it below. Only re-runs on reconfigure, not on
# every build — a plain `cmake --build` after editing engine TypeScript
# won't pick up the change until the next `cmake -S .. -B .`. Fine for a
# personal project at this stage; revisit if that gets annoying.
find_program(NPM_EXECUTABLE npm REQUIRED)
execute_process(
  COMMAND ${NPM_EXECUTABLE} run build:runtime
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  RESULT_VARIABLE RUNTIME_BUNDLE_RESULT
)
if(NOT RUNTIME_BUNDLE_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to bundle the Lineage engine runtime (npm run build:runtime)")
endif()

juce_add_binary_data(LineageRuntimeAssets
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/generated/runtime.bundle.js
)
set_target_properties(LineageRuntimeAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# MIDI effect, not an audio effect or instrument (DESIGN.md §11) — no audio
# buses, MIDI in and out. VST3 only: the format Bitwig and Ableton Live
# share across every platform either of them runs on.
juce_add_plugin(Lineage
  COMPANY_NAME "Lineage"
  PLUGIN_MANUFACTURER_CODE Lnge
  PLUGIN_CODE Lin1
  FORMATS VST3
  PRODUCT_NAME "Lineage"
  IS_SYNTH FALSE
  IS_MIDI_EFFECT TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(Lineage PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/JsEngine.cpp
)

target_compile_definitions(Lineage PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Lineage
  PRIVATE
    juce::juce_audio_utils
    LineageRuntimeAssets
    qjs
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Standalone smoke test of JsEngine + the embedded bundle, independent of
# the full plugin/JUCE host machinery — lets the bridge be verified without
# needing a DAW to load a VST3 into.
add_executable(LineageBridgeTest
  Tests/BridgeTest.cpp
  Source/JsEngine.cpp
)
target_link_libraries(LineageBridgeTest PRIVATE qjs LineageRuntimeAssets)
