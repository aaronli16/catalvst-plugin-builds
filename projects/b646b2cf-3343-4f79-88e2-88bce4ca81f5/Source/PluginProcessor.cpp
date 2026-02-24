#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
UntitledPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Threshold: -60 to 0 dB, default -20 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "THRESHOLD", 1 },
        "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
        -20.0f,
        "dB"
    ));

    // Drive: 0 to 100%, default 30%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 },
        "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        30.0f,
        "%"
    ));

    // Crush: 0 to 100%, default 0%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CRUSH", 1 },
        "Crush",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f,
        "%"
    ));

    // Mix: 0 to 100%, default 100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        "%"
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
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    compressor.prepare(spec);
    compressor.reset();

    // Pre-allocate dry buffer for mix blending
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
    auto thresholdDB = parameters.getRawParameterValue("THRESHOLD")->load();
    auto drivePercent = parameters.getRawParameterValue("DRIVE")->load();
    auto crushPercent = parameters.getRawParameterValue("CRUSH")->load();
    auto mixPercent = parameters.getRawParameterValue("MIX")->load();

    const float mix = mixPercent / 100.0f;
    const float driveAmount = drivePercent / 100.0f;
    const float crushAmount = crushPercent / 100.0f;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Save dry signal into pre-allocated buffer (no allocation)
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // --- Stage 1: Compression ---
    compressor.setThreshold(thresholdDB);
    compressor.setRatio(4.0f);
    compressor.setAttack(10.0f);
    compressor.setRelease(100.0f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);

    // --- Stage 2: Saturation (waveshaping via tanh) ---
    if (driveAmount > 0.001f)
    {
        // Map drive 0-1 to gain 1-20 for saturation intensity
        const float driveGain = 1.0f + driveAmount * 19.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = data[i] * driveGain;
                sample = std::tanh(sample);
                data[i] = sample;
            }
        }
    }

    // --- Stage 3: Bit Crush (sample rate + bit depth reduction) ---
    if (crushAmount > 0.01f)
    {
        // Map crush 0-1 to bit depth 16 down to 2
        const float bitDepth = 16.0f - crushAmount * 14.0f;
        const float levels = std::pow(2.0f, bitDepth);

        // Map crush 0-1 to sample-hold factor 1 to 40
        const float holdFactor = 1.0f + crushAmount * 39.0f;
        const int holdInterval = std::max(1, static_cast<int>(holdFactor));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float heldSample = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                if (i % holdInterval == 0)
                {
                    // Quantize to reduced bit depth
                    heldSample = std::round(data[i] * levels) / levels;
                }
                data[i] = heldSample;
            }
        }
    }

    // --- Stage 4: Dry/Wet Mix ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = buffer.getWritePointer(ch);
        const auto* dryData = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float output = dryData[i] * (1.0f - mix) + wetData[i] * mix;

            // NaN/Inf safety guard
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
