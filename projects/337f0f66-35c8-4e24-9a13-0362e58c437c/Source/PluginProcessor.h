#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// WashoutSmileAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class WashoutSmileAudioProcessor : public juce::AudioProcessor
{
public:
    WashoutSmileAudioProcessor();
    ~WashoutSmileAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "WashoutSmile"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // NOTE: For reverb/delay effects, override this to return a realistic tail time
    // (e.g., 10.0-20.0 seconds). Returning 0.0 tells the DAW to cut processing
    // immediately when input stops, which kills reverb/delay tails.
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

    // --------------------------------------------------------------------------
    // DSP Members
    // --------------------------------------------------------------------------

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // Highpass filter (one per channel)
    juce::dsp::StateVariableTPTFilter<float> highpassFilter;

    // Reverb + dry/wet
    juce::dsp::Reverb reverb;
    juce::dsp::DryWetMixer<float> reverbDryWet;

    // Feedback delay (sample-by-sample)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 96000 * 2 };
    juce::dsp::DryWetMixer<float> delayDryWet;
    juce::SmoothedValue<float> delayTimeSmoothed;

    // Phaser + dry/wet
    juce::dsp::Phaser<float> phaser;
    juce::dsp::DryWetMixer<float> phaserDryWet;

    // Smoothed values for real-time parameters
    juce::SmoothedValue<float> intensitySmoothed;
    juce::SmoothedValue<float> highpassFreqSmoothed;
    juce::SmoothedValue<float> noiseGainSmoothed;
    juce::SmoothedValue<float> satDriveSmoothed;
    juce::SmoothedValue<float> bitcrushMixSmoothed;
    juce::SmoothedValue<float> stereoWidthSmoothed;
    juce::SmoothedValue<float> riserPhaseSmoothed;

    // Shepard tone oscillator phases (multiple octaves)
    static constexpr int kNumShepardOscs = 6;
    float shepardPhases[kNumShepardOscs] = {};
    float shepardBaseFreq = 80.0f;

    // Pre-allocated dry buffer for manual dry/wet
    juce::AudioBuffer<float> dryBuffer;

    // Noise RNG
    juce::Random noiseRng;

    // Bitcrusher state
    float bitcrushHoldL = 0.0f;
    float bitcrushHoldR = 0.0f;
    int bitcrushCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WashoutSmileAudioProcessor)
};
