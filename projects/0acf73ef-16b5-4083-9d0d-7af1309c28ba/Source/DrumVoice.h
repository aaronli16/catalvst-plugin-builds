#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// ==============================================================================
// DrumVoice — A single synthesised drum voice with analog-style character.
// Each voice produces a short burst of sine + noise shaped by pitch and
// amplitude envelopes, then passed through saturation and tone filtering.
// ==============================================================================

class DrumVoice
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        phase         = 0.0;
        ampEnv        = 0.0f;
        pitchEnv      = 0.0f;
        noiseEnv      = 0.0f;
        active        = false;
        sampleIndex   = 0;
        filterState   = 0.0f;
    }

    // Trigger the voice with a given base frequency and velocity (0-1).
    void trigger(float baseFreqHz, float velocity, float noiseAmount,
                 float decayNorm, float punchNorm)
    {
        baseFreq      = baseFreqHz;
        vel           = velocity;
        noiseMix      = noiseAmount;

        // Decay time mapped 20 ms – 800 ms
        decayTime     = 0.02f + decayNorm * 0.78f;
        // Punch controls the pitch sweep depth (0-3 octaves above base)
        pitchSweep    = punchNorm * 3.0f;

        ampEnv        = 1.0f;
        pitchEnv      = 1.0f;
        noiseEnv      = 1.0f;
        phase         = 0.0;
        sampleIndex   = 0;
        filterState   = 0.0f;
        active        = true;
    }

    // Render one sample of the drum voice with saturation and tone control.
    float processSample(float driveAmount, float toneNorm)
    {
        if (!active)
            return 0.0f;

        float dt = 1.0f / static_cast<float>(sampleRate);
        float t  = static_cast<float>(sampleIndex) * dt;

        // Amplitude envelope — exponential decay
        float ampDecayRate = 1.0f / std::max(decayTime, 0.005f);
        ampEnv = std::exp(-ampDecayRate * t);

        // Pitch envelope — fast exponential drop (attack transient)
        float pitchDecayRate = 40.0f;
        pitchEnv = std::exp(-pitchDecayRate * t);

        // Noise envelope — very fast decay
        float noiseDecayRate = 60.0f;
        noiseEnv = std::exp(-noiseDecayRate * t);

        // Kill voice when amplitude is negligible
        if (ampEnv < 0.0001f)
        {
            active = false;
            return 0.0f;
        }

        // Frequency with pitch sweep
        float freq = baseFreq * (1.0f + pitchSweep * pitchEnv);
        float phaseInc = static_cast<double>(freq) / sampleRate;
        phase += phaseInc;
        if (phase >= 1.0)
            phase -= 1.0;

        // Oscillator: sine
        float osc = std::sin(2.0f * juce::MathConstants<float>::pi * static_cast<float>(phase));

        // Noise component
        float noise = (random.nextFloat() * 2.0f - 1.0f) * noiseEnv * noiseMix;

        // Mix oscillator and noise
        float raw = (osc * (1.0f - noiseMix * 0.5f) + noise) * ampEnv * vel;

        // Saturation / drive (soft-clip via tanh)
        float driveGain = 1.0f + driveAmount * 12.0f;
        float saturated = std::tanh(raw * driveGain);

        // Simple one-pole low-pass for tone control
        // toneNorm 0 = dark (200 Hz cutoff), 1 = bright (18 kHz)
        float cutoff = 200.0f + toneNorm * toneNorm * 17800.0f;
        float rc     = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoff);
        float alpha   = dt / (rc + dt);
        filterState   = filterState + alpha * (saturated - filterState);

        ++sampleIndex;

        return filterState;
    }

    bool isActive() const { return active; }

