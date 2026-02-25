#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
PendulumAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Division (choice): 1/16, 1/8d, 1/8, 1/4d, 1/4, 1/2, 1/1
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "DIVISION", 1 },
        "Division",
        juce::StringArray { "1/16", "1/8d", "1/8", "1/4d", "1/4", "1/2", "1/1" },
        2  // default = 1/8
    ));

    // Feedback: 0.0 to 0.95
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f),
        0.4f
    ));

    // Mix: 0.0 to 1.0
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.35f
    ));

    // Damping: 0.0 to 1.0 (low-pass on feedback path)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DAMPING", 1 },
        "Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

PendulumAudioProcessor::PendulumAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

PendulumAudioProcessor::~PendulumAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void PendulumAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Max delay: 4 seconds at current sample rate (covers very slow tempos with 1/1 note)
    const int maxDelaySamples = static_cast<int>(sampleRate * 4.0);
    delayLine.setMaximumDelayInSamples(maxDelaySamples);
    delayLine.prepare(spec);
    delayLine.reset();

    dryWetMixer.prepare(spec);
    dryWetMixer.reset();

    // Initialize smoothed values
    delayTimeSmoothed.reset(sampleRate, 0.05);  // 50ms ramp for delay time
    feedbackSmoothed.reset(sampleRate, 0.02);
    dampingSmoothed.reset(sampleRate, 0.02);

    // Reset damping filter state
    dampState[0] = 0.0f;
    dampState[1] = 0.0f;
    feedbackSample[0] = 0.0f;
    feedbackSample[1] = 0.0f;
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void PendulumAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void PendulumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters
    auto divisionIndex = static_cast<int>(parameters.getRawParameterValue("DIVISION")->load());
    auto feedback = parameters.getRawParameterValue("FEEDBACK")->load();
    auto mix = parameters.getRawParameterValue("MIX")->load();
    auto damping = parameters.getRawParameterValue("DAMPING")->load();

    // Get BPM from DAW playhead, default to 120 if unavailable
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            if (auto bpmOpt = posInfo->getBpm())
                bpm = *bpmOpt;
        }
    }

    // Compute delay time in samples from BPM and division
    float targetDelaySamples = computeDelaySamples(bpm, divisionIndex);
    delayTimeSmoothed.setTargetValue(targetDelaySamples);
    feedbackSmoothed.setTargetValue(feedback);
    dampingSmoothed.setTargetValue(damping);

    // Set dry/wet mix
    dryWetMixer.setWetMixProportion(mix);

    // Push dry signal into the mixer
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    // Process delay with feedback and damping per-sample
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        auto delaySamps = delayTimeSmoothed.getNextValue();
        auto fb = feedbackSmoothed.getNextValue();
        auto damp = dampingSmoothed.getNextValue();

        // Damping coefficient: higher damping = more low-pass filtering
        // dampCoeff near 1.0 = heavy filtering; near 0.0 = no filtering
        float dampCoeff = damp * 0.85f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto input = buffer.getSample(ch, i);

            // Read delayed sample
            auto delayed = delayLine.popSample(ch, delaySamps, true);

            // Apply one-pole low-pass to the feedback path (damping)
            dampState[ch] = dampState[ch] + dampCoeff * (delayed - dampState[ch]);
            float dampedDelayed = dampState[ch];

            // Feed input + damped feedback back into delay line
            float toDelay = input + std::tanh(dampedDelayed * fb);

            // NaN/Inf guard
            if (std::isnan(toDelay) || std::isinf(toDelay))
                toDelay = 0.0f;

            delayLine.pushSample(ch, toDelay);

            // Write only wet (delayed) signal — DryWetMixer blends with dry
            buffer.setSample(ch, i, delayed);
        }
    }

    // Blend dry/wet
    dryWetMixer.mixWetSamples(block);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* PendulumAudioProcessor::createEditor()
{
    return new PendulumAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void PendulumAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PendulumAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ==============================================================================
// Helper: Compute delay time in samples from BPM and division index
// ==============================================================================

float PendulumAudioProcessor::computeDelaySamples(double bpm, int divisionIndex) const
{
    // Clamp index
    if (divisionIndex < 0) divisionIndex = 0;
    if (divisionIndex > 6) divisionIndex = 6;

    // Clamp BPM to sane range
    if (bpm < 20.0) bpm = 20.0;
    if (bpm > 300.0) bpm = 300.0;

    // Beat duration in seconds
    double beatSec = 60.0 / bpm;
    // Delay in seconds = beats * beat duration
    double delaySec = static_cast<double>(divisionBeats[divisionIndex]) * beatSec;
    // Convert to samples
    float delaySamples = static_cast<float>(delaySec * currentSampleRate);

    // Clamp to max delay
    float maxSamples = static_cast<float>(currentSampleRate * 4.0) - 1.0f;
    if (delaySamples > maxSamples) delaySamples = maxSamples;
    if (delaySamples < 1.0f) delaySamples = 1.0f;

    return delaySamples;
}

// ==============================================================================
// Factory Function - Required by JUCE plugin system
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PendulumAudioProcessor();
}
