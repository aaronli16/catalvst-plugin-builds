#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
RiserXAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Main intensity knob (0-100% mapped to 0.0-1.0)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "INTENSITY", 1 },
        "Intensity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f,
        "%"
    ));

    // Tempo sync bar count (1, 2, 4, 8, 16)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "SYNC_BARS", 1 },
        "Sync Bars",
        juce::StringArray { "1", "2", "4", "8", "16" },
        2  // default = 4 bars
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

RiserXAudioProcessor::RiserXAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

RiserXAudioProcessor::~RiserXAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void RiserXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Reverb
    reverb.prepare(spec);
    reverb.reset();

    // Main delay line (up to ~2 seconds at 44100)
    delayLine.prepare(spec);
    delayLine.reset();

    // Flanger delay lines (mono spec)
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    flangerDelayL.prepare(monoSpec);
    flangerDelayL.reset();
    flangerDelayR.prepare(monoSpec);
    flangerDelayR.reset();

    // High-pass filter
    hpFilter.prepare(spec);
    hpFilter.reset();
    hpFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);

    // Pre-allocate dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);

    // Reset feedback accumulators
    delayFeedbackL = 0.0f;
    delayFeedbackR = 0.0f;
    flangerPhase = 0.0f;
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void RiserXAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void RiserXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters once per block
    const float intensity = parameters.getRawParameterValue("INTENSITY")->load();
    const int syncChoice = static_cast<int>(parameters.getRawParameterValue("SYNC_BARS")->load());

    // If intensity is zero, pass through dry signal
    if (intensity < 0.001f)
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Exponential curves for each effect (aggressive build character)
    const float t = intensity;
    const float reverbAmount   = std::pow(t, 1.5f);
    const float delayAmount    = std::pow(t, 2.0f);
    const float flangerAmount  = std::pow(t, 2.5f);
    const float filterAmount   = std::pow(t, 1.8f);
    const float driveAmount    = std::pow(t, 2.2f);

    // --- Reverb parameters (once per block) ---
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize   = 0.3f + reverbAmount * 0.7f;
    reverbParams.damping    = 0.8f - reverbAmount * 0.6f;
    reverbParams.wetLevel   = reverbAmount * 0.6f;
    reverbParams.dryLevel   = 1.0f - reverbAmount * 0.3f;
    reverbParams.width      = 0.5f + reverbAmount * 0.5f;
    reverb.setParameters(reverbParams);

    // --- High-pass filter sweep: 20 Hz -> 4000 Hz with intensity ---
    const float hpFreq = 20.0f + filterAmount * 3980.0f;
    hpFilter.setCutoffFrequency(hpFreq);
    hpFilter.setResonance(0.7f + filterAmount * 1.8f);

    // --- Delay time based on sync bars choice ---
    // Sync values: 0=1bar, 1=2bar, 2=4bar, 3=8bar, 4=16bar
    // We use a shorter delay that subdivides. At high intensity -> shorter ping-pong
    const float delayTimeSamples = static_cast<float>(currentSampleRate) *
        (0.05f + (1.0f - delayAmount) * 0.35f); // 50ms to 400ms range

    // --- Save dry signal into pre-allocated buffer ---
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // --- Per-sample processing ---
    const float flangerRate = 0.2f + flangerAmount * 4.0f; // LFO rate in Hz
    const float flangerDepth = 0.5f + flangerAmount * 3.0f; // depth in ms
    const float phaseInc = flangerRate / static_cast<float>(currentSampleRate);

    float* leftData  = (numChannels > 0) ? buffer.getWritePointer(0) : nullptr;
    float* rightData = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // --- Flanger LFO ---
        float lfoL = std::sin(flangerPhase * juce::MathConstants<float>::twoPi);
        float lfoR = std::sin((flangerPhase + 0.25f) * juce::MathConstants<float>::twoPi); // stereo offset
        flangerPhase += phaseInc;
        if (flangerPhase >= 1.0f)
            flangerPhase -= 1.0f;

        float flangerDelayMsL = 1.0f + flangerDepth * (0.5f + 0.5f * lfoL);
        float flangerDelayMsR = 1.0f + flangerDepth * (0.5f + 0.5f * lfoR);
        float flangerDelaySamplesL = flangerDelayMsL * 0.001f * static_cast<float>(currentSampleRate);
        float flangerDelaySamplesR = flangerDelayMsR * 0.001f * static_cast<float>(currentSampleRate);

        // Process left channel
        if (leftData != nullptr)
        {
            float dry = leftData[i];

            // Delay with feedback
            float delayed = delayLine.popSample(0, delayTimeSamples);
            delayFeedbackL = std::tanh(delayed * (0.3f + delayAmount * 0.55f));
            delayLine.pushSample(0, dry + delayFeedbackL);
            float withDelay = dry + delayed * delayAmount;

            // Flanger
            float flanged = flangerDelayL.popSample(0, flangerDelaySamplesL);
            flangerDelayL.pushSample(0, withDelay);
            float withFlanger = withDelay + flanged * flangerAmount * 0.7f;

            // Saturation / distortion (aggressive character)
            float driven = withFlanger * (1.0f + driveAmount * 6.0f);
            driven = std::tanh(driven);

            leftData[i] = driven;
        }

        // Process right channel
        if (rightData != nullptr)
        {
            float dry = rightData[i];

            float delayed = delayLine.popSample(1, delayTimeSamples * 0.75f);
            delayFeedbackR = std::tanh(delayed * (0.3f + delayAmount * 0.55f));
            delayLine.pushSample(1, dry + delayFeedbackR);
            float withDelay = dry + delayed * delayAmount;

            float flanged = flangerDelayR.popSample(0, flangerDelaySamplesR);
            flangerDelayR.pushSample(0, withDelay);
            float withFlanger = withDelay + flanged * flangerAmount * 0.7f;

            float driven = withFlanger * (1.0f + driveAmount * 6.0f);
            driven = std::tanh(driven);

            rightData[i] = driven;
        }
    }

    // --- Apply reverb to the full block ---
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);
    }

    // --- Apply high-pass filter sweep ---
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        hpFilter.process(context);
    }

    // --- Final NaN/Inf guard ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::isnan(data[i]) || std::isinf(data[i]))
                data[i] = 0.0f;
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* RiserXAudioProcessor::createEditor()
{
    return new RiserXAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void RiserXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void RiserXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new RiserXAudioProcessor();
}
