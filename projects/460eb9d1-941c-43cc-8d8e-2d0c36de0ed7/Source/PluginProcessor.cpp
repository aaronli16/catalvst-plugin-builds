#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Delay Time: 50ms to 1200ms, default 400ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_TIME", 1 },
        "Delay Time",
        juce::NormalisableRange<float>(50.0f, 1200.0f, 1.0f),
        400.0f,
        "ms"
    ));

    // Feedback: 0% to 95%, default 45%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f),
        45.0f,
        "%"
    ));

    // Mix: 0% to 100%, default 35%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        35.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

OneKnobDelayAudioProcessor::OneKnobDelayAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

OneKnobDelayAudioProcessor::~OneKnobDelayAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void OneKnobDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1; // Each delay line is mono

    // Prepare delay lines
    delayLineL.prepare(spec);
    delayLineR.prepare(spec);
    delayLineL.reset();
    delayLineR.reset();

    // Prepare feedback filters — gentle low-pass at 4kHz for tape warmth
    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 4000.0f);
    feedbackFilterL.prepare(spec);
    feedbackFilterR.prepare(spec);
    feedbackFilterL.coefficients = coefficients;
    feedbackFilterR.coefficients = coefficients;
    feedbackFilterL.reset();
    feedbackFilterR.reset();

    // Initialize smoothed values (ramp over 20ms)
    smoothedDelayTime.reset(sampleRate, 0.02);
    smoothedFeedback.reset(sampleRate, 0.02);
    smoothedMix.reset(sampleRate, 0.02);

    // Set initial values
    smoothedDelayTime.setCurrentAndTarget(parameters.getRawParameterValue("DELAY_TIME")->load());
    smoothedFeedback.setCurrentAndTarget(parameters.getRawParameterValue("FEEDBACK")->load() / 100.0f);
    smoothedMix.setCurrentAndTarget(parameters.getRawParameterValue("MIX")->load() / 100.0f);

    // Clear feedback state
    feedbackSampleL = 0.0f;
    feedbackSampleR = 0.0f;
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void OneKnobDelayAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void OneKnobDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters
    smoothedDelayTime.setTargetValue(parameters.getRawParameterValue("DELAY_TIME")->load());
    smoothedFeedback.setTargetValue(parameters.getRawParameterValue("FEEDBACK")->load() / 100.0f);
    smoothedMix.setTargetValue(parameters.getRawParameterValue("MIX")->load() / 100.0f);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Need at least stereo for ping-pong
    if (numChannels < 2) return;

    auto* dataL = buffer.getWritePointer(0);
    auto* dataR = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float delayMs   = smoothedDelayTime.getNextValue();
        const float feedback  = smoothedFeedback.getNextValue();
        const float mix       = smoothedMix.getNextValue();

        // Convert delay time in ms to samples
        const float delaySamples = static_cast<float>(delayMs * currentSampleRate / 1000.0);

        // Read from delay lines
        const float delayedL = delayLineL.popSample(0, delaySamples);
        const float delayedR = delayLineR.popSample(0, delaySamples);

        // Apply low-pass filter to feedback path for tape warmth
        const float filteredL = feedbackFilterL.processSample(delayedL);
        const float filteredR = feedbackFilterR.processSample(delayedR);

        // Subtle tape saturation on the feedback path (soft-clip)
        const float saturatedL = std::tanh(filteredL * 1.2f) / std::tanh(1.2f);
        const float saturatedR = std::tanh(filteredR * 1.2f) / std::tanh(1.2f);

        // Ping-pong routing:
        // Left delay input = dry left + feedback from RIGHT (cross-feed)
        // Right delay input = dry right + feedback from LEFT (cross-feed)
        const float inputToL = dataL[i] + saturatedR * feedback;
        const float inputToR = dataR[i] + saturatedL * feedback;

        // Push into delay lines
        delayLineL.pushSample(0, inputToL);
        delayLineR.pushSample(0, inputToR);

        // Mix dry and wet
        dataL[i] = dataL[i] * (1.0f - mix) + delayedL * mix;
        dataR[i] = dataR[i] * (1.0f - mix) + delayedR * mix;
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* OneKnobDelayAudioProcessor::createEditor()
{
    return new OneKnobDelayAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void OneKnobDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OneKnobDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new OneKnobDelayAudioProcessor();
}
