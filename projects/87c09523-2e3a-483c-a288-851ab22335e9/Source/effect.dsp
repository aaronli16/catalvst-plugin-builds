import("stdfaust.lib");

// Warm Plate Reverb
// Built on jpverb — Lexicon-inspired algorithmic reverb with chorus modulation
// Tuned for smooth, diffuse decay with warmth

decay    = hslider("Decay", 2.5, 0.1, 8.0, 0.1) : si.smoo;
damping  = hslider("Damping", 0.45, 0.0, 1.0, 0.01) : si.smoo;
warmth   = hslider("Warmth", 0.6, 0.0, 1.0, 0.01) : si.smoo;
mix      = hslider("Mix", 0.4, 0.0, 1.0, 0.01) : si.smoo;

// Fixed settings optimized for plate character
size        = 1.8;          // Large space for smooth diffusion
earlyDiff   = 0.75;         // Smooth early reflections
modDepth    = 0.08;         // Subtle chorus for shimmer
modRate     = 1.2;          // Gentle modulation rate

// Frequency response — warmth control attenuates highs for darker tail
lowMult  = 1.0;             // Full low-frequency decay
midMult  = 1.0;             // Full mid-frequency decay
highMult = 1.0 - (warmth * 0.6);  // Warmth knob darkens the tail
lowCut   = 400;             // Low/mid crossover
highCut  = 3500;            // Mid/high crossover

plateReverb = re.jpverb(decay, damping, size, earlyDiff, modDepth, modRate, 
                        lowMult, midMult, highMult, lowCut, highCut);

process = _,_ : ef.dryWetMixerConstantPower(mix, plateReverb);
