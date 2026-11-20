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
- Backward compatibility is not weighed in the initial research; a later step, "what to keep", costs the move for the 600+ users (README, "Owner's direction").
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

## 2026-11-19, 23:28 to 23:51: the first bench session

The owner: "The bench is ready now, Pico 2 W on the probe, powder loaded"; this AR2209 "has been exposed in the air for a long time so its property might look different". Pico 2 W, 2.0.3-84 (eaea601, Debug), the A&D FX-300i, the large tube and the low-flow small tube. Open loop through the console (`tools/charge_sim/bench/record.py`); raw frames and step counts in `logs/adr009/` of the main checkout, each run's line in `research/bench_2026-11-19/`. Every session ended `VERDICT PASS`. 193 gr poured into the cup, which now needs emptying.

**Set on the unit, with the owner's word:** the fine motor's gear ratio 2.105263 (generation 1) to 1.818182 (40:22), stored. The settings before are in `logs/adr009/settings_before`. The step counts had confirmed it: 0.5 rev/s asked gave 0.432.

**What was measured**

| Thing | Measured | The simulation had assumed |
|---|---|---|
| A&D frames | 20.7 a second; `US` while the weight moves, `ST` at rest | 20.9 |
| Fine tube, powder per turn | 0.71 to 0.81 gr at every speed from 0.05 to 5 rev/s; 0.70 to 0.86 in nudges | 1.35 (reference), 0.45 (first) |
| Fine tube at 5 rev/s | 3.8 gr/s; at 3.5 rev/s a steady 3.0 gr/s through 8 s | a ceiling of 2.4 gr/s from the inlet window |
| Coarse tube, powder per turn | 8.0 to 10.1 gr from 0.1 to 3 rev/s; 23.6 gr/s at 3 rev/s | 12 |
| Shown only after a stop, as seconds of the flow | 0.5 to 0.65 s, both tubes, 0.4 to 24 gr/s | 0.25 |
| The reading after a stop | within a digit of its end by 0.8 to 1.0 s | 0.5 s |
| One kernel after a small turn: the reading first moves | median 1.24 s after the turn began (a tenth by 0.7 s, nine tenths by 2.0 s); still by 1.55 s (nine tenths by 2.6 s) | 0.25 s |
| A kernel | gains cluster at 0.020 to 0.025 gr | 0.022 |
| Not primed | the fine tube's first two 2 s runs gave nothing, the next three 0.10, 0.10, 0.26 gr | |

**The nudge test** (fine tube, `motor fine <speed>`, a short pause, `motor fine 0`, then still):

| Run | Turn | Still | Nudges | Nothing | 1 | 2 | 3 or more kernels | Largest |
|---|---|---|---|---|---|---|---|---|
| A | 0.030 (11°) | 1.3 s | 100 | 43 | 36 | 9 | 12 | 6 |
| B | 0.011 (4°) | 1.2 s | 150 | 116 | 17 | 12 | 5 | 4 |
| C | 0.011 (4°) | 3.0 s | 110 | 84 | 15 | 2 | 9 | 8 |

- **A and B's pauses were too short for this scale:** a kernel can take over 2 s to show, so some were counted to the next nudge. C is the one to read.
- **In C, of 26 nudges that released anything, 9 released three kernels or more (35%), 5 released five or more.** A 4° turn let go of up to 8 kernels, 0.17 gr. Long runs of nothing come before the large falls.
- **Read against the bench guide's rule:** clumps are the limit for this batch and this tube. A finish by nudges alone would overcharge far more often than 1 in 50, whatever the controller before it.

**What it changes**
- **The fine tube's flow follows its speed.** No inlet ceiling, no sag over 8 s, no fall toward 5 rev/s: the held-powder state and the ceiling can leave the model, and the owner's "speed does not correlate with charge speed" did not show in these runs (not looked for: the coarse tube above 3 rev/s, a hopper nearly empty, a long run at 5 rev/s).
- **The scale is slower to show a kernel than to show a grain,** as a filter with two speeds would be. Whether that second or more is the scale or the kernel falling late after the tube is released (`motor <tube> 0` switches the driver off) is not separated: the chain test of the guide's step 1 would.
- **A finish that waits for the scale after each nudge costs 2 to 3 s a nudge here,** which does not fit a 7 to 15 s throw. Today's loop is quick because it does not wait; its stop threshold is a guess at what is still to show.
- **Oscillation (D4) is now the first experiment,** not a later one: the question is whether a swing or a flick breaks a clump before it falls.

**Found in passing**
- **The scale task's stack:** `stats` gave `stack_min="Scale Task":160` at the session's start and `:48` at its end, after about 20,000 tapped frames. The tap's line is built on that task's stack. Tap recordings were stopped there. For ADR-008's or ADR-013's owner to look at; not changed.
- `bad_frames=6` from the start of the session to its end: none added by it.
- The unit's selected profile is "AR2208,gr"; no charge was run.

## 2026-11-19 23:53 to 2026-11-20 00:13: the flick test, console only

Chosen by the owner over a firmware command ("Flick test, console only"), after emptying the cup. `record.py flick`: the fine tube through a list of speeds one after another, then 2.6 s still; weights by `state`, six readings averaged, no tap. A swing made from the PC is about 45 ms a move, so 4 to 5 Hz: far below the 20 to 50 Hz the research pointed at.

| Kind | Moves, rev/s | Trials | Kernels a trial | Trials that released | one | two | three or more | Largest |
|---|---|---|---|---|---|---|---|---|
| Plain nudge (run C above, with the tap) | 0.2 | 110 | 0.57 | 26 | 15 | 2 | 9 | 8 |
| Plain nudge, turn about with the two below | 0.2 | 40 | 0.82 | 13 | 4 | 3 | 6 | 5 |
| Small shake, turn about | 0.2, -0.2, 0.2, -0.2, 0.2 | 40 | 0.23 | 6 | 4 | 1 | 1 | 3 |
| Wide shake, turn about | 0.2, -0.6, 0.6, -0.6, 0.6 | 40 | 0.25 | 7 | 5 | 1 | 1 | 3 |
| Small shake, alone | as above | 70 | 0.30 | 12 | 7 | 3 | 2 | 5 |
| Wide shake, alone | as above | 70 | 0.31 | 9 | 2 | 3 | 4 | 4 |

