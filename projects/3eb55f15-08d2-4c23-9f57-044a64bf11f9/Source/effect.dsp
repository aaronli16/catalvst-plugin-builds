import("stdfaust.lib");

// Warm plate reverb with Dattorro topology
// Controls: Decay, Damping (warmth/darkness), Mix

// Parameters
decay      = hslider("[01] Decay",    0.7,   0.0,  0.95,  0.001) : si.smoo;
damping    = hslider("[02] Damping",  0.3,   0.0,  0.999, 0.001) : si.smoo;
mix        = hslider("[03] Mix",      0.35,  0.0,  1.0,   0.01)  : si.smoo;

// Fixed pre-delay optimized for plate character
preDelSamples = 20 * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000.0));

// Bandwidth set for warmth
bw = 0.7;

// Dattorro plate with recommended diffusion coefficients
plate = re.dattorro_rev(preDelSamples, bw, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Output safety: -1 dB trim + ADAA hard clip
DB_MINUS_1 = 0.8913;
safeOut = *(DB_MINUS_1) : aa.hardclip;

process = _,_ 
        : ef.dryWetMixerConstantPower(mix, plate)
        : safeOut, safeOut;
