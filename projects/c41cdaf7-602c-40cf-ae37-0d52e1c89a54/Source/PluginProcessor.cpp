#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SpectralAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.35f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.55f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER", 1 },
        "Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.50f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SpectralAudioProcessor::SpectralAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SpectralAudioProcessor::~SpectralAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void SpectralAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Prepare reverb
    reverb.prepare(spec);
    reverb.reset();

    // Prepare dry/wet mixer
    dryWetMixer.prepare(spec);
    dryWetMixer.reset();

    // Prepare shimmer delay line (used for shimmer feedback path)
    shimmerDelay.setMaximumDelayInSamples(static_cast<int>(sampleRate * 2.0));
    shimmerDelay.prepare(spec);
    shimmerDelay.reset();

    // Prepare shimmer pitch shifter
    shimmerPitchShift.prepare(sampleRate, samplesPerBlock, static_cast<int>(getTotalNumOutputChannels()));
    shimmerPitchShift.reset();

    // Pre-allocate shimmer feedback buffer
    shimmerBuffer.setSize(static_cast<int>(getTotalNumOutputChannels()), samplesPerBlock);
    shimmerBuffer.clear();

    // Initialize smoothed values
    shimmerAmountSmoothed.reset(sampleRate, 0.05);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void SpectralAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void SpectralAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters
    auto mix = parameters.getRawParameterValue("MIX")->load();
    auto decay = parameters.getRawParameterValue("DECAY")->load();
    auto shimmerAmount = parameters.getRawParameterValue("SHIMMER")->load();

    // Configure reverb — dark & ethereal character
    // Decay parameter maps to roomSize (0.5 to 1.0 range for long tails)
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 0.5f + decay * 0.5f;
    reverbParams.damping = 0.6f + (1.0f - decay) * 0.3f;  // More damping at lower decay = darker
    reverbParams.wetLevel = 1.0f;   // Fully wet — DryWetMixer handles blend
    reverbParams.dryLevel = 0.0f;   // Fully wet — DryWetMixer handles blend
    reverbParams.width = 1.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    // Set dry/wet mix
    dryWetMixer.setWetMixProportion(mix);

    // Set shimmer smoothing target
    shimmerAmountSmoothed.setTargetValue(shimmerAmount);

    // Push dry signal into mixer
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    // Shimmer feedback: inject pitch-shifted reverb tail back into the input
    // Process sample-by-sample for the shimmer feedback path
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const float shimmerDelayMs = 30.0f; // Short delay for shimmer feedback
    const float shimmerDelaySamples = shimmerDelayMs * static_cast<float>(currentSampleRate) / 1000.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float currentShimmer = shimmerAmountSmoothed.getNextValue();

            // Read the delayed signal (reverb tail from previous blocks)
            float delayedSample = shimmerDelay.popSample(ch, shimmerDelaySamples, true);

            // Pitch-shift the delayed signal up one octave
            float pitchedSample = shimmerPitchShift.processSample(ch, delayedSample);

            // Mix the pitch-shifted feedback into the input
            float inputWithShimmer = channelData[i] + std::tanh(pitchedSample * currentShimmer * 0.7f);

            // NaN/Inf guard
            if (std::isnan(inputWithShimmer) || std::isinf(inputWithShimmer))
                inputWithShimmer = channelData[i];

            channelData[i] = inputWithShimmer;

            // Push the current sample into shimmer delay for feedback
            shimmerDelay.pushSample(ch, channelData[i]);
        }
    }

    // Process reverb on the full block (input now includes shimmer feedback)
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Blend wet reverb with dry signal
    dryWetMixer.mixWetSamples(block);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* SpectralAudioProcessor::createEditor()
{
    return new SpectralAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void SpectralAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SpectralAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new SpectralAudioProcessor();
}
