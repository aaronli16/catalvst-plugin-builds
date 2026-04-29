import("stdfaust.lib");

// Warm plate reverb — classic plate character with smooth tail

// Parameters
decay   = hslider("Decay", 0.7, 0.0, 0.95, 0.001) : si.smoo;
damping = hslider("Damping", 0.4, 0.0, 0.99, 0.001) : si.smoo;  // Higher default for warmth
size    = hslider("Size", 0.7, 0.001, 1.0, 0.001) : si.smoo;     // Maps to bandwidth
mix     = hslider("Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;

// Pre-delay kept short for plate character (20ms fixed)
preDelSamples = 20.0 * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000.0));

// Dattorro plate reverb with recommended diffusion coefficients
// Parameters: (pre_delay, bw, i_diff1, i_diff2, decay, d_diff1, d_diff2, damping)
plate = re.dattorro_rev(preDelSamples, size, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Output safety — soft ceiling to prevent clipping
safeOut = *(0.8913) : aa.hardclip;

process = _,_ : ef.dryWetMixerConstantPower(mix, plate) : safeOut, safeOut;
