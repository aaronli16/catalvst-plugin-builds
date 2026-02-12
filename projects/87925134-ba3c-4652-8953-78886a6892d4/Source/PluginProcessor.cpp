#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
LowPassSingleKnobFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // CUTOFF: 0.0 = fully filtered (silence), 1.0 = wide open (20kHz)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CUTOFF", 1 },
        "Cutoff",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        1.0f  // Default: wide open
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

LowPassSingleKnobFilterAudioProcessor::LowPassSingleKnobFilterAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

LowPassSingleKnobFilterAudioProcessor::~LowPassSingleKnobFilterAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play
// ==============================================================================

void LowPassSingleKnobFilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Initialize filter wide open
    *lowPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 20000.0f);
    lowPassFilter.prepare(spec);
    lowPassFilter.reset();

    // Smooth cutoff changes over ~20ms to avoid zipper noise
    smoothedCutoff.reset(sampleRate, 0.02);
    smoothedCutoff.setCurrentAndTargetValue(1.0f);
}

// ==============================================================================
// Release Resources
// ==============================================================================

void LowPassSingleKnobFilterAudioProcessor::releaseResources()
{
}

// ==============================================================================
// Process Block
// ==============================================================================

void LowPassSingleKnobFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read the normalized cutoff parameter (0 = silence, 1 = wide open)
    auto cutoffNorm = parameters.getRawParameterValue("CUTOFF")->load();
    smoothedCutoff.setTargetValue(cutoffNorm);

    // Skip ahead to get the smoothed value for this block
    float smoothedValue = smoothedCutoff.skip(buffer.getNumSamples());

    // Full silence when knob is at zero
    if (smoothedValue <= 0.001f && cutoffNorm <= 0.001f)
    {
        buffer.clear();
        lowPassFilter.reset();
        return;
    }

    // Map normalized 0-1 to 20Hz-20kHz logarithmically
    float clampedNorm = juce::jlimit(0.01f, 1.0f, smoothedValue);
    float minLog = std::log10(20.0f);
    float maxLog = std::log10(20000.0f);
    float freq = std::pow(10.0f, minLog + clampedNorm * (maxLog - minLog));
    freq = juce::jlimit(20.0f, 20000.0f, freq);

    // Update filter coefficients
    *lowPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate, freq);

    // Run the filter
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    lowPassFilter.process(context);

    // Apply a gentle gain fade near the bottom of the range for clean muting
    if (smoothedValue < 0.05f)
    {
        float gain = smoothedValue / 0.05f;
        buffer.applyGain(gain);
    }
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* LowPassSingleKnobFilterAudioProcessor::createEditor()
{
    return new LowPassSingleKnobFilterAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
// ==============================================================================

void LowPassSingleKnobFilterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LowPassSingleKnobFilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new LowPassSingleKnobFilterAudioProcessor();
}
