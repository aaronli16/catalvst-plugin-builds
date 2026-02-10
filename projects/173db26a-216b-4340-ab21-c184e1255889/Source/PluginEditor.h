#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// OneKnobReverbAudioProcessorEditor - WebView-based Plugin Editor
// ==============================================================================
//
// CRITICAL: Member declaration order prevents release build crashes.
// Destruction order (reverse of declaration):
// 1. Attachments destroyed FIRST
// 2. WebView destroyed SECOND
// 3. Relays destroyed LAST
// ==============================================================================

class OneKnobReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit OneKnobReverbAudioProcessorEditor(OneKnobReverbAudioProcessor& p);
    ~OneKnobReverbAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    OneKnobReverbAudioProcessor& processorRef;

    // ==========================================================================
    // CRITICAL MEMBER DECLARATION ORDER - DO NOT REORDER!
    // Order: Relays -> WebView -> Attachments
    // ==========================================================================

    // 1. RELAYS FIRST
    std::unique_ptr<juce::WebSliderRelay> mixRelay;

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS LAST
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OneKnobReverbAudioProcessorEditor)
};
