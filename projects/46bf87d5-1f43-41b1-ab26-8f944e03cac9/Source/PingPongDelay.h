/**
 * Stereo Ping-Pong Delay
 *
 * Alternates delay taps between the left and right channels, creating a
 * wide, rhythmically bouncing stereo image. The first repeat appears on
 * the opposite side to the input, then bounces back and forth, decaying
 * with each pass.
 *
 * Algorithm topology:
 *   Mono input (sum of channels)
 *     → L delay line (first bounce on left)
 *     → R delay line (second bounce on right, reads from L feedback)
 *     → L delay line (reads from R feedback)
 *     → ... (each line feeds back into the other)
 *
 * Implementation:
 *   Two mono delay lines of equal length. On each sample:
 *     - L delay line input = mono input + R feedback × spread
 *     - R delay line input = L output × feedback
 *   This creates the characteristic "ball bouncing between walls" movement.
 *
 *   The spread parameter controls the stereo width of the ping-pong:
 *   spread=0.0: both channels mix to mono (no stereo movement)
 *   spread=1.0: full ping-pong width (default, maximum spatial separation)
 *
 * Character:
 *   Spatial and rhythmic. The bouncing image adds width and movement to
 *   any mono source. Works especially well on synth leads, hi-hats, and
 *   anything where stereo interest enhances the groove without cluttering
 *   the centre of the mix.
 *
 * Sonic tags: stereo, rhythmic, spacious, wide, bouncing, lush
 * Use cases: synths, hi-hats, percussion, creating width in a mix,
 *            vocal doubles, anything that benefits from spatial movement
 * Limitations: Sounds best on mono or centre-panned sources. On a wide stereo
 *              source the ping-pong can become hard to hear clearly.
 *              At very high feedback the bouncing can become chaotic.
 *
 * Source URL: https://github.com/user/catalvst (clean-room implementation)
 * License: original / MIT
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class PingPongDelay
{
public:
    //==========================================================================
    // Parameters
    struct Params
    {
        float delayMs  = 250.0f;  // 50–2000 ms   single-tap delay time
        float feedback = 0.5f;    // 0.0–0.9      how many bounces before silence
        float spread   = 1.0f;    // 0.0–1.0      stereo width of the ping-pong
        float mix      = 0.35f;   // 0.0–1.0      dry/wet blend
    };

    //==========================================================================
    // Call once during plugin initialisation (prepareToPlay)
    void prepare(double newSampleRate, int /*samplesPerBlock*/)
    {
        sr = static_cast<float>(newSampleRate);

        // Maximum delay: 2000ms
        const int maxDelaySamples = juce::roundToInt(sr * 2.001f);

        // IMPORTANT: call setMaximumDelayInSamples BEFORE prepare(spec).
        // This is the JUCE DelayLine requirement — buffer must be sized first.
        leftDelay.setMaximumDelayInSamples(maxDelaySamples);
        rightDelay.setMaximumDelayInSamples(maxDelaySamples);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = newSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(512);
        spec.numChannels      = 1; // two separate mono delay lines

        leftDelay.prepare(spec);
        rightDelay.prepare(spec);

        // Feedback registers
        leftFeedback  = 0.0f;
        rightFeedback = 0.0f;
    }

    //==========================================================================
    // Process a block of audio in place.
    // Handles both mono→stereo and stereo→stereo buffer configurations.
    // The ping-pong always produces stereo output.
    void process(juce::AudioBuffer<float>& buffer, const Params& params)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Clamp parameters
        const float delayMs  = juce::jlimit(50.0f,  2000.0f, params.delayMs);
        const float feedback = juce::jlimit(0.0f,   0.9f,    params.feedback);
        const float spread   = juce::jlimit(0.0f,   1.0f,    params.spread);
        const float mix      = juce::jlimit(0.0f,   1.0f,    params.mix);

        const float delaySamples = juce::jlimit(
            1.0f,
            static_cast<float>(leftDelay.getMaximumDelayInSamples() - 1),
            delayMs / 1000.0f * sr
        );

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

            // --- Left delay ---
            // Input: new audio + right channel feedback (spread controls cross-feed)
            leftDelay.pushSample(0, inputMono + rightFeedback * spread * feedback);
            const float leftOut = leftDelay.popSample(0, delaySamples, true);

            // --- Right delay ---
            // Input: left output × feedback (creates the bounce)
            rightDelay.pushSample(0, leftOut * feedback);
            const float rightOut = rightDelay.popSample(0, delaySamples, true);

            // Update feedback registers for next sample
            // Left picks up from right (spreads to L), right picks up from left
            leftFeedback  = leftOut;
            rightFeedback = rightOut;

            // --- Output ---
            // Wet signal: left channel hears leftOut (first bounce), right hears rightOut
            // Dry signal: preserve original stereo content
            if (numChannels >= 2)
            {
                const float dryL = buffer.getSample(0, s);
                const float dryR = buffer.getSample(1, s);
                buffer.setSample(0, s, dryGain * dryL + wetGain * leftOut);
                buffer.setSample(1, s, dryGain * dryR + wetGain * rightOut);
            }
            else
            {
                // Mono input expanded to stereo output via ping-pong
                buffer.setSample(0, s, dryGain * inputMono + wetGain * leftOut);
                // Note: mono buffer only has ch=0; if caller wants stereo they need 2-ch buffer
            }
        }
    }

    //==========================================================================
    // Reset all internal state
    void reset()
    {
        leftDelay.reset();
        rightDelay.reset();
        leftFeedback  = 0.0f;
        rightFeedback = 0.0f;
    }

private:
    float sr = 44100.0f;

    // Two separate mono delay lines — one per stereo channel
    // Lagrange interpolation for smooth sub-sample delay times
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> leftDelay;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> rightDelay;

    // Cross-feedback registers (updated each sample, used next sample)
    float leftFeedback  = 0.0f;
    float rightFeedback = 0.0f;
};
