/**
 * Tempo-Synced Delay
 *
 * A BPM-aware delay that automatically matches its delay time to the host DAW
 * tempo. The producer chooses a rhythmic subdivision (quarter note, eighth,
 * dotted eighth, etc.) and the delay time is computed from the live BPM.
 * The result always sits perfectly in the groove — no manual time adjustment
 * needed as the track tempo changes.
 *
 * JUCE 8 AudioPlayHead API (NOT deprecated getCurrentPosition):
 *   The JUCE 8 pattern for reading BPM uses the Optional-based position API:
 *
 *     if (auto* ph = getPlayHead()) {
 *         if (auto pos = ph->getPosition()) {
 *             if (auto bpm = pos->getBpm()) {
 *                 currentBPM = static_cast<float>(*bpm);
 *             }
 *         }
 *     }
 *
 *   This class stores currentBPM internally and updates it when
 *   updateBPMFromPlayHead() is called (typically at the top of processBlock).
 *   Falls back to 120 BPM when no host tempo is available.
 *
 * Note division table (beats per note, relative to a quarter note = 1 beat):
 *   0 = quarter    → 1 beat  (e.g. 500ms at 120 BPM)
 *   1 = eighth     → 0.5 beats
 *   2 = dotted 8th → 0.75 beats (the classic "U2" delay sound)
 *   3 = triplet    → 0.333 beats (1/3 of a beat)
 *   4 = half       → 2 beats
 *   5 = whole      → 4 beats
 *
 * A simple one-pole LP filter in the feedback path keeps repeats slightly
 * warmer than the original signal — the "tone" parameter controls the amount.
 *
 * Character:
 *   Precise, musical, and rhythmically transparent. Tempo-synced delay feels
 *   like it was mixed into the track deliberately rather than added afterwards.
 *   Clean and transparent — ideal when you want delay without character.
 *
 * Sonic tags: precise, musical, rhythmic, clean, tight, groovy
 * Use cases: any tempo-sensitive material, EDM, pop, rhythmic elements,
 *            leads, vocal chops, sidechain-adjacent rhythmic effects
 * Limitations: Requires a DAW with a tempo track; falls back to 120 BPM in
 *              standalone mode. Not suitable when you want the delay to drift
 *              or have analog character — use tape delay instead.
 *
 * Source URL: https://github.com/user/catalvst (clean-room implementation)
 * License: original / MIT
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class TempoSyncDelay
{
public:
    //==========================================================================
    // Note division enum
    // Stored as int parameter (0–5) for easy use as a parameter choice.
    enum class NoteDivision
    {
        Quarter    = 0,  // 1 beat
        Eighth     = 1,  // 0.5 beats
        DottedEighth = 2, // 0.75 beats — the classic slap-back interval
        Triplet    = 3,  // 0.333 beats (quarter triplet)
        Half       = 4,  // 2 beats
        Whole      = 5   // 4 beats
    };

    //==========================================================================
    // Parameters
    struct Params
    {
        int   noteDivision = 0;     // 0=quarter, 1=eighth, 2=dotted8th, 3=triplet, 4=half, 5=whole
        float feedback     = 0.4f;  // 0.0–0.9   feedback amount
        float tone         = 0.8f;  // 0.0–1.0   feedback LP filter (0=dark, 1=bright)
        float mix          = 0.35f; // 0.0–1.0   dry/wet blend
    };

    //==========================================================================
    // Call once during plugin initialisation (prepareToPlay)
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);

        // Maximum delay: 4 beats at 20 BPM = 12000ms. Give some headroom.
        const int maxDelaySamples = juce::roundToInt(sr * 13.0f);

        // IMPORTANT: call setMaximumDelayInSamples BEFORE prepare(spec).
        // This is the JUCE DelayLine requirement — the buffer must be sized
        // before the processor spec is applied.
        delayLine.setMaximumDelayInSamples(maxDelaySamples);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = newSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(512);
        spec.numChannels      = 1; // mono delay with stereo output
        delayLine.prepare(spec);

        currentBPM     = 120.0f;
        lpState        = 0.0f;
        feedbackSample = 0.0f;
    }

    //==========================================================================
    // Update internal BPM from the host's AudioPlayHead.
    // Call this at the top of processBlock, passing getPlayHead() as argument.
    //
    // JUCE 8 API: uses Optional-based position API introduced in JUCE 7+.
    // The old getCurrentPosition(info) approach is deprecated in JUCE 8.
    void updateBPMFromPlayHead(juce::AudioPlayHead* playHead)
    {
        if (playHead == nullptr)
            return;

        // JUCE 8 pattern: getPosition() returns Optional<PositionInfo>
        if (auto pos = playHead->getPosition())
        {
            // getBpm() returns Optional<double>
            if (auto bpm = pos->getBpm())
            {
                // Clamp to sane range: 20–300 BPM
                currentBPM = juce::jlimit(20.0f, 300.0f, static_cast<float>(*bpm));
            }
        }
        // If either Optional is empty (standalone mode, no tempo track),
        // currentBPM retains its last valid value (or fallback 120 BPM from prepare()).
    }

    //==========================================================================
    // Process a block of audio in place.
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Clamp parameters
        const int   noteDivision = juce::jlimit(0, 5, params.noteDivision);
        const float feedback     = juce::jlimit(0.0f, 0.9f,  params.feedback);
        const float tone         = juce::jlimit(0.0f, 1.0f,  params.tone);
        const float mix          = juce::jlimit(0.0f, 1.0f,  params.mix);

        // Compute delay time from BPM and note division
        const float delaySamples = computeDelaySamples(currentBPM, noteDivision);

        // LP filter coefficient for feedback tone shaping
        // tone=0 → cutoff ~600Hz (dark), tone=1 → ~8000Hz (bright)
        const float cutoffHz = 600.0f * std::pow(13.3f, tone); // 600–8000 Hz
        const float lpCoeff  = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / sr);

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

            // --- Write input + feedback into delay line ---
            delayLine.pushSample(0, inputMono + feedbackSample);

            // --- Read at tempo-synced delay time ---
            const float delayed = delayLine.popSample(0, delaySamples, true);

            // --- LP filter in feedback path (controls brightness of repeats) ---
            lpState += lpCoeff * (delayed - lpState);
            feedbackSample = lpState * feedback;

            // --- Mix dry + wet (equal-power) ---
            if (numChannels >= 2)
            {
                const float dryL = buffer.getSample(0, s);
                const float dryR = buffer.getSample(1, s);
                buffer.setSample(0, s, dryGain * dryL + wetGain * delayed);
                buffer.setSample(1, s, dryGain * dryR + wetGain * delayed);
            }
            else
            {
                buffer.setSample(0, s, dryGain * inputMono + wetGain * delayed);
            }
        }
    }

    //==========================================================================
    // Reset all internal state
    void reset()
    {
        delayLine.reset();
        lpState        = 0.0f;
        feedbackSample = 0.0f;
        // Note: do NOT reset currentBPM — it should persist across transport stops
    }

    //==========================================================================
    // Get current BPM (useful for UI display)
    float getCurrentBPM() const { return currentBPM; }

private:
    //==========================================================================
    // Convert BPM + note division enum to delay time in samples.
    // Quarter note = 60/BPM seconds = one beat.
    float computeDelaySamples(float bpm, int divisionIndex) const
    {
        // Beats per note for each division index
        // (relative to a quarter note = 1 beat)
        constexpr float beatsPerNote[] = {
            1.0f,        // 0: quarter
            0.5f,        // 1: eighth
            0.75f,       // 2: dotted eighth (3/4 of a beat)
            1.0f / 3.0f, // 3: triplet (quarter triplet = 1/3 beat)
            2.0f,        // 4: half
            4.0f         // 5: whole
        };

        const float beats        = beatsPerNote[juce::jlimit(0, 5, divisionIndex)];
        const float beatSeconds  = 60.0f / bpm;
        const float delaySec     = beats * beatSeconds;
        const float delaySamples = delaySec * sr;

        // Clamp to valid range for the delay line
        return juce::jlimit(
            1.0f,
            static_cast<float>(delayLine.getMaximumDelayInSamples() - 1),
            delaySamples
        );
    }

    float sr = 44100.0f;

    // Linear interpolation (static delay time, no modulation needed)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;

    // Current BPM from host (updated via updateBPMFromPlayHead)
    float currentBPM = 120.0f;

    // Feedback LP filter state
    float lpState = 0.0f;

    // Feedback sample register
    float feedbackSample = 0.0f;
};
