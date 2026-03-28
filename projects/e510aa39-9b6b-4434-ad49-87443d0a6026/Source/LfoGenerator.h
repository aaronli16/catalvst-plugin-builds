/**
 * LFO Generator — Low-Frequency Oscillator
 *
 * Generates a low-frequency modulation signal with sine, triangle, and square
 * waveforms. Used by chorus, flanger, tremolo, auto-pan, and vibrato effects
 * to modulate delay times, gain, pitch, or filter cutoff.
 *
 * Parameters:
 *   rate  (Hz)  — oscillation frequency; typical range 0.01 to 20 Hz
 *   depth (0-1) — modulation depth; scales the output from -depth to +depth
 *   shape       — waveform: Sine (smooth), Triangle (linear), Square (hard)
 *
 * Output: getNextSample() returns a value in the range [-depth, +depth]
 *
 * Source: Clean-room implementation from standard DSP textbook LFO algorithms.
 * DaisySP Oscillator (MIT) and Signalsmith LFO use the same underlying math.
 * License: original / MIT
 * Source URL: https://github.com/user/catalvst (clean-room implementation)
 *
 * Usage (e.g., tremolo):
 *   LfoGenerator lfo;
 *   lfo.prepare(sampleRate);
 *   lfo.setRate(4.0f);    // 4 Hz tremolo
 *   lfo.setDepth(0.5f);   // 50% depth
 *   lfo.setShape(LfoGenerator::Shape::Sine);
 *   // In processBlock per-sample loop:
 *   float gain = 1.0f + lfo.getNextSample();  // 0.5 to 1.5 gain range
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class LfoGenerator
{
public:
    //=========================================================================
    enum class Shape
    {
        Sine,      // Smooth sinusoidal; best for subtle modulation (chorus, vibrato)
        Triangle,  // Linear ramp up/down; slightly harder edge than sine
        Square     // Hard on/off; good for gating, tremolo with rhythmic feel
    };

    //=========================================================================
    // Prepare must be called with the host sample rate before processing.
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        phase = 0.0f;
        updatePhaseIncrement();
    }

    //=========================================================================
    // Set oscillation rate in Hz. Clamp to [0.01, 20] for LFO range.
    void setRate(float rateHz)
    {
        rate = juce::jlimit(0.01f, 20.0f, rateHz);
        updatePhaseIncrement();
    }

    // Set modulation depth. 0.0 = no modulation, 1.0 = full range.
    void setDepth(float newDepth)
    {
        depth = juce::jlimit(0.0f, 1.0f, newDepth);
    }

    // Select waveform shape.
    void setShape(Shape newShape)
    {
        shape = newShape;
    }

    //=========================================================================
    // Returns next LFO sample in the range [-depth, +depth].
    // Advances internal phase by one sample.
    float getNextSample()
    {
        float output = 0.0f;

        switch (shape)
        {
            case Shape::Sine:
                // Standard sine oscillator. Phase in [0, 1], map to [0, 2pi].
                output = std::sin(phase * juce::MathConstants<float>::twoPi);
                break;

            case Shape::Triangle:
                // Linear ramp: rises from -1 to +1 over first half, falls back
                // over second half. Produces a pointed waveform without DC offset.
                if (phase < 0.5f)
                    output = 4.0f * phase - 1.0f;   // -1 at 0, +1 at 0.5
                else
                    output = 3.0f - 4.0f * phase;   // +1 at 0.5, -1 at 1.0
                break;

            case Shape::Square:
                // Hard-switching square wave. 50% duty cycle.
                output = (phase < 0.5f) ? 1.0f : -1.0f;
                break;
        }

        // Advance phase and wrap to [0, 1)
        phase += phaseIncrement;
        if (phase >= 1.0f)
            phase -= 1.0f;

        return output * depth;
    }

    //=========================================================================
    // Reset phase to zero. Call when plugin is reset or reset is requested.
    void reset()
    {
        phase = 0.0f;
    }

private:
    //=========================================================================
    void updatePhaseIncrement()
    {
        if (sampleRate > 0.0)
            phaseIncrement = static_cast<float>(rate / sampleRate);
    }

    double sampleRate    = 44100.0;
    float  rate          = 1.0f;    // Hz
    float  depth         = 1.0f;    // 0-1
    float  phase         = 0.0f;    // Current phase in [0, 1)
    float  phaseIncrement = 0.0f;   // How much phase advances per sample
    Shape  shape         = Shape::Sine;
};
