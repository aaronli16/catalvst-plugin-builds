#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
WhiteNoiseOneknobGeneratorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Wet/Dry mix parameter (0% = 100% dry input, 100% = 100% wet noise)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wet", 1 },
        "Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

WhiteNoiseOneknobGeneratorAudioProcessor::WhiteNoiseOneknobGeneratorAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

WhiteNoiseOneknobGeneratorAudioProcessor::~WhiteNoiseOneknobGeneratorAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void WhiteNoiseOneknobGeneratorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate);
    
    // Allocate buffer for white noise generation
    noiseBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void WhiteNoiseOneknobGeneratorAudioProcessor::releaseResources()
{
    noiseBuffer.setSize(0, 0);
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void WhiteNoiseOneknobGeneratorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read wet parameter (0.0 = dry, 1.0 = wet)
    auto wet = parameters.getRawParameterValue("wet")->load();
    auto dry = 1.0f - wet;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Generate white noise
    noiseBuffer.setSize(numChannels, numSamples, false, false, true);
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* noiseData = noiseBuffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Generate white noise: random values between -1.0 and 1.0
            noiseData[sample] = (random.nextFloat() * 2.0f) - 1.0f;
        }
    }

    // Mix dry input with wet noise
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* outputData = buffer.getWritePointer(channel);
        auto* noiseData = noiseBuffer.getReadPointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float drySignal = outputData[sample];
            float wetSignal = noiseData[sample];
            
            // Blend dry and wet signals
            outputData[sample] = (drySignal * dry) + (wetSignal * wet);
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* WhiteNoiseOneknobGeneratorAudioProcessor::createEditor()
{
    return new WhiteNoiseOneknobGeneratorAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void WhiteNoiseOneknobGeneratorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void WhiteNoiseOneknobGeneratorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new WhiteNoiseOneknobGeneratorAudioProcessor();
}
