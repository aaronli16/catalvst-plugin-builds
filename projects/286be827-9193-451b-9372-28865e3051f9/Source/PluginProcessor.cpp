#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DattorroPlateReverb.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
Velour2AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.2f, 10.0f, 0.1f),
        3.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DAMPING", 1 },
        "Damping",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PREDELAY", 1 },
        "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        20.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        35.0f));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

Velour2AudioProcessor::Velour2AudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

Velour2AudioProcessor::~Velour2AudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void Velour2AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void Velour2AudioProcessor::releaseResources()
{
    reverb.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void Velour2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters (atomic, real-time safe)
    auto decaySec  = parameters.getRawParameterValue("DECAY")->load();
    auto dampPct   = parameters.getRawParameterValue("DAMPING")->load();
    auto predelay  = parameters.getRawParameterValue("PREDELAY")->load();
    auto mixPct    = parameters.getRawParameterValue("MIX")->load();

    // Map JUCE parameter ranges to DattorroPlateReverb::Params (0–1)
    DattorroPlateReverb::Params reverbParams;
    reverbParams.decay      = 0.1f + (decaySec - 0.2f) / 9.8f * 0.88f;
    reverbParams.damping    = dampPct / 100.0f;
    reverbParams.predelayMs = predelay;
    reverbParams.mix        = mixPct / 100.0f;

    reverb.process(buffer, reverbParams);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* Velour2AudioProcessor::createEditor()
{
    return new Velour2AudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void Velour2AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Velour2AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new Velour2AudioProcessor();
}
