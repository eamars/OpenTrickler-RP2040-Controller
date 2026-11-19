# ADR-009: progress

The ADR is a Draft, not approved. What is recorded here is research the owner asked for ahead of its design.

## 2026-11-19: research, no unit

Asked by the owner while ADR-008 was being built by another session: physics, modelling and control for a charge with the low-flow small tube and the standard large tube, AR2209 and the A&D; not firmware code. The probe was in use, so nothing was run on a unit. Branch `adr009_research`, local, not pushed.

**The owner's figures and choices (2026-11-19)**
- Today about 1 overcharge in 10 to 15 throws. Aim: about 10 s a throw, 15 s accepted; 1 in 50 "a bonus".
- The design target for AR2209 at 42.5 gr was 7 to 15 s "depending on the cluster"; "my AR2209 tends to have more cluster than other powder".
- The fine tube is the low-flow one. A stored gear ratio of 40:19 is from generation 1; generation 2 has more teeth (22).
- A finely tuned lip is hard to print and not preferred: "I'd prefer using high frequency oscillation if cluster is the real limitation."
- PR #113 may be read for its data.
- A second goal: automatic tuning for a community whose printed tubes differ, in two modes, autonomous and assisted by an external AI agent through the API.
- The design is left to the agent; the measurements are to be written down for whoever runs them (`research/bench_guide.md`).

**What was made**
- **The tubes measured from their STLs** (`tools/charge_sim/geometry/`): large, bore 13.8 to 15.0 mm, 5-start rifling of 20 mm lead and 0.59 mm depth, 32 teeth, open rear; small low-flow, bore 6.55 to 7.5 mm with a 5.7 mm nozzle, 5-start rifling of 11 mm lead and 0.47 mm depth, 22 teeth, rear capped, one inlet window of about 3 mm by 55°. Centrifuging at 5.8 and 8.2 trickler rev/s.
- **Four desk studies** (`research/sources/`): powders, flow through a rotating tube, dosing control, the scales' timing. One agent was stopped by a usage-policy safeguard; the owner had it retried on another model and narrowed to flow dynamics, and it finished.
- **A simulation of a throw on the PC** (`tools/charge_sim/`, pure Python): the plant, four scale models, today's loop ported from `charge_mode.cpp`, a candidate controller, its calibration, and the comparisons.

**What was found**
- **The simulation's fine tube was first assumed too weak, and one real figure corrected it.** PR #113 measured on a unit that a fine Kp of 6 leaves +0.156 gr after the stop and a Kp of 3 leaves 0 to +0.028 gr (N135, an A&D). The first plant gave +0.02 gr at Kp 6. With the fine tube's flow tripled it gives +0.19 and +0.04 (`pr113_check.py`), and a 42.5 gr throw in 9.4 s, inside the owner's 7 to 15 s. That plant is now the reference. It gives today's loop 25% over, above the owner's 7 to 10%, so the truth lies between the two plants; the bench decides.
- **Two causes of an overcharge, both real in the reference plant:** feed into the time the scale has not yet shown (the controller's to cure), and kernels leaving together (not). 400 throws each at 42.5 gr, true weight against the unit's verdict rule:

| Plant | Today's loop: over, under, time | Candidate: over, under, time |
|---|---|---|
| Reference (clumps of 1.3, irregular) | 25%, 2%, 9.4 s | 14%, 0%, 9.8 s |
| Single kernels, regular | 15%, 0%, 9.5 s | 0%, 0%, 9.6 s |
| Single kernels, irregular | 17%, 0%, 9.5 s | 2.5%, 0%, 10.1 s |
| Clumps of 2 | 40%, 8%, 9.4 s | 30%, 0%, 8.4 s |
| Fine tube flow a third (the first priors) | 15%, 12%, 18.7 s | 16%, 0%, 12.1 s |
| Fine tube flow doubled | 55%, 3%, 6.0 s | 16%, 0%, 9.2 s |
| Draughts, 6 a minute | 28%, 8%, 9.4 s | 10%, 10%, 9.3 s |
| Print A: both tubes x0.6 | 22%, 6%, 10.8 s | 14%, 0%, 10.6 s |
| Print B: coarse x1.5, fine x0.6, rough | 28%, 6%, 9.0 s | 16%, 0%, 7.6 s |
| Print C: fine inlet x0.4 | 20%, 6%, 15.6 s | 12%, 0%, 10.5 s |
| A&D at SLOW (5 readings a second) | 93%, 0%, 8.9 s | 14%, 0%, 11.3 s |
| G&G JJ-B, polled | 100%, 0%, 8.1 s | 15%, 1%, 32.6 s |
| Strain gauge, 2 readings a second | 100%, 0%, 8.1 s | 14%, 3%, 30.6 s |
| Long stick, 0.04 gr a kernel | 49%, 14%, 13.5 s | 55%, 0%, 9.5 s |

- **Read:** the candidate holds 10 to 16% wherever the clumps are as assumed, whatever the print, the fine tube's flow or the scale, and reaches 0 to 2.5% when kernels leave singly. Today's loop moves from 15% to 100% with the same changes. The last digit is noise (two runs of the same row differed by 2.5 points).
- **Two loops on one weight** (the owner's question): sharing the feed three ways made no difference that 800 throws could show: both tubes by their own law 13.9% over and 11.8 s, the fine tube waiting for the coarse 15.5% and 12.0 s, one demand split between them 14.4% and 12.9 s (first plant). The fault of today's arrangement is elsewhere (README, "Decisions proposed").
- **Tried and dropped:** counting powder pushed to the lip and not yet seen as delivered, with a reverse turn when too much hangs: 14% over against 12% without.
- **The measurement's lag is in the model** as separate parts (dead time, sensor, a two-speed filter, the report's interval, the link). `scale_step.py` prints what they add up to: on the A&D at FAST one kernel shows after 0.25 s; a grain is half shown at 0.21 s and within a digit at 0.5 s. All from estimates: no maker and no reloader publishes it.
- **PR #113 holds no flow figures,** only gains and the fall after the stop. Its other real figures: coarse time 4.4 to 8.4 s in one N135 series; the D term worth under 1% of the fine speed at Kd 4; a stop threshold on an exact multiple of the scale's digit is decided by float rounding.
- **A matrix run lost a worker once** (`BrokenProcessPool` at the G&G row); the same throws ran clean in one process and by `mc.py`. Not chased.

**Not done**
- Nothing measured on a unit. `research/bench_guide.md` is the next step.
- Oscillation is not in the simulation: there is nothing to model it from until the bench's step 7.
- The automatic tuning is designed (README) and its measuring half simulated (`calibrate.py`); its verifying and trimming half is not.
- numpy and matplotlib were installed into the venv and not used; they are not in `requirements.txt`.
