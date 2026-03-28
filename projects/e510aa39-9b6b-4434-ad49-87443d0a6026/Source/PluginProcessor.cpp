#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FreevervRoomReverb.h"
#include "SignalsmithHallReverb.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
ClarityAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "TYPE", 1 },
        "Type",
        juce::StringArray { "Room", "Hall" },
        0
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.1f),
        2.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BRIGHTNESS", 1 },
        "Brightness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.7f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.35f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

ClarityAudioProcessor::ClarityAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

ClarityAudioProcessor::~ClarityAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void ClarityAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    roomReverb.prepare(sampleRate, samplesPerBlock);
    hallReverb.prepare(sampleRate, samplesPerBlock);
    
    roomReverb.reset();
    hallReverb.reset();
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void ClarityAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void ClarityAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto type       = static_cast<int>(parameters.getRawParameterValue("TYPE")->load());
    auto decay      = parameters.getRawParameterValue("DECAY")->load();
    auto brightness = parameters.getRawParameterValue("BRIGHTNESS")->load();
    auto mix        = parameters.getRawParameterValue("MIX")->load();

    // Process with selected reverb type (both reverbs have internal dry/wet mixing)
    if (type == 0) // Room
    {
        FreevervRoomReverb::Params params;
        params.roomSize = juce::jmap(decay, 0.1f, 10.0f, 0.0f, 1.0f);
        params.damping  = 1.0f - brightness;
        params.width    = 1.0f;
        params.mix      = mix;
        roomReverb.process(buffer, params);
    }
    else // Hall
    {
        SignalsmithHallReverb::Params params;
        params.decaySeconds = decay;
        params.brightness   = brightness;
        params.size         = 0.8f;
        params.mix          = mix;
        hallReverb.process(buffer, params);
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* ClarityAudioProcessor::createEditor()
{
    return new ClarityAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void ClarityAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ClarityAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new ClarityAudioProcessor();
}
