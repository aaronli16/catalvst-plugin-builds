/**
 * Shimmer Reverb
 *
 * Algorithm: reverb diffusion network + pitched feedback loop.
 * Pitch shifter concept informed by Signalsmith Audio's stretch library (MIT):
 *   https://github.com/Signalsmith-Audio/signalsmith-stretch
 *   (We use a simplified 4-grain granular approach rather than full spectral.)
 *
 * How shimmer works:
 *   Shimmer reverb is a reverb whose feedback path includes a pitch shifter.
 *   Each time the reverb signal loops back, it is transposed by a set interval
 *   (typically an octave). The result is a "shimmering" swelling tail that rises
 *   in pitch — a signature sound in ambient and post-rock music (think Brian Eno,
 *   Sigur Ros, Strymon BigSky).
 *
 *   Topology:
 *     Input
 *       -> Input diffuser (4 allpass stages, g=0.6)
 *       -> Tank (4 delay lines in feedback loop)
 *       |
 *     Pitch Shift (4-grain Hann-windowed granular with phase continuity)
 *       |
 *     x shimmer_amount -> add back into tank input (shimmer feedback)
 *       |
 *     Output taps -> stereo output
 *
 * Pitch shift implementation (4-grain granular pitch shifter):
 *   Four overlapping read heads move through a circular buffer at a speed
 *   determined by 2^(semitones/12). Each grain is faded in/out by a Hann
 *   window of 50ms (shorter than the previous 80ms for tighter shimmer).
 *   The 4 grains are phase-offset by grainSize/4 for smooth crossfading.
 *
 *   Phase continuity: when a grain resets, its read position starts from
 *   the previous grain's current position rather than jumping to writeIdx.
 *   This eliminates the discontinuity that caused flutter in the 2-grain version.
 *
 * Character:
 *   Lush, ethereal, and cinematic — the rising shimmer tail creates an otherworldly
 *   swelling texture. At low shimmer amounts it adds subtle brightness and life to
 *   pads. At high amounts it produces dense, evolving clouds of sound.
 *
 * Sonic tags: ethereal, shimmering, atmospheric, lush, evolving, cinematic, ambient
 * Use cases: pads, guitar, strings, cinematic scoring, ambient, post-rock
 * Limitations: Not appropriate for rhythmic/percussive material. The granular
 *              pitch shift introduces some artefacts at extreme semitone values.
 *
 * Source URL: https://github.com/Signalsmith-Audio/signalsmith-stretch (MIT, concept)
 * License: MIT
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// ============================================================================
// Allpass stage (reused from standard Schroeder / Dattorro literature)
// ============================================================================

class ShimmerAllpass
{
public:
    void prepare(int delaySamples)
    {
        size = std::max(delaySamples, 1);
        buffer.assign(size, 0.0f);
        writeIdx = 0;
    }

    float process(float x, float g)
    {
        const float delayed = buffer[writeIdx];
        const float v       = x - g * delayed;
        buffer[writeIdx]    = v;
        writeIdx            = (writeIdx + 1) % size;
        return g * v + delayed;
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
// Shimmer Delay Line (used in the 4-line reverb tank)
// ============================================================================

class ShimmerDelay
{
public:
    void prepare(int delaySamples)
    {
        size = std::max(delaySamples, 1);
        buffer.assign(size, 0.0f);
        writeIdx = 0;
    }

    void write(float x)
    {
        buffer[writeIdx] = x;
        writeIdx = (writeIdx + 1) % size;
    }

    float read(int tapSamples) const
    {
        tapSamples = juce::jlimit(1, size, tapSamples);
        const int readIdx = (writeIdx - tapSamples + size) % size;
        return buffer[readIdx];
    }

    float readInterp(float tapSamples) const
    {
        tapSamples = juce::jlimit(1.0f, static_cast<float>(size - 1), tapSamples);
        const int   iSamp  = static_cast<int>(tapSamples);
        const float frac   = tapSamples - static_cast<float>(iSamp);
        const int   idx0   = (writeIdx - iSamp     + size) % size;
        const int   idx1   = (writeIdx - iSamp - 1 + size) % size;
        return buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

    int getSize() const { return size; }

private:
    std::vector<float> buffer;
    int size     = 1;
    int writeIdx = 0;
};

// ============================================================================
// 4-Grain Granular Pitch Shifter with Phase Continuity
//
// Improvements over the 2-grain version:
//   - 4 overlapping grains (smoother crossfading, less flutter)
//   - 50ms grain size (less latency, tighter shimmer)
//   - Phase continuity: each new grain starts from the previous grain's
//     read position, not from writeIdx — eliminates discontinuity jumps
// ============================================================================

class GranularPitchShifter
{
public:
    static constexpr int kNumGrains = 4;

    void prepare(double sampleRate, int grainSizeMs = 50)
    {
        sr = static_cast<float>(sampleRate);
        grainSize = juce::roundToInt(sr * grainSizeMs / 1000.0f);

        // Buffer: 4 grain sizes of capacity for read-ahead room
        const int bufSize = grainSize * 6;
        pitchBuf.assign(bufSize, 0.0f);
        bufSize_ = bufSize;
        writeIdx = 0;

        // Precompute Hann window
        hannWindow.resize(grainSize);
        for (int i = 0; i < grainSize; ++i)
        {
            const float phase = static_cast<float>(i) / static_cast<float>(grainSize - 1);
            hannWindow[i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * phase));
        }

        // Initialise 4 grains with 25% phase offsets (grainSize/4 apart)
        for (int g = 0; g < kNumGrains; ++g)
        {
            grainPhase[g] = (g * grainSize) / kNumGrains;
            grainPos[g]   = static_cast<float>(grainPhase[g]);
        }
    }

    float process(float x, float semitones)
    {
        // Write input to circular buffer
        pitchBuf[writeIdx] = x;
        writeIdx = (writeIdx + 1) % bufSize_;

        // Playback speed: > 1 = pitch up, < 1 = pitch down
        const float speed = std::pow(2.0f, semitones / 12.0f);

        float output = 0.0f;

        for (int g = 0; g < kNumGrains; ++g)
        {
            // Current window position (0 to grainSize-1)
            const int phase = grainPhase[g];
            const float win = hannWindow[phase];

            // Read from buffer at grain position (linear interpolation)
            const float tapPos = grainPos[g];
            const int   tapInt = static_cast<int>(tapPos);
            const float frac   = tapPos - static_cast<float>(tapInt);

            const int idx0 = (writeIdx - tapInt - 1 + bufSize_) % bufSize_;
            const int idx1 = (writeIdx - tapInt - 2 + bufSize_) % bufSize_;
            const float interpolated = pitchBuf[idx0] + frac * (pitchBuf[idx1] - pitchBuf[idx0]);

            output += win * interpolated;

            // Advance grain read head by speed
            grainPos[g] += speed;

            // Advance window phase
            grainPhase[g]++;

            // Reset grain when it completes one window — phase continuity:
            // new grain starts from the PREVIOUS grain's current position
            // instead of jumping to writeIdx (eliminates flutter)
            if (grainPhase[g] >= grainSize)
            {
                grainPhase[g] = 0;
                // Phase continuity: inherit position from the grain that's
                // currently at ~50% through its window (the most active grain)
                const int donorGrain = (g + kNumGrains / 2) % kNumGrains;
                grainPos[g] = grainPos[donorGrain];
            }
        }

        return output;
    }

    void reset()
    {
        std::fill(pitchBuf.begin(), pitchBuf.end(), 0.0f);
        writeIdx = 0;
        for (int g = 0; g < kNumGrains; ++g)
        {
            grainPhase[g] = (g * grainSize) / kNumGrains;
            grainPos[g]   = static_cast<float>(grainPhase[g]);
        }
    }

private:
    std::vector<float> pitchBuf;
    std::vector<float> hannWindow;
    int   bufSize_    = 0;
    int   writeIdx    = 0;
    int   grainSize   = 0;
    float grainPos[kNumGrains]  = {};
    int   grainPhase[kNumGrains] = {};
    float sr = 44100.0f;
};

// ============================================================================
// Shimmer Reverb
// ============================================================================

class ShimmerReverb
{
public:
    //==========================================================================
    // Parameters
    struct Params
    {
        float decay         = 0.6f;   // 0.0–1.0     tail length
        float shimmerAmount = 0.3f;   // 0.0–1.0     how much pitched signal feeds back
        float pitchShift    = 12.0f;  // -12 to 24 semitones  pitch shift interval
        float mix           = 0.4f;   // 0.0–1.0     dry/wet blend
    };

    //==========================================================================
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);
        const float k = sr / 44100.0f;

        // ---- Input diffuser (4 allpass stages) ----
        inputAP[0].prepare(juce::roundToInt(347 * k));
        inputAP[1].prepare(juce::roundToInt(113 * k));
        inputAP[2].prepare(juce::roundToInt(37  * k));
        inputAP[3].prepare(juce::roundToInt(59  * k));

        // ---- Reverb tank (4 delay lines, mutually inharmonic) ----
        tankDelay[0].prepare(juce::roundToInt(1031 * k));
        tankDelay[1].prepare(juce::roundToInt(1483 * k));
        tankDelay[2].prepare(juce::roundToInt(2017 * k));
        tankDelay[3].prepare(juce::roundToInt(2657 * k));

        // ---- LP damping filter states ----
        std::fill(lpState, lpState + 4, 0.0f);

        // ---- Pitch shifter (50ms grain size, 4 grains) ----
        pitchShifter.prepare(newSampleRate, 50);

        // ---- Feedback register ----
        shimmerFeedback = 0.0f;
    }

    //==========================================================================
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        const float decay         = juce::jlimit(0.0f,  0.9999f, params.decay);
        const float shimmerAmount = juce::jlimit(0.0f,  1.0f,    params.shimmerAmount);
        const float pitchShift    = juce::jlimit(-12.0f, 24.0f,  params.pitchShift);
        const float mix           = juce::jlimit(0.0f,  1.0f,    params.mix);

        // Fixed LP damping
        const float lpAlpha = 0.3f;

        // Equal-power dry/wet
        const float angle   = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos(angle);
        const float wetGain = std::sin(angle);

        // Output tap read positions
        const int tap0 = tankDelay[0].getSize() - 1;
        const int tap1 = tankDelay[1].getSize() - 1;
        const int tap2 = tankDelay[2].getSize() - 1;
        const int tap3 = tankDelay[3].getSize() - 1;

        for (int s = 0; s < numSamples; ++s)
        {
            // ---- Mono input ----
            float inputMono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputMono += buffer.getSample(ch, s);
            if (numChannels > 1)
                inputMono /= static_cast<float>(numChannels);

            // ---- Input diffusion ----
            float diff = inputMono;
            diff = inputAP[0].process(diff, 0.6f);
            diff = inputAP[1].process(diff, 0.6f);
            diff = inputAP[2].process(diff, 0.6f);
            diff = inputAP[3].process(diff, 0.6f);

            // ---- Inject diffused signal + shimmer feedback into tank ----
            const float tankInput = diff + shimmerAmount * shimmerFeedback;

            // ---- Read tank outputs ----
            const float d0 = tankDelay[0].read(tap0);
            const float d1 = tankDelay[1].read(tap1);
            const float d2 = tankDelay[2].read(tap2);
            const float d3 = tankDelay[3].read(tap3);

            // ---- LP damping + decay -> write back ----
            float reads[4] = { d0, d1, d2, d3 };
            for (int i = 0; i < 4; ++i)
            {
                lpState[i] = lpAlpha * lpState[i] + (1.0f - lpAlpha) * reads[i];
                const float fb = lpState[i] * decay + tankInput * 0.15f;
                tankDelay[i].write(fb);
            }

            // ---- Compute reverb output (stereo decorrelated) ----
            const float outL = d0 - d1 + d2 - d3;
            const float outR = d1 - d0 + d3 - d2;

            // ---- Pitch shift the reverb output for shimmer feedback ----
            const float reverbMono = 0.5f * (outL + outR);
            shimmerFeedback = pitchShifter.process(reverbMono, pitchShift);

            // ---- Dry/wet blend ----
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
        for (auto& ap : inputAP)    ap.reset();
        for (auto& d  : tankDelay)  d.reset();
        std::fill(lpState, lpState + 4, 0.0f);
        pitchShifter.reset();
        shimmerFeedback = 0.0f;
    }

private:
    float sr = 44100.0f;

    std::array<ShimmerAllpass, 4> inputAP;
    std::array<ShimmerDelay,   4> tankDelay;

    float lpState[4] = {};

    GranularPitchShifter pitchShifter;
    float shimmerFeedback = 0.0f;
};
