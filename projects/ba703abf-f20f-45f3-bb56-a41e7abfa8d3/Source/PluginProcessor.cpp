#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DattorroPlateReverb.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
VelvetAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.2f, 10.0f, 0.01f),
        3.0f,
        "s"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DAMPING", 1 },
        "Damping",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        45.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PREDELAY", 1 },
        "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        20.0f,
        "ms"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        35.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

VelvetAudioProcessor::VelvetAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

VelvetAudioProcessor::~VelvetAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void VelvetAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void VelvetAudioProcessor::releaseResources()
{
    reverb.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void VelvetAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto decaySec = parameters.getRawParameterValue("DECAY")->load();
    auto damping  = parameters.getRawParameterValue("DAMPING")->load() / 100.0f;
    auto predelay = parameters.getRawParameterValue("PREDELAY")->load();
    auto mix      = parameters.getRawParameterValue("MIX")->load() / 100.0f;

    DattorroPlateReverb::Params reverbParams;
    reverbParams.decay      = 1.0f - std::exp(-0.5f * decaySec);
    reverbParams.damping    = damping;
    reverbParams.predelayMs = predelay;
    reverbParams.mix        = mix;

    reverb.process(buffer, reverbParams);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* VelvetAudioProcessor::createEditor()
{
    return new VelvetAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void VelvetAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VelvetAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new VelvetAudioProcessor();
}
