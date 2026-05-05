declare name "X Reverb";
declare author "Catalvst";
declare description "Cosmic shimmer reverb — dual pitch shifters in feedback loop.";

import("stdfaust.lib");

// ====== Knob parameters (label = data-param in HTML, EXACT match) ======
shimmer   = hslider("Shimmer",   0.35, 0, 1, 0.01) : si.smoo;
size      = hslider("Size",      0.50, 0, 1, 0.01) : si.smoo;
pitch     = hslider("Pitch",     0.60, 0, 1, 0.01) : si.smoo;
damping   = hslider("Damping",   0.35, 0, 1, 0.01) : si.smoo;
mix       = hslider("Mix",       0.35, 0, 1, 0.01) : si.smoo;
preDelay  = hslider("Pre-Delay", 0.10, 0, 1, 0.01) : si.smoo;
diffusion = hslider("Diffusion", 0.55, 0, 1, 0.01) : si.smoo;
width     = hslider("Width",     0.85, 0, 1, 0.01) : si.smoo;

// ====== Mappings (0–1 → real DSP units) ======
fbGain   = shimmer * 0.85;                // 0–0.85 (capped to prevent runaway)
t60Mid   = 1.0 + size * 19.0;             // 1–20 s
t60Low   = t60Mid * 0.6;                  // low band shorter than mid
fifthMix = pitch;                         // 0 = octave only, 1 = full octave + fifth
dampHz   = 200.0 * pow(90.0, damping);    // ~200 Hz – 18 kHz, log
preMs    = preDelay * 250.0;              // 0–250 ms
diffFb   = 0.5 + diffusion * 0.35;        // 0.5–0.85 allpass feedback
widthAmt = width;                         // 0 = mono, 1 = full stereo

// ====== Modulation (fixed cosmic values — not user-exposed) ======
// Slow LFO at 0.5 Hz, depth 0.3. Sin/cos decorrelates L/R for stereo movement.
LFO_RATE  = 0.5;
LFO_DEPTH = 0.3;
modL = (os.oscsin(LFO_RATE) * LFO_DEPTH) * 0.5 + 1.0;  // ~0.85..1.15
modR = (os.osccos(LFO_RATE) * LFO_DEPTH) * 0.5 + 1.0;

// ====== Reverb tank ======
SR_MAX = 96000;
verb = re.zita_rev1_stereo(preMs, 200.0, dampHz, t60Low, t60Mid, SR_MAX);

// ====== Input conditioning ======
inFilt = fi.highpass(2, 80.0) : fi.lowpass(2, 18000.0);

// ====== Diffuser network ======
// Short allpass delays (7–23 ms) for tight, non-smearing diffusion typical of
// shimmer pedals (vs. plate reverbs which use longer 30–80 ms allpasses).
diffuser = fi.allpass_comb(2048, 0.0067 * ma.SR, diffFb)
         : fi.allpass_comb(2048, 0.0103 * ma.SR, diffFb)
         : fi.allpass_comb(2048, 0.0157 * ma.SR, diffFb)
         : fi.allpass_comb(2048, 0.0231 * ma.SR, diffFb);

// ====== Pitch-shifted feedback loop ======
// Pitch shifters live INSIDE the reverb's feedback loop. Each pass through
// the tank gets shifted again — that's what produces the cascading bloom.
//
// Window 4096 / xfade 2048: slower crossfade rate (~10 Hz vs 21 Hz at 2048/1024)
// = less audible warble at the cost of slight transient smearing. For a
// sustained-source plugin like shimmer, this is a clear win on harshness.
W = 4096;
X = 2048;
shifter(s) = ef.transpose(W, X, s);

// Aggressive feedback-path lowpassing is THE secret to silky shimmer.
// Without these, every loop iteration moves your highs up an octave — 4kHz
// becomes 8kHz becomes 16kHz becomes pain. Pre-LP keeps already-bright content
// out of the shifters; post-LP catches what they brightened.
preShiftLP  = fi.lowpass(2, 5000.0);
postShiftLP = fi.lowpass(2, 5000.0);

// Soft tanh saturation in the feedback loop replaces banned co.limiter_lad_stereo.
// Acts as a soft ceiling — without it, any meaningful Shimmer value runs away.

// Channel-specific feedback paths. +12 octave is unconditional; +7 fifth is
// gated by the Pitch knob. Sin/cos LFOs decorrelate L/R for stereo movement.
shimmerL(x) = (x : preShiftLP : shifter(12))
            + (x : preShiftLP : shifter(7) : *(fifthMix))
            : postShiftLP      // catch any harshness the shifters added
            : fi.dcblocker     // pitch shifters generate small DC; block it
            : ma.tanh          // soft ceiling so loop never explodes
            : *(modL)          // slow LFO mod for cosmic drift
            : *(fbGain);       // overall feedback amount (Shimmer)
shimmerR(x) = (x : preShiftLP : shifter(12))
            + (x : preShiftLP : shifter(7) : *(fifthMix))
            : postShiftLP
            : fi.dcblocker
            : ma.tanh
            : *(modR)
            : *(fbGain);

shimmerStereo = shimmerL, shimmerR;

// ====== Tank with feedback ======
// `body ~ feedback` wires feedback's outputs back into body's inputs.
// Body: 4-in 2-out  (sums external L,R with feedback L,R via `+,+`)
// Feedback: 2-in 2-out (shimmerStereo)
tank = ((+ , + : par(i, 2, inFilt) : par(i, 2, diffuser) : verb)) ~ shimmerStereo;

// ====== Stereo width on wet output (mid/side blend) ======
// width=0 → mono (L=R=M); width=1 → full stereo. Dry stays untouched.
applyWidth(l, r) = m + s * widthAmt, m - s * widthAmt
  with {
    m = (l + r) * 0.5;
    s = (l - r) * 0.5;
  };

// Final wet-bus high-cut at 10 kHz: insurance against any remaining sparkle
// that snuck past the in-loop lowpasses. Dry path is untouched.
wetHighCut = par(i, 2, fi.lowpass(2, 10000.0));

tankWidth = tank : applyWidth : wetHighCut;

// ====== Process chain ======
// dryWetMixerConstantPower(wetAmount, FX): splits the input internally,
// runs one copy through FX, mixes via constant-power crossfade.
process = _, _ : ef.dryWetMixerConstantPower(mix, tankWidth);