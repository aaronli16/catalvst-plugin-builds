/**
 * Tape Delay
 *
 * Simulates the character of analog tape echo machines: warm, colored repeats
 * that get progressively darker and more diffuse on each pass. Three elements
 * distinguish tape echo from a plain digital delay:
 *   1. Flutter — small LFO-modulated variation in delay time (~0.5ms @ ~3Hz)
 *      simulating the mechanical speed wobble of a tape transport
 *   2. Tone — one-pole lowpass filter in the feedback path; each repeat loses
 *      high-frequency content, the way tape absorption darkens real echoes
 *   3. Saturation — soft tanh clipper in the feedback path; gives the delay
 *      signal a warm, slightly compressed character
 *
 * Algorithm topology:
 *   Input
 *     → Dry/wet capture
 *     → DelayLine (time modulated by flutter LFO)
 *     → LP filter (feedback path — tone control)
 *     → Soft saturation (tanh)
 *     → × feedback
 *     → back into delay line input
 *     → output
 *
 * Character:
 *   Warm, lo-fi, analog. Each repeat is darker and more saturated than the last.
 *   Extreme flutter settings produce a vintage wobble reminiscent of worn-out
 *   tape machines. Low flutter produces a gentle, natural-feeling pitch movement.
 *
 * Sonic tags: warm, analog, lo-fi, dark, vintage, saturated
 * Use cases: vocals, guitar, dub music, vintage-style echoes, anything that
 *            benefits from natural degradation in the repeat trail
 * Limitations: Heavy flutter can cause pitch instability on sustained tonal content.
 *              Not ideal for pristine, transparent delay — use tempo-sync delay instead.
 *
 * Source URL: https://github.com/user/catalvst (clean-room implementation)
 * License: original / MIT
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

class TapeDelay
{
public:
    //==========================================================================
    // Parameters
    struct Params
    {
        float delayMs  = 375.0f;  // 10–2000 ms   base delay time
        float feedback = 0.4f;    // 0.0–0.95     feedback amount (how many repeats)
        float tone     = 0.6f;    // 0.0–1.0      0=dark (heavy LP), 1=bright (light LP)
        float flutter  = 0.2f;    // 0.0–1.0      tape speed wobble depth
        float mix      = 0.35f;   // 0.0–1.0      dry/wet blend
    };

    //==========================================================================
    // Call once during plugin initialisation (prepareToPlay)
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);

        // Maximum delay: 2000ms + 0.5ms flutter headroom
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

        // Flutter LFO: ~3Hz, very low depth (modulates delay time by up to ±0.5ms)
        flutterPhase     = 0.0f;
        flutterRate      = 3.0f;

        // Feedback LP filter state (one-pole IIR)
        lpState = 0.0f;

        // Feedback sample register
        feedbackSample = 0.0f;
    }

    //==========================================================================
    // Process a block of audio in place (mono or stereo input → stereo output).
    // The delay is mono internally; the wet signal is copied to both channels.
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Clamp parameters
        const float delayMs  = juce::jlimit(10.0f, 2000.0f, params.delayMs);
        const float feedback = juce::jlimit(0.0f,  0.95f,   params.feedback);
        const float tone     = juce::jlimit(0.0f,  1.0f,    params.tone);
        const float flutter  = juce::jlimit(0.0f,  1.0f,    params.flutter);
        const float mix      = juce::jlimit(0.0f,  1.0f,    params.mix);

        // Flutter: maximum wobble = ±0.5ms in samples
        const float flutterDepthSamples = flutter * (sr * 0.0005f);

        // LP filter coefficient: tone=0 → cutoff ~800Hz (dark), tone=1 → cutoff ~8kHz (bright)
        // Map [0,1] → cutoff [800, 8000] Hz using exponential scale
        const float cutoffHz = 800.0f * std::pow(10.0f, tone * 1.0f); // 800–8000 Hz
        const float lpCoeff  = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / sr);

        // Flutter LFO phase increment
        const float flutterPhaseInc = flutterRate / sr;

        // Equal-power dry/wet gains
        const float angle   = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos(angle);
        const float wetGain = std::sin(angle);

        for (int s = 0; s < numSamples; ++s)
        {
            // Mix input channels to mono
            float inputMono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputMono += buffer.getSample(ch, s);
            if (numChannels > 1)
                inputMono /= static_cast<float>(numChannels);

            // --- Flutter LFO: modulates delay time with a sine wobble ---
            const float flutterLFO = std::sin(flutterPhase * juce::MathConstants<float>::twoPi);
            flutterPhase += flutterPhaseInc;
            if (flutterPhase >= 1.0f)
                flutterPhase -= 1.0f;

            // Compute modulated delay in samples (base + LFO wobble)
            const float baseDelaySamples    = delayMs / 1000.0f * sr;
            const float modulatedDelaySamples = juce::jlimit(
                1.0f,
                static_cast<float>(delayLine.getMaximumDelayInSamples() - 1),
                baseDelaySamples + flutterLFO * flutterDepthSamples
            );

            // --- Write input + feedback into delay line ---
            delayLine.pushSample(0, inputMono + feedbackSample);

            // --- Read delayed sample ---
            const float delayed = delayLine.popSample(0, modulatedDelaySamples, true);

            // --- LP filter in feedback path (simulates tape absorption) ---
            // One-pole IIR: lpState += lpCoeff * (delayed - lpState)
            lpState += lpCoeff * (delayed - lpState);
            const float filtered = lpState;

            // --- Soft saturation: tanh clipper gives warm, gentle compression ---
            // Gain of 1.5 before tanh drives the signal into subtle saturation
            const float saturated = std::tanh(filtered * 1.5f) / 1.5f;

            // --- Update feedback register for next sample ---
            feedbackSample = saturated * feedback;

            // --- Mix dry + wet (equal-power) ---
            const float wet = delayed; // Use un-saturated read for output (saturation is internal)

            if (numChannels >= 2)
            {
                // Stereo: preserve dry channel separation, wet is mono-to-stereo
                buffer.setSample(0, s, dryGain * buffer.getSample(0, s) + wetGain * wet);
                buffer.setSample(1, s, dryGain * buffer.getSample(1, s) + wetGain * wet);
            }
            else
            {
                buffer.setSample(0, s, dryGain * inputMono + wetGain * wet);
            }
        }
    }

    //==========================================================================
    // Reset all internal state
    void reset()
    {
        delayLine.reset();
        flutterPhase   = 0.0f;
        lpState        = 0.0f;
        feedbackSample = 0.0f;
    }

private:
    float sr = 44100.0f;

    // JUCE fractional delay line (supports interpolation for flutter modulation)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;

    // Flutter LFO state
    float flutterPhase   = 0.0f;
    float flutterRate    = 3.0f;   // Hz

    // Feedback LP filter state (one-pole IIR)
    float lpState        = 0.0f;

    // Feedback sample (written back into delay line input each sample)
    float feedbackSample = 0.0f;
};
