#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Mix parameter - Controls wet/dry blend (0% = fully dry, 100% = fully wet)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f  // Default 50% mix
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
    // Configure DSP processing specifications
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Initialize reverb
    reverb.prepare(spec);
    reverb.reset();
    
    // Configure reverb parameters for big hall with long, lush decay
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 1.0f;        // Maximum room size for big hall
    reverbParams.damping = 0.3f;         // Low damping for bright, lush tail
    reverbParams.wetLevel = 1.0f;        // Full wet (we'll mix manually)
    reverbParams.dryLevel = 0.0f;        // No dry (we'll mix manually)
    reverbParams.width = 1.0f;           // Full stereo width
    reverbParams.freezeMode = 0.0f;      // Normal mode (not frozen)
    
    reverb.setParameters(reverbParams);
    
    // Allocate dry buffer for wet/dry mixing
    dryBuffer.setSize(spec.numChannels, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void OneKnobReverbAudioProcessor::releaseResources()
{
    // Clean up allocated resources
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

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't have corresponding inputs
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Read mix parameter (0.0 = dry, 1.0 = wet)
    auto mix = parameters.getRawParameterValue("mix")->load();

    // Store dry signal for mixing later
    dryBuffer.makeCopyOf(buffer, true);

    // Process reverb (replaces buffer with wet signal)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Mix dry and wet signals based on mix parameter
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* wetData = buffer.getWritePointer(channel);
        auto* dryData = dryBuffer.getReadPointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            wetData[sample] = dryData[sample] * (1.0f - mix) + wetData[sample] * mix;
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
