#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// ==============================================================================
// ShimmerEngine — Dreamy ambient shimmer reverb
// ==============================================================================
// Architecture:
//   Input -> [Dry path]
//            [Diffusion network -> Pitch shift feedback -> Reverb tail] -> Wet path
//   Output = Dry * (1 - mix) + Wet * mix
//
// The shimmer effect is achieved by feeding the reverb output through a
// pitch-shifted delay (octave up) and mixing it back into the reverb input,
// creating cascading crystalline overtones.
// ==============================================================================

class ShimmerEngine
{
public:
    ShimmerEngine() = default;

    void prepare(double sampleRate, int maxBlockSize, int numChannels)
    {
        sr = sampleRate;
        channels = numChannels;

        // Main reverb
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
        spec.numChannels = static_cast<juce::uint32>(numChannels);

        reverb.prepare(spec);
        reverb.reset();

        // Allpass diffusers for smearing the input
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kNumAllpass; ++i)
            {
                allpassDelays[ch][i].resize(static_cast<size_t>(sampleRate * 0.1));
                std::fill(allpassDelays[ch][i].begin(), allpassDelays[ch][i].end(), 0.0f);
                allpassWritePos[ch][i] = 0;
            }
        }

        // Shimmer pitch-shift delay line (simple granular octave-up)
        shimmerBufferSize = static_cast<int>(sampleRate * 2.0); // 2 seconds
        for (int ch = 0; ch < 2; ++ch)
        {
            shimmerBuffer[ch].resize(static_cast<size_t>(shimmerBufferSize), 0.0f);
            shimmerWritePos[ch] = 0;
            shimmerReadPos[ch] = 0.0;
        }

        // Pre-delay / early reflection buffer
        earlyBufferSize = static_cast<int>(sampleRate * 0.15);
        for (int ch = 0; ch < 2; ++ch)
        {
            earlyBuffer[ch].resize(static_cast<size_t>(earlyBufferSize), 0.0f);
            earlyWritePos[ch] = 0;
        }

        // Feedback buffer for shimmer accumulation
        for (int ch = 0; ch < 2; ++ch)
            shimmerAccum[ch] = 0.0f;

        // Damping filter (one-pole lowpass on wet signal)
        for (int ch = 0; ch < 2; ++ch)
            dampState[ch] = 0.0f;

        // Highpass filter state for shimmer path (removes mud from pitch shift)
        for (int ch = 0; ch < 2; ++ch)
            shimmerHPState[ch] = 0.0f;

        // Smoothing for parameters
        mixSmoothed.reset(sampleRate, 0.05);
        sizeSmoothed.reset(sampleRate, 0.05);
        shimmerSmoothed.reset(sampleRate, 0.05);
        decaySmoothed.reset(sampleRate, 0.05);
    }

    void reset()
    {
        reverb.reset();

        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill(shimmerBuffer[ch].begin(), shimmerBuffer[ch].end(), 0.0f);
            std::fill(earlyBuffer[ch].begin(), earlyBuffer[ch].end(), 0.0f);
            shimmerWritePos[ch] = 0;
            shimmerReadPos[ch] = 0.0;
            earlyWritePos[ch] = 0;
            shimmerAccum[ch] = 0.0f;
            dampState[ch] = 0.0f;
            shimmerHPState[ch] = 0.0f;

            for (int i = 0; i < kNumAllpass; ++i)
            {
                std::fill(allpassDelays[ch][i].begin(), allpassDelays[ch][i].end(), 0.0f);
                allpassWritePos[ch][i] = 0;
            }
        }
    }

    // Set parameters (normalized 0-1 for mix, size, shimmer; seconds for decay)
    void setMix(float v)     { mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setSize(float v)    { sizeSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setShimmer(float v) { shimmerSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setDecay(float seconds) { decaySmoothed.setTargetValue(juce::jlimit(0.1f, 20.0f, seconds)); }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(buffer.getNumChannels(), 2);

        // We need a copy of the dry signal
        juce::AudioBuffer<float> dryBuffer;
        dryBuffer.makeCopyOf(buffer);

        // Process sample by sample for parameter smoothing and shimmer feedback
        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = mixSmoothed.getNextValue();
            const float size = sizeSmoothed.getNextValue();
            const float shimmer = shimmerSmoothed.getNextValue();
            const float decay = decaySmoothed.getNextValue();

            // Update reverb parameters based on size and decay
            updateReverbParams(size, decay);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float drySample = dryBuffer.getSample(ch, i);
                float input = drySample;

                // Mix shimmer feedback into the reverb input
                float shimmerFeedback = shimmerAccum[ch] * shimmer * 0.7f;

                // Highpass the shimmer feedback to prevent mud buildup
                float hpCoeff = 0.95f;
                shimmerHPState[ch] = hpCoeff * (shimmerHPState[ch] + shimmerFeedback - shimmerAccum[ch] * shimmer * 0.7f);
                // Simpler approach: just use the shimmer feedback with damping
                input += shimmerFeedback;

                // Soft clip the input to prevent runaway feedback
                input = std::tanh(input * 0.8f) * 1.25f;

                // Early reflections (simple multi-tap delay)
                float earlyOut = processEarlyReflections(ch, input, size);

                // Write to buffer for reverb processing
                buffer.setSample(ch, i, earlyOut);
            }
        }

        // Process through JUCE reverb (operates on the full buffer)
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        // Post-reverb: generate shimmer and apply mix
        for (int i = 0; i < numSamples; ++i)
        {
            // We need to re-read mix since it was consumed above, but we'll approximate
            // by using the current target values for the post-processing
            const float mix = mixSmoothed.getCurrentValue();
            const float shimmer = shimmerSmoothed.getCurrentValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                float wetSample = buffer.getSample(ch, i);
                float drySample = dryBuffer.getSample(ch, i);

                // Generate shimmer: pitch shift the wet signal up one octave
                float pitched = pitchShiftOctaveUp(ch, wetSample);

                // Apply damping to shimmer accumulator
                float dampCoeff = 0.9985f; // Controls shimmer tail
                shimmerAccum[ch] = shimmerAccum[ch] * dampCoeff + pitched * 0.3f;

                // Soft limit the accumulator
                shimmerAccum[ch] = std::tanh(shimmerAccum[ch]);

                // Add shimmer sparkle directly to wet signal
                float sparkle = pitched * shimmer * 0.4f;

                // Damping filter on wet signal (subtle warmth)
                float dampFreq = 0.3f + (1.0f - shimmer * 0.3f) * 0.5f;
                dampState[ch] += dampFreq * (wetSample - dampState[ch]);
                float dampedWet = dampState[ch] + sparkle;

                // Mix dry/wet with equal power crossfade
                float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
                float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

                float output = drySample * dryGain + dampedWet * wetGain;

                buffer.setSample(ch, i, output);
            }
        }
    }

