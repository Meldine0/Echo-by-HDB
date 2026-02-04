cmake_minimum_required(VERSION 3.22)

project(EchoByHDB VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_PATH "" CACHE PATH "Path to JUCE")
if(NOT JUCE_PATH)
  message(FATAL_ERROR "Set JUCE_PATH to the JUCE directory (e.g. -DJUCE_PATH=C:/JUCE)")
endif()

add_subdirectory(${JUCE_PATH} JUCE)

juce_add_plugin(EchoByHDB
  COMPANY_NAME "HDB"
  BUNDLE_ID "com.hdb.echo-by-hdb"
  PLUGIN_MANUFACTURER_CODE "HDB"
  PLUGIN_CODE "HDBE"
  PRODUCT_NAME "Echo by HDB"
  VERSION "1.0.0"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  FORMATS VST3
)

juce_generate_juce_header(EchoByHDB)

target_sources(EchoByHDB
  PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.h
)

target_compile_definitions(EchoByHDB
  PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(EchoByHDB
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
