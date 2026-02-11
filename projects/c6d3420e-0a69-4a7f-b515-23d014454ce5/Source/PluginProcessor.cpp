#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleHalfbeatEchoAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

SimpleHalfbeatEchoAudioProcessor::SimpleHalfbeatEchoAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

SimpleHalfbeatEchoAudioProcessor::~SimpleHalfbeatEchoAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play
// ==============================================================================

void SimpleHalfbeatEchoAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1; // We process each channel individually

    delayLineL.prepare(spec);
    delayLineR.prepare(spec);
    delayLineL.reset();
    delayLineR.reset();

    // Default half-beat delay at 120 BPM: 60/120 * 0.5 = 0.25 seconds
    float defaultDelaySamples = static_cast<float>(sampleRate * 0.25);
    smoothedDelayTimeL.reset(sampleRate, 0.05); // 50ms smoothing
    smoothedDelayTimeR.reset(sampleRate, 0.05);
    smoothedDelayTimeL.setCurrentAndTargetValue(defaultDelaySamples);
    smoothedDelayTimeR.setCurrentAndTargetValue(defaultDelaySamples);
}

// ==============================================================================
// Release Resources
// ==============================================================================

void SimpleHalfbeatEchoAudioProcessor::releaseResources()
{
    delayLineL.reset();
    delayLineR.reset();
}

// ==============================================================================
// Process Block
// ==============================================================================

void SimpleHalfbeatEchoAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto mix = parameters.getRawParameterValue("MIX")->load();

    // Calculate half-beat delay from host BPM
    auto playHead = getPlayHead();
    double bpm = 120.0; // Default fallback

    if (playHead != nullptr)
    {
        auto posInfo = playHead->getPosition();
        if (posInfo.hasValue() && posInfo->getBpm().hasValue())
        {
            bpm = *posInfo->getBpm();
            if (bpm < 20.0) bpm = 120.0;  // Sanity check
            if (bpm > 300.0) bpm = 300.0;
        }
    }

    // Half beat = one eighth note = 60 / bpm * 0.5 seconds
    float halfBeatSeconds = static_cast<float>(60.0 / bpm * 0.5);
    float delaySamples = halfBeatSeconds * static_cast<float>(currentSampleRate);

    // Clamp to safe range
    delaySamples = juce::jlimit(1.0f, static_cast<float>(maxDelaySamples - 1), delaySamples);

    smoothedDelayTimeL.setTargetValue(delaySamples);
    smoothedDelayTimeR.setTargetValue(delaySamples);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Feedback amount (fixed at a nice musical value)
    const float feedback = 0.35f;

    if (numChannels >= 2)
    {
        auto* leftData  = buffer.getWritePointer(0);
        auto* rightData = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            float delayL = smoothedDelayTimeL.getNextValue();
            float delayR = smoothedDelayTimeR.getNextValue();

            // Read delayed samples
            float delayedL = delayLineL.popSample(0, delayL);
            float delayedR = delayLineR.popSample(0, delayR);

            // Input + feedback
            float inputL = leftData[i];
            float inputR = rightData[i];

            // Push input + feedback into delay line
            delayLineL.pushSample(0, inputL + delayedL * feedback);
            delayLineR.pushSample(0, inputR + delayedR * feedback);

            // Mix dry/wet
            leftData[i]  = inputL * (1.0f - mix) + delayedL * mix;
            rightData[i] = inputR * (1.0f - mix) + delayedR * mix;
        }
    }
    else if (numChannels == 1)
    {
        auto* data = buffer.getWritePointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            float delayT = smoothedDelayTimeL.getNextValue();
            float delayed = delayLineL.popSample(0, delayT);
            float input = data[i];

            delayLineL.pushSample(0, input + delayed * feedback);
            data[i] = input * (1.0f - mix) + delayed * mix;
        }
    }
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* SimpleHalfbeatEchoAudioProcessor::createEditor()
{
    return new SimpleHalfbeatEchoAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
// ==============================================================================

void SimpleHalfbeatEchoAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimpleHalfbeatEchoAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new SimpleHalfbeatEchoAudioProcessor();
}
