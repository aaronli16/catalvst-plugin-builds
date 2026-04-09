#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Generic WebView-based plugin editor scaffold.
// Serves HTML/CSS/JS from BinaryData (embedded at compile time).
// Dynamically creates WebSliderRelay per Faust parameter.
// Injects a JS shim that wires [data-param] elements to JUCE parameter relays.

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void resized() override;

private:
    using Resource = juce::WebBrowserComponent::Resource;

    std::optional<Resource> getResource (const juce::String& url) const;

    PluginProcessor& processorRef;

    // WebSliderRelays — one per Faust parameter.
    // Must be created BEFORE webBrowser (non-copyable, non-moveable).
    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliderRelays;

    // The WebBrowserComponent — constructed with options from all relays
    std::unique_ptr<juce::WebBrowserComponent> webBrowser;

    // Attachments — bidirectional sync between JUCE AudioParameter and WebSliderRelay
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
