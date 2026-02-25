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
    // Pitch relays
    std::unique_ptr<juce::WebSliderRelay> pitchSpeedRelay;
    std::unique_ptr<juce::WebSliderRelay> pitchAmountRelay;
    std::unique_ptr<juce::WebSliderRelay> pitchGlideRelay;
    std::unique_ptr<juce::WebSliderRelay> pitchEnabledRelay;

    // Reverb relays
    std::unique_ptr<juce::WebSliderRelay> reverbSizeRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbDecayRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbDampRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbWidthRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbEnabledRelay;

    // Delay relays
    std::unique_ptr<juce::WebSliderRelay> delayTimeRelay;
    std::unique_ptr<juce::WebSliderRelay> delayFeedbackRelay;
    std::unique_ptr<juce::WebSliderRelay> delayFilterRelay;
    std::unique_ptr<juce::WebSliderRelay> delayEnabledRelay;

    // Mix relays
    std::unique_ptr<juce::WebSliderRelay> dryWetRelay;
    std::unique_ptr<juce::WebSliderRelay> outputRelay;

    // --------------------------------------------------------------------------
    // 2. WEBVIEW SECOND (created after relays, destroyed before relays)
    // --------------------------------------------------------------------------
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // --------------------------------------------------------------------------
    // 3. PARAMETER ATTACHMENTS LAST (created last, destroyed first)
    // --------------------------------------------------------------------------
    // Pitch attachments
    std::unique_ptr<juce::WebSliderParameterAttachment> pitchSpeedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> pitchAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> pitchGlideAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> pitchEnabledAttachment;

    // Reverb attachments
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbSizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbDecayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbDampAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbWidthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbEnabledAttachment;

    // Delay attachments
    std::unique_ptr<juce::WebSliderParameterAttachment> delayTimeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayFilterAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayEnabledAttachment;

    // Mix attachments
    std::unique_ptr<juce::WebSliderParameterAttachment> dryWetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UntitledPluginAudioProcessorEditor)
};
