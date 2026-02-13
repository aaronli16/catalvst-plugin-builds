#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// AmbientBuildupOneknobProcessorAudioProcessorEditor - WebView-based Plugin Editor
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

class AmbientBuildupOneknobProcessorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AmbientBuildupOneknobProcessorAudioProcessorEditor(AmbientBuildupOneknobProcessorAudioProcessor& p);
    ~AmbientBuildupOneknobProcessorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    AmbientBuildupOneknobProcessorAudioProcessor& processorRef;

    // ==========================================================================
    // CRITICAL MEMBER DECLARATION ORDER - DO NOT REORDER!
    // Order: Relays -> WebView -> Attachments
    // ==========================================================================

    // 1. RELAYS FIRST
    std::unique_ptr<juce::WebSliderRelay> buildupRelay;

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS LAST
    std::unique_ptr<juce::WebSliderParameterAttachment> buildupAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientBuildupOneknobProcessorAudioProcessorEditor)
};
