#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
ResonoxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Frequency: 80 Hz to 4000 Hz, logarithmic skew
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FREQ", 1 },
        "Frequency",
        juce::NormalisableRange<float>(80.0f, 4000.0f, 0.1f, 0.3f),
        440.0f,
        "Hz"
    ));

    // Decay: 50 ms to 2000 ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DECAY", 1 },
        "Decay",
        juce::NormalisableRange<float>(50.0f, 2000.0f, 1.0f, 0.5f),
        500.0f,
        "ms"
    ));

    // Blur: 0% to 100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BLUR", 1 },
        "Blur",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        25.0f,
        "%"
    ));

    // Dry/Wet mix: 0% to 100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

ResonoxAudioProcessor::ResonoxAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

ResonoxAudioProcessor::~ResonoxAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void ResonoxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    dryWetMixer.prepare(spec);
    dryWetMixer.reset();

    spectralEngine.prepare(sampleRate, static_cast<int>(getTotalNumOutputChannels()));

    freqSmoothed.reset(sampleRate, 0.05);
    decaySmoothed.reset(sampleRate, 0.05);
    blurSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);

    setLatencySamples(spectralEngine.getLatencyInSamples());
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void ResonoxAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void ResonoxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters once per block
    auto freqHz = parameters.getRawParameterValue("FREQ")->load();
    auto decayMs = parameters.getRawParameterValue("DECAY")->load();
    auto blurPct = parameters.getRawParameterValue("BLUR")->load();
    auto mixPct = parameters.getRawParameterValue("MIX")->load();

    // Set DryWetMixer proportion (0.0 to 1.0)
    dryWetMixer.setWetMixProportion(mixPct / 100.0f);

    // Push dry signal before processing
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    // Normalize blur from percentage to 0..1
    float blurNorm = blurPct / 100.0f;

    // Process through spectral resonator (produces wet-only output)
    spectralEngine.processBlock(buffer, freqHz, decayMs, blurNorm);

    // NaN/Inf guard on output
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::isnan(data[i]) || std::isinf(data[i]))
                data[i] = 0.0f;
        }
    }

    // Mix wet and dry
    dryWetMixer.mixWetSamples(block);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* ResonoxAudioProcessor::createEditor()
{
    return new ResonoxAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void ResonoxAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ResonoxAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new ResonoxAudioProcessor();
}
