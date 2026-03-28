#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
AbyssalAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.35f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SIZE", 1 },
        "Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

AbyssalAudioProcessor::AbyssalAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

AbyssalAudioProcessor::~AbyssalAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void AbyssalAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    reverb.prepare(sampleRate, samplesPerBlock);
    dryWetMixer.prepare(sampleRate, samplesPerBlock);

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    darkFilterL.prepare(monoSpec);
    darkFilterR.prepare(monoSpec);
    darkFilterL.reset();
    darkFilterR.reset();
    *darkFilterL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1000.0, 0.707);
    *darkFilterR.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 1000.0, 0.707);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void AbyssalAudioProcessor::releaseResources()
{
    reverb.reset();
    dryWetMixer.reset();
    darkFilterL.reset();
    darkFilterR.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void AbyssalAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto mix  = parameters.getRawParameterValue("MIX")->load();
    auto size = parameters.getRawParameterValue("SIZE")->load();

    // Save dry signal before processing
    dryWetMixer.setDryBuffer(buffer);

    // Configure reverb: octave-down shimmer, fully wet output
    ShimmerReverb::Params reverbParams;
    reverbParams.decay         = size;
    reverbParams.shimmerAmount = 0.7f;
    reverbParams.pitchShift    = -12.0f;
    reverbParams.mix           = 1.0f;

    reverb.process(buffer, reverbParams);

    // Apply dark low-pass filter to wet signal only
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 1)
    {
        auto* dataL = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            dataL[i] = darkFilterL.processSample(dataL[i]);
    }
    if (buffer.getNumChannels() >= 2)
    {
        auto* dataR = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
            dataR[i] = darkFilterR.processSample(dataR[i]);
    }

    // Blend dry and wet with constant-power crossfade
    dryWetMixer.mix(buffer, mix);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* AbyssalAudioProcessor::createEditor()
{
    return new AbyssalAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void AbyssalAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AbyssalAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new AbyssalAudioProcessor();
}
