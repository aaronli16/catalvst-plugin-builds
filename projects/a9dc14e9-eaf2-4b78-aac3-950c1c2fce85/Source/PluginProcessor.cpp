#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
UntitledPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Oscillator
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OSC_WAVEFORM", 1 }, "Osc Waveform",
        juce::NormalisableRange<float>(0.0f, 3.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OSC_COARSE", 1 }, "Osc Coarse",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f, "st"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OSC_FINE", 1 }, "Osc Fine",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f, "ct"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OSC_SUB", 1 }, "Sub Level",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 60.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OSC_NOISE", 1 }, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OSC_UNISON", 1 }, "Unison",
        juce::NormalisableRange<float>(1.0f, 8.0f, 1.0f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OSC_DETUNE", 1 }, "Detune",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f, "%"));

    // Filter
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_TYPE", 1 }, "Filter Type",
        juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_CUTOFF", 1 }, "Filter Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 800.0f, "Hz"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_RESO", 1 }, "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_ENV", 1 }, "Filter Envelope",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_DRIVE", 1 }, "Filter Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    // Amp Envelope
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "AMP_ATTACK", 1 }, "Attack",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 0.1f, 0.4f), 5.0f, "ms"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "AMP_DECAY", 1 }, "Decay",
        juce::NormalisableRange<float>(0.0f, 5000.0f, 0.1f, 0.4f), 200.0f, "ms"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "AMP_SUSTAIN", 1 }, "Sustain",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 80.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "AMP_RELEASE", 1 }, "Release",
        juce::NormalisableRange<float>(0.0f, 5000.0f, 0.1f, 0.4f), 150.0f, "ms"));

    // LFO
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LFO_SHAPE", 1 }, "LFO Shape",
        juce::NormalisableRange<float>(0.0f, 3.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LFO_RATE", 1 }, "LFO Rate",
        juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f, 0.5f), 4.0f, "Hz"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LFO_DEPTH", 1 }, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    // Distortion
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DIST_TYPE", 1 }, "Distortion Type",
        juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DIST_AMOUNT", 1 }, "Distortion Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DIST_MIX", 1 }, "Distortion Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    // Effects
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FX_CHORUS", 1 }, "Chorus",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FX_REVERB", 1 }, "Reverb",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FX_DELAY", 1 }, "Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FX_CRUSH", 1 }, "Bit Crush",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f, "%"));

    // Output
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OUTPUT_GAIN", 1 }, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f, "dB"));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

UntitledPluginAudioProcessor::UntitledPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
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
