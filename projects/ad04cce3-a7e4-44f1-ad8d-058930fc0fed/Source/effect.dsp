import("stdfaust.lib");

// =========================================================================
// Warm Plate Reverb
//
// Dense Dattorro plate reverb with darker, warmer character.
// Darker bandwidth + increased damping for vintage EMT-style smooth tail.
// =========================================================================

// ---- knobs ---------------------------------------------------------------
decay      = hslider("[01] Decay",       0.7,   0.0,    0.95,   0.001) : si.smoo;
damping    = hslider("[02] Damping",     0.3,   0.0,    0.999,  0.001) : si.smoo;
preDelMs   = hslider("[03] Pre-Delay [unit:ms]", 20.0, 0.0, 100.0, 0.1) : si.smoo;
diffusion  = hslider("[04] Diffusion",   0.7,   0.0,    1.0,    0.001) : si.smoo;
mix        = hslider("[05] Mix",         0.35,  0.0,    1.0,    0.01)  : si.smoo;

// ---- plate reverb --------------------------------------------------------
// Pre-delay conversion: ms to samples, bounded for Faust interval analysis
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000.0));

// Bandwidth for warmth — darker than default (0.5 vs typical 0.7)
bw = 0.5;

// Use diffusion knob to control input diffuser blend
i_diff1 = 0.625 * diffusion;
i_diff2 = 0.75 * diffusion;

// Dattorro-recommended decay diffusion coefficients
d_diff1 = 0.7;
d_diff2 = 0.5;

plate = re.dattorro_rev(preDelSamples, bw, i_diff1, i_diff2, decay, d_diff1, d_diff2, damping);

// ---- output safety -------------------------------------------------------
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

// ---- signal flow ---------------------------------------------------------
process = _,_ : ef.dryWetMixerConstantPower(mix, plate) : safeOut, safeOut;
