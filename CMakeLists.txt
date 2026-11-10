cmake_minimum_required(VERSION 3.22)
project(WRAITH_WEAVE VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(WRAITH_WEAVE
    COMPANY_NAME "WRAITH Audio"
    BUNDLE_ID "audio.wraith.weave"
    PLUGIN_MANUFACTURER_CODE WrAu
    PLUGIN_CODE Wv01
    FORMATS VST3
    PRODUCT_NAME "WRAITH WEAVE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(WRAITH_WEAVE PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h)
target_compile_definitions(WRAITH_WEAVE PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(WRAITH_WEAVE PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Generate JuceHeader.h for PluginProcessor and PluginEditor
juce_generate_juce_header(WRAITH_WEAVE)

