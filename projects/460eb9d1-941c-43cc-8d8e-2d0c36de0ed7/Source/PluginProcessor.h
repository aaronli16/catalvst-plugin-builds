#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// OneKnobDelayAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class OneKnobDelayAudioProcessor : public juce::AudioProcessor
{
public:
    OneKnobDelayAudioProcessor();
    ~OneKnobDelayAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OneKnobDelay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

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
    // DSP Members - Ping-Pong Delay with Tape Saturation
    // ==========================================================================

    // Stereo delay lines (max 2 seconds at 192kHz)
    static constexpr int maxDelaySamples = 384000;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLineL { maxDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLineR { maxDelaySamples };

    // Low-pass filters for tape-style warmth on the feedback path
    juce::dsp::IIR::Filter<float> feedbackFilterL;
    juce::dsp::IIR::Filter<float> feedbackFilterR;

    // Smoothed parameters to prevent zipper noise
    juce::SmoothedValue<float> smoothedDelayTime;
    juce::SmoothedValue<float> smoothedFeedback;
    juce::SmoothedValue<float> smoothedMix;

    // Feedback buffers for the ping-pong routing
    float feedbackSampleL = 0.0f;
    float feedbackSampleR = 0.0f;

    // Current sample rate for delay time calculations
    double currentSampleRate = 44100.0;

    // ==========================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OneKnobDelayAudioProcessor)
};
