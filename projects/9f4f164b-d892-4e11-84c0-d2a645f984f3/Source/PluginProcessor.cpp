#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
EndlessSmileSidechainEffectAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "INTENSITY", 1 },
        "Intensity",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f,
        "%"
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

EndlessSmileSidechainEffectAudioProcessor::EndlessSmileSidechainEffectAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

EndlessSmileSidechainEffectAudioProcessor::~EndlessSmileSidechainEffectAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play
// ==============================================================================

void EndlessSmileSidechainEffectAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Prepare reverb
    reverb.prepare(spec);
    reverb.reset();

    // Prepare delay lines
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    delayLineL.prepare(monoSpec);
    delayLineL.reset();
    delayLineR.prepare(monoSpec);
    delayLineR.reset();

    // Set max delay to 1 second
    delayLineL.setMaximumDelayInSamples(static_cast<int>(sampleRate));
    delayLineR.setMaximumDelayInSamples(static_cast<int>(sampleRate));

    // Smoothed intensity
    smoothedIntensity.reset(sampleRate, 0.05); // 50ms smoothing
    smoothedIntensity.setCurrentAndTargetValue(0.0f);

    // Reset feedback
    delayFeedbackL = 0.0f;
    delayFeedbackR = 0.0f;
}

// ==============================================================================
// Release Resources
// ==============================================================================

void EndlessSmileSidechainEffectAudioProcessor::releaseResources()
{
}

// ==============================================================================
// Process Block
// ==============================================================================

void EndlessSmileSidechainEffectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto intensity = parameters.getRawParameterValue("INTENSITY")->load();
    float norm = intensity / 100.0f; // 0.0 to 1.0

    smoothedIntensity.setTargetValue(norm);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels < 2) return;

    // Make a dry copy for mixing
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // ======================================================================
    // STEP 1: REVERB
    // Reverb starts at 0% and ramps up. Room size and wet level increase.
    // ======================================================================
    {
        // Reverb intensity: active from 0% to 100%, ramps up
        float reverbAmount = norm; // Full range

        reverbParams.roomSize = 0.3f + reverbAmount * 0.65f;    // 0.3 -> 0.95
        reverbParams.damping = 0.5f - reverbAmount * 0.3f;      // 0.5 -> 0.2 (less damping = longer tail)
        reverbParams.wetLevel = reverbAmount * 0.7f;             // 0.0 -> 0.7
        reverbParams.dryLevel = 1.0f - reverbAmount * 0.3f;     // 1.0 -> 0.7
        reverbParams.width = 0.5f + reverbAmount * 0.5f;        // 0.5 -> 1.0
        reverbParams.freezeMode = 0.0f;

        reverb.setParameters(reverbParams);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);
    }

    // ======================================================================
    // STEP 2: DELAY
    // Delay kicks in around 15%, builds feedback and mix.
    // Synced-style delay: ~375ms (like dotted 8th at 120bpm feel)
    // ======================================================================
    {
        float delayNorm = (norm < 0.15f) ? 0.0f : (norm - 0.15f) / 0.85f;
        delayNorm = std::min(delayNorm, 1.0f);

        // Delay time: 200ms to 400ms as intensity increases
        float delayTimeMs = 200.0f + delayNorm * 200.0f;
        float delaySamples = static_cast<float>(delayTimeMs * currentSampleRate / 1000.0);

        delayLineL.setDelay(delaySamples);
        delayLineR.setDelay(delaySamples);

        // Feedback and mix increase with intensity
        float feedback = delayNorm * 0.6f;  // 0 -> 0.6
        float delayMix = delayNorm * 0.5f;  // 0 -> 0.5

        auto* dataL = buffer.getWritePointer(0);
        auto* dataR = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            float currentSmooth = smoothedIntensity.getNextValue();
            juce::ignoreUnused(currentSmooth);

            // Read from delay
            float delayedL = delayLineL.popSample(0);
            float delayedR = delayLineR.popSample(0);

            // Write input + feedback into delay
            delayLineL.pushSample(0, dataL[i] + delayedL * feedback);
            delayLineR.pushSample(0, dataR[i] + delayedR * feedback);

            // Mix delayed signal in
            dataL[i] = dataL[i] + delayedL * delayMix;
            dataR[i] = dataR[i] + delayedR * delayMix;
        }
    }

    // ======================================================================
    // STEP 3: WHITE NOISE
    // Noise fades in starting at 50%, becomes dominant at 100%.
    // At full intensity, the signal is almost entirely white noise.
    // ======================================================================
    {
        float noiseNorm = (norm < 0.5f) ? 0.0f : (norm - 0.5f) / 0.5f;
        noiseNorm = std::min(noiseNorm, 1.0f);

        if (noiseNorm > 0.0f)
        {
            // Noise level ramps up dramatically
            float noiseLevel = noiseNorm * noiseNorm * 0.9f;  // Exponential ramp, max 0.9

            // As noise increases, the processed signal fades a bit
            float signalLevel = 1.0f - (noiseNorm * noiseNorm * 0.5f); // Down to 0.5 at max

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float noise = (noiseRng.nextFloat() * 2.0f - 1.0f) * noiseLevel;
                    data[i] = data[i] * signalLevel + noise;
                }
            }
        }
    }

    // ======================================================================
    // DRY/WET MIX
    // At 0% intensity, output is fully dry. As intensity increases,
    // the processed signal takes over.
    // ======================================================================
    {
        float wetMix = norm;
        float dryMix = 1.0f - wetMix * 0.3f; // Keep some dry signal even at full

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wetData = buffer.getWritePointer(ch);
            auto* dryData = dryBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                // Crossfade between dry and processed
                wetData[i] = dryData[i] * (1.0f - wetMix) + wetData[i] * wetMix;
            }
        }
    }
}

// ==============================================================================
// Create Editor
// ==============================================================================

juce::AudioProcessorEditor* EndlessSmileSidechainEffectAudioProcessor::createEditor()
{
    return new EndlessSmileSidechainEffectAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management
// ==============================================================================

void EndlessSmileSidechainEffectAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EndlessSmileSidechainEffectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EndlessSmileSidechainEffectAudioProcessor();
}
