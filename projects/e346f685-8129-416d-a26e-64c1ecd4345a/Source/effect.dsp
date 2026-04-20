import("stdfaust.lib");

delayMs = hslider("Delay", 300, 10, 800, 1) : si.smoo;
feedback = hslider("Feedback", 0.55, 0, 0.9, 0.01) : si.smoo;
mix = hslider("Mix", 0.45, 0, 1, 0.01) : si.smoo;

maxS = int(1.0 * ma.SR);
delS = delayMs * ma.SR / 1000.0;

// Ping-pong: L feeds into R's delay and vice versa via cross-feedback
pingPong(l, r) = delL, delR
letrec {
  'delL = (l + delR * feedback) : de.fdelay(maxS, delS);
  'delR = (r + delL * feedback) : de.fdelay(maxS, delS);
};

process = _,_ : ef.dryWetMixerConstantPower(mix, pingPong);
