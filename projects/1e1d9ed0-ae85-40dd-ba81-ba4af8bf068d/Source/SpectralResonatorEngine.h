#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>
#include <vector>

// ==============================================================================
// SpectralResonatorEngine
// ==============================================================================
// FFT-based spectral resonator. Applies resonance around a target frequency
// with configurable decay and spectral blur (diffusion).
//
// Uses 2048-point FFT with 75% overlap (hop = 512).
// Hann window for analysis/synthesis.
// Overlap-add reconstruction.
// ==============================================================================

class SpectralResonatorEngine
{
public:
    static constexpr int fftOrder = 11;            // 2^11 = 2048
    static constexpr int fftSize = 1 << fftOrder;  // 2048
    static constexpr int hopSize = fftSize / 4;    // 512 (75% overlap)

    SpectralResonatorEngine() : fft(fftOrder) {}

    void prepare(double sampleRate, int numChannels)
    {
        currentSampleRate = sampleRate;
        channels = numChannels;

        // Pre-allocate all buffers
        inputFifo.resize(static_cast<size_t>(channels));
        outputFifo.resize(static_cast<size_t>(channels));
        fifoIndex.resize(static_cast<size_t>(channels), 0);
        fftBuffer.resize(static_cast<size_t>(fftSize * 2), 0.0f);
        resonanceAccum.resize(static_cast<size_t>(channels));

        for (int ch = 0; ch < channels; ++ch)
        {
            inputFifo[static_cast<size_t>(ch)].resize(static_cast<size_t>(fftSize), 0.0f);
            outputFifo[static_cast<size_t>(ch)].resize(static_cast<size_t>(fftSize * 2), 0.0f);
            resonanceAccum[static_cast<size_t>(ch)].resize(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
        }

        // Hann window
        window.resize(static_cast<size_t>(fftSize));
        for (int i = 0; i < fftSize; ++i)
            window[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(fftSize)));

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            std::fill(inputFifo[static_cast<size_t>(ch)].begin(), inputFifo[static_cast<size_t>(ch)].end(), 0.0f);
            std::fill(outputFifo[static_cast<size_t>(ch)].begin(), outputFifo[static_cast<size_t>(ch)].end(), 0.0f);
            std::fill(resonanceAccum[static_cast<size_t>(ch)].begin(), resonanceAccum[static_cast<size_t>(ch)].end(), 0.0f);
            fifoIndex[static_cast<size_t>(ch)] = 0;
        }
    }

    // Process a buffer of audio. Output is the wet (resonated) signal only.
    // DryWetMixer in the processor handles the blend.
    void processBlock(juce::AudioBuffer<float>& buffer, float freqHz, float decayMs, float blurNorm)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min(buffer.getNumChannels(), channels);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            auto chIdx = static_cast<size_t>(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                // Push sample into input FIFO
                inputFifo[chIdx][static_cast<size_t>(fifoIndex[chIdx])] = data[i];

                // Read from output FIFO (with overlap-add result)
                data[i] = outputFifo[chIdx][static_cast<size_t>(fifoIndex[chIdx])];
                outputFifo[chIdx][static_cast<size_t>(fifoIndex[chIdx])] = 0.0f;

                fifoIndex[chIdx]++;

                if (fifoIndex[chIdx] >= hopSize)
                {
                    fifoIndex[chIdx] = 0;
                    processFFTFrame(ch, freqHz, decayMs, blurNorm);
                }
            }
        }
    }

    int getLatencyInSamples() const { return fftSize; }

