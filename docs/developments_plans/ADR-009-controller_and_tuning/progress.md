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

## 2026-11-20, 21:08 to 22:31: a capture session, the same unit and powder

**The owner:** "Before working on any fitting, I have the scale with powder loaded. Do you want to capture more data per our earlier discussion?" Asked which unit, powder, cup and how much: the Pico 2 W as before, the same air-exposed AR2209, an empty cup on the pan, all of the list. During it: "Are you going to run forward AND reverse, and other capture as well?", for which two recordings were added (a reverse of four lengths, and back and forth). The PC had died just before (the owner: "Computer died"); nothing was lost.

**What each recording measures is a number of the simulation; no setting of today's loop was tried.** Pico 2 W, 2.0.3-84, the A&D at FAST. Every weight by `state` (47 a second), no tap. Every session `VERDICT PASS`. Nothing stored or changed on the unit; it is idle, 546 gr in the cup, which needs emptying. Raw folders under the main checkout's `logs/adr009`; the figures below in `research/bench_2026-11-20/evening_capture.txt`, each made by a script kept in `tools/charge_sim/bench/`.

| Folder | Recording | Trials |
|---|---|---|
| `210809_creepstop` | Fine tube, stops from 0.2, 0.5 and 1 rev/s | 15 a speed |
| `211314_ramp` | Fine tube slowed from 1.5 to 0.03 rev/s in fifteen equal ratios over 3.3 s, stopped; and the same held 3 s at 0.03 first | 30 and 30 |
| `212307_ramp` | Fine tube slowed as the recorded throws' last three seconds (1.0, 0.75, 0.5, 0.31, 0.22, 0.105, 0.05 rev/s), stopped | 30 |
| `212658_ramp` | Fine tube stepped between 0.05, 0.2 and 1 rev/s with no stop | 5 runs of eight changes |
| `213243`, `213358`, `213453_creepstop` | Fine tube steady: 60 s at 0.2, 40 s at 0.5, 20 s at 1 rev/s | 1 each |
| `213528_creepstop` | Fine tube stopped plainly and with the charge's reverse quarter turn, from 0.05, 0.1 and 0.3 rev/s | 15 a speed and kind |
| `214803_ramp` | Fine tube forward at 0.2 rev/s, back by 0.05, 0.1, 0.25 or 0.5 of a turn, forward again 6 s | 8 a length |
| `215552_ramp` | A minute at a net 0.05 rev/s: steady, in small swings (+0.25, -0.15 rev/s) and in wide ones (+0.45, -0.35), about four swings a second | 2 each |
| `220252`, `220337_creepstop` | Coarse tube steady: 30 s at 0.1, 8 s at 0.3 and at 1 rev/s | 1 each |
| `220430_nudgewatch` | 300 nudges of 0.017 of a turn, the reading watched 5 s after each | 300 |

**1. The finish's missing powder is ordinary open-loop behaviour.** The tube taken through a real finish's speeds with no controller leaves what the real throws left:

| After the stop | The twelve slow-finish throws | The throw-shaped ramp, open loop |
|---|---|---|
| Digits landed, mean and spread | 4.5, 2.5 | 4.5, 2.2 |
| Arrived by 0.2, 0.6 and 1.0 s | 1.8, 4.0, 4.5 | 1.5, 3.8, 4.4 |

The last 0.6 s of its turning accounts for 2.1 of the 4.5. The gentler ramp, which spends its last second under 0.1 rev/s, leaves 2.0 (its turning accounts for 0.9), and 1.2 when held at 0.03 rev/s first.

**2. What a stop leaves depends on the speed it comes from, and most at the middle speeds.** Stops from a steady turn, last night's and today's together:

| Fine tube | Flow before, digits a second | After the stop, digits | Of them beyond 0.6 s of the flow | All in by |
|---|---|---|---|---|
| 0.02 rev/s | 0.9 | +0.4 | -0.1 | 1.5 s |
| 0.05 | 1.5 | +2.6 | +1.7 | 1.5 s |
| 0.1 | 3.1 | +3.0 | +1.1 | 1.5 s |
| 0.2 | 6.9 | +9.9 | +5.8 | 1.5 s |
| 0.3 | 9.5 | +10.0 | +4.3 | 1.5 s |
| 0.5 | 17.2 | +16.6 | +6.3 | 1.5 s |
| 1 | 41.4 | +25.3 | +0.5 | 1.5 s (24.5 by 1.0 s) |

