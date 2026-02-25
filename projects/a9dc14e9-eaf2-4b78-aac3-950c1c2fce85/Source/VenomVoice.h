#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <random>

// =============================================================================
// VenomVoice — A single monophonic/polyphonic bass synth voice
// =============================================================================

struct VenomVoiceParams
{
    // Oscillator
    int waveform = 0;       // 0=sine, 1=saw, 2=square, 3=triangle
    float coarseSt = 0.0f;  // semitones -24..24
    float fineCt = 0.0f;    // cents -100..100
    float subLevel = 0.6f;  // 0..1
    float noiseLevel = 0.0f;// 0..1
    int unisonCount = 1;    // 1..8
    float detune = 0.2f;    // 0..1

    // Filter
    int filterType = 0;     // 0=LP, 1=BP, 2=HP
    float cutoff = 800.0f;  // Hz
    float resonance = 0.3f; // 0..1
    float filterEnv = 0.5f; // 0..1
    float filterDrive = 0.0f; // 0..1

    // Amp Envelope
    float attackMs = 5.0f;
    float decayMs = 200.0f;
    float sustain = 0.8f;
    float releaseMs = 150.0f;

    // LFO
    int lfoShape = 0;
    float lfoRate = 4.0f;
    float lfoDepth = 0.5f;

    // Distortion
    int distType = 0;       // 0=soft, 1=hard, 2=fold
    float distAmount = 0.3f;// 0..1
    float distMix = 0.5f;   // 0..1

    // Effects
    float chorusMix = 0.0f;
    float reverbMix = 0.1f;
    float delayMix = 0.0f;
    float crushAmount = 0.4f;

    // Output
    float outputGainDb = 0.0f;
};

class VenomOscillator
{
public:
    void reset()
    {
        phase = 0.0;
    }

    void setFrequency(double freq, double sr)
    {
        increment = freq / sr;
    }

    float generate(int waveform)
    {
        float out = 0.0f;
        double p = phase;

        switch (waveform)
        {
            case 0: // Sine
                out = static_cast<float>(std::sin(p * juce::MathConstants<double>::twoPi));
                break;
            case 1: // Saw (PolyBLEP)
                out = static_cast<float>(2.0 * p - 1.0);
                out -= polyBlep(p, increment);
                break;
            case 2: // Square (PolyBLEP)
                out = (p < 0.5) ? 1.0f : -1.0f;
                out += polyBlep(p, increment);
                out -= polyBlep(std::fmod(p + 0.5, 1.0), increment);
                break;
            case 3: // Triangle (integrated square)
                out = (p < 0.5) ? 1.0f : -1.0f;
                out += polyBlep(p, increment);
                out -= polyBlep(std::fmod(p + 0.5, 1.0), increment);
                // Leaky integrator to form triangle from square
                triState = 0.999f * triState + out * static_cast<float>(increment) * 4.0f;
                out = triState;
                break;
        }

        phase += increment;
        if (phase >= 1.0) phase -= 1.0;

        return out;
    }

private:
    double phase = 0.0;
    double increment = 0.0;
    float triState = 0.0f;

    static float polyBlep(double t, double dt)
    {
        if (t < dt)
        {
            double n = t / dt;
            return static_cast<float>(n + n - n * n - 1.0);
        }
        else if (t > 1.0 - dt)
        {
            double n = (t - 1.0) / dt;
            return static_cast<float>(n * n + n + n + 1.0);
        }
        return 0.0f;
    }
};

