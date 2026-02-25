#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// PendulumAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class PendulumAudioProcessor : public juce::AudioProcessor
{
public:
    PendulumAudioProcessor();
    ~PendulumAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pendulum"; }
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

    // DSP members
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 44100 * 4 };
    juce::dsp::DryWetMixer<float> dryWetMixer;

    // One-pole low-pass filters for damping in the feedback path (one per channel)
    float dampState[2] = { 0.0f, 0.0f };

    // Pre-allocated feedback buffer (stores one sample per channel for the feedback loop)
    float feedbackSample[2] = { 0.0f, 0.0f };

    // Smoothed values for real-time parameter changes
    juce::SmoothedValue<float> delayTimeSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> dampingSmoothed;

    // Cached sample rate for BPM calculations
    double currentSampleRate = 44100.0;

    // Division multipliers (beats) corresponding to choice index 0..6
    // 1/16, 1/8d, 1/8, 1/4d, 1/4, 1/2, 1/1
    static constexpr float divisionBeats[7] = { 0.25f, 0.75f, 0.5f, 1.5f, 1.0f, 2.0f, 4.0f };

    // Helper to compute delay time in samples from BPM and division
    float computeDelaySamples(double bpm, int divisionIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PendulumAudioProcessor)
};
