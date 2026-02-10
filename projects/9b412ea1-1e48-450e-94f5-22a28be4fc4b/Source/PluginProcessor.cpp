#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Wet/Dry mix parameter (0% to 100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WET", 1 },
        "Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f  // Default: 0% wet (all dry)
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
    // Initialize DSP processing spec
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Prepare reverb processor
    reverb.prepare(spec);
    reverb.reset();

    // Configure reverb parameters for bright plate sound
    juce::dsp::Reverb::Parameters params;
    params.roomSize = 0.7f;      // Medium-large room for plate character
    params.damping = 0.3f;       // Low damping = brighter sound
    params.wetLevel = 1.0f;      // Full wet (we'll mix manually)
    params.dryLevel = 0.0f;      // No dry in reverb (we'll mix manually)
    params.width = 1.0f;         // Full stereo width
    params.freezeMode = 0.0f;    // No freeze
    reverb.setParameters(params);

    // Prepare dry buffer for parallel processing
    dryBuffer.setSize(spec.numChannels, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void OneKnobReverbAudioProcessor::releaseResources()
{
    // Clean up resources
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void OneKnobReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Get wet parameter (0.0 to 1.0)
    const float wetAmount = parameters.getRawParameterValue("WET")->load();
    const float dryAmount = 1.0f - wetAmount;

    // Store dry signal
    dryBuffer.makeCopyOf(buffer, true);

    // Process reverb (buffer now contains wet signal)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Mix dry and wet signals
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* outputData = buffer.getWritePointer(channel);
        const auto* dryData = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            outputData[sample] = (dryData[sample] * dryAmount) + (outputData[sample] * wetAmount);
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
