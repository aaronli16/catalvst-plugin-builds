/**
 * Multi-Tap Delay
 *
 * Three independent delay taps with per-tap timing and level. Each tap reads
 * from the same delay buffer at a different position, then their outputs are
 * summed with a master feedback path that returns into the buffer input.
 *
 * This creates complex, polyrhythmic echo patterns: set the three tap times
 * at 1/8, 1/4, and 1/2 note ratios to get a pulsing, layered groove. Use
 * unequal tap levels (e.g. 0.8, 0.5, 0.3) for a natural decay across the
 * three taps. The master feedback sends the summed output back into the
 * buffer for additional "echo of the echo" depth.
 *
 * Algorithm topology:
 *   Input + feedback
 *     → Write head (single circular buffer — all taps share one buffer)
 *     ↓
 *   [Tap 1 read at tap1_ms] × tap1_level
 *   [Tap 2 read at tap2_ms] × tap2_level
 *   [Tap 3 read at tap3_ms] × tap3_level
 *     → Sum → output
 *     → Sum × feedback → back to write head
 *
 * Note: All three taps share one delay line. Tap 3 must be the longest tap
 * (or equal to it). The buffer is sized to accommodate the maximum tap3_ms.
 * Taps are independently settable — they don't have to be multiples of each other.
 *
 * Character:
 *   Complex, rhythmic, textured. The interplay between three independent echo
 *   positions creates patterns the producer's ear wouldn't compose consciously.
 *   Low feedback gives three clean, distinct echoes; high feedback produces an
 *   evolving rhythmic cloud.
 *
 * Sonic tags: rhythmic, complex, textured, layered, polyrhythmic, deep
 * Use cases: percussion, snare delays, synth stabs, ambient textures, sound design,
 *            adding rhythmic interest to static content
 * Limitations: Requires careful tap time choices to avoid a muddy result.
 *              Not ideal for simple single-echo use cases — use tape or tempo-sync.
 *              Feedback can accumulate quickly if all three tap levels are high.
 *
 * Source URL: https://github.com/user/catalvst (clean-room implementation)
 * License: original / MIT
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class MultiTapDelay
{
public:
    //==========================================================================
    // Parameters
    struct Params
    {
        float tap1Ms    = 125.0f;  // 10–2000 ms   first tap delay time
        float tap2Ms    = 250.0f;  // 10–2000 ms   second tap delay time
        float tap3Ms    = 500.0f;  // 10–2000 ms   third tap delay time
        float tap1Level = 0.8f;    // 0.0–1.0      first tap level
        float tap2Level = 0.6f;    // 0.0–1.0      second tap level
        float tap3Level = 0.4f;    // 0.0–1.0      third tap level
        float feedback  = 0.3f;    // 0.0–0.85     master feedback amount
        float mix       = 0.3f;    // 0.0–1.0      dry/wet blend
    };

    //==========================================================================
    // Call once during plugin initialisation (prepareToPlay)
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);

        // Maximum tap time: 2000ms (tap3_ms upper limit)
        const int maxDelaySamples = juce::roundToInt(sr * 2.001f);

        // IMPORTANT: call setMaximumDelayInSamples BEFORE prepare(spec).
        // This is the JUCE DelayLine requirement — the buffer must be sized
        // before the processor spec is applied.
        delayLine.setMaximumDelayInSamples(maxDelaySamples);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = newSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(512);
        spec.numChannels      = 1; // mono delay with stereo output
        delayLine.prepare(spec);

        feedbackSample = 0.0f;
    }

    //==========================================================================
    // Process a block of audio in place.
    // All taps are mono; the wet output is copied to both channels.
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Clamp parameters
        const float tap1Ms    = juce::jlimit(10.0f, 2000.0f, params.tap1Ms);
        const float tap2Ms    = juce::jlimit(10.0f, 2000.0f, params.tap2Ms);
        const float tap3Ms    = juce::jlimit(10.0f, 2000.0f, params.tap3Ms);
        const float tap1Level = juce::jlimit(0.0f,  1.0f,    params.tap1Level);
        const float tap2Level = juce::jlimit(0.0f,  1.0f,    params.tap2Level);
        const float tap3Level = juce::jlimit(0.0f,  1.0f,    params.tap3Level);
        const float feedback  = juce::jlimit(0.0f,  0.85f,   params.feedback);
        const float mix       = juce::jlimit(0.0f,  1.0f,    params.mix);

        const float maxTapSamples = static_cast<float>(delayLine.getMaximumDelayInSamples() - 1);

        const float tap1Samples = juce::jlimit(1.0f, maxTapSamples, tap1Ms / 1000.0f * sr);
        const float tap2Samples = juce::jlimit(1.0f, maxTapSamples, tap2Ms / 1000.0f * sr);
        const float tap3Samples = juce::jlimit(1.0f, maxTapSamples, tap3Ms / 1000.0f * sr);

        // Equal-power dry/wet gains
        const float angle   = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos(angle);
        const float wetGain = std::sin(angle);

        for (int s = 0; s < numSamples; ++s)
        {
            // Mix input to mono
            float inputMono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputMono += buffer.getSample(ch, s);
            if (numChannels > 1)
                inputMono /= static_cast<float>(numChannels);

            // --- Write input + feedback into delay buffer ---
            delayLine.pushSample(0, inputMono + feedbackSample);

            // --- Read three independent taps ---
            // Each tap reads at a different point in the delay buffer.
            // The delay line holds all tap history in one circular buffer.
            const float tap1Out = delayLine.popSample(0, tap1Samples, false);
            const float tap2Out = delayLine.popSample(0, tap2Samples, false);
            const float tap3Out = delayLine.popSample(0, tap3Samples, true); // advance read ptr on last

            // --- Sum taps with per-tap levels ---
            const float wetSum =  tap1Level * tap1Out
                                + tap2Level * tap2Out
                                + tap3Level * tap3Out;

            // --- Master feedback: summed output feeds back into write head ---
            feedbackSample = wetSum * feedback;

            // --- Mix dry + wet (equal-power) ---
            if (numChannels >= 2)
            {
                const float dryL = buffer.getSample(0, s);
                const float dryR = buffer.getSample(1, s);
                buffer.setSample(0, s, dryGain * dryL + wetGain * wetSum);
                buffer.setSample(1, s, dryGain * dryR + wetGain * wetSum);
            }
            else
            {
                buffer.setSample(0, s, dryGain * inputMono + wetGain * wetSum);
            }
        }
    }

    //==========================================================================
    // Reset all internal state
    void reset()
    {
        delayLine.reset();
        feedbackSample = 0.0f;
    }

private:
    float sr = 44100.0f;

    // Single circular buffer — all 3 taps share this one delay line.
    // Linear interpolation is sufficient for static (non-modulated) taps.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;

    // Master feedback register (summed wet signal fed back to write head)
    float feedbackSample = 0.0f;
};
