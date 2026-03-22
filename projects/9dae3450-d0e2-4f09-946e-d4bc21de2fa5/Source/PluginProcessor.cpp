#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
MetronomeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // TIME: note division index (0=1/8, 1=1/4, 2=1/2, 3=1/1)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TIME", 1 },
        "Time",
        juce::NormalisableRange<float>(0.0f, 3.0f, 1.0f),
        1.0f
    ));

    // DOTTED modifier
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "DOTTED", 1 },
        "Dotted",
        false
    ));

    // TRIPLET modifier
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "TRIPLET", 1 },
        "Triplet",
        false
    ));

    // FEEDBACK: 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f
    ));

    // MIX: dry/wet blend
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

MetronomeAudioProcessor::MetronomeAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

MetronomeAudioProcessor::~MetronomeAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void MetronomeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    pingPongDelay.prepare(sampleRate, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void MetronomeAudioProcessor::releaseResources()
{
    pingPongDelay.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void MetronomeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters
    auto timeIndex = static_cast<int>(parameters.getRawParameterValue("TIME")->load());
    auto dotted    = parameters.getRawParameterValue("DOTTED")->load() > 0.5f;
    auto triplet   = parameters.getRawParameterValue("TRIPLET")->load() > 0.5f;
    auto feedback  = parameters.getRawParameterValue("FEEDBACK")->load();
    auto mix       = parameters.getRawParameterValue("MIX")->load();

    // Get BPM from DAW (default 120 for standalone)
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            if (posInfo->getBpm().hasValue())
                bpm = *posInfo->getBpm();
        }
    }

    // Calculate delay time from BPM and note division
    const float noteMultipliers[] = { 0.5f, 1.0f, 2.0f, 4.0f };
    float delayMs = static_cast<float>(60000.0 / bpm)
                  * noteMultipliers[juce::jlimit(0, 3, timeIndex)];

    if (dotted)
        delayMs *= 1.5f;
    else if (triplet)
        delayMs *= (2.0f / 3.0f);

    delayMs = juce::jlimit(50.0f, 2000.0f, delayMs);

    // Build params and process
    PingPongDelay::Params ppParams;
    ppParams.delayMs  = delayMs;
    ppParams.feedback = feedback;
    ppParams.spread   = 1.0f;
    ppParams.mix      = mix;

    pingPongDelay.process(buffer, ppParams);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* MetronomeAudioProcessor::createEditor()
{
    return new MetronomeAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void MetronomeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MetronomeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new MetronomeAudioProcessor();
}
