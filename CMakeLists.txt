cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MONOLITH VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MONOLITH_JUCE_DIR "" CACHE PATH "Optional path to a local JUCE 8 source tree (leave empty to fetch JUCE 8.0.15)")

if(MONOLITH_JUCE_DIR AND EXISTS "${MONOLITH_JUCE_DIR}/CMakeLists.txt")
    message(STATUS "MONOLITH: Using local JUCE from ${MONOLITH_JUCE_DIR}")
    add_subdirectory("${MONOLITH_JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce-build" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    message(STATUS "MONOLITH: Fetching JUCE 8.0.15 via FetchContent...")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MONOLITH
    COMPANY_NAME                "Monolith Audio"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Mnlt
    PLUGIN_CODE                 Mnch
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "MONOLITH"
    BUNDLE_ID                   "com.monolithaudio.monolith"
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             "Instrument|Synth"
)

# ── V2 Layer A texture assets (pre-processed by Tools/v2_process_assets.py) ──
file(GLOB MONOLITH_V2_ASSET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/v2_samples/*.flac")
juce_add_binary_data(MonolithV2Assets
    HEADER_NAME V2Assets.h
    NAMESPACE   V2Assets
    SOURCES     ${MONOLITH_V2_ASSET_FILES}
)
set_target_properties(MonolithV2Assets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(MONOLITH_V2_SOURCES
    Source/V2/TextureLayer.cpp
    Source/V2/ControlPlane.cpp
    Source/V2/MidiLearn.cpp
    Source/Presets/PresetLibrary.cpp
)

set(MONOLITH_UI_SOURCES
    Source/PluginEditor.cpp
    Source/UI/MonolithLookAndFeel.cpp
    Source/UI/MeterComponents.cpp
    Source/UI/OutputAnalyser.cpp
    Source/UI/HeroVisualizer.cpp
    Source/UI/TopBar.cpp
    Source/UI/SectionPanels.cpp
    Source/UI/PresetBrowser.cpp
    Source/UI/MacroEditor.cpp
)

set(MONOLITH_DSP_AND_PRESET_SOURCES
    Source/DSP/PhonemeField.cpp
    Source/DSP/HarmonicLayers.cpp
    Source/DSP/EntropyStage.cpp
    Source/DSP/SwarmVoice.cpp
    Source/DSP/BrutalistReverb.cpp
    Source/DSP/SwarmEngine.cpp
    Source/V2/MpeExpression.cpp
    Source/Presets/FactoryPresets.cpp
)

target_sources(MONOLITH
    PRIVATE
        ${MONOLITH_DSP_AND_PRESET_SOURCES}
        Source/PluginProcessor.cpp
        ${MONOLITH_V2_SOURCES}
        ${MONOLITH_UI_SOURCES}
)

target_include_directories(MONOLITH
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(MONOLITH
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(MONOLITH
    PRIVATE
        MonolithV2Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(MONOLITH PRIVATE /W4)
else()
    target_compile_options(MONOLITH PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Headless Automated DSP, Sample-Rate & 30-Preset Gain-Staging Audit App ────
juce_add_console_app(MonolithDspAudit
    PRODUCT_NAME "MonolithDspAudit"
)

target_sources(MonolithDspAudit
    PRIVATE
        Tests/DspAuditTest.cpp
        ${MONOLITH_DSP_AND_PRESET_SOURCES}
)

target_include_directories(MonolithDspAudit
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(MonolithDspAudit
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(MonolithDspAudit
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(MonolithDspAudit PRIVATE /W4)
else()
    target_compile_options(MonolithDspAudit PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Phase 2: Locked 8-Gate Headless Flagship QA Harness ───────────────────────
juce_add_console_app(MonolithQA
    PRODUCT_NAME "MonolithQA"
)

target_sources(MonolithQA
    PRIVATE
        Tests/QA/MonolithQA.cpp
        Source/PluginProcessor.cpp
        ${MONOLITH_V2_SOURCES}
        ${MONOLITH_DSP_AND_PRESET_SOURCES}
)

target_include_directories(MonolithQA
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(MonolithQA
    PRIVATE
        MONOLITH_HEADLESS_QA=1
        "JucePlugin_Name=\"MONOLITH\""
        "MONOLITH_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(MonolithQA
    PRIVATE
        MonolithV2Assets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(MonolithQA PRIVATE /W4 /O2)
else()
    target_compile_options(MonolithQA PRIVATE -Wall -Wextra -Wpedantic $<$<CONFIG:Release>:-O3>)
endif()

# ── V2 Stage 4: headless UI snapshot runner (1x / 2x Retina PNGs for visual self-inspection) ──
juce_add_console_app(MonolithUISnapshot
    PRODUCT_NAME "MonolithUISnapshot"
)

target_sources(MonolithUISnapshot
    PRIVATE
        Tests/UI/UISnapshotRunner.cpp
        Source/PluginProcessor.cpp
        ${MONOLITH_V2_SOURCES}
        ${MONOLITH_UI_SOURCES}
        ${MONOLITH_DSP_AND_PRESET_SOURCES}
)

target_include_directories(MonolithUISnapshot
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(MonolithUISnapshot
    PRIVATE
        "JucePlugin_Name=\"MONOLITH\""
        "MONOLITH_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(MonolithUISnapshot
    PRIVATE
        MonolithV2Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(MonolithUISnapshot PRIVATE /W4)
else()
    target_compile_options(MonolithUISnapshot PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME MonolithQA COMMAND MonolithQA)
