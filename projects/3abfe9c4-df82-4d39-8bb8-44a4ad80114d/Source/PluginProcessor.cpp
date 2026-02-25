#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
MarinersAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SIZE", 1 },
        "Size",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        65.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        50.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRYWET", 1 },
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        40.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER", 1 },
        "Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f),
        0.0f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

MarinersAudioProcessor::MarinersAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

MarinersAudioProcessor::~MarinersAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void MarinersAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverb.prepare(spec);
    reverb.reset();

    dryWetMixer.prepare(spec);
    dryWetMixer.reset();

    // Shimmer delay: max 2 seconds
    const int maxDelaySamples = static_cast<int>(sampleRate * 2.0);
    shimmerDelay.setMaximumDelayInSamples(maxDelaySamples);
    shimmerDelay.prepare(spec);
    shimmerDelay.reset();

    // Pre-allocate dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);

    // Dark character lowpass filter at ~3kHz
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 3000.0f, 0.7f);
    darkFilterL.coefficients = coeffs;
    darkFilterR.coefficients = coeffs;
    darkFilterL.reset();
    darkFilterR.reset();

    // Reset shimmer accumulators
    shimmerAccumL = 0.0f;
    shimmerAccumR = 0.0f;
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void MarinersAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void MarinersAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters once per block
    auto sizeRaw = parameters.getRawParameterValue("SIZE")->load() / 100.0f;
    auto decayRaw = parameters.getRawParameterValue("DECAY")->load() / 100.0f;
    auto mixVal = parameters.getRawParameterValue("DRYWET")->load() / 100.0f;
    auto shimmerOn = parameters.getRawParameterValue("SHIMMER")->load() >= 0.5f;

    // Scale size to a tamer range (0.2 to 0.85) so it doesn't overwhelm
    float sizeVal = 0.2f + sizeRaw * 0.65f;
    // Scale damping so higher decay = less damping (longer tail)
    float dampVal = 0.6f - decayRaw * 0.45f;

    // Configure reverb parameters once per block
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = sizeVal;
    reverbParams.damping = dampVal;
    reverbParams.wetLevel = 1.0f;   // Always fully wet — DryWetMixer controls blend
    reverbParams.dryLevel = 0.0f;   // Always zero — DryWetMixer controls blend
    reverbParams.width = 1.0f;
    reverb.setParameters(reverbParams);

    // Set dry/wet mix proportion
    dryWetMixer.setWetMixProportion(mixVal);

    // Save dry signal into the DryWetMixer
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    // Process reverb (buffer now contains fully wet reverb output)
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Apply dark character lowpass filter to the wet reverb signal
    auto numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 1)
    {
        auto* leftData = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            leftData[i] = darkFilterL.processSample(leftData[i]);
    }
    if (buffer.getNumChannels() >= 2)
    {
        auto* rightData = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
            rightData[i] = darkFilterR.processSample(rightData[i]);
    }

    // Shimmer processing: adds ethereal octave-up feedback into the wet signal
    if (shimmerOn)
    {
        const float shimmerFeedback = 0.35f;
        const float shimmerGain = 0.3f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* channelData = buffer.getWritePointer(ch);
            float& shimmerAccum = (ch == 0) ? shimmerAccumL : shimmerAccumR;

            for (int i = 0; i < numSamples; ++i)
            {
                float wetSample = channelData[i];

                // Read from delay line (20ms grain for pitch-shifted texture)
                float delaySamples = static_cast<float>(currentSampleRate) * 0.02f;
                float delayed = shimmerDelay.popSample(ch, delaySamples, true);

                // Accumulate with feedback, soft clip to prevent runaway
                shimmerAccum = std::tanh(shimmerAccum * shimmerFeedback + delayed * 0.5f);

                // Push wet reverb output + feedback into delay
                float toDelay = wetSample + shimmerAccum * shimmerFeedback;
                if (std::isnan(toDelay) || std::isinf(toDelay))
                    toDelay = 0.0f;
                shimmerDelay.pushSample(ch, toDelay);

                // Add shimmer to the wet signal (stays in the wet path)
                float output = wetSample + shimmerAccum * shimmerGain;
                if (std::isnan(output) || std::isinf(output))
                    output = 0.0f;

                channelData[i] = output;
            }
        }
    }
    else
    {
        // When shimmer is off, keep the delay line fed with silence
        // so it doesn't produce stale output when re-enabled
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                shimmerDelay.popSample(ch, 1.0f, true);
                shimmerDelay.pushSample(ch, 0.0f);
            }
        }
        shimmerAccumL = 0.0f;
        shimmerAccumR = 0.0f;
    }

    // DryWetMixer blends the saved dry signal with the current buffer (wet + shimmer)
    // At mix = 0, output is 100% dry (no reverb). At mix = 1, output is 100% wet.
    dryWetMixer.mixWetSamples(block);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* MarinersAudioProcessor::createEditor()
{
    return new MarinersAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void MarinersAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MarinersAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new MarinersAudioProcessor();
}
