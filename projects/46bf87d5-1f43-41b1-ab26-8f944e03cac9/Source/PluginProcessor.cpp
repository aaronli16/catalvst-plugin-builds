#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
AscensionAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "NOTE_DIV", 1 },
        "Note Division",
        juce::NormalisableRange<float>(0.0f, 5.0f, 1.0f),
        0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.9f, 0.01f),
        0.4f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

AscensionAudioProcessor::AscensionAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

AscensionAudioProcessor::~AscensionAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void AscensionAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    tempoDelay.prepare(sampleRate, samplesPerBlock);
    hallReverb.prepare(sampleRate, samplesPerBlock);
    dryWetMixer.prepare(sampleRate, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void AscensionAudioProcessor::releaseResources()
{
    tempoDelay.reset();
    hallReverb.reset();
    dryWetMixer.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void AscensionAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto noteDivVal = parameters.getRawParameterValue("NOTE_DIV")->load();
    auto feedbackVal = parameters.getRawParameterValue("FEEDBACK")->load();
    auto mixVal = parameters.getRawParameterValue("MIX")->load();

    // Read host tempo for tempo-synced delay
    tempoDelay.updateBPMFromPlayHead(getPlayHead());

    dryWetMixer.setDryBuffer(buffer);

    TempoSyncDelay::Params delayParams;
    delayParams.mix = 1.0f;
    delayParams.tone = 0.7f;
    delayParams.feedback = feedbackVal;
    delayParams.noteDivision = static_cast<int>(noteDivVal);

    tempoDelay.process(buffer, delayParams);

    SignalsmithHallReverb::Params reverbParams;
    reverbParams.mix = 0.35f;
    reverbParams.size = 0.6f;
    reverbParams.brightness = 0.7f;
    reverbParams.decaySeconds = 2.0f;

    hallReverb.process(buffer, reverbParams);

    dryWetMixer.mix(buffer, mixVal);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* AscensionAudioProcessor::createEditor()
{
    return new AscensionAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void AscensionAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AscensionAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new AscensionAudioProcessor();
}
