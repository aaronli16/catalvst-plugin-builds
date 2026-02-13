#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// AmbientBuildupOneknobProcessorAudioProcessor
// ==============================================================================

class AmbientBuildupOneknobProcessorAudioProcessor : public juce::AudioProcessor
{
public:
    AmbientBuildupOneknobProcessorAudioProcessor();
    ~AmbientBuildupOneknobProcessorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AmbientBuildupOneknobProcessor"; }
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

    // Delay line - max 1 second
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 48000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 48000 };
    float delayFeedbackL = 0.0f;
    float delayFeedbackR = 0.0f;

    // High-pass filter (one per channel)
    juce::dsp::StateVariableTPTFilter<float> hpfL;
    juce::dsp::StateVariableTPTFilter<float> hpfR;

    // White noise random generator
    juce::Random noiseRandom;

    // Sample rate cache
    double currentSampleRate = 44100.0;

    // Smoothed parameter to avoid zipper noise
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedBuildup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientBuildupOneknobProcessorAudioProcessor)
};
