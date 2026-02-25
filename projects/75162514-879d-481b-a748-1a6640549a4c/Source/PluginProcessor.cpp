#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
UntitledPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Master dry/wet mix
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f, "%"
    ));

    // Reverb parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_DECAY", 1 },
        "Reverb Decay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        65.0f, "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_DEGRADE", 1 },
        "Reverb Degrade",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        40.0f, "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_SHIMMER", 1 },
        "Reverb Shimmer",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        30.0f, "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_TONE", 1 },
        "Reverb Tone",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        45.0f, "%"
    ));

    // Delay parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_TIME", 1 },
        "Delay Time",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f, "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_FEEDBACK", 1 },
        "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        55.0f, "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_WARMTH", 1 },
        "Delay Warmth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        60.0f, "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_WOW", 1 },
        "Delay Wow",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        25.0f, "%"
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
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Prepare reverb
    reverb.prepare(spec);
    reverb.reset();

    // Prepare delay lines
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    delayLineL.prepare(monoSpec);
    delayLineR.prepare(monoSpec);
    delayLineL.reset();
    delayLineR.reset();

    // Prepare filters
    degradeFilterL.prepare(monoSpec);
    degradeFilterR.prepare(monoSpec);
    degradeFilterL.reset();
    degradeFilterR.reset();

    warmthFilterL.prepare(monoSpec);
    warmthFilterR.prepare(monoSpec);
    warmthFilterL.reset();
    warmthFilterR.reset();

    toneFilterL.prepare(monoSpec);
    toneFilterR.prepare(monoSpec);
    toneFilterL.reset();
    toneFilterR.reset();

    // Pre-allocate dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);

    // Reset state
    delayFeedbackStateL = 0.0f;
    delayFeedbackStateR = 0.0f;
    wowPhase = 0.0f;
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

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Read all parameters once per block
    const float mix            = parameters.getRawParameterValue("MIX")->load() / 100.0f;
    const float reverbDecay    = parameters.getRawParameterValue("REVERB_DECAY")->load() / 100.0f;
    const float reverbDegrade  = parameters.getRawParameterValue("REVERB_DEGRADE")->load() / 100.0f;
    const float reverbShimmer  = parameters.getRawParameterValue("REVERB_SHIMMER")->load() / 100.0f;
    const float reverbTone     = parameters.getRawParameterValue("REVERB_TONE")->load() / 100.0f;
    const float delayTime      = parameters.getRawParameterValue("DELAY_TIME")->load() / 100.0f;
    const float delayFeedback  = parameters.getRawParameterValue("DELAY_FEEDBACK")->load() / 100.0f;
    const float delayWarmth    = parameters.getRawParameterValue("DELAY_WARMTH")->load() / 100.0f;
    const float delayWow       = parameters.getRawParameterValue("DELAY_WOW")->load() / 100.0f;

    // Save dry signal (using pre-allocated buffer)
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // ── REVERB ──────────────────────────────────────────────────
    // Set reverb parameters once per block
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize   = 0.4f + reverbDecay * 0.58f;       // 0.4 – 0.98
    reverbParams.damping    = 1.0f - reverbTone;                 // brighter tone = less damping
    reverbParams.wetLevel   = 1.0f;
    reverbParams.dryLevel   = 0.0f;
    reverbParams.width      = 0.8f + reverbShimmer * 0.2f;      // wider with shimmer
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    // Create a copy for reverb processing (work on buffer, keep dry separate)
    juce::dsp::AudioBlock<float> reverbBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> reverbContext(reverbBlock);
    reverb.process(reverbContext);

    // Lo-fi degradation: lowpass filter on reverb output to crush highs
    // Frequency range: 1500 Hz (full degrade) to 18000 Hz (clean)
    float degradeFreq = 18000.0f - reverbDegrade * 16500.0f;
    degradeFreq = juce::jlimit(200.0f, 18000.0f, degradeFreq);
    auto degradeCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, degradeFreq, 0.7f);
    *degradeFilterL.coefficients = *degradeCoeffs;
    *degradeFilterR.coefficients = *degradeCoeffs;

    // Tone filter on reverb (additional shaping)
    float toneFreq = 800.0f + reverbTone * 14200.0f;  // 800 Hz to 15000 Hz
    auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, toneFreq, 0.5f);
    *toneFilterL.coefficients = *toneCoeffs;
    *toneFilterR.coefficients = *toneCoeffs;

    // Apply degradation and tone filtering per-sample on reverb output
    if (numChannels >= 2)
    {
        auto* leftData  = buffer.getWritePointer(0);
        auto* rightData = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            leftData[i]  = degradeFilterL.processSample(leftData[i]);
            rightData[i] = degradeFilterR.processSample(rightData[i]);
            leftData[i]  = toneFilterL.processSample(leftData[i]);
            rightData[i] = toneFilterR.processSample(rightData[i]);

            // Shimmer: subtle pitch-up emulation via feedback accumulation
            float shimmerGain = reverbShimmer * 0.15f;
            leftData[i]  += std::tanh(leftData[i] * 1.5f) * shimmerGain;
            rightData[i] += std::tanh(rightData[i] * 1.5f) * shimmerGain;
        }
    }
    else if (numChannels == 1)
    {
        auto* data = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
        {
            data[i] = degradeFilterL.processSample(data[i]);
            data[i] = toneFilterL.processSample(data[i]);
            data[i] += std::tanh(data[i] * 1.5f) * reverbShimmer * 0.15f;
        }
    }

    // Store reverb output
    juce::AudioBuffer<float> reverbOutput;
    // Use the buffer as reverb output — we'll need to combine later
    // Actually, let's work in-place: buffer now holds wet reverb signal.

    // ── DELAY (tape-style) ──────────────────────────────────────
    // Delay time: 50ms to 1200ms mapped from 0-1
    float delayTimeSec = 0.05f + delayTime * 1.15f;
    float delaySamplesBase = static_cast<float>(delayTimeSec * currentSampleRate);

    // Feedback clamped to prevent runaway
    float fbGain = delayFeedback * 0.85f;  // max 85% feedback

    // Warmth filter: lowpass in feedback path
    float warmthFreq = 12000.0f - delayWarmth * 10000.0f;  // 2000–12000 Hz
    warmthFreq = juce::jlimit(500.0f, 12000.0f, warmthFreq);
    auto warmthCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, warmthFreq, 0.6f);
    *warmthFilterL.coefficients = *warmthCoeffs;
    *warmthFilterR.coefficients = *warmthCoeffs;

    // Wow: tape speed modulation LFO
    float wowRate = 0.3f + delayWow * 2.7f;   // 0.3–3 Hz
    float wowDepth = delayWow * 0.003f;        // subtle pitch wobble

    if (numChannels >= 2)
    {
        auto* leftData  = buffer.getWritePointer(0);
        auto* rightData = buffer.getWritePointer(1);
        auto* dryL      = dryBuffer.getReadPointer(0);
        auto* dryR      = dryBuffer.getReadPointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            // LFO for tape wow
            float wowMod = std::sin(wowPhase * juce::MathConstants<float>::twoPi);
            wowPhase += wowRate / static_cast<float>(currentSampleRate);
            if (wowPhase >= 1.0f) wowPhase -= 1.0f;

            float modulatedDelay = delaySamplesBase * (1.0f + wowMod * wowDepth);
            modulatedDelay = juce::jlimit(1.0f, static_cast<float>(delayLineL.getMaximumDelayInSamples() - 1), modulatedDelay);

            // Read from delay lines
            float delayedL = delayLineL.popSample(0, modulatedDelay);
            float delayedR = delayLineR.popSample(0, modulatedDelay);

            // Apply warmth filter in feedback path
            float feedbackL = warmthFilterL.processSample(delayedL);
            float feedbackR = warmthFilterR.processSample(delayedR);

            // Soft clip feedback to prevent blowup
            feedbackL = std::tanh(feedbackL);
            feedbackR = std::tanh(feedbackR);

            // Push to delay lines: input + feedback
            delayLineL.pushSample(0, dryL[i] + feedbackL * fbGain);
            delayLineR.pushSample(0, dryR[i] + feedbackR * fbGain);

            // Combine: reverb output (already in buffer) + delay output
            float wetL = leftData[i] + delayedL;
            float wetR = rightData[i] + delayedR;

            // Mix dry and wet
            float outL = dryL[i] * (1.0f - mix) + wetL * mix;
            float outR = dryR[i] * (1.0f - mix) + wetR * mix;

            // NaN/Inf guard
            if (std::isnan(outL) || std::isinf(outL)) outL = 0.0f;
            if (std::isnan(outR) || std::isinf(outR)) outR = 0.0f;

            leftData[i]  = outL;
            rightData[i] = outR;
        }
    }
    else if (numChannels == 1)
    {
        auto* data = buffer.getWritePointer(0);
        auto* dry  = dryBuffer.getReadPointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            float wowMod = std::sin(wowPhase * juce::MathConstants<float>::twoPi);
            wowPhase += wowRate / static_cast<float>(currentSampleRate);
            if (wowPhase >= 1.0f) wowPhase -= 1.0f;

            float modulatedDelay = delaySamplesBase * (1.0f + wowMod * wowDepth);
            modulatedDelay = juce::jlimit(1.0f, static_cast<float>(delayLineL.getMaximumDelayInSamples() - 1), modulatedDelay);

            float delayed = delayLineL.popSample(0, modulatedDelay);
            float feedback = warmthFilterL.processSample(delayed);
            feedback = std::tanh(feedback);
            delayLineL.pushSample(0, dry[i] + feedback * fbGain);

            float wet = data[i] + delayed;
            float out = dry[i] * (1.0f - mix) + wet * mix;

            if (std::isnan(out) || std::isinf(out)) out = 0.0f;
            data[i] = out;
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
