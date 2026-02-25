#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// Parameter Layout - Define all controllable parameters
// ==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
WashoutSmileAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // INTENSITY: 0.0 = 0%, 1.0 = 110%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "INTENSITY", 1 },
        "Intensity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f,
        "%"
    ));

    // MODE: 0 = Lift Off, 1 = Stratosphere, 2 = Dada Dynamite
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "MODE", 1 },
        "Mode",
        juce::StringArray { "Lift Off", "Stratosphere", "Dada Dynamite" },
        0
    ));

    return layout;
}

// ==============================================================================
// Constructor
// ==============================================================================

WashoutSmileAudioProcessor::WashoutSmileAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

WashoutSmileAudioProcessor::~WashoutSmileAudioProcessor()
{
}

// ==============================================================================
// Prepare to Play - Called before playback starts
// ==============================================================================

void WashoutSmileAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Highpass filter
    highpassFilter.prepare(spec);
    highpassFilter.reset();
    highpassFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    highpassFilter.setCutoffFrequency(20.0f);

    // Reverb
    reverb.prepare(spec);
    reverb.reset();
    reverbDryWet.prepare(spec);
    reverbDryWet.reset();

    // Delay
    const int maxDelaySamples = static_cast<int>(sampleRate * 2.0);
    delayLine.setMaximumDelayInSamples(maxDelaySamples);
    delayLine.prepare(spec);
    delayLine.reset();
    delayDryWet.prepare(spec);
    delayDryWet.reset();
    delayTimeSmoothed.reset(sampleRate, 0.05);

    // Phaser
    phaser.prepare(spec);
    phaser.reset();
    phaserDryWet.prepare(spec);
    phaserDryWet.reset();

    // Smoothed values
    intensitySmoothed.reset(sampleRate, 0.03);
    highpassFreqSmoothed.reset(sampleRate, 0.03);
    noiseGainSmoothed.reset(sampleRate, 0.03);
    satDriveSmoothed.reset(sampleRate, 0.02);
    bitcrushMixSmoothed.reset(sampleRate, 0.03);
    stereoWidthSmoothed.reset(sampleRate, 0.03);
    riserPhaseSmoothed.reset(sampleRate, 0.05);

    // Pre-allocate buffers
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);

    // Reset Shepard phases
    for (int i = 0; i < kNumShepardOscs; ++i)
        shepardPhases[i] = 0.0f;

    // Reset bitcrusher
    bitcrushHoldL = 0.0f;
    bitcrushHoldR = 0.0f;
    bitcrushCounter = 0;
}

// ==============================================================================
// Release Resources - Called when playback stops
// ==============================================================================

void WashoutSmileAudioProcessor::releaseResources()
{
    // Optional: Clean up any resources allocated in prepareToPlay
}

// ==============================================================================
// Process Block - Main DSP processing (called on audio thread)
// ==============================================================================

void WashoutSmileAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Read parameters
    const float intensityRaw = parameters.getRawParameterValue("INTENSITY")->load();
    const int mode = static_cast<int>(parameters.getRawParameterValue("MODE")->load());

    // Intensity 0–1 maps to 0–110%. Scale to 0–1.1 for computations.
    const float intensity = intensityRaw * 1.1f;

    intensitySmoothed.setTargetValue(intensity);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // ── Compute per-block DSP targets based on intensity and mode ──

    // Highpass sweep: 20 Hz at 0%, up to 8000 Hz at 110%
    const float hpFreq = 20.0f + std::pow(intensity, 2.0f) * 7980.0f;
    highpassFreqSmoothed.setTargetValue(hpFreq);

    // Reverb mix: 0% at 0, 60% at 100% for Lift Off
    const float reverbMix = std::min(intensity * 0.6f, 0.6f);

    // Stereo width: 0 at 0, 0.5 at 100%
    const float stereoWidth = std::min(intensity * 0.5f, 0.5f);
    stereoWidthSmoothed.setTargetValue(stereoWidth);

    // === Mode-dependent features ===
    // Stratosphere and Dada add more effects
    const bool hasDelay = (mode >= 1);
    const bool hasNoise = (mode >= 1);
    const bool hasPhaser = (mode >= 1);
    const bool hasRiser = (mode >= 2);
    const bool hasBitcrush = (mode >= 2);
    const bool hasSaturation = (mode >= 2);

    // Delay feedback: 0 to 50% from intensity 15–100%
    const float delayFeedback = hasDelay ? std::clamp((intensity - 0.15f) / 0.85f, 0.0f, 1.0f) * 0.5f : 0.0f;
    const float delayMix = hasDelay ? std::clamp((intensity - 0.15f) / 0.85f, 0.0f, 1.0f) * 0.4f : 0.0f;

    // 1/4 note delay at ~130 BPM = ~461 ms
    const float delayMs = 461.0f;
    delayTimeSmoothed.setTargetValue(delayMs * static_cast<float>(currentSampleRate) / 1000.0f);

    // Noise: fades in from 60% intensity, louder toward 110%
    const float noiseGain = hasNoise ? std::clamp((intensity - 0.6f) / 0.5f, 0.0f, 1.0f) * 0.12f : 0.0f;
    noiseGainSmoothed.setTargetValue(noiseGain);

    // Phaser: from 20% intensity, rate increases with intensity
    const float phaserRate = hasPhaser ? 0.2f + intensity * 4.0f : 0.0f;
    const float phaserDepth = hasPhaser ? std::clamp((intensity - 0.2f) / 0.8f, 0.0f, 1.0f) * 0.7f : 0.0f;
    const float phaserMix = hasPhaser ? std::clamp((intensity - 0.2f) / 0.8f, 0.0f, 1.0f) * 0.35f : 0.0f;

    // Saturation drive: kicks in from 30% intensity (Dada only)
    const float satDrive = hasSaturation ? 1.0f + std::clamp((intensity - 0.3f) / 0.8f, 0.0f, 1.0f) * 8.0f : 1.0f;
    satDriveSmoothed.setTargetValue(satDrive);

    // Bitcrusher: kicks in above 80% (Dada only)
    const float bitcrushAmount = hasBitcrush ? std::clamp((intensity - 0.8f) / 0.3f, 0.0f, 1.0f) : 0.0f;
    bitcrushMixSmoothed.setTargetValue(bitcrushAmount);

    // Shepard riser rate: increases with intensity (Dada only)
    const float riserGain = hasRiser ? std::clamp((intensity - 0.1f) / 0.9f, 0.0f, 1.0f) * 0.08f : 0.0f;
    const float riserSpeed = hasRiser ? 0.5f + intensity * 3.0f : 0.0f;
    riserPhaseSmoothed.setTargetValue(riserGain);

    // ══════════════════════════════════════════════════════════════
    // STAGE 1: HIGHPASS FILTER (all modes)
    // ══════════════════════════════════════════════════════════════
    // Apply per-sample smoothed cutoff
    for (int i = 0; i < numSamples; ++i)
    {
        float freq = highpassFreqSmoothed.getNextValue();
        freq = std::clamp(freq, 20.0f, 20000.0f);
        highpassFilter.setCutoffFrequency(freq);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float sample = data[i];
            // Process single sample through filter
            float filtered = highpassFilter.processSample(ch, sample);
            data[i] = filtered;
        }
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 2: REVERB (all modes)
    // ══════════════════════════════════════════════════════════════
    {
        juce::dsp::Reverb::Parameters reverbParams;
        reverbParams.roomSize = 0.6f + intensity * 0.3f;
        reverbParams.damping = 0.4f;
        reverbParams.wetLevel = 1.0f;
        reverbParams.dryLevel = 0.0f;
        reverbParams.width = 1.0f;
        reverb.setParameters(reverbParams);

        reverbDryWet.setWetMixProportion(reverbMix);

        juce::dsp::AudioBlock<float> block(buffer);
        reverbDryWet.pushDrySamples(block);

        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        reverbDryWet.mixWetSamples(block);
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 3: PHASER (Stratosphere + Dada)
    // ══════════════════════════════════════════════════════════════
    if (hasPhaser && phaserMix > 0.001f)
    {
        phaser.setRate(phaserRate);
        phaser.setDepth(phaserDepth);
        phaser.setCentreFrequency(1500.0f);
        phaser.setFeedback(0.4f);
        phaser.setMix(1.0f);

        phaserDryWet.setWetMixProportion(phaserMix);

        juce::dsp::AudioBlock<float> block(buffer);
        phaserDryWet.pushDrySamples(block);

        juce::dsp::ProcessContextReplacing<float> context(block);
        phaser.process(context);

        phaserDryWet.mixWetSamples(block);
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 4: FEEDBACK DELAY (Stratosphere + Dada)
    // ══════════════════════════════════════════════════════════════
    if (hasDelay && delayMix > 0.001f)
    {
        delayDryWet.setWetMixProportion(delayMix);

        juce::dsp::AudioBlock<float> block(buffer);
        delayDryWet.pushDrySamples(block);

        const float fbk = std::min(delayFeedback, 0.95f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float delaySmp = delayTimeSmoothed.getNextValue();
                float input = data[i];
                float delayed = delayLine.popSample(ch, delaySmp, true);

                float feedbackSample = input + std::tanh(delayed * fbk);
                if (std::isnan(feedbackSample) || std::isinf(feedbackSample))
                    feedbackSample = 0.0f;

                delayLine.pushSample(ch, feedbackSample);
                data[i] = delayed;
            }
        }

        delayDryWet.mixWetSamples(block);
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 5: SATURATION (Dada only)
    // ══════════════════════════════════════════════════════════════
    if (hasSaturation)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float drive = satDriveSmoothed.getNextValue();
                float sample = data[i];
                float saturated = std::tanh(sample * drive);
                saturated /= std::max(drive, 1.0f);
                data[i] = saturated;
            }
        }
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 6: BITCRUSHER (Dada only, above 80%)
    // ══════════════════════════════════════════════════════════════
    if (hasBitcrush)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float crushMix = bitcrushMixSmoothed.getNextValue();
            if (crushMix < 0.001f) continue;

            // Reduce sample rate by holding samples
            // At max crush, hold every 16th sample
            int holdLen = std::max(1, static_cast<int>(1.0f + crushMix * 15.0f));

            bitcrushCounter++;
            if (bitcrushCounter >= holdLen)
            {
                bitcrushCounter = 0;
                if (numChannels > 0) bitcrushHoldL = buffer.getSample(0, i);
                if (numChannels > 1) bitcrushHoldR = buffer.getSample(1, i);
            }

            // Bit reduction: quantize amplitude
            float bits = 16.0f - crushMix * 12.0f; // 16-bit down to 4-bit
            float levels = std::pow(2.0f, bits);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float holdVal = (ch == 0) ? bitcrushHoldL : bitcrushHoldR;
                float quantized = std::round(holdVal * levels) / levels;
                float original = buffer.getSample(ch, i);
                float output = original + (quantized - original) * crushMix;
                buffer.setSample(ch, i, output);
            }
        }
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 7: SHEPARD TONE RISER (Dada only)
    // ══════════════════════════════════════════════════════════════
    if (hasRiser && riserGain > 0.001f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float gain = riserPhaseSmoothed.getNextValue();
            if (gain < 0.0001f) continue;

            float riserSample = 0.0f;

            // Multiple sine oscillators at octave intervals, fading in/out
            for (int osc = 0; osc < kNumShepardOscs; ++osc)
            {
                // Each oscillator at a different octave
                float freq = shepardBaseFreq * std::pow(2.0f, static_cast<float>(osc));

                // Continuously rise in pitch
                float phaseInc = (freq * (1.0f + riserSpeed * 0.1f)) / static_cast<float>(currentSampleRate);
                shepardPhases[osc] += phaseInc;
                if (shepardPhases[osc] >= 1.0f)
                    shepardPhases[osc] -= 1.0f;

                // Gaussian-like amplitude envelope based on log frequency position
                float logFreq = std::log2(freq * (1.0f + riserSpeed * 0.1f));
                float center = 9.0f; // ~512 Hz center
                float width = 3.0f;
                float amp = std::exp(-0.5f * std::pow((logFreq - center) / width, 2.0f));

                riserSample += std::sin(2.0f * juce::MathConstants<float>::pi * shepardPhases[osc]) * amp;
            }

            riserSample *= gain / static_cast<float>(kNumShepardOscs);

            if (std::isnan(riserSample) || std::isinf(riserSample))
                riserSample = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float current = buffer.getSample(ch, i);
                buffer.setSample(ch, i, current + riserSample);
            }
        }
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 8: NOISE GENERATOR (Stratosphere + Dada)
    // ══════════════════════════════════════════════════════════════
    if (hasNoise)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float nGain = noiseGainSmoothed.getNextValue();
            if (nGain < 0.0001f) continue;

            float noise = (noiseRng.nextFloat() * 2.0f - 1.0f) * nGain;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float current = buffer.getSample(ch, i);
                buffer.setSample(ch, i, current + noise);
            }
        }
    }

    // ══════════════════════════════════════════════════════════════
    // STAGE 9: STEREO WIDENING (all modes)
    // ══════════════════════════════════════════════════════════════
    if (numChannels >= 2)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float width = stereoWidthSmoothed.getNextValue();
            float L = buffer.getSample(0, i);
            float R = buffer.getSample(1, i);
            float mid = (L + R) * 0.5f;
            float side = (L - R) * 0.5f;
            side *= (1.0f + width);
            float outL = mid + side;
            float outR = mid - side;
            buffer.setSample(0, i, outL);
            buffer.setSample(1, i, outR);
        }
    }

    // ══════════════════════════════════════════════════════════════
    // FINAL: NaN/Inf guard on entire output
    // ══════════════════════════════════════════════════════════════
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::isnan(data[i]) || std::isinf(data[i]))
                data[i] = 0.0f;
        }
    }
}

// ==============================================================================
// Create Editor - Returns the plugin's GUI
// ==============================================================================

juce::AudioProcessorEditor* WashoutSmileAudioProcessor::createEditor()
{
    return new WashoutSmileAudioProcessorEditor(*this);
}

// ==============================================================================
// State Management - Save/Load plugin state (presets, DAW recall)
// ==============================================================================

void WashoutSmileAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void WashoutSmileAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new WashoutSmileAudioProcessor();
}
