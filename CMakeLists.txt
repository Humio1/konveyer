cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(KONVEYER VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(KV_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(KV_FORMATS VST3 Standalone)
else()
    set(KV_FORMATS VST3)
endif()

juce_add_plugin(Konveyer
    COMPANY_NAME "KONVEYER"
    BUNDLE_ID "com.humio1.konveyer"
    PLUGIN_MANUFACTURER_CODE Hmio
    PLUGIN_CODE Knvy
    FORMATS ${KV_FORMATS}
    PRODUCT_NAME "KONVEYER"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(KonveyerData SOURCES
    Resources/fonts/RussoOne-Regular.ttf
    Resources/fonts/Oswald-SemiBold.ttf
    Resources/fonts/Oswald-Regular.ttf
    Resources/fonts/Oswald-Light.ttf
    Resources/fonts/PTMono-Regular.ttf
    Resources/fonts/BigShouldersStencil-ExtraBold.ttf)

set(KV_ENGINE_SOURCES
    Source/Engine/Filters.cpp
    Source/Engine/Equalizer.cpp
    Source/Engine/Dynamics.cpp
    Source/Engine/Imager.cpp
    Source/Engine/Limiter.cpp
    Source/Engine/Engine.cpp)

target_sources(Konveyer PRIVATE
    ${KV_ENGINE_SOURCES}
    Source/Params.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Style.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/Crt.cpp
    Source/UI/PanelEq.cpp
    Source/UI/PanelDyn.cpp
    Source/UI/PanelStereoLim.cpp
    Source/UI/PanelConveyor.cpp)

target_include_directories(Konveyer PRIVATE Source)

target_compile_definitions(Konveyer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(UNIX AND NOT APPLE)
    # Lets the plugin compile on a bare Linux machine for quick checks.
    target_compile_definitions(Konveyer PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

set(KV_JUCE_MODULES juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra)
if(APPLE OR WIN32)
    list(APPEND KV_JUCE_MODULES juce::juce_audio_utils)
endif()

target_link_libraries(Konveyer
    PRIVATE
        KonveyerData
        ${KV_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline engine test (no JUCE needed)
add_executable(EngineTest tests/EngineTest.cpp ${KV_ENGINE_SOURCES})
target_include_directories(EngineTest PRIVATE Source)

# Developer tool: render the editor to PNGs without a host (off by default)
option(KV_DEV_TOOLS "Build developer tools" OFF)
if(KV_DEV_TOOLS)
    add_executable(EditorSnapshot tests/EditorSnapshot.cpp)
    target_link_libraries(EditorSnapshot PRIVATE Konveyer)
    target_include_directories(EditorSnapshot PRIVATE Source ${JUCE_SOURCE_DIR}/modules
        ${CMAKE_BINARY_DIR}/Konveyer_artefacts/JuceLibraryCode)
    get_target_property(KV_DEFS Konveyer COMPILE_DEFINITIONS)
    target_compile_definitions(EditorSnapshot PRIVATE ${KV_DEFS})

    add_executable(PluginTest tests/PluginTest.cpp)
    target_link_libraries(PluginTest PRIVATE Konveyer)
    target_include_directories(PluginTest PRIVATE Source ${JUCE_SOURCE_DIR}/modules
        ${CMAKE_BINARY_DIR}/Konveyer_artefacts/JuceLibraryCode)
    target_compile_definitions(PluginTest PRIVATE ${KV_DEFS})
endif()
