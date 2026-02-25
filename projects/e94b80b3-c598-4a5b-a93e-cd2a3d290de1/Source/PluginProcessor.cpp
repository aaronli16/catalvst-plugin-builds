#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
UntitledPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // === Pitch Correction ===
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PITCH_SPEED", 1 }, "Pitch Speed",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 42.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PITCH_AMOUNT", 1 }, "Pitch Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 65.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PITCH_GLIDE", 1 }, "Pitch Glide",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 55.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PITCH_ENABLED", 1 }, "Pitch Enabled",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f));

    // === Reverb ===
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_SIZE", 1 }, "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 60.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_DECAY", 1 }, "Reverb Decay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_DAMP", 1 }, "Reverb Damp",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 45.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_WIDTH", 1 }, "Reverb Width",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 70.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_ENABLED", 1 }, "Reverb Enabled",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f));

    // === Delay ===
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_TIME", 1 }, "Delay Time",
        juce::NormalisableRange<float>(1.0f, 1000.0f, 0.1f), 320.0f, "ms"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_FEEDBACK", 1 }, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_FILTER", 1 }, "Delay Filter",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_ENABLED", 1 }, "Delay Enabled",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f));

    // === Mix / Output ===
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRY_WET", 1 }, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 55.0f, "%"));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "OUTPUT", 1 }, "Output",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f, "dB"));

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

    // Reverb
    reverb.prepare(spec);
    reverb.reset();

    // Delay lines
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    delayLineL.prepare(monoSpec);
    delayLineL.reset();
    delayLineR.prepare(monoSpec);
    delayLineR.reset();

    // Delay filters
    delayLPF_L.prepare(monoSpec);
    delayLPF_L.reset();
    delayLPF_R.prepare(monoSpec);
    delayLPF_R.reset();
    delayHPF_L.prepare(monoSpec);
    delayHPF_L.reset();
    delayHPF_R.prepare(monoSpec);
    delayHPF_R.reset();

    // Pre-allocate dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);
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

    // Read all parameters once per block
    auto pitchSpeed    = parameters.getRawParameterValue("PITCH_SPEED")->load();
    auto pitchAmount   = parameters.getRawParameterValue("PITCH_AMOUNT")->load();
    auto pitchGlide    = parameters.getRawParameterValue("PITCH_GLIDE")->load();
    auto pitchEnabled  = parameters.getRawParameterValue("PITCH_ENABLED")->load() > 0.5f;

    auto reverbSize    = parameters.getRawParameterValue("REVERB_SIZE")->load();
    auto reverbDecay   = parameters.getRawParameterValue("REVERB_DECAY")->load();
    auto reverbDamp    = parameters.getRawParameterValue("REVERB_DAMP")->load();
    auto reverbWidth   = parameters.getRawParameterValue("REVERB_WIDTH")->load();
    auto reverbEnabled = parameters.getRawParameterValue("REVERB_ENABLED")->load() > 0.5f;

    auto delayTime     = parameters.getRawParameterValue("DELAY_TIME")->load();
    auto delayFeedback = parameters.getRawParameterValue("DELAY_FEEDBACK")->load();
    auto delayFilter   = parameters.getRawParameterValue("DELAY_FILTER")->load();
    auto delayEnabled  = parameters.getRawParameterValue("DELAY_ENABLED")->load() > 0.5f;

    auto dryWet        = parameters.getRawParameterValue("DRY_WET")->load() / 100.0f;
    auto outputDB      = parameters.getRawParameterValue("OUTPUT")->load();
    auto outputGain    = juce::Decibels::decibelsToGain(outputDB);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    juce::ignoreUnused(pitchSpeed, pitchAmount, pitchGlide);

    // ---- Store dry signal (pre-allocated buffer, no heap alloc) ----
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // ---- Pitch Correction (spectral placeholder: subtle saturation + formant emphasis) ----
    if (pitchEnabled)
    {
        float satDrive = 1.0f + (pitchAmount / 100.0f) * 0.3f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = data[i] * satDrive;
                sample = std::tanh(sample);
                data[i] = sample;
            }
        }
    }

    // ---- Reverb processing ----
    if (reverbEnabled)
    {
        juce::dsp::Reverb::Parameters reverbParams;
        reverbParams.roomSize   = reverbSize / 100.0f;
        reverbParams.damping    = reverbDamp / 100.0f;
        reverbParams.wetLevel   = reverbDecay / 100.0f;
        reverbParams.dryLevel   = 1.0f - (reverbDecay / 100.0f) * 0.5f;
        reverbParams.width      = reverbWidth / 100.0f;
        reverbParams.freezeMode = 0.0f;
        reverb.setParameters(reverbParams);

        juce::dsp::AudioBlock<float> reverbBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> reverbContext(reverbBlock);
        reverb.process(reverbContext);
    }

    // ---- Delay processing ----
    if (delayEnabled)
    {
        float delaySamples = (delayTime / 1000.0f) * static_cast<float>(currentSampleRate);
        float fbGain = (delayFeedback / 100.0f) * 0.85f; // cap at 0.85 for safety

        // Compute filter cutoff from delayFilter (0-100):
        // 0 = dark (LPF at 800Hz), 50 = neutral, 100 = bright (LPF at 12kHz)
        float lpfFreq = 800.0f + (delayFilter / 100.0f) * 11200.0f;
        float hpfFreq = 80.0f + ((100.0f - delayFilter) / 100.0f) * 400.0f;

        lpfFreq = juce::jlimit(200.0f, 18000.0f, lpfFreq);
        hpfFreq = juce::jlimit(20.0f, 2000.0f, hpfFreq);

        auto lpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, lpfFreq);
        auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, hpfFreq);

        *delayLPF_L.coefficients = *lpfCoeffs;
        *delayLPF_R.coefficients = *lpfCoeffs;
        *delayHPF_L.coefficients = *hpfCoeffs;
        *delayHPF_R.coefficients = *hpfCoeffs;

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            auto& dLine  = (ch == 0) ? delayLineL  : delayLineR;
            auto& lpf    = (ch == 0) ? delayLPF_L  : delayLPF_R;
            auto& hpf    = (ch == 0) ? delayHPF_L  : delayHPF_R;

            for (int i = 0; i < numSamples; ++i)
            {
                float delayedSample = dLine.popSample(0, delaySamples, true);

                // Filter the delayed signal
                float filtered = lpf.processSample(delayedSample);
                filtered = hpf.processSample(filtered);

                // Feed back with soft clipping
                float feedIn = data[i] + filtered * fbGain;
                feedIn = std::tanh(feedIn);
                dLine.pushSample(0, feedIn);

                // Mix delayed signal into output
                data[i] += filtered * 0.5f;
            }
        }
    }

    // ---- Dry/Wet Mix ----
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wetData = buffer.getWritePointer(ch);
        auto* dryData = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float mixed = dryData[i] * (1.0f - dryWet) + wetData[i] * dryWet;
            mixed *= outputGain;

            // NaN/Inf safety guard
            if (std::isnan(mixed) || std::isinf(mixed))
                mixed = 0.0f;

            wetData[i] = mixed;
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
