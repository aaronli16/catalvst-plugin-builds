#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
GritAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BIT_DEPTH", 1 },
        "Bit Depth",
        juce::NormalisableRange<float>(4.0f, 12.0f, 0.1f),
        12.0f,
        "bit"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SAMPLE_RATE", 1 },
        "Sample Rate",
        juce::NormalisableRange<float>(1000.0f, 44100.0f, 1.0f),
        44100.0f,
        "Hz"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

GritAudioProcessor::GritAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

GritAudioProcessor::~GritAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void GritAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    bitDepthSmoothed.reset(sampleRate, 0.02);
    sampleRateSmoothed.reset(sampleRate, 0.02);
    bitDepthSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("BIT_DEPTH")->load());
    sampleRateSmoothed.setCurrentAndTargetValue(parameters.getRawParameterValue("SAMPLE_RATE")->load());

    for (int ch = 0; ch < 2; ++ch)
    {
        holdSample[ch] = 0.0f;
        holdCounter[ch] = 0.0f;
    }
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void GritAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void GritAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    bitDepthSmoothed.setTargetValue(parameters.getRawParameterValue("BIT_DEPTH")->load());
    sampleRateSmoothed.setTargetValue(parameters.getRawParameterValue("SAMPLE_RATE")->load());

    const int numChannels = std::min(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float bd = bitDepthSmoothed.getNextValue();
        const float sr = sampleRateSmoothed.getNextValue();
        const float holdInterval = static_cast<float>(hostSampleRate) / sr;
        const float scale = std::pow(2.0f, bd - 1.0f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);

            holdCounter[ch] += 1.0f;
            if (holdCounter[ch] >= holdInterval)
            {
                holdCounter[ch] -= holdInterval;
                holdSample[ch] = data[i];
            }

            float sample = holdSample[ch];
            sample = std::round(sample * scale) / scale;

            if (std::isnan(sample) || std::isinf(sample))
                sample = 0.0f;

            data[i] = sample;
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* GritAudioProcessor::createEditor()
{
    return new GritAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void GritAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void GritAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new GritAudioProcessor();
}
