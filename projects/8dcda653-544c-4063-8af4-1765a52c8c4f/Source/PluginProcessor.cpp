#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleStereoDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Delay Time: 50ms to 1000ms, default 350ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TIME", 1 },
        "Time",
        juce::NormalisableRange<float>(50.0f, 1000.0f, 1.0f),
        350.0f,
        "ms"
    ));

    // Feedback: 0% to 95%, default 45%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 95.0f, 1.0f),
        45.0f,
        "%"
    ));

    // Mod Depth: 0% to 100%, default 25%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MOD", 1 },
        "Mod",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        25.0f,
        "%"
    ));

    // Mix: 0% to 100%, default 30%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        30.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SimpleStereoDelayAudioProcessor::SimpleStereoDelayAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SimpleStereoDelayAudioProcessor::~SimpleStereoDelayAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void SimpleStereoDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1; // Each delay line processes one channel

    delayLineL.prepare(spec);
    delayLineR.prepare(spec);
    delayLineL.reset();
    delayLineR.reset();

    // Pre-allocate the dry buffer
    dryBuffer.setSize(2, samplesPerBlock);

    // Reset feedback state
    feedbackSampleL = 0.0f;
    feedbackSampleR = 0.0f;
    lfoPhase = 0.0f;

    // Initialize smoothed values
    delayTimeSmoothed.reset(sampleRate, 0.05);
    feedbackSmoothed.reset(sampleRate, 0.05);
    modDepthSmoothed.reset(sampleRate, 0.05);
    mixSmoothed.reset(sampleRate, 0.05);

    delayTimeSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("TIME")->load());
    feedbackSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("FEEDBACK")->load() / 100.0f);
    modDepthSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("MOD")->load() / 100.0f);
    mixSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("MIX")->load() / 100.0f);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void SimpleStereoDelayAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void SimpleStereoDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameter targets (once per block)
    delayTimeSmoothed.setTargetValue(parameters.getRawParameterValue("TIME")->load());
    feedbackSmoothed.setTargetValue(parameters.getRawParameterValue("FEEDBACK")->load() / 100.0f);
    modDepthSmoothed.setTargetValue(parameters.getRawParameterValue("MOD")->load() / 100.0f);
    mixSmoothed.setTargetValue(parameters.getRawParameterValue("MIX")->load() / 100.0f);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Save dry signal into pre-allocated buffer (no allocation)
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // LFO rate fixed at 0.5 Hz for smooth modulation
    const float lfoRate = 0.5f;
    const float lfoIncrement = lfoRate / static_cast<float>(currentSampleRate);

    // Get write pointers (handle mono gracefully)
    auto* leftData  = buffer.getWritePointer(0);
    auto* rightData = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // Get smoothed parameter values per sample (lightweight math)
        const float delayTimeMs = delayTimeSmoothed.getNextValue();
        const float feedback    = feedbackSmoothed.getNextValue();
        const float modDepth    = modDepthSmoothed.getNextValue();
        const float mix         = mixSmoothed.getNextValue();

        // LFO modulation on delay time
        const float lfoValue = std::sin(2.0f * juce::MathConstants<float>::pi * lfoPhase);
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        // Modulated delay in samples (max modulation range: +/- 3ms)
        const float modAmountMs = modDepth * 3.0f * lfoValue;
        const float delaySamplesBase = (delayTimeMs / 1000.0f) * static_cast<float>(currentSampleRate);
        const float modSamples = (modAmountMs / 1000.0f) * static_cast<float>(currentSampleRate);

        const float delaySamplesL = juce::jmax(1.0f, delaySamplesBase + modSamples);
        const float delaySamplesR = juce::jmax(1.0f, delaySamplesBase - modSamples);

        delayLineL.setDelay(delaySamplesL);
        delayLineR.setDelay(delaySamplesR);

        // Read input
        const float inputL = leftData[i];
        const float inputR = rightData != nullptr ? rightData[i] : inputL;

        // Ping-pong: left input feeds right delay, right input feeds left delay
        // Push input + cross-feedback into delay lines
        const float toDelayL = inputL + std::tanh(feedbackSampleR * feedback);
        const float toDelayR = inputR + std::tanh(feedbackSampleL * feedback);

        delayLineL.pushSample(0, toDelayL);
        delayLineR.pushSample(0, toDelayR);

        // Read delayed output
        feedbackSampleL = delayLineL.popSample(0);
        feedbackSampleR = delayLineR.popSample(0);

        // Mix dry and wet
        const float dryL = dryBuffer.getSample(0, i);
        const float dryR = numChannels > 1 ? dryBuffer.getSample(1, i) : dryL;

        float outL = dryL * (1.0f - mix) + feedbackSampleL * mix;
        float outR = dryR * (1.0f - mix) + feedbackSampleR * mix;

        // NaN/Inf safety guard
        if (std::isnan(outL) || std::isinf(outL)) outL = 0.0f;
        if (std::isnan(outR) || std::isinf(outR)) outR = 0.0f;

        leftData[i] = outL;
        if (rightData != nullptr)
            rightData[i] = outR;
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* SimpleStereoDelayAudioProcessor::createEditor()
{
    return new SimpleStereoDelayAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void SimpleStereoDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimpleStereoDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new SimpleStereoDelayAudioProcessor();
}
