import("stdfaust.lib");

// ===================================================================
// testDAWUIV5 — Warm Plate Reverb
//
// A thick, warm plate reverb with smooth, enveloping tail.
// Optimized for vocals, pads, and strings.
// ===================================================================

// Parameters
decay      = hslider("Decay",     0.7,   0.0,  0.95,   0.001) : si.smoo;
damping    = hslider("Damping",   0.3,   0.0,  0.999,  0.001) : si.smoo;
prefilter  = hslider("Prefilter", 0.5,   0.0,  1.0,    0.001) : si.smoo;
mix        = hslider("Mix",       0.35,  0.0,  1.0,    0.01)  : si.smoo;

// Pre-delay (fixed at 20 ms for smooth plate character)
preDelSamples = int(20.0 * ma.SR / 1000.0) : max(0) : min(10000);

// Dattorro plate with recommended diffusion coefficients
// Signature: (pre_delay, bw, i_diff1, i_diff2, decay, d_diff1, d_diff2, damping)
// prefilter controls bandwidth (input high-frequency rolloff)
plate = re.dattorro_rev(preDelSamples, prefilter, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Constant-power dry/wet mix
process = _,_ : ef.dryWetMixerConstantPower(mix, plate);
