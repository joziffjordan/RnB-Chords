cmake_minimum_required(VERSION 3.22)
project(RnBChords VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

# Use a local JUCE folder if you pass -DJUCE_PATH=..., otherwise download JUCE.
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(RNB_FORMATS VST3 AU Standalone)
else()
    set(RNB_FORMATS VST3 Standalone)
endif()

juce_add_plugin(RnBChords
    COMPANY_NAME "Dev Griffin"
    PRODUCT_NAME "RnB Chords"
    PLUGIN_MANUFACTURER_CODE Dvgr
    PLUGIN_CODE Rnbc
    FORMATS ${RNB_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

juce_generate_juce_header(RnBChords)

juce_add_binary_data(RnBChordsData SOURCES Resources/rnb_chords_db.json)

target_sources(RnBChords PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(RnBChords PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RnBChords
    PRIVATE
        RnBChordsData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
