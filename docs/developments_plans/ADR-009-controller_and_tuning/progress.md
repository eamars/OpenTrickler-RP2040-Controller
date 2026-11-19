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

**Not done in this session:** the scale at rest and with the motors turning on an empty hopper (the hopper was loaded); the chain test; a second pass of the maps; the coarse tube above 3 rev/s; the history run; ten recorded throws with today's loop (each needs the cup taken off and put back); oscillation and the flick; the simulation refitted to these figures.

**Not done**
- The simulation still holds its assumed figures: `research/bench_guide.md`'s "After the session" is the next step.
- Oscillation is not in the simulation: there is nothing to model it from until the bench's step 7.
- The automatic tuning is designed (README) and its measuring half simulated (`calibrate.py`); its verifying and trimming half is not.
- numpy and matplotlib were installed into the venv and not used; they are not in `requirements.txt`.
