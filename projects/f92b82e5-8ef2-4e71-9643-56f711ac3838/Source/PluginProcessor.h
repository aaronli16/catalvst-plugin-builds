#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// MetronomeAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class MetronomeAudioProcessor : public juce::AudioProcessor
{
public:
    MetronomeAudioProcessor();
    ~MetronomeAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Metronome"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // NOTE: For reverb/delay effects, override this to return a realistic tail time
    // (e.g., 10.0-20.0 seconds). Returning 0.0 tells the DAW to cut processing
    // immediately when input stops, which kills reverb/delay tails.
    double getTailLengthSeconds() const override { return 0.0; }

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
    // ===== TODO: ADD YOUR DSP MEMBER VARIABLES HERE =====
    // ==========================================================================
    //
    // Examples:
    //   juce::dsp::Reverb reverb;
    //   juce::dsp::Compressor<float> compressor;
    //   juce::dsp::DelayLine<float> delayLine { 44100 };
    //   juce::dsp::IIR::Filter<float> filter;
    //
    // For complex DSP, create separate classes (e.g., ReverbEngine.h)
    //
    // ==========================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetronomeAudioProcessor)
};
