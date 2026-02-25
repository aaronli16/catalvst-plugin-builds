#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <random>

// ==============================================================================
// BuildupEngine — Single-knob multi-FX processor for EDM buildups
// ==============================================================================
// Three preset modes with stacked effects controlled by one INTENSITY knob.
// All DSP objects pre-allocated in prepare(), no heap allocations in process().
// ==============================================================================

class BuildupEngine
{
public:
    enum class PresetMode { LiftOff = 0, Stratosphere, DadaDynamite };

    BuildupEngine() = default;

    void prepare(double sampleRate, int maxBlockSize, int numChannels)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = maxBlockSize;
        currentNumChannels = numChannels;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
        spec.numChannels = static_cast<juce::uint32>(numChannels);

        // --- Highpass filter ---
        highpassFilter.prepare(spec);
        highpassFilter.reset();
        highpassFilter.setMode(juce::dsp::LadderFilterMode::HPF24);
        highpassFilter.setCutoffFrequencyHz(20.0f);
        highpassFilter.setResonance(0.1f);

        // --- Reverb ---
        reverb.prepare(spec);
        reverb.reset();
        reverbDryWet.prepare(spec);
        reverbDryWet.reset();

        // --- Delay ---
        const int maxDelaySamples = static_cast<int>(sampleRate * 2.0);
        delayLine.setMaximumDelayInSamples(maxDelaySamples);
        delayLine.prepare(spec);
        delayLine.reset();
        delayDryWet.prepare(spec);
        delayDryWet.reset();

        // --- Phaser ---
        phaser.prepare(spec);
        phaser.reset();
        phaserDryWet.prepare(spec);
        phaserDryWet.reset();

        // --- Pre-allocate dry buffer for stereo widening and other manual processing ---
        dryBuffer.setSize(numChannels, maxBlockSize);

        // --- SmoothedValues ---
        intensitySmoothed.reset(sampleRate, 0.05);     // 50ms ramp
        hpfCutoffSmoothed.reset(sampleRate, 0.02);     // 20ms for filter sweeps
        reverbMixSmoothed.reset(sampleRate, 0.05);
        delayFeedbackSmoothed.reset(sampleRate, 0.05);
        delayMixSmoothed.reset(sampleRate, 0.05);
        phaserMixSmoothed.reset(sampleRate, 0.05);
        noiseGainSmoothed.reset(sampleRate, 0.05);
        stereoWidthSmoothed.reset(sampleRate, 0.05);
        satDriveSmoothed.reset(sampleRate, 0.02);
        bitcrushMixSmoothed.reset(sampleRate, 0.05);
        riserGainSmoothed.reset(sampleRate, 0.05);

        // --- Shepard tone oscillator phases ---
        for (int i = 0; i < kNumShepardOsc; ++i)
            shepardPhase[i] = 0.0;

        // --- Noise RNG ---
        rng.seed(42);

