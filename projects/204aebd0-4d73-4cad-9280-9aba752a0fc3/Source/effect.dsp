import("stdfaust.lib");

// =========================================================================
// Warm Plate Reverb
// Dattorro 1997 topology — lush, smooth tail with warm damping character.
// =========================================================================

// Parameters
preDelMs   = hslider("[01] Pre-Delay [unit:ms]", 20.0, 0.0, 100.0, 0.1) : si.smoo;
decay      = hslider("[02] Decay", 0.7, 0.0, 0.95, 0.001) : si.smoo;
damping    = hslider("[03] Damping", 0.15, 0.0, 0.999, 0.001) : si.smoo;
size       = hslider("[04] Size", 0.7, 0.001, 1.0, 0.001) : si.smoo;
mix        = hslider("[05] Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;

// Pre-delay in samples (bounded for Faust interval analysis)
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000.0));

// Dattorro plate with recommended diffusion coefficients
// size parameter controls bandwidth (higher = brighter early reflections)
plate = re.dattorro_rev(preDelSamples, size, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Output safety — -1 dB ceiling + ADAA hard clip
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

process = _,_ 
        : ef.dryWetMixerConstantPower(mix, plate)
        : safeOut, safeOut;
