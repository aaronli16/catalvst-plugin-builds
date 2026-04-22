import("stdfaust.lib");

// ===================================================================
// Warm Plate Reverb — Dattorro 1997 plate with lush smooth tail
// ===================================================================

// Parameters
size       = hslider("Size", 0.5, 0, 1, 0.01) : si.smoo;
decay      = hslider("Decay", 0.7, 0.0, 0.95, 0.001) : si.smoo;
damping    = hslider("Damping", 0.4, 0.0, 0.999, 0.001) : si.smoo;
mix        = hslider("Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;

// Map size to pre-delay (0-50ms range for plate scale)
preDelMs = size * 50.0;
preDelSamples = preDelMs * ma.SR / 1000.0 : int : max(0) : min(int(0.05 * 96000.0));

// Bandwidth scales with size — larger plates have more high-frequency energy
bw = 0.6 + (size * 0.3);

// Dattorro plate with recommended diffusion coefficients
plate = re.dattorro_rev(preDelSamples, bw, 0.625, 0.75, decay, 0.7, 0.5, damping);

// Constant-power mix
process = _,_ : ef.dryWetMixerConstantPower(mix, plate);
