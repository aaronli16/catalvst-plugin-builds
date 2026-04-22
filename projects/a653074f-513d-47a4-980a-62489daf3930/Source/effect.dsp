import("stdfaust.lib");

// =========================================================================
// Warm Plate Reverb
//
// Classic plate character with warm, smooth tail.
// Based on Dattorro 1997 plate reverb topology.
// =========================================================================

// ---- Parameters ---------------------------------------------------------
decay      = hslider("[01] Decay",    0.7,   0.0,  0.95,  0.001) : si.smoo;
damping    = hslider("[02] Damping",  0.15,  0.0,  0.999, 0.001) : si.smoo;
size       = hslider("[03] Size",     0.5,   0.0,  1.0,   0.01)  : si.smoo;
mix        = hslider("[04] Dry/Wet",  0.35,  0.0,  1.0,   0.01)  : si.smoo;

// ---- Plate Configuration ------------------------------------------------
// Pre-delay mapped from Size parameter (20-80ms)
preDelMs      = 20.0 + (size * 60.0);
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.1 * 96000.0));

// Bandwidth mapped from Size (smaller = darker/warmer, larger = brighter)
bw = 0.5 + (size * 0.4);

// Dattorro plate with recommended diffusion coefficients
plate = re.dattorro_rev(preDelSamples, bw, 0.625, 0.75, decay, 0.7, 0.5, damping);

// ---- Output Safety ------------------------------------------------------
DB_MINUS_1 = 0.8913;
safeOut    = *(DB_MINUS_1) : aa.hardclip;

// ---- Process ------------------------------------------------------------
process = _,_ 
        : ef.dryWetMixerConstantPower(mix, plate)
        : safeOut, safeOut;
