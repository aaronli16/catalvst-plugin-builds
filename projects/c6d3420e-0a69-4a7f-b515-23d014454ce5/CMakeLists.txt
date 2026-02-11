cmake_minimum_required(VERSION 3.15)

project(SimpleHalfbeatEchoBuild VERSION 1.0.0)

# ==============================================================================
# SimpleHalfbeatEcho - JUCE Plugin Build Configuration
# ==============================================================================

if(DEFINED JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} juce_build EXCLUDE_FROM_ALL)
endif()

juce_add_plugin(SimpleHalfbeatEcho
    COMPANY_NAME "Catalvst"
    PLUGIN_MANUFACTURER_CODE Ctlv
    PLUGIN_CODE Simp
    FORMATS VST3
    PRODUCT_NAME "SimpleHalfbeatEcho"
    NEEDS_WEB_BROWSER TRUE
)

# ==============================================================================
# WebView UI Resources
# ==============================================================================
juce_add_binary_data(SimpleHalfbeatEcho_UIResources
    SOURCES
        Source/ui/index.html
        Source/ui/js/juce/index.js
        Source/ui/js/juce/check_native_interop.js
)

# ==============================================================================
# Source Files
# ==============================================================================
target_sources(SimpleHalfbeatEcho
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(SimpleHalfbeatEcho
    PRIVATE
        Source
)

# ==============================================================================
# Required JUCE Modules
# ==============================================================================
target_link_libraries(SimpleHalfbeatEcho
    PRIVATE
        SimpleHalfbeatEcho_UIResources
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

# Generate JuceHeader.h (must come AFTER target_link_libraries)
juce_generate_juce_header(SimpleHalfbeatEcho)

# ==============================================================================
# Compile Definitions
# ==============================================================================
target_compile_definitions(SimpleHalfbeatEcho
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
