cmake_minimum_required(VERSION 3.15)

project(OneKnobReverbBuild VERSION 1.0.0)

if(DEFINED JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} juce_build EXCLUDE_FROM_ALL)
endif()

juce_add_plugin(OneKnobReverb
    COMPANY_NAME "Catalvst"
    PLUGIN_MANUFACTURER_CODE Ctlv
    PLUGIN_CODE Onek
    FORMATS VST3
    PRODUCT_NAME "OneKnobReverb"
    NEEDS_WEB_BROWSER TRUE
)

juce_add_binary_data(OneKnobReverb_UIResources
    SOURCES
        Source/ui/index.html
        Source/ui/js/juce/index.js
        Source/ui/js/juce/check_native_interop.js
)

target_sources(OneKnobReverb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(OneKnobReverb
    PRIVATE
        Source
)

target_link_libraries(OneKnobReverb
    PRIVATE
        OneKnobReverb_UIResources
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

juce_generate_juce_header(OneKnobReverb)

target_compile_definitions(OneKnobReverb
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
