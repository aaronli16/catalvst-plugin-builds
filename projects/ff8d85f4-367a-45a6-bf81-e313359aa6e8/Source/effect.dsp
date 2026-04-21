import("stdfaust.lib");

// Warm vintage plate reverb
// Topology: Dattorro 1997 plate with warm character tuning

// ---- knobs ---------------------------------------------------------------
decay      = hslider("[01] Decay",     0.7,   0.0,    0.95,   0.001) : si.smoo;
damping    = hslider("[02] Damping",   0.35,  0.0,    0.999,  0.001) : si.smoo;
size       = hslider("[03] Size",      0.65,  0.3,    1.0,    0.001) : si.smoo;
mix        = hslider("[04] Mix",       0.35,  0.0,    1.0,    0.01)  : si.smoo;

// Pre-delay scaled by size — smaller rooms have shorter pre-delay
preDelMs = 15.0 * size;
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.1 * 96000.0));

// Bandwidth scaled inversely with damping for vintage warmth
// Higher damping = darker tail, so reduce input bandwidth too
bw = max(0.4, 0.9 - damping * 0.5);

// Dattorro-recommended diffusion coefficients for plate character
plate = re.dattorro_rev(preDelSamples, bw, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Output safety: -1 dB ceiling + ADAA hard clip
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

process = _,_ 
        : ef.dryWetMixerConstantPower(mix, plate)
        : safeOut, safeOut;
