#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f
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
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverb.prepare(spec);
    reverb.reset();

    // Pre-allocate dry buffer for wet/dry mixing
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void OneKnobReverbAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void OneKnobReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto mix = parameters.getRawParameterValue("MIX")->load();

    // Save dry signal into pre-allocated buffer
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

    // Configure reverb for small room character (once per block)
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize   = 0.35f;
    reverbParams.damping    = 0.55f;
    reverbParams.wetLevel   = 1.0f;
    reverbParams.dryLevel   = 0.0f;
    reverbParams.width      = 0.8f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    // Process reverb (buffer now contains 100% wet signal)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Mix dry and wet: output = dry * (1 - mix) + wet * mix
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wetData = buffer.getWritePointer(ch);
        auto* dryData = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float output = dryData[i] * (1.0f - mix) + wetData[i] * mix;

            if (std::isnan(output) || std::isinf(output))
                output = 0.0f;

            wetData[i] = output;
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
