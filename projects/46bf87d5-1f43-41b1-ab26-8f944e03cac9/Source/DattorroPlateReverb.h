/**
 * Dattorro Allpass Plate Reverb
 *
 * Based on Jon Dattorro's 1997 JAES paper:
 *   "Effect Design Part 1: Reverberator and Other Filters"
 *   J. Audio Eng. Soc., Vol. 45, No. 9, 1997 September
 *
 * Reference implementation adapted from el-visio/dattorro-verb (MIT):
 *   https://github.com/el-visio/dattorro-verb
 *   Original author: el-visio; License: MIT
 *
 * Algorithm topology (from the 1997 paper, Figure 15):
 *
 *   Input
 *     → Predelay (up to 100ms)
 *     → Input diffuser: 4 allpass stages (AP1-AP4)
 *         AP1: 142 samples, g=0.75
 *         AP2: 107 samples, g=0.75
 *         AP3: 379 samples, g=0.625
 *         AP4: 277 samples, g=0.625
 *     → Tank (two interlocked feedback paths):
 *         Path A: AP5(672,g=0.7) → D1(4453) → LP damp → × decay  → sum
 *         Path B: AP6(908,g=0.5) → D2(4217) → LP damp → × decay  → sum
 *         (Each path feeds into the other's input, creating the plate "tank")
 *     → Output taps: 8 taps read from D1, D2, AP5, AP6 at specific offsets
 *         for stereo decorrelation
 *
 * Delay lengths are Dattorro's original constants, scaled to actual sample rate
 * (original normalised to 29761 Hz).
 *
 * Character:
 *   Warm, lush plate reverb with a smooth diffuse tail — no flutter echo, no
 *   metallic ringing. The allpass diffusion topology produces the characteristic
 *   "velvet" density of classic plate hardware. Excels at vocals, synth pads,
 *   and acoustic instruments needing depth without boxiness.
 *
 * Sonic tags: warm, lush, diffuse, smooth, classic, deep
 * Use cases: vocals, synth pads, strings, piano, ambient textures, ballads
 * Limitations: Can smear transient attack at high decay values; reduce decay for drums
 *
 * Source URL: https://github.com/el-visio/dattorro-verb (MIT)
 * License: MIT
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// ============================================================================
// Allpass delay line
// Implements the Schroeder allpass section used throughout Dattorro's network.
// ============================================================================

class DattorroAllpass
{
public:
    void prepare(int delayLengthSamples)
    {
        size = delayLengthSamples;
        buffer.assign(size, 0.0f);
        writeIdx = 0;
    }

    // Schroeder allpass: y = -g*x + state + g*state  (all-pass transfer function)
    // Output has flat frequency magnitude but dispersed phase — creates density.
    float process(float x, float g)
    {
        const float delayed = buffer[writeIdx];
        const float v       = x - g * delayed;
        buffer[writeIdx]    = v;
        writeIdx            = (writeIdx + 1) % size;
        return g * v + delayed;
    }

    // Read at a tap offset from the write head (for output taps in the tank).
    // tapSamples: how many samples behind the write head to read from.
    float readTap(int tapSamples) const
    {
        tapSamples = juce::jlimit(1, size, tapSamples);
        const int readIdx = (writeIdx - tapSamples + size) % size;
        return buffer[readIdx];
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

private:
    std::vector<float> buffer;
    int size     = 1;
    int writeIdx = 0;
};

// ============================================================================
// Simple delay line (no allpass coefficient)
// ============================================================================

class DattorroDelay
{
public:
    void prepare(int maxDelaySamples)
    {
        size = maxDelaySamples;
        buffer.assign(size, 0.0f);
        writeIdx = 0;
    }

    void write(float x)
    {
        buffer[writeIdx] = x;
        writeIdx = (writeIdx + 1) % size;
    }

    // Read at tapSamples behind the current write head.
    float readTap(int tapSamples) const
    {
        tapSamples = juce::jlimit(1, size, tapSamples);
        const int readIdx = (writeIdx - tapSamples + size) % size;
        return buffer[readIdx];
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

private:
    std::vector<float> buffer;
    int size     = 1;
    int writeIdx = 0;
};

// ============================================================================
// Dattorro Plate Reverb
// ============================================================================

class DattorroPlateReverb
{
public:
    //==========================================================================
    // Parameters
    struct Params
    {
        float decay      = 0.5f;   // 0.0–1.0   tail length (0=short, 1=infinite)
        float damping    = 0.7f;   // 0.0–1.0   high-frequency absorption in tank
        float predelayMs = 20.0f;  // 0–100 ms  pre-reverb delay (adds sense of distance)
        float mix        = 0.4f;   // 0.0–1.0   dry/wet blend
    };

    //==========================================================================
    // Call once during plugin initialisation (prepareToPlay)
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);

        // Dattorro's original delay constants were specified at 29761 Hz.
        // Scale to actual sample rate to preserve reverb character.
        const float k = sr / 29761.0f;

        // ---- Input diffuser (4 allpass stages) ----
        inputAP[0].prepare(juce::roundToInt(142 * k));
        inputAP[1].prepare(juce::roundToInt(107 * k));
        inputAP[2].prepare(juce::roundToInt(379 * k));
        inputAP[3].prepare(juce::roundToInt(277 * k));

        // ---- Tank allpasses ----
        tankAP[0].prepare(juce::roundToInt(672  * k));
        tankAP[1].prepare(juce::roundToInt(908  * k));

        // ---- Tank delays (D1 and D2 in Figure 15) ----
        tankD[0].prepare(juce::roundToInt(4453 * k));
        tankD[1].prepare(juce::roundToInt(4217 * k));

        // Store scaled tap offsets for output reading (Section IV of paper)
        tap[0]  = juce::roundToInt(266  * k);
        tap[1]  = juce::roundToInt(2974 * k);
        tap[2]  = juce::roundToInt(1913 * k);
        tap[3]  = juce::roundToInt(1996 * k);
        tap[4]  = juce::roundToInt(355  * k);
        tap[5]  = juce::roundToInt(3124 * k);
        tap[6]  = juce::roundToInt(2111 * k);
        tap[7]  = juce::roundToInt(335  * k);

        // ---- Pre-delay line (max 100ms) ----
        const int predelayMax = juce::roundToInt(sr * 0.101f);
        predelay.prepare(predelayMax);

        // ---- LP filter states reset ----
        lpState[0] = lpState[1] = 0.0f;

        // ---- Tank feedback state reset ----
        tankFeedback[0] = tankFeedback[1] = 0.0f;
    }

    //==========================================================================
    // Process a block of audio in place.
    // Call with the main plugin AudioBuffer — modifies in place.
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Clamp parameters to valid ranges
        const float decay      = juce::jlimit(0.0f, 0.9999f, params.decay);
        const float damping    = juce::jlimit(0.0f, 0.9999f, params.damping);
        const float mix        = juce::jlimit(0.0f, 1.0f,    params.mix);
        const int   predelayN  = juce::roundToInt(
            juce::jlimit(0.0f, 100.0f, params.predelayMs) / 1000.0f * sr);

        // Equal-power dry/wet gains
        const float angle    = mix * juce::MathConstants<float>::halfPi;
        const float dryGain  = std::cos(angle);
        const float wetGain  = std::sin(angle);

        // Fixed diffusion coefficients from Dattorro 1997
        constexpr float kInputDiff1 = 0.75f;
        constexpr float kInputDiff2 = 0.625f;
        constexpr float kDecayDiff1 = 0.70f;
        constexpr float kDecayDiff2 = 0.50f;

        // LP damping: coefficient for one-pole lowpass in the tank
        // damping=0 → lpCoeff=1 (no filtering), damping=1 → lpCoeff≈0 (silence)
        const float lpCoeff = 1.0f - damping;

        for (int s = 0; s < numSamples; ++s)
        {
            // Mix input channels to mono for the reverb input path
            float inputMono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputMono += buffer.getSample(ch, s);
            if (numChannels > 1)
                inputMono /= static_cast<float>(numChannels);

            // --- Pre-delay ---
            predelay.write(inputMono);
            float x = predelayN > 0 ? predelay.readTap(predelayN) : inputMono;

            // --- Input diffusion (4 allpass stages) ---
            x = inputAP[0].process(x, kInputDiff1);
            x = inputAP[1].process(x, kInputDiff1);
            x = inputAP[2].process(x, kInputDiff2);
            x = inputAP[3].process(x, kInputDiff2);

            // --- Tank feedback path A (left side) ---
            // Input to path A = diffused signal + feedback from path B
            float pathA = x + decay * tankFeedback[1];
            pathA       = tankAP[0].process(pathA, kDecayDiff1);
            // One-pole LP filter absorbs highs on each pass through the tank
            lpState[0]  = damping * lpState[0] + lpCoeff * pathA;
            pathA       = lpState[0] * decay;
            tankD[0].write(pathA);

            // --- Tank feedback path B (right side) ---
            float pathB = x + decay * tankFeedback[0];
            pathB       = tankAP[1].process(pathB, kDecayDiff2);
            lpState[1]  = damping * lpState[1] + lpCoeff * pathB;
            pathB       = lpState[1] * decay;
            tankD[1].write(pathB);

            // Update feedback registers for next sample
            tankFeedback[0] = tankD[0].readTap(juce::roundToInt(4453 * sr / 29761.0f));
            tankFeedback[1] = tankD[1].readTap(juce::roundToInt(4217 * sr / 29761.0f));

            // --- Output taps (stereo decorrelation, Section IV of paper) ---
            // Left output: taps from D1, AP6, and D2
            const float outL =  0.6f * tankD[0].readTap(tap[0])
                              + 0.6f * tankD[0].readTap(tap[1])
                              - 0.6f * tankAP[1].readTap(tap[2])
                              + 0.6f * tankD[1].readTap(tap[3]);

            // Right output: taps from D2, AP5, and D1 (different positions → decorrelated)
            const float outR =  0.6f * tankD[1].readTap(tap[4])
                              + 0.6f * tankD[1].readTap(tap[5])
                              - 0.6f * tankAP[0].readTap(tap[7])
                              + 0.6f * tankD[0].readTap(tap[6]);

            // --- Mix dry + wet using equal-power crossfade ---
            if (numChannels >= 2)
            {
                const float dryL = buffer.getSample(0, s);
                const float dryR = buffer.getSample(1, s);
                buffer.setSample(0, s, dryGain * dryL + wetGain * outL);
                buffer.setSample(1, s, dryGain * dryR + wetGain * outR);
            }
            else
            {
                const float dry = buffer.getSample(0, s);
                buffer.setSample(0, s, dryGain * dry + wetGain * 0.5f * (outL + outR));
            }
        }
    }

    //==========================================================================
    // Reset all internal state (call when plugin is reset or bypass engaged)
    void reset()
    {
        for (auto& ap : inputAP) ap.reset();
        for (auto& ap : tankAP)  ap.reset();
        for (auto& d  : tankD)   d.reset();
        predelay.reset();
        lpState[0]      = lpState[1]      = 0.0f;
        tankFeedback[0] = tankFeedback[1] = 0.0f;
    }

private:
    float sr = 44100.0f;

    // Input diffuser (4 allpass stages per Dattorro 1997 Figure 15)
    std::array<DattorroAllpass, 4> inputAP;

    // Tank allpasses (one per feedback path)
    std::array<DattorroAllpass, 2> tankAP;

    // Tank delay lines (D1 and D2 in the paper)
    std::array<DattorroDelay, 2> tankD;

    // Pre-delay line
    DattorroDelay predelay;

    // One-pole LP filter states for in-tank damping
    float lpState[2] = { 0.0f, 0.0f };

    // Cross-coupled feedback register (updated each sample)
    float tankFeedback[2] = { 0.0f, 0.0f };

    // Scaled output tap offsets (computed once in prepare())
    int tap[8] = {};
};
