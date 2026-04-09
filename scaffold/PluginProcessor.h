#pragma once

#include <JuceHeader.h>
#include "FaustDSP.h"

class PluginProcessor : public juce::AudioProcessor
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

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    std::unique_ptr<FaustDSP> fDSP;
    std::unique_ptr<MapUI> fMapUI;
    std::unique_ptr<APIUI> fParamUI;

    juce::AudioProcessorValueTreeState apvts;

    // Cached param addresses for processBlock (avoids string lookups per block)
    struct ParamMapping {
        juce::String faustAddress;
        std::atomic<float>* juceParam;
    };
    std::vector<ParamMapping> paramMappings;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (FaustDSP& dsp);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
