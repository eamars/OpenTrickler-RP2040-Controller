# ADR-009 step 2: the use cases against the simulation

The owner (2026-11-26, 10:58): "I want you to also try more experiments, for failure modes, or out of ordinary exceptions, like: 1. Starting with the absurdly wrong PID 2. Someone accidentally put integral term 3. Different powder? I want you to map all use cases against your simulation, out of the known experiment so far, then conduct the experiment with your simulation."

Step 2 as it ships at the interim decision (the README): the forecast seeded by step 1, the throw never cut on it, the tuner on kp and kd by the share modelled. Every case below was run on it on the twin (`tools/charge_sim/step2_cases.py`, 2026-11-26, 11:10; two sessions a case, the tuner's cases 300 throws a session with the last 100 judged, the fixed ones 150 judged whole; the A&D's digit 0.02 gr, tolerance 0.03 unless the case changes it). The rig is the twin's f2 (nearest the bench's unit) unless named; the start the bench's stored gains (fine kp 2, kd 10, top 5 rev/s) unless "owner's" (fine kp 0.8, kd 15, top 2 rev/s). A variant marked *assumed* is an assumption put on a rig (`rigs.VARIANTS`), not a measurement.

**The reference:** the bench's gains, the tuner on: SD 0.062, mean +0.047, 5% to 95% −0.01 to +0.17, worst +0.40, no under, 6.6 s (95% 9.3 s). With no tuner: SD 0.078, +0.064, 4.9 s.

## The normal path

| Case | Before this run | Now: SD, mean, worst, time | Verdict |
|---|---|---|---|
| A calibrated powder, the stored gains | the twin's four rigs; the bench's 49 throws (on the forecast's cut) | the reference above | **Works** |
| The owner's hand-tuned gains, the tuner on | the twin | 0.051, +0.031, +0.49, 12.3 s (95% 23.9 s) | **Works**, a little wider than the tuner off |
| The same, the tuner off | the twin | 0.035, +0.029, +0.26, 11.3 s | **Works** (the narrowest) |
| 3 gr, under the coarse tube's threshold (the fine tube alone) | none | 0.047, +0.038, +0.27, 6.0 s | **Works** |
| 30 gr | before the seed only | 0.048, +0.033, +0.47, 10.7 s | **Works** |
| 60 gr | none | 0.052, +0.037, +0.31, 12.5 s | **Works** |
| Ball powder (BM2) | the twin | 0.100, +0.058, +0.61, 9.4 s | **Works**, wider in these two sessions (0.047 over four before) |
| Big kernels, three times as heavy (*assumed* powder) | none | the tuner 0.184, +0.153, +1.26, 9.8 s; from the owner's gains 0.120, +0.090; no tuner 0.243, +0.225 | **Degraded:** lumps land after the stop. The tuner narrows it a little; the kernel sets the floor (the owner: "some larger kernel powder is hard to dispense precisely") |
| The user's tolerance 0.02 | none | the reference's throws (the tuner took the same steps) | **Works** |
| The user's tolerance 0.1 | none | 0.082, +0.063, 6.2 s, 84% within it | **Works:** the tuner takes quicker gains, as the tolerance allows |
| A scale's digit as fine as a gram scale's (0.0154 gr, the weights still in grains) | none | 0.052, +0.044, +0.36, 7.0 s | **Works** |
| A scale in grams with the profiles' defaults in grains (rigs.py's `grams`: every weight / 15.4324, the digit 0.001 g; 10 gr as 0.648 g) | none | the defaults as they are, no tuner: SD 0.003 g (0.05 gr) in **49 s** a throw: the fine gains 15 times too weak, the coarse threshold of 5 now 5 g, past the whole charge, so the coarse tube never runs; the tolerance of 0.03 now 0.03 g (0.46 gr), fifteen times looser. Gram thresholds, grain gains: 42.5 s; the tuner on, 15.3 s after 300 throws. **Since `8806f78`** (the gains put right): 6.9 s from the first trial on, SD 0.008 g (0.12 gr) against 0.004 g with every setting converted | **Improved:** the tuner brings the fine gains to grams in one window. The coarse gains and the thresholds stay in grains |
| Step 1 on a scale in grams | none | the method's records in grams: least speeds 0.02 and 0.16 rev/s as in grains, lags 0.58 to 1.01 s (grains 0.48 to 1.06), weight a turn 0.052 g (0.807 gr / 15.43) | **Works**, the method. Five constants around it are in grains (below) |

## Failure modes and exceptions