- **Three kernels or more, of the trials that released:** plain 15 of 39 (38%), small shake 3 of 18 (17%), wide shake 5 of 16 (31%).
- **Verdict: no clear winner.** The small shake's share is half the plain nudge's, but on 18 releases that is within chance (about 1 in 8 by Fisher's test), and the wide shake shows nothing. A slow shake from the console does not settle whether oscillation breaks clumps.
- **A shaken trial moves about half the powder of a plain one** (0.3 against 0.6 to 0.8 kernels), so its back-and-forth is not one clean forward move: either the timing from the PC is uneven or a backward move takes powder back from the lip. Turn about, the plain nudges released more than alone (0.82 against 0.57): they may have collected what the shakes left.
- **What it says for the next step:** the small shake is worth carrying into a real test, at 20 to 50 Hz and with an exact net turn, which needs the `wiggle` command (bench guide, step 7) and several hundred trials a kind. The trial count matters more than the kinds: a release comes in one trial of four, so 100 trials give about 25 releases.
- 1.62 gr in the cup at the end. Every session `VERDICT PASS`. Lines in `logs/adr009/flick_*.txt` of the main checkout; `bench/flick_summary.py` sums them.

## 2026-11-20: a second agent's review of the bench's recordings, no unit

Asked by the owner of the agent taking over: an independent review first. Made from the raw frames in `logs/adr009/` of the main checkout, with `tools/charge_sim/bench/steady_release.py` (new). No contact with the unit.

**What the recordings say that the first reading missed**

| Recording | Kernels a second | Variance over mean of the kernels in a window (1 s, 2 s, 4 s) | Largest rise within 0.3 s |
|---|---|---|---|
| Fine tube, steady 0.05 rev/s, 38 s | 1.8 | 1.4, 1.1, 0.65 | 3.8 kernels |
| Steady 0.1 rev/s, 38 s | 3.6 | 1.0, 0.72, 1.1 | 5.7 |
| Steady 0.2 rev/s, 20 s | 6.8 | 0.83, 0.66, 0.66 | 4.8 |
| Nudge run C (4° turns, 3 s still, the driver switched off between) | | 4.9, from each nudge's gain | 7.6 in one nudge |

- **Turning steadily, this powder leaves the fine tube a kernel at a time.** Variance over mean is 1 when kernels leave singly and at random; a release of several at once raises it to the size of the release. The steady runs give 0.65 to 1.4; the nudges give 4.9. Both pour the same powder a turn (about 7 kernels a fifth of a turn).
- **So the clumps of the nudge test are made by nudging,** not carried by the powder: a tube that stops lets powder build at its lip and fall together at a later move. Each console nudge also switches the driver on and off (`remote_control.c`: `motor <tube> 0` calls `motor_enable(false)`), which a charge never does between its start and its end (`charge_mode.cpp`, lines 833 and 878).
- **"Clumps at the lip are the limit; no stopping rule reaches 1 in 50" does not follow.** It holds for a finish by nudges (D2), which is the research's own candidate, not for a finish that keeps turning. Today's loop finishes by a steady slow turn; that part of it is right.
- **Oscillation has no problem to answer yet.** It was to break clumps; a steady turn shows none. No research build and no 300-trial series until a steady finish is shown to fail.
- **The scale after a stop from a slow turn is still within about 1 s** (three stops, at 0.05, 0.1 and 0.2 rev/s: the last change at 0.8, 1.2 and 0.6 s). The 1.2 to 2.6 s of the nudge test is one kernel alone after a tube released: releases of three kernels or more first showed at 0.7 to 1.3 s, single ones at 0.3 to 2.5 s, six of eleven between 1.8 and 2.2 s. Scale or late kernel is still not separated, and matters less: a steady finish does not wait for single kernels.
- **Not measured at all, and the one figure a finish needs:** how much the reading rises after a stop from a slow steady turn, and how much that varies. Three samples exist: +4.8, +3.8 and +2.9 kernels, from 0.05, 0.1 and 0.2 rev/s, each with the driver switched off at the stop. Their spread decides the over-rate of any finish that stops once; their mean is only an offset.

**On the approach, for the record**
- The bench work is sound and was kept honest: every run's line, the raw frames, what was not separated. The flow per turn, the gear ratio, the scale's lag after a flow and the kernel's mass stand.
- The order was the wrong way round: a simulation of 2,000 lines and four desk studies before any measurement, and twenty minutes on the bench then changed its plant in three places. The handover's next actions repeated it (refit and redesign in simulation "before more bench time").
- The real system's own figure was never taken: no throw of today's loop has been recorded. "1 in 10 to 15" is the owner's memory, from before the fine gear ratio was corrected.
- **The gear ratio's correction changes today's loop on this unit:** the fine tube now turns 16% faster for the same stored profile (0.864 of the speed asked before). The AR2209 profile's gains and least speed were tuned at the old ratio. Expect more overcharges from today's loop on the Pico 2 W until it is tuned again or replaced; say so before the owner throws real charges with it.

**The next step: the creep-stop test** (`record.py creepstop`, new, console only, no firmware): the fine tube at 0.05 and 0.1 rev/s for 3 to 6 s, stopped, watched for 3 s; 30 stops of each speed with the driver left on (a hold at 0.0005 rev/s, as a charge leaves it) and 30 with it switched off. About 18 minutes, about 30 gr into the cup, the cup not handled.
- **Pass:** at one of the speeds, with the driver left on, a band of three digits (the tolerance) holds the rise after the stop in at least 29 of 30 stops. Thirty stops cannot show 1 in 50; they show whether it is worth the real throws that can. A finish by one predicted stop from a steady turn can then keep the tolerance; the design is today's finish with its stop moved earlier by the mean, and the feed before it bounded (D1's law). Then real throws.
- **Fail:** the spread is wider than the band. Then the finish needs a second, slower stage (0.02 rev/s, tried the same way), and only if that fails too is oscillation's question open.
- It also separates the driver's switching off from the powder (the two kinds of stop), and gives the scale's time to stillness after a real finish's stop.

## 2026-11-20, 00:26 to 00:42: the creep-stop test on the bench

The owner: "Bench is ready"; "don't fill over 300 grain"; "The A&D is at FAST settings." Pico 2 W, 2.0.3-84, the same powder and tubes. `record.py creepstop --speeds 0.05,0.1 --count 30`: 120 stops in a shuffled order, weights by `state` (47 a second), no tap. The session ended `VERDICT PASS`; 4.9 gr in the cup before, 35.8 gr after. Folder `logs/adr009/20261120_002709_creepstop`, its lines in `research/bench_2026-11-20/`.

