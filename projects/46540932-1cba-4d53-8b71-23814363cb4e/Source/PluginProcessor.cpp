#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
UntitledPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DETUNE", 1 },
        "Detune",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WARMTH", 1 },
        "Warmth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f),
        1.0f
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

UntitledPluginAudioProcessor::UntitledPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

UntitledPluginAudioProcessor::~UntitledPluginAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void UntitledPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Reset all voices
    for (auto& v : voices)
    {
        v.active = false;
        v.noteNumber = -1;
        v.envLevel = 0.0f;
        v.envStage = 0;
        for (int i = 0; i < numUnison; ++i)
            v.phase[i] = 0.0;
    }

    // Reset warmth filter
    warmthFilterState[0] = 0.0f;
    warmthFilterState[1] = 0.0f;

    juce::ignoreUnused(samplesPerBlock);
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void UntitledPluginAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void UntitledPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters once per block
    auto detuneAmount = parameters.getRawParameterValue("DETUNE")->load();
    auto warmthOn = parameters.getRawParameterValue("WARMTH")->load() > 0.5f;

    // Handle MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            startVoice(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            stopVoice(msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            for (auto& v : voices) { v.active = false; v.envStage = 0; v.envLevel = 0.0f; }
    }

    // Clear buffer
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Envelope rates
    const float attackRate = 1.0f / (float)(currentSampleRate * 0.005);  // 5ms attack
    const float releaseRate = 1.0f / (float)(currentSampleRate * 0.08);  // 80ms release

    // Detune spread: up to 25 cents at full detune
    const double maxDetuneCents = 25.0;
    const double detuneSpread = detuneAmount * maxDetuneCents;

    // Unison detune offsets (centered, spread evenly)
    double detuneOffsets[numUnison];
    for (int u = 0; u < numUnison; ++u)
    {
        double t = (numUnison > 1) ? ((double)u / (double)(numUnison - 1)) * 2.0 - 1.0 : 0.0;
        detuneOffsets[u] = t * detuneSpread; // in cents
    }

    // Stereo pan for each unison voice (spread left-right)
    float panL[numUnison], panR[numUnison];
    for (int u = 0; u < numUnison; ++u)
    {
        double t = (numUnison > 1) ? (double)u / (double)(numUnison - 1) : 0.5;
        float pan = (float)(t * detuneAmount); // 0=left, 1=right, scaled by detune
        panL[u] = std::cos(pan * juce::MathConstants<float>::halfPi);
        panR[u] = std::sin(pan * juce::MathConstants<float>::halfPi);
    }

    const float unisonGain = 0.25f / (float)numUnison;

    for (int i = 0; i < numSamples; ++i)
    {
        float outL = 0.0f;
        float outR = 0.0f;

        for (auto& v : voices)
        {
            if (v.envStage == 0)
                continue;

            // Envelope
            if (v.envStage == 1) // attack
            {
                v.envLevel += attackRate;
                if (v.envLevel >= 1.0f) { v.envLevel = 1.0f; v.envStage = 2; }
            }
            else if (v.envStage == 3) // release
            {
                v.envLevel -= releaseRate;
                if (v.envLevel <= 0.0f) { v.envLevel = 0.0f; v.envStage = 0; v.active = false; continue; }
            }

            float voiceL = 0.0f;
            float voiceR = 0.0f;

            for (int u = 0; u < numUnison; ++u)
            {
                // Frequency with detune
                double freqCents = detuneOffsets[u];
                double freq = v.frequency * std::pow(2.0, freqCents / 1200.0);
                double phaseInc = freq / currentSampleRate;

                // PolyBLEP sawtooth
                float saw = polyBlepSaw(v.phase[u], phaseInc);

                voiceL += saw * panL[u];
                voiceR += saw * panR[u];

                // Advance phase
                v.phase[u] += phaseInc;
                if (v.phase[u] >= 1.0) v.phase[u] -= 1.0;
            }

            float env = v.envLevel * v.velocity;
            outL += voiceL * env * unisonGain;
            outR += voiceR * env * unisonGain;
        }

        // Warmth filter (one-pole lowpass at ~4kHz)
        if (warmthOn)
        {
            float cutoff = 4000.0f / (float)currentSampleRate;
            float coeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoff);
            warmthFilterState[0] += coeff * (outL - warmthFilterState[0]);
            warmthFilterState[1] += coeff * (outR - warmthFilterState[1]);

            // Subtle saturation
            outL = std::tanh(warmthFilterState[0] * 1.3f);
            outR = std::tanh(warmthFilterState[1] * 1.3f);
        }

        // NaN/Inf guard
        if (std::isnan(outL) || std::isinf(outL)) outL = 0.0f;
        if (std::isnan(outR) || std::isinf(outR)) outR = 0.0f;

        if (numChannels >= 1) buffer.setSample(0, i, outL);
        if (numChannels >= 2) buffer.setSample(1, i, outR);
    }
}

// ==============================================================================
// Synth Voice Helpers
// ==============================================================================

float UntitledPluginAudioProcessor::polyBlepSaw(double phase, double phaseIncrement)
{
    // Naive sawtooth: 2*phase - 1
    float saw = (float)(2.0 * phase - 1.0);

    // PolyBLEP correction at discontinuity
    double t = phase;
    double dt = phaseIncrement;

    if (t < dt)
    {
        t /= dt;
        saw -= (float)(t + t - t * t - 1.0);
    }
    else if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        saw -= (float)(t * t + t + t + 1.0);
    }

    return saw;
}

void UntitledPluginAudioProcessor::startVoice(int noteNumber, float velocity)
{
    // Find a free voice, or steal the oldest releasing voice
    int freeIndex = -1;
    int releasingIndex = -1;
    float lowestEnv = 2.0f;

    for (int i = 0; i < maxVoices; ++i)
    {
        if (!voices[i].active && voices[i].envStage == 0)
        {
            freeIndex = i;
            break;
        }
        if (voices[i].envStage == 3 && voices[i].envLevel < lowestEnv)
        {
            lowestEnv = voices[i].envLevel;
            releasingIndex = i;
        }
    }

    int idx = (freeIndex >= 0) ? freeIndex : ((releasingIndex >= 0) ? releasingIndex : 0);

    auto& v = voices[idx];
    v.active = true;
    v.noteNumber = noteNumber;
    v.velocity = velocity;
    v.frequency = juce::MidiMessage::getMidiNoteInHertz(noteNumber);
    v.envLevel = 0.0f;
    v.envStage = 1; // attack

    // Randomize phases slightly for analog feel
    for (int u = 0; u < numUnison; ++u)
        v.phase[u] = (double)(std::rand()) / (double)RAND_MAX;
}

void UntitledPluginAudioProcessor::stopVoice(int noteNumber)
{
    for (auto& v : voices)
    {
        if (v.active && v.noteNumber == noteNumber && v.envStage != 3)
        {
            v.envStage = 3; // release
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* UntitledPluginAudioProcessor::createEditor()
{
    return new UntitledPluginAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void UntitledPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void UntitledPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new UntitledPluginAudioProcessor();
}
