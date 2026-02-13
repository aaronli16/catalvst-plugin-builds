#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
CathedralAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f
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
// Prepare to Play
// ==============================================================================

void CathedralAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Configure reverb for massive cathedral tail
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize   = 1.0f;    // Maximum room size
    reverbParams.damping    = 0.3f;    // Low damping = long, dark tail
    reverbParams.wetLevel   = 1.0f;    // We handle mix manually
    reverbParams.dryLevel   = 0.0f;    // We handle mix manually
    reverbParams.width      = 1.0f;    // Full stereo width
    reverbParams.freezeMode = 0.0f;

    reverb.setParameters(reverbParams);
    reverb.prepare(spec);
    reverb.reset();

    // Dark filter: low-pass at ~2.5kHz to remove brightness from the reverb
    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 2500.0f, 0.707f);
    darkFilter.state = coefficients;
    darkFilter.prepare(spec);
    darkFilter.reset();

    // Allocate dry buffer
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);
    dryBuffer.clear();
}

// ==============================================================================
// Release Resources
// ==============================================================================

void CathedralAudioProcessor::releaseResources()
{
    reverb.reset();
    darkFilter.reset();
}

// ==============================================================================
// Process Block
// ==============================================================================

void CathedralAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto mix = parameters.getRawParameterValue("MIX")->load();

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Save dry signal
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Process reverb on the buffer (wet signal)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);

    // Apply dark filter to the wet reverb signal
    darkFilter.process(context);

    // Mix dry and wet
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* dryData = dryBuffer.getReadPointer(ch);
        auto* outData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            outData[i] = dryData[i] * (1.0f - mix) + outData[i] * mix;
        }
    }
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* CathedralAudioProcessor::createEditor()
{
    return new CathedralAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
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
// Factory Function
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CathedralAudioProcessor();
}
