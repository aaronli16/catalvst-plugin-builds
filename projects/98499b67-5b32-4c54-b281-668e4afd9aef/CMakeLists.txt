cmake_minimum_required(VERSION 3.15)

project(SmoothWaveFlanger VERSION 1.0.0)

# ==============================================================================
# SmoothWaveFlanger - JUCE Plugin Build Configuration
# ==============================================================================

if(DEFINED JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} juce_build EXCLUDE_FROM_ALL)
endif()

juce_add_plugin(SmoothWaveFlanger
    COMPANY_NAME "Catalvst"
    PLUGIN_MANUFACTURER_CODE Ctlv
    PLUGIN_CODE XMYB
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "SmoothWaveFlanger"
    NEEDS_WEB_BROWSER TRUE
)

# ==============================================================================
# WebView UI Resources - Embed HTML/CSS/JS into plugin binary
# ==============================================================================
juce_add_binary_data(SmoothWaveFlanger_UIResources
    SOURCES
        ui/index.html
        ui/js/juce/index.js
        ui/js/juce/check_native_interop.js
)

# ==============================================================================
# Source Files
# ==============================================================================
target_sources(SmoothWaveFlanger
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Include paths
target_include_directories(SmoothWaveFlanger
    PRIVATE
        Source
)

# ==============================================================================
# Required JUCE Modules
# ==============================================================================
target_link_libraries(SmoothWaveFlanger
    PRIVATE
        SmoothWaveFlanger_UIResources
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_generate_juce_header(SmoothWaveFlanger)

# ==============================================================================
# Compile Definitions
# ==============================================================================
target_compile_definitions(SmoothWaveFlanger
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
