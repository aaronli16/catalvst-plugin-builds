#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
OneKnobSmoothReverbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WET", 1 },
        "Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

OneKnobSmoothReverbAudioProcessor::OneKnobSmoothReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

OneKnobSmoothReverbAudioProcessor::~OneKnobSmoothReverbAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play
// ==============================================================================

void OneKnobSmoothReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverb.prepare(spec);
    reverb.reset();
    previousWet = 0.0f;
}

// ==============================================================================
// Release Resources
// ==============================================================================

void OneKnobSmoothReverbAudioProcessor::releaseResources()
{
    reverb.reset();
}

// ==============================================================================
// Process Block
// ==============================================================================

void OneKnobSmoothReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto wet = parameters.getRawParameterValue("WET")->load();

    // Map the single wet knob to reverb parameters for a lush, evolving sound.
    // As wet increases: longer decay, more dampening, bigger room, wider stereo.
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize   = 0.4f + wet * 0.55f;       // 0.4 -> 0.95
    reverbParams.damping    = 0.3f + wet * 0.35f;        // 0.3 -> 0.65
    reverbParams.wetLevel   = wet * 0.75f;                // 0.0 -> 0.75
    reverbParams.dryLevel   = 1.0f - wet * 0.5f;         // 1.0 -> 0.5
    reverbParams.width      = 0.5f + wet * 0.5f;         // 0.5 -> 1.0
    reverbParams.freezeMode = 0.0f;

    reverb.setParameters(reverbParams);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    previousWet = wet;
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* OneKnobSmoothReverbAudioProcessor::createEditor()
{
    return new OneKnobSmoothReverbAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
// ==============================================================================

void OneKnobSmoothReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OneKnobSmoothReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new OneKnobSmoothReverbAudioProcessor();
}
