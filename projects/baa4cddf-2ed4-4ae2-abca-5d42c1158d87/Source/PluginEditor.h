#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

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

    // 1. RELAYS FIRST
    std::unique_ptr<juce::WebSliderRelay> wetRelay;

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST
    std::unique_ptr<juce::WebSliderParameterAttachment> wetAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OneKnobReverbAudioProcessorEditor)
};
