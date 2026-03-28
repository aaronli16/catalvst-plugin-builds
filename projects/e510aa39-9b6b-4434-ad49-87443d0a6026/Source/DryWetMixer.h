/**
 * Dry/Wet Mixer — Constant-Power Crossfade
 *
 * Mixes a dry (unprocessed) and wet (processed) signal using equal-power
 * crossfading based on the cosine law. This avoids the volume dip that
 * occurs at 50% mix with a simple linear blend.
 *
 * Math:
 *   dry_gain = cos(mix * pi/2)
 *   wet_gain = sin(mix * pi/2)
 * At mix=0.0: dry_gain=1, wet_gain=0 (fully dry)
 * At mix=0.5: dry_gain=wet_gain=0.707 (-3dB each, total power preserved)
 * At mix=1.0: dry_gain=0, wet_gain=1 (fully wet)
 *
 * Source: Clean-room implementation of the equal-power crossfade concept.
 * The math is standard textbook DSP (no novel IP). DaisySP Crossfade and
 * JUCE SmoothedValue use the same principle.
 * Source URL: https://github.com/user/catalvst (clean-room implementation)
 * License: original / MIT
 *
 * Usage:
 *   DryWetMixer mixer;
 *   mixer.prepare(sampleRate);
 *   // In processBlock, before applying your effect:
 *   mixer.setDryBuffer(inputBuffer);
 *   // Apply your effect to inputBuffer...
 *   mixer.mix(inputBuffer, mix);  // inputBuffer is the wet signal
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class DryWetMixer
{
public:
    //=========================================================================
    // Prepare must be called before processing.
    // maxSamplesPerBlock: JUCE calls this when plugin is initialised.
    void prepare(double newSampleRate, int maxSamplesPerBlock)
    {
        sampleRate = newSampleRate;
        // Size the dry buffer to hold one block for any channel count
        dryBuffer.setSize(2, maxSamplesPerBlock, false, true, true);
    }

    //=========================================================================
    // Call this BEFORE applying your effect to capture the dry signal.
    // Copies incoming buffer into internal dry storage.
    void setDryBuffer(const juce::AudioBuffer<float>& inputBuffer)
    {
        const int numChannels = juce::jmin(inputBuffer.getNumChannels(), dryBuffer.getNumChannels());
        const int numSamples  = juce::jmin(inputBuffer.getNumSamples(), dryBuffer.getNumSamples());

        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, inputBuffer, ch, 0, numSamples);
    }

    //=========================================================================
    // Mix the wet signal (wetBuffer) with the stored dry signal.
    // mix: 0.0 = fully dry, 1.0 = fully wet
    //
    // Equal-power formula ensures constant perceived loudness across the mix range.
    // Result is written back into wetBuffer.
    void mix(juce::AudioBuffer<float>& wetBuffer, float mix)
    {
        // Clamp mix to valid range
        mix = juce::jlimit(0.0f, 1.0f, mix);

        // Compute equal-power gains using the cosine law
        const float angle   = mix * juce::MathConstants<float>::halfPi;
        const float dryGain = std::cos(angle);  // 1.0 at mix=0, 0.0 at mix=1
        const float wetGain = std::sin(angle);  // 0.0 at mix=0, 1.0 at mix=1

        const int numChannels = juce::jmin(wetBuffer.getNumChannels(), dryBuffer.getNumChannels());
        const int numSamples  = wetBuffer.getNumSamples();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Scale wet signal in place
            wetBuffer.applyGain(ch, 0, numSamples, wetGain);

            // Add scaled dry signal
            wetBuffer.addFrom(ch, 0, dryBuffer, ch, 0, numSamples, dryGain);
        }
    }

    //=========================================================================
    // Reset internal state. Call when plugin is reset or bypassed.
    void reset()
    {
        dryBuffer.clear();
    }

private:
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> dryBuffer;
};
