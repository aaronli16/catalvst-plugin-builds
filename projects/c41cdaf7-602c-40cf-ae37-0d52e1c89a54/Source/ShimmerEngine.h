#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

// ==============================================================================
// ShimmerEngine — Octave-up pitch shifting via granular overlap-add
// ==============================================================================
// Processes audio through a simple granular pitch shifter that transposes
// the signal up by one octave. Used in the feedback path of the shimmer reverb.
// ==============================================================================

class ShimmerEngine
{
public:
    ShimmerEngine() = default;

    void prepare(double sampleRate, int maxBlockSize, int numChannels)
    {
        sr = sampleRate;

        // Grain size ~20ms for smooth octave shifting
        grainSize = static_cast<int>(sr * 0.02);
        bufferSize = grainSize * 4;

        // Pre-allocate circular buffers per channel
        circularBuffer.setSize(numChannels, bufferSize);
        circularBuffer.clear();

        writePos.resize(static_cast<size_t>(numChannels), 0);
        readPos.resize(static_cast<size_t>(numChannels), 0.0);

        numCh = numChannels;
    }

    void reset()
    {
        circularBuffer.clear();
        for (auto& w : writePos) w = 0;
        for (auto& r : readPos) r = 0.0;
    }

    // Process a single sample for one channel — returns octave-up shifted sample
    float processSample(int channel, float input)
    {
        if (channel >= numCh) return input;

        auto* buf = circularBuffer.getWritePointer(channel);
        int wp = writePos[static_cast<size_t>(channel)];

        // Write input into circular buffer
        buf[wp] = input;

        // Read position advances at 2x speed for octave-up
        double& rp = readPos[static_cast<size_t>(channel)];
        rp += 2.0;
        if (rp >= static_cast<double>(bufferSize))
            rp -= static_cast<double>(bufferSize);

        // Two overlapping grains for smooth crossfade
        double rp2 = rp + static_cast<double>(grainSize);
        if (rp2 >= static_cast<double>(bufferSize))
            rp2 -= static_cast<double>(bufferSize);

        // Compute position within grain for crossfade envelope
        double distFromWrite1 = static_cast<double>(wp) - rp;
        if (distFromWrite1 < 0.0) distFromWrite1 += static_cast<double>(bufferSize);
        double grainPhase1 = distFromWrite1 / static_cast<double>(grainSize);
        grainPhase1 = std::fmod(grainPhase1, 1.0);

        double distFromWrite2 = static_cast<double>(wp) - rp2;
        if (distFromWrite2 < 0.0) distFromWrite2 += static_cast<double>(bufferSize);
        double grainPhase2 = distFromWrite2 / static_cast<double>(grainSize);
        grainPhase2 = std::fmod(grainPhase2, 1.0);

        // Hann window envelope for each grain
        float env1 = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * grainPhase1)));
        float env2 = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * grainPhase2)));

        // Linear interpolation read from circular buffer
        auto interpRead = [&](double pos) -> float {
            int idx0 = static_cast<int>(pos);
            int idx1 = idx0 + 1;
            if (idx1 >= bufferSize) idx1 -= bufferSize;
            float frac = static_cast<float>(pos - static_cast<double>(idx0));
            return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
        };

        float grain1 = interpRead(rp) * env1;
        float grain2 = interpRead(rp2) * env2;

        float output = grain1 + grain2;

        // Advance write position
        wp++;
        if (wp >= bufferSize) wp = 0;
        writePos[static_cast<size_t>(channel)] = wp;

        // NaN/Inf guard
        if (std::isnan(output) || std::isinf(output))
            output = 0.0f;

        return output;
    }

private:
    double sr = 44100.0;
    int grainSize = 882;
    int bufferSize = 3528;
    int numCh = 2;

    juce::AudioBuffer<float> circularBuffer;
    std::vector<int> writePos;
    std::vector<double> readPos;
};
