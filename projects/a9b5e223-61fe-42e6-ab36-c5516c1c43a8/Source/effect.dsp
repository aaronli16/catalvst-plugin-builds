import("stdfaust.lib");

// Warm Plate Reverb — lush plate with damping control for warmth

// Parameters
preDelMs = hslider("[01] Pre-Delay [unit:ms]", 20.0, 0.0, 100.0, 0.1) : si.smoo;
decay    = hslider("[02] Decay", 0.7, 0.0, 0.95, 0.001) : si.smoo;
damping  = hslider("[03] Damping", 0.3, 0.0, 0.999, 0.001) : si.smoo;
size     = hslider("[04] Size", 0.7, 0.001, 1.0, 0.001) : si.smoo;
mix      = hslider("[05] Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;

// Pre-delay in samples — bound for Faust's interval analysis
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000));

// Size controls bandwidth (high frequency content entering the reverb)
// Lower bandwidth = warmer, darker character
bandwidth = size;

// Dattorro plate with recommended diffusion coefficients
plate = re.dattorro_rev(preDelSamples, bandwidth, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Output safety
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

process = _,_ : ef.dryWetMixerConstantPower(mix, plate) : safeOut, safeOut;
