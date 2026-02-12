#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
AmbientSmearEffectAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "INTENSITY", 1 },
        "Intensity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

AmbientSmearEffectAudioProcessor::AmbientSmearEffectAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

AmbientSmearEffectAudioProcessor::~AmbientSmearEffectAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play
// ==============================================================================

void AmbientSmearEffectAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Prepare reverb
    reverb.prepare(spec);
    reverb.reset();

    // Prepare delay lines (mono specs since we handle L/R separately)
    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    delayLineL.prepare(monoSpec);
    delayLineR.prepare(monoSpec);
    delayLineL.reset();
    delayLineR.reset();

    // Reset feedback buffers
    delayFeedbackL = 0.0f;
    delayFeedbackR = 0.0f;

    // Smoothed parameter
    smoothedIntensity.reset(sampleRate, 0.05); // 50ms smoothing
    smoothedIntensity.setCurrentAndTargetValue(
        parameters.getRawParameterValue("INTENSITY")->load());
}

// ==============================================================================
// Release Resources
// ==============================================================================

void AmbientSmearEffectAudioProcessor::releaseResources()
{
}

// ==============================================================================
// Process Block
// ==============================================================================

void AmbientSmearEffectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto intensity = parameters.getRawParameterValue("INTENSITY")->load();
    smoothedIntensity.setTargetValue(intensity);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels < 2)
        return;

    // Store dry signal
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // ---- PING-PONG DELAY ----
    // Delay times: dotted-eighth feel for spacey ping-pong
    // Left: 375ms (dotted eighth at ~120bpm), Right: 250ms (eighth)
    const float baseDelayTimeL = 0.375f; // seconds
    const float baseDelayTimeR = 0.250f; // seconds

    auto* leftData  = buffer.getWritePointer(0);
    auto* rightData = buffer.getWritePointer(1);

    // Create a separate buffer for the delay wet signal
    juce::AudioBuffer<float> delayWetBuffer(2, numSamples);
    delayWetBuffer.clear();

    auto* delayWetL = delayWetBuffer.getWritePointer(0);
    auto* delayWetR = delayWetBuffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        float currentIntensity = smoothedIntensity.getNextValue();

        // Scale feedback with intensity: 0.0 -> 0.0, 1.0 -> 0.75
        float feedback = currentIntensity * 0.75f;

        // Delay times in samples
        float delaySamplesL = baseDelayTimeL * static_cast<float>(currentSampleRate);
        float delaySamplesR = baseDelayTimeR * static_cast<float>(currentSampleRate);

        delayLineL.setDelay(delaySamplesL);
        delayLineR.setDelay(delaySamplesR);

        // Read from delay lines
        float delayOutL = delayLineL.popSample(0);
        float delayOutR = delayLineR.popSample(0);

        // Ping-pong: feed left input + right feedback into left delay,
        //            feed right input + left feedback into right delay
        float delayInL = leftData[i]  + delayOutR * feedback;
        float delayInR = rightData[i] + delayOutL * feedback;

        // Push into delay lines
        delayLineL.pushSample(0, delayInL);
        delayLineR.pushSample(0, delayInR);

        // Store wet delay output
        delayWetL[i] = delayOutL;
        delayWetR[i] = delayOutR;
    }

    // ---- REVERB ----
    // Configure reverb based on intensity
    // Hall: big room, long decay that scales with intensity
    float currentIntensityForReverb = intensity; // use raw for reverb params

    reverbParams.roomSize   = 0.5f + currentIntensityForReverb * 0.49f;  // 0.5 -> 0.99
    reverbParams.damping    = 0.5f - currentIntensityForReverb * 0.3f;   // 0.5 -> 0.2 (less damping = brighter tail)
    reverbParams.wetLevel   = 1.0f;                                       // We'll handle mix ourselves
    reverbParams.dryLevel   = 0.0f;
    reverbParams.width      = 0.8f + currentIntensityForReverb * 0.2f;   // 0.8 -> 1.0
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters(reverbParams);

    // Feed the original dry signal + some delay into reverb
    // Create a reverb input buffer: dry + scaled delay wet
    juce::AudioBuffer<float> reverbInputBuffer(2, numSamples);
    reverbInputBuffer.clear();

    // Mix dry + delay as reverb input (delay feeds into reverb for lushness)
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* reverbIn = reverbInputBuffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);
        auto* delayWet = delayWetBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            reverbIn[i] = dry[i] + delayWet[i] * 0.3f;
        }
    }

    // Process reverb
    juce::dsp::AudioBlock<float> reverbBlock(reverbInputBuffer);
    juce::dsp::ProcessContextReplacing<float> reverbContext(reverbBlock);
    reverb.process(reverbContext);

    // ---- FINAL MIX ----
    // Intensity controls the wet/dry blend
    // At 0%: 100% dry, 0% effects
    // At 100%: 0% dry, 100% wet (equal reverb + delay)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* output = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);
        auto* reverbWet = reverbInputBuffer.getReadPointer(ch); // reverb was processed in-place
        auto* delayWet = delayWetBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float wetMix = intensity; // direct mapping: intensity = wet level
            float dryMix = 1.0f - wetMix;

            // Equal blend of reverb and delay for the wet signal
            float wetSignal = (reverbWet[i] * 0.5f + delayWet[i] * 0.5f);

            output[i] = dry[i] * dryMix + wetSignal * wetMix;
        }
    }
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* AmbientSmearEffectAudioProcessor::createEditor()
{
    return new AmbientSmearEffectAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
// ==============================================================================

void AmbientSmearEffectAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AmbientSmearEffectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new AmbientSmearEffectAudioProcessor();
}
