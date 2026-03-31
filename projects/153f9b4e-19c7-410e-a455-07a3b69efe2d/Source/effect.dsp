import("stdfaust.lib");

// Satin 2 - Bright plate reverb with tape saturation
// Parameters: Decay, Mix, Tone

// Parameters with smoothing
decay = hslider("Decay", 0.65, 0.1, 0.99, 0.01) : si.smoo;
mix = hslider("Mix", 0.35, 0, 1, 0.01) : si.smoo;
tone = hslider("Tone", 5000, 500, 8000, 1) : si.smoo;

// Dattorro plate reverb (stereo in, stereo out)
// Parameters: input_diffusion1, input_diffusion2, decay_diffusion1, decay_diffusion2, 
//             decay, damping
plateReverb = re.dattorro_rev(0.75, 0.625, 0.7, 0.5, decay, 0.0005);

// Tone control - lowpass filter on reverb output
toneFilter = fi.lowpass3e(tone);

// Tape saturation - subtle cubic nonlinearity for warmth
tapeSat = ef.cubicnl(0.3, 0);

// Signal chain per channel:
// Input → Dattorro reverb → Tone filter → Tape saturation
reverbChain = plateReverb : toneFilter, toneFilter : tapeSat, tapeSat;

// Dry/wet mix - manual constant-power crossfade
// wet = sin(mix * pi/2), dry = cos(mix * pi/2)
wetGain = sin(mix * ma.PI * 0.5);
dryGain = cos(mix * ma.PI * 0.5);

process = _,_ <: (_,_ * dryGain, dryGain), (_,_ : reverbChain * wetGain, wetGain) :> _,_;
