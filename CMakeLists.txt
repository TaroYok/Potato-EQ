cmake_minimum_required(VERSION 3.22)
project(PotatoEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")

# JUCE: uses a local checkout if JUCE_DIR is given, otherwise downloads the pinned version.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
if (JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce   # JUCE 9.0.5
        URL https://github.com/juce-framework/JUCE/archive/d60b7a78030661574a615b586d9325ba7a7fa7dd.tar.gz)
    FetchContent_MakeAvailable(juce)
endif()

option(POTATO_HEADLESS_LINUX "Build with minimal Linux system deps (CI/containers)" OFF)

set(POTATO_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND POTATO_FORMATS AU)
endif()

juce_add_plugin(PotatoEQ
    COMPANY_NAME "Taroy Audio"
    BUNDLE_ID "com.taroyaudio.potatoeq"
    PLUGIN_MANUFACTURER_CODE Tary
    PLUGIN_CODE Pot1
    FORMATS ${POTATO_FORMATS}
    PRODUCT_NAME "Potato EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect)

# Core DSP is plain C++ (no GUI), shared by the plugin and the test runner.
add_library(potato_dsp STATIC
    Source/dsp/Loudness.cpp
    Source/dsp/AutoEq.cpp)
target_include_directories(potato_dsp PUBLIC Source)
target_compile_features(potato_dsp PUBLIC cxx_std_20)
set_target_properties(potato_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(PotatoEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/EqDisplay.cpp
    Source/ui/Panels.cpp)

target_compile_definitions(PotatoEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if (POTATO_HEADLESS_LINUX)
    target_compile_definitions(PotatoEQ PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XINPUT=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(PotatoEQ
    PRIVATE
        potato_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP tests (no JUCE needed)
add_executable(potato_tests tests/DspTests.cpp)
target_link_libraries(potato_tests PRIVATE potato_dsp)
enable_testing()
add_test(NAME dsp COMMAND potato_tests)

# Full-plugin harness (processor + editor screenshot)
option(POTATO_BUILD_HARNESS "Build the end-to-end plugin harness" OFF)
if (POTATO_BUILD_HARNESS)
    juce_add_console_app(potato_harness PRODUCT_NAME "PotatoHarness")
    target_sources(potato_harness PRIVATE
        tests/PluginHarness.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/ui/EqDisplay.cpp Source/ui/Panels.cpp)
    target_include_directories(potato_harness PRIVATE Source)
    target_compile_definitions(potato_harness PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XINPUT=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
    target_link_libraries(potato_harness PRIVATE potato_dsp juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
