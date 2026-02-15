#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SmoothWaveFlangerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Rate: LFO speed in Hz (slow & wide sweep)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 },
        "Rate",
        juce::NormalisableRange<float>(0.05f, 5.0f, 0.01f, 0.4f),
        0.4f,
        "Hz"
    ));

    // Depth: how much the LFO modulates the delay time (0-100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 },
        "Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        "%"
    ));

    // Manual: base delay time in milliseconds
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MANUAL", 1 },
        "Manual",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f),
        5.0f,
        "ms"
    ));

    // Feedback: amount of delayed signal fed back (0-95%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f),
        35.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SmoothWaveFlangerAudioProcessor::SmoothWaveFlangerAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SmoothWaveFlangerAudioProcessor::~SmoothWaveFlangerAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void SmoothWaveFlangerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    delayLineL.prepare(spec);
    delayLineR.prepare(spec);
    delayLineL.reset();
    delayLineR.reset();

    lfoPhase = 0.0f;
    feedbackStateL = 0.0f;
    feedbackStateR = 0.0f;
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void SmoothWaveFlangerAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void SmoothWaveFlangerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters (atomic, real-time safe)
    const float rateHz      = parameters.getRawParameterValue("RATE")->load();
    const float depthPct    = parameters.getRawParameterValue("DEPTH")->load();
    const float manualMs    = parameters.getRawParameterValue("MANUAL")->load();
    const float feedbackPct = parameters.getRawParameterValue("FEEDBACK")->load();

    // Convert to working units
    const float depth    = depthPct / 100.0f;
    const float feedback = feedbackPct / 100.0f;
    const float baseDelaySamples = static_cast<float>(manualMs * 0.001 * currentSampleRate);
    const float lfoIncrement = rateHz / static_cast<float>(currentSampleRate);

    // Feedback smoothing coefficient (gentle one-pole lowpass at ~2 kHz)
    const float fbSmooth = std::exp(-juce::MathConstants<float>::twoPi * 2000.0f
                                     / static_cast<float>(currentSampleRate));

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    auto* dataL = (numChannels > 0) ? buffer.getWritePointer(0) : nullptr;
    auto* dataR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // Triangle LFO for smooth, vintage sweep (0..1 range)
        float lfoValue = 2.0f * std::abs(2.0f * lfoPhase - 1.0f) - 1.0f; // -1..+1
        lfoValue = (lfoValue + 1.0f) * 0.5f; // 0..1

        // Modulated delay time in samples
        float modulatedDelay = baseDelaySamples + depth * baseDelaySamples * lfoValue;
        modulatedDelay = juce::jlimit(0.0f, static_cast<float>(maxDelaySamples - 1), modulatedDelay);

        // Advance LFO phase
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        // Process left channel
        if (dataL != nullptr)
        {
            float inputL = dataL[i];
            float delayedL = delayLineL.popSample(0, modulatedDelay);

            // Warm feedback smoothing
            feedbackStateL = delayedL + fbSmooth * (feedbackStateL - delayedL);
            float fbSampleL = feedbackStateL * feedback;

            delayLineL.pushSample(0, inputL + fbSampleL);

            // Mix: equal parts dry + wet for classic flanger comb effect
            dataL[i] = inputL + delayedL;
        }

        // Process right channel
        if (dataR != nullptr)
        {
            float inputR = dataR[i];
            float delayedR = delayLineR.popSample(0, modulatedDelay);

            // Warm feedback smoothing
            feedbackStateR = delayedR + fbSmooth * (feedbackStateR - delayedR);
            float fbSampleR = feedbackStateR * feedback;

            delayLineR.pushSample(0, inputR + fbSampleR);

            dataR[i] = inputR + delayedR;
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* SmoothWaveFlangerAudioProcessor::createEditor()
{
    return new SmoothWaveFlangerAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void SmoothWaveFlangerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SmoothWaveFlangerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ==============================================================================
// Factory Function - Required by JUCE plugin system
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmoothWaveFlangerAudioProcessor();
}
