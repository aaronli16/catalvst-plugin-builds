import("stdfaust.lib");

// Parameters
driveAmt = hslider("Drive", 3.0, 1.0, 10.0, 0.01) : si.smoo;
foldAmt  = hslider("Fold", 2.0, 1.0, 5.0, 0.01) : si.smoo;
tone     = hslider("Tone", 0.5, 0.0, 1.0, 0.01) : si.smoo;
output   = hslider("Output", 0.7, 0.0, 1.0, 0.01) : si.smoo;
mix      = hslider("Mix", 0.5, 0.0, 1.0, 0.01) : si.smoo;

// Wavefolder using sin() for complex metallic harmonics
wavefolder = *(driveAmt) : *(foldAmt) : sin;

// Tone control
toneCutoff = 800 + tone * 17200;

// Distortion chain per channel
distortion = wavefolder : fi.dcblocker : fi.lowpass(2, toneCutoff) : *(output);

process = _,_ : ef.dryWetMixerConstantPower(mix, (distortion, distortion));