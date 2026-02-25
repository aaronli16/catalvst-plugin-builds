#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
LimitSmileAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "INTENSITY", 1 },
        "Intensity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "MODE", 1 },
        "Mode",
        juce::StringArray { "Lift Off", "Stratosphere", "Dada Dynamite" },
        0
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

LimitSmileAudioProcessor::LimitSmileAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

LimitSmileAudioProcessor::~LimitSmileAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void LimitSmileAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    buildupEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void LimitSmileAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void LimitSmileAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto intensity = parameters.getRawParameterValue("INTENSITY")->load();
    auto mode = static_cast<int>(parameters.getRawParameterValue("MODE")->load());

    buildupEngine.processBlock(buffer, intensity, mode);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* LimitSmileAudioProcessor::createEditor()
{
    return new LimitSmileAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void LimitSmileAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LimitSmileAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new LimitSmileAudioProcessor();
}
