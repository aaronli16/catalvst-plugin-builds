#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "FaustDSP.h"

// Generic Faust→JUCE processor scaffold.
// Works with ANY Faust-generated FaustDSP.h — dynamically reads parameters
// via APIUI at construction time and creates matching JUCE AudioParameterFloats.

class PluginProcessor : public juce::AudioProcessor,
                        public juce::AudioProcessorValueTreeState::Listener
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Called when a JUCE parameter changes — forwards value to Faust via MapUI
    void parameterChanged (const juce::String& parameterId, float newValue) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Faust parameter labels (needed by PluginEditor for WebSliderRelay names)
    const juce::StringArray& getParamLabels() const { return paramLabels; }

private:
    std::unique_ptr<FaustDSP> fDSP;
    std::unique_ptr<MapUI> fMapUI;

    // Cached param info — MUST be declared before apvts!
    // createParameterLayout() populates these during apvts construction.
    // If declared after apvts, their default constructors run AFTER and wipe the data.
    juce::StringArray paramLabels;    // Faust labels (display names, used as relay names)
    juce::StringArray paramAddresses; // Faust addresses (e.g., "/Dattorro/Decay_Rate")
    juce::StringArray paramIds;       // Sanitized IDs for APVTS (e.g., "Decay_Rate")

    juce::AudioProcessorValueTreeState apvts;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