- **From 0.2 to 0.5 rev/s about 4 to 6 digits more than the delay accounts for land after a stop;** from 1 rev/s next to none. The simulation had no stop above 0.1 rev/s and took what follows a stop there for nothing. Today's loop passes through 0.2 to 0.5 rev/s in its last second.
- It does not depend on the run's length (2 to 4 s) and little on the trial before (0.2 rev/s: 7.8 digits after a trial at 0.2, 11.6 after one at 1).
- **What falls is owed at the next start** (from the reverse stops' session, starts after a plain stop): at 0.3 rev/s the start is 6 digits short of a steady flow by its third second, at 0.1 rev/s 4 short, at 0.05 rev/s 1 short.

**3. A tube that slows down does not lag, and pours its usual powder.** Over each kind of ramp the reading trails the tube's own speed by 0.60 to 0.65 s (best fit a trial, 0.45 to 1.0), the same as after a steady stop, at 0.77 gr a turn (spread 0.08). **So the evening's lead, "the flow follows the speed over a part of a turn", is wrong as a lag:** what it stood in for is point 2.

**4. Speed steps with no stop.** From 1 rev/s down to 0.2 or 0.05 the reading's slope changes 0.47 to 0.56 s after the command and no powder is gained or lost (-0.3 and -1.3 digits). From 0.2 down to 0.05, about 2 digits more arrive than a clean change gives (9 changes of 10). Steps up are too noisy on five runs to read.

**5. The fine tube turned backward** (nothing of it was measured before)
- **The charge's reverse quarter turn after a stop spills about a kernel:** +1.7 digits at 0.05 rev/s, +0.8 at 0.1, +0.7 at 0.3, where the coarse tube's spills 0.6 to 1.1 gr.
- **The forward turn after a reverse gives less than its turns,** and the loss levels off at a third of a turn:

| Back by | While turning back, digits | The forward turn after it is short by |
|---|---|---|
| 0.05 turn | +1.2 | 0.12 turn (4 digits; spread 7) |
| 0.10 | +1.4 | 0.23 turn (9 digits) |
| 0.25 | +5.0 | 0.36 turn (14 digits) |
| 0.50 | +4.6 | 0.33 turn (13 digits) |

  About a second after it starts forward again the flow is back at its steady rate.
- **Back and forth at four swings a second does not even the slow flow.** Variance over mean of the digits in 1, 2 and 4 s: steady 1.7 to 1.9, 1.6 to 2.1, 0.7 to 1.0; small swings 2.3, 2.5 to 2.7, 0.8 to 2.9; wide swings 1.6 to 2.4, 1.8 to 3.1, 2.3 to 2.9. Two runs of a minute each: not worse by much, and no better. With last night's flick test that is twice the same answer for swings made from the PC; a fast oscillation needs a command in the firmware and is still not measured.

**6. The fine tube steady at the middle speeds** pours evenly: variance over mean in 1, 2 and 4 s 0.77, 0.94, 0.19 at 0.2 rev/s and 0.86, 0.62, 0.36 at 0.5; 0.73 to 0.82 gr a turn.

**7. The coarse tube at 0.1 rev/s is lumpy:** variance over mean 5.5 and 4.6 in 1 and 2 s, its largest second 79 digits of a mean 49; 0.57 s of its flow after the stop. The 8 s runs at 0.3 and 1 rev/s give its flow (9.7 and 9.4 gr a turn) and its stop (0.62 and 0.55 s of flow) but are too short for windows of a second; their samples are there for shorter ones.

**8. How the scale shows a small change** (300 nudges; 69% gave nothing, 13% one digit, 8% two, 4% three, 6% four and more; 0.69 digits a nudge, which is the tube's full 0.81 gr a turn)

| The release, digits | Releases | The reading first moves, s after the nudge began: a tenth, half, nine tenths of them |
|---|---|---|
| 1 | 40 | 0.67, 1.95, 4.05 |
| 2 | 24 | 0.80, 1.17, 1.61 |
| 3 | 13 | 0.86, 1.05, 1.17 |
| 4 | 6 | 0.80, 0.87, 0.92 |
| 5 and more | 11 | 0.61, 0.81, 0.91 |

- The time to the first move falls steadily with the size of the change; one digit alone is spread from under a second to over four. Of the two-digit releases, 15 of 24 were shown a digit at a time, the second 1.5 s after the first.
- **For a finish that adds single kernels:** a wait of 2 s sees half of them, 4 s nine in ten.

**What changes in the afternoon's and the evening's conclusions**
- The simulation is kind at the finish because **it has nothing falling after a stop from the middle speeds**, and no mass owed at the start after. That is now measured from 0.02 to 1 rev/s, and the throw-shaped ramp is an open-loop check of it that is not the throws.
- "The flow follows the speed over a part of a turn" is withdrawn (point 3).
- "No evenness of the powder gives 1 over in 50 with a finish by one stop" was drawn on a simulation without this; it is to be run again once the record has it. It can only get harder for one stop: what a stop leaves is spread by 2 to 4 digits at every speed above 0.02 rev/s.

**In the recorder** (`bench/record.py`): a `ramp` test (speeds one after another with no stop, negative ones too, then a stop, the tube's own speed read with each weight); the runs written after every trial, so a PC that stops loses one trial; a watched nudge is `creepstop` with a turn of 0.05 s, summed by `nudge_watch.py`.

**Found in passing, for ADR-012's owner:** the unit's count of late step feeds on the fine motor went from 18 to 101 over the session (`motor`: `fine_underruns`), the coarse motor's stayed 0. Not looked into; the session sent the fine motor a new speed up to eight times a second for minutes.

**Not captured:** a second unit, another powder, an oscillation faster than the PC can command, the coarse tube's evenness at 0.3 and 1 rev/s over a longer run, the scale with a known weight.

**The owner then asked a question under the whole design, and set the night's frame** (the README's "Owner's direction" has the words): is speed against charge weight the right relationship at all, where steps against weight or step frequency against weight are others; how powders differ in their "step size" and how a step parameter would be tuned for each; that a scale shows a stream of change sooner than one small change (his G&G JJ223BF takes 5 s and more for a tiny one); that the control may be a hybrid; and: no decision while capturing, "make assumption, and use the actual trickler to verify the assumption". The cup was cleared for it. What follows is capture and verdicts on assumptions; nothing of the design is chosen.

## 2026-11-20, 22:53 to 23:34: turns against weight, by fixed moves

**From the recordings already made** (`bench/turns_to_weight.py`, `bench/within_run.py`; `research/bench_2026-11-20/late_capture.txt`): every trial of the day that begins and ends at rest, its true turns (the unit's count of its steps) against the powder in the cup.

| Fine tube | Trials | Turns | Grains a turn | A trial off the line by |
|---|---|---|---|---|
| 0.02 rev/s | 31 | 0.12 to 2.4 | 0.74 | 3.4 digits |
| 0.05 | 78 | 0.15 to 6 | 0.74 | 3.7 |
| 0.1 | 78 | 0.3 to 9 | 0.71 | 5.1 |
| 0.2 | 17 | 0.4 to 18 | 0.72 | 5.4 |
| 0.5 | 16 | 1 to 20 | 0.77 | 4.8 |
| 1 | 16 | 2.5 to 20 | 0.80 | 4.8 |
| The slowing ramps | 90 | 1.3 to 1.5 | 0.77 | 6 |
| Every speed on one line | 348 | | 0.757 | 6.6 digits |

- One line through nothing fits every speed; a start and a stop together cost nothing (the free lines pass within 3 digits of no powder at no turn).
- The scatter does not grow with the amount: 0.15 of a turn and 20 turns are both 4 to 6 digits off. Inside a steady run it levels off the same way (0.2 rev/s: 1.7 digits over a tenth of a turn, 2.4 over a quarter, 2.6 over a half, 2.7 over one, 3.7 over two).
- Step frequency is the speed by another name: it is not a third relationship.

**Three assumptions, each run on the unit** (fixed moves from rest to rest by `record.py ramp` with one speed a plan, 4 s watched after each, the kinds shuffled; `VERDICT PASS`; folders `225315_ramp` fine, 390 moves, and `232930_ramp` coarse, 60; 384 gr in the cup after; `bench/moves_look.py`, `bench/phase_look.py`)

**1. What a move pours is set by its turn and not by its speed. Holds.**

| Fine tube, a move of | At 0.05 rev/s: digits, spread | At 0.2 | At 1 |
|---|---|---|---|
| 0.03 turn | 0.8, 1.3 | 0.9, 1.4 | |
| 0.06 | 1.6, 1.7 | 1.9, 2.0 | 5.1, 2.8 (0.086 turn) |
| 0.12 | 4.1, 2.5 | 3.3, 2.4 | 5.5, 2.7 (0.145 turn) |
| 0.25 | 8.8, 3.5 | 9.1, 4.0 | 10.0, 3.5 |
| 0.5 | | 18.4, 4.4 | 17.8, 4.7 |

The PC cannot time a move at 1 rev/s finer than about 0.09 of a turn; the turn given is the one the steps counted.

**2. A move's spread levels off as the move grows. Holds, at about half a turn.** From 1.3 digits at 0.03 of a turn to 4.4 to 4.7 at a half, where the longer moves and the steady runs sit. A move of 0.03 of a turn pours nothing two times in three; of 0.06, four times in ten at 0.05 rev/s. The moves of 0.03 and 0.06 of a turn gave 0.5 gr a turn where the longer ones gave 0.7: 4 s is too short a watch for a single kernel on this scale (one in ten takes longer), and what a short move leaves at the lip the next move takes.

**3. Part of that spread is the tube's once-a-turn unevenness, and the step count alone says where the tube is in its turn. Holds for the fine tube.** The tube's angle was followed through the session's 78 turns by adding up the steps of every move. Each move's powder against the mean of its own kind, by the eighth of a turn its middle fell in: -0.20, +0.29, -0.38, -0.72, -0.14, +0.07, +0.26, +0.88 of that kind's spread. A wave of 0.52 of a spread where chance (the same moves, their angles shuffled) gives 0.09, and none of 500 shuffles as large. The same holds for the moves of up to 0.15 of a turn alone (0.41) and with the half turns (0.57). **Not shown for the coarse tube** (0.28 against chance's 0.24, 48 moves).

**The coarse tube by turns:** 0.26 of a turn 122 digits (spread 17), 0.51 266 (13), at 0.5 rev/s; 0.29 137 (16), 0.53 247 (12), 1.04 477 (20) at 2 rev/s: 9.2 to 10.4 gr a turn, a move 12 to 20 digits (0.24 to 0.4 gr) off whatever its size or speed. The day's earlier trials (stops after mixed kinds, some after a reverse turn) had put it at 43 to 65 digits.

**The coarse tube steady, 23:36** (the owner: "Cup is empty"; `233605_creepstop`, 20 s each, `VERDICT PASS`, 286 gr in the cup after): the evenness the 8 s runs could not give.

| Coarse tube | Grains a turn | Variance over mean of the digits in 1 and 2 s | Spread of the digits over 0.25, 0.5, 1, 2 turns | After the stop |
|---|---|---|---|---|
| 0.1 rev/s (30 s, earlier) | 9.9 | 5.5, 4.6 | 20 over a quarter | 0.57 s of its flow |
| 0.3 | 9.9 | 2.7, 1.0 | 17, 14 | 0.52 s |
| 1 | 9.8 | 0.4, 0.6 | -, 18, 17, 18 | 0.54 s |

Its scatter inside a run levels off as the fine tube's does, at 14 to 18 digits (about 0.3 gr), the figure its fixed moves gave. It pours evenly at 1 rev/s and in lumps at 0.1. After a stop from any of the three speeds 0.52 to 0.57 s of its flow follows and no more: nothing beyond the scale's delay, where the fine tube leaves 4 to 6 digits from its middle speeds.

**Not answered, and why:** how powders differ in what a turn pours and in a move's spread: one powder. The assumption to test when another is loaded: the grains a turn follow the bulk density (this AR2209 gives 0.757 gr a turn in the fine tube and about 9.4 in the coarse), the turn that is one kernel follows the kernel's mass (0.028 of a turn here), and the spread stays a few kernels.

## 2026-11-20, 23:45: a second powder, ADI BM2: what is expected, written before its first run

**The owner:** "Next powder is ADI benchmark 2. smaller kernel and less clump, on pico 2w still. I want to perform all tests in one batch if possible." And: "You may need to prime the tubes before taking formal measurements."

**The batch** is `bench/protocol.py`: tonight's recordings as one fixed list of twenty steps, both tubes primed first (the fine tube 15 turns, the coarse 4, each recorded), each step its own session and verdict, the cup's room checked before each. About two hours and 590 gr, so one cup. The recorder now keeps the unit's own step count with every trial, so the tube's place in its turn can be set beside another session's.

**Assumptions, each with what the recording will show if it holds.** The figures for AR2209 are tonight's on this unit. BM2's bulk density and kernel are from memory and from the owner's "smaller kernel", not looked up: the kernel taken as about half of AR2209's 0.021 gr.

| | Assumption | AR2209 here | BM2 if it holds |
|---|---|---|---|
| 1 | The grains a turn follow the bulk density | fine 0.757, coarse 9.8 to 9.9 | within 5% of the same: the two powders' densities differ by about 2%, so this pair cannot test it sharply |
| 2 | What a turn pours is still set by the turn and not the speed | the same digits from a move at 0.05, 0.2 and 1 rev/s | the same |
| 3 | A move's spread is a few kernels, so it is smaller in grains for a smaller kernel | 4.4 to 4.7 digits at half a turn (about 4 kernels) | about 2.5 digits at half a turn |
| 3b | The rival: the spread is a volume at the lip, the same in grains | | 4 to 5 digits again |
| 4 | A short move pours nothing less often | a move of 0.03 turn: nothing two times in three | under half the time |
| 5 | What a stop from the middle speeds leaves is a few kernels | 4 to 6 digits beyond the delay, from 0.2 to 0.5 rev/s | 2 to 3 digits |
| 5b | The rival: it is a layer of powder in motion, the same in grains | | 4 to 6 digits again |
| 6 | The once-a-turn wave is the tube's | a wave of half a spread | a wave again, and at the same place in the turn by the step count if the tube was not taken out and the unit not restarted |
| 7 | Slow turning is less lumpy (the owner: less clump) | variance over mean 1.9 to 3.4 at 0.02 to 0.05 rev/s | under 1.5 |
| 8 | The scale's answer to a small change is the scale's | one digit first shows after a median 1.95 s | the same within 0.4 s |
| 9 | What a reverse costs is the tube's | levels off at a third of a turn | the same within 0.1 turn |

**Next (fitting, not begun, by the owner's word "before working on any fitting"):** (1) `measure.py` and the simulation's twin take the new tests (stops to 1 rev/s, the ramps, the reverse lengths, the watched nudges); (2) the record built again with what follows a stop at every speed and what a reverse costs; the throw-shaped ramp and the three sets of throws as its checks; (3) the scale's small-change shape from point 8; (4) the capture protocol as one script for the second unit and the other powders; (5) the design.

## 2026-11-21, 01:50: ADI BM2 recorded in one batch; the nine assumptions judged

**What was run.** `bench/protocol.py` on the Pico 2 W (2.0.3-84, eaea601), the A&D at FAST, the tubes cleared of AR2209 and the unit restarted first (the owner). Steps 3 to 16, the fine tube, 23:52 to 01:34 (833 trials); the cup emptied by the owner; steps 17 to 20, the coarse tube, 01:46 to 01:49 (24 trials). Every step PASS. Every figure below is in `research/bench_2026-11-21/bm2_batch.txt`, made by `bench/batch_look.py`; the raw folders are under the checkout's `logs/adr009/`, named in it. Nothing on the unit was stored or changed. No fitting and no design was done (the owner: "focus on the data capture").

**Priming: the owner's warning held, and fixed turns are not a priming.** The fine tube, cleared, poured nothing for 4 turns and 0.27 gr a turn over its first 15; the coarse tube 5.97 gr a turn over its first 4. The batch was stopped and the fine tube turned in runs of 10 turns until two poured the same (0.553 and 0.532 gr a turn). The coarse tube was primed just before its own steps (8.06, 8.25, 8.14 gr a turn over three runs of 4 turns), since a tube that has stood through the fine tube's hour and a half is not the tube that was primed. `protocol.py` has `--to` for this, and says so at its top. The batch's own two priming steps are kept only as a first fill.

**Two faults of the session, both the agent's, both the same trap.** A recorder that loses its console leaves the motor at its last speed: the firmware turns until told otherwise. (1) The first batch, stopped from outside, left the fine tube at 0.2 rev/s for about 5 s. (2) A background reader of the scale was making its last reading as the coarse priming began; the two console sessions collided, the priming session lost its connection with the coarse tube at 1 rev/s, and it turned unrecorded for 16 s (122 gr) until stopped. One console session at a time: a background session is seen to have exited before another starts, and after any session that ends badly `motor fine 0` and `motor coarse 0` are sent at once.

**The nine assumptions, written before the first run, against what was recorded** (the fine tube unless said):

| | Assumption | AR2209 | BM2 | Verdict |
|---|---|---|---|---|
| 1 | The grains a turn follow the bulk density | fine 0.757; coarse 9.4 to 9.9 | fine 0.548; coarse 8.0 to 8.35 | **Fails.** 28% less in the fine tube, 16% less in the coarse: not one factor for a powder either |
| 2 | A turn's powder is set by the turn and not the speed | the same from a move at 0.05, 0.2 and 1 rev/s | 0.511 to 0.535 gr a turn from 0.02 to 0.5 rev/s; 0.561 at 1 rev/s; steady runs 26.1 to 26.6 digits a turn, 28.2 at 1 rev/s | **Holds,** with about 6% more a turn at 1 rev/s |
| 3 | A move's spread is a few kernels, so smaller in grains | 4.4 to 4.7 digits at half a turn | 2.9 to 3.1 | **Holds roughly** (2.5 was expected) |
| 3b | The rival: the same in grains | | | **Fails** |
| 4 | A short move pours nothing less often | 0.03 turn: nothing 60 to 67% of the time | 62 to 71% | **Fails:** no change |
| 5 | A stop from the middle speeds leaves a few kernels beyond 0.6 s of the flow | 5.8, 4.3, 6.3 digits from 0.2, 0.3, 0.5 rev/s; 0.5 from 1 | 2.1, 2.0, 4.9; 8.5 from 1 | **Neither 5 nor 5b:** it grows with the speed (below) |
| 6 | The once-a-turn wave is the tube's | 0.52 spreads, chance 0.09 | 0.51 spreads, chance 0.11 | **Holds.** Its place in the turn cannot be compared: the unit was restarted |
| 7 | Slow turning is less lumpy | variance over mean of the digits in a second at 0.05 rev/s: 1.7 to 1.9 | 1.5 to 2.2 | **Fails** over a second. Over a quarter turn and more it is evener: a fixed move's variance over mean 0.5 to 0.8 against 1.0 to 1.7 |
| 8 | The scale's answer to a small change is the scale's | one digit first shows after a median 1.95 s; two after 1.17 s; five after 0.81 s | 2.22 s; 1.52 s; 0.81 s | **Holds** (within 0.4 s) |
| 9 | What a reverse costs is the tube's | 0.12, 0.23, 0.36, 0.33 turn for 0.05, 0.1, 0.25, 0.5 turn back | 0.37, 0.43, 0.51, 0.59 turn | **Fails:** it costs more and does not level off by half a turn back |

**Not predicted, and found:**
- **The delay from the tube to the reading is longer with BM2.** The reading trails the tube's speed by 0.80 to 0.85 s in the three ramps (AR2209: 0.60 to 0.65 s); a speed step shows in the reading's slope 0.83 to 0.92 s after its command; and what arrives after a stop is 0.88 to 1.06 s of the flow at every speed from 0.05 to 1 rev/s. So part of what the record calls the scale's delay is the powder's way from the tube to the pan, and it differs by powder. AR2209's after-stop was nearer a fixed amount (1.44 s of the flow at 0.2 rev/s, 0.61 s at 1); BM2's is nearer a fixed time.
- **The throw-shaped ramp with no controller** leaves 3.0 digits after its stop (sd 1.8); AR2209 left 4.5 (sd 2.2).
- **A reverse quarter turn at a stop spills more with BM2:** 1.6 to 3.0 digits more than a plain stop from 0.05 to 0.3 rev/s (AR2209: 0.7 to 0.8).
- **On 3, a third reading the two powders cannot tell from "a few kernels":** a half-turn move's spread is about a quarter of what it poured for both (3.0 of 13 digits; 4.5 of 18).
- **The coarse tube's fixed moves are as wide in grains as AR2209's:** a quarter or half turn is 8.5 to 19 digits off (AR2209 11.6 to 16.9), five moves a cell.

**As found, not looked into:** the unit counted 47 late feeds of the fine motor's step pulses over the session (16.7 million steps; none of the coarse motor's); when they fell is not known. `stats` shows one frame of the scale that did not parse. BM2's kernel mass is not measured: every "variance over mean" in the looks is in AR2209's kernel (0.021 gr), so those figures compare the two powders only as spreads in digits; the owner was asked for the weight of 50 or 100 kernels, at leisure.

**Looked up after the run (02:10), on the owner's word that "BM2 kernel data can be found from either quickload or GRT".** Neither program is on this PC, so their own tables were not read. What was found of them: QuickLOAD's powder record is the heat of explosion, the ratio of specific heats, the burning rate factor, two burn-shape factors, a factor b, the solid density and the bulk density (a forum thread listing its inputs); GRT's shared files hold the same kind (`research/sources/grt_powders.csv`: `pc`, `pcd`, the burn coefficients). No kernel mass and no kernel size in either, as far as seen. A third-party table gives Hodgdon Benchmark (the same powder) a solid density of 1.620 g/cc and a bulk density of 0.906 g/cc, against 0.88 to 0.93 for AR2209 (`research/sources/powders_physical.csv`). No published size or mass of a Benchmark kernel was found in five searches. So: assumption 1's densities, taken from memory, are confirmed as near equal (within about 3%), and its failure stands on looked-up figures: two powders of the same bulk density differ by 28% in what the fine tube's turn pours. And what these programs publish for a powder (its densities) does not give what a turn pours; the kernel's mass still has to be weighed or found elsewhere.

**BM2's kernel, counted by the owner (02:30).** The owner: "Do you need precise number? After a clear cup I can count the small number of kernels if you can drop some." Within 10% is enough: the mass only turns digits into kernels. Half a turn of the fine tube at 0.2 rev/s into the emptied cup (`20261121_022935_kernelcount`; 0.508 turn by the step count) left 0.14 gr, steady; the owner counted **12 kernels**. So **0.0117 gr a kernel** (0.0108 to 0.0125 with the reading's half digit), 0.56 of the 0.021 gr taken for AR2209, which is a community figure and not this lot's. The drop was half of what a primed tube's half turn pours (0.27 gr): the tube had stood 40 minutes. With it:
- one kernel is 0.58 of the scale's digit, and 0.021 of a turn of the fine tube (AR2209: 1.05 digits, 0.028 of a turn);
- a half-turn move's spread, 3.0 digits, is 5.1 kernels (AR2209: 4.5 digits, 4.3 kernels): assumption 3, "a few kernels", holds in kernels as well as in digits;
- what a stop from 0.2 to 0.3 rev/s leaves beyond the delay, 2.0 to 2.1 digits, is 3.5 kernels (AR2209: 4.3 to 5.8 digits, 4.1 to 5.5 kernels);
- a watched nudge gave 0.58 digits, one kernel, on average, and showed nothing 70% of the time: a single BM2 kernel is less than a digit and often does not show. So assumption 4's verdict ("a short move pours nothing as often as with AR2209") is of what the scale shows, not of what fell: with this powder the two differ.
The looks' "variance over mean" for BM2 are to be multiplied by 1.8 (0.021 over 0.0117) to be in BM2's own kernels.

**AR2209's kernel stays 0.021 gr, on the owner's word (02:38):** "For AR2209, 0.021gr is about the weight per kernel. So I don't think it worth a weight." The owner then asked for an interim conclusion and the next step for a full analysis: the next step is at the top of `research/HANDOVER.md` ("Tomorrow's order"); the interim conclusion, with the talk that followed it, is in `research/talk_2026-11-21.md`, which the owner asked to be kept "only as the record, and not to be directly cited as the reference".

## 2026-11-21, 03:00: the two powders read together (the owner: "What can we learn from today's data so far, combined with two powder?")

No new recording; the figures are those above, put in each powder's own kernel (AR2209 0.021 gr, BM2 0.0117 gr) and against the flow in digits a second. **Two readings below revise what was written earlier tonight; they are leads for the fit to test, not results.**

**1. In kernels the two powders scatter alike.** A fixed move of a quarter to half a turn: variance over mean, in the powder's own kernels, 1.0 to 1.7 for AR2209 and 0.95 to 1.3 for BM2 (the looks' 0.53 to 0.73 times 1.8). So kernels leave nearly one by one at random, a little bunched, and a move's error is about the square root of 1.0 to 1.7 times the kernels it pours, levelling at 4 to 5 kernels past half a turn (AR2209 4.3, BM2 5.1). A powder's scatter in grains follows from its kernel's mass. The shortest moves are more bunched for both (2 and more).

**2. The step for a powder is two measured numbers, not a tuned one.** Kernels a turn of the fine tube: AR2209 36, BM2 47; so one kernel is 0.028 and 0.021 of a turn (about 790 and 600 microsteps). The turn that is one kernel is the kernel's mass over the grains a turn.

