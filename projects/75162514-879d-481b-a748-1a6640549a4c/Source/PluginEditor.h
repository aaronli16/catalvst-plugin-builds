#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// UntitledPluginAudioProcessorEditor - WebView-based Plugin Editor
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

class UntitledPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UntitledPluginAudioProcessorEditor(UntitledPluginAudioProcessor& p);
    ~UntitledPluginAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // --------------------------------------------------------------------------
    // Resource Provider - Maps URLs to embedded binary data
    // --------------------------------------------------------------------------
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    // Reference to audio processor
    UntitledPluginAudioProcessor& processorRef;

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
    std::unique_ptr<juce::WebSliderRelay> mixRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbDecayRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbDegradeRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbShimmerRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbToneRelay;
    std::unique_ptr<juce::WebSliderRelay> delayTimeRelay;
    std::unique_ptr<juce::WebSliderRelay> delayFeedbackRelay;
    std::unique_ptr<juce::WebSliderRelay> delayWarmthRelay;
    std::unique_ptr<juce::WebSliderRelay> delayWowRelay;

    // --------------------------------------------------------------------------
    // 2. WEBVIEW SECOND (created after relays, destroyed before relays)
    // --------------------------------------------------------------------------
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // --------------------------------------------------------------------------
    // 3. PARAMETER ATTACHMENTS LAST (created last, destroyed first)
    // --------------------------------------------------------------------------
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbDecayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbDegradeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbShimmerAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbToneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayTimeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayWarmthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayWowAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UntitledPluginAudioProcessorEditor)
};
