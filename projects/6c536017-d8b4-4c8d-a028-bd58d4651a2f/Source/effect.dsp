import("stdfaust.lib");

// Shimmer reverb — Dattorro plate with LFO-modulated allpass diffusion for shimmer

// Parameters
mix        = hslider("Mix", 0.35, 0.0, 1.0, 0.01) : si.smoo;
size       = hslider("Size", 0.7, 0.0, 1.0, 0.01) : si.smoo;
damping    = hslider("Damping", 0.005, 0.0, 0.999, 0.001) : si.smoo;
feedback   = hslider("Feedback", 0.7, 0.0, 0.95, 0.001) : si.smoo;
delayMs    = hslider("Delay", 20.0, 0.0, 100.0, 0.1) : si.smoo;
diffusion  = hslider("Diffusion", 0.75, 0.0, 1.0, 0.01) : si.smoo;
modDepth   = hslider("Mod Depth", 0.5, 0.0, 1.0, 0.01) : si.smoo;
modRate    = hslider("Mod Rate", 0.5, 0.01, 5.0, 0.01) : si.smoo;

// Pre-delay in samples (compile-time bounded for Dattorro)
preDelSamples = delayMs * ma.SR / 1000.0 : int : max(0) : min(int(0.2 * 96000.0));

// Bandwidth derived from size (larger spaces have darker input)
bandwidth = 0.5 + size * 0.5;

// Dattorro plate with standard diffusion coefficients
// Input diffusion scaled by diffusion parameter
i_diff1 = 0.625 * diffusion;
i_diff2 = 0.75 * diffusion;

plate = re.dattorro_rev(preDelSamples, bandwidth, i_diff1, i_diff2, feedback, 0.7, 0.5, damping);

// Shimmer modulation — modulated allpass after the plate
// LFO generates quadrature signals for stereo width
lfoL = os.oscrs(modRate);
lfoR = os.oscrc(modRate);

// Modulated allpass parameters
maxDel = 1024;
baseDelay = 20.0;  // samples
sweepRange = 10.0; // samples

// Map LFO to delay modulation
delL = baseDelay + sweepRange * modDepth * (1 + lfoL) / 2;
delR = baseDelay + sweepRange * modDepth * (1 + lfoR) / 2;

// Allpass with modulated delay for shimmer
modAllpassL = fi.tf2(0, delL / ma.SR, 1, 0, -(delL / ma.SR));
modAllpassR = fi.tf2(0, delR / ma.SR, 1, 0, -(delR / ma.SR));

shimmerL(x) = x : de.fdelay(maxDel, delL) : *(0.7) : + ~ *(0.5);
shimmerR(x) = x : de.fdelay(maxDel, delR) : *(0.7) : + ~ *(0.5);

shimmer(l, r) = shimmerL(l), shimmerR(r);

// Chain: plate reverb → shimmer modulation
wetChain = plate : shimmer;

process = _,_ : ef.dryWetMixerConstantPower(mix, wetChain);
