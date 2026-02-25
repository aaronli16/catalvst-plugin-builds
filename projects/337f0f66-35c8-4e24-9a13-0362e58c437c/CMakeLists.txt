cmake_minimum_required(VERSION 3.15)

project(WashoutSmileBuild VERSION 1.0.0)

# ==============================================================================
# WashoutSmile - JUCE Plugin Build Configuration
# ==============================================================================
# This file is auto-generated. Claude will update the SOURCE_FILES section.
# ==============================================================================

# JUCE_SOURCE_DIR is passed via -DJUCE_SOURCE_DIR=/path/to/JUCE when building
# via GitHub Actions. For local development, set this to your JUCE installation.
if(DEFINED JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} juce_build EXCLUDE_FROM_ALL)
endif()

juce_add_plugin(WashoutSmile
    COMPANY_NAME "Catalvst"
    PLUGIN_MANUFACTURER_CODE Ctlv
    PLUGIN_CODE M06T
    FORMATS VST3
    PRODUCT_NAME "WashoutSmile"
    NEEDS_WEB_BROWSER TRUE
)

# ==============================================================================
# WebView UI Resources - Embed HTML/CSS/JS into plugin binary
# ==============================================================================
juce_add_binary_data(WashoutSmile_UIResources
    SOURCES
        Source/ui/index.html
        Source/ui/js/juce/index.js
        Source/ui/js/juce/check_native_interop.js
)

# ==============================================================================
# Source Files
# ==============================================================================
target_sources(WashoutSmile
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Include paths
target_include_directories(WashoutSmile
    PRIVATE
        Source
)

# ==============================================================================
# Required JUCE Modules
# ==============================================================================
target_link_libraries(WashoutSmile
    PRIVATE
        WashoutSmile_UIResources
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

juce_generate_juce_header(WashoutSmile)

# ==============================================================================
# Compile Definitions
# ==============================================================================
target_compile_definitions(WashoutSmile
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
