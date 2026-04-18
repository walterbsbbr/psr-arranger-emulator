cmake_minimum_required(VERSION 3.24)
project(PSREmulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ─── JUCE 8 ───────────────────────────────────────────────────────────────────
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─── FluidSynth (homebrew / system) ──────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth)

# ─── Application target ───────────────────────────────────────────────────────
juce_add_gui_app(PSREmulator
    PRODUCT_NAME        "PSR Arranger Emulator"
    VERSION             "0.1.0"
    COMPANY_NAME        "walterbsbbr"
    BUNDLE_ID           "com.walterbsbbr.psremu"
    MICROPHONE_PERMISSION_ENABLED FALSE
)

juce_generate_juce_header(PSREmulator)

target_sources(PSREmulator PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp

    Source/SoundFont/FluidSynthEngine.cpp
    Source/SoundFont/SoundFontManager.cpp

    Source/Engine/ChordDetector.cpp
    Source/Engine/MidiRouter.cpp
    Source/Engine/TransposeEngine.cpp
    Source/Engine/LoopPlayer.cpp
    Source/Engine/StyleEngine.cpp

    Source/STY/StyParser.cpp
    Source/STY/CasmParser.cpp

    Source/UI/StylePanel.cpp
    Source/UI/TransportPanel.cpp
    Source/UI/MixerPanel.cpp
    Source/UI/ChordDisplay.cpp
    Source/UI/SetupDialog.cpp
)

target_include_directories(PSREmulator PRIVATE
    Source/
    ${FLUIDSYNTH_INCLUDE_DIRS}
)

target_compile_definitions(PSREmulator PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PSREmulator,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PSREmulator,JUCE_VERSION>"
)

target_link_libraries(PSREmulator PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    ${FLUIDSYNTH_LIBRARIES}
)

target_link_directories(PSREmulator PRIVATE ${FLUIDSYNTH_LIBRARY_DIRS})
target_compile_options(PSREmulator PRIVATE ${FLUIDSYNTH_CFLAGS_OTHER})