        // --- Bitcrusher state ---
        for (int ch = 0; ch < 2; ++ch)
            bitcrushHold[ch] = 0.0f;
        bitcrushCounter = 0;
    }

    void reset()
    {
        highpassFilter.reset();
        reverb.reset();
        reverbDryWet.reset();
        delayLine.reset();
        delayDryWet.reset();
        phaser.reset();
        phaserDryWet.reset();

        for (int i = 0; i < kNumShepardOsc; ++i)
            shepardPhase[i] = 0.0;

        for (int ch = 0; ch < 2; ++ch)
            bitcrushHold[ch] = 0.0f;
        bitcrushCounter = 0;
    }

    // --------------------------------------------------------------------------
    // Main process call — intensity is 0.0 to 1.0 (representing 0–110%)
    // mode: 0 = LiftOff, 1 = Stratosphere, 2 = DadaDynamite
    // --------------------------------------------------------------------------
    void processBlock(juce::AudioBuffer<float>& buffer, float intensity, int mode)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const auto presetMode = static_cast<PresetMode>(juce::jlimit(0, 2, mode));

        // Map normalized 0–1 to 0–110 percentage scale
        const float intensityPct = intensity * 110.0f;

        // Exponential curve for dramatic response at high values
        const float expIntensity = std::pow(intensity, 1.8f);

        intensitySmoothed.setTargetValue(intensity);

        // ==================================================================
        // 1. HIGHPASS FILTER SWEEP (all presets)
        // ==================================================================
        {
            // HPF: 20Hz at 0%, sweeps to ~8kHz at 100%
            // Exponential frequency mapping
            const float minFreq = 20.0f;
            const float maxFreq = 8000.0f;
            float hpfTarget = minFreq * std::pow(maxFreq / minFreq, expIntensity);
            hpfTarget = juce::jlimit(20.0f, 18000.0f, hpfTarget);

            hpfCutoffSmoothed.setTargetValue(hpfTarget);
            highpassFilter.setCutoffFrequencyHz(hpfCutoffSmoothed.skip(numSamples));

            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            highpassFilter.process(context);
        }

        // ==================================================================
        // 2. REVERB (all presets)
        // ==================================================================
        {
            // Reverb: dry at 0%, up to 60% wet at 100%
            float reverbMix = juce::jlimit(0.0f, 0.6f, expIntensity * 0.6f);
            reverbMixSmoothed.setTargetValue(reverbMix);

            juce::dsp::Reverb::Parameters reverbParams;
            reverbParams.roomSize = 0.4f + expIntensity * 0.5f; // 0.4 to 0.9
            reverbParams.damping = 0.5f - expIntensity * 0.3f;  // 0.5 to 0.2
            reverbParams.wetLevel = 1.0f;  // DryWetMixer controls blend
            reverbParams.dryLevel = 0.0f;
            reverbParams.width = 1.0f;
            reverb.setParameters(reverbParams);

            reverbDryWet.setWetMixProportion(reverbMixSmoothed.skip(numSamples));

            juce::dsp::AudioBlock<float> block(buffer);
            reverbDryWet.pushDrySamples(block);
            juce::dsp::ProcessContextReplacing<float> context(block);
            reverb.process(context);
            reverbDryWet.mixWetSamples(block);
        }

        // ==================================================================
        // 3. STEREO WIDENING (all presets)
        // ==================================================================
        if (numChannels >= 2 && intensityPct > 10.0f)
        {
            float widthAmount = juce::jlimit(0.0f, 1.0f, (intensityPct - 10.0f) / 90.0f);
            stereoWidthSmoothed.setTargetValue(widthAmount * 0.5f); // max 50% extra width

            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);

            for (int i = 0; i < numSamples; ++i)
            {
                float w = stereoWidthSmoothed.getNextValue();
                float mid = (left[i] + right[i]) * 0.5f;
                float side = (left[i] - right[i]) * 0.5f;
                side *= (1.0f + w);
                left[i] = mid + side;
                right[i] = mid - side;
            }
        }

        // ==================================================================
        // 4. DELAY (Stratosphere + Dada)
        // ==================================================================
        if (presetMode >= PresetMode::Stratosphere && intensityPct > 5.0f)
        {
            // 1/4 note at 128 BPM = ~468ms, use fixed for now
            const float delayMs = 468.0f;
            const float delaySamples = delayMs * static_cast<float>(currentSampleRate) / 1000.0f;

            // Feedback: 0% at low intensity, up to 50%
            float feedbackNorm = juce::jlimit(0.0f, 1.0f, (intensityPct - 5.0f) / 95.0f);
            float feedback = feedbackNorm * 0.5f;
            delayFeedbackSmoothed.setTargetValue(feedback);

            float delayMix = feedbackNorm * 0.4f;
            delayMixSmoothed.setTargetValue(delayMix);
            delayDryWet.setWetMixProportion(delayMixSmoothed.skip(numSamples));

            juce::dsp::AudioBlock<float> block(buffer);
            delayDryWet.pushDrySamples(block);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float fb = delayFeedbackSmoothed.getNextValue();
                    float delayed = delayLine.popSample(ch, delaySamples, true);
                    float feedIn = data[i] + std::tanh(delayed * fb);

                    if (std::isnan(feedIn) || std::isinf(feedIn))
                        feedIn = 0.0f;

                    delayLine.pushSample(ch, feedIn);
                    data[i] = delayed;
                }
            }

            delayDryWet.mixWetSamples(block);
        }
        else
        {
            // Keep delay line fed with silence to avoid stale data
            for (int ch = 0; ch < numChannels; ++ch)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    delayLine.popSample(ch, 1.0f, true);
                    delayLine.pushSample(ch, 0.0f);
                }
            }
        }

        // ==================================================================
        // 5. PHASER / CHORUS (Stratosphere + Dada)
        // ==================================================================
        if (presetMode >= PresetMode::Stratosphere && intensityPct > 20.0f)
        {
            float phaserNorm = juce::jlimit(0.0f, 1.0f, (intensityPct - 20.0f) / 80.0f);
            phaserMixSmoothed.setTargetValue(phaserNorm * 0.5f);

            phaser.setRate(0.5f + phaserNorm * 4.0f);    // 0.5 to 4.5 Hz
            phaser.setDepth(0.3f + phaserNorm * 0.6f);
            phaser.setCentreFrequency(800.0f + phaserNorm * 2000.0f);
            phaser.setFeedback(0.3f + phaserNorm * 0.4f);
            phaser.setMix(1.0f);  // DryWetMixer controls blend

            phaserDryWet.setWetMixProportion(phaserMixSmoothed.skip(numSamples));

            juce::dsp::AudioBlock<float> block(buffer);
            phaserDryWet.pushDrySamples(block);
            juce::dsp::ProcessContextReplacing<float> context(block);
            phaser.process(context);
            phaserDryWet.mixWetSamples(block);
        }

        // ==================================================================
        // 6. NOISE GENERATOR (Stratosphere at 60%+, Dada at 50%+)
        // ==================================================================
        {
            float noiseThreshold = (presetMode == PresetMode::DadaDynamite) ? 50.0f : 60.0f;
            if (presetMode >= PresetMode::Stratosphere && intensityPct > noiseThreshold)
            {
                float noiseNorm = juce::jlimit(0.0f, 1.0f,
                    (intensityPct - noiseThreshold) / (110.0f - noiseThreshold));
                float noiseGain = noiseNorm * 0.12f; // Keep noise subtle
                noiseGainSmoothed.setTargetValue(noiseGain);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* data = buffer.getWritePointer(ch);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        float g = noiseGainSmoothed.getNextValue();
                        float noise = noiseDist(rng);
                        data[i] += noise * g;
                    }
                }
            }
        }

        // ==================================================================
        // 7. SHEPARD TONE RISER (Dada only)
        // ==================================================================
        if (presetMode == PresetMode::DadaDynamite && intensityPct > 10.0f)
        {
            float riserNorm = juce::jlimit(0.0f, 1.0f, (intensityPct - 10.0f) / 100.0f);
            float riserGain = riserNorm * 0.08f; // Subtle background riser
            riserGainSmoothed.setTargetValue(riserGain);

            // Base rate increases with intensity — faster spin at high values
            double baseFreq = 80.0 + riserNorm * 200.0; // 80Hz to 280Hz
            double speedMultiplier = 1.0 + riserNorm * 3.0; // Speeds up

            for (int i = 0; i < numSamples; ++i)
            {
                float riserSample = 0.0f;
                float g = riserGainSmoothed.getNextValue();

                for (int osc = 0; osc < kNumShepardOsc; ++osc)
                {
                    // Each oscillator is spaced one octave apart
                    double freq = baseFreq * std::pow(2.0, osc) * speedMultiplier;

                    // Wrap frequency into audible range with fade envelope
                    while (freq > 10000.0) freq *= 0.5;
                    while (freq < 40.0) freq *= 2.0;

                    // Gaussian-shaped amplitude envelope based on log frequency
                    double logFreq = std::log2(freq);
                    double center = std::log2(500.0); // Peak at ~500Hz
                    double sigma = 2.5;
                    double envelope = std::exp(-(logFreq - center) * (logFreq - center) / (2.0 * sigma * sigma));

                    shepardPhase[osc] += freq / currentSampleRate;
                    if (shepardPhase[osc] > 1.0) shepardPhase[osc] -= 1.0;

                    riserSample += static_cast<float>(
                        std::sin(shepardPhase[osc] * juce::MathConstants<double>::twoPi) * envelope
                    );
                }

                riserSample /= static_cast<float>(kNumShepardOsc);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float out = buffer.getSample(ch, i) + riserSample * g;
                    if (std::isnan(out) || std::isinf(out)) out = 0.0f;
                    buffer.setSample(ch, i, out);
                }
            }
        }

        // ==================================================================
        // 8. SATURATION (Dada at 40%+)
        // ==================================================================
        if (presetMode == PresetMode::DadaDynamite && intensityPct > 40.0f)
        {
            float satNorm = juce::jlimit(0.0f, 1.0f, (intensityPct - 40.0f) / 70.0f);
            float drive = 1.0f + satNorm * 6.0f; // 1x to 7x drive
            satDriveSmoothed.setTargetValue(drive);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float d = satDriveSmoothed.getNextValue();
                    float input = data[i];
                    float output = std::tanh(input * d);
                    output /= std::max(d * 0.5f, 1.0f); // Gain compensation
                    data[i] = output;
                }
            }
        }

        // ==================================================================
        // 9. BITCRUSHER (Dada at 80%+)
        // ==================================================================
        if (presetMode == PresetMode::DadaDynamite && intensityPct > 80.0f)
        {
            float crushNorm = juce::jlimit(0.0f, 1.0f, (intensityPct - 80.0f) / 30.0f);
            bitcrushMixSmoothed.setTargetValue(crushNorm * 0.5f); // Max 50% mix

            // Reduce sample rate: full rate down to ~1/16
            int decimation = 1 + static_cast<int>(crushNorm * 15.0f);
            // Reduce bit depth: 16 bits down to 4 bits
            float bitDepth = 16.0f - crushNorm * 12.0f;
            float quantLevels = std::pow(2.0f, bitDepth);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float mixAmt = bitcrushMixSmoothed.getNextValue();
                    float clean = data[i];

                    // Sample rate reduction
                    if (bitcrushCounter % decimation == 0)
                        bitcrushHold[ch] = data[i];

                    // Bit depth reduction
                    float crushed = std::round(bitcrushHold[ch] * quantLevels) / quantLevels;

                    // Mix crushed with clean
                    data[i] = clean * (1.0f - mixAmt) + crushed * mixAmt;

                    if (ch == numChannels - 1) bitcrushCounter++;
                }
            }
        }

        // Final safety clamp
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (std::isnan(data[i]) || std::isinf(data[i]))
                    data[i] = 0.0f;
                data[i] = juce::jlimit(-2.0f, 2.0f, data[i]);
            }
        }
    }