class VenomEnvelope
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void reset()
    {
        stage = Idle;
        output = 0.0f;
    }

    void setParams(float attackMs, float decayMs, float sustainLevel, float releaseMs, double sampleRate)
    {
        attackRate = (attackMs > 0.1f) ? 1.0f / (static_cast<float>(sampleRate) * attackMs * 0.001f) : 1.0f;
        decayRate = (decayMs > 0.1f) ? 1.0f / (static_cast<float>(sampleRate) * decayMs * 0.001f) : 1.0f;
        sustain = sustainLevel;
        releaseRate = (releaseMs > 0.1f) ? 1.0f / (static_cast<float>(sampleRate) * releaseMs * 0.001f) : 1.0f;
    }

    void noteOn()
    {
        stage = Attack;
    }

    void noteOff()
    {
        if (stage != Idle)
            stage = Release;
    }

    bool isActive() const { return stage != Idle; }

    float process()
    {
        switch (stage)
        {
            case Idle:
                return 0.0f;
            case Attack:
                output += attackRate;
                if (output >= 1.0f)
                {
                    output = 1.0f;
                    stage = Decay;
                }
                break;
            case Decay:
                output -= (output - sustain) * decayRate;
                if (output <= sustain + 0.0001f)
                {
                    output = sustain;
                    stage = Sustain;
                }
                break;
            case Sustain:
                output = sustain;
                break;
            case Release:
                output -= output * releaseRate;
                if (output < 0.0001f)
                {
                    output = 0.0f;
                    stage = Idle;
                }
                break;
        }
        return output;
    }

private:
    Stage stage = Idle;
    float output = 0.0f;
    float attackRate = 0.01f;
    float decayRate = 0.001f;
    float sustain = 0.8f;
    float releaseRate = 0.001f;
};

class VenomLFO
{
public:
    void reset()
    {
        phase = 0.0;
    }

    void setRate(float hz, double sampleRate)
    {
        increment = static_cast<double>(hz) / sampleRate;
    }

    float process(int shape)
    {
        float out = 0.0f;
        switch (shape)
        {
            case 0: // Sine
                out = std::sin(static_cast<float>(phase) * juce::MathConstants<float>::twoPi);
                break;
            case 1: // Saw
                out = static_cast<float>(2.0 * phase - 1.0);
                break;
            case 2: // Square
                out = (phase < 0.5) ? 1.0f : -1.0f;
                break;
            case 3: // Triangle
                out = static_cast<float>(4.0 * std::abs(phase - 0.5) - 1.0);
                break;
        }
        phase += increment;
        if (phase >= 1.0) phase -= 1.0;
        return out;
    }

private:
    double phase = 0.0;
    double increment = 0.0;
};

// =============================================================================
// A single polyphonic voice
// =============================================================================

class VenomVoice
{
public:
    static constexpr int MAX_UNISON = 8;

    void prepare(double sr)
    {
        sampleRate = sr;
        for (auto& osc : unisonOscs) osc.reset();
        subOsc.reset();
        ampEnv.reset();
        filterEnv.reset();
        lfo.reset();
        currentNote = -1;

        // Pre-seed random generator
        rng.seed(42);
    }

    void noteOn(int note, float velocity)
    {
        currentNote = note;
        vel = velocity;
        ampEnv.noteOn();
        filterEnv.noteOn();
        for (auto& osc : unisonOscs) osc.reset();
        subOsc.reset();
    }

    void noteOff()
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
    }

    bool isActive() const { return ampEnv.isActive(); }
    int getNote() const { return currentNote; }

    float process(const VenomVoiceParams& p)
    {
        if (!isActive()) return 0.0f;

        // Update envelopes
        ampEnv.setParams(p.attackMs, p.decayMs, p.sustain, p.releaseMs, sampleRate);
        filterEnv.setParams(5.0f, p.decayMs * 0.8f, 0.0f, p.releaseMs, sampleRate);

        float ampVal = ampEnv.process();
        float fEnvVal = filterEnv.process();

        // LFO
        lfo.setRate(p.lfoRate, sampleRate);
        float lfoVal = lfo.process(p.lfoShape);

        // Compute base frequency with coarse/fine tuning
        double baseFreq = 440.0 * std::pow(2.0, (currentNote - 69.0 + p.coarseSt + p.fineCt * 0.01) / 12.0);

        // Main oscillator(s) with unison
        float oscOut = 0.0f;
        int voices = juce::jlimit(1, MAX_UNISON, p.unisonCount);
        float detuneRange = p.detune * 0.03f; // max ~3% detune

        for (int u = 0; u < voices; ++u)
        {
            float detuneOffset = 0.0f;
            if (voices > 1)
                detuneOffset = detuneRange * (2.0f * static_cast<float>(u) / static_cast<float>(voices - 1) - 1.0f);

            double freq = baseFreq * (1.0 + detuneOffset);
            unisonOscs[u].setFrequency(freq, sampleRate);
            oscOut += unisonOscs[u].generate(p.waveform);
        }
        oscOut /= std::sqrt(static_cast<float>(voices)); // Normalize unison

        // Sub oscillator (one octave down, always sine)
        subOsc.setFrequency(baseFreq * 0.5, sampleRate);
        float subOut = subOsc.generate(0) * p.subLevel;

        // Noise
        float noiseOut = 0.0f;
        if (p.noiseLevel > 0.001f)
        {
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            noiseOut = dist(rng) * p.noiseLevel * 0.5f;
        }

        // Mix oscillators
        float mix = oscOut + subOut + noiseOut;

        // Filter with LFO modulation and envelope
        float lfoMod = lfoVal * p.lfoDepth;
        float envMod = fEnvVal * p.filterEnv;
        float modCutoff = p.cutoff * std::pow(2.0f, lfoMod * 3.0f + envMod * 4.0f);
        modCutoff = juce::jlimit(20.0f, 20000.0f, modCutoff);

        // Filter drive (pre-filter saturation)
        if (p.filterDrive > 0.01f)
        {
            float driveGain = 1.0f + p.filterDrive * 8.0f;
            mix = std::tanh(mix * driveGain) / std::tanh(driveGain);
        }

        // Simple 2-pole state variable filter
        mix = processFilter(mix, modCutoff, p.resonance, p.filterType);

        // Apply amp envelope
        mix *= ampVal * vel;

        return mix;
    }

