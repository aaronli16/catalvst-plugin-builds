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
        juce::ParameterID { "WARMTH", 1 },
        "Warmth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

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
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    warmthFilterL.prepare(spec);
    warmthFilterL.reset();
    warmthFilterR.prepare(spec);
    warmthFilterR.reset();

    // Pre-allocate the dry buffer for dry/wet mixing
    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
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

    // Read parameters once per block
    auto warmth = parameters.getRawParameterValue("WARMTH")->load();
    auto mix = parameters.getRawParameterValue("MIX")->load();

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Save dry signal into pre-allocated buffer
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Update warmth filter coefficients once per block
    // Maps warmth (0-1) to cutoff frequency: 20kHz (no warmth) down to 2kHz (full warmth)
    float cutoffHz = 20000.0f * std::pow(0.1f, warmth);
    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, cutoffHz);
    *warmthFilterL.coefficients = *coefficients;
    *warmthFilterR.coefficients = *coefficients;

    // Saturation drive amount: gentle at 0, warm overdrive at 1
    float drive = 1.0f + warmth * 4.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = channelData[i];

            // Tape-style soft saturation using tanh
            sample = std::tanh(sample * drive) / std::tanh(drive);

            // Add subtle even-harmonic warmth (asymmetric clipping characteristic)
            sample += 0.05f * warmth * (sample * sample - sample);

            channelData[i] = sample;
        }

        // Apply warmth low-pass filter per channel
        if (ch == 0)
        {
            auto block = juce::dsp::AudioBlock<float>(buffer).getSingleChannelBlock((size_t)ch);
            juce::dsp::ProcessContextReplacing<float> context(block);
            warmthFilterL.process(context);
        }
        else if (ch == 1)
        {
            auto block = juce::dsp::AudioBlock<float>(buffer).getSingleChannelBlock((size_t)ch);
            juce::dsp::ProcessContextReplacing<float> context(block);
            warmthFilterR.process(context);
        }
    }

    // Dry/wet mix
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = buffer.getWritePointer(ch);
        const auto* dryData = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float output = dryData[i] * (1.0f - mix) + wetData[i] * mix;

            // NaN/Inf guard
            if (std::isnan(output) || std::isinf(output))
                output = 0.0f;

            wetData[i] = output;
        }
    }
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
