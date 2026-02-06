#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobDarkReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Wet parameter - controls reverb mix (0% = dry, 100% = wet)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wet", 1 },
        "Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

OneKnobDarkReverbAudioProcessor::OneKnobDarkReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

OneKnobDarkReverbAudioProcessor::~OneKnobDarkReverbAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void OneKnobDarkReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize reverb processor
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverb.prepare(spec);
    reverb.reset();
    
    // Configure reverb parameters for a dark, smooth sound
    juce::dsp::Reverb::Parameters params;
    params.roomSize = 0.8f;      // Large room
    params.damping = 0.7f;       // Damped (darker sound)
    params.wetLevel = 0.5f;      // Will be controlled by parameter
    params.dryLevel = 0.5f;      // Will be controlled by parameter
    params.width = 1.0f;         // Full stereo width
    params.freezeMode = 0.0f;    // No freeze
    
    reverb.setParameters(params);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void OneKnobDarkReverbAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void OneKnobDarkReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read the wet parameter (0.0 to 1.0)
    auto wetAmount = parameters.getRawParameterValue("wet")->load();
    
    // Update reverb mix based on wet parameter
    juce::dsp::Reverb::Parameters params;
    params.roomSize = 0.8f;
    params.damping = 0.7f;
    params.wetLevel = wetAmount;
    params.dryLevel = 1.0f - wetAmount;
    params.width = 1.0f;
    params.freezeMode = 0.0f;
    
    reverb.setParameters(params);
    
    // Process audio through reverb
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* OneKnobDarkReverbAudioProcessor::createEditor()
{
    return new OneKnobDarkReverbAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void OneKnobDarkReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OneKnobDarkReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new OneKnobDarkReverbAudioProcessor();
}
