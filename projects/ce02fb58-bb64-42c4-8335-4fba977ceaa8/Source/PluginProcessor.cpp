#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
CathedralAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(0.5f, 20.0f, 0.1f, 0.4f),
        6.0f,
        "s"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SIZE", 1 },
        "Size",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        75.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DAMPING", 1 },
        "Damping",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        40.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        35.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER_ENABLED", 1 },
        "Shimmer Enabled",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f),
        0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER_TONE", 1 },
        "Shimmer Tone",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        60.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER_AMOUNT", 1 },
        "Shimmer Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        30.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

CathedralAudioProcessor::CathedralAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

CathedralAudioProcessor::~CathedralAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void CathedralAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverb.prepare(spec);
    reverb.reset();

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    shimmerDelayL.prepare(monoSpec);
    shimmerDelayR.prepare(monoSpec);
    shimmerDelayL.reset();
    shimmerDelayR.reset();

    shimmerFilterL.prepare(monoSpec);
    shimmerFilterR.prepare(monoSpec);
    shimmerFilterL.reset();
    shimmerFilterR.reset();

    // Pre-allocate dry buffer
    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);

    shimmerAccumL = 0.0f;
    shimmerAccumR = 0.0f;
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void CathedralAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void CathedralAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters once per block
    auto decayVal       = parameters.getRawParameterValue("DECAY")->load();
    auto sizeVal        = parameters.getRawParameterValue("SIZE")->load() / 100.0f;
    auto dampingVal     = parameters.getRawParameterValue("DAMPING")->load() / 100.0f;
    auto mixVal         = parameters.getRawParameterValue("MIX")->load() / 100.0f;
    auto shimmerOn      = parameters.getRawParameterValue("SHIMMER_ENABLED")->load() > 0.5f;
    auto shimmerTone    = parameters.getRawParameterValue("SHIMMER_TONE")->load() / 100.0f;
    auto shimmerAmount  = parameters.getRawParameterValue("SHIMMER_AMOUNT")->load() / 100.0f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Save dry signal into pre-allocated buffer (no allocation)
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Configure reverb parameters once per block
    juce::dsp::Reverb::Parameters reverbParams;
    // Map decay (0.5–20s) to roomSize (0.0–1.0) with large cathedral character
    reverbParams.roomSize  = juce::jlimit(0.0f, 1.0f, 0.4f + sizeVal * 0.6f);
    reverbParams.damping   = dampingVal;
    reverbParams.width     = 0.8f + sizeVal * 0.2f;
    reverbParams.wetLevel  = 1.0f;
    reverbParams.dryLevel  = 0.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    // Process reverb on the buffer (wet only)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Apply decay-based gain shaping: longer decay = more sustained tail
    float decayGain = juce::jlimit(0.3f, 1.0f, decayVal / 10.0f);

    // Shimmer processing
    if (shimmerOn && numChannels >= 2)
    {
        // Update shimmer filter coefficients once per block
        float shimmerCutoff = 1000.0f + shimmerTone * 8000.0f;
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            currentSampleRate, shimmerCutoff);
        *shimmerFilterL.coefficients = *coeffs;
        *shimmerFilterR.coefficients = *coeffs;

        // Shimmer delay time: roughly one octave pitch-shift illusion
        float shimmerDelaySamples = static_cast<float>(currentSampleRate) / 220.0f;
        shimmerDelayL.setDelay(shimmerDelaySamples);
        shimmerDelayR.setDelay(shimmerDelaySamples * 1.002f); // Slight detune for width

        auto* leftData  = buffer.getWritePointer(0);
        auto* rightData = buffer.getWritePointer(1);

        float feedbackGain = 0.3f + shimmerAmount * 0.45f; // Max 0.75 to prevent runaway

        for (int i = 0; i < numSamples; ++i)
        {
            // Feed wet reverb signal into shimmer delay with feedback
            float delOutL = shimmerDelayL.popSample(0);
            float delOutR = shimmerDelayR.popSample(0);

            // High-pass filter the delayed signal for that airy octave-up character
            float filteredL = shimmerFilterL.processSample(delOutL);
            float filteredR = shimmerFilterR.processSample(delOutR);

            // Accumulate shimmer with soft clipping in the feedback path
            shimmerAccumL = std::tanh(filteredL * feedbackGain + leftData[i] * 0.5f);
            shimmerAccumR = std::tanh(filteredR * feedbackGain + rightData[i] * 0.5f);

            shimmerDelayL.pushSample(0, shimmerAccumL);
            shimmerDelayR.pushSample(0, shimmerAccumR);

            // Mix shimmer into wet signal
            leftData[i]  += filteredL * shimmerAmount;
            rightData[i] += filteredR * shimmerAmount;
        }
    }
    else if (shimmerOn && numChannels == 1)
    {
        float shimmerCutoff = 1000.0f + shimmerTone * 8000.0f;
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            currentSampleRate, shimmerCutoff);
        *shimmerFilterL.coefficients = *coeffs;

        float shimmerDelaySamples = static_cast<float>(currentSampleRate) / 220.0f;
        shimmerDelayL.setDelay(shimmerDelaySamples);

        auto* data = buffer.getWritePointer(0);
        float feedbackGain = 0.3f + shimmerAmount * 0.45f;

        for (int i = 0; i < numSamples; ++i)
        {
            float delOut = shimmerDelayL.popSample(0);
            float filtered = shimmerFilterL.processSample(delOut);
            shimmerAccumL = std::tanh(filtered * feedbackGain + data[i] * 0.5f);
            shimmerDelayL.pushSample(0, shimmerAccumL);
            data[i] += filtered * shimmerAmount;
        }
    }

    // Dry/wet mix with decay shaping
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = buffer.getWritePointer(ch);
        auto* dryData = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float wetSample = wetData[i] * decayGain;
            float output = dryData[i] * (1.0f - mixVal) + wetSample * mixVal;

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

juce::AudioProcessorEditor* CathedralAudioProcessor::createEditor()
{
    return new CathedralAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void CathedralAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CathedralAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new CathedralAudioProcessor();
}
