import("stdfaust.lib");

// Tape delay — authentic tape echo character with wow/flutter, warm saturation,
// and frequency-dependent decay. Based on DAFX §12.5 Space Echo topology.
//
// Architecture: in -> + -> de.fdelay (wow-modulated) -> loopEQ -> out
//                     ^                                   |
//                     +-------- k * feedback -------------+
//
// Wow/flutter: multi-component (capstan ~22 Hz, pinch ~3.5 Hz, drift ~0.5 Hz)
// summed for organic breathing quality. Depth is 0.5-3% of delay time.
//
// Loop EQ (inside feedback): gentle HP for stability + warmth-controlled LP
// for frequency-dependent decay. NO saturation in the loop to avoid intermod.

// ---- knobs -----------------------------------------------------------
timeMs     = hslider("[01] Delay Time [unit:ms]", 350.0, 40.0, 1000.0, 0.1)  : si.smoo;
feedback   = hslider("[02] Feedback",             0.55,  0.0,   0.95, 0.01) : si.smoo;
mix        = hslider("[03] Mix",                  0.35,  0.0,   1.0,  0.01) : si.smoo;

// ---- wow/flutter: multi-component modulation -----------------------------
// Capstan rotation, pinch-wheel rotation, stochastic drift
wowCapstan = os.osc(22.0);
wowPinch   = os.osc(3.5);
wowDrift   = no.lfnoise(0.5);
wowComposite = wowPinch * 0.6 + wowCapstan * 0.2 + wowDrift * 0.2;

// Delay time + wow modulation (3% depth for realistic tape behavior)
maxSamples  = int(1.05 * 96000.0);
baseSamples = timeMs * ma.SR / 1000.0;
modSamples  = baseSamples * 0.03;
delSamples  = baseSamples + wowComposite * modSamples
            : max(1.0) : min(float(maxSamples) - 1.0);

// ---- loop EQ: HP + warmth LP (inside feedback) ---------------------------
// HP prevents DC/LF runaway. LP creates frequency-dependent decay.
warmthCutoff = 6000.0;
loopEQ = fi.highpass(1, 80.0) : fi.lowpass(1, warmthCutoff);

// ---- feedback delay tap -------------------------------------------------
oneTap(x) = loop ~ _
with {
  loop(fb) = x + (fb : loopEQ : *(feedback))
           : de.fdelay(maxSamples, delSamples);
};

delayStereo = oneTap, oneTap;

// ---- output safety -------------------------------------------------------
DB_MINUS_1 = 0.8913;
safeOut    = *(DB_MINUS_1) : aa.hardclip;

process = _,_
        : ef.dryWetMixerConstantPower(mix, delayStereo)
        : safeOut, safeOut;