| Case | Before this run | Now: SD, mean, worst, time | Verdict |
|---|---|---|---|
| kp absurdly low (0.02) | kp / 4 at gate 1 (the forecast's cut) | 0.042, +0.035, 17.4 s; the tuner's steady gain only doubled in 600 throws. **Since `8806f78`**, four sessions: 0.064, +0.048, 7.0 s (the reference 0.057) | **Fixed:** the gains put right in the first window |
| kp absurdly high (200) | kp x 4 at gate 1 | the tuner 0.172, +0.211, +1.24, 2.8 s; no tuner 0.118, +0.118. **Since `8806f78`**, four sessions: 0.138, +0.100, worst +1.82 | **Improved, safely:** the gains put right; still wider than the reference |
| kd 0 | none | 0.052, +0.040, 7.6 s | **Works** (the stored kd does almost nothing anyway) |
| kd absurdly high (100,000) | none | 0.047, +0.039, 14.3 s | **Safe but slow:** braking hard; the tuner raised kp to 3.7 to 5.5 against it |
| An integral term by mistake: ki 0.01, 0.1, 1 | none | the tuner 0.132 / 0.082 / 0.169, mean +0.076 / +0.071 / +0.195, worst +1.47 / +0.51 / +0.63; ki 0.1 with no tuner 0.114, +0.145. **Since `b62eb2c`** (the tuner's first trial without ki), four sessions: 0.092 / 0.089 / 0.084, mean +0.069 / +0.067 / +0.062, ki dropped in 2, 3 and 4 sessions of 4 | **Improved:** an absurd ki is taken out; a small one does little harm and is sometimes kept |
| Another powder's calibration (the seed half this one's) | none | 0.047, +0.038, 7.9 s | **Works:** the throws' own replace it |
| The same, twice this one's | none | 0.102, +0.064, +0.92, 6.6 s | **Works, wider** |
| A powder never calibrated (no seed) | before the never-cut stop | its first 50 throws: 0.275, **+0.871** (+0.45 to +1.23), 2.7 s, every one over; the last 100 of 300: 0.054, +0.044 | **Starts as the old build, then recovers:** the tuner's first trial slows the gains, the slower throws teach their own weight a turn |
| The same from the owner's gains | none | the last 100 of 300: 0.056, +0.043 | **Works** |
| The fine tube's hopper empty | none | every throw ran to the twin's limit of 90 s. **Since `f34ab17`** (the starved stop): every throw stops in 5.8 to 6.7 s, and the charge ends on the fault "Fine: no powder" | **Fixed** (it failed: the throw never ended, as in v2.0.3) |
| The coarse tube's hopper empty | none | 0.054, +0.033, 7.1 s | **Works:** the fine tube pours it all |
| Draught on the pan after the calibration (*assumed*: six gusts a minute, 0.03 gr) | none | 0.082, +0.058, +0.59, 7.1 s | **Degraded a little** |
| One report in twenty lost (*assumed*) | the charge mode's pauses on the bench (ADR-013), not the controller | 0.040, +0.036, 7.2 s | **Works** |
| A wild coarse tube (coarse kp ten times the bench's) | none | every throw +1.64 to +2.31, 1.4 s | **Fails:** the coarse tube's powder lands past the target; the fine tube cannot take it back, and the tuner leaves the coarse gains alone |
| The same, its two knobs (no tuner; `step2_coarse_study.py`, 2 x 40 at 10 and 30 gr, `step2_twin_coarse_f2.png`) | none | at 10 gr: the threshold 5 to 7, +0.078, SD 0.077, 4.0 s; the coarse kp to the bench's 0.02, +0.062, 0.100, 5.0 s. At **30 gr**: the threshold 7, **+12.8**; 9, +9.6; the coarse kp 0.02, +0.099, 0.086, 6.6 s | **The PID, not the threshold** (corrected, 2026-11-26): more room covers a wild coarse tube on a small charge only; what it has in the air at its stop grows with the charge |
| The night rig (the most coarse powder in the air) | the twin | 0.131, +0.088, +0.81, 7.0 s | **Degraded** (known) |

**Not simulated:** a restart in a session (the carry and the tuner begin afresh in RAM by design; the seed and the stored gains carry over); a cold tube pouring short at first, a lot change or drift within a session (the twin has no hook for a rig that changes mid-session; a wrong seed is its start); a bump on the scale large enough to read past the target (the draught's gusts are its small form; a reading past the target stops the throw short); the charge mode's own paths (the zero, the cup, a lost scale, a driver fault: on the bench, ADR-013 and gate 3).

## What it shows

- **The normal path holds** across charges of 3 to 60 gr, ball and stick powder, a scale in grams and tolerances of 0.02 to 0.1: no under anywhere, the SD near 0.05.
- **Every failure fails towards over,** as the never-cut stop was built to make it: no case threw under.
- **A lumpy powder is the largest degradation the normal path meets** (SD 0.18 with the tuner), as the owner expected.
- **Four failures needed more than the tuner.** Taken in turn on 2026-11-26 (the owner, 11:20: the failures first while the bench is out of reach, an exception to the order, not a rule):
  1. **An empty hopper never ended a throw: fixed** (`f34ab17`). Once the coarse tube has stopped and a weight a turn is known, each 5 of the fine tube's turns shown must raise the reading by a tenth of what they carry, or the throw stops starved and the charge ends on "Fine: no powder", waiting to be acknowledged. No false stop in 9,000 tuned throws on five rigs (big kernels and draught among them); at 3 turns and a quarter, a carry run high tripped big kernels 9 times. A powder never calibrated has no weight a turn on its first throws, and the check waits for one.
  2. **An integral term only adds overshoot: improved** (`b62eb2c`). The tuner's first trial is the gains without it, kept only if cheaper.
  3. **A wild coarse tube overshoots every throw: the owner's** (the coarse tube's gains are not the tuner's; today's loop has the same exposure). Options: (a) leave it, the user's coarse gains as today; (b) the coarse tube's speed capped so its powder in the air at its stop cannot pass the target, a forecast on the coarse tube, which the owner's direction of 2026-11-25, 18:05 keeps to the fine tube; (c) the tuner also trying the coarse kp when throws land over by more than the fine tube's part. Mine: (a), the goal being a little better than today's PID.
  4. **An absurdly high kp** pins the fine tube to its envelope, where the tuner cannot see a difference between its trials: **left** (it fails safely, some 0.2 gr over). A bound on the gains it starts from would bring it in range; not built.
