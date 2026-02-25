#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
// UntitledPluginAudioProcessor
// ==============================================================================
// Main audio processor class. Handles DSP and parameter management.
// ==============================================================================

class UntitledPluginAudioProcessor : public juce::AudioProcessor
{
public:
    UntitledPluginAudioProcessor();
    ~UntitledPluginAudioProcessor() override;

    // --------------------------------------------------------------------------
    // AudioProcessor Interface (Required - DO NOT MODIFY SIGNATURES)
    // --------------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UntitledPlugin"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

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
    static constexpr int maxVoices = 8;
    static constexpr int numUnison = 5; // oscillators per voice for detune spread

    struct Voice
    {
        bool active = false;
        int noteNumber = -1;
        float velocity = 0.0f;
        double phase[numUnison] = {};
        double frequency = 0.0;
        float envLevel = 0.0f;
        float envStage = 0; // 0=off, 1=attack, 2=sustain, 3=release
    };

    Voice voices[maxVoices];
    double currentSampleRate = 44100.0;

    // Warmth filter (simple one-pole lowpass per channel)
    float warmthFilterState[2] = { 0.0f, 0.0f };

    // Helper
    float polyBlepSaw(double phase, double phaseIncrement);
    void startVoice(int noteNumber, float velocity);
    void stopVoice(int noteNumber);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UntitledPluginAudioProcessor)
};
