#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DattorroPlateReverb.h"
#include "DryWetMixer.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SludgeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(10.0f, 20.0f, 0.1f),
        14.0f,
        "s"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        40.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SludgeAudioProcessor::SludgeAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SludgeAudioProcessor::~SludgeAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void SludgeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock);
    dryWetMixer.prepare(sampleRate, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void SludgeAudioProcessor::releaseResources()
{
    reverb.reset();
    dryWetMixer.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void SludgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto decaySeconds = parameters.getRawParameterValue("DECAY")->load();
    auto mix          = parameters.getRawParameterValue("MIX")->load() / 100.0f;

    // Map 10-20 second range to high decay coefficients (0.85–0.98)
    float decayCoeff = 0.85f + (decaySeconds - 10.0f) / 10.0f * 0.13f;
    decayCoeff = juce::jlimit(0.85f, 0.98f, decayCoeff);

    dryWetMixer.setDryBuffer(buffer);

    DattorroPlateReverb::Params reverbParams;
    reverbParams.decay      = decayCoeff;
    reverbParams.damping    = 0.55f;      // Dark, warm character
    reverbParams.predelayMs = 25.0f;      // Slight distance feel
    reverbParams.mix        = 1.0f;       // Fully wet — DryWetMixer handles blend
    reverb.process(buffer, reverbParams);

    dryWetMixer.mix(buffer, mix);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* SludgeAudioProcessor::createEditor()
{
    return new SludgeAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void SludgeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SludgeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new SludgeAudioProcessor();
}
