/**
 * Freeverb — Schroeder-Moorer Room Reverb
 *
 * Original implementation by Jezar at Dreampoint (public domain, 2000).
 * Reference: https://ccrma.stanford.edu/~jos/pasp/Freeverb.html
 *
 * Algorithm topology:
 *   Mono input
 *     → 8 parallel comb filters (feedback with damping LP filter per comb)
 *     → 4 series allpass filters (Schroeder allpass, g=0.5 fixed)
 *     → Stereo spread: L reads from tuning constants, R offset by +23 samples
 *
 * This is Jezar's original Freeverb, implemented from the classic tuning
 * constants published on the Dsp::FreeverMission page. The comb filter
 * lengths (1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617) and allpass
 * lengths (556, 441, 341, 225) are tuned at 44100 Hz and are scaled
 * proportionally at other sample rates.
 *
 * Character:
 *   Classic, recognisable room reverb with a bright, present initial reflection
 *   density. The parallel comb structure gives a coloured, room-like quality
 *   that works well for natural acoustic spaces — rooms, chambers, and short
 *   halls. Familiar and well-understood; a go-to starting point for room reverb.
 *
 * Sonic tags: classic, bright, room, natural, familiar
 * Use cases: drums, acoustic guitar, percussion, spoken word, any source needing
 *            a natural room feel without excessive density
 * Limitations: Can sound metallic or "ringy" on very long decay times with
 *              percussive or tonal sources; for long, smooth tails prefer
 *              Dattorro plate or Signalsmith hall
 *
 * Source URL: https://ccrma.stanford.edu/~jos/pasp/Freeverb.html (public domain)
 * License: public domain
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// ============================================================================
// Feedback Comb Filter (with internal damping LP filter)
// The damping LP prevents the reverb tail from staying too bright.
// ============================================================================

class FreevervCombFilter
{
public:
    void prepare(int delaySamples)
    {
        size = delaySamples;
        buffer.assign(size, 0.0f);
        writeIdx = 0;
        lpState  = 0.0f;
    }

    // feedback: how much of the comb output feeds back (room size)
    // damp:     how strongly the LP filter rolls off highs on each cycle
    float process(float x, float feedback, float damp)
    {
        const float output = buffer[writeIdx];

        // One-pole LP: y[n] = damp*y[n-1] + (1-damp)*x[n]
        // Higher damp → more rolloff → darker, warmer reverb tail
        lpState  = output * (1.0f - damp) + lpState * damp;

        buffer[writeIdx] = x + lpState * feedback;
        writeIdx = (writeIdx + 1) % size;

        return output;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
        lpState  = 0.0f;
    }

private:
    std::vector<float> buffer;
    int   size     = 1;
    int   writeIdx = 0;
    float lpState  = 0.0f;
};

// ============================================================================
// Allpass Filter (Schroeder, fixed g=0.5)
// ============================================================================

class FreevervAllpass
{
public:
    void prepare(int delaySamples)
    {
        size = delaySamples;
        buffer.assign(size, 0.0f);
        writeIdx = 0;
    }

    float process(float x)
    {
        constexpr float g   = 0.5f;
        const float delayed = buffer[writeIdx];
        buffer[writeIdx]    = x + g * delayed;
        writeIdx            = (writeIdx + 1) % size;
        return -g * x + delayed;
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
// Freeverb Room Reverb
// ============================================================================

class FreevervRoomReverb
{
public:
    //==========================================================================
    // Parameters
    struct Params
    {
        float roomSize = 0.5f;  // 0.0–1.0  comb feedback depth (room size / decay)
        float damping  = 0.5f;  // 0.0–1.0  high-frequency damping in comb filters
        float width    = 1.0f;  // 0.0–1.0  stereo spread (0=mono, 1=full stereo)
        float mix      = 0.33f; // 0.0–1.0  dry/wet blend
    };

    //==========================================================================
    // Call once during plugin initialisation
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);

        // Freeverb's original constants were tuned at 44100 Hz.
        // Scale to actual sample rate to preserve reverb character.
        const float k = sr / 44100.0f;

        // ---- Comb filter delays (original Freeverb constants) ----
        // Left channel delays
        static const int combTuningsL[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        // Right channel: offset by +23 samples for stereo decorrelation (Jezar's original)
        static const int combTuningsR[8] = { 1139, 1211, 1300, 1379, 1445, 1514, 1580, 1640 };

        for (int i = 0; i < 8; ++i)
        {
            combL[i].prepare(juce::roundToInt(combTuningsL[i] * k));
            combR[i].prepare(juce::roundToInt(combTuningsR[i] * k));
        }

        // ---- Allpass delays (original Freeverb constants) ----
        // Left channel
        static const int apTuningsL[4] = { 556, 441, 341, 225 };
        // Right channel: offset by +23
        static const int apTuningsR[4] = { 579, 464, 364, 248 };

        for (int i = 0; i < 4; ++i)
        {
            allpassL[i].prepare(juce::roundToInt(apTuningsL[i] * k));
            allpassR[i].prepare(juce::roundToInt(apTuningsR[i] * k));
        }
    }

    //==========================================================================
    // Process a block of audio in place.
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Map roomSize parameter to Freeverb's internal feedback range
        // Jezar's original: feedback = roomSize * scaleRoom + offsetRoom
        // where scaleRoom=0.28, offsetRoom=0.7 → feedback ∈ [0.7, 0.98]
        const float feedback = juce::jlimit(0.0f, 0.9999f, params.roomSize) * 0.28f + 0.7f;
        const float damp     = juce::jlimit(0.0f, 1.0f, params.damping);
        const float width    = juce::jlimit(0.0f, 1.0f, params.width);
        const float mix      = juce::jlimit(0.0f, 1.0f, params.mix);

        // Equal-power dry/wet
        const float angle   = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos(angle);
        const float wetGain = std::sin(angle);

        // Width mixing coefficients: width=1 → wet1=0.5, wet2=0.5 (full stereo)
        // width=0 → wet1=1.0, wet2=0 (mono)
        const float wet1 = wetGain * (width * 0.5f + 0.5f);
        const float wet2 = wetGain * ((1.0f - width) * 0.5f);

        for (int s = 0; s < numSamples; ++s)
        {
            // Sum input channels to mono (Freeverb processes mono → stereo)
            float inputMono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputMono += buffer.getSample(ch, s);
            if (numChannels > 1)
                inputMono /= static_cast<float>(numChannels);

            // Freeverb scales input by a fixed gain (from original source)
            const float scaledInput = inputMono * 0.015f;

            // ---- 8 parallel comb filters ----
            float outL = 0.0f, outR = 0.0f;
            for (int i = 0; i < 8; ++i)
            {
                outL += combL[i].process(scaledInput, feedback, damp);
                outR += combR[i].process(scaledInput, feedback, damp);
            }

            // ---- 4 series allpass filters ----
            for (int i = 0; i < 4; ++i)
            {
                outL = allpassL[i].process(outL);
                outR = allpassR[i].process(outR);
            }

            // ---- Stereo mix with dry/wet and width ----
            const float dryL = (numChannels >= 2) ? buffer.getSample(0, s) : inputMono;
            const float dryR = (numChannels >= 2) ? buffer.getSample(1, s) : inputMono;

            // wet1*L + wet2*R creates the stereo spread
            const float mixedL = wet1 * outL + wet2 * outR;
            const float mixedR = wet1 * outR + wet2 * outL;

            if (numChannels >= 2)
            {
                buffer.setSample(0, s, dryGain * dryL + mixedL);
                buffer.setSample(1, s, dryGain * dryR + mixedR);
            }
            else
            {
                buffer.setSample(0, s, dryGain * dryL + 0.5f * (mixedL + mixedR));
            }
        }
    }

    //==========================================================================
    // Reset all internal state
    void reset()
    {
        for (auto& c : combL)    c.reset();
        for (auto& c : combR)    c.reset();
        for (auto& a : allpassL) a.reset();
        for (auto& a : allpassR) a.reset();
    }

private:
    float sr = 44100.0f;

    // 8 parallel comb filters per channel (L and R)
    std::array<FreevervCombFilter, 8> combL;
    std::array<FreevervCombFilter, 8> combR;

    // 4 series allpass filters per channel
    std::array<FreevervAllpass, 4> allpassL;
    std::array<FreevervAllpass, 4> allpassR;
};
