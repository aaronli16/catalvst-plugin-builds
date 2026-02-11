#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// SimpleHalfbeatDelayAudioProcessor
// ==============================================================================

class SimpleHalfbeatDelayAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleHalfbeatDelayAudioProcessor();
    ~SimpleHalfbeatDelayAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SimpleHalfbeatDelay"; }
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

    // DSP: Delay line with max 2 seconds at 192kHz
    static constexpr int maxDelaySamples = 384000;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { maxDelaySamples };

    double currentSampleRate = 44100.0;

    // Smoothed parameters to avoid clicks
    juce::SmoothedValue<float> smoothedMix;
    juce::SmoothedValue<float> smoothedFeedback;

    // Feedback buffer (one sample per channel)
    float feedbackSample[2] = { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleHalfbeatDelayAudioProcessor)
};
