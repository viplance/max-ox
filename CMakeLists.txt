cmake_minimum_required(VERSION 3.20)
if (POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

file(READ "package.json" PACKAGE_JSON)
string(JSON PKG_VERSION GET "${PACKAGE_JSON}" "version")

project(MaxOx VERSION ${PKG_VERSION})

add_subdirectory(vendor/JUCE)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/MaxOx_artefacts/JuceLibraryCode/MaxOx_VST3")

juce_add_plugin(MaxOx
  COMPANY_NAME "SharkoAudio"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Shrk
  PLUGIN_CODE Mxox
  FORMATS VST3
  PRODUCT_NAME "MaxOx"
)

target_sources(MaxOx PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginConstants.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/DSP/LookAheadLimiter.cpp
  Source/DSP/LookAheadLimiter.h
  Source/DSP/PerceptualPeakShaver.cpp
  Source/DSP/PerceptualPeakShaver.h
  Source/DSP/TruePeakGuard.cpp
  Source/DSP/TruePeakGuard.h
  Source/DSP/AdaptiveLowCut.cpp
  Source/DSP/AdaptiveLowCut.h
  Source/UI/NeedleMeter.cpp
  Source/UI/NeedleMeter.h
  Source/UI/AnalogKnob.cpp
  Source/UI/AnalogKnob.h
)

juce_generate_juce_header(MaxOx)

target_compile_definitions(MaxOx PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  PLUGIN_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(MaxOx PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_plugin_client
  juce::juce_gui_extra
  juce::juce_gui_basics
  juce::juce_graphics
  juce::juce_events
  juce::juce_core
  juce::juce_data_structures
  juce::juce_cryptography
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_dsp
  "-framework Accelerate"
)

set_target_properties(MaxOx PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

if (APPLE)
  set_target_properties(MaxOx PROPERTIES OSX_ARCHITECTURES "arm64")
endif()

if (MSVC)
  target_compile_options(MaxOx PRIVATE /W4)
else()
  target_compile_options(MaxOx PRIVATE -Wall -Wextra -Wshadow -Wnon-virtual-dtor)
endif()

if (TARGET MaxOx_VST3)
  add_custom_command(TARGET MaxOx_VST3 POST_BUILD
    COMMAND codesign --force --deep --sign -
      "$<TARGET_FILE_DIR:MaxOx_VST3>/../.."
    COMMAND codesign --force --deep --sign -
      "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/MaxOx.vst3"
    COMMENT "Code-signing MaxOx VST3 bundle"
    VERBATIM
  )
endif()

enable_testing()

juce_add_console_app(MaxOxDSPTests
  PRODUCT_NAME "MaxOx DSP Tests"
)

target_sources(MaxOxDSPTests PRIVATE
  Tests/Main.cpp
  Tests/LimiterTests.cpp
  Tests/AdaptiveLowCutTests.cpp
  Tests/PeakShaverTests.cpp
  Tests/GainAnomalyTests.cpp
  Tests/TransparencyTests.cpp
  Source/DSP/LookAheadLimiter.cpp
  Source/DSP/LookAheadLimiter.h
  Source/DSP/PerceptualPeakShaver.cpp
  Source/DSP/PerceptualPeakShaver.h
  Source/DSP/TruePeakGuard.cpp
  Source/DSP/TruePeakGuard.h
  Source/DSP/AdaptiveLowCut.cpp
  Source/DSP/AdaptiveLowCut.h
)

target_include_directories(MaxOxDSPTests PRIVATE Source Tests)
juce_generate_juce_header(MaxOxDSPTests)

target_link_libraries(MaxOxDSPTests PRIVATE
  juce::juce_audio_basics
  juce::juce_dsp
)

set_target_properties(MaxOxDSPTests PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

if (MSVC)
  target_compile_options(MaxOxDSPTests PRIVATE /W4)
else()
  target_compile_options(MaxOxDSPTests PRIVATE -Wall -Wextra -Wshadow)
endif()

add_test(NAME MaxOxDSPTests COMMAND MaxOxDSPTests)
