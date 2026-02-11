#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WET", 1 },
        "Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f
    ));

    return layout;
}

OneKnobReverbAudioProcessor::OneKnobReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

OneKnobReverbAudioProcessor::~OneKnobReverbAudioProcessor()
{
}

void OneKnobReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverb.prepare(spec);
    reverb.reset();

    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);
}

void OneKnobReverbAudioProcessor::releaseResources()
{
}

void OneKnobReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto wet = parameters.getRawParameterValue("WET")->load();

    // Configure reverb — fixed hall-like setting, only mix changes
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize   = 0.7f;
    reverbParams.damping    = 0.4f;
    reverbParams.wetLevel   = 1.0f;   // Full wet from reverb engine
    reverbParams.dryLevel   = 0.0f;   // We handle dry/wet mix ourselves
    reverbParams.width      = 1.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Save dry signal
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Process reverb (buffer now contains 100% wet)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Mix dry + wet
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* outData = buffer.getWritePointer(ch);
        const auto* dryData = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            outData[i] = dryData[i] * (1.0f - wet) + outData[i] * wet;
        }
    }
}

juce::AudioProcessorEditor* OneKnobReverbAudioProcessor::createEditor()
{
    return new OneKnobReverbAudioProcessorEditor(*this);
}

void OneKnobReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OneKnobReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OneKnobReverbAudioProcessor();
}
