#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ==============================================================================
// CathedralAudioProcessorEditor - WebView-based Plugin Editor
// ==============================================================================

class CathedralAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit CathedralAudioProcessorEditor(CathedralAudioProcessor& p);
    ~CathedralAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    CathedralAudioProcessor& processorRef;

    // ==========================================================================
    // CRITICAL MEMBER DECLARATION ORDER - DO NOT REORDER!
    // Order: Relays -> WebView -> Attachments
    // ==========================================================================

    // 1. RELAYS FIRST
    std::unique_ptr<juce::WebSliderRelay> mixRelay;

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CathedralAudioProcessorEditor)
};
