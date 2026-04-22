import("stdfaust.lib");

// =========================================================================
// Warm Plate Reverb
// Based on Dattorro 1997 topology — smooth, lush, musical tail
// =========================================================================

// ---- Controls -----------------------------------------------------------
decay      = hslider("[01] Decay",     0.7,   0.0,  0.95,  0.001) : si.smoo;
damping    = hslider("[02] Damping",   0.35,  0.0,  0.999, 0.001) : si.smoo;
size       = hslider("[03] Size",      0.7,   0.1,  1.0,   0.01)  : si.smoo;
mix        = hslider("[04] Mix",       0.35,  0.0,  1.0,   0.01)  : si.smoo;

// ---- Dattorro Plate Reverb ----------------------------------------------
// Signature: (pre_delay, bw, i_diff1, i_diff2, decay, d_diff1, d_diff2, damping)
// Use Size to control both pre-delay (spaciousness) and bandwidth (air)
// Higher size → longer pre-delay and more high-frequency content
preDelSamples = (size * 50.0 * ma.SR / 1000.0) : int : max(0) : min(int(0.1 * 96000.0));
bandwidth = 0.5 + (size * 0.4);  // 0.5 to 0.9 — darker at small size, brighter at large

// Dattorro-recommended diffusion coefficients
plate = re.dattorro_rev(preDelSamples, bandwidth, 0.625, 0.75, decay, 0.7, 0.5, damping);

// ---- Output Safety ------------------------------------------------------
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

process = _,_ : ef.dryWetMixerConstantPower(mix, plate) : safeOut, safeOut;
