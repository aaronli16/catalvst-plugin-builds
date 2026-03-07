#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
HaloAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.35f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.5f, 10.0f, 0.1f),
        4.0f,
        "s"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

HaloAudioProcessor::HaloAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

HaloAudioProcessor::~HaloAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void HaloAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverb.prepare(spec);
    reverb.reset();

    dryWetMixer.prepare(spec);
    dryWetMixer.reset();
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void HaloAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void HaloAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto mix = parameters.getRawParameterValue("MIX")->load();
    auto decay = parameters.getRawParameterValue("DECAY")->load();

    // Map decay (0.5–10.0s) to roomSize (0.3–1.0)
    float roomSize = 0.3f + 0.7f * (decay - 0.5f) / 9.5f;

    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = roomSize;
    reverbParams.damping  = 0.2f;
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverbParams.width    = 1.0f;
    reverb.setParameters(reverbParams);

    dryWetMixer.setWetMixProportion(mix);

    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);
    reverb.process(juce::dsp::ProcessContextReplacing<float>(block));
    dryWetMixer.mixWetSamples(block);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* HaloAudioProcessor::createEditor()
{
    return new HaloAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void HaloAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void HaloAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new HaloAudioProcessor();
}