**3. What arrives after a stop, as seconds of the flow before it, lies on one curve against that flow for both powders and both tubes** (`after_stop.py`; the coarse tube's are single stops):

| Flow before the stop, digits/s | AR2209 | BM2 |
|---|---|---|
| 1 to 3 (fine, 0.05 to 0.1 rev/s) | 1.71, 0.96 | 1.06, 1.05 |
| 6 to 10 (fine) | 1.44, 1.06 | 0.97, 0.88 |
| 13 to 27 (fine) | 0.96 | 0.97, 0.92 |
| 41 (fine, 1 rev/s) | 0.61 | |
| 125 to 490 (coarse, 0.3 and 1 rev/s) | 0.53, 0.54 | 0.62, 0.50 |

And the watched nudges give the same scale's first answer to a sudden change: 0.8 s for five digits, about 2 s for one. **The lead:** the lag from tube to reading is not one number; it falls from about 2 s for a single digit to about 1 s for a slow stream and 0.5 s for a fast one, and it is mostly the scale's, since two powders and two tubes share it. It is the owner's note of 11-20 (a stream of change shows sooner than one small change), seen on the A&D in figures.
- **This revises 01:50's "the delay from the tube to the reading is longer with BM2 ... part of the delay is the powder's".** At the same speed BM2 flows at 28% fewer digits a second; at the same digits a second (13 to 27) the two powders agree (0.96 against 0.92 to 0.97). BM2's speed steps fit a plain lag of about 0.9 s: a step up by 27 digits/s gave 6.3 digits less than a change at 0.6 s would, a step down 7.7 more.
- **It puts 11-20's "a stop from 0.2 to 0.5 rev/s leaves 4 to 6 digits beyond the scale's 0.6 s delay, and the next start owes them" in question.** Counted against 0.95 s of the flow in place of 0.6 s, AR2209's stops leave +3.3, +1.0 and +0.3 digits from 0.2, 0.3 and 0.5 rev/s, and BM2's +0.1, -0.5 and +0.3. What then remains as the powder's own is AR2209 at its slow speeds (0.05 and 0.2 rev/s: 1 to 3 digits more than BM2), which fits its clumping. Either way the simulation was kind for the same reason, too little arriving after a stop at these flows; but the mend differs: a scale whose lag depends on the flow, or powder left at the lip.
- **Not settled by these recordings:** the fine tube's flows and the coarse tube's do not overlap except at AR2209's one point (41 digits/s), so a lag that is the fine tube's own cannot be ruled out. **The desk test:** the scale is the same in both sessions, so the scale's part of the record must come out the same from AR2209's recordings and from BM2's; a model that needs two scales for one scale is wrong. A bench test, when a powder is next loaded: the two tubes at the same digits a second (the coarse at 0.05 rev/s, the fine at 1 to 1.5), stops from each.

**4. What stays the powder's (with this tube):** the grains a turn (the same bulk density, 28% apart, so the bed the fine tube carries is shallower with BM2: 0.039 cc a turn against 0.054), what a reverse costs, and AR2209's bunching at slow speeds.

**5. What two powders on one unit and one scale can and cannot separate.** They separate the scale from the powder (point 3's test). They cannot separate the tube from the powder: that is the second unit's. A third powder with AR2209's kernel (AR2208, 0.020 gr) tests whether the grains a turn go with the kernel's size.

**For the owner's question, how a powder relates to the step and how the step is set for a powder: what this pair of powders says must be measured on the unit, for each powder, and what carried over.** Measured for each powder: the grains a turn of each tube (not from a density table, and not one factor for both tubes), the delay from the tube to the reading, what a stop leaves, and what a reverse costs. Carried over unchanged: the turn and not the speed sets the powder; the once-a-turn wave; the scale's answer to a digit; the share of the shortest moves that pour nothing. No design follows from it here.

**The bench as left (01:49):** Pico 2 W on the probe, idle, both motors at 0, BM2 in the hopper and both tubes; 391.7 gr in the cup.

## 2026-11-21, about 12:30: is what arrives after a stop the scale's or the powder's? A desk test, its pass written first

*This heading and the next four first named a time of day (daytime, afternoon, evening, night, late night) that was not the clock's: all five were written between 12:30 and 14:50. They are set to the times of their commits.*

No unit. Another agent took over from the handover. **The recordings are now in the repository** (`tools/charge_sim/recordings/`, copied by `bench/keep.py`, with an index and each session's unit, powder and scale) and the branch is on GitHub: the owner, "push the record to the github for future analysis, by me or other people". The console's own logs are left out; the settings copy is without its Wi-Fi, update source and id. `measure.py` gives the same figures from the copy as from the bench's folder.

**The question** is 03:00's lead, and the owner said to test it before anything is built on it. The simulation is kind at the finish because too little arrives after a stop. Either the scale's lag is not one number (longer for a trickle than for a stream), or powder is left at the lip and falls after the stop, owed at the next start. The first is mended in the scale, the second in the tube. Both powders were weighed by the same scale, which is what the test leans on.

**Three checks, from the recordings alone, with no model.** What arrives after a stop is the reading 2.4 to 3.0 s after it less the reading at it (every stop was watched 3 s at least), in digits; only plain stops with the driver held, the fine tube.

1. **At the same flow.** If it is the scale's, a stop of BM2 looks like a stop of AR2209 from the same flow in digits a second, whatever the tube's speed. For each speed BM2 was stopped from, three forecasts of what arrives, each from AR2209's stops alone:
   - **the scale's:** AR2209's figure at BM2's flow, on a straight line between AR2209's two nearest flows (none arrives at no flow);
   - **the powder's, the same in grains:** AR2209's figure at the same speed, less the plain delay's share of the difference in flow;
   - **the powder's, the same in kernels:** as the last, what is beyond the plain delay scaled by the kernels' masses (0.0117 over 0.021).
   The plain delay is the coarse tube's (what arrives after its stops over its flow), where nothing was found beyond it.
   **Pass for the scale:** its forecasts are off by a digit or less (root mean square over the speeds), none is off by more than two digits and more than twice its standard error, and it is the nearest of the three. A digit, because a digit is the charge's tolerance: a model that is a digit wrong after a stop cannot judge a finish.
2. **The trial before.** If it is the scale's, what arrives after a stop cannot depend on the trial before it: the reading had rested. For each powder and speed, the stops that followed a faster trial against those that followed a slower or equal one. **Pass for the scale:** at no speed do the two differ by more than a digit and more than twice the difference's standard error. (Of about a dozen comparisons, chance alone gives one past twice its error in two sets like this, so one such is read with its neighbours.)
3. **The start after.** If powder is owed, a run begins short, and shorter after a trial that left more. The reading's gain over a run's first 2 s, split the same way. **Pass for the scale:** as in 2.

**The verdict:** the scale's part alone stands if 1 passes and 2 and 3 show nothing. If 1 fails, or 2 or 3 show the trial before, a part is the powder's and is built into the tube (the handover's step 3).

### Run: the scale alone fails; what arrives after a stop stays a measured table of each powder

`bench/scale_or_powder.py --first pico2w_ar2209 --second pico2w_bm2`; its figures are `research/bench_2026-11-21/scale_or_powder.txt`.

| Fine tube | AR2209: stops, flow d/s, arrived (its error), s of the flow | BM2: the same | BM2 forecast from AR2209 at the same flow, and off by |
|---|---|---|---|
| 0.02 rev/s | 31, 0.81, 0.42 (0.17), 0.52 | 21, 0.42, 1.05 (0.27), 2.47 | 0.22, +0.83 |
| 0.05 | 47, 1.73, 2.45 (0.36), 1.41 | 33, 1.25, 1.36 (0.24), 1.09 | 1.38, -0.02 |
| 0.1 | 47, 3.48, 3.09 (0.36), 0.89 | 33, 2.71, 2.52 (0.30), 0.93 | 2.80, -0.29 |
| 0.2 | 17, 7.16, 9.65 (0.89), 1.35 | 13, 5.39, 5.54 (0.37), 1.03 | 6.49, -0.96 |
| 0.3 | 15, 9.44, 9.93 (0.95), 1.05 | 12, 7.39, 6.42 (0.50), 0.87 | 9.68, -3.26 |
| 0.5 | 16, 19.2, 16.9 (0.94), 0.88 | 13, 13.3, 12.5 (0.83), 0.94 | 12.7, -0.22 |
| 1 | 16, 40.0, 25.1 (1.9), 0.63 | 13, 27.5, 24.4 (0.53), 0.89 | 20.2, +4.21 |

- **Check 1 fails for the scale.** Its forecasts are off by 2.07 digits (a digit allowed), two of them past two digits and three times their error (0.3 and 1 rev/s). It is the nearest of the three all the same: the powder's forecasts are off by 2.92 (the same in grains) and 2.87 (the same in kernels). So neither "it is all the scale's" nor last night's two ways of putting it on the powder forecasts a second powder to a digit.
- **Check 2 passes, check 3 passes, and neither could have seen much.** At no speed does a stop, or a start, differ by the trial before it by more than a digit and twice its error; but those errors are 0.5 to 2 digits, so a part of one or two digits would not have shown. AR2209 at 0.2 rev/s leaves 10.4 after a faster trial and 7.8 after another (the figure of 11-20), with an error of 2.0.
- **A start is short by what a stop leaves** (the tool's second table): by its first 2 s a run is behind its flow by 2.3, 4.1, 6.6, 11.6, 18.1 and 26.0 digits from 0.05 to 1 rev/s (AR2209), where the stops leave 2.5, 3.1, 9.7, 9.9, 16.9 and 25.1; BM2 the same way. Any delay between the tube and the reading gives that, the scale's or the powder's. **So 11-20's "what falls is owed at the next start" is not a finding of its own:** the recordings do not tell powder owed from a reading that is late.

**What the recordings do say, as figures:**
- **BM2 is a plain delay.** From 0.05 to 1 rev/s (1.3 to 27.5 digits a second) 0.87 to 1.09 s of its flow arrives after a stop, a stop 1.3 to 3.0 digits off. One number.
- **AR2209 is that on average and uneven by speed.** 0.88 to 0.89 s at 0.1 and 0.5 rev/s; 1.35 s at 0.2 and 1.41 s at 0.05 (3.2 and 0.9 digits more than 0.9 s gives); 0.63 s at 1 rev/s. From 0.05 to 0.5 rev/s a stop is 2.4 to 3.8 digits off, and from 1 rev/s 7.6: its sixteen stops left 11 to 37 digits, where BM2's thirteen left 22 to 29.
- **Every curve rises at its full flow for the first 0.55 to 0.6 s after the stop,** both powders, both tubes: that much is a delay all share. The coarse tube's stops end there (0.58 s of its flow, 21 stops). What differs is what comes after it.

**Read after the result, so a lead and not a finding:** one curve would still fit both powders if the scale answers sooner above a flow somewhere between 27.5 digits a second (BM2 at 1 rev/s, 0.89 s) and 40 (AR2209 at 1 rev/s, 0.63 s; the coarse tube from 49 up, 0.51 to 0.57 s), with AR2209's 1 to 3 digits more at 0.05 to 0.3 rev/s as its own. No recording lies in that gap with both powders: BM2's fine tube was never turned above 1 rev/s, AR2209's never stopped between 0.5 and 1, and the coarse tube at 0.1 rev/s with BM2 is one lumpy stop. **The recording that settles it,** at the next powder loaded: the fine tube stopped from 0.7, 1.5 and 2 rev/s, fifteen stops each, beside the batch's own 0.5 and 1. If the seconds of the flow fall at the same digits a second for the powder loaded as for these two, the step is the scale's.

**What follows for the simulation, by the rule written first:** what arrives after a stop is kept as a measured table by speed for each powder and tube, over a scale with the delay all the curves share. That is the handover's step 3 as it was written, and `plant.py` already has the place for it (`after_stop_kernels`, to 1 rev/s now). No scale of a new shape is built. **What stays open, and for whom it matters:** how much of that table is the scale's. For one unit with one scale the throws come out the same either way; it decides only how the table is carried to another scale, which is the owner's question on other makes, and the three stops above answer it.

## 2026-11-21, 12:50 to 13:20: a record for each powder; the pass written before the build

**The owner: "Yes please go on"**, with GRT's pages for both powders and a copy of the program.

**GRT, read first.** Its database file is encrypted and was not opened; the program was not run. Its own documentation lists a propellant's fields (`doku/en/doku/file_propellant.txt`, `inputfields-propellant.txt`): burn coefficients, the heat, the material density and the bulk density. No size and no mass of a kernel. The owner's two pages: AR 2209, material 1560 kg/m3, bulk 990 kg/m3; BM 2, material 1540, bulk 980. So the two bulk densities are 1% apart by GRT's own figures (the third-party table of 11-21 had them within 3%), and the fine tube's 28% between the two powders is not in them: 0.757 gr a turn is 0.050 cc of AR2209, 0.548 gr is 0.036 cc of BM2. With the material densities a kernel's volume follows from its mass: 0.87 mm3 for AR2209 (0.021 gr), 0.49 mm3 for BM2 (0.0117 gr). The page has two more tabs, "Picture" and "Detail Picture", not seen here: if a picture carries a scale, a kernel's length and width can be read off it and its mass forecast from the material density, which is the owner's "predict the kernel from published data". Asked of the owner.

**A correction to this morning's last paragraph.** It said `plant.py` "already has the place" for what arrives after a stop. It did not: its place dropped a random count of kernels at a full stop and owed them to the next start. The recordings are not that shape. BM2's stops from 1 rev/s leave 24.4 digits within 1.9 of each other, where a random leftover of that size would scatter by as much as itself; every curve rises at its full flow for 0.55 to 0.6 s and then tails off over about another 0.6 s, as a flow spread evenly over that time does; and the same lateness is there when the tube only slows (the ramps trail the tube by 0.80 to 0.85 s with BM2, a speed step shows 0.83 to 0.92 s after its command). **So the mechanism is replaced:** a tube has a table `late_s`, the mean seconds by which powder released at a speed shows later than the scale's own delay, each release spread evenly from nothing to twice that. It holds at a stop, while slowing and at a start alike, keeps every grain, and needs nothing owed. Whether it is the powder's way to the pan or the balance's averaging is this morning's open question; the reading is the same either way.

**What is built.**
- `measure.py` takes a session (`recordings/sessions.json`), never a folder of another powder; a session with no tapped map gets its map from its own stops; the watched nudges are the nudge test for both; the kernel's mass is the session's.
- One record a session: `pico2w_ar2209` and `pico2w_bm2`. The scale is solved once, from AR2209's session, and BM2's record takes it whole (`--scale-from`): the same balance weighed both.
- `late_s` is solved a speed at a time from what arrives after that speed's stops. The old record `bench_pico2w` and its outputs are removed.

**The check, and its pass, before any record is built.** Nothing below is used to make a record.
- **The speed plans with no controller,** three for each powder (`like_throw`: a real finish's speeds; `slow`; `slow_hold`). What arrives after the stop: AR2209 4.50 (sd 2.19), 1.97 (1.83), 1.23 (1.63) digits; BM2 3.00 (1.77), 1.27 (1.00), 1.00 (1.21).
- **AR2209's throws of today's loop,** three settings (29 throws); the slow finish is 10 over of 12, at rest +0.072 gr.
- **Tolerances are `fit_report.py`'s own, set on 11-20 before any of this:** what arrives after a stop and its spread within 0.6 digit or a quarter; a set of throws' share over within 0.17, its weight at rest within 0.03 gr or a quarter.
- **Pass:** for each powder `like_throw`'s two figures within tolerance; and for AR2209 the slow finish's share over and weight at rest within tolerance. The simulation gave about half over before, against the bench's 10 of 12: that is the figure this was built to mend. The other plans and settings are reported, each within or outside.

### Built and checked: BM2's record passes; AR2209's passes on its throws and fails on its throw-shaped ramp

`build_rig.py --session pico2w_ar2209`, then `--session pico2w_bm2 --scale-from pico2w_ar2209`; each record beside the bench in `tools/charge_sim/fit/report_<record>.txt` (`fit_report.py`, seed 1). The scale, solved once from AR2209's session: delay 0.406 s, a small change held 0.875 s.

**How late the fine tube's powder shows, mean seconds beyond the scale's delay** (`late_s`, solved a speed at a time from that speed's stops; the coarse tube's is 0.03 s at most at every speed, both powders):

| Fine tube, rev/s | 0.02 | 0.05 | 0.1 | 0.2 | 0.3 | 0.5 | 1 |
|---|---|---|---|---|---|---|---|
| AR2209 | 0 | 0.38 | 0 | 0.56 | 0.19 | 0.19 | 0 |
| BM2 | 1.47 | 0.19 | 0.03 | 0.19 | 0.19 | 0.25 | 0.25 |

BM2's is one number from 0.2 to 1 rev/s, as this morning's figures said. Its entry at 0.02 rev/s is at the end of the range looked in (1.5 s) and stands on 20 stops that left one digit each: not a measurement to lean on. AR2209's is uneven, with its most at 0.2 rev/s.

**The check** (measured | simulated; nothing here was used to make a record):

| | Arrives after the stop, digits | Its spread | |
|---|---|---|---|
| BM2, `like_throw` | 3.00 \| 3.56 | 1.77 \| 1.32 | **within** |
| BM2, `slow_hold` | 1.00 \| 1.42 | 1.21 \| 1.12 | within |
| BM2, `slow` | 1.27 \| 1.92 | 1.00 \| 1.02 | outside by 0.05 digit |
| AR2209, `like_throw` | 4.50 \| 6.59 | 2.19 \| 3.19 | **outside** |
| AR2209, `slow_hold` | 1.23 \| 1.32 | 1.63 \| 1.76 | within |
| AR2209, `slow` | 1.97 \| 2.58 | 1.83 \| 2.12 | outside by 0.01 digit |

| AR2209, today's loop | Share over | At rest, gr | Its spread, gr | Time, s |
|---|---|---|---|---|
| Slow finish (12 throws) | 0.83 \| 0.82 **within** | +0.07 \| +0.10 **within** | 0.05 \| 0.11 outside | 11.3 \| 10.7 |
| Fine gain 0.8 (12) | 0.83 \| 0.97 within | +0.21 \| +0.27 outside by 0.01 | 0.12 \| 0.18 outside | 4.6 \| 5.0 |
| As stored (5) | 1.00 \| 1.00 | +1.33 \| +1.36 | 0.09 \| 0.30 outside | 6.2 \| 6.6 |

**By the pass written first:** BM2 passes. AR2209 passes on the slow finish, the figure this was built to mend (the same record with no lateness gives 0.47 over, the old simulation's figure), and fails on `like_throw`: the simulation leaves two digits too many after a tube slowed through a finish's speeds.

**Where AR2209's two digits come from: one entry, 0.2 rev/s** (a diagnosis; no record was changed by it). The same record with that one entry at its neighbours' 0.19 s: `like_throw` 5.32 (spread 2.27), within; the slow finish 0.76 over, at rest +0.08 (spread 0.07), all within; and then its own stops from 0.2 rev/s read 7.1 digits where the bench's fifteen left 9.9. So a stop from a steady 0.2 rev/s leaves about three digits that a tube passing through 0.22 rev/s for half a second does not. That part is not a lateness of the powder released at that speed, and the table cannot hold both.

**A lead on what it is, from 17 stops, so no more than a lead:** the stops from 0.2 rev/s were shuffled with stops from 0.5 and 1. The ten that came a few seconds after a faster trial left 11.0 digits; the five after a trial at 0.2 left 7.8, and the two after 60 and 90 s of steady turning left 8 and 7: seven stops at 7.7, which is what the neighbours' lateness gives. The difference is twice its error. BM2 shows none (5.5 against 5.6). And AR2209's stops from 1 rev/s leave less than any lateness accounts for (0.63 s of the flow) and scatter from 11 to 37 digits. Read together: with this clumping powder a fast run's stop keeps some powder at the lip, and the next trial pours it. If so, the entry at 0.2 rev/s is the test's order and not the tube's lateness, and this morning's check 2, which found nothing past twice its error, was too weak to see it.

**The recording that settles it,** at the next AR2209 loaded (fresh AR2209 is on the owner's list): the stops of each speed in a block of their own, the block's first stop not counted, in place of shuffled speeds. `protocol.py` shuffles today. It goes in with the stops from 0.7, 1.5 and 2 rev/s and the throws.

**Also outside, in the records' own recordings** (100 of 118 figures within for BM2, 106 of 147 for AR2209): the spread of what a stop from 1 rev/s leaves (BM2 1.8 measured, 6.5 simulated: the releases at 1 rev/s are solved too lumpy from one 20 s run); AR2209's flow in the first seconds of a run from 0.1 to 0.3 rev/s (the simulation starts quicker than the bench); and a throw's spread at rest for AR2209, twice the bench's at every setting.

**Where this leaves the handover's order.** Step 4 (two records, each checked) is done: BM2's record can be designed on; AR2209's gives the right share over for today's loop but is two digits harsh where a finish slows through 0.2 rev/s, and its spread at rest is double, so a figure from it is quoted with that said. Step 5 (the owner's questions by simulation) is next and needs no bench.

## 2026-11-21, 13:30: the owner's two tolerances; where today's loop stands and what a finish by small moves could reach

**The owner:** "Yes please go on, and remember, don't over do ... AR2209 throw is much harder than BM2 due to its kernel size. In most cases +/-0.02 gr is acceptable, and I'd accept +/- 0.04 gr for loads that doesn't need to be that accurate." (The README has it as direction.) With two pictures of the kernels from GRT, "mostly visual based": `research/sources/grt_two_powders.md` has what they give, a shape and not a mass to the tenth wanted. No unit; nothing new is built into the simulation here, and no controller is designed.

**1. What is there today, on each record, at both tolerances** (`bands.py`; 500 throws at 20 gr, of the true weight; `fit/bands.txt`). Today's loop with the three settings thrown on the bench on 11-20, and `flow_dose`, the candidate written before any bench session:

| | Good within 0.02 gr | Over | Good within 0.04 gr | Over | Error, gr (spread) | Time, s |
|---|---|---|---|---|---|---|
| **BM2**, today's loop as stored | 0% | 100% | 0% | 100% | +1.06 (0.14) | 6.0 |
| BM2, fine gain 0.8 | 50% | 50% | 74% | 26% | +0.04 (0.06) | 8.8 |
| BM2, fine gain 0.8 and a slow finish | 79% | 20% | 90% | 10% | +0.02 (0.06) | 15.3 |
| BM2, `flow_dose` | 68% | 32% | 77% | 23% | +0.05 (0.15) | 6.6 (one in twenty over 23.7) |
| **AR2209**, today's loop as stored | 0% | 100% | 0% | 100% | +1.42 (0.28) | 4.2 |
| AR2209, fine gain 0.8 | 4% | 96% | 7% | 93% | +0.29 (0.23) | 4.8 |
| AR2209, fine gain 0.8 and a slow finish | 15% | 85% | 26% | 74% | +0.11 (0.11) | 10.6 |
| AR2209, `flow_dose` | 2% | 98% | 2% | 98% | +0.70 (0.34) | 4.5 |

- **BM2's rows are a forecast:** no throw of today's loop was recorded with BM2 (the handover's step 1). They are written here before any such throw. AR2209's slow finish is the one the bench threw (10 over of 12); its record is two digits harsh where a finish slows through 0.2 rev/s (this afternoon), so its rows lean to the harsh side.
- **The same settings are 79% good with BM2 and 15% with AR2209.** The owner's "much harder" in figures: AR2209 pours 38% more a turn and its kernel is a whole digit, so the same least speed leaves more unseen at the stop, in larger pieces.
- **The looser band mends BM2 (90%) and not AR2209 (26%):** AR2209's overs are a tenth of a grain, not a digit.
- **`flow_dose` is not the candidate to go on from:** worse than the slow finish with both powders.

**2. What a finish by small moves and waits could reach, from the recorded nudges alone** (`bench/topup_bound.py`, no model; `research/bench_2026-11-21/topup_bound.txt`). The finish ends short and adds moves of the fine tube until the settled reading is in the band; each move's gain is drawn from the session's own watched nudges (0.017 of a turn, the reading 5 s after). It knows each gain exactly and never moves once in the band, so what is over is the powder's doing and no controller's.

| | Within 0.02 gr: good, over | Within 0.04 gr: good, over | Moves from 3 digits short (mean; one in twenty over) |
|---|---|---|---|
| BM2 (240 nudges) | 88 to 90%, 10 to 12% | 99%, 1% | 5.6; 15 |
| AR2209 (300 nudges) | 80 to 81%, 19 to 20% | 92 to 94%, 6 to 8% | 5.0; 13 |

A nudge gave nothing two times in three; one in sixteen read a digit less (the balance's own flicker), and with AR2209 one in eighteen gave four digits or more (BM2: one in twenty-seven). At a wait of 2 s a move, which shows half of the single digits, three digits short costs 10 s; at 4 s, which shows nine in ten, 20 s.

**What the two tables say together**
- **Within 0.02 gr, one over in fifty is out of reach of a finish by moves of this size with either powder:** one over in five with AR2209 and one in ten with BM2 is the best such a finish does, and it is the size of what a move releases that sets it. The owner's own figure for his trickler today, one over in ten to fifteen, is in that range.
- **Within 0.04 gr it is in reach for BM2** (one in a hundred) **and near for AR2209** (one in fifteen).
- **With BM2 today's loop is already near that best** when it finishes slowly (79% against 88 to 90%), at 15 s a throw. **With AR2209 it is far from it** (15% against 80%): that gap is the controller's, not the powder's, and is what a better finish can win.
- **A finish by moves and waits is slow:** three digits short is 10 to 20 s by itself.

**Not said by these:** what a move of another size or kind releases. The steady stream is even where the nudges are not (variance over mean of the kernels 0.9 at 0.2 rev/s against 3 to 5 in nudges, 11-20), and a stop from the slowest speeds scatters by 0.9 to 1.4 digits; a finish that stops a stream at the right count of turns may do as well as the moves, sooner. That is the next thing to try in the simulation. And one over in fifty within 0.02 gr asks for smaller releases, which is the owner's oscillation: a question for the tube and the firmware, not for the controller.

### The owner's question: does that finish count the scale's longer lag for a single small change? It did not; with it

**The owner:** "Does the nudge styled finish considered the longer lag from the scale due to its inconsistent weight change?" No: the table above is the kindest finish, which knows each move's whole gain (the reading 5 s after) and charges the scale's lag only as a wait a move. `topup_bound.py` now draws the same finish deciding after a wait: of every move so far it sees only what the scale had shown by then (each recorded nudge's own readings, every quarter second), moves again while that is short of the band and stops when it is in. What shows late is then moved for twice or arrives after the stop. From 3 digits short, of the true weight (`research/bench_2026-11-21/topup_bound.txt`):

| Wait after each move | BM2 within 0.02 gr: good, over | AR2209 within 0.02 gr | BM2 within 0.04 gr | AR2209 within 0.04 gr | The finish alone: mean; one in twenty over (BM2, 0.02 gr) |
|---|---|---|---|---|---|
| 0.5 s | 53%, 43% | 48%, 46% | 80%, 14% | 71%, 22% | 4 s; 9 s |
| 1 s | 70%, 25% | 63%, 31% | 89%, 5% | 80%, 12% | 7 s; 16 s |
| 2 s | 82%, 15% | 74%, 22% | 93%, 2% | 84%, 9% | 12 s; 30 s |
| 3 s | 85%, 13% | 77%, 20% | 95%, 1% | 86%, 8% | 17 s; 45 s |
| 4 s | 87%, 12% | 78%, 20% | 96%, 1% | 88%, 8% | 23 s; 60 s |
| Each gain known | 88%, 12% | 80%, 20% | 99%, 1% | 92%, 8% | |

(What is neither good nor over is under: 1 to 8%, a reading that flickered into the band.) Of the nudges that gave something, the A&D had shown the whole of it by 1 s for 7 to 14%, by 2 s for 47 to 61%, by 3 s for 75 to 81%, by 4 s for 93 to 95%.

- **To come within two points of the best, a move must be waited 4 s:** the last three digits then cost 19 to 23 s on average and a minute one time in twenty. At 2 s a move the finish gives up 6 to 7 points and still costs 10 to 12 s.
- **So the bench's A&D makes a finish by moves and waits either slow or worse than the table above, and it is the scale's answer to one small change that does it,** as the owner's note of 11-20 said. A balance that takes 5 s and more for a small change (the owner's G&G JJ223BF) would be worse again; by how much is not known here, since no nudge was ever recorded on one. That is an input the research lacks: the same watched nudges on that scale would give its row.
- **It strengthens the case for the next thing to try:** a finish that leans on the stream, which the scale follows 0.6 to 0.9 s behind whatever the size, and waits for a single small change as few times as it can.

## 2026-11-21, 13:35 to 14:30: two proposals for the controller and its calibration, built and tried in the simulation

**The owner:** "There are more than 7 scale to be supported, so I'm not planning to evaluate each of them to account for individual measurement lag. Therefore the later calibration routine must account for that, and your simulation today should assume the inconsistent measurement lag subject to the weight change speed." And: "I'd like you to make two proposals ... with confident ranking from high to med. Each proposal shall include the detailed procedure for both trickle control, as well as calibration routine ... on ANY opentrickler build/material/finish, and ANY powder, and ANY scale." (The README has both as direction.)

**The proposals are `research/proposals_2026-11-21.md`:** the frame both share, each one's throw and calibration step by step, the trial's tables, what the bench itself says, what is lacking. No unit; nothing is approved. This entry is what was built and measured to write it.

**What was read first:** the nine scale drivers (2 to 21 readings a second; four frames carry a stable mark no driver reads) and the desk research of 2026-11-19 (`research/sources/dosing_control.md`, `scale_dynamics.md`), whose "bulk, dribble, jog with a learned cut-off" is proposal 1's ancestor.

**What was built** (`tools/charge_sim/`):
- `staged.py`: one controller with two finishes ("jog": proposal 1; "glide": proposal 2), and the calibration both take their numbers from, run on the simulated rig as a unit would run it. Nothing in it is set for a rig.
- `scales_like.py`: four other scales, each the measured one with its delays stretched; the controller is calibrated again on each.
- `bands.py` takes scales; `mc.py` gives each session its own seed, so each session of 25 throws has its own calibration.

**Found while building, each of which changed the design:**
1. **The forecast must give each parcel of powder the lag of the flow it was turned out at.** With one lag for the recent past (the present speed's), a slowing tube's fast powder was counted as still on its way after it had shown, and the rehearsals rested 10 to 16 digits short. With a lag a parcel they rested within two digits with no correction. This is the owner's "lag subject to the weight change speed", inside the controller.
2. **The simulated fine tube is lumpier than the bench's above 0.5 rev/s.** One AR2209 throw went 61 digits over: at 1 rev/s the simulated tube poured nothing for 2 s and then 1.4 gr at once. The bench's recordings, the reading's gain in every half second of every steady run: at 1 rev/s the least was 0.30 of the mean, the most 2.2, and the longest time with no rise 0.2 s (AR2209); the simulation's, -0.05, 4.3 and 1.4 s. At 0.5 rev/s the two agree in spread (0.36 against 0.37 of the mean; BM2 0.19 against 0.29) and the simulation's gaps are still the longer (0.7 s against 0.2). **So the trial holds the fine tube to 0.5 rev/s, and the record's releases at high speed are a fault of the plant's model to mend** (the gaps come from a release whose wait is in proportion to its own size, with no bound). Changing how even the releases are did not bring it to the bench (regularity 4 to 16, bursts halved).
3. **A stop aimed at the target has too many overs.** The stream finish aimed at the target, a digit under and two under (400 throws each): AR2209 69%, 74%, 74% good with 27%, 20%, 20% over; BM2 83%, 88%, 86% with 14%, 6%, 6%. A digit under is the rule.
4. **A throw at rest is judged on the lowest reading it shows since its last rise.** Judged on the highest, a reading flickering across the band's lower edge was called good: the jog finish with BM2 86% good and 11% under; on the lowest, 94% and none under (AR2209: 70% to 74%).
5. **A slower creep did not help** (the powder on its way at half the band): AR2209 75% against 76%, BM2 88% against 86%, 1.5 to 2 s slower.
6. **The calibration's own waits must come from the scale.** Fixed at the A&D's 4 s, the routine could not see a later scale's small changes. Four first moves, each watched up to 15 s, now set every later watch.

**The trial's tables** are in the proposals' file and in `tools/charge_sim/fit/bands_proposals.txt` (the measured scale and two later ones), `bands_late_small.txt`, `bands_holds.txt` and `bands_borrowed.txt`; 500 throws of 20 gr a row, of the true weight. In short, good within 0.02 gr and the median time:

| | BM2: jog | glide | today's slow finish | AR2209: jog | glide | today's |
|---|---|---|---|---|---|---|
| The scale as measured | 94.4%, 12.7 s | 89.0%, 11.9 s | 79.4%, 15.3 s | 73.8%, 16.6 s | 74.4%, 13.0 s | 15.0%, 10.6 s |
| Every small change held 2.5 s | 89.8%, 15.9 s | 85.2%, 15.2 s | | 77.8%, 22.5 s | 68.4%, 15.2 s | |
| A small change held 5 s, four readings a second | 89.0%, 36.7 s | 86.8%, 31.6 s | | 74.8%, 33.9 s | 67.6%, 24.6 s | |
| Twice as late in everything | 89.6%, 31.5 s | 82.2%, 28.4 s | 0.2% | 67.6%, 36.1 s | 59.6%, 25.6 s | 0.4% |
| Three times as late | 72.4%, 58.6 s | 65.0%, 45.9 s | 0% | 41.6% (42% not ended in 90 s) | 42.0% (29%) | 0% |

Overs on the measured scale: jog 4.8% and 17.4%, glide 10.8% and 24.0%, today's 20% and 85%. Within 0.04 gr, as a mode of its own: jog 98.6% in 8.9 s and 91.0% in 12.8 s; glide 98.6% in 9.2 s and 87.2% in 10.2 s.

**The scale's part carried from one powder to the other** (`bands_borrowed.txt`; the lag table measured with the other powder, the rest with its own): jog 94.2% against 94.4% (BM2) and 69.8% against 73.8%, 4 s slower (AR2209); glide 88.6% against 89.0%, 3.5 s slower, and 73.8% against 74.4%. It holds within four points: the rehearsals place the stop whatever the table says, and a table that is off costs time.

**What the calibration cost in the trial:** 10 to 12.5 minutes and 135 to 155 gr through the cup on the measured scale; 18 to 21 and 24 to 31 minutes on the two later ones. Its watches: 3.2 to 4.2 s, 8.2 to 9.3 s, 12 to 15 s.

**From the bench's own fixed moves, by size** (the moves of 2026-11-20 and BM2's batch, no model): 0.03 of a turn gave nothing 63% (AR2209) and 67% (BM2) of the time, and of those that gave, four digits and more in 14% and 31%; 0.06 of a turn gave nothing 24% of the time and four and more in 40% and 19%; 0.12 of a turn, 65% and 58%. So a move twice as large halves the empty waits and has far more large releases: the smallest move stays small.

**The ranking, and why it turned during the work.** I began with the stream finish as the first proposal: the owner's point that a stream is shown sooner than a small change, and the bench's stops from 0.02 rev/s. The trial put the jog finish first on accuracy on every scale, by 0 to 8 points, with half to three quarters of the overs; the stream finish is the quicker by 1 to 4 s on the measured scale and 3 to 10 s on later ones. A margin takes what a stream does late; a stop a digit short cannot. **Proposal 1 (confidence high) is stop short and jog; proposal 2 (medium) is glide and stop.** They are one controller with two numbers changed.

**Not done:** no throw of either on a unit; the calibration's verdict to the user (step 9) and the learning of grains a turn are designed and not tried; the shorter calibration is tried only in its lag table.

## 2026-11-21, 14:30 to 14:50: the bench trial of the two proposals prepared; its forecast and its pass written first

**The owner, asked whether to prepare the trial that runs both proposals on the real unit from the PC:** "Yes, prepare it". No unit was touched: the bench rules ask for the owner's word that the bench is ready, and it has not been given in this session.

**What was built** (`research/trial_guide.md` is the session's guide):
- **`tools/charge_sim/bench/trial.py`:** the PC as the controller. It reads the weight by the console's `state` and turns the tubes by `motor`, as the recorder does, and runs on that the code the simulation ran: the calibration's measurements once, then for each finish and band ten rehearsals and 25 throws of 10 gr. The scale is never zeroed: a throw is counted from the weight the cup rests at. It keeps the calibration and every throw as it goes and goes on from a stop (`--resume`), since the cup must be emptied twice.
- **`staged.py`'s calibration runs on any bench** (`measure_on`, `rehearse`): the simulation's, or a unit's through the console. The simulation's figures are unchanged by it (the four rows of the measured scale run again: 94.4%, 89.0%, 73.8%, 74.4%, to the last digit).
- **A made-up unit:** the simulation answering the console's commands, each taking what a command takes on the bench (18 to 26 ms). The whole trial was run against it for both powders, from a fresh start and from a kept calibration, with its comparison against the forecast.
- **`keep.py`** copies a trial's calibration and throws into the repository with its recordings.

**Its limits against a unit's own firmware, both to the proposals' disadvantage:** the PC counts the speeds it asked for and not the unit's steps, and acts once a command and not every millisecond.

**How it is kept safe:** both motors are stopped however it ends (a limit, the keyboard, a fault of its own), and before its first weight. A throw is stopped when it is not done in 120 s, when a reading is 3 gr over, when a tube has turned further than a throw can need, when the scale gives no weight or the console no reply; a step is not begun that the cup has no room for (600 gr). What it cannot cover is the recorder's own trap: a PC or a console that dies leaves the last speed turning, and the guide gives the command for it.

**A held tube from the console:** `motor <tube> 0` switches the driver off, and the recorder's stand-in for a held stop, 0.0005 rev/s, would creep a coarse tube by 0.12 gr in a throw of 25 s. The trial holds at 0.00002 rev/s (coarse) and 0.0001 rev/s (fine). Read in `remote_control.c` and `motors.c`: the console takes any speed within the maximum and nothing clamps a small one. Not yet seen on the unit.

**The forecast** (`trial.py --forecast`, `tools/charge_sim/fit/trial_forecast_<record>.json`): the same trial against the simulation sixteen times, each with its own calibration; the middle, and the range without the lowest and the highest. Good, and a throw's median time:

| | BM2: good | Time | AR2209: good | Time |
|---|---|---|---|---|
| Proposal 1 within 0.02 gr | 96%; 80 to 100% | 11.4 s; 8.9 to 14.3 | 76%; 60 to 84% | 14.5 s; 13.8 to 21.8 |
| Proposal 2 within 0.02 gr | 84%; 76 to 96% | 11.2 s; 7.5 to 13.0 | 74%; 52 to 84% | 10.9 s; 7.5 to 14.5 |
| Proposal 1 within 0.04 gr | 100%; 96 to 100% | 8.3 s; 7.4 to 10.6 | 90%; 84 to 96% | 12.0 s; 9.7 to 17.7 |
| Proposal 2 within 0.04 gr | 100%; 92 to 100% | 8.5 s; 7.2 to 10.4 | 88%; 84 to 96% | 9.1 s; 7.9 to 11.8 |

Proposal 2's first rest within a digit of its aim: 76% (56 to 88%) with BM2, 52% (44 to 60%) with AR2209.

**Where the bench is expected to differ, said before the run:** slower by 5 to 8 s a throw for proposal 1 within 0.02 gr and 2 to 4 s for proposal 2 (the bench's scale shows a small move later than the simulation's); slower again by 1 to 3 s (the bench's coarse tube stops less tightly, so the fine tube carries more); proposal 1's good share nearer the recordings' ceiling (82 to 87% with BM2, 74 to 78% with AR2209); proposal 2's first rests the open question, either way.

**What the trial is judged on** (the guide has the table): A, it runs to its end with no limit tripped; B, proposal 1 within 0.02 gr good in at least 80% (BM2) or 60% (AR2209); C, proposal 2 within 0.02 gr good in at least 76% or 52% and over in no more than 24% or 48%; D, proposal 2's first rest within a digit of its aim in at least 20 throws of 25; E, within 0.04 gr good in at least 96% and 92% (BM2) or 84% and 84% (AR2209). Time is set beside the forecast and not judged.

**Next:** the owner's word that the bench is ready, with which powder. About an hour and ten minutes and two emptyings of the cup.

## 2026-11-21, 14:52 to 16:22: the two proposals on the unit: short runs first, what they found, what was mended

**The owner:** "Ready, AR2209 loaded". The Pico 2 W on the probe (firmware 2.0.3-84, eaea601), the A&D FX-300i, AR2209 long in the air, an empty cup on the zeroed scale. Before the first turn: `state` gave charge 0, weight 0.000 and both drivers ok, and the two hold speeds were taken with the weight unmoved (0.0001 rev/s is 2.82 steps a second of the fine tube, 0.00002 rev/s 0.82 of the coarse).

**Short runs before the long one:** the trial was first run with two throws a set, to see the tool work on a unit before an hour and two cups of powder were spent on it. Three such runs were needed. None is the trial: its checks A to E are judged on the full run alone.

### The first run: the calibration, and moves that released nothing

`20261121_145252_trial_pico2w_ar2209`. **The calibration's measurements** took 13.0 minutes and 173 gr:
- grains a turn: coarse 9.60, fine 0.797;
- every watch 5.3 s (the scale showed the slowest probe move after 2.9 s);
- the fine tube's stops, the lag by flow: 1.37, 1.19, 1.44, 0.94, 0.72, 0.79 s at 1, 2, 4, 8, 16 and 20 digits a second; the coarse tube's: 0.66, 0.58, 0.61 s at 0.3, 1 and 2 rev/s, its spread at a stop 0.10, 0.45 and 0.27 gr;
- the small moves: 0.58 digits on average (three at most) and shown within 4.4 s; 3.25 (eight at most) within 2.0 s; 5.63 (eleven at most) within 2.8 s.

**The glide finish worked at once:** its rehearsals rested at +3, -1, +1, -2, 0, -4, -2, -1, -2, -2 digits, and its throws within 0.02 gr at +1 and 0 (19.7 and 14.4 s).

**The jog finish's throws were void:** each made its eight moves and none released anything.

**Why: a tube started from a hold starts late.** The console's stand-in for a held tube is a speed too slow to pour. The firmware hands the PIO its steps a segment ahead, and a new speed is on the pin only when the steps already handed over are made: two segments at the old speed (`step_pulses.c`). At 2.82 steps a second a segment is one step, so a move of a third of a second was over before its first step. **Mended in the tool:** a held tube is switched off (`motor <tube> 0`, which stops the pulses at once) and then started. The made-up unit was given the firmware's way of taking a speed, and showed both the fault and the mend before the unit turned again.

**For ADR-012's record, not this ADR's to change:** a new step rate waits for two segments at the old one, which is tenths of a second and more at rates under ten steps a second. A charge that creeps a tube and then asks for a speed would meet it.

### The second run: throws that stopped two and three grains short

`20261121_152433_trial_pico2w_ar2209`, the first run's calibration. The glide finish again good in both throws within 0.02 gr (first rests -1 and +1, 19.8 and 16.8 s). But:
- jog within 0.02 gr: one throw first rested 4 digits short and made eight moves for two digits; the other **first rested 108 digits short**, and eight moves left it 63 short;
- jog within 0.04 gr: one throw **first rested 144 digits short**; the other was good in 9.7 s;
- glide within 0.04 gr: one throw rested 3 digits short and eight moves added nothing.

**Read from the kept rows and the console's log, no unit touched.** Four things, each measured:

1. **The forecast counted a fast coarse stream twice for a moment.** In the throw that rested 108 digits short the coarse tube's first powder showed 0.56 s after the throw began, and the forecast dropped the first parcel only at 0.73 s. For those tenths the reading rose and nothing was taken off what was held unseen: 2.4 gr counted twice, at 19 gr a second. The forecast passed the target, both tubes stopped, and the fine tube's stop was final. Where the 0.17 s came from:
   - the lag was measured at a stop, as what arrives after it over the flow. From a PC that holds the scale's delay and also the PC's own: the reading it decides on is a command old and its stop lands a command later. 0.60 s at 2 rev/s where the reading ran 0.53 s behind the turns all through the run (the calibration's own two stops, sample by sample);
   - the PC counted the turns from the speeds it had asked for, and booked each round's powder at the round's end: its first round of a throw was 0.105 s long;
   - a fast start shows sooner than its lag: the first powder at 0.44 to 0.46 s.
2. **Every throw's coarse tube left far more than its margin**, for the same reason: a median of 60 to 96 digits in the first run and 115 to 139 in the second (which sent one more command before each start), where the margin was 30. The simulated trials left a median of 25.
3. **A throw's small moves were smaller than the moves the calibration had measured.** The calibration's smallest move was asked as 0.0176 of a turn and was 0.0240 by the unit's own count (0.0706 for 0.0628; 0.158 for 0.151): its turn was ended a command or two late. A throw's move was ended by the clock within one round, 0.014 to 0.022 of a turn. So a throw's moves released less than the table said they would.
4. **A throw found far short at rest had no way back.** Eight small moves were all it had, and they cannot add two grains.

**Why the simulation had not shown it:** it has the fault, in 1% of its throws (10 and 11 of 800 jog throws first rested further than 30 digits short). Its reading rises 0.57 to 0.61 s after a coarse start at 2 rev/s where the bench's rises after 0.48 s, with the same lag at a stop (0.63 against 0.61 s). So what the forecast holds unseen is nearly right in the simulation through a start, and is not on the bench. **The record's scale has its delay and its filter's lag split by an assumption** (0.41 and 0.10 s), and only their sum was pinned. The first rise after a start pins the split; it is not yet a figure of the record. Not done here: the record is rebuilt after the trial, not during it.

### What was mended, in the simulation's code, before the unit turned again

In the controller and its calibration (`staged.py`), which the proposals are:
- **The lag is the powder's time from turned out to shown, taken over the whole run** (the area between the powder turned out and the reading, over the powder poured), not what is unseen at the moment of a stop. It is the same quantity where the two can be told apart, and it does not hold the latency of whoever turns the tubes. On the kept stops: coarse 0.60, 0.55, 0.58 s (by the stop: 0.66, 0.58, 0.61); fine 1.02, 1.17, 1.10, 1.06, 0.85, 0.82 s (by the stop: 1.37, 1.19, 1.44, 0.94, 0.72, 0.79), a steadier table.
- **Both tubes keep clear of the coarse tube's powder still unseen, by three times its spread a turn.** That powder is known by its turns alone, and a turn's powder varied by 6.1% from run to run in the calibration. A small charge is all unseen when the coarse tube stops: 6% of its nine grains is 0.55 gr, as much as the whole margin of 0.6 gr that was kept. In a large charge the reading has caught up and little is added.
- **The fine tube takes its stop on its own powder alone:** while a digit or more of the coarse tube's powder is unseen, a forecast at its margin pauses the fine tube and does not end the stream.
- **A rest short by more than the largest small move gives at most goes back to the stream,** by the same law, from the reading at rest. Whatever stopped the tubes early then costs the throw time and not its weight. A throw has eight tries after its first rest, moves and returns together. Nothing is learnt from a rest that is returned from.
- **A small move is a turn, made to the unit's count:** the calibration keeps the turn each size really was, and a throw's move is made to it.

In the tool (`bench/trial.py`), which stands in for the firmware:
- **The turns are the unit's own count** (`motor`, each round), in place of the PC's reckoning from the speeds it asked for. A unit's firmware has that count for nothing.
- A reading is stamped when it comes, and a turn shorter than a second is made with nothing read in between (the speed, the wait, the hold).
- A first start of a throw is not switched off first: the last throw's end left both tubes off.
- `--remeasure <folder>`: a kept trial's turns measured again by the rules as they stand, with no tube turned. The first run's 102 turns gave the figures above.

**What the forecast still gets wrong, measured on the kept coarse stops:** at 2 rev/s it is over by 1.4 gr at most, 0.6 s after a start, and under by 0.8 gr at most; usually 0.3 gr off. Two other ways of letting a parcel show (evenly between the first rise and as long after the lag; a first rise and a tail) were tried on the same stops and came to 1.1 and 1.2 gr: no better, and not taken. The margin that grows with the coarse tube's unseen powder is what covers it.

### The third run: the mends on the unit

`20261121_160810_trial_pico2w_ar2209`: the first run's turns measured again (`--remeasure`), two throws a set, from a cup the owner had emptied. **Said before it ran:** the coarse tube leaves 1 to 3 gr for the fine tube and causes no over; a first rest more than 11 digits short goes back to the stream and does not end under; the small moves are within a fifth of their turn by the unit's count.

**All three held:**
- the forecast's furthest above where the throw ended: 0 to 9 digits in the eight throws (over 100 in the second run's two);
- the coarse tube turned 0.73 to 0.86 of a turn and left 0.9 to 2.9 gr (44 to 147 digits), and no throw first rested more than a digit over;
- one throw first rested 12 digits short, went back to the stream once and came to 2 short;
- 20 small moves: 0.0242 to 0.0246 of a turn where 0.0240 was asked, 0.0723 and 0.0725 for 0.0706, 0.156 for 0.158.

**The throws:** jog within 0.02 gr, one over (+2) and one under (-3, its eight tries spent); glide within 0.02 gr, both good (10.3 and 27.9 s); jog within 0.04 gr, both good (11.9 and 19.9 s); glide within 0.04 gr, one good, one over (+4).

**What the smallest move does with this powder, now that its turn is exact:** 17 moves of 0.024 of a turn gave 0, 0, 0, 3, 0, 7, 0, 0, 0, 4, 0, 0, 0, 0, -1, 0, 2 digits. Nothing in 12 of 17, and then two to seven digits at once. Six in a row gave nothing in the throw that ended under; the one that gave seven took a throw from 3 short to 4 over. The calibration had said the same in its own way (0.58 digits on average, three at most nine times in ten). A band of one digit either side needs one to three digits from a move, and this tube with this powder seldom gives that.

### The forecast written again, and what I expect, before the full run

The controller changed, so its forecast was run again (`tools/charge_sim/fit/trial_forecast_pico2w_ar2209.json`; sixteen simulated trials of 25 throws a set at 10 gr; the middle, and the range without the lowest and the highest):

| AR2209 | Good | Over | A throw, median | Moves a throw | Before the mends: good, time |
|---|---|---|---|---|---|
| Proposal 1 within 0.02 gr | 74%; 64 to 84% | 16%; 8 to 28% | 17.6 s; 14.5 to 24.2 | 3.5 | 76%, 14.5 s |
| Proposal 2 within 0.02 gr | 70%; 64 to 84% | 28%; 16 to 36% | 13.0 s; 10.6 to 19.2 | 1.2 | 74%, 10.9 s |
| Proposal 1 within 0.04 gr | 92%; 72 to 96% | 6%; 0 to 16% | 13.1 s; 11.6 to 18.4 | 2.0 | 90%, 12.0 s |
| Proposal 2 within 0.04 gr | 88%; 76 to 96% | 12%; 4 to 24% | 11.6 s; 9.0 to 16.0 | 0.4 | 88%, 9.1 s |

Proposal 2's first rest within a digit of its aim: 54% (44 to 80%). The mends cost the simulation two to three seconds a throw (the coarse tube's wider margin on a small charge: it now leaves a median of 57 to 82 digits where it left 25) and leave its good shares where they were. In the simulation no throw of 1,600 now first rests further than 30 digits short.

**The checks A to E stay as they were written** before any run (`research/trial_guide.md`), though the new forecast's low ends for 0.04 gr (72% and 76%) are under check E's 84%.

**What I expect of the bench, from the three short runs, said before the full run:**
- **A passes.**
- **B fails or is close:** proposal 1 within 0.02 gr good in 35 to 60% of throws, 25 to 35 s a throw. Its three valid throws so far were none good, and its finish rests on a move this powder does not give.
- **C passes:** proposal 2 within 0.02 gr good in 60 to 80%. Its six throws so far were all good.
- **D fails:** its first rests so far lay within a digit of the aim in three throws of six, where the check asks for 20 of 25.
- **E:** proposal 1 passes or is close; proposal 2 is the doubt, since one move's clump of four to seven digits is over a band of two.
- **So the ranking may turn on the bench:** with this powder the stream's stop is the tighter end and the moves the looser, the reverse of the simulation.

## 2026-11-21, 16:24 to 17:15: the full trial with AR2209, judged

`20261121_162403_trial_pico2w_ar2209` (kept in `tools/charge_sim/recordings/` with the three short runs; `recordings/sessions.json` says which is which). The first run's turns measured again; for each set ten rehearsals of 1 gr, then 25 throws of 10 gr. 46 minutes of running, with one stop at 594 gr for the cup, which the owner emptied. No limit tripped. At its end the cup held 558.5 gr and both motors were off.

| AR2209, 25 throws a set | Good | Over | Under | A throw, median | Moves a throw | The forecast: good; time | Said before the run |
|---|---|---|---|---|---|---|---|
| Proposal 1 (jog) within 0.02 gr | **56%** (14) | 24% | 20% | 25.1 s | 4.4 | 74%, 64 to 84%; 17.6 s, 14.5 to 24.2 | 35 to 60%, 25 to 35 s |
| Proposal 2 (glide) within 0.02 gr | **68%** (17) | 32% | 0% | 13.2 s | 0.7 | 70%, 64 to 84%; 13.0 s, 10.6 to 19.2 | 60 to 80% |
| Proposal 1 within 0.04 gr | **84%** (21) | 12% | 4% | 14.7 s | 1.8 | 92%, 72 to 96%; 13.1 s, 11.6 to 18.4 | passes or is close |
| Proposal 2 within 0.04 gr | **84%** (21) | 16% | 0% | 12.8 s | 0.6 | 88%, 76 to 96%; 11.6 s, 9.0 to 16.0 | the doubt |

**The checks, as written before any run:**

| | The check | Result | |
|---|---|---|---|
| A | The tool and both controllers run on a real unit | The calibration was measured on the unit (the first short run) and every throw of the trial ended by its own verdict; no limit tripped | **passes** |
| B | Proposal 1 within 0.02 gr good in at least 60% | 56% | **fails** |
| C | Proposal 2 within 0.02 gr good in at least 52%, over in no more than 48% | 68% good, 32% over | **passes** |
| D | Proposal 2's first rest within a digit of its aim in at least 20 throws of 25 | 15 of 25 | **fails** |
| E | Within 0.04 gr good in at least 84% and 84% | 84% and 84% | **passes**, on the line |

By the guide's reading of the outcomes: B failed, so "the simulation is kinder to small moves than the bench by more than I allowed; read the throws' moves and waits before anything is built". They are read below.

**One throw of the first set was lost to a fault of the rule added that afternoon.** A rest was sent back to the stream when it was short by more than the largest small move gives at most (11 digits). The fine tube's learned stop margin had by then grown to 11 digits of room, so a stream begun from 11 digits short stopped at once: throw 17 went back eight times, poured nothing and ended 11 short. Six more throws of that set went back when moves would have served (three good, two over, one under). The rule was corrected before the third set (a rest goes back only when the stream would pour more from it than the largest move gives on average: short by more than the stop margin and that move together) and tried on the made-up unit; after it two throws of 54 went back. With throw 17 good, B would have stood at 60%, its line: **B fails narrowly either way, and the finding does not hang on it.**

### What the forecast had right, and what it had wrong

**Right, the stream's stop.** Every figure of proposal 2 fell inside its forecast's range but one (within 0.02 gr it made fewer moves than forecast, 0.7 a throw against 0.8 to 1.6), and every figure of both sets within 0.04 gr did. The first rests spread by 1.7 and 2.2 digits (proposal 2; forecast 2.4) and by 2.2 and 3.5 (proposal 1, stopped from its dribble of 8 digits a second; forecast 2.5 and 2.8). The creep stopped by the reading, which no real scale had shown before today, is as tight on the bench as in the simulation.

**Wrong, the small moves.** The same two sizes, by what one move gave, in digits:

| | Nothing | 1 | 2 | 3 | 4 to 6 | 7 and more |
|---|---|---|---|---|---|---|
| Smallest (0.024 of a turn), bench, 142 moves | 66% | 11% | 6% | 4% | 9% | 5% |
| Smallest, simulation, 1,759 | 60% | 16% | 10% | 6% | 7% | 2% |
| Middle (0.07 of a turn), bench, 59 | 25% | 12% | 7% | 10% | 34% | 12% |
| Middle, simulation, 1,095 | 16% | 16% | 19% | 18% | 25% | 7% |

A band of a digit either side needs one to three digits from a move. The bench's smallest move gives that one time in five and the simulation's one in three; the bench's middle move gives four and more nearly half the time. The powder gathers at the tube's lip and falls as a clump: six smallest moves in a row gave nothing in one throw, and one gave nine digits in another.

**What that did to proposal 1 within 0.02 gr:** every throw of it needs moves (its first rests lay 3 to 11 digits short, as designed). Nine throws of 25 spent all eight tries; five were taken over by a move's clump. Each smallest move waits 4.4 s for the scale. Hence 25 s a throw where the forecast said 17.6.

**The rehearsals overstate the spread of proposal 1's stop.** Ten throws of 1 gr with the fine tube alone rested with a spread of 4.1 digits, so the aim was set 9.1 digits short; the 25 throws' own first rests spread by 2.2, which would have set it 5.4 short. A rehearsal's short stream from rest is not a throw's.

### The ceiling, from the trial's own measurements

`tools/charge_sim/bench/best_finish.py`: the finish played on what was measured, with no model of the powder. A first rest is drawn from the stops measured (54 of each kind), moved to an aim; from any rest the better of "stop here" and "one more move of this size" is taken, a move's gain drawn from the moves measured (142 and 59); eight tries. Draws are taken as independent, which the lip's gathering is not, so it is an estimate.

| AR2209 on this unit | Within 0.02 gr | Within 0.04 gr |
|---|---|---|
| The stream's stop as proposal 2 makes it, best aimed a digit short | **71% good**, 1.5 moves a throw | **90% good**, 0.7 moves |
| The stop from proposal 1's dribble, best aimed 4 to 5 digits short | 67%, 2.9 moves | 89%, 1.7 moves |
| The trial itself | 68% (proposal 2), 56% (proposal 1) | 84%, 84% |
| The same play on the simulation's own moves and stops | 79% | 92% |

- **With this powder the ceiling within 0.02 gr is about 70%, whatever the finish.** The curve is flat from an aim of ten digits short to the target itself (65 to 71%): the aim does not decide how many throws are good. It decides how many moves a throw makes, and so its time.
- **From a rest two to four digits short, a throw is rescued little better than one time in two** (53 to 59%); from six or seven short, 71 to 73%.
- **Proposal 2 reached its ceiling (68 of 71). Proposal 1 did not (56 of 66 at its aim):** its rule for choosing a move is not the best play of these odds, and one throw was lost to the fault above.

### The coarse tube in a small charge

- **Its powder a turn, throw by throw:** 9.71 gr on average with a spread of 5.1% (8.6 to 11.0), where the calibration's ten runs said 6.1%. The margin that grows with its unseen powder stands on that figure, and the trial bears it out.
- **It left a median of 90 to 100 digits for the fine tube** (as meant: its margin and 18% of what is unseen), which the fine tube carries in about five seconds.
- **In 23 throws of 100 its stage outlasted the scale's delay** (0.9 to 1.4 s where the others took 0.5 to 0.9 s): the reading came, the margin shrank as it should, and the tube went on at its floor. Those throws ended a median of 36 digits short after the coarse tube alone, which is its margin of 30; but five ended 6, 20, 22, 24 and 26 digits short. None was over. In those the forecast held the tube's last powder at its floor for less than it was (the calibration's own runs at 0.3 rev/s gave 9.75 to 11.3 gr a turn, against the 9.6 used). **A risk to mend at the desk: one throw in 100 came within 0.12 gr of an over by the coarse tube alone.**

### What it means for the two proposals

1. **The frame holds on a real unit:** the forecast, one law a tube, the verdict at rest, the calibration that takes every number from the unit. With the hardest of the two powders it gave 68% within 0.02 gr in 13 s and 84% within 0.04 gr in 13 s, where today's loop put 85% of its throws over with this powder (the simulation's figure for it; the bench's own throws of 2026-11-20 agreed).
2. **With this powder the ranking turns.** Proposal 2 is the more accurate within 0.02 gr (68% against 56%) in half the time, and level within 0.04 gr. Proposal 1's finish rests on a move this powder does not give.
3. **It is one powder.** BM2's kernel is half a digit and its recorded nudges were kinder (88 to 90% at best within 0.02 gr); proposal 1 may still lead there. The simulation's forecast for BM2 with the mended controller: proposal 1 94% (88 to 100%) in 11.3 s, proposal 2 88% (84 to 96%) in 13.0 s. No BM2 throw of either has been made.
4. **The two are one controller, and the trial says who should choose between them: the calibration.** It already measures what each move gives and where the stream's stop rests. Playing the finish on those, as the ceiling above was found, gives the aim that makes the most good throws in the fewest moves, the move to make from each shortfall, and the share of good throws to tell the user before the first throw (the proposals' sixth rule). With AR2209 it picks proposal 2's finish. Designed here in one afternoon and tried on nothing: a direction, not a result.

### The simulation after the bench

- **The mended controller in the simulation** (`fit/bands_after_bench.txt`; 500 throws of 20 gr on the measured scale): proposal 1 94.8% good in 11.6 s (BM2) and 73.6% in 16.9 s (AR2209); proposal 2 87.8% in 13.4 s and 72.2% in 12.9 s. Within two points of the figures before the mends (94.4, 73.8, 89.0, 74.4%).
- **BM2's forecast written again** for the mended controller (`fit/trial_forecast_pico2w_bm2.json`).
- **What the record of AR2209 has wrong, each now measured:** its small moves are too kind (the table above); its reading rises 0.1 s later after a coarse start than the bench's; its coarse tube pours less a turn at speed than today's did (8.4 against 9.5 to 9.8 gr at 2 rev/s). The recordings to mend it with are in the repository. Not done.

### Not done, and what comes next

- **A trial with BM2:** the same tool, an hour, its forecast written. It says whether the ranking turns for a fine powder too, and whether the calibration should choose.
- **The record rebuilt** from the trial's moves and starts, so that the simulation's small moves are the bench's.
- **The finish chosen by the calibration** (point 4): in the simulation first.
- **The coarse tube's last powder at its floor** (above): at the desk.
- **For ADR-012's record:** a new step rate is on the pin only when the steps already handed to the PIO are made, two segments at the old rate, which is tenths of a second and more under ten steps a second (`step_pulses.c`).
- Nothing is approved and no firmware is designed: the ADR is a draft.

## 2026-11-21, 21:14 to 21:31: gates 0 and 1, the controller in C, checked at the desk

The plan was approved this evening ("Approved. You can start with gate 0, 1, and 2."), with both loops kept behind one setting (README, "Four answers for the build" and the revision after them).

**Gate 0.** The research branch, with the plan and the ADR marked Open, merged into main and pushed once (`929f538`). The build's branch, `adr009_controller`, starts there.

**Gate 1, what was built** (`79f53a8`):
- `src/charge_control.c` and `.h`: `staged.py`'s `Staged` and `set_rules`, and the end of `rehearse` that places the stop (`charge_control_place_stop`), in C, in its order. No FreeRTOS, no motor, no allocation. Its number type is float on the unit; `CHARGE_CONTROL_DOUBLE` builds it in double for the PC's check of its logic. Differences from the reference, each in the file's head: times are seconds since the throw's start; given the unit's count, a parcel is still made every 10 ms on the tick, its speed the turns over that stretch; the limits (120 s, 15 fine turns, 3 gr over) are the firmware's own.
- Zig 0.13.0, pinned in `toolchain.env` and fetched by both setup scripts, is the PC's C compiler (`zig cc`); the firmware is not built with it. `tools/charge_sim/firmware_controller.py` builds the controller as a library and loads it by ctypes, with `staged.Staged`'s interface; `check_firmware_controller.py` judges it; CI runs its exact part.
- `staged.py` and the C break a time tie the same way (`TIE_S`, a tenth of a millisecond): the simulation's times and a record's numbers lie on a millisecond's grid, and a parcel due on a reading's millisecond fell one way in double and the other in float. And the C counts its parcels with an integer: a sum of 0.01 s in float drifted past the tie.

**Gate 1, its pass** (written in the plan before it ran; 200 throws a set, 20 gr, both records, both finishes, both bands):

| | Result |
|---|---|
| The rules, C (float) against `set_rules` | agree to a part in ten thousand in all 8 sets |
| Lockstep, the C in double given the reference's very calls | 1600 of 1600 throws with no difference in their decisions, speeds or first errors |
| The float build, thrown apart | within three standard errors of the reference in all 8 sets (e.g. AR2209 glide 0.02: 72% / 75% good, 13.5 s / 13.5 s) |
| The firmware's own path, the turns given by count | the same (AR2209 glide 0.02: 72%) |
| The bench trial's kept calibration through the C rules | agrees, both finishes and bands |

What the check found on its way, each mended before it passed: the parcel tie and the float sum above; and, later in gate 2, that 128 parcels a tube are too few (with BM2's record a creep's powder takes over 1.28 s to show; merging the overflow stopped glides up to 0.5 s early): 256, as first written.

**Pass: gate 1 is built.**

## 2026-11-21, 21:31 to 22:07: gate 2 built, and its first throws on the rig

**What was built** (`0df64e6`, `0c2b449`):
- **The motors:** a move of an exact count of steps (`motor_move_turns`): the motors' task gives the pulse feed a budget (`step_pulses_set_budget`), the PIO makes those steps and no more, and the motor is held at zero once they are on the pin. The steps counted on each pin are read every millisecond (`motor_get_steps_made`, one task at a time).
- **The charge:** `settings/charge controller`, `staged` (default) or `pid`, chooses the loop. The staged throw runs every millisecond in the job task: the turns from the steps counted, each reading as it comes (`scale_take_new_measurement`), the speeds asked, a small move made to its count. ADR-013's pause, the zero, the cup and the API's throw are the PID loop's, shared. The PID loop is as it was, but for subtracting a series' base (0 outside one).
- **Stored layout, said here:** `controller` is a byte in the filler after `coarse_stop_backoff_enable` (offset 0x19); `eeprom_charge_mode_data_t` stays 64 bytes and a unit's stored record its CRC (seen on the bench: `saved=true`, `controller="staged"`). Each powder profile's calibration has a slot of its own, 13 to 20 K, 420 bytes (the measured part, the rehearsals' first errors, finish and band; the rules are made on the unit). No existing record changes.
- **A start with no calibration is refused:** 409 `not_calibrated` (API), "Not calibrated / Calibrate the powder, / or choose PID" (screen), an `@err` (console).
- **The console:** `ccal` writes a calibration from the PC (`tools/charge_sim/bench/series.py` sends it), `ccon` sets the frame's constants live, never stored, `ccrec` gives the last throw's rows, `charge <gr> series <n>` pours n throws into one cup for the bench, each from the weight the cup rests at.
- **Memory:** the controller and its calibration (about 5 KB) come from the FreeRTOS heap for a staged charge and go back at its end; the Pico W's RAM at link time is 252.9 of 256 KB (it overflowed by 17 KB with them static). The row record (`ccrec`, 800 rows) is the Pico 2 W's only: the Pico W's recordings are gate 7's, with the API's.

**Not yet built of gate 2's plan:** the API's recordings (`/api/v1/recordings`); the simulated scale's own calibration and its weight no longer held at the target, so `charge_cycle` runs the staged controller; the proofs `charge_cycle`, `pulses` and `menu_tour --compare` on this build.

**The throws** (Pico 2 W, A&D FX-300i, the air-exposed AR2209, 10 gr, the bench trial's calibration of this afternoon pushed by `ccal`; `tools/charge_sim/recordings/*_series_*`; `bench/series_look.py` reads them):

| Series (glide, 0.02 gr) | Its stop | Good | Over | Under | Median |
|---|---|---|---|---|---|
| A, throws 1 to 12 | the PC trial's rehearsals (stop room 0), learned up to 0.033 | 6 of 12 | 6 | 0 | 13 s |
| A, throws 13 to 25 | learned, 0.032 to 0.039 | 9 of 13 (69%) | 4 | 0 | 19.7 s |
| B, 25 throws | placed from A's own throws (0.034), `fine_top_rps=1.0` | 20 of 25 (80%) | 5 | 0 | 18.0 s |

What they say:
- **The firmware's controller works on the unit:** 52 throws, no fault, no under, every move made to its count, the learning moving the stop as designed.
- **The stop the PC's rehearsals placed is the PC's:** at the fine stop, a throw came to rest a median +0.04 gr above its forecast in series A; the stop learned from the unit's own throws is 0.034 gr, about two digits earlier. So a calibration's rehearsals must be the unit's own throws ending, as the trial's weak point 1 said.
- **The coarse tube stops early when the forecast counts its first powder twice.** A fast start's powder first moved the reading at 0.485 s, where its lag is 0.58 s; for a moment the forecast holds it twice (8.3 gr at 0.66 s, falling to 7.6 as its parcels fall due). When that bulge crosses the coarse margin the coarse stops for good about 1 gr early: 15 throws of 25 in A, and all 25 in B, where the fine tube's own powder at 1 rev/s adds to the bulge. The fine tube then carries 3.5 gr in place of 1.5. The forecast's model of the scale is a pure delay where the scale is a delay and a filter: the cause is there, not in a constant.
- **The time is in the last 0.3 gr:** at low speed this powder pours less than its 0.8 gr a turn, the forecast falls back to what the scale confirms, and the fine tube creeps on for several seconds; a throw that rests short adds a move of 2 to 4 s. Stopping near the target (A's first half, and the trial) is 13 s with half the throws over; stopping two digits short is 69 to 80% good in 18 to 20 s.

**Against gate 2's bar** (68% good within 0.02 gr, median at most 13 s): the accuracy is met (B: 80%), the time is not (18 s). The 0.04 gr band is not yet thrown.

## 2026-11-22, 11:33: the forecast's scale, a delay and then a lag; its passes written first

The owner chose this before the rest of gate 2 ("Yes, fix forecast"). It is the mend gate 2's plan names for "the slow start of a small charge".

**Why first, from series A and B's own throws:** the throws split by where the coarse tube stopped. Ten of A's stopped it on time, at about 8.5 gr, and took a median 13.3 s. Fifteen of A's and all 25 of B's stopped it at about 6.5 gr, and took a median 18 s. The early stop is the whole gap to gate 2's 13 s.

**The change:** a parcel of powder is no longer all unseen until its lag and then all shown. It starts to show after a delay, and then shows as a first-order lag τ does, the delay and τ adding to the lag measured at its flow. So the measured lags, and with them the forecast of a steady stream, are unchanged; what changes is a start and a stop.

**The passes, before each step is run:**
1. **The shape, from the recordings.** Fitted run by run on the trial calibration's 102 runs (`20261121_145252_trial_pico2w_ar2209`) and on the coarse starts of the 52 throws. Pass: one τ for the coarse tube, the runs at 0.3, 1 and 2 rev/s within 30% of their median, the fit within 1.5 digits RMS. If τ follows the speed, it is a table by speed, as the lag is.
2. **The replay, on the 52 throws.** The coarse tube's flow made again from the speeds asked (50 rev/s² ramps, scaled to each throw's counted turns). First the old forecast, made again this way, matches the one the unit logged within 0.15 gr RMS over the first 2 s; otherwise the replay proves nothing. Then the bulge (the forecast's highest in the first second, above where it settles once the coarse tube's powder has all shown, the fine tube's powder in between taken off) is under 0.2 gr in 50 of 52 throws, against about 1.4 gr today.
3. **The simulation,** both records, both finishes, both bands, 200 throws each on the seeds of gate 1. Pass: the good share in each set is within three standard errors of today's or better, and the median time is no longer. On AR2209's record the coarse tube's share of a 20 gr charge rises.
4. **The C,** the same change: gate 1's pass again (the lockstep 1600 of 1600, the float build within noise).
5. **The rig,** 25 throws, gate 2's bar: at least 68% good within 0.02 gr with a median of at most 13 s. Expected: the coarse tube at about 8.5 gr in nearly every throw, and a median near 13 s.

### 11:40 to 12:30: the cause is the calibration's lags, not the forecast's shape

**Step 1, the shape (`bench/scale_shape.py`, the trial calibration's 102 runs): fails as written.** A delay and then a lag τ, fitted run by run:

| Tube, rev/s | Runs | Delay s | τ s | Delay + τ | Mean lag (`behind`) | RMS, digits (delay alone) |
|---|---|---|---|---|---|---|
| coarse 0.3 | 3 | 0.584 | 0.004 | 0.588 | 0.588 | 3.97 (3.98) |
| coarse 1 | 9 | 0.442 | 0.076 | 0.518 | 0.539 | 5.77 (6.75) |
| coarse 2 | 2 | 0.448 | 0.098 | 0.546 | 0.560 | 11.5 (13.7) |
| fine, 0.025 to 1 | 43 | 0.61 to 1.2 | 0.04 to 0.32 | | | 0.35 to 3.7 (0.44 to 3.7) |

τ is not one number (none at 0.3 rev/s), and no fit comes within 1.5 digits: what is left is the coarse tube's lumps, not the scale. The second number buys little.

**Step 2, the replay (`bench/forecast_replay.py`, the 52 throws of series A and B):**
- **The remaking is good:** the old forecast made again from the speeds asked matches the unit's logged one within a median 0.09 gr RMS over the first 2 s (90%: 0.13), and its bulges are the logged ones (median 0.89 against 0.86 gr).
- **τ alone does little** (fails as written): with τ = 0.1 s the bulge falls from a median 0.89 to 0.61 gr, under 0.2 gr in 12 throws of 50.
- **The readings say what is wrong** (`--fit`, every throw's readings to the coarse tube's stop and a second after, the coarse tube's powder a turn fitted for each throw): the best fit moves every measured lag 0.05 to 0.06 s earlier, with τ 0.10 to 0.12 s, at 0.150 gr RMS. As calibrated: 0.467. τ alone: 0.368. The move alone (−0.06 s): 0.213.
- **With the lags 0.055 s earlier,** the bulge's median is gone (−0.01 gr; 90% 0.47 gr), with τ or without it. It is under 0.2 gr in 34 throws of 50, not in 50 of 52 as written: what is left is the lumps again, and the margin that grows with the coarse tube's unseen powder is there for them.

**So the trial's calibration, made by the PC through the console, holds lags 0.055 s longer than the firmware meets.** The measure over a whole run (the mend of 2026-11-21, 16:22) took out most of the PC's latency but not all of it: a reading is stamped when it reaches the PC, and a start lands a command late. The PC's own loop had the same latency, so the PC's trial threw well on it (68%, 13 s); the firmware does not, so on the firmware every parcel was held unseen 0.055 s after it showed, at the start's 19 gr a second. The scale's τ is real (about 0.11 s; the record's assumed split of 0.41 and 0.10 s fits it), but it does not change where the coarse tube stops once the lags are right.

**The simulation tried the cause** (AR2209's record, 10 gr, glide at 0.02, 200 throws each; the controller `glide_pc_lags` is the same calibration with every lag 0.055 s later, `staged.shift_lags`):

| | Coarse tube's powder (10% to 90%) | Median | Good | Over |
|---|---|---|---|---|
| Simulation, its own lags | 8.58 gr (7.02 to 8.80) | 12.7 s | 69% | 29% |
| Simulation, lags 0.055 s late | 6.58 gr (6.35 to 6.76) | 18.6 s | 72% | 27% |
| Rig, series A's throws with the coarse tube on time (10) | about 8.5 gr | 13.3 s | | |
| Rig, series B (25) | about 6.5 gr | 18.0 s | 80% | 20% |

The simulation makes the rig's two kinds of throw from this one difference: **pass, the cause is found.**

**What changes, and what does not:**
- **No mechanism changes.** The forecast stays as it is; steps 3 and 4 above (the forecast's new shape in the simulation and in C) are not done.
- **Gate 2:** the trial's calibration is written to the unit with every lag 0.055 s earlier (`series.py --lag-shift -0.055`, kept in the series' record). The rules made from the lags move with them (the creep, the glide's and the coarse tube's slowing).
- **Gate 3 needs nothing new:** the calibration as a job on the unit measures the lags in the firmware's own time. Its pass against the trial's lags ("each lag within 0.1 s") holds with them 0.055 s shorter.

**Step 5, the rig, its forecast written first:** series C, the same as series B (the trial's calibration, B's first errors, `fine_top_rps=1.0`, 10 gr, 25 throws) but for the lags. Expected: the coarse tube at about 8.5 gr in at least 22 throws of 25, a median of 12 to 14 s, and more overs than B's 5 (the simulation: 29%; series A's on-time throws were often over). Pass: gate 2's bar, at least 68% good within 0.02 gr with a median of at most 13 s.

### 11:50 to 12:00: series C on the rig, judged; the band's edge mended

The owner: "Bench ready". `20261122_115105_series_glide002_c_lagshift_pico2w_ar2209` (two throws first, both good, the coarse tube at 7.7 and 8.0 gr): series B's calibration, first errors and `fine_top_rps=1.0`, every lag 0.055 s earlier.

| Series (glide, 0.02 gr, 25 throws) | Coarse tube's powder | Good | Over | Under | Median |
|---|---|---|---|---|---|
| B, lags as the PC measured them | about 6.5 gr, every throw | 20 (80%) | 5 | 0 | 18.0 s |
| C, lags 0.055 s earlier | 7.4 to 8.6 gr; at least 7.5 in 24 of 25 | 15 (60%) | 10 | 0 | 13.7 s |

- **The cause is confirmed on the rig:** the coarse tube stops where the margin puts it, and the time falls by 4.3 s, as the simulation said (12.7 s there).
- **Against gate 2's bar: fail** (60% good against 68%; 13.7 s against 13). The forecast had more overs coming (the simulation: 29%), but not 40%.
- **Where the overs come from:** five throws first rested 2 to 6 digits over, the stream's own stop (the learning moved the stop room from 0.031 to 0.043 gr over the series); four rested 1 to 3 digits short and one small move released 4 to 6 digits; and one, throw 37, first rested at 9.98, inside the band, and was moved on to 10.08.
- **Throw 37 is a fault of the controller's edge, now mended.** A rest's weight is a reading less the cup's base, and in the unit's float 10.02 over a cup of 60 gr is 0.0200043 over the target, past the band's edge of a millionth: three rests at 10.02 were logged over (32, 40, 49), and three at 9.98 were moved on (37, 40, 52). On a charge of 100 gr the same falls by chance with no base at all (100.02 less 100 is 0.0199966). The edge is now a quarter digit past the band, in `charge_control.c` and `staged.py` alike (rests lie on the scale's digits). Gate 1's check again: lockstep 200 of 200 in all 8 sets, float and counted within noise, the rules agree: **pass**. Not yet on the unit.
- **B against C is the trade of this powder:** B's fine tube carried 3.5 gr at its slow end and rested in the band four times in five, at 18 s; C's carries 1.5 gr and is quick, and the stream's stop and the moves' clumps cost it a throw in five. The bar asks for both at once; the trial's ceiling for this AR2209 (about 70% within 0.02, `bench/best_finish.py`) is close to it.

## 2026-11-22, 12:05: the algorithm verified on the throws, the calibration checked number by number

The owner: "Continue. I want you to also make sure you focus is to verify the control algorithm, and validate the calibration accuracy, not stuck in the manual tuning nightmare." So no constant is searched for on the rig: each miss is traced to a calibrated number or a rule, from the throws' own rows.

**The tool,** `bench/verify_series.py`: a series' throws against the calibration they were thrown on. In closed loop every throw lands at its target, so the two tubes' grains a turn cannot be told apart by a regression; each tube is read where only its own powder is arriving (the coarse tube once it is quiet after its stop, the fine tube from there to the first rest, in throws with no move).

| The calibration's number | Calibration | Series A | Series B and C | Verdict |
|---|---|---|---|---|
| Coarse tube's spread a throw | 6.1% | 5.3% | 6.0% | right |
| What arrives after a stop from the creep | +0.027 gr, sd 0.028 | +0.037, sd 0.031 | +0.026, sd 0.033 | right |
| Coarse tube's grains a turn | 9.60 | 9.88 (+2.9%) | 10.17 (+5.9%) | low |
| Fine tube's grains a turn, at the finish | 0.797 | 0.753 (−5.5%) | 0.715 (−10.2%, sd 0.17 a throw) | high |
| Lags | as the PC measured | | 0.055 s long | mended (above) |

The calibration's own stops, run from rest at each speed, give 9.6 to 9.9 gr a coarse turn and 0.66 to 0.79 a fine one: a short run from rest is not what a throw does. That is gate 3's planned mend (its rehearsals a throw's last stretch, not a stream from rest), now with numbers.

**The algorithm, against the truth** (series C; the truth is what finally landed, less what the fine tube had still to turn out):
- **At a stop from the creep, with the coarse tube quiet** (20 throws): the throw rested +0.004 gr from the forecast, sd 0.036. The forecast is right where the controller acts on it, and its spread is the powder's own after-stop spread. **Pass.**
- **At a stop while the coarse tube's powder was still arriving** (5 throws, all about 0.75 s after the coarse stop, where the calibration has the coarse tube quiet only after 0.91 s): −0.057, sd 0.076, twice as wide.
- **From the coarse tube's stop on:** the forecast is 0.25 gr low at the stop (sd 0.27), right within 0.4 s (sd 0.22, then 0.13 at 0.6 s). The coarse tube's 6% more powder a turn is about half of that bias (6% of the 2 gr still unseen).

**Where series C's ten overs come from:**
- two stream stops on a forecast still low from the coarse tube's tail (30, 41);
- three stream stops from the creep that rested 2 to 5 digits over: the after-stop spread, lumps;
- five small moves that released a clump (37's from a rest the edge fault had refused).

**Series D, its forecast written first:** series C's settings and the edge fault mended, with the two grains a turn the throws gave (coarse 10.17, fine 0.715) written into the calibration in place of the trial's. Nothing else changes; the rules made from them move with them, as on any unit.
- **The calibration's check, pass:** the forecast at the coarse tube's stop within ±0.10 gr of the truth on average (against −0.25 in C), and the fine tube's grains a turn from D's throws within 5% of the 0.715 written.
- **The algorithm's check, pass:** at a stop from the creep, the rest within ±0.01 gr of the forecast on average, sd at most 0.04.
- **The outcome, expected, not judged:** 60 to 72% good within 0.02 gr, a median of 12 to 14 s. The stream's stop spread (sd 0.035) against a band of ±0.02 and the small moves' clumps are this powder's limit, about 70% (the trial's ceiling), so gate 2's bar is judged, not expected.

### 12:12 to 12:21: series D, the calibration's grains a turn from the throws, judged

The edge fault's mend flashed first (`rc_run.py flash`, PASS; `setting settings/charge` the same before and after). `20261122_121340_series_glide002_d_yields_pico2w_ar2209`: series C's settings, the calibration's grains a turn 10.17 and 0.715 (`series.py --coarse-gr-turn --fine-gr-turn`). Its rules moved with them: the creep 0.0247 to 0.0291 rev/s, the wait before a rest is judged 2.33 to 2.82 s (the faster creep counts the 0.05 rev/s stop's quiet time).

| Series (glide, 0.02 gr, 25 throws) | Good | Over | Under | Median |
|---|---|---|---|---|
| B: lags as the PC measured them | 20 (80%) | 5 | 0 | 18.0 s |
| C: lags mended | 15 (60%) | 10 | 0 | 13.7 s |
| D: lags and grains a turn mended | **20 (80%)** | 5 | 0 | **11.7 s** |

**Against its forecast** (`bench/verify_series.py`):
- the forecast at the coarse tube's stop: −0.072 gr on average (C: −0.25), sd 0.21: **pass**;
- the fine tube's grains a turn: D's throws give 0.625, 12.6% under the 0.715 written, sd 0.19 a throw: **fail**. The figure at the finish is about 0.63 in C and D alike; B's 0.71 was over a wider stretch, at higher speeds;
- the coarse tube's: 10.50, 3.2% over the 10.17 written. Series by series it was 9.88, 10.22, 10.18 and 10.50: it moves by about 3% from one series to the next, half its spread a throw;
- at a stop from the creep, the rest less the forecast: +0.003 gr (pass), sd 0.045 (fail against 0.04, just). What arrives after the stop is +0.051, sd 0.048 at the faster creep, against the calibration's +0.027, sd 0.028 at 0.025 rev/s;
- the outcome came out above the range expected (80% against 60 to 72%; 11.7 s against 12 to 14 s). Over 25 throws, 80% is between 59 and 93% at 95%: C and D differ by 1.6 standard errors, so D is not yet proof that the mend is worth 20 points, only that it costs nothing and takes the bias out.

**Where D's five overs come from:** four from small moves that released a clump (6, 15, 16, 20: rests 2 to 6 digits short, ended 2 to 4 over); one fine stop on a forecast still low from the coarse tube's tail (24: 4.5 s, +0.08).

**Gate 2's bar at 0.02 gr is met on D** (at least 68% good, a median of at most 13 s). Not yet thrown or built of the gate: the 0.04 gr band (84%), the API's recordings, the simulated scale's calibration for `charge_cycle`, the proofs (`charge_cycle`, `pulses`, `menu_tour --compare`, `motor` over the 50 throws).

**What the verification hands to gate 3** (the calibration as a job on the unit), each a number the throws measured and the open-loop runs from rest did not:
- the lags, in the firmware's own time (0.055 s shorter than the PC's);
- each tube's grains a turn in a throw's own shape: the coarse tube 3 to 6% more than a run from rest at 1 rev/s, the fine tube about 20% less at the finish (0.63 against 0.80);
- what arrives after a stop at the creep the unit really uses (its rule moves with the grains a turn).
`verify_series.py` on a calibration job's own throws is its check.

### Series E, the 0.04 gr band, its forecast written first

The owner emptied the cup ("Cup emptied, go"). Series D's calibration and settings (lags 0.055 s earlier, grains a turn 10.17 and 0.715, B's first errors, `fine_top_rps=1.0`), the band 0.04 gr, 10 gr, 25 throws, two first. The aim is the same as at 0.02 (a digit short: `-max(digit, band / 2)`), so the stream's stop does not move; what changes is that a rest within two digits ends the throw, so fewer small moves are made.
- **Pass, gate 2's bar:** at least 84% good within 0.04 gr.
- **Expected:** 88 to 96% (the simulation's glide at 0.04: 90%; D's own throws judged at 0.04: their first rests within two digits in 20 of 25, their final rests in 22 of 25); a median under D's 11.7 s; the forecast at a creep stop as in D (+0.003 gr, sd 0.045).

### 12:26 to 12:34: series E at 0.04 gr, judged

`20261122_122716_series_glide004_e_pico2w_ar2209` (its two throws first: both good). The band's rules moved the creep from 0.029 to 0.058 rev/s (it follows the band) and the coarse tube's margin a little; the aim and the stop room stayed.

| Series (10 gr, 25 throws) | Band | Good within 0.04 gr | Over | Under | Median |
|---|---|---|---|---|---|
| D (the 0.02 gr band), judged at 0.04 | 0.02 | 22 (88%) | 3 | 0 | 11.7 s |
| E | 0.04 | **20 (80%)** | 5 | 0 | **9.2 s** |

**Against its forecast:** gate 2's bar at 0.04 gr is **not met** (80% against 84%, a throw short); 88 to 96% was expected. Quicker than D, as expected. The algorithm's check holds: at a stop from the creep the throw rested +0.003 gr from the forecast, sd 0.045, as in D. What arrives after the stop, +0.050 gr (sd 0.047), is the calibration's at that creep (+0.048 at 0.05 rev/s): the faster creep cost no accuracy at its stop.

**Where the five overs come from:** four first rests 3 to 5 digits over, the stream's own stop (31, 45, 47, 48), against one such rest in D; one from small moves (33).

**A lead, not the cause (tested below, and rejected): the stop's learning.** Each throw's first error moves the stop by 15% of its distance from the aim, whatever the stop's spread. With this powder's spread (sd 0.045 gr, more than two digits) that is a walk on noise: over E the stop room went from 0.031 to 0.010 and back to 0.040, as wide as the band itself (D's: 0.031 to 0.055). At 0.04 gr a first rest of −0.04 is good and ends the throw, but it still taught the stop to come later (13 first rests at −0.04 or below; their median −0.04 against the aim's −0.02), and the overs at the stream's stop followed the low point (45, 47, 48 at a room of 0.022 to 0.034 after 0.010). The rule was written for a stop whose spread is about a digit (the simulation's).

**So, on the rig, gate 2's two bars stand as:** 0.02 gr met (D: 80%, 11.7 s); 0.04 gr met by D's throws (88%) and not by the 0.04 setting's own (E: 80%). Proposed at the time, tested below and not built: a learning that takes the stop's own measured spread into account (for instance the mean of the first errors so far, its gain falling as the throws add up, and nothing learnt within the spread), tried in the simulation on both bands with the rig's spread before it is thrown. A change of the rule is the owner's to approve.

### 12:36 to 13:15: the stop's learning tried on the rig's own stops: kept as it is

The owner took the proposal ("I'll take your proposal"). Tried first in a play of the learning alone, since the simulation's stops spread far less than the rig's: `bench/learning_loop.py` takes each first rest of series D and E plus the stop room it was thrown with as the rig's own noise (48 throws, sd 0.046 gr) and plays 2,000 sessions of 25 throws with each rule, from a stop placed right and placed two digits wrong.

| Stop placed | | The controller's rule (15% of each first error) | Proposed (the mean of the stop rooms each throw asks for, against the placed stop worth 5.3 throws) |
|---|---|---|---|
| right | stop room's wander, second half (sd) | 0.0105 gr | 0.0071 gr |
| right | first rests over, at 0.02 / 0.04 gr | 15.4% / 8.2% | 14.2% / 8.1% |
| two digits wrong | throws until within half a digit | 13 to 15 | more than 25 |

**Verdict: the proposal fails, and the rule stays.** It wanders a third less but buys a point at most, and it mends a wrong placement too slowly: a stop is learnt to mend a calibration that is off, and the cap of two digits a throw clips a real error with the noise. (Its first form, the mean of the errors themselves, was worse: they shrink as the stop moves.) Nothing is built; `staged.py` is as it was.

**And so E's miss is not the learning's.** With the stop placed right, the walk costs nothing at 0.04 gr: the first rests over are 8% either way, two throws in 25, where E had four (one series in about seven). E's 80% is within the noise of what the rig's stop allows at 0.04 gr; the lead I wrote under series E was a guess from one series, and the play does not bear it out.

**What limits the 0.04 gr band is the stop's spread on the rig, sd 0.046 gr**, wider than the calibration measured it (0.028 at the creep's own stops from rest; 0.039 in its rehearsals). That is a third number for gate 3's calibration to measure in a throw's shape, beside the lags and the grains a turn.

## 2026-11-22, 13:29 to 13:45: gate 2's proofs on the Pico 2 W

The owner: "Proofs next ... don't drift into the tuning. your job is to verify the robustness of the controller." The cup emptied.

**`motor`, the step pulses over the 54 throws since the 12:12 flash: fail, then mended.** The fine motor's feed had run late 8 times (`fine_underruns=8`; the coarse motor's 0). The same count had grown on the old firmware too (18 to 101 on 11-20, noted then for ADR-012 and not looked into). The cause, predicted from the arithmetic and then measured: the feed runs every millisecond and queues one segment of steps only once the queue is empty, and a segment's steps were rounded down, so at a rate just under 1,000 steps a second a segment lasted barely 1 ms and the next feed had no time to spare. On the fine tube that is 0.027 to 0.035 rev/s, the 0.02 gr band's creep.

| Fine tube, held 10 s | Segment, rounded down | Slack | Late feeds before | After the mend |
|---|---|---|---|---|
| 0.0340 rev/s | 1.044 ms | 0.04 ms | 18 | 0 |
| 0.0355 rev/s | 1.000 ms | 0 | 16 | 0 |
| 0.0290 rev/s (series D's creep) | 1.224 ms | 0.22 ms | 2 | 0 |
| 0.05, 0.1 rev/s; coarse 0.3 rev/s | 1.4 ms and more | 0.4 ms and more | 0 | 0 |

The mend (`step_pulses.c`): a segment's steps rounded up, so a segment is never shorter than 2 ms and the feed always has a millisecond to spare. A new rate reaches the pin within about 4 ms, as before at most. The steps made follow the speed asked (0.034 rev/s for 10 s: 9,618 steps). It is the step feed, so the Pico W proves it too (gate 7).

**`pulses`: pass** (on its second run). A probe reset, an assert and a hard fault each stop the step pulses, with the fault images built from this source (`build_2w_fault1`, `build_2w_fault2`). The first run did not judge the hard fault: openocd's ROM call to erase the flash failed once, after the assert, the first time in thirteen runs of `pulses`; the second run had none.

**`menu_tour --compare tests/menu_baseline/pico2_w`: pass.** Fourteen pages, all as the baseline but the two that are not judged (the scale's reading, the version).

**`charge_cycle`: the simulated scale given what the staged controller needs, then pass.** As it was, it could not judge the new controller: its powder followed the motors' shaft speeds at made-up factors, it was read and poured once a reading (so a small move between two readings poured nothing: the first cycle's eight moves released nothing and it ended 0.04 gr short), it snapped the weight to the target within 0.1 gr and held it there (an over could not show), and it lifted the cup by its own clock (which a throw's rests between moves would trip). Now (`simulated_scale.c`, `charge_calibration.c`):
- each tube pours 10 and 0.8 gr a turn of its own (not of its motor's shaft), forward only (the PID loop's back-off pours nothing), added up every 2 ms, read at the scale's interval and rounded to a 0.02 gr digit; an over shows as one;
- the cup is lifted when the charge asks for it, and put back as before;
- its calibration is its own, written from those numbers (a stop at any speed leaves half an interval of flow unseen, a move releases its turns' powder): used whenever the simulated scale runs, whatever the profile holds; nothing stored is read or written.
Then: the staged controller 30.000 for 30, 5.7 s (first rest −0.06, four moves); the PID loop, switched to live and back, 30.000, 8.5 s.

### 13:46 to 13:55: the staged controller under faults, on the unit (`bench/robust_staged.py`)

Each case a charge of 20 gr on the simulated scale (10 readings a second: a pause after 300 ms without one, lost after 1 s), one fault put in at a stage of the throw, `state` read every 30 ms. The passes are in the tool's head, written before its first run.

| Case | Staged | PID loop | What was seen |
|---|---|---|---|
| none | pass | | a throw, no pause (19.98, 4.1 s) |
| three frames that do not parse, in the coarse stream | pass | | counted (`bad_frames` +3), no pause, a throw |
| no reading for 0.6 s: in the coarse stream / the fine creep / a small move | pass ×3 | | a pause after 300 ms, both tubes still to the quiet's end, resumed from the settled weight, a good throw (the move cut short and its powder taken from the settled weight) |
| no reading for 5 s, in the coarse stream | pass | | paused, then "Charge: ended, Scale: no reading", both tubes still |
| `charge stop` in the coarse stream | pass | pass | over 16 ms after it; both tubes still from 0.2 s (the coarse tube's ramp down) |
| the fine driver read as shorted, in the fine stream | pass | | "Charge: ended, Fine driver short", both tubes still |
| the cup lifted for 2 s in the coarse stream, then put back | **fail** | **fail** | both tubes turned the whole time the cup was off, in both loops. The staged throw ended at its limit, 49.7 gr for 20: 48 gr poured onto the bare pan |

Two faults of the harness, not of the unit, mended on the way: the stop's window was judged with no samples once the charge had ended; an aborted charge left the simulated pan full, so the next case waited for a zero (the simulated scale is now zeroed between cases, as a person empties the cup).

**Found, for the owner: a cup lifted mid-throw is poured on, in both loops.** The reading falls by the cup's weight, both loops read it as a charge far short, and both turn at full speed. Not new: today's PID loop does the same. The staged controller already holds that powder only adds (its rest is a reading that makes no new high); a reading that falls well below the throw's highest (more than a few of the scale's digits; a cup is grams) is a cup lifted or something knocked, and both tubes would pause, as at a missing reading, until the reading is back where it was. Not built: it changes what both loops do, and the plan keeps the PID loop as it is.

## 2026-11-22, 13:55 to 13:59: the PID loop and the staged controller compared in the simulation

The owner: "consider and compare the old PID in simulation first, you can plug in the value from the other trickler on 192.168.2.168", with three questions (the pros and cons of each, the calibration's accuracy, how to combine them to be fast, accurate, adaptable by calibration, easy to tune and share). His Pico W's settings were read over its web API (GETs only; `logs/adr009/picow_168_20261122`, the same as on 11-19) and its AR2209 profile is `legacy_pid.AR2209_PICOW_168`. The study is `research/pid_and_staged_2026-11-22.md`; its figures in `tools/charge_sim/fit/study_20261122/`. In short:
- **The PID loop is fast (6 to 9 s) and plain, and blind to the powder in the air:** with the owner's gains on the bench's AR2209 rig it is over in 90% (mean +0.11 gr), on a scale twice as late in 100%; with BM2, 68% good.
- **The staged controller is accurate on both powders and scales (67 to 90% within 0.02 gr in the simulation) and slower (10 to 15 s, 27 to 30 s on the slow scale);** the time is in its waits.
- **A calibration that is off costs it time, not accuracy:** the lags 0.055 s long cost 6 s; grains a turn off by 6 and 15% cost up to six points with BM2 and nothing with AR2209.
- **The PID loop fed the forecast** (`legacy_pid.ForecastPid`, its gains untouched) is no longer over (10% at most, on either scale) but short (48 to 79%): it has no finish. The forecast and a finish together are the staged controller; what the PID loop can give it is speed (its waits) and plain gains made by the calibration.

**The owner on the cup lifted mid-throw (14:10):** "Document in the ADR sign off. I don't want you to lose your focus to the controller design to the safety. The safety must be a sign off condition, but not the current focus." Written into the README's "How the unit shows it works" as a condition of the ADR's sign-off: `robust_staged.py`'s `cup_lifted` passes on both units, for every loop left to users, before the ADR closes. Nothing built now.

## 2026-11-22, 14:47 to 14:49: the two modes on the unit (an MVP), and their session's forecast

The owner, after the talk of this afternoon (`research/talk_2026-11-22.md`, a record, not a reference): "I'd like to see the MVP running ... build it first on the target, and let's have a real world trickling session, each 10-20gr trickle target, with different mode, and let's compare the real world obseration versus the simulation." No plan revision: how the parameters are packed is still open.

**Built, the least that runs the two modes:**
- `settings/charge mode`: `accurate` (within 0.02 gr) or `fast` (within 0.04 gr), applied at the next charge;
- at each staged charge's start the calibration's rules are made for the mode's band and the glide, whatever band the calibration was written with (`charge_calibration_get(profile, band, ...)`);
- **stored layout, said here:** `trickle_mode` is a byte in the charge settings' filler after `controller` (0x1A); `eeprom_charge_mode_data_t` stays 64 bytes, the next field at 0x1C as before, so a unit's stored record keeps its CRC and a stored 0 there is accurate, the band of every throw before;
- `series.py --mode` sets it live, never stored, judges the throws at its band, and puts the stored charge settings back at the end.
Not built: the mode on the screen and in the portal's form (the screens are gates 4 and 5).

**The session:** the Pico 2 W, the A&D FX-300i, the air-exposed AR2209, series D's calibration (lags 0.055 s earlier, grains a turn 10.17 and 0.715), B's first errors, `fine_top_rps=1.0`. Four series of 20 throws, the modes taken in turn: accurate at 10 gr, fast at 10 gr, accurate at 20 gr, fast at 20 gr. That is 1,200 gr; the cup holds 600 before a series stops, so the owner empties it between.

**The simulation's forecast** (AR2209's record, 300 throws each, its own calibration, each mode judged at its own band; `fit/study_20261122/mvp_forecast_*gr.txt`):

| Mode | 10 gr: good / over / median | 20 gr: good / over / median |
|---|---|---|
| accurate, ±0.02 | 72% / 26% / 12.5 s | 76% / 22% / 12.9 s |
| fast, ±0.04 | 91% / 8% / 10.0 s | 90% / 10% / 10.6 s |

**What the rig is expected to show against it,** from series D and E: accurate about as the simulation or better (D was 80% good, 20% over, at 11.7 s, where the simulation said 72%); fast worse than the simulation (E was 80% good, 20% over, at 9.2 s, where it said 91%), since the rig's stops scatter more than the simulation's (sd 0.046 against 0.028 gr) and a wider band leaves more to that scatter. At 20 gr the coarse tube pours more and the time grows by about a second. The comparison is judged figure by figure: good, over, median time, the coarse tube's share, and the forecast against the rest (`verify_series.py`).

### 14:50 to 15:18: the session, the two modes at 10 and 20 gr, against the simulation

The owner emptied the cup between the series ("Cup emptied, go"). `recordings/*_series_mvp_{accurate10,fast10,accurate20,fast20}_pico2w_ar2209`, 20 throws each, plus the two of the first check (`mvp_fast10_dry`: 2 good, 7.2 s). The simulation (AR2209's record, its own calibration, 150 throws each) counted the same way.

| Mode, target | Rig: good / over / median | Simulation: good / over / median | Forecast written first |
|---|---|---|---|
| accurate, 10 gr | 70% / 30% / 12.2 s | 73% / 27% / 13.3 s | 72% / 26% / 12.5 s |
| fast, 10 gr | 90% / 10% / 8.2 s | 89% / 11% / 13.2 s | 91% / 8% / 10.0 s |
| accurate, 20 gr | 55% / 45% / 13.8 s | 71% / 25% / 11.4 s | 76% / 22% / 12.9 s |
| fast, 20 gr | 100% / 0% / 12.5 s | 90% / 10% / 11.1 s | 90% / 10% / 10.6 s |

**Where it came from, rig against simulation:**

| | First rests over | Overs after top-ups | Throws topped up | Median, no top-up / topped up |
|---|---|---|---|---|
| accurate 10, rig / simulation | 10% / 11% | 20% / 16% | 45% / 39% | 10.7 / 20.4 s; 10.3 / 17.0 s |
| fast 10 | 5% / 5% | 5% / 6% | 20% / 16% | 7.9 / 16.7 s; 11.2 / 16.2 s |
| accurate 20 | 15% / 11% | 30% / 15% | 40% / 39% | 11.3 / 21.1 s; 10.2 / 16.3 s |
| fast 20 | 0% / 4% | 0% / 6% | 35% / 18% | 8.7 / 20.6 s; 11.0 / 14.4 s |

What it says:
- **The simulation predicts the rig in three of the four.** Accurate at 10 gr, and fast at 10 and 20 gr, are within the noise of 20 throws (one standard error is about 10 points). Accurate at 20 gr is worse on the rig: its top-ups put twice the simulation's overs in (30% against 15%), so 45% over where 25% was forecast. Twenty throws put that at about two standard errors.
- **The over budget of one in 15:** fast held it or came close at both targets on the rig (2 of 20 over, then 0 of 20). Accurate did not (6 and 9 of 20).
- **The time:** fast is 8.2 and 12.5 s, about the owner's 10 s. Accurate is 12.2 and 13.8 s, under its 20 s: it does not spend the time it is allowed. A throw with no top-up takes 8 to 11 s on the rig; one with a top-up 17 to 21 s. The top-ups decide both the time and, in accurate, most of the overs. They took longer on the rig than in the simulation (17 to 21 s against 14 to 17 s): the calibration's move waits (4.4 s for the smallest) are the rig's.
- **The forecast and the calibration still hold** (`verify_series.py` on each series): a rest from the creep lies −0.005 to +0.012 gr from the forecast on average; the coarse tube's grains a turn within 4% of the calibration's, the fine tube's within 0 to 12%.
- **After the verdict:** in 11 throws of 80 the settled weight differed from the rest by a digit (late kernels or the scale's flicker). Fast at 20 gr's two "settled over" are such: +0.04 at rest, +0.06 settled.
- **The step pulses:** no late feed on either motor over the 82 throws (`motor`: 0 and 0), the creep at 0.029 and 0.058 rev/s.
