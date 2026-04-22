import("stdfaust.lib");

// =========================================================================
// Warm Plate Reverb — Dattorro 1997 topology wrapped with output safety
// =========================================================================

// Parameters
preDelMs   = hslider("Pre-Delay", 20.0, 0.0, 100.0, 0.1) : si.smoo;
bandwidth  = hslider("Bandwidth", 0.7, 0.001, 1.0, 0.001) : si.smoo;
decay      = hslider("Decay", 0.7, 0.0, 0.95, 0.001) : si.smoo;
damping    = hslider("Damping", 0.4, 0.0, 0.999, 0.001) : si.smoo;
mix        = hslider("Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;

// Pre-delay conversion: ms → samples (bounded for compile-time analysis)
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000.0));

// Dattorro plate with recommended diffusion coefficients
// Signature: (pre_delay, bw, i_diff1, i_diff2, decay, d_diff1, d_diff2, damping)
plate = re.dattorro_rev(preDelSamples, bandwidth, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Output safety: -1 dB trim + ADAA hard clip
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

process = _,_ 
        : ef.dryWetMixerConstantPower(mix, plate)
        : safeOut, safeOut;