private:
    double sampleRate = 44100.0;
    double phase      = 0.0;

    float baseFreq    = 60.0f;
    float vel         = 1.0f;
    float noiseMix    = 0.0f;
    float decayTime   = 0.3f;
    float pitchSweep  = 0.0f;

    float ampEnv      = 0.0f;
    float pitchEnv    = 0.0f;
    float noiseEnv    = 0.0f;

    float filterState = 0.0f;

    bool  active      = false;
    int   sampleIndex = 0;

    juce::Random random;
};


// ==============================================================================
// DrumKit — Maps MIDI notes to drum sounds, manages polyphonic voices.
// ==============================================================================

struct DrumSound
{
    float baseFreqHz  = 60.0f;   // Fundamental pitch
    float noiseAmount = 0.0f;    // 0 = pure tone, 1 = noise heavy
};

class DrumKit
{
public:
    static constexpr int kMaxVoices = 16;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices)
            v.prepare(sampleRate);

        buildSoundMap();
    }

    void reset()
    {
        for (auto& v : voices)
            v.reset();
    }

    void noteOn(int midiNote, float velocity, float decayNorm, float punchNorm)
    {
        auto it = soundMap.find(midiNote);
        if (it == soundMap.end())
            return;

        const auto& snd = it->second;
        auto& voice = voices[nextVoice];
        voice.trigger(snd.baseFreqHz, velocity, snd.noiseAmount, decayNorm, punchNorm);
        nextVoice = (nextVoice + 1) % kMaxVoices;
    }

    float processSample(float drive, float tone)
    {
        float sum = 0.0f;
        for (auto& v : voices)
            sum += v.processSample(drive, tone);

        return sum;
    }

    bool hasActiveVoices() const
    {
        for (auto& v : voices)
            if (v.isActive()) return true;
        return false;
    }

private:
    void buildSoundMap()
    {
        // General MIDI drum map (notes 36-51 cover standard MPC pads)
        // Bank A pads 1-16 map to MIDI notes 36-51
        soundMap[36] = { 55.0f,  0.05f };  // Kick
        soundMap[37] = { 180.0f, 0.45f };  // Rim / Side Stick
        soundMap[38] = { 150.0f, 0.55f };  // Snare
        soundMap[39] = { 200.0f, 0.7f  };  // Clap
        soundMap[40] = { 160.0f, 0.5f  };  // Snare 2
        soundMap[41] = { 90.0f,  0.1f  };  // Low Tom
        soundMap[42] = { 400.0f, 0.85f };  // Closed Hi-Hat
        soundMap[43] = { 100.0f, 0.1f  };  // Low Tom 2
        soundMap[44] = { 400.0f, 0.8f  };  // Pedal Hi-Hat
        soundMap[45] = { 130.0f, 0.1f  };  // Mid Tom
        soundMap[46] = { 350.0f, 0.9f  };  // Open Hi-Hat
        soundMap[47] = { 140.0f, 0.1f  };  // Mid Tom 2
        soundMap[48] = { 200.0f, 0.15f };  // High Tom
        soundMap[49] = { 300.0f, 0.95f };  // Crash
        soundMap[50] = { 220.0f, 0.15f };  // High Tom 2
        soundMap[51] = { 500.0f, 0.75f };  // Ride

        // Additional pads for Conga, Cowbell, Tamb, Cymbal, Shaker, Snap, Click, FX
        soundMap[52] = { 240.0f, 0.2f  };  // Conga
        soundMap[53] = { 650.0f, 0.3f  };  // Cowbell
        soundMap[54] = { 800.0f, 0.85f };  // Tambourine
        soundMap[55] = { 350.0f, 0.95f };  // Cymbal
        soundMap[56] = { 900.0f, 0.9f  };  // Shaker
        soundMap[57] = { 1200.0f, 0.6f };  // Snap
        soundMap[58] = { 1500.0f, 0.3f };  // Click
        soundMap[59] = { 100.0f, 0.7f  };  // FX
    }

    double sr = 44100.0;
    DrumVoice voices[kMaxVoices];
    int nextVoice = 0;
    std::map<int, DrumSound> soundMap;
};
