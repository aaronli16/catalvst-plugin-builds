/**
 * Parameter Smoother — One-Pole Exponential Smoothing
 *
 * Smooths abrupt parameter changes (knob moves, automation jumps) to prevent
 * zipper noise — the clicking/crackling artifact caused by stepping through
 * sample values without interpolation.
 *
 * Algorithm: One-pole recursive low-pass filter (IIR)
 *   smoothed = smoothed + alpha * (target - smoothed)
 * where:
 *   alpha = 1 - exp(-2 * pi * smoothingFreq / sampleRate)
 *
 * smoothingFreq is the -3dB cutoff frequency. Lower = slower/smoother.
 * Typical values:
 *   10 Hz  — very smooth (30+ ms to reach target); good for reverb decay
 *   50 Hz  — moderate smoothing (~5 ms); good for gain/mix faders
 *  100 Hz  — fast smoothing (~2 ms); good for high-rate modulation targets
 *
 * This is the same underlying math as JUCE SmoothedValue, but expressed as a
 * standalone utility class that Claude can reference without needing JUCE
 * template machinery.
 *
 * Source: Standard DSP one-pole smoothing filter.
 * License: original / MIT
 * Source URL: https://github.com/user/catalvst (clean-room implementation)
 *
 * Usage:
 *   ParameterSmoother smoother;
 *   smoother.prepare(sampleRate, 50.0f);  // 50 Hz cutoff
 *   // In processBlock per-sample loop:
 *   smoother.setTarget(paramValue);       // set from UI or automation
 *   float smoothedGain = smoother.getNextValue();
 */

#pragma once
#include <JuceHeader.h>
#include <cmath>

class ParameterSmoother
{
public:
    //=========================================================================
    // Prepare the smoother with sample rate and initial smoothing frequency.
    // smoothingFreqHz: -3dB cutoff; lower = slower response.
    // Typical: 10 Hz (slow) to 100 Hz (fast).
    void prepare(double newSampleRate, float smoothingFreqHz = 50.0f)
    {
        sampleRate = newSampleRate;
        setSmoothingFreq(smoothingFreqHz);
    }

    //=========================================================================
    // Change the smoothing speed at runtime.
    // smoothingFreqHz: cutoff in Hz; lower = slower/smoother response.
    void setSmoothingFreq(float smoothingFreqHz)
    {
        smoothingFreqHz = juce::jlimit(0.1f, 10000.0f, smoothingFreqHz);
        // One-pole coefficient: alpha = 1 - exp(-2pi * freq / sampleRate)
        alpha = 1.0f - std::exp(
            -juce::MathConstants<float>::twoPi * smoothingFreqHz
            / static_cast<float>(sampleRate)
        );
    }

    //=========================================================================
    // Set the target value. Smoothed value will approach this exponentially.
    // Call once per processBlock or per UI parameter change.
    void setTarget(float newTarget)
    {
        target = newTarget;
    }

    //=========================================================================
    // Returns the next smoothed value, one sample at a time.
    // Call this in the per-sample loop to get a smooth signal.
    //
    // One-pole IIR: smoothed += alpha * (target - smoothed)
    // Equivalent to: smoothed = alpha*target + (1-alpha)*smoothed
    float getNextValue()
    {
        smoothed += alpha * (target - smoothed);
        return smoothed;
    }

    //=========================================================================
    // Skip smoothing and jump directly to target. Use when:
    //   - Plugin first loads (avoid startup glide from 0 to initial value)
    //   - User explicitly resets the effect
    void snapToTarget()
    {
        smoothed = target;
    }

    //=========================================================================
    // Returns true if the smoother has effectively reached its target
    // (within 0.0001 of target). Useful to skip processing when settled.
    bool isSettled() const
    {
        return std::abs(smoothed - target) < 0.0001f;
    }

    //=========================================================================
    // Current smoothed value without advancing state.
    float getCurrentValue() const { return smoothed; }

    //=========================================================================
    // Reset to a specific value and set it as the new target.
    // Call when plugin is reset or when initialising from a saved preset.
    void reset(float initialValue = 0.0f)
    {
        smoothed = initialValue;
        target   = initialValue;
    }

private:
    double sampleRate = 44100.0;
    float  alpha      = 0.01f;  // Filter coefficient (set in setSmoothingFreq)
    float  target     = 0.0f;   // Where we're heading
    float  smoothed   = 0.0f;   // Current smoothed value
};
