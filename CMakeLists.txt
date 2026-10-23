cmake_minimum_required(VERSION 3.22)
project(PinkyLite2 VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(PINKY_BUILD_TESTS "Build the headless DSP/UI test tool" OFF)
set(PINKY_JUCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading JUCE 8.0.4")

if(PINKY_JUCE_DIR)
    add_subdirectory(${PINKY_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PINKY_SOURCES PluginProcessor.cpp Params.cpp Settings.cpp PluginEditor.cpp)

set(PINKY_JUCE_LIBS juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

set(PINKY_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

option(PINKY_MINIMAL_LINUX "Disable optional X11/ALSA features (dev containers only)" OFF)
if(PINKY_MINIMAL_LINUX)
    list(APPEND PINKY_DEFS JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

juce_add_plugin(PinkyLite2
    COMPANY_NAME "seoulbby"
    BUNDLE_ID com.seoulbby.pinkylite2
    PLUGIN_MANUFACTURER_CODE Seou
    PLUGIN_CODE Pnk2
    FORMATS VST3 Standalone
    PRODUCT_NAME "Pinky Lite 2"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PinkyLite2 PRIVATE ${PINKY_SOURCES})
target_include_directories(PinkyLite2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(PinkyLite2 PUBLIC ${PINKY_DEFS})
target_link_libraries(PinkyLite2 PRIVATE ${PINKY_JUCE_LIBS})

if(PINKY_BUILD_TESTS)
    juce_add_console_app(PinkyTests PRODUCT_NAME "PinkyTests" COMPANY_NAME "seoulbby")
    target_sources(PinkyTests PRIVATE Main.cpp ${PINKY_SOURCES})
    target_include_directories(PinkyTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(PinkyTests PUBLIC ${PINKY_DEFS} JucePlugin_Name="Pinky Lite 2" JucePlugin_VersionString="2.0.0")
    target_link_libraries(PinkyTests PRIVATE ${PINKY_JUCE_LIBS})
endif()