| Fine tube | Driver at the stop | Flow before, digits a second | Rise after the stop, digits (0.02 gr) | Spread | Three digits hold | Still by (median, longest) |
|---|---|---|---|---|---|---|
| 0.05 rev/s | on | 1.8 | +3.0 | 2.6 | 15 of 30 | 0.7 s, 3.0 s |
| 0.05 rev/s | off | 1.9 | +2.7 | 2.3 | 15 of 30 | 0.7 s, 3.0 s |
| 0.1 rev/s | on | 3.7 | +3.1 | 2.6 | 17 of 30 | 0.6 s, 2.5 s |
| 0.1 rev/s | off | 3.4 | +2.9 | 2.3 | 15 of 30 | 0.6 s, 2.5 s |

**Verdict: fail.** The pass was 29 of 30 within three digits; the best is 17. A finish by one stop on the reading, from either speed, lands in the tolerance about half the time.

**What it shows**
- **The rise after a stop does not follow the speed.** It is +3 digits from 0.05 and from 0.1 rev/s alike. What the scale had not yet shown accounts for about 1 digit at 0.05 and 2 at 0.1 (the flow times 0.55 s); the rest, 1 to 2 digits on average, comes with the stop itself.
- **Its spread is the trouble, not its mean:** from 0 (8 stops of 30) to 10 digits (0.2 gr). The largest rises followed one to two seconds with no gain before the stop: powder gathered at the lip, then fell together.
- **The driver's switching off is not the cause:** the two kinds of stop differ by less than their spread.
- **The reading is still within 1 to 1.5 s of the stop** (nine tenths of the rise by 1.0 s, at both speeds). That stands from the review.
- **The steady turn was lumpier here than in the long runs of 11-19:** the digits gained a second have a variance over mean of 1.8 to 2.9 at 0.05 rev/s and 1.3 to 1.6 at 0.1, against 1.0 to 1.4 then. Over a whole trial, 1.6.
- **Each turn starts late:** nothing shows in the first second at 0.05 rev/s (0.18 digits at 0.1), a second and more than the scale's lag explains. A tube that has stopped gives up what stood at its lip and must bring powder forward again.

**What the review got wrong, and what stands.** "Turning steadily, this powder leaves a kernel at a time" was drawn from three recordings of 20 to 38 s and was too strong: bursts are in a steady turn too, milder than in nudges (variance over mean 1.3 to 2.9 against 4.9), with a tail to ten kernels. The first agent's "clumps at the lip are the limit" is nearer the truth than the review allowed, for every finish tried so far and not only for nudges. What stands: nudging makes it worse, the driver's switching does not matter, and the scale is still by 1.5 s after a stop.

**The slower stage, 00:45 to 00:50** (`--speeds 0.02 --count 30 --stops held --run-s 6:10`; `VERDICT PASS`; 41.4 gr in the cup after; folder `20261120_004507_creepstop`):

| Fine tube | Flow before, digits a second | Rise after the stop | Spread | By digit | Three digits hold |
|---|---|---|---|---|---|
| 0.02 rev/s, driver on | 0.8 | +0.4 | 0.9 | -1: 2, 0: 18, +1: 7, +2: 1, +3: 2 | 27 of 30 |

- **Fail by the rule (29 of 30), and nearer than the faster speeds.** The rise is what the scale had not shown and no more (0.8 digits a second times half a second): the "1 to 2 digits that come with the stop" seen at 0.05 and 0.1 rev/s are not here, so they are not made by stopping. What they are is not known.
- **But the flow at 0.02 rev/s comes in bursts:** the digits gained in a second have a variance over mean of 2.0 to 5.8, as lumpy as the nudges (4.9), with up to 10 digits in one second after seconds of nothing. A stop at a moment chosen by the PC seldom meets a burst, which is why the rise is small.
- **The test's own limit, seen only now:** a real finish stops when the reading crosses a threshold, and a burst is what carries it across. This test stopped at random moments, so it measured what lands after the stop and not how far the crossing itself overshoots. At 0.02 rev/s the crossing is the larger part. The test understates a real finish's overs wherever the flow is bursty.
- **The reading at this flow is slow to move:** nearly nothing of the rise shows before 0.9 s, most of it between 0.9 and 1.5 s. One digit alone is slower to show than a flow, as the nudges said.

**Where that leaves the finish**

