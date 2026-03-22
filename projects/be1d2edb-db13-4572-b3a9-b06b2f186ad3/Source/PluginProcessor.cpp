#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
FlutterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TIME", 1 },
        "Time",
        juce::NormalisableRange<float>(50.0f, 1200.0f, 1.0f),
        380.0f,
        "ms"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        45.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FLUTTER", 1 },
        "Flutter",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        30.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TAPE_AGE", 1 },
        "Tape Age",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 },
        "Tone",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        60.0f,
        "%"
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        35.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

FlutterAudioProcessor::FlutterAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

FlutterAudioProcessor::~FlutterAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void FlutterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    tapeDelay.prepare(sampleRate, samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void FlutterAudioProcessor::releaseResources()
{
    tapeDelay.reset();
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void FlutterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto timeMs    = parameters.getRawParameterValue("TIME")->load();
    auto feedback  = parameters.getRawParameterValue("FEEDBACK")->load() / 100.0f;
    auto flutterAmt = parameters.getRawParameterValue("FLUTTER")->load() / 100.0f;
    auto tapeAge   = parameters.getRawParameterValue("TAPE_AGE")->load() / 100.0f;
    auto tone      = parameters.getRawParameterValue("TONE")->load() / 100.0f;
    auto mix       = parameters.getRawParameterValue("MIX")->load() / 100.0f;

    // Tape Age darkens tone and increases flutter, simulating worn tape
    float compositeTone    = tone * (1.0f - tapeAge * 0.4f);
    float compositeFlutter = juce::jlimit(0.0f, 1.0f, flutterAmt + tapeAge * 0.25f);

    TapeDelay::Params delayParams;
    delayParams.delayMs  = timeMs;
    delayParams.feedback = feedback;
    delayParams.flutter  = compositeFlutter;
    delayParams.tone     = compositeTone;
    delayParams.mix      = mix;

    tapeDelay.process(buffer, delayParams);
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* FlutterAudioProcessor::createEditor()
{
    return new FlutterAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void FlutterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FlutterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new FlutterAudioProcessor();
}
