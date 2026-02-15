#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleSpringReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Mix: 0-100% dry/wet blend
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        35.0f,
        "%"
    ));

    // Size: 0-100% room size
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SIZE", 1 },
        "Size",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        60.0f,
        "%"
    ));

    // Shimmer: 0-100% shimmer intensity (octave-up pitch shift feedback)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER", 1 },
        "Shimmer",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        40.0f,
        "%"
    ));

    // Decay: 0.1-20 seconds reverb tail length
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f, 0.4f),
        4.0f,
        "s"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SimpleSpringReverbAudioProcessor::SimpleSpringReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SimpleSpringReverbAudioProcessor::~SimpleSpringReverbAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void SimpleSpringReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    shimmerEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    shimmerEngine.reset();
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void SimpleSpringReverbAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void SimpleSpringReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameter values (atomic, real-time safe)
    auto mix     = parameters.getRawParameterValue("MIX")->load()     / 100.0f; // 0-1
    auto size    = parameters.getRawParameterValue("SIZE")->load()    / 100.0f; // 0-1
    auto shimmer = parameters.getRawParameterValue("SHIMMER")->load() / 100.0f; // 0-1
    auto decay   = parameters.getRawParameterValue("DECAY")->load();            // 0.1-20s

    // Update engine parameters
    shimmerEngine.setMix(mix);
    shimmerEngine.setSize(size);
    shimmerEngine.setShimmer(shimmer);
    shimmerEngine.setDecay(decay);

    // Process the audio
    shimmerEngine.processBlock(buffer);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* SimpleSpringReverbAudioProcessor::createEditor()
{
    return new SimpleSpringReverbAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void SimpleSpringReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimpleSpringReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new SimpleSpringReverbAudioProcessor();
}