| Fine tube's speed | How the powder leaves (variance over mean, 1 s) | In flight when the scale shows it |
|---|---|---|
| Nudges of 4° | 4.9, to 8 kernels | nothing, but 2 s a nudge |
| 0.02 rev/s | 2.0 to 5.8, to 10 digits | under 1 digit |
| 0.05 rev/s | 1.8 to 2.9 | about 3 digits, spread 2.4 |
| 0.1 rev/s | 1.3 to 1.6 | about 3 digits, spread 2.4 |
| 0.2 rev/s and over (11-19's runs) | 0.7 to 0.8 | 4 digits and more |

- **The bind:** the powder leaves evenly only when the tube turns fast enough that several digits are in flight behind the scale; slow enough for the scale, it leaves in bursts. No speed in this table gives both.
- **So the first agent's direction was right and the review's was not:** what would help is the powder leaving evenly at a low speed, which is what the owner's oscillation is for. The review parked it on evidence from 0.05 rev/s and over; at 0.02 rev/s the bursts are as large as the nudges'. By the closing condition the review itself set (a fail at 0.02 rev/s), oscillation's question is open again.

**What it predicts, to be checked by real throws:** today's loop stops on the first reading within 0.03 gr under the target and calls a throw over at 0.03 gr above. With these rises, a stop from its least speed would end over in about half the throws (16 of 30 in each kind with the driver on), where the owner remembers 1 in 10 to 15. Either a real throw's finish is kinder than this test's (its tube has been turning for seconds, not started from rest three seconds before), or the powder and the corrected gear ratio have made today's loop that much worse. Ten recorded throws tell which; nothing else does.

## 2026-11-20, 00:55: the first real throws recorded, today's loop as stored

The owner: "Emptied. You can start the real throw. You can re-zero the scale after each throw so I don't need to [go] and empty the cup." `record.py throws --host 192.168.2.166 --target 42.5 --profile 1 --count 6 --rezero`: each throw a charge started over the API with the stored "AR2209,gr" profile, the console's `state` read 45 times a second, then the charge stopped and the scale zeroed with the powder left in the cup. `VERDICT PASS`. Five throws, not six: each poured 43.8 gr, and the sixth would have passed the recorder's 260 gr. 219 gr in the cup. Folder `20261120_005513_throws`.

| Throw | Time | Coarse ran | Reading at the stop | A second later (the unit's verdict) | At rest |
|---|---|---|---|---|---|
| 1 | 6.2 s | 5.3 s | +0.10 | +1.30 over | +1.30 |
| 2 | 6.3 s | 5.4 s | +0.18 | +1.40 over | +1.38 |
| 3 | 6.3 s | 5.4 s | +0.16 | +1.16 over | +1.18 |
| 4 | 6.1 s | 5.3 s | +0.10 | +1.32 over | +1.34 |
| 5 | 6.3 s | 5.3 s | +0.24 | +1.46 over | +1.46 |

**Every throw over, by 1.2 to 1.5 gr.** Not the finish: the throw never reaches one.
- **The fine tube turns at 5 rev/s, its most, until the reading is 2.5 gr short** (its Kp of 2 asks for 2 rev/s a grain to go), and is still at 2.4 rev/s a quarter second before the stop. At 5 rev/s this tube pours 3.8 gr/s (11-19's map), a third of the whole feed. What is in flight behind the scale at the stop is the 1.2 to 1.5 gr.
- **By the research's own rule the gain is four times too high for this tube and scale:** a proportional feed round a delay overshoots above 0.37 / (delay x flow per turn) = 0.37 / (0.55 s x 0.75 gr) = 0.9 rev/s a grain for the fine tube. Stored: 2, and a D term that does not help.
- **The prediction made before the throws ("over in about half") was wrong in its reason:** it assumed a finish at the least speed.
- **This is not the owner's "1 in 10 to 15".** Either this unit's profile was never tuned for these tubes (the Pico 2 W has its own EEPROM, and its fine gear ratio was a generation-1 leftover), or the fine tube turns faster for the same setting than when the profile was tuned. ADR-012 rewrote the step pulses and left its check against the older firmware to these first real throws ("Handed on by ADR-012"): that check is now owed, and its result is "worse" until shown otherwise. The corrected gear ratio accounts for 16%, not for this.
- **The scale after a stop from a fast flow:** at rest within 0.5 s (the reading a second after the stop is the reading at rest, within a digit). The unit's verdict at one second was right in all five.

## 2026-11-20, 01:00 to 01:09: today's loop with its settings taken from the measurements

The owner: "Cleared"; the cup holds 700 gr and more; "Next time you don't need to throw the 42.5gr, perhaps 20gr or about should be closed enough." Twelve throws at 20 gr a run, the scale zeroed between throws, settings written live through the console and never stored, put back after (`setting ... revert`; both resources read `saved=true`). Both sessions `VERDICT PASS`. 484 gr in the cup at the end. Folders `20261120_010032_throws` and `20261120_010435_throws`.

| Run | Fine tube's settings | Over (at rest, 0.03 gr and more) | At rest, against the target | Rise after the stop | Time |
|---|---|---|---|---|---|
| As stored (42.5 gr, above) | Kp 2, most 5 rev/s, least 0.1 | 5 of 5 | +1.18 to +1.46 gr | +59 digits | 6.3 s |
| Gain from the measurements | Kp 0.8 | 10 of 12 | +0.02 to +0.40 gr | +11.5 digits | 4.4 s |
| And a slow finish | Kp 0.8, most 1.5 rev/s, least 0.03 (the motor's own least lowered with it) | 10 of 12 | 0.00 to +0.16 gr | +4.6 digits, spread 2.5 | 11.0 s (9.8 to 15.7) |

- **Settings taken from two measured numbers (the tube's gr a turn, the scale's delay) took the error from 1.3 gr to under 0.1 gr with no tuning by trial.** That part of the research (D1, D3) holds on a real unit.
- **The second run still arrived too fast:** only three throws spent a second at the least speed before the stop (two good at +0.02, one +0.20 from a burst); the other nine reached the target with the fine tube still slowing. The coarse tube stops at a reading 5 gr short with 3 gr and more of both tubes' powder in flight, so the fine tube has 1.5 gr of real room, not 5.
- **In the third run every throw reached a slow finish, and every stop was at one or two digits short.** What came after the stop: 1, 1, 3, 3, 4, 4, 5, 5, 5, 6, 8 and 10 digits. Its mean (4.6) is an offset that a stop threshold of 0.11 gr in place of 0.03 would take away; its spread (2.5 digits) is what stays. With the best offset, three digits would hold 7 of the 12, with 3 over and 2 under.
- **That spread is the creep-stop test's** (2.3 to 2.6 digits at 0.05 and 0.1 rev/s), now in real throws: the test's main figure stands. Throw 12 is the burst that crosses: three seconds at 0.03 rev/s one digit short, then 8 digits at once.
- **At these slow flows the scale is more than a second behind:** 3 digits a second fed at the stop and 4 to 5 digits still to show. The 0.55 s measured after fast flows does not hold for a slow one, and a gain set by it is still too high at the end (0.37 / (1.2 s x 0.75 gr) = 0.4). A finish slow enough for that takes about 13 s by itself.

**Where it stands**
- **Today's loop, set from measurements, lands within about 0.1 gr in 11 s on this bench; within 0.02 gr about half the time.** The stored profile is 1.3 gr over.
- **The limit is the scatter of what lands after the stop, about 2.5 digits,** the same in the open-loop test and in real throws, at every speed tried. No setting of today's loop removes it.
- **Not explained: the owner's "1 in 10 to 15".** This bench cannot give it with any settings tried. Which unit, tubes and powder that figure is from decides whether the bench is the worst case (this batch clumps "more than fresh") or the usual one.

**The owner's answer (2026-11-20, 01:12): "I was on the Pico W setup on 192.168.2.168 so it was not the same."** The "1 in 10 to 15" is the Pico W's, the owner's own trickler. Its settings as copied on 11-19 (`logs/adr008/picow/bench_after`, read from disk, no contact with the unit):

| | Pico W, tuned by the owner | Pico 2 W, as stored | Pico 2 W, set from the measurements tonight |
|---|---|---|---|
| Fine Kp | 0.8 | 2 | 0.8 |
| Fine tube's most | 2 rev/s (the motor's 3) | 5 | 1.5 |
| Fine tube's least | 0.08 (the motor's 0.01) | 0.1 (the motor's) | 0.03 |
| Fine gear ratio | 1.818 | 2.105 until 11-19 | 1.818 |
| Coarse Kp, most | 0.05, 5 (the motor's) | 0.02, 5 | as stored |

- **The rule from two measured numbers gave the owner's own hand tuning:** the bound was 0.9, 0.8 was set, and the owner's profile has 0.8 with a top speed of 2. So the calibration can stand in for tuning by hand, and the Pico 2 W's profile was simply never tuned for its tubes. No sign in this of a change by ADR-012; its check against the older firmware is still owed.
- **With nearly the owner's settings this bench was over in 10 of 12, where the owner's trickler is over in 1 of 10 to 15.** What differs is then not the settings: the powder (this batch long in the air, which the owner says clumps more), the tubes as printed and fitted, or both. Fresh powder in this bench's hopper, with the same two tests, tells which.

## 2026-11-20, 01:20 to 02:05: the owner's direction, two more recordings, the simulation refitted

**The owner, 01:20:** "your session, as well as previous session is to capture data so you can design the simulation. I don't want you to slip into the live debug or tuning session. The goal is to design the simulation with adjustable parameter so we don't need to develop the algorithm solely on the device, but based on the simulation and verify by the experiment." Written into the ADR's "Owner's direction" and the handover. The two runs of throws with changed settings (01:00 to 01:09) were that slip; they are kept as checks the simulation must reproduce. At 01:50: "carry on with your simulation and fitting ... record your progress the same way as the previous agent ... leave the decisions to tomorrow."

**Recorded for the simulation, 01:21 to 01:32** (the owner: "Yes, record"; `record.py creepstop`, weights by `state`, no hand at the cup; every session `VERDICT PASS`; 382 gr in the cup after, the unit idle with its stored settings):

| Recording | What it measures in the simulation | Result |
|---|---|---|
| Fine tube steady, 120 s at 0.02 and 0.05 rev/s, 90 s at 0.1 and 0.2 | How the powder leaves at each speed: burst sizes, their spacing, any variation by angle | Variance over mean of the digits in 1, 2 and 4 s: 3.4, 3.3, 2.4 at 0.02; 1.9, 1.8, 1.2 at 0.05; 2.2, 2.6, 1.5 at 0.1; 0.9, 0.7, 0.45 at 0.2. At 0.02 rev/s 35 rises: 19 of one digit, 16 of two to ten: the nudge test's mix |
| Coarse tube, 36 stops from 0.1, 0.3 and 1 rev/s, half plain and half with the charge's reverse quarter turn | What lands after the coarse tube stops | Plain: 0.54, 1.65 and 5.6 gr, which is the flow times 0.6 s and no more. With the reverse turn: 1.17, 2.72 and 6.3 gr |

- **The charge's back-off of the coarse tube spills powder: 0.6 to 1.1 gr more than a plain stop,** at every speed. It was added (PR #108) so that powder does not trail; the research had assumed a reverse turn takes powder back. Not judged here as good or bad: the fine tube pours after it. The simulation now has it as measured.
- **Does the tube pour in step with its turning?** (`bench/fold_by_angle.py`, the flow folded onto a part of a turn; 0.5 is even.) Once a turn: 0.74, 0.65, 0.68 and 0.59 at the four speeds, against 0.51 to 0.67 for periods that should show nothing: a once-a-turn unevenness is likely (the small tube is fed through one window). Once a rifling start (a fifth of a turn): 0.69 at 0.05 rev/s on six turns, but 0.60 and 0.54 at 0.1 and 0.2: **not shown.** An agent's note to the owner at 01:29 said it was; the longer runs took that back. About twenty turns at 0.05 rev/s would settle it.
- **The owner on the rifling (01:40):** it mimics an auger, as the tube has no tilt; "your program should be able to profile the rifling, as people may inherit my designs ... the rifling is already a part of the legacy". The simulation's tube now has the rifling's starts and a depth of its variation as its own numbers (`rifling_starts`, `rifling_ripple`), beside the once-a-turn one, and `fold_by_angle.py` measures both on a unit. The agent's view, given to the owner: keep the rifling; it is what makes a tube pour the same powder a turn at every speed (0.71 to 0.81 gr from 0.05 to 5 rev/s), and nothing measured blames it for the bursts.

**The simulation refitted** (`tools/charge_sim`; its README says how to run each part)
- **The method:** each bench test is run again in the simulation (`bench_twin.py`), and both are summed up by the same code (`bench/stats.py`). `fit_report.py` prints every measured figure beside the simulated one and marks those outside a tolerance; `fit_search.py` tries sets of parameters and ranks them. The bench's figures are in `fit/bench_pico2w.json`, each with its recordings.
- **Its parameters are three sets:** a powder, a tube, a scale (`rigs.py`). The bench's unit is the rig `bench_pico2w`.
- **What the model needed, beyond new numbers:**
  - the bed goes on avalanching for a time after a start, so a nudged tube and a slow one burst alike (`roll_tau_s`);
  - what falls after a stop is owed at the next start (the bench: about 1.5 digits missing in a start's first two seconds, about as much landing after a stop);
  - a reverse turn spills (`reverse_spill_gr`);
  - the scale holds a change of a digit or two for over a second and then takes it whole, and follows a larger one at once (`hold_s`, `open_above_gr`): one kernel first shows 1.7 s after a nudge, three and more 0.9 s.
- **The fit:** 70 to 72 of 90 figures within tolerance on three seeds, where the plant assumed before the bench has 18 of 58 (`fit/report_seed*.txt`).

| Today's loop, the three settings recorded | Bench: at rest, time, over | Simulation |
|---|---|---|
| As stored, 42.5 gr | +1.33 gr, 6.2 s, 5 of 5 | +1.21 gr, 7.0 s, all |
| Fine Kp 0.8, 20 gr | +0.21 gr, 4.6 s, 10 of 12 | +0.14 gr, 5.5 s, 9 of 10 |
| And a slow finish, 20 gr | +0.07 gr, 11.3 s, 10 of 12 | +0.04 gr, 11.7 s, about half |

- **What it reproduces:** each tube's powder a turn; the seconds of flow that show after a stop from a fast turn; the bursts at 0.02, 0.05 and 0.2 rev/s and in nudges; the rise after a stop from 0.05 rev/s, its spread, and the share three digits hold; the coarse tube's stops with and without the back-off; today's loop at three settings within 0.03 to 0.12 gr and under a second.
- **What it does not, and what would settle each:**
  - **It is kinder than the bench at the finish:** 0.03 to 0.12 gr less over in each run of throws, and half the slow-finish throws over where the bench had 10 of 12. Twelve throws leave that wide (10 of 12 is 52% to 98%); more throws at that setting would narrow it.
  - **The rise after a stop is too large from 0.02 and 0.1 rev/s** (1.2 against 0.4 digits; 4.4 against 3.1) and right from 0.05. The fall after a stop is one number here; it may depend on the speed.
  - **A release of three kernels and more shows 0.25 s sooner than on the bench** (0.63 against 0.90 s), while the delay behind a fast flow is right. The scale's inner filter is a delay and a smoothing here; a known weight put on the pan with no tube turning would give its true shape. Not possible with what is on the bench without the owner's hand or a rig.
  - **The largest second at a slow turn is larger than the bench's** (21 against 8 digits at 0.02 rev/s), and nudges release three and more half as often (4% against 8%).
  - **The 0.1 rev/s long run was burstier than the simulation and than the bench's own other runs at that speed** (2.2 against 1.0 to 1.8): the once-a-turn unevenness is the likely cause, not yet in the fitted numbers.
  - **The coarse tube's start after a back-off** could not be read: plain stops and back-offs were shuffled, so a trial's start follows another kind's stop.
- **Fit for what, today:** designing the feed (how much flow the delay allows, where the coarse tube stops, what the back-off costs) and comparing finishes against each other. Not yet for quoting a finish's over-rate: there it is known to be kind.

**For the owner, tomorrow** (nothing here was decided or run):
1. **Fresh AR2209 in this bench's hopper,** the same recordings (about 25 minutes, no hand at the cup after the start): a second powder as a second set of numbers, and whether this batch or the tubes make the bursts.
2. **Twenty turns at 0.05 rev/s** (7 minutes, 15 gr): the rifling seen or not.
3. **A known weight on the pan,** if a way is found: the scale's own response, apart from the powder.
4. **The Pico 2 W's stored AR2209 profile is 1.3 gr over on every throw** with these tubes; the Pico W's has the gain the measurements give. Whether to store a measured profile on the Pico 2 W is the owner's.
5. **The coarse back-off spills 0.6 to 1.1 gr.** Whether that is wanted is a question for the design in the simulation, not a fault to fix now.
6. Then the feed and the finish designed in the simulation, and its prediction checked by throws.

**One read of the unit after the owner had gone to bed:** `state` and `stats` at 02:01, to record how the unit was left (idle, both motors at 0, 382.38 gr in the cup, the stored profile selected). The agent had said there would be no contact; this one read changes nothing, and is recorded because it was said.

**Not done in this session:** the scale at rest and with the motors turning on an empty hopper (the hopper was loaded); the chain test; a second pass of the maps; the coarse tube above 3 rev/s; the history run; ten recorded throws with today's loop (each needs the cup taken off and put back); oscillation and the flick; the simulation refitted to these figures.

**Not done**
- The simulation still holds its assumed figures: `research/bench_guide.md`'s "After the session" is the next step.
- Oscillation is not in the simulation: there is nothing to model it from until the bench's step 7.
- The automatic tuning is designed (README) and its measuring half simulated (`calibrate.py`); its verifying and trimming half is not.
- numpy and matplotlib were installed into the venv and not used; they are not in `requirements.txt`.

## 2026-11-20, daytime: the simulation rebuilt on measured records, no unit

**The owner, after reading the night's notes:** "I don't want you to enter the tuning mode. Your job is to build simulation to answer my above questions without touching the physical units later on." The questions: what the simulation still needs and which powders; how it is calibrated to another setup; control beyond PID; other scales by their lag and period; speed against accuracy as a user's choice; whether a tube may turn back and forth. Calibration is seen in three parts (scale, tube, powder), built into the OpenTrickler, working without an AI agent; and the simulation must be one other users can adopt. To the review's proposal (the calibration's measurements become the simulation's input; one capture campaign; then no unit until verification): "Yes, start". No contact with a unit in this session.

**What was wrong with the fit of 02:05.** About twenty physics numbers a tube, searched by hand until the figures agreed, the three runs of throws among the targets. Nobody could repeat that for another unit, and "it reproduces today's loop" was partly fitted.

**What was built** (`tools/charge_sim`; its README has the commands)
- **A tube is tables a calibration measures** (`plant.py`): its grains a turn, the size of its releases and what falls after a stop, each by speed. The held powder, the inlet, the centrifuging and the avalanche formula are gone: no recording could give them. Checked first against the hand fit written as tables: 74 of 90 figures, as before (70 to 72).
- **A rig is a record** (`records/bench_pico2w.json`): a scale, a powder's kernel, each tube with that powder in it, and every number's source: measured, solved, stated or assumed.
- **One command makes it from a session's raw folders** (`build_rig.py --logs ... --manifest ...`, 6 minutes on 32 cores). Each solved number is the value at which the simulation, run through the bench's own test, gives the bench's figure, one number at a time. An effect that may not be there (a ripple, kernels after a stop, an evenness) is taken at the least value that explains the figures, so none is claimed from noise. The scale's delay is solved once, from fast flows, so it is never traded against powder falling after a stop (the first build did trade them: 35 kernels after a coarse stop).
- **The throws of today's loop are no longer used to make the record.** They are the check.
- **`bench/measure.py` reads a session's manifest** in place of folder names in the code, and gives two new figures: the scale's frame rhythm (0.0475 s) and the flow folded by the tube's angle. Run again on the raw recordings it gives every earlier figure unchanged.
- Removed: `matrix.py`, `pr113_check.py`, `fit_search.py` (the assumed plant's, and the hand search). They are in git before this commit.

**Does the built record reproduce the bench?** (`fit/report_seed1.txt` to `3`)

| | Hand fit of 02:05 | Built by the command |
|---|---|---|
| The record's own recordings (maps, stops, steady runs, nudges) | 60 of 72 | 63 to 70 of 80 |
| Throws of today's loop | 14 of 18, fitted to them | 13 to 14 of 18, never shown them |

| Today's loop | Bench: at rest, time, over | Built record |
|---|---|---|
| As stored, 42.5 gr | +1.33 gr, 6.2 s, 5 of 5 | +1.36 gr, 6.6 s, all |
| Fine Kp 0.8, 20 gr | +0.21 gr, 4.6 s, 10 of 12 | +0.17 gr, 5.1 s, 86% |
| And a slow finish, 20 gr | +0.07 gr, 11.3 s, 10 of 12 | +0.04 gr, 11.3 s, 49% |

**Verdict: pass for the pipeline.** A record made with no hand in it predicts throws it never saw as well as the hand fit that was shown them. **It is still kind at the slow finish** (49 to 54% over on three seeds against 10 of 12; its spread on the stored profile's throws is also twice the bench's, from coarse bursts solved on six stops a speed), which is the next thing to mend.

**Which numbers the recordings pin down** (`pinned.py`, `fit/pinned_bench_pico2w.txt`): each number moved by a half, chance measured first on four other seeds.
- **Pinned, 26:** the scale's delay and its hold; both tubes' grains a turn; the fine tube's bursts at each speed, its once-a-turn ripple (0.39: real, where the rifling's is 0), what falls after its stops; the coarse tube's bursts.
- **Loose and harmless to today's loop, 24:** among them every number of the fine tube turned backward. Harmless only because today's loop never turns it back: for an oscillating finish they are simply not measured.
- **Loose and a throw notices, 2: both are the shape of the scale's response to a small change** (how fast it follows, and from what size of change it follows at once). No recording used gives them, and a half more or less moves the overs by 11 to 12 throws in 100.

**Why the simulation is kind at the finish: the scale, not the powder.** *(Wrong, as the evening's work below showed: three shapes of the scale's response left the finish where it was. What follows is kept as it was written.)*
- Trading the scale's delay for a slow rise of the same total lateness takes the slow finish from 52% over to 92% (the bench: 83%) and the rise after its stop from 2.1 to 4.3 digits (the bench: 4.6), while the stops' figures hardly move. But the coarse tube's stops show the scale is sharp for a large flow (three quarters of the rise by 0.4 s, all by 0.8 s). So it is the small changes that the model has wrong.
- **The raw frames show what the scale does** (11-19's tapped folders; nothing new recorded):
  - it calls the weight stable 0.38 s after the last change (140 times in three folders, nine in ten of them within 3 ms of 0.380 s), and never changes the weight without first calling it unstable (1,636 changes);
  - a release of 7 or 8 kernels first shows 0.7 to 0.9 s after the nudge, as a jump of 2 or 3 digits, then a digit about every 0.1 s: a ramp, not "followed at once";
  - one kernel first shows anywhere from 0.7 to 3.1 s after the nudge. The model holds every small change 1.1 s, then shows it whole.
- **Read (the agent's, not yet fitted):** the balance holds its reading inside a band while it calls it stable, and lets go when the change is clear or after a time that varies. At a finish of one to two digits a second the reading is then one to two digits behind and catches up in a jump: the stop comes late. That would be the selection the creep-stop test could not show ("a real stop is triggered by a burst arriving").
- **Not mended in this session.** It is the next piece of desk work and needs no bench: the frames of 11-19 hold about 1,600 changes of weight with their flags to fit a stable-band model to.

**Other scales** (`scale_map.py`, `fit/scale_map_bench_pico2w.txt`; today's loop, slow finish, 300 throws a cell, overs of the true weight, the scale's hold as measured)

| Delay | A reading every 0.05 s | 0.25 s | 0.5 s |
|---|---|---|---|
| 0.2 s | 43% | 46% | 59% |
| 0.45 s (the bench's is 0.41) | 58% | 75% | 98% |
| 0.8 s | 100% | 100% | 100% |
| 1.2 s | 100% | 100% | 100% |

- **Today's loop with one set of settings holds only near the scale it was set on.** With its fine gain set for each scale by the rule (0.37 / (lag x grains a turn)) it stays at 45 to 70% up to a delay of 0.8 s at any period, and fails at 1.2 s (about 90%): the coarse tube's stop is not set by any rule.
- The map is of delay, period and hold, not of makes: a make is a point on it once its three figures are measured.

**How evenly must the fine tube pour?** (`evenness.py`, `fit/evenness_bench_pico2w.txt`; today's finish at 0.03 rev/s, each variant at its own best stop, 400 throws)

| Fine tube | Scale | Over | Under | Good |
|---|---|---|---|---|
| As measured | as measured | 29% | 14% | 57% |
| Nothing falls after a stop | as measured | 21% | 12% | 67% |
| A kernel at a time | as measured | 19% | 9% | 72% |
| A kernel at a time, like a metronome, nothing after a stop | as measured | 5% | 7% | 88% |
| The same | shows every change at once | 5% | 1% | 95% |

- **Verdict: no evenness gives 1 over in 50 with a finish by one stop on the reading.** A tube that pours one kernel at a time, as evenly as a metronome, with nothing falling after its stop, still leaves 1 in 20 over and 1 in 14 under on this scale.
- **So oscillation alone is not the answer, and neither is a better stop.** The finish itself must change: a stage that ends short on purpose and adds what is missing, judged on a weight the scale has had time to show. What oscillation would buy is a smaller, steadier last stage, and the scale's own hold costs about as much as the powder's bursts.
- The simulation is kind at the finish, so the real figures are worse than these, not better. The order of the rows is what to read.

**The first candidate (`flow_dose`) on the measured rig:** 98% over, +0.63 gr (100 throws). Its calibration and its nudges were written for the plant assumed before the bench. It is a place to start the design from, not a figure.

**What the recordings do not yet give, for the capture campaign**
1. **The scale's response to a small change:** to be fitted from the frames already recorded (above). The campaign then needs its frames with their flags: weights by `state` lose them, and the tap ran the scale task's stack down on 11-19, so the stack is mended first or the tap kept short.
2. **The fine tube turned backward:** a reverse turn after a stop, the start after it, a back-and-forth at the console's rate. Nothing of it is measured.
3. **The coarse tube's evenness:** it has no run longer than 2.5 s, and six stops a speed; its bursts are solved from the spread of six rises.
4. **A stop the reading triggers,** in open loop, and a stop after slowing down from speed: what a real finish does and the creep-stop test did not.
5. **A second unit and other powders,** to tell tube from powder: the owner offers the Pico W's own build, fresh AR2209, AR2208 and ADI BM2.

**Next, in order:** (1) the scale's stable band fitted from the frames, the record rebuilt, the slow finish checked again; (2) the capture protocol written as one fixed script that leaves one folder and one manifest a cell; (3) the campaign; (4) the finish and the feed designed on the records, the predictions written down before the verifying throws.

## 2026-11-20, evening: the scale's shape tried three ways; the finish's gap is the powder's, not the scale's

The owner, to "fit the scale's small-change behaviour from the frames already recorded?": "Yes, fit it". No unit. **The record and the model are as committed in the afternoon: nothing tried here was better by the figures, so nothing was kept but the tools' worker pool and two more judged figures.**

**What the frames say of the scale, with no model** (the tapped folders of 11-19)
- **A large change is followed after a plain delay.** After a stop from a fast flow (coarse 1 and 3 rev/s, fine 3.5 and 5) the reading goes on rising in a straight line and is done by 0.6 to 0.7 s: 0.18, 0.35, 0.53, 0.72, 0.88, 0.96, 1.00 of the rise by each tenth of a second.
- **A small change from rest is followed slowly, and not as "held, then shown whole":**

| Release, digits | Releases | The reading first moves, s after the nudge | First jump, digits | Then |
|---|---|---|---|---|
| 1 | 13 | 0.29, 0.71, then 1.81 to 3.08 | 1 | |
| 2 | 4 | 0.82 to 1.53 | 1 | the second digit 1.4 to 1.9 s later |
| 3 | 3 | 0.90 to 1.26 | 1, 1, 2 | 0.06 to 0.12 s a digit |
| 5 and more | 5 | 0.73 to 0.94 | 2 or 3 | 0.10 to 0.14 s a digit |

**Three shapes of the scale's response, each against the bench's figures and the slow-finish throws**

| The scale | Its own figures | Slow finish: rise after the stop, over |
|---|---|---|
| The bench | | 4.6 digits, 10 of 12 |
| Holds a small change, then shows it whole (as committed) | 63 to 70 of 80 | 2.2, 54% |
| Follows a small change slowly, a large one at once (two speeds) | 65 to 69 of 81; the nudges worse (a single kernel shows too soon: late ones 24 to 32% against 57%) | 1.9 to 2.5, 44 to 55% |
| Holds only from rest: follows small changes until still for 0.38 s | not built | 1.6 to 2.7, 38 to 56% |

- **Verdict: fail, three times.** No shape of the scale's response to a small change moves the slow finish. The afternoon's "the cause is the scale's response to a small change" was wrong, and "a throw notices" in `pinned.py` was true of the simulation, not of the bench.
- **The two-speed shape is the truer picture of a single kernel and of two** (the table above), and the hold fits the figures better. Neither is right; what would tell is several hundred nudges with five seconds between and the frames kept.

**What the recorded throws themselves show** (the twelve slow-finish throws, sample by sample, beside 150 simulated)

| In the last second | Bench | Simulation |
|---|---|---|
| Fine tube 1 s and 0.5 s before the stop, rev/s | 0.217, 0.105 | 0.156, 0.082 |
| Reading 1 s before the stop, digits short of the target | 13.7 (from 2 to 26) | 9.2 |
| Rise after the stop by 0.2, 0.6 and 3 s, digits | 1.8, 4.0, 4.5 | 0.1, 0.5, 1.9 |

- **A slow-finish throw never creeps.** It reaches the stop still slowing down, its reading climbing 12 digits a second, and what follows the stop comes within 0.6 s, as a pipe empties. The scale is following at once there; its small-change behaviour is not in play.
- **The bench pours more powder for the same commanded turns while the fine tube slows down** than the simulation does: 17 digits arrive from one second before the stop against 11, roughly a third more once the bench's faster turning is allowed for (a rough figure, from the means at six moments). The simulation's flow is its speed times a constant, at once.
- The reading trails the commanded speed by 0.58 to 0.72 s over the last four seconds (best fit, each throw; the simulation 0.60), so it is not a longer delay overall.

**One thing did move the finish: the flow following the speed over a part of a turn.** Tried by hand on the committed record, the fine tube's flow made to follow its speed over 0.035 of a turn (a hundredth of a second at 3 rev/s, a second at 0.03):

| | Bench | As committed | With the lag |
|---|---|---|---|
| Slow finish: at rest, rise after the stop, over | +0.072 gr, 4.6, 83% | +0.039, 2.2, 54% | +0.074, 3.9, 81% |
| Fine Kp 0.8: at rest, rise | +0.21 gr, 11.5 | +0.20, 9.4 | +0.24, 11.9 |
| As stored: at rest | +1.33 gr | +1.36 | +1.46 |
| Starts, digits in the first second at 0.02, 0.05, 0.1 rev/s | 0.03, 0.03, 0.19 | 0.33, 0.19, 0.95 | 0.0, 0.01, 0.33 |

- **The starts are open-loop recordings of the record's own,** and they ask for the same part of a turn: at 0.02, 0.05 and 0.1 rev/s the tube turns about 0.03 of a turn before powder comes. So the value is not taken from the throws, and it then lands the slow finish.
- **Not adopted, because it starves the nudges.** As modelled, a tube that starts pours nothing over its first 0.03 of a turn and never makes it up; a nudge of 0.011 of a turn would then give about a seventh of what the bench's nudges gave (0.46 kernels a nudge, the full grains a turn): by calculation, not run. Made to give it back (the powder under way falling after the stop), the starts and the rises after a stop go wrong instead (rise from 0.05 rev/s 3.6 to 4.4 digits against 2.9). A lag in time (0.2 s) lands the slow finish too and puts the faster throws 0.5 gr over.
- **Read (the agent's):** something of this kind is real: a tube that slows down goes on pouring at its faster rate for a part of a turn. Its right form is not in these recordings, because none of them slows a tube down in open loop.

**The figures the report now judges, for the next agent:** `start_1s` (the digits in a turn's first second: the simulation starts too soon, 0.25 against 0.03 and 0.9 against 0.2) and the nudges' `single_late` (the share of single kernels first shown later than 1.5 s: 57% on the bench, 82 to 96% with the hold). With them the committed record has 66 to 73 of 86 of its own figures and 13 to 14 of 18 on the throws.

**Found in passing: worker processes that fail after many jobs** (`tools/charge_sim/workers.py`)
- Five builds in five lost a worker process with no error, or once raised `ZeroDivisionError: float modulo` on `x % 1.0`, which correct arithmetic cannot do. Never at the same job twice; nothing in Windows' logs; the PC was busy with other work (an LLM server, an image server).
- The same jobs never failed in a process started for them: one job run 96 times side by side on 16 cores gave the same number to the last bit, and the failing batch passed alone three times.
- **Cause not found.** What holds: fresh worker processes for every batch, on half the cores, and a failed job run once more (the runs are seeded, so it gives what it would have). A build made this way gave the committed record again with no difference, one job failing on the way (a `TypeError`) and passing when run again.
- The first agent's lost worker of 11-19 ("not reproduced") is likely the same.

**What settles the finish, for the capture campaign** (each measures a number of the simulation; none tries a setting)
1. **A tube slowed down in open loop:** the fine tube from 1.5 to 0.03 rev/s over two to four seconds, then stopped, thirty times, and the same held at 0.03 for three seconds first. Whether more powder comes than the turns account for, and over what part of a turn.
2. **Speed steps with no stop between** (`record.py steps` does it): up and down between 0.05, 0.2 and 1 rev/s. The flow's answer to a change of speed, both ways.
3. **Stops and steady runs of the fine tube between 0.2 and 1 rev/s:** today's loop's last second lives there, and there is one run.
4. **Several hundred nudges with five seconds between, the frames kept:** the scale's small-change shape, which a finish that adds single kernels will lean on.
5. As before: the fine tube turned backward, the coarse tube's evenness, a second unit and other powders.

**Next, in order:** (1) the capture protocol as one fixed script, with these in it; (2) the campaign; (3) the flow's lag and the scale's shape fitted from it; (4) the design.