private:
    // --- Configuration ---
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int currentNumChannels = 2;

    // --- Highpass Filter ---
    juce::dsp::LadderFilter<float> highpassFilter;

    // --- Reverb ---
    juce::dsp::Reverb reverb;
    juce::dsp::DryWetMixer<float> reverbDryWet;

    // --- Delay ---
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 88200 };
    juce::dsp::DryWetMixer<float> delayDryWet;

    // --- Phaser ---
    juce::dsp::Phaser<float> phaser;
    juce::dsp::DryWetMixer<float> phaserDryWet;

    // --- Shepard Tone ---
    static constexpr int kNumShepardOsc = 6;
    double shepardPhase[kNumShepardOsc] = {};

    // --- Noise Generator ---
    std::mt19937 rng;
    std::uniform_real_distribution<float> noiseDist { -1.0f, 1.0f };

    // --- Bitcrusher ---
    float bitcrushHold[2] = {};
    int bitcrushCounter = 0;

    // --- Pre-allocated Buffers ---
    juce::AudioBuffer<float> dryBuffer;

    // --- Smoothed Parameters ---
    juce::SmoothedValue<float> intensitySmoothed;
    juce::SmoothedValue<float> hpfCutoffSmoothed;
    juce::SmoothedValue<float> reverbMixSmoothed;
    juce::SmoothedValue<float> delayFeedbackSmoothed;
    juce::SmoothedValue<float> delayMixSmoothed;
    juce::SmoothedValue<float> phaserMixSmoothed;
    juce::SmoothedValue<float> noiseGainSmoothed;
    juce::SmoothedValue<float> stereoWidthSmoothed;
    juce::SmoothedValue<float> satDriveSmoothed;
    juce::SmoothedValue<float> bitcrushMixSmoothed;
    juce::SmoothedValue<float> riserGainSmoothed;
};
