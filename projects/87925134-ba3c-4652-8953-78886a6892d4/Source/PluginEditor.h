#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// LowPassSingleKnobFilterAudioProcessorEditor - WebView-based Plugin Editor
// ==============================================================================
//
// CRITICAL: Member declaration order prevents release build crashes.
// Destruction order (reverse of declaration):
// 1. Attachments destroyed FIRST
// 2. WebView destroyed SECOND
// 3. Relays destroyed LAST
// ==============================================================================

class LowPassSingleKnobFilterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LowPassSingleKnobFilterAudioProcessorEditor(LowPassSingleKnobFilterAudioProcessor& p);
    ~LowPassSingleKnobFilterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    LowPassSingleKnobFilterAudioProcessor& processorRef;

    // ==========================================================================
    // CRITICAL MEMBER DECLARATION ORDER - DO NOT REORDER!
    // Order: Relays -> WebView -> Attachments
    // ==========================================================================

    // 1. RELAYS FIRST (created first, destroyed last)
    std::unique_ptr<juce::WebSliderRelay> cutoffRelay;

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (created last, destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> cutoffAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LowPassSingleKnobFilterAudioProcessorEditor)
};
