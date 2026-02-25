#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// UntitledPluginAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class UntitledPluginAudioProcessor : public juce::AudioProcessor
{
public:
    UntitledPluginAudioProcessor();
    ~UntitledPluginAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UntitledPlugin"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15.0; }

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

    // Reverb DSP
    juce::dsp::Reverb reverb;

    // Delay line (max 2 seconds at 192kHz)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLineL { 384000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLineR { 384000 };

    // Lo-fi degradation filter (lowpass to roll off highs for lo-fi character)
    juce::dsp::IIR::Filter<float> degradeFilterL;
    juce::dsp::IIR::Filter<float> degradeFilterR;

    // Delay warmth filter (lowpass in feedback path)
    juce::dsp::IIR::Filter<float> warmthFilterL;
    juce::dsp::IIR::Filter<float> warmthFilterR;

    // Delay tone filter (lowpass on reverb output)
    juce::dsp::IIR::Filter<float> toneFilterL;
    juce::dsp::IIR::Filter<float> toneFilterR;

    // Pre-allocated dry buffer (no allocation in processBlock)
    juce::AudioBuffer<float> dryBuffer;

    // Delay feedback state
    float delayFeedbackStateL = 0.0f;
    float delayFeedbackStateR = 0.0f;

    // Tape wow LFO phase
    float wowPhase = 0.0f;

    // Current sample rate
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UntitledPluginAudioProcessor)
};
