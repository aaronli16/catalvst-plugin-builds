import("stdfaust.lib");

// Warm Plate Reverb
// Classic plate reverb with smooth, lush tail using Dattorro topology

// Parameters
preDelMs   = hslider("[01] Pre-Delay [unit:ms]", 25.0, 0.0, 100.0, 0.1) : si.smoo;
decay      = hslider("[02] Decay", 0.7, 0.0, 0.95, 0.001) : si.smoo;
damping    = hslider("[03] Damping", 0.3, 0.0, 0.999, 0.001) : si.smoo;
mix        = hslider("[04] Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;

// Pre-delay conversion: ms to samples, bounded for compile-time analysis
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000.0));

// Bandwidth set to 0.65 for warmth (rolls off high frequencies entering the plate)
// Dattorro-recommended diffusion coefficients: i_diff1=0.625, i_diff2=0.75, d_diff1=0.7, d_diff2=0.5
plate = re.dattorro_rev(preDelSamples, 0.65, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Output safety: -1 dB trim + ADAA hard clip ceiling
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

process = _,_ : ef.dryWetMixerConstantPower(mix, plate) : safeOut, safeOut;
