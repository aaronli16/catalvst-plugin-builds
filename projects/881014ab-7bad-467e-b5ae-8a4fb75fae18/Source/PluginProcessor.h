#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// RiserXAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class RiserXAudioProcessor : public juce::AudioProcessor
{
public:
    RiserXAudioProcessor();
    ~RiserXAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RiserX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

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

    // DSP Members
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 88200 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> flangerDelayL { 4410 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> flangerDelayR { 4410 };

    // High-pass filter (one per channel)
    juce::dsp::StateVariableTPTFilter<float> hpFilter;

    // Pre-allocated dry buffer for mix blending
    juce::AudioBuffer<float> dryBuffer;

    // Flanger LFO phase
    float flangerPhase = 0.0f;

    // Delay feedback accumulators (per channel)
    float delayFeedbackL = 0.0f;
    float delayFeedbackR = 0.0f;

    // Current sample rate for real-time calculations
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RiserXAudioProcessor)
};
