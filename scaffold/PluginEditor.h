#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void resized() override;

private:
    PluginProcessor& processorRef;

    // WebSliderRelays — one per Faust parameter. Must be created BEFORE webBrowser.
    // Non-copyable, non-moveable — stored as unique_ptrs.
    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliderRelays;

    // Attachments that bidirectionally sync JUCE AudioParameter ↔ WebSliderRelay
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;

    // WebView — constructed with options from all relays
    std::unique_ptr<juce::WebBrowserComponent> webBrowser;

    // Resource provider: serves BinaryData files to the WebView
    static std::optional<juce::WebBrowserComponent::Resource>
        resourceProvider (const juce::String& url);

    // MIME type lookup
    static juce::String getMimeType (const juce::String& path);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