private:
    double sampleRate = 44100.0;
    int currentNote = -1;
    float vel = 1.0f;

    VenomOscillator unisonOscs[MAX_UNISON];
    VenomOscillator subOsc;
    VenomEnvelope ampEnv;
    VenomEnvelope filterEnv;
    VenomLFO lfo;
    std::mt19937 rng;

    // State variable filter state
    float svfLow = 0.0f, svfBand = 0.0f, svfHigh = 0.0f;

    float processFilter(float input, float cutoff, float reso, int type)
    {
        float g = std::tan(juce::MathConstants<float>::pi * juce::jlimit(20.0f, 20000.0f, cutoff) / static_cast<float>(sampleRate));
        float q = 0.5f + reso * 9.5f; // Q from 0.5 to 10
        float k = 1.0f / q;

        // Oversample 2x for stability
        for (int os = 0; os < 2; ++os)
        {
            svfHigh = (input - (k + g) * svfBand - svfLow) / (1.0f + k * g + g * g);
            svfBand += g * svfHigh;
            svfLow += g * svfBand;

            // Soft-clip band state to prevent blowup
            svfBand = std::tanh(svfBand);
        }

        switch (type)
        {
            case 0: return svfLow;    // Low-pass
            case 1: return svfBand;   // Band-pass
            case 2: return svfHigh;   // High-pass
            default: return svfLow;
        }
    }
};

// =============================================================================
// VenomSynthEngine — Manages polyphony and global effects
// =============================================================================

class VenomSynthEngine
{
public:
    static constexpr int MAX_VOICES = 16;

    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        for (auto& v : voices) v.prepare(sampleRate);

        // Prepare global effects
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
        spec.numChannels = 2;

        chorus.prepare(spec);
        chorus.reset();

        reverb.prepare(spec);
        reverb.reset();

        delayLine.prepare(spec);
        delayLine.reset();

        // Pre-allocate mono buffer
        monoBuffer.setSize(1, maxBlockSize);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const VenomVoiceParams& params)
    {
        int numSamples = buffer.getNumSamples();
        buffer.clear();

        // Handle MIDI
        for (const auto metadata : midi)
        {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn())
                handleNoteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())
                handleNoteOff(msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                allNotesOff();
        }

        // Render voices into mono, then copy to stereo
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = 0.0f;
            for (auto& v : voices)
            {
                if (v.isActive())
                    sample += v.process(params);
            }

            // Distortion (post-voice, pre-effects)
            sample = applyDistortion(sample, params.distType, params.distAmount, params.distMix);

            // Bit crush effect
            if (params.crushAmount > 0.01f)
                sample = applyCrush(sample, params.crushAmount);

            // NaN/Inf guard
            if (std::isnan(sample) || std::isinf(sample))
                sample = 0.0f;

            // Write to both channels (mono synth -> stereo)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample(ch, i, sample);
        }

        // Apply global effects using juce::dsp pipeline
        applyChorus(buffer, params.chorusMix);
        applyReverb(buffer, params.reverbMix);
        applyDelay(buffer, params.delayMix);

        // Output gain
        float gain = juce::Decibels::decibelsToGain(params.outputGainDb);
        buffer.applyGain(gain);

        // Final NaN guard on entire buffer
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (std::isnan(data[i]) || std::isinf(data[i]))
                    data[i] = 0.0f;
            }
        }
    }

