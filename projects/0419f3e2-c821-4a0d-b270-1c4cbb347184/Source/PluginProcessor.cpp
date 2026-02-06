#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Dry/Wet mix parameter (0 = fully dry, 1 = fully wet)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "dryWet", 1 },
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f  // Default to fully dry
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

OneKnobReverbAudioProcessor::OneKnobReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

OneKnobReverbAudioProcessor::~OneKnobReverbAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void OneKnobReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Set up reverb parameters for a nice, simple reverb sound
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 0.7f;      // Medium-large room
    reverbParams.damping = 0.5f;       // Balanced high frequency damping
    reverbParams.wetLevel = 1.0f;      // Full wet (we'll mix manually)
    reverbParams.dryLevel = 0.0f;      // No dry in reverb (we'll mix manually)
    reverbParams.width = 1.0f;         // Full stereo width
    reverbParams.freezeMode = 0.0f;    // No freeze
    
    reverbL.setParameters(reverbParams);
    reverbR.setParameters(reverbParams);
    
    // Prepare reverb processors
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;  // Process each channel separately
    
    reverbL.prepare(spec);
    reverbR.prepare(spec);
    
    reverbL.reset();
    reverbR.reset();
    
    // Prepare dry buffer
    dryBuffer.setSize(2, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void OneKnobReverbAudioProcessor::releaseResources()
{
    dryBuffer.setSize(0, 0);
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void OneKnobReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read the dry/wet parameter (0.0 = fully dry, 1.0 = fully wet)
    auto dryWet = parameters.getRawParameterValue("dryWet")->load();
    
    // Calculate dry and wet gains
    float wetGain = dryWet;
    float dryGain = 1.0f - dryWet;
    
    // Get number of samples and channels
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    // Make sure dry buffer is the right size
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    
    // Copy dry signal to dry buffer
    for (int ch = 0; ch < numChannels; ++ch)
    {
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }
    
    // Process reverb on each channel
    if (numChannels >= 1)
    {
        // Left channel
        auto* leftData = buffer.getWritePointer(0);
        reverbL.processMono(leftData, numSamples);
    }
    
    if (numChannels >= 2)
    {
        // Right channel
        auto* rightData = buffer.getWritePointer(1);
        reverbR.processMono(rightData, numSamples);
    }
    
    // Mix dry and wet signals
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = buffer.getWritePointer(ch);
        auto* dryData = dryBuffer.getReadPointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            wetData[i] = (dryData[i] * dryGain) + (wetData[i] * wetGain);
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* OneKnobReverbAudioProcessor::createEditor()
{
    return new OneKnobReverbAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void OneKnobReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OneKnobReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new OneKnobReverbAudioProcessor();
}
