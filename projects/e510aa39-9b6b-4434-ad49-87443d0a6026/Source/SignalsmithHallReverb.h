/**
 * Signalsmith FDN Hall Reverb
 *
 * Based on the Signalsmith Audio basics library Reverb class (MIT):
 *   https://github.com/Signalsmith-Audio/signalsmith-audio-basics
 *   Author: Geraint Luff / Signalsmith Audio Ltd; License: MIT
 *
 * Algorithm: Feedback Delay Network (FDN) with:
 *   - 4 cascaded Hadamard diffusion stages (pre-FDN)
 *   - 8-channel FDN with Householder feedback mixing
 *   - LFO modulation on 3 of 8 delay channels (~2ms depth)
 *   - Pseudorandom delay lengths (4 seeds, exponential spacing)
 *   - Multi-band absorption (separate LP + HP damping)
 *
 * How it works:
 *   The input is first smeared through 4 cascaded Hadamard diffusion stages.
 *   Each stage splits the signal into N channels, applies short allpass-like
 *   delays, then recombines via a Hadamard matrix. This creates dense, smooth
 *   early reflections instead of discrete echoes.
 *
 *   The diffused signal then enters the 8-channel FDN loop:
 *     Diffused input → distribute to 8 delay lines (pseudorandom lengths × size)
 *     ↓
 *     Read outputs → 8×8 Householder feedback matrix
 *       H = I - (2/N) * ones * ones^T
 *     ↓
 *     Multi-band absorption: LP damping (brightness) + HP damping (warmth)
 *     ↓
 *     LFO modulation on channels 1, 3, 5 (~2ms depth, prevents metallic buildup)
 *     ↓
 *     × decay → + input inject → write back to delays
 *     ↓
 *     Output L and R: alternating-sign tap sums for stereo decorrelation
 *
 * Character:
 *   Clean, spacious, and modern-sounding hall reverb with extremely smooth
 *   tail density. The Hadamard diffusion stages create a dense onset that
 *   classic FDN designs lack. LFO modulation prevents metallic ringing at
 *   long decay times. Multi-band absorption gives independent control over
 *   high and low frequency tail behaviour.
 *
 * Sonic tags: clean, modern, spacious, transparent, smooth, large, airy, dense
 * Use cases: orchestral, choir, ambient pads, any source requiring a large
 *            neutral space without adding "character" or colouration
 * Limitations: Very long decay times can obscure rhythmic content; use
 *              shorter decay_seconds for tempo-sensitive material
 *
 * Source URL: https://github.com/Signalsmith-Audio/signalsmith-audio-basics (MIT)
 * License: MIT
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

// ============================================================================
// FDN Delay Line with fractional read for LFO modulation
// ============================================================================

class FdnDelayLine
{
public:
    void prepare(int maxDelaySamples)
    {
        size = std::max(maxDelaySamples + 4, 8); // extra margin for interpolation
        buffer.assign(size, 0.0f);
        writeIdx = 0;
    }

    // Read at a fixed integer delay (oldest sample when tap == size)
    float read(int tap) const
    {
        tap = juce::jlimit(1, size - 1, tap);
        const int idx = (writeIdx - tap + size) % size;
        return buffer[idx];
    }

    // Read at a fractional delay (linear interpolation) for LFO modulation
    float readInterp(float tap) const
    {
        tap = juce::jlimit(1.0f, static_cast<float>(size - 2), tap);
        const int   iTap = static_cast<int>(tap);
        const float frac = tap - static_cast<float>(iTap);
        const int   idx0 = (writeIdx - iTap     + size) % size;
        const int   idx1 = (writeIdx - iTap - 1 + size) % size;
        return buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);
    }

    // Write new input and advance pointer
    void write(float x)
    {
        buffer[writeIdx] = x;
        writeIdx = (writeIdx + 1) % size;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

private:
    std::vector<float> buffer;
    int size     = 8;
    int writeIdx = 0;
};

// ============================================================================
// Hadamard Diffusion Stage
//
// Each stage: N short allpass delays → N×N Hadamard matrix mix.
// 4 cascaded stages create dense, smooth onset before the FDN loop.
// Uses in-place Hadamard transform (recursive butterfly).
// ============================================================================

class HadamardDiffuser
{
public:
    static constexpr int N = 8;

    void prepare(float sampleRate, float baseLengthMs, int seed)
    {
        // Generate pseudorandom delay lengths from seed
        // Exponential spacing starting from baseLengthMs
        unsigned int rng = static_cast<unsigned int>(seed * 1073741827 + 12345);
        for (int i = 0; i < N; ++i)
        {
            rng = rng * 1664525u + 1013904223u; // LCG
            float jitter = 0.7f + 0.6f * static_cast<float>(rng & 0xFFFF) / 65535.0f;
            float lenMs  = baseLengthMs * jitter;
            int   lenSamples = std::max(1, juce::roundToInt(lenMs / 1000.0f * sampleRate));
            delayBuf[i].assign(lenSamples, 0.0f);
            delaySize[i] = lenSamples;
            writeIdx[i]  = 0;
        }
    }

    // Process N channels in-place through this diffusion stage
    void process(float* channels)
    {
        // Read from each delay line, write input, advance
        float delayed[N];
        for (int i = 0; i < N; ++i)
        {
            delayed[i] = delayBuf[i][writeIdx[i]];
            delayBuf[i][writeIdx[i]] = channels[i];
            writeIdx[i] = (writeIdx[i] + 1) % delaySize[i];
        }

        // In-place Hadamard transform (Walsh-Hadamard, recursive butterfly)
        hadamardTransform(delayed);

        // Normalise by 1/sqrt(N) to preserve energy
        const float norm = 1.0f / std::sqrt(static_cast<float>(N));
        for (int i = 0; i < N; ++i)
            channels[i] = delayed[i] * norm;
    }

    void reset()
    {
        for (int i = 0; i < N; ++i)
        {
            std::fill(delayBuf[i].begin(), delayBuf[i].end(), 0.0f);
            writeIdx[i] = 0;
        }
    }

private:
    std::vector<float> delayBuf[N];
    int delaySize[N] = {};
    int writeIdx[N]  = {};

    // In-place Walsh-Hadamard transform for power-of-2 length
    static void hadamardTransform(float* data)
    {
        for (int len = 1; len < N; len <<= 1)
        {
            for (int i = 0; i < N; i += len << 1)
            {
                for (int j = i; j < i + len; ++j)
                {
                    float a = data[j];
                    float b = data[j + len];
                    data[j]       = a + b;
                    data[j + len] = a - b;
                }
            }
        }
    }
};

// ============================================================================
// Signalsmith Hall Reverb (8-channel FDN with Hadamard diffusion)
// ============================================================================

class SignalsmithHallReverb
{
public:
    static constexpr int N = 8;
    static constexpr int kNumDiffusionStages = 4;

    // Indices of delay channels with LFO modulation (3 of 8)
    static constexpr int kModChannels[3] = { 1, 3, 5 };

    //==========================================================================
    // Parameters
    struct Params
    {
        float decaySeconds = 2.0f;  // 0.1–20.0s  RT60 tail length
        float brightness   = 0.7f;  // 0.0–1.0    HF content in tail (0=dark, 1=bright)
        float size         = 0.8f;  // 0.0–1.0    room size (scales delay lengths)
        float mix          = 0.35f; // 0.0–1.0    dry/wet blend
    };

    //==========================================================================
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);

        // ---- Hadamard diffusion stages ----
        // Each stage uses progressively longer delays for broadening density
        // Seeds are arbitrary primes for decorrelated pseudorandom lengths
        const float diffusionBaseLengths[kNumDiffusionStages] = { 1.0f, 1.5f, 2.2f, 3.3f };
        const int   diffusionSeeds[kNumDiffusionStages] = { 7, 13, 23, 37 };
        for (int s = 0; s < kNumDiffusionStages; ++s)
            diffusers[s].prepare(sr, diffusionBaseLengths[s], diffusionSeeds[s]);

        // ---- FDN delay lines ----
        rebuildDelays(1.0f);
        currentSize = 1.0f;

        // ---- Filter states ----
        std::fill(lpState, lpState + N, 0.0f);
        std::fill(hpState, hpState + N, 0.0f);

        // ---- LFO ----
        lfoPhase = 0.0f;
    }

    //==========================================================================
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        const float decaySeconds = juce::jlimit(0.1f, 20.0f, params.decaySeconds);
        const float brightness   = juce::jlimit(0.0f, 1.0f,  params.brightness);
        const float size         = juce::jlimit(0.0f, 1.0f,  params.size);
        const float mix          = juce::jlimit(0.0f, 1.0f,  params.mix);

        // Rebuild delay lines if room size changed significantly
        if (std::abs(size - currentSize) > 0.01f)
        {
            rebuildDelays(size);
            currentSize = size;
        }

        // Per-line feedback gain from RT60
        const float sizeScale    = 0.3f + 0.7f * size;
        const float avgDelayMs   = 38.0f * sizeScale;
        const float avgDelaySecs = avgDelayMs / 1000.0f;
        const float decayGain    = juce::jlimit(0.0f, 0.9999f,
            std::pow(10.0f, -3.0f * avgDelaySecs / decaySeconds));

        // Multi-band absorption coefficients
        // LP: brightness=1 → no LP (lpAlpha=0), brightness=0 → strong LP (lpAlpha=0.85)
        const float lpAlpha = (1.0f - brightness) * 0.85f;
        // HP: gentle low-cut to prevent bass buildup (fixed coefficient)
        const float hpAlpha = 0.03f;

        // Equal-power dry/wet crossfade
        const float angle   = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos(angle);
        const float wetGain = std::sin(angle);

        // LFO rate: slow (~0.5 Hz) for subtle detuning
        const float lfoRate  = 0.5f;
        const float lfoInc   = lfoRate / sr;
        // Modulation depth in samples (~2ms at current sample rate)
        const float modDepthSamples = sr * 0.002f;

        for (int s = 0; s < numSamples; ++s)
        {
            // ---- Mono input sum ----
            float inputMono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputMono += buffer.getSample(ch, s);
            if (numChannels > 1)
                inputMono /= static_cast<float>(numChannels);

            // ---- Hadamard diffusion (pre-FDN) ----
            // Distribute input to N channels, then diffuse
            float diffChannels[N];
            for (int i = 0; i < N; ++i)
                diffChannels[i] = inputMono * 0.1f; // input injection level

            for (int d = 0; d < kNumDiffusionStages; ++d)
                diffusers[d].process(diffChannels);

            // ---- Advance LFO ----
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

            // ---- Step 1: Read current outputs from all delay lines ----
            float state[N];
            for (int i = 0; i < N; ++i)
                state[i] = delays[i].read(nominalDelaySamples[i]);

            // LFO-modulated reads for channels 1, 3, 5
            // Each gets a different LFO phase offset (120 degrees apart)
            for (int m = 0; m < 3; ++m)
            {
                const int ch = kModChannels[m];
                const float phaseOffset = static_cast<float>(m) / 3.0f;
                float phase = lfoPhase + phaseOffset;
                if (phase >= 1.0f) phase -= 1.0f;
                float lfoVal = std::sin(juce::MathConstants<float>::twoPi * phase);
                float modTap = static_cast<float>(nominalDelaySamples[ch]) + lfoVal * modDepthSamples;
                state[ch] = delays[ch].readInterp(modTap);
            }

            // ---- Step 2: Householder mixing matrix ----
            float sumState = 0.0f;
            for (int i = 0; i < N; ++i)
                sumState += state[i];
            const float householderMix = (2.0f / static_cast<float>(N)) * sumState;

            // ---- Step 3: Multi-band absorption + decay + diffused input → write ----
            for (int i = 0; i < N; ++i)
            {
                float mixed = state[i] - householderMix;

                // LP filter (brightness control — rolls off highs)
                lpState[i] = lpAlpha * lpState[i] + (1.0f - lpAlpha) * mixed;

                // HP filter (prevents bass buildup)
                // y[n] = (1-a) * (y[n-1] + x[n] - x[n-1])  approximated as:
                hpState[i] = hpState[i] + hpAlpha * (lpState[i] - hpState[i]);
                float filtered = lpState[i] - hpState[i];

                // Decay gain + inject diffused input
                const float injectSign = (i % 2 == 0) ? 1.0f : -1.0f;
                delays[i].write(filtered * decayGain + injectSign * diffChannels[i]);
            }

            // ---- Step 4: Stereo output — alternating-sign tap sums ----
            float outL = 0.0f, outR = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
                outL += sign * state[i];
                outR -= sign * state[i];
            }
            outL /= static_cast<float>(N);
            outR /= static_cast<float>(N);

            // ---- Dry/wet blend (equal-power) ----
            const float dryL = (numChannels >= 2) ? buffer.getSample(0, s) : inputMono;
            const float dryR = (numChannels >= 2) ? buffer.getSample(1, s) : inputMono;

            if (numChannels >= 2)
            {
                buffer.setSample(0, s, dryGain * dryL + wetGain * outL);
                buffer.setSample(1, s, dryGain * dryR + wetGain * outR);
            }
            else
            {
                buffer.setSample(0, s, dryGain * dryL + wetGain * 0.5f * (outL + outR));
            }
        }
    }

    //==========================================================================
    void reset()
    {
        for (auto& d : delays)    d.reset();
        for (auto& df : diffusers) df.reset();
        std::fill(lpState, lpState + N, 0.0f);
        std::fill(hpState, hpState + N, 0.0f);
        lfoPhase = 0.0f;
    }

private:
    float sr          = 44100.0f;
    float currentSize = 1.0f;
    float lfoPhase    = 0.0f;

    std::array<FdnDelayLine, N> delays;
    int nominalDelaySamples[N] = {};
    float lpState[N] = {};
    float hpState[N] = {};

    std::array<HadamardDiffuser, kNumDiffusionStages> diffusers;

    // Pseudorandom delay lengths generated from 4 seeds with exponential spacing.
    // This replaces the hand-picked prime lengths — the Signalsmith approach uses
    // LCG-seeded exponential distributions for better decorrelation.
    void rebuildDelays(float size)
    {
        const float sizeScale = 0.3f + 0.7f * size;

        // 4 seeds with exponential spacing: each pair of channels shares a seed
        // but with different offsets, creating 8 mutually inharmonic lengths.
        const unsigned int seeds[4] = { 31, 59, 97, 127 };
        for (int i = 0; i < N; ++i)
        {
            unsigned int rng = seeds[i / 2] + static_cast<unsigned int>(i) * 7919u;
            rng = rng * 1664525u + 1013904223u;

            // Base range: 15ms to 65ms, exponentially distributed
            float t = static_cast<float>(rng & 0xFFFF) / 65535.0f;
            float delayMs = 15.0f * std::exp(t * std::log(65.0f / 15.0f));
            delayMs *= sizeScale;

            int delaySamples = std::max(1, juce::roundToInt(delayMs / 1000.0f * sr));
            nominalDelaySamples[i] = delaySamples;
            // Allocate extra for LFO modulation headroom
            delays[i].prepare(delaySamples + juce::roundToInt(sr * 0.003f));
        }

        std::fill(lpState, lpState + N, 0.0f);
        std::fill(hpState, hpState + N, 0.0f);
    }
};

// Static member definitions
constexpr int SignalsmithHallReverb::kModChannels[3];