private:
    // Allpass diffuser
    static constexpr int kNumAllpass = 4;
    std::vector<float> allpassDelays[2][kNumAllpass];
    int allpassWritePos[2][kNumAllpass] = {};

    float processAllpass(int ch, int idx, float input, int delaySamples, float coefficient)
    {
        auto& buf = allpassDelays[ch][idx];
        int bufSize = static_cast<int>(buf.size());
        if (bufSize == 0 || delaySamples >= bufSize) return input;

        int readPos = (allpassWritePos[ch][idx] - delaySamples + bufSize) % bufSize;
        float delayed = buf[static_cast<size_t>(readPos)];

        float output = -coefficient * input + delayed;
        float writeVal = input + coefficient * delayed;

        buf[static_cast<size_t>(allpassWritePos[ch][idx])] = writeVal;
        allpassWritePos[ch][idx] = (allpassWritePos[ch][idx] + 1) % bufSize;

        return output;
    }

    // Early reflections
    std::vector<float> earlyBuffer[2];
    int earlyBufferSize = 0;
    int earlyWritePos[2] = {};

    float processEarlyReflections(int ch, float input, float size)
    {
        if (earlyBufferSize == 0) return input;

        earlyBuffer[ch][static_cast<size_t>(earlyWritePos[ch])] = input;

        // Multi-tap early reflections scaled by size
        float out = input * 0.5f;
        const float tapTimes[] = { 0.013f, 0.029f, 0.041f, 0.059f, 0.073f };
        const float tapGains[] = { 0.15f, 0.12f, 0.10f, 0.08f, 0.06f };

        for (int t = 0; t < 5; ++t)
        {
            int tapDelay = static_cast<int>(tapTimes[t] * sr * (0.3f + size * 0.7f));
            tapDelay = juce::jmin(tapDelay, earlyBufferSize - 1);
            int readPos = (earlyWritePos[ch] - tapDelay + earlyBufferSize) % earlyBufferSize;
            out += earlyBuffer[ch][static_cast<size_t>(readPos)] * tapGains[t];
        }

        earlyWritePos[ch] = (earlyWritePos[ch] + 1) % earlyBufferSize;

        // Run through allpass diffusers for smearing
        int baseSamples = static_cast<int>(sr * 0.005 * (0.5 + size * 0.5));
        out = processAllpass(ch, 0, out, baseSamples, 0.5f);
        out = processAllpass(ch, 1, out, static_cast<int>(baseSamples * 1.4), 0.5f);
        out = processAllpass(ch, 2, out, static_cast<int>(baseSamples * 1.9), 0.5f);
        out = processAllpass(ch, 3, out, static_cast<int>(baseSamples * 2.3), 0.5f);

        return out;
    }

    // Pitch shift one octave up using granular overlap technique
    std::vector<float> shimmerBuffer[2];
    int shimmerBufferSize = 0;
    int shimmerWritePos[2] = {};
    double shimmerReadPos[2] = {};

    float pitchShiftOctaveUp(int ch, float input)
    {
        if (shimmerBufferSize == 0) return 0.0f;

        // Write input to circular buffer
        shimmerBuffer[ch][static_cast<size_t>(shimmerWritePos[ch])] = input;
        shimmerWritePos[ch] = (shimmerWritePos[ch] + 1) % shimmerBufferSize;

        // Read at double speed for octave up
        const double pitchRatio = 2.0;
        const int grainSize = static_cast<int>(sr * 0.04); // 40ms grain

        // Two overlapping read heads for crossfade
        double readPos1 = shimmerReadPos[ch];
        double readPos2 = std::fmod(readPos1 + grainSize * 0.5, static_cast<double>(shimmerBufferSize));

        // Grain windowing (triangular crossfade)
        double grainPhase1 = std::fmod(readPos1, static_cast<double>(grainSize)) / grainSize;
        double grainPhase2 = std::fmod(readPos2, static_cast<double>(grainSize)) / grainSize;

        // Hann-style window
        float window1 = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * grainPhase1));
        float window2 = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * grainPhase2));

        // Read with linear interpolation
        float sample1 = readInterpolated(ch, readPos1) * window1;
        float sample2 = readInterpolated(ch, readPos2) * window2;

        // Advance read position at pitch ratio speed
        shimmerReadPos[ch] = std::fmod(shimmerReadPos[ch] + pitchRatio, static_cast<double>(shimmerBufferSize));

        return (sample1 + sample2) * 0.7f;
    }

    float readInterpolated(int ch, double pos) const
    {
        int idx0 = static_cast<int>(pos) % shimmerBufferSize;
        int idx1 = (idx0 + 1) % shimmerBufferSize;
        float frac = static_cast<float>(pos - std::floor(pos));

        if (idx0 < 0) idx0 += shimmerBufferSize;
        if (idx1 < 0) idx1 += shimmerBufferSize;

        return shimmerBuffer[ch][static_cast<size_t>(idx0)] * (1.0f - frac)
             + shimmerBuffer[ch][static_cast<size_t>(idx1)] * frac;
    }

    void updateReverbParams(float size, float decay)
    {
        juce::dsp::Reverb::Parameters reverbParams;
        reverbParams.roomSize = 0.3f + size * 0.65f;        // Large, spacious rooms
        reverbParams.damping = 0.2f + (1.0f - size) * 0.4f; // Less damping for bigger size
        reverbParams.wetLevel = 0.8f;                        // High wet internally (mix handled externally)
        reverbParams.dryLevel = 0.0f;                        // No dry from reverb (handled externally)
        reverbParams.width = 0.8f + size * 0.2f;             // Wider for bigger rooms
        reverbParams.freezeMode = 0.0f;

        // Decay influences damping — longer decay = less damping
        float decayNorm = juce::jlimit(0.0f, 1.0f, (decay - 0.1f) / 19.9f);
        reverbParams.damping = reverbParams.damping * (1.0f - decayNorm * 0.6f);
        reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, reverbParams.roomSize + decayNorm * 0.2f);

        reverb.setParameters(reverbParams);
    }

    // DSP members
    juce::dsp::Reverb reverb;
    float shimmerAccum[2] = {};
    float dampState[2] = {};
    float shimmerHPState[2] = {};
    double sr = 44100.0;
    int channels = 2;

    // Parameter smoothing
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> sizeSmoothed;
    juce::SmoothedValue<float> shimmerSmoothed;
    juce::SmoothedValue<float> decaySmoothed;
};
