#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// SimpleReverbControlAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class SimpleReverbControlAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleReverbControlAudioProcessor();
    ~SimpleReverbControlAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SimpleReverbControl"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --------------------------------------------------------------------------
    // Parameter Access (for Editor)
    // --------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState parameters;

private:
    // --------------------------------------------------------------------------
    // Parameter Layout - Defines all plugin parameters
    // --------------------------------------------------------------------------
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ==========================================================================
    // DSP Member Variables
    // ==========================================================================
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> reverbBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleReverbControlAudioProcessor)
};