private:
    void processFFTFrame(int channel, float freqHz, float decayMs, float blurNorm)
    {
        auto chIdx = static_cast<size_t>(channel);

        // Copy input FIFO into FFT buffer with zero padding
        // We need to collect the last fftSize samples. The FIFO wraps at hopSize,
        // so we shift existing data and add the new hop.
        // Actually, for overlap-add, we accumulate from the FIFO.
        // Let's use a simpler approach: maintain a full-size circular buffer.

        // Shift the analysis buffer: move last (fftSize - hopSize) samples forward,
        // then append the new hopSize samples from inputFifo.
        // We'll use fftBuffer as our working space.

        // Build the analysis frame from the last fftSize samples
        // The inputFifo stores the latest hopSize samples (indices 0..hopSize-1).
        // We need to reconstruct the full frame from the overlap history.

        // Move old data
        for (int i = 0; i < fftSize - hopSize; ++i)
            analysisFrame[static_cast<size_t>(i)] = analysisFrame[static_cast<size_t>(i + hopSize)];

        // Append new hop
        for (int i = 0; i < hopSize; ++i)
            analysisFrame[static_cast<size_t>(fftSize - hopSize + i)] = inputFifo[chIdx][static_cast<size_t>(i)];

        // Apply window and copy to FFT buffer
        for (int i = 0; i < fftSize; ++i)
            fftBuffer[static_cast<size_t>(i)] = analysisFrame[static_cast<size_t>(i)] * window[static_cast<size_t>(i)];

        // Zero the imaginary part
        for (int i = fftSize; i < fftSize * 2; ++i)
            fftBuffer[static_cast<size_t>(i)] = 0.0f;

        // Forward FFT
        fft.performRealOnlyForwardTransform(fftBuffer.data(), true);

        // ---- Spectral resonance processing ----
        const int numBins = fftSize / 2 + 1;
        const float binWidth = static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);

        // Decay factor per hop: how much the resonance accumulator retains
        // decayMs is the time for the resonance to decay by ~60dB
        // decayFactor = 10^(-3 * hopSize / (decayMs/1000 * sampleRate))
        const float decaySamples = (decayMs / 1000.0f) * static_cast<float>(currentSampleRate);
        const float decayPerHop = (decaySamples > 0.0f)
            ? std::pow(10.0f, -3.0f * static_cast<float>(hopSize) / decaySamples)
            : 0.0f;

        // Blur: controls the width of the resonance peak in bins
        // blurNorm goes from 0 (tight) to 1 (very diffuse)
        // At 0: resonance width ~2 bins. At 1: resonance width ~ numBins/4
        const float blurWidth = 2.0f + blurNorm * static_cast<float>(numBins) * 0.25f;

        // Target bin for resonant frequency
        const float targetBin = freqHz / binWidth;

        // Process each bin
        auto& accum = resonanceAccum[chIdx];

        for (int bin = 0; bin < numBins; ++bin)
        {
            const size_t realIdx = static_cast<size_t>(bin * 2);
            const size_t imagIdx = static_cast<size_t>(bin * 2 + 1);
            const size_t binIdx = static_cast<size_t>(bin);

            // Current bin magnitude
            float re = fftBuffer[realIdx];
            float im = fftBuffer[imagIdx];
            float mag = std::sqrt(re * re + im * im);

            // Resonance envelope: Gaussian centered on target frequency
            // Also include harmonics of the target frequency
            float resonanceGain = 0.0f;

            // Check fundamental and harmonics (up to Nyquist)
            for (int harmonic = 1; harmonic <= 16; ++harmonic)
            {
                float harmonicBin = targetBin * static_cast<float>(harmonic);
                if (harmonicBin >= static_cast<float>(numBins)) break;

                float dist = static_cast<float>(bin) - harmonicBin;
                float harmonicGain = std::exp(-(dist * dist) / (2.0f * blurWidth * blurWidth));

                // Higher harmonics get less resonance (natural rolloff)
                harmonicGain /= std::sqrt(static_cast<float>(harmonic));

                resonanceGain = std::max(resonanceGain, harmonicGain);
            }

            // Accumulate: decay old energy, add new weighted input
            accum[binIdx] = accum[binIdx] * decayPerHop + mag * resonanceGain;

            // Soft clip the accumulator to prevent runaway
            accum[binIdx] = std::tanh(accum[binIdx] * 0.5f) * 2.0f;

            // Guard against NaN/Inf
            if (std::isnan(accum[binIdx]) || std::isinf(accum[binIdx]))
                accum[binIdx] = 0.0f;

            // Reconstruct output: use accumulated magnitude with original phase
            float phase = std::atan2(im, re);
            float outMag = accum[binIdx];

            fftBuffer[realIdx] = outMag * std::cos(phase);
            fftBuffer[imagIdx] = outMag * std::sin(phase);
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(fftBuffer.data());

        // Overlap-add into output FIFO
        for (int i = 0; i < fftSize; ++i)
        {
            // Apply synthesis window and normalize for 75% overlap (sum of squared Hann = 1.5)
            float sample = fftBuffer[static_cast<size_t>(i)] * window[static_cast<size_t>(i)] * (2.0f / 3.0f);
            outputFifo[chIdx][static_cast<size_t>(i)] += sample;
        }

        // Shift output FIFO: move by hopSize
        for (int i = 0; i < fftSize + hopSize; ++i)
        {
            if (i + hopSize < static_cast<int>(outputFifo[chIdx].size()))
                outputFifo[chIdx][static_cast<size_t>(i)] = outputFifo[chIdx][static_cast<size_t>(i + hopSize)];
            else
                outputFifo[chIdx][static_cast<size_t>(i)] = 0.0f;
        }
    }

    juce::dsp::FFT fft;
    double currentSampleRate = 44100.0;
    int channels = 2;

    // Per-channel buffers
    std::vector<std::vector<float>> inputFifo;      // [channel][hopSize]
    std::vector<std::vector<float>> outputFifo;      // [channel][fftSize * 2]
    std::vector<int> fifoIndex;                      // [channel]
    std::vector<std::vector<float>> resonanceAccum;  // [channel][numBins]

    // Shared working buffers (single-threaded audio processing)
    std::vector<float> fftBuffer;                    // [fftSize * 2]
    std::vector<float> window;                       // [fftSize]
    std::vector<float> analysisFrame = std::vector<float>(static_cast<size_t>(fftSize), 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralResonatorEngine)
};
