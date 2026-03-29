#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
ApexAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Size parameter - controls room size and decay time
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SIZE", 1 },
        "Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.7f
    ));

    // Mix parameter - dry/wet blend
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

ApexAudioProcessor::ApexAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

ApexAudioProcessor::~ApexAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void ApexAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Set up DSP processing specs
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Prepare reverb with cathedral settings
    reverb.prepare(spec);
    reverb.reset();
    
    // Prepare dry/wet mixer
    dryWetMixer.prepare(spec);
    dryWetMixer.reset();
    
    // Prepare high-shelf filter for brightness
    highShelfFilter.prepare(spec);
    highShelfFilter.reset();
    *highShelfFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, 4000.0f, 0.7f, 1.8f
    );
    
    // Initialize parameter smoothing
    mixSmoothed.reset(sampleRate, 0.05);
    sizeSmoothed.reset(sampleRate, 0.1);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void ApexAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void ApexAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters
    auto size = parameters.getRawParameterValue("SIZE")->load();
    auto mix = parameters.getRawParameterValue("MIX")->load();
    
    // Set smoothed target values
    mixSmoothed.setTargetValue(mix);
    sizeSmoothed.setTargetValue(size);
    
    // Configure reverb parameters for cathedral character
    juce::dsp::Reverb::Parameters reverbParams;
    
    // Map size to room size and decay time
    auto currentSize = sizeSmoothed.getNextValue();
    reverbParams.roomSize = 0.3f + (currentSize * 0.7f); // 0.3 to 1.0 range
    reverbParams.damping = 0.2f; // Low damping for bright, airy sound
    reverbParams.width = 1.0f; // Full stereo width
    reverbParams.wetLevel = 1.0f; // DryWetMixer handles the blend
    reverbParams.dryLevel = 0.0f;
    reverbParams.freezeMode = 0.0f;
    
    reverb.setParameters(reverbParams);
    
    // Set up dry/wet mixer
    auto currentMix = mixSmoothed.getNextValue();
    dryWetMixer.setWetMixProportion(currentMix);
    
    // Process audio
    juce::dsp::AudioBlock<float> block(buffer);
    
    // Save dry signal
    dryWetMixer.pushDrySamples(block);
    
    // Apply reverb
    reverb.process(juce::dsp::ProcessContextReplacing<float>(block));
    
    // Apply high-shelf filter for brightness
    highShelfFilter.process(juce::dsp::ProcessContextReplacing<float>(block));
    
    // Blend dry and wet signals
    dryWetMixer.mixWetSamples(block);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* ApexAudioProcessor::createEditor()
{
    return new ApexAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void ApexAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ApexAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new ApexAudioProcessor();
}
