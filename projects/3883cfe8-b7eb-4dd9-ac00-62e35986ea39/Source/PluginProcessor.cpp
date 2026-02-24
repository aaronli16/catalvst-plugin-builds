#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
RetroverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ROBOT", 1 },
        "Robot Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BLEND", 1 },
        "Vocoder / Chorus Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Wet / Dry",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.75f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

RetroverbAudioProcessor::RetroverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

RetroverbAudioProcessor::~RetroverbAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void RetroverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    // Compute logarithmically spaced band frequencies (200 Hz to 8000 Hz)
    for (int i = 0; i < numVocoderBands; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numVocoderBands - 1);
        bandFrequencies[static_cast<size_t>(i)] = 200.0f * std::pow(40.0f, t); // 200 to 8000 Hz
    }

    // Prepare vocoder filter banks
    for (int i = 0; i < numVocoderBands; ++i)
    {
        auto si = static_cast<size_t>(i);
        analysisBandsL[si].prepare(spec);
        analysisBandsR[si].prepare(spec);
        synthesisBandsL[si].prepare(spec);
        synthesisBandsR[si].prepare(spec);
        analysisBandsL[si].reset();
        analysisBandsR[si].reset();
        synthesisBandsL[si].reset();
        synthesisBandsR[si].reset();
        envelopeL[si] = 0.0f;
        envelopeR[si] = 0.0f;
    }

    updateVocoderCoefficients();

    // Prepare chorus delay lines
    chorusDelayL.prepare(spec);
    chorusDelayR.prepare(spec);
    chorusDelayL.reset();
    chorusDelayR.reset();
    chorusPhaseL = 0.0f;
    chorusPhaseR = 0.25f; // Offset for stereo width

    // Pre-allocate dry buffer
    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);

    // Setup smoothed values
    robotSmoothed.reset(sampleRate, 0.02);
    blendSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void RetroverbAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void RetroverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters once per block
    auto robotParam = parameters.getRawParameterValue("ROBOT")->load();
    auto blendParam = parameters.getRawParameterValue("BLEND")->load();
    auto mixParam   = parameters.getRawParameterValue("MIX")->load();

    robotSmoothed.setTargetValue(robotParam);
    blendSmoothed.setTargetValue(blendParam);
    mixSmoothed.setTargetValue(mixParam);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Store dry signal in pre-allocated buffer
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Chorus LFO parameters
    const float chorusRate   = 0.8f;   // Hz
    const float chorusDepth  = 0.003f; // seconds
    const float chorusCenter = 0.007f; // seconds
    const float lfoInc = chorusRate / static_cast<float>(currentSampleRate);

    // Envelope follower coefficients
    const float envAttack  = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.002f));
    const float envRelease = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.015f));

    for (int i = 0; i < numSamples; ++i)
    {
        const float robot = robotSmoothed.getNextValue();
        const float blend = blendSmoothed.getNextValue();
        const float mix   = mixSmoothed.getNextValue();

        // Vocoder amount scales with robot * (1 - blend)
        const float vocoderAmt = robot * (1.0f - blend);
        // Chorus amount scales with robot * blend
        const float chorusAmt  = robot * blend;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float inputSample = buffer.getReadPointer(ch)[i];
            float drySample   = dryBuffer.getReadPointer(ch)[i];

            // ---- Vocoder processing ----
            float vocoderOut = 0.0f;

            auto& analysisBands  = (ch == 0) ? analysisBandsL  : analysisBandsR;
            auto& synthesisBands = (ch == 0) ? synthesisBandsL  : synthesisBandsR;
            auto& envelopes      = (ch == 0) ? envelopeL        : envelopeR;

            for (int b = 0; b < numVocoderBands; ++b)
            {
                auto sb = static_cast<size_t>(b);
                // Analysis: bandpass filter the input
                float bandSample = analysisBands[sb].processSample(inputSample);

                // Envelope follower
                float rectified = std::abs(bandSample);
                float coeff = (rectified > envelopes[sb]) ? envAttack : envRelease;
                envelopes[sb] = envelopes[sb] * coeff + rectified * (1.0f - coeff);

                // Synthesis: apply envelope to a buzz/noise carrier
                // Use a simple square-ish carrier derived from the band signal itself
                float carrier = (bandSample >= 0.0f) ? 1.0f : -1.0f;
                float synthSample = synthesisBands[sb].processSample(carrier * envelopes[sb]);

                vocoderOut += synthSample;
            }

            // Normalize vocoder output
            vocoderOut /= static_cast<float>(numVocoderBands) * 0.5f;

            // ---- Chorus processing ----
            auto& chorusDelay = (ch == 0) ? chorusDelayL : chorusDelayR;
            auto& chorusPhase = (ch == 0) ? chorusPhaseL : chorusPhaseR;

            chorusDelay.pushSample(0, inputSample);

            float lfoValue = std::sin(2.0f * juce::MathConstants<float>::pi * chorusPhase);
            float delaySec = chorusCenter + chorusDepth * lfoValue;
            float delaySamples = delaySec * static_cast<float>(currentSampleRate);
            delaySamples = juce::jlimit(1.0f, 4400.0f, delaySamples);

            float chorusOut = chorusDelay.popSample(0, delaySamples);

            chorusPhase += lfoInc;
            if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;

            // Mix chorus: original + delayed for classic chorus sound
            float chorusMixed = inputSample * 0.7f + chorusOut * 0.7f;

            // ---- Blend vocoder and chorus ----
            float wetSignal = vocoderOut * vocoderAmt + chorusMixed * chorusAmt;

            // When robot is near zero, wet signal fades to near-dry
            float finalWet = wetSignal;

            // ---- Wet/Dry mix ----
            float output = drySample * (1.0f - mix) + finalWet * mix;

            // NaN/Inf guard
            if (std::isnan(output) || std::isinf(output))
                output = 0.0f;

            buffer.getWritePointer(ch)[i] = output;
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* RetroverbAudioProcessor::createEditor()
{
    return new RetroverbAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void RetroverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void RetroverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ==============================================================================
// Vocoder Coefficient Update
// ==============================================================================

void RetroverbAudioProcessor::updateVocoderCoefficients()
{
    float q = 8.0f; // Narrow bandpass Q for vocoder character

    for (int i = 0; i < numVocoderBands; ++i)
    {
        auto si = static_cast<size_t>(i);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            currentSampleRate, static_cast<double>(bandFrequencies[si]), q);

        *analysisBandsL[si].coefficients  = *coeffs;
        *analysisBandsR[si].coefficients  = *coeffs;
        *synthesisBandsL[si].coefficients = *coeffs;
        *synthesisBandsR[si].coefficients = *coeffs;
    }
}

// ==============================================================================
// Factory Function - Required by JUCE plugin system
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RetroverbAudioProcessor();
}
