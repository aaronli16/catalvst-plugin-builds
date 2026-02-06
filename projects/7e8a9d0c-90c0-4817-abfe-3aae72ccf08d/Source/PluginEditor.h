#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// MinimalBeatEchoPluginAudioProcessorEditor - WebView-based Plugin Editor
// ==============================================================================
//
// CRITICAL: Member declaration order prevents 90% of release build crashes.
//
// Destruction order (reverse of declaration):
// 1. Attachments destroyed FIRST (stop using relays and WebView)
// 2. WebView destroyed SECOND (safe, attachments are gone)
// 3. Relays destroyed LAST (safe, nothing using them)
//
// ==============================================================================

class MinimalBeatEchoPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MinimalBeatEchoPluginAudioProcessorEditor(MinimalBeatEchoPluginAudioProcessor& p);
    ~MinimalBeatEchoPluginAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // --------------------------------------------------------------------------
    // Resource Provider - Maps URLs to embedded binary data
    // --------------------------------------------------------------------------
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    // Reference to audio processor
    MinimalBeatEchoPluginAudioProcessor& processorRef;

    // ==========================================================================
    // CRITICAL MEMBER DECLARATION ORDER - DO NOT REORDER!
    //
    // Order: Relays -> WebView -> Attachments
    //
    // Members are destroyed in REVERSE order of declaration.
    // ==========================================================================

    // --------------------------------------------------------------------------
    // 1. RELAYS FIRST (created first, destroyed last)
    // --------------------------------------------------------------------------
    std::unique_ptr<juce::WebSliderRelay> timeRelay;

    // --------------------------------------------------------------------------
    // 2. WEBVIEW SECOND (created after relays, destroyed before relays)
    // --------------------------------------------------------------------------
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // --------------------------------------------------------------------------
    // 3. PARAMETER ATTACHMENTS LAST (created last, destroyed first)
    // --------------------------------------------------------------------------
    std::unique_ptr<juce::WebSliderParameterAttachment> timeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MinimalBeatEchoPluginAudioProcessorEditor)
};
