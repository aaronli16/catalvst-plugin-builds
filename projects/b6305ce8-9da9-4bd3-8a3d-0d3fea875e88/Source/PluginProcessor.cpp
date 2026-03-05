#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
CrunchAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BITS", 1 },
        "Bits",
        juce::NormalisableRange<float>(1.0f, 16.0f, 0.1f),
        16.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CRUSH", 1 },
        "Crush",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

CrunchAudioProcessor::CrunchAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

CrunchAudioProcessor::~CrunchAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void CrunchAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    dryWetMixer.prepare(spec);
    dryWetMixer.reset();

    crushSmoothed.reset(sampleRate, 0.03);

    for (int ch = 0; ch < 2; ++ch)
    {
        holdSample[ch] = 0.0f;
        holdCounter[ch] = 0.0f;
    }
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void CrunchAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void CrunchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto bitsParam = parameters.getRawParameterValue("BITS")->load();
    auto crushParam = parameters.getRawParameterValue("CRUSH")->load();
    auto mixParam = parameters.getRawParameterValue("MIX")->load();

    crushSmoothed.setTargetValue(crushParam);
    dryWetMixer.setWetMixProportion(mixParam);

    float levels = std::exp2f(bitsParam) - 1.0f;

    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float crush = crushSmoothed.getNextValue();
        float holdPeriod = 1.0f + crush * 0.49f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);

            holdCounter[ch] += 1.0f;
            if (holdCounter[ch] >= holdPeriod)
            {
                holdCounter[ch] -= holdPeriod;
                holdSample[ch] = data[i];
            }

            float crushed = holdSample[ch];
            if (levels > 0.0f)
                crushed = std::round(crushed * levels) / levels;

            if (std::isnan(crushed) || std::isinf(crushed))
                crushed = 0.0f;

            data[i] = crushed;
        }
    }

    dryWetMixer.mixWetSamples(block);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* CrunchAudioProcessor::createEditor()
{
    return new CrunchAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void CrunchAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CrunchAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new CrunchAudioProcessor();
}
