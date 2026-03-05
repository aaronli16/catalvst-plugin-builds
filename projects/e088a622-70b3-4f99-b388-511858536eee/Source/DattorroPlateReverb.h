#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>

class DattorroAllpass {
public:
    void prepare(int len) { size = len; buffer.assign(size, 0.0f); writeIdx = 0; }
    float process(float x, float g) {
        const float delayed = buffer[writeIdx];
        const float v = x - g * delayed;
        buffer[writeIdx] = v;
        writeIdx = (writeIdx + 1) % size;
        return g * v + delayed;
    }
    float readTap(int tap) const {
        tap = juce::jlimit(1, size, tap);
        return buffer[(writeIdx - tap + size) % size];
    }
    void reset() { std::fill(buffer.begin(), buffer.end(), 0.0f); writeIdx = 0; }
private:
    std::vector<float> buffer;
    int size = 1, writeIdx = 0;
};

class DattorroDelay {
public:
    void prepare(int len) { size = len; buffer.assign(size, 0.0f); writeIdx = 0; }
    void write(float x) { buffer[writeIdx] = x; writeIdx = (writeIdx + 1) % size; }
    float readTap(int tap) const {
        tap = juce::jlimit(1, size, tap);
        return buffer[(writeIdx - tap + size) % size];
    }
    void reset() { std::fill(buffer.begin(), buffer.end(), 0.0f); writeIdx = 0; }
private:
    std::vector<float> buffer;
    int size = 1, writeIdx = 0;
};

class DattorroPlateReverb {
public:
    void prepare(double newSampleRate, int /*samplesPerBlock*/) {
        sr = static_cast<float>(newSampleRate);
        const float k = sr / 29761.0f;
        inputAP[0].prepare(juce::roundToInt(142 * k));
        inputAP[1].prepare(juce::roundToInt(107 * k));
        inputAP[2].prepare(juce::roundToInt(379 * k));
        inputAP[3].prepare(juce::roundToInt(277 * k));
        tankAP[0].prepare(juce::roundToInt(672 * k));
        tankAP[1].prepare(juce::roundToInt(908 * k));
        tankD[0].prepare(juce::roundToInt(4453 * k));
        tankD[1].prepare(juce::roundToInt(4217 * k));
        tap[0] = juce::roundToInt(266 * k);  tap[1] = juce::roundToInt(2974 * k);
        tap[2] = juce::roundToInt(1913 * k); tap[3] = juce::roundToInt(1996 * k);
        tap[4] = juce::roundToInt(355 * k);  tap[5] = juce::roundToInt(3124 * k);
        tap[6] = juce::roundToInt(2111 * k); tap[7] = juce::roundToInt(335 * k);
        const int predelayMax = juce::roundToInt(sr * 0.101f);
        predelay.prepare(predelayMax);
        avgDelaySec = (4453.0f + 4217.0f) * 0.5f * k / sr;
        lpState[0] = lpState[1] = 0.0f;
        tankFeedback[0] = tankFeedback[1] = 0.0f;
    }

    // Processes buffer to contain WET-ONLY output (caller handles dry/wet mix)
    void processWetOnly(juce::AudioBuffer<float>& buffer, float decaySeconds, float damping, float predelayMs) {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const float decay = juce::jlimit(0.0f, 0.9999f,
            std::pow(10.0f, -3.0f * avgDelaySec / juce::jlimit(0.1f, 20.0f, decaySeconds)));
        const float damp = juce::jlimit(0.0f, 0.9999f, damping);
        const int predelayN = juce::roundToInt(juce::jlimit(0.0f, 100.0f, predelayMs) / 1000.0f * sr);
        const float lpCoeff = 1.0f - damp;

        for (int s = 0; s < numSamples; ++s) {
            float inputMono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputMono += buffer.getSample(ch, s);
            if (numChannels > 1) inputMono /= static_cast<float>(numChannels);

            predelay.write(inputMono);
            float x = predelayN > 0 ? predelay.readTap(predelayN) : inputMono;
            x = inputAP[0].process(x, 0.75f);
            x = inputAP[1].process(x, 0.75f);
            x = inputAP[2].process(x, 0.625f);
            x = inputAP[3].process(x, 0.625f);

            float pathA = x + decay * tankFeedback[1];
            pathA = tankAP[0].process(pathA, 0.70f);
            lpState[0] = damp * lpState[0] + lpCoeff * pathA;
            pathA = lpState[0] * decay;
            tankD[0].write(pathA);

            float pathB = x + decay * tankFeedback[0];
            pathB = tankAP[1].process(pathB, 0.50f);
            lpState[1] = damp * lpState[1] + lpCoeff * pathB;
            pathB = lpState[1] * decay;
            tankD[1].write(pathB);

            tankFeedback[0] = tankD[0].readTap(juce::roundToInt(4453 * sr / 29761.0f));
            tankFeedback[1] = tankD[1].readTap(juce::roundToInt(4217 * sr / 29761.0f));

            float outL = 0.6f * tankD[0].readTap(tap[0]) + 0.6f * tankD[0].readTap(tap[1])
                       - 0.6f * tankAP[1].readTap(tap[2]) + 0.6f * tankD[1].readTap(tap[3]);
            float outR = 0.6f * tankD[1].readTap(tap[4]) + 0.6f * tankD[1].readTap(tap[5])
                       - 0.6f * tankAP[0].readTap(tap[7]) + 0.6f * tankD[0].readTap(tap[6]);

            if (std::isnan(outL) || std::isinf(outL)) outL = 0.0f;
            if (std::isnan(outR) || std::isinf(outR)) outR = 0.0f;

            if (numChannels >= 2) {
                buffer.setSample(0, s, outL);
                buffer.setSample(1, s, outR);
            } else {
                buffer.setSample(0, s, 0.5f * (outL + outR));
            }
        }
    }

    void reset() {
        for (auto& ap : inputAP) ap.reset();
        for (auto& ap : tankAP) ap.reset();
        for (auto& d : tankD) d.reset();
        predelay.reset();
        lpState[0] = lpState[1] = 0.0f;
        tankFeedback[0] = tankFeedback[1] = 0.0f;
    }

private:
    float sr = 44100.0f;
    float avgDelaySec = 0.0f;
    std::array<DattorroAllpass, 4> inputAP;
    std::array<DattorroAllpass, 2> tankAP;
    std::array<DattorroDelay, 2> tankD;
    DattorroDelay predelay;
    float lpState[2] = {};
    float tankFeedback[2] = {};
    int tap[8] = {};
};
