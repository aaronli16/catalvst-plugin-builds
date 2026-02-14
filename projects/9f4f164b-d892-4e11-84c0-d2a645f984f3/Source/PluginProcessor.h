#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// EndlessSmileSidechainEffectAudioProcessor
// ==============================================================================

class EndlessSmileSidechainEffectAudioProcessor : public juce::AudioProcessor
{
public:
    EndlessSmileSidechainEffectAudioProcessor();
    ~EndlessSmileSidechainEffectAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EndlessSmileSidechainEffect"; }
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

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ==========================================================================
    // DSP Members
    // ==========================================================================

    // Reverb
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    // Delay line (stereo, up to 1 second)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 48000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 48000 };

    // Random number generator for white noise
    juce::Random noiseRng;

    // Smoothed intensity to avoid zipper noise
    juce::SmoothedValue<float> smoothedIntensity;

    // Store sample rate for delay time calculation
    double currentSampleRate = 44100.0;

    // Delay feedback buffers
    float delayFeedbackL = 0.0f;
    float delayFeedbackR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EndlessSmileSidechainEffectAudioProcessor)
};
