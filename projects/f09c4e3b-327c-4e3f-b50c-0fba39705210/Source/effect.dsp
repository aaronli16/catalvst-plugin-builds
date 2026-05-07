declare name "Robotizer";
declare author "Catalvst";
declare description "Ring-mod robot vocal effect — Daleks/Cylons style.";

import("stdfaust.lib");

// ====== Knob parameters (label = data-param in HTML, EXACT match) ======
robotize = hslider("Robotize", 0.5,  0, 1, 0.01) : si.smoo;
pitch    = hslider("Pitch",    0.5,  0, 1, 0.01) : si.smoo;
tone     = hslider("Tone",     0.5,  0, 1, 0.01) : si.smoo;
width    = hslider("Width",    0.5,  0, 1, 0.01) : si.smoo;

// ====== Mappings ======

// Robotize = ring-mod carrier frequency. Log sweep:
//   0.0 → 20 Hz   (subtle warble, almost tremolo)
//   0.5 → ~110 Hz (classic Cylon clang)
//   1.0 → 600 Hz  (full Dalek metal / inharmonic destruction)
ringFreq    = 20.0 * pow(30.0, robotize);

// Pitch = pitch shift in semitones. Default 0.5 = no shift.
//   0.0 → -12 semitones (deep "Mortal Kombat announcer")
//   0.5 → 0  semitones  (no shift — pure ring mod character)
//   1.0 → +12 semitones (chipmunk robot)
pitchSemi   = (pitch - 0.5) * 24.0;

// Tone = saturation drive + tilt EQ ("bite/aggression" macro)
//   0.0 → drive 1.0, low-shelf boost + high-shelf cut (clean + dark)
//   0.5 → drive ~4.5, flat EQ (neutral)
//   1.0 → drive 8.0, high-shelf boost + low-shelf cut (saturated + bright)
drive       = 1.0 + tone * 7.0;
toneEqDb    = (tone - 0.5) * 24.0;       // ±12 dB

// Width = stereo offset between L and R ring-mod carriers (creates motion)
//   0.0 → identical L/R carriers (mono ring mod)
//   1.0 → 15% pitch offset between L and R carriers (strong stereo beating)
widthOffset = 1.0 + width * 0.15;

// ====== DSP elements ======

// Stereo sine carriers — slight pitch offset on R produces a slow beating
// pattern between L/R that gives natural stereo motion without needing
// a separate LFO. At width=0 both carriers identical (no motion).
ringCarL    = os.osc(ringFreq);
ringCarR    = os.osc(ringFreq * widthOffset);

// Soft-clip saturation: drive into tanh. NO normalization back — letting
// the saturation actually get loud and gritty is what gives Tone its
// audible character. tanh is the safety: bounds output at ±1 regardless
// of drive amount.
saturate    = *(drive) : ma.tanh;

// Tilt EQ: high-shelf at 3 kHz with one polarity + low-shelf at 200 Hz
// with opposite polarity. Net effect is a "tone tilt" — at low Tone
// the spectrum is dark+full-bass, at high Tone it's bright+tight-bass,
// with neutral flat at Tone=0.5.
toneEq      = fi.high_shelf(toneEqDb, 3000.0) : fi.low_shelf(-toneEqDb, 200.0);

// Pitch shifter (Faust delay-line transpose). Window 1024 samples,
// crossfade 10 samples — standard quality settings.
pitchShift  = ef.transpose(1024, 10, pitchSemi);

// ====== Per-channel processing ======
// Chain: pitch shift → ring mod (multiply by carrier) → saturate → tone EQ
chL(x)      = x : pitchShift : *(ringCarL) : saturate : toneEq;
chR(x)      = x : pitchShift : *(ringCarR) : saturate : toneEq;

// ====== Robotize as constant-power dry/wet macro ======
// At Robotize=0 → pure dry input (no effect at all).
// At Robotize=1 → pure wet ring-mod robot output.
// sin/cos crossfade maintains perceived loudness across the sweep (linear
// has an audible 3 dB dip around 0.5). Carrier freq still sweeps with
// Robotize too, so as you turn up you get MORE wet AND more aggressive
// carrier character — natural "robot intensity" macro.
wetGain     = sin(robotize * ma.PI / 2);
dryGain     = cos(robotize * ma.PI / 2);

process(l, r) = (l * dryGain + chL(l) * wetGain,
                 r * dryGain + chR(r) * wetGain);
