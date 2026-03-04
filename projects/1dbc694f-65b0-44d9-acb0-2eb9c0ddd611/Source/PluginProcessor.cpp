#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
ScalpelAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ==========================================================================
    // ===== TODO: ADD YOUR PARAMETERS HERE =====
    // ==========================================================================
    //
    // Float parameter example:
    //   layout.add(std::make_unique<juce::AudioParameterFloat>(
    //       juce::ParameterID { "GAIN", 1 },      // ID (must match WebSliderRelay)
    //       "Gain",                               // Display name
    //       juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),  // min, max, step
    //       0.5f,                                 // Default value
    //       "dB"                                  // Unit label (optional)
    //   ));
    //
    // Bool parameter example:
    //   layout.add(std::make_unique<juce::AudioParameterBool>(
    //       juce::ParameterID { "BYPASS", 1 },
    //       "Bypass",
    //       false
    //   ));
    //
    // Choice parameter example:
    //   layout.add(std::make_unique<juce::AudioParameterChoice>(
    //       juce::ParameterID { "MODE", 1 },
    //       "Mode",
    //       juce::StringArray { "Normal", "Warm", "Bright" },
    //       0
    //   ));
    //
    // ==========================================================================

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

ScalpelAudioProcessor::ScalpelAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

ScalpelAudioProcessor::~ScalpelAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void ScalpelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
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

void ScalpelAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void ScalpelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
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

juce::AudioProcessorEditor* ScalpelAudioProcessor::createEditor()
{
    return new ScalpelAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void ScalpelAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ScalpelAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new ScalpelAudioProcessor();
}
