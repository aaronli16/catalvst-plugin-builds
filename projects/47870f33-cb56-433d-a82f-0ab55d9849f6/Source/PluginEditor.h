#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// SimpleHalfbeatDelayAudioProcessorEditor - WebView-based Plugin Editor
// ==============================================================================
//
// CRITICAL: Member declaration order prevents release build crashes.
// Destruction order (reverse): Attachments -> WebView -> Relays
// ==============================================================================

class SimpleHalfbeatDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SimpleHalfbeatDelayAudioProcessorEditor(SimpleHalfbeatDelayAudioProcessor& p);
    ~SimpleHalfbeatDelayAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    SimpleHalfbeatDelayAudioProcessor& processorRef;

    // ==========================================================================
    // CRITICAL ORDER: Relays -> WebView -> Attachments
    // ==========================================================================

    // 1. RELAYS (created first, destroyed last)
    std::unique_ptr<juce::WebSliderRelay> mixRelay;
    std::unique_ptr<juce::WebSliderRelay> feedbackRelay;

    // 2. WEBVIEW (created after relays, destroyed before relays)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS (created last, destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> feedbackAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleHalfbeatDelayAudioProcessorEditor)
};
