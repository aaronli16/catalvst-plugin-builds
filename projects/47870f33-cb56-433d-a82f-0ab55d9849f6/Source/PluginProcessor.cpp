#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleHalfbeatDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // MIX: 0-100% wet/dry blend
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        "%"
    ));

    // FEEDBACK: 0-90% to prevent runaway oscillation
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 90.0f, 0.1f),
        30.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SimpleHalfbeatDelayAudioProcessor::SimpleHalfbeatDelayAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SimpleHalfbeatDelayAudioProcessor::~SimpleHalfbeatDelayAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play
// ==============================================================================

void SimpleHalfbeatDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    delayLine.prepare(spec);
    delayLine.reset();

    smoothedMix.reset(sampleRate, 0.02);    // 20ms smoothing
    smoothedFeedback.reset(sampleRate, 0.02);

    feedbackSample[0] = 0.0f;
    feedbackSample[1] = 0.0f;
}

// ==============================================================================
// Release Resources
// ==============================================================================

void SimpleHalfbeatDelayAudioProcessor::releaseResources()
{
}

// ==============================================================================
// Process Block
// ==============================================================================

void SimpleHalfbeatDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Read parameters (0-100 range, convert to 0-1)
    const float mixParam     = parameters.getRawParameterValue("MIX")->load() / 100.0f;
    const float feedbackParam = parameters.getRawParameterValue("FEEDBACK")->load() / 100.0f;

    smoothedMix.setTargetValue(mixParam);
    smoothedFeedback.setTargetValue(feedbackParam);

    // Calculate half-note delay time in samples from host BPM
    // Half note = 2 beats. At 120 BPM, that's 1 second.
    double bpm = 120.0;

    if (auto* playHead = getPlayHead())
    {
        auto posInfo = playHead->getPosition();
        if (posInfo.hasValue())
        {
            auto bpmOpt = posInfo->getBpm();
            if (bpmOpt.hasValue())
                bpm = *bpmOpt;
        }
    }

    // Half note = 2 beats worth of time
    const double halfNoteSeconds = (60.0 / bpm) * 2.0;
    const float delaySamples = static_cast<float>(halfNoteSeconds * currentSampleRate);

    // Clamp to max delay
    const float clampedDelay = juce::jmin(delaySamples, static_cast<float>(maxDelaySamples - 1));

    // Process each sample
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix      = smoothedMix.getNextValue();
        const float feedback = smoothedFeedback.getNextValue();
        const float dry      = 1.0f - mix;

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float inputSample = data[sample];

            // Set delay time
            delayLine.setDelay(clampedDelay);

            // Read from delay line
            const float delayedSample = delayLine.popSample(ch);

            // Write input + feedback into delay line
            delayLine.pushSample(ch, inputSample + (delayedSample * feedback));

            // Output: dry + wet mix
            data[sample] = (inputSample * dry) + (delayedSample * mix);
        }
    }
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* SimpleHalfbeatDelayAudioProcessor::createEditor()
{
    return new SimpleHalfbeatDelayAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
// ==============================================================================

void SimpleHalfbeatDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimpleHalfbeatDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ==============================================================================
// Factory
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleHalfbeatDelayAudioProcessor();
}
