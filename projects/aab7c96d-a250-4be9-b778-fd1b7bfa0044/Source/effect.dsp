import("stdfaust.lib");

// Warm plate reverb — Dattorro 1997 topology with damping for warmth

// Parameters
decay      = hslider("Decay", 0.7, 0.0, 0.95, 0.001) : si.smoo;
damping    = hslider("Damping", 0.3, 0.0, 0.999, 0.001) : si.smoo;
size       = hslider("Size", 0.5, 0.1, 1.0, 0.01) : si.smoo;
mix        = hslider("Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;

// Pre-delay scaled by size parameter (10-50 ms range)
preDelMs = 10.0 + (size * 40.0);
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.1 * ma.SR));

// Bandwidth — slightly reduced for warmth
bw = 0.7;

// Dattorro plate reverb with recommended diffusion coefficients
plate = re.dattorro_rev(preDelSamples, bw, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Constant-power dry/wet mix
process = _,_ : ef.dryWetMixerConstantPower(mix, plate);
