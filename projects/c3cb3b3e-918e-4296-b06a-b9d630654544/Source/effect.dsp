import("stdfaust.lib");

// Warm Plate Reverb
// Based on Dattorro topology with warm character

preDel = hslider("PreDelay", 10, 0, 100, 1) : int : max(0) : min(100);
bw = hslider("Bandwidth", 0.6, 0.0001, 1, 0.001) : si.smoo;
decay = hslider("Decay", 0.65, 0, 0.9999, 0.0001) : si.smoo;
damp = hslider("Damping", 0.4, 0, 0.999, 0.001) : si.smoo;
size = hslider("Size", 1.0, 0, 10, 0.1) : si.smoo;
mix = hslider("Mix", 0.35, 0, 1, 0.01) : si.smoo;

// Sum L+R to mono, split to 3 channels for Dattorro FDN
monoStereoRev(l, r) = (l + r) * 0.5 <: _,_,_ : re.dattorro_rev(preDel, bw, 0.625, 0.75, decay, damp, size);

process = _,_ : ef.dryWetMixerConstantPower(mix, monoStereoRev);
