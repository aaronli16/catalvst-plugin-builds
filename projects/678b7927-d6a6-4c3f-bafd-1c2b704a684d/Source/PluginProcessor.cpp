#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleReverbControlAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Dry/Wet Mix parameter (0-100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRYWET", 1 },
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SimpleReverbControlAudioProcessor::SimpleReverbControlAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SimpleReverbControlAudioProcessor::~SimpleReverbControlAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void SimpleReverbControlAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Set up DSP processing specs
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Initialize reverb
    reverb.prepare(spec);
    reverb.reset();

    // Configure reverb parameters
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.5f;
    reverbParams.wetLevel = 0.5f;
    reverbParams.dryLevel = 0.5f;
    reverbParams.width = 1.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    // Allocate reverb buffer
    reverbBuffer.setSize(spec.numChannels, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void SimpleReverbControlAudioProcessor::releaseResources()
{
    reverb.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void SimpleReverbControlAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read dry/wet parameter (0-100%)
    auto dryWetPercent = parameters.getRawParameterValue("DRYWET")->load();
    auto wetLevel = dryWetPercent / 100.0f; // Convert to 0-1 range
    auto dryLevel = 1.0f - wetLevel;

    // Copy dry signal for later mixing
    reverbBuffer.makeCopyOf(buffer);

    // Process reverb on the buffer
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    
    // Update reverb wet/dry levels
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.5f;
    reverbParams.wetLevel = 1.0f;  // Process at full wet, we'll mix manually
    reverbParams.dryLevel = 0.0f;   // No dry in the reverb processor
    reverbParams.width = 1.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);
    
    reverb.process(context);

    // Mix dry and wet signals
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* wetData = buffer.getWritePointer(channel);
        auto* dryData = reverbBuffer.getReadPointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            wetData[sample] = (dryData[sample] * dryLevel) + (wetData[sample] * wetLevel);
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* SimpleReverbControlAudioProcessor::createEditor()
{
    return new SimpleReverbControlAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void SimpleReverbControlAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimpleReverbControlAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new SimpleReverbControlAudioProcessor();
}
