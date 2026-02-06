#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
MinimalBeatEchoPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Time selection: 0 = 1/2 note, 1 = 1/4 note
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "time", 1 },
        "Time",
        juce::StringArray { "1/2", "1/4" },
        0  // Default to 1/2 note
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

MinimalBeatEchoPluginAudioProcessor::MinimalBeatEchoPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

MinimalBeatEchoPluginAudioProcessor::~MinimalBeatEchoPluginAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void MinimalBeatEchoPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    
    // Prepare delay lines
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;  // Mono delay lines
    
    delayLineLeft.prepare(spec);
    delayLineRight.prepare(spec);
    
    delayLineLeft.reset();
    delayLineRight.reset();
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void MinimalBeatEchoPluginAudioProcessor::releaseResources()
{
    // Delay lines automatically clean up
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void MinimalBeatEchoPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Get playhead info for BPM
    auto playHead = getPlayHead();
    if (playHead == nullptr)
    {
        buffer.clear();
        return;
    }

    auto positionInfo = playHead->getPosition();
    if (!positionInfo.hasValue() || !positionInfo->getBpm().hasValue())
    {
        buffer.clear();
        return;
    }

    double bpm = *positionInfo->getBpm();
    
    // Get time selection parameter (0 = 1/2 note, 1 = 1/4 note)
    auto timeParam = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("time"));
    int timeChoice = timeParam->getIndex();
    
    // Calculate delay time in samples based on BPM
    // 1 beat = 60/BPM seconds
    // 1/2 note = 2 beats, 1/4 note = 1 beat
    double beatsPerDelay = (timeChoice == 0) ? 2.0 : 1.0;
    double delayTimeSeconds = (60.0 / bpm) * beatsPerDelay;
    int delaySamples = static_cast<int>(delayTimeSeconds * currentSampleRate);
    
    // Clamp to valid range
    delaySamples = juce::jlimit(1, 384000, delaySamples);
    
    // Set delay times
    delayLineLeft.setDelay(static_cast<float>(delaySamples));
    delayLineRight.setDelay(static_cast<float>(delaySamples));
    
    // Process audio (100% wet, single echo)
    int numSamples = buffer.getNumSamples();
    
    // Process left channel
    if (buffer.getNumChannels() > 0)
    {
        auto* channelData = buffer.getWritePointer(0);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float inputSample = channelData[i];
            
            // Push input to delay line
            delayLineLeft.pushSample(0, inputSample);
            
            // Get delayed sample (this is the echo)
            float delayedSample = delayLineLeft.popSample(0);
            
            // Output only the delayed signal (100% wet)
            channelData[i] = delayedSample;
        }
    }
    
    // Process right channel
    if (buffer.getNumChannels() > 1)
    {
        auto* channelData = buffer.getWritePointer(1);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float inputSample = channelData[i];
            
            // Push input to delay line
            delayLineRight.pushSample(0, inputSample);
            
            // Get delayed sample (this is the echo)
            float delayedSample = delayLineRight.popSample(0);
            
            // Output only the delayed signal (100% wet)
            channelData[i] = delayedSample;
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* MinimalBeatEchoPluginAudioProcessor::createEditor()
{
    return new MinimalBeatEchoPluginAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void MinimalBeatEchoPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MinimalBeatEchoPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new MinimalBeatEchoPluginAudioProcessor();
}
