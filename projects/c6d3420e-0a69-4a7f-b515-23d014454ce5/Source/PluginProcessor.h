#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// SimpleHalfbeatEchoAudioProcessor
// ==============================================================================

class SimpleHalfbeatEchoAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleHalfbeatEchoAudioProcessor();
    ~SimpleHalfbeatEchoAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SimpleHalfbeatEcho"; }
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

    // DSP: Stereo delay lines for half-beat echo
    // Max delay = 2 seconds at 192kHz = 384000 samples (generous)
    static constexpr int maxDelaySamples = 384000;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { maxDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { maxDelaySamples };

    double currentSampleRate = 44100.0;

    // Smoothed delay time to avoid clicks on BPM changes
    juce::SmoothedValue<float> smoothedDelayTimeL;
    juce::SmoothedValue<float> smoothedDelayTimeR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleHalfbeatEchoAudioProcessor)
};
