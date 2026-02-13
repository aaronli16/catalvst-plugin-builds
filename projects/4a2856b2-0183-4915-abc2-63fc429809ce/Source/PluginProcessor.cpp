#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
AmbientBuildupOneknobProcessorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BUILDUP", 1 },
        "Buildup",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

AmbientBuildupOneknobProcessorAudioProcessor::AmbientBuildupOneknobProcessorAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

AmbientBuildupOneknobProcessorAudioProcessor::~AmbientBuildupOneknobProcessorAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play
// ==============================================================================

void AmbientBuildupOneknobProcessorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Reverb
    reverb.prepare(spec);
    reverb.reset();

    // Delay lines
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    delayLineL.prepare(monoSpec);
    delayLineR.prepare(monoSpec);
    delayLineL.reset();
    delayLineR.reset();
    delayFeedbackL = 0.0f;
    delayFeedbackR = 0.0f;

    // High-pass filters
    hpfL.prepare(monoSpec);
    hpfR.prepare(monoSpec);
    hpfL.reset();
    hpfR.reset();
    hpfL.setType(juce::dsp::StateVariableTPTFilter<float>::Type::highpass);
    hpfR.setType(juce::dsp::StateVariableTPTFilter<float>::Type::highpass);

    // Smoothed value for zipper-free control
    smoothedBuildup.reset(sampleRate, 0.05); // 50ms smoothing
    smoothedBuildup.setCurrentAndTargetValue(0.0f);
}

// ==============================================================================
// Release Resources
// ==============================================================================

void AmbientBuildupOneknobProcessorAudioProcessor::releaseResources()
{
}

// ==============================================================================
// Process Block
// ==============================================================================

void AmbientBuildupOneknobProcessorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any extra output channels
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    // Read the single buildup parameter
    auto buildupRaw = parameters.getRawParameterValue("BUILDUP")->load();
    smoothedBuildup.setTargetValue(buildupRaw);

    // Get write pointers
    float* channelL = numChannels > 0 ? buffer.getWritePointer(0) : nullptr;
    float* channelR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float t = smoothedBuildup.getNextValue();

        // === Parameter Mapping (smooth, gradual, reverb-dominant) ===

        // Noise: starts at 10%, ramps up - bright & airy level
        float noiseAmount = juce::jmax(0.0f, (t - 0.1f) / 0.9f);
        noiseAmount *= noiseAmount * 0.15f; // Squared curve, max level 0.15

        // Delay: starts at 15%, feedback builds
        float delayMix = juce::jmax(0.0f, (t - 0.15f) / 0.85f);
        float delayTime = 0.15f + delayMix * 0.25f; // 150ms to 400ms
        float delayFeedback = delayMix * 0.65f; // Max 65% feedback

        // HPF: starts at 5%, sweeps from 20Hz up to 4kHz
        float hpfAmount = juce::jmax(0.0f, (t - 0.05f) / 0.95f);
        float hpfFreq = 20.0f + hpfAmount * hpfAmount * 3980.0f; // Squared curve to 4kHz

        // === Generate white noise ===
        float noiseL = noiseRandom.nextFloat() * 2.0f - 1.0f;
        float noiseR = noiseRandom.nextFloat() * 2.0f - 1.0f;

        // === Process each sample ===
        float sampleL = channelL ? channelL[i] : 0.0f;
        float sampleR = channelR ? (numChannels > 1 ? channelR[i] : sampleL) : sampleL;

        // Add noise
        sampleL += noiseL * noiseAmount;
        sampleR += noiseR * noiseAmount;

        // === Delay processing (ping-pong style) ===
        float delaySamplesL = static_cast<float>(delayTime * currentSampleRate);
        float delaySamplesR = static_cast<float>(delayTime * 1.13f * currentSampleRate); // Slightly offset for stereo width

        delayLineL.setDelay(delaySamplesL);
        delayLineR.setDelay(delaySamplesR);

        float delayedL = delayLineL.popSample(0);
        float delayedR = delayLineR.popSample(0);

        // Push with feedback
        delayLineL.pushSample(0, sampleL + delayedR * delayFeedback);
        delayLineR.pushSample(0, sampleR + delayedL * delayFeedback);

        // Mix delay in
        sampleL += delayedL * delayMix * 0.5f;
        sampleR += delayedR * delayMix * 0.5f;

        // === High-pass filter ===
        hpfL.setCutoffFrequency(hpfFreq);
        hpfR.setCutoffFrequency(hpfFreq);

        sampleL = hpfL.processSample(0, sampleL);
        sampleR = hpfR.processSample(0, sampleR);

        // Write back
        if (channelL) channelL[i] = sampleL;
        if (channelR) channelR[i] = sampleR;
    }

    // === Reverb (block-based processing) ===
    // Map reverb: starts immediately, gets huge
    float reverbAmount = smoothedBuildup.getCurrentValue();

    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 0.3f + reverbAmount * 0.7f;       // 0.3 to 1.0
    reverbParams.damping = 0.5f - reverbAmount * 0.35f;        // 0.5 to 0.15 (less damping = brighter)
    reverbParams.wetLevel = reverbAmount * 0.75f;               // 0 to 0.75
    reverbParams.dryLevel = 1.0f - reverbAmount * 0.3f;        // 1.0 to 0.7
    reverbParams.width = 0.5f + reverbAmount * 0.5f;           // 0.5 to 1.0
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Soft clip to prevent harsh digital clipping
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            data[i] = std::tanh(data[i]);
        }
    }
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* AmbientBuildupOneknobProcessorAudioProcessor::createEditor()
{
    return new AmbientBuildupOneknobProcessorAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
// ==============================================================================

void AmbientBuildupOneknobProcessorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AmbientBuildupOneknobProcessorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ==============================================================================
// Factory Function
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmbientBuildupOneknobProcessorAudioProcessor();
}
