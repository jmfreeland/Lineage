cmake_minimum_required(VERSION 3.22)

project(Lineage VERSION 0.1.0)

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
)

target_compile_definitions(Lineage PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Lineage
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
