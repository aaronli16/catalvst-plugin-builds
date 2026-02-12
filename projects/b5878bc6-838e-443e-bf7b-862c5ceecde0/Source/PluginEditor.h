#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// AmbientSmearEffectAudioProcessorEditor - WebView-based Plugin Editor
// ==============================================================================

class AmbientSmearEffectAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AmbientSmearEffectAudioProcessorEditor(AmbientSmearEffectAudioProcessor& p);
    ~AmbientSmearEffectAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    AmbientSmearEffectAudioProcessor& processorRef;

    // ==========================================================================
    // CRITICAL MEMBER DECLARATION ORDER - DO NOT REORDER!
    // Order: Relays -> WebView -> Attachments
    // ==========================================================================

    // 1. RELAYS FIRST
    std::unique_ptr<juce::WebSliderRelay> intensityRelay;

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST
    std::unique_ptr<juce::WebSliderParameterAttachment> intensityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientSmearEffectAudioProcessorEditor)
};
