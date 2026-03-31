import("stdfaust.lib");

// Sienna - Warm Plate Reverb
// Smooth, dense reverb with tone control for vocal warmth

// Parameters
preDelay = hslider("Pre-Delay", 0, 0, 100, 0.1) : si.smoo;  // ms
decay = hslider("Decay", 1.5, 0.1, 10, 0.01) : si.smoo;     // seconds
tone = hslider("Tone", 6000, 2000, 12000, 1) : si.smoo;     // Hz (lowpass cutoff)
mix = hslider("Mix", 0.3, 0, 1, 0.01) : si.smoo;            // 0-1 (0-100%)

// Pre-delay stage
maxPreDelay = 96000;  // 1 second at 96kHz
preDelayLine = de.fdelay(maxPreDelay, preDelay * ma.SR / 1000);

// Dattorro reverb (plate-style, smooth and dense)
// Parameters: input diffusion 1, input diffusion 2, decay diffusion 1, decay diffusion 2,
//             decay, damping, input low cut, input high cut
reverbCore = re.dattorro_rev(
    0.75,           // input diffusion 1
    0.625,          // input diffusion 2
    0.7,            // decay diffusion 1
    0.5,            // decay diffusion 2
    decay,          // decay time
    0.0005,         // damping (minimal, we filter separately)
    0.0,            // no input low cut
    1.0             // no input high cut
);

// Tone filter (lowpass on reverb tail for warmth)
toneFilter = fi.lowpass(3, tone);

// Dry/wet mix (constant power)
dryWetMix(dry_l, dry_r, wet_l, wet_r) = 
    dry_l * sqrt(1 - mix) + wet_l * sqrt(mix),
    dry_r * sqrt(1 - mix) + wet_r * sqrt(mix);

// Main process
process = _,_ <: ((_,_ : preDelayLine, preDelayLine : reverbCore : toneFilter, toneFilter), _,_) : dryWetMix;