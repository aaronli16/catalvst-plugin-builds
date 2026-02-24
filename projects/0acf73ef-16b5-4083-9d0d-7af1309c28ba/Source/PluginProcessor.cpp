#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
UntitledPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 },
        "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.35f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 },
        "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PUNCH", 1 },
        "Punch",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.6f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.45f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 },
        "Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.75f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

UntitledPluginAudioProcessor::UntitledPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

UntitledPluginAudioProcessor::~UntitledPluginAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void UntitledPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // ==========================================================================
    // ===== TODO: INITIALIZE YOUR DSP HERE =====
    // ==========================================================================
    //
    // Set up DSP processing specs:
    //   juce::dsp::ProcessSpec spec;
    //   spec.sampleRate = sampleRate;
    //   spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    //   spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    //
    //   myDspProcessor.prepare(spec);
    //   myDspProcessor.reset();
    //
    // ==========================================================================

    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void UntitledPluginAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void UntitledPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // ==========================================================================
    // ===== TODO: IMPLEMENT YOUR DSP PROCESSING HERE =====
    // ==========================================================================
    //
    // Read parameters (atomic, real-time safe):
    //   auto gain = parameters.getRawParameterValue("GAIN")->load();
    //   auto mix = parameters.getRawParameterValue("MIX")->load();
    //
    // Process each channel:
    //   for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    //   {
    //       auto* channelData = buffer.getWritePointer(channel);
    //       for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    //       {
    //           // Your DSP here
    //           channelData[sample] *= gain;
    //       }
    //   }
    //
    // Or use juce::dsp::AudioBlock for processor chains:
    //   juce::dsp::AudioBlock<float> block(buffer);
    //   juce::dsp::ProcessContextReplacing<float> context(block);
    //   myProcessor.process(context);
    //
    // ==========================================================================
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* UntitledPluginAudioProcessor::createEditor()
{
    return new UntitledPluginAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void UntitledPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void UntitledPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new UntitledPluginAudioProcessor();
}
