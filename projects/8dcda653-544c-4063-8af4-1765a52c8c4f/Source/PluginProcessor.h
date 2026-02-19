#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// SimpleStereoDelayAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class SimpleStereoDelayAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleStereoDelayAudioProcessor();
    ~SimpleStereoDelayAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SimpleStereoDelay"; }
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

    // DSP Members
    static constexpr int maxDelaySamples = 96000; // ~2 seconds at 48kHz
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { maxDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { maxDelaySamples };

    // Pre-allocated dry buffer for mix blending
    juce::AudioBuffer<float> dryBuffer;

    // Feedback state (per-channel)
    float feedbackSampleL = 0.0f;
    float feedbackSampleR = 0.0f;

    // LFO phase for modulation
    float lfoPhase = 0.0f;
    double currentSampleRate = 44100.0;

    // Smoothed values for click-free parameter changes
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayTimeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modDepthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleStereoDelayAudioProcessor)
};