private:
    double sr = 44100.0;
    VenomVoice voices[MAX_VOICES];

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 88200 };

    juce::AudioBuffer<float> monoBuffer;

    // Crush state
    float crushHold = 0.0f;
    int crushCounter = 0;

    void handleNoteOn(int note, float velocity)
    {
        // Find a free voice or steal the oldest
        for (auto& v : voices)
        {
            if (!v.isActive())
            {
                v.noteOn(note, velocity);
                return;
            }
        }
        // Voice stealing: reuse first voice
        voices[0].noteOn(note, velocity);
    }

    void handleNoteOff(int note)
    {
        for (auto& v : voices)
        {
            if (v.isActive() && v.getNote() == note)
            {
                v.noteOff();
                return;
            }
        }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            v.noteOff();
    }

    float applyDistortion(float sample, int type, float amount, float mix)
    {
        if (amount < 0.01f) return sample;

        float dry = sample;
        float driveGain = 1.0f + amount * 20.0f;
        float driven = sample * driveGain;
        float wet = 0.0f;

        switch (type)
        {
            case 0: // Soft clip (tanh)
                wet = std::tanh(driven);
                break;
            case 1: // Hard clip
                wet = juce::jlimit(-1.0f, 1.0f, driven);
                break;
            case 2: // Wave fold
            {
                float folded = driven;
                // Fold back and forth
                while (folded > 1.0f || folded < -1.0f)
                {
                    if (folded > 1.0f) folded = 2.0f - folded;
                    if (folded < -1.0f) folded = -2.0f - folded;
                }
                wet = folded;
                break;
            }
        }

        return dry * (1.0f - mix) + wet * mix;
    }

    float applyCrush(float sample, float amount)
    {
        // Bit reduction + sample rate reduction
        int bits = juce::jlimit(2, 16, static_cast<int>(16.0f - amount * 12.0f));
        float steps = std::pow(2.0f, static_cast<float>(bits));
        float crushed = std::round(sample * steps) / steps;

        // Sample rate reduction
        int holdSamples = juce::jlimit(1, 32, static_cast<int>(1.0f + amount * 24.0f));
        crushCounter++;
        if (crushCounter >= holdSamples)
        {
            crushHold = crushed;
            crushCounter = 0;
        }

        return crushHold;
    }

    void applyChorus(juce::AudioBuffer<float>& buffer, float mix)
    {
        if (mix < 0.01f) return;

        chorus.setRate(1.5f);
        chorus.setDepth(0.3f);
        chorus.setCentreDelay(7.0f);
        chorus.setFeedback(-0.2f);
        chorus.setMix(mix);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        chorus.process(context);
    }

    void applyReverb(juce::AudioBuffer<float>& buffer, float mix)
    {
        if (mix < 0.01f) return;

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.6f;
        rp.damping = 0.4f;
        rp.wetLevel = mix;
        rp.dryLevel = 1.0f - mix * 0.5f;
        rp.width = 1.0f;
        reverb.setParameters(rp);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);
    }

    void applyDelay(juce::AudioBuffer<float>& buffer, float mix)
    {
        if (mix < 0.01f) return;

        float delaySamples = static_cast<float>(sr) * 0.375f; // Dotted 8th at ~80 BPM feel
        float feedback = 0.35f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float delayed = delayLine.popSample(ch, delaySamples);
                float input = data[i] + delayed * feedback;
                input = std::tanh(input); // Prevent feedback blowup
                delayLine.pushSample(ch, input);
                data[i] = data[i] * (1.0f - mix) + delayed * mix;
            }
        }
    }
};
