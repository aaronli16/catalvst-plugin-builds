import("stdfaust.lib");

// ============================================================================
// Reflex - Tempo-Synced Stereo Delay
// ============================================================================
// Clean digital delay with tempo sync
// Time: Note division selector (0=1/4, 1=1/8, 2=1/16, 3=1/32, 4=1/8T, 5=1/16T)
// Feedback: 0-95% (capped for stability)
// BPM from DAW host
// ============================================================================

// Parameters
timeSel = hslider("Time", 2, 0, 6, 1) : si.smoo;  // Note division selector
feedback = hslider("Feedback", 0.4, 0, 0.95, 0.01) : si.smoo;
bpm = hslider("BPM", 120, 30, 300, 0.1) : si.smoo;  // BPM from DAW

// Note division calculator
// Returns delay time in milliseconds for the selected division
noteDivMs(sel) = ba.selectn(7, sel, (
    120000 / bpm,          // 0: Half note (1/2)
    60000 / bpm,           // 1: Quarter note (1/4)
    30000 / bpm,           // 2: Eighth note (1/8)
    15000 / bpm,           // 3: Sixteenth note (1/16)
    7500 / bpm,            // 4: Thirty-second note (1/32)
    20000 / bpm,           // 5: Eighth triplet (1/8T = 1/8 * 2/3)
    10000 / bpm            // 6: Sixteenth triplet (1/16T = 1/16 * 2/3)
));

// Convert milliseconds to samples
delaySamples = noteDivMs(timeSel) * ma.SR / 1000;

// Maximum delay buffer size (4 seconds for slow tempos)
maxDelay = 192000;

// Mono delay line with feedback
monoDelay = +~(de.fdelay(maxDelay, delaySamples) * feedback);

// Stereo processing: apply same delay to both channels
process = monoDelay, monoDelay;