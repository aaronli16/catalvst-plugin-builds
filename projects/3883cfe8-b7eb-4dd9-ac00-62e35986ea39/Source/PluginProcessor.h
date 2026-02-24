#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// RetroverbAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class RetroverbAudioProcessor : public juce::AudioProcessor
{
public:
    RetroverbAudioProcessor();
    ~RetroverbAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Retroverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

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
    static constexpr int numVocoderBands = 16;

    // Vocoder analysis and synthesis filter banks (stereo)
    std::array<juce::dsp::IIR::Filter<float>, numVocoderBands> analysisBandsL;
    std::array<juce::dsp::IIR::Filter<float>, numVocoderBands> analysisBandsR;
    std::array<juce::dsp::IIR::Filter<float>, numVocoderBands> synthesisBandsL;
    std::array<juce::dsp::IIR::Filter<float>, numVocoderBands> synthesisBandsR;

    // Envelope followers for vocoder bands
    std::array<float, numVocoderBands> envelopeL {};
    std::array<float, numVocoderBands> envelopeR {};

    // Chorus via modulated delay lines
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelayL { 4410 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> chorusDelayR { 4410 };

    // Chorus LFO phases
    float chorusPhaseL = 0.0f;
    float chorusPhaseR = 0.0f;

    // Pre-allocated dry buffer
    juce::AudioBuffer<float> dryBuffer;

    // Cached sample rate
    double currentSampleRate = 44100.0;

    // Band center frequencies for vocoder
    std::array<float, numVocoderBands> bandFrequencies {};

    // Smoothed parameter values
    juce::SmoothedValue<float> robotSmoothed;
    juce::SmoothedValue<float> blendSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    void updateVocoderCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RetroverbAudioProcessor)
};
