# ADR-009 research: physics, simulation and control of a throw

2026-11-19. Desk research and simulation, made ahead of ADR-009's design at the owner's request. Nothing here was run on a unit: the probe was in use by ADR-008's work. It is input to ADR-009's revision, not a plan the owner has approved.

**Every simulated figure below rests on an assumed powder model.** The tubes' geometry is measured (from the STLs) and the A&D's reading rhythm is measured (ADR-013); how kernels leave a tube's lip is not. The rankings hold only as far as "Which assumptions decide the result" says.

## The short answer

1. **Overcharges come from one of two places, and the bench must say which.** Either the fine tube still feeds while the scale has not caught up (a control fault, curable), or several kernels leave the lip together (a property of powder and tube, not curable by any stopping rule). In the simulation the second dominates: the overcharge rate follows the clump size whatever the controller is.
2. **AR2209's kernel is the tolerance.** One kernel is 0.020 to 0.025 gr (community figures); the A&D's digit in grains is 0.02 gr; the band is ±0.02 gr. Three kernels falling together from just under the band is an overcharge. The first thing to measure on the bench is how often that happens.
3. **A controller without tuning is within reach, and it is faster.** The candidate (`flow_dose`) takes its numbers from a calibration of about two minutes and the powder's kernel mass. In simulation a 42.5 gr throw takes 12 s against today's 19 s, on every scale it holds the overcharge rate at the clump's floor, and it ends under on about 1% of throws against today's 12 to 14%.
4. **It does not reach 1 in 50 by itself** unless real clumps are rarer than assumed. With single, regular kernels it gives 0.5% over; with the assumed clumps 13 to 16%, the same as today's loop.
5. **Today's loop is tuned to one scale.** On the slower scales it overcharges in 36 to 78% of simulated throws, and its verdict a second after the stop reads too early to show most of them.
6. **High-frequency oscillation is unproven either way.** Nothing published uses it on a rotary tube. Nudges with a pause are established practice (two commercial dispensers, industry's "jog"). Oscillation is a bench experiment judged by one number, the share of nudges that release three or more kernels.

## Corrections to what was said during the session

- **The firmware's speeds are trickler rev/s, not motor rev/s** (`motors.c` divides by the gear ratio). The default 5 rev/s limit is therefore below centrifuging for both tubes (5.8 rev/s large, 8.2 small), not above it as first reported. The large tube at 5 rev/s is at 85% of it, where the bed is thrown across the bore and flow per turn probably falls.
- **The Pico 2 W stores a fine gear ratio of 2.105 (40:19)**; the low-flow tube's STL has 22 teeth (40:22 = 1.818, the firmware's default). If that unit carries the 22-tooth tube, its fine tube turns at 86% of the speed asked. To be checked at the bench.

## What is measured, what is assumed

| Thing | Status | Source |
|---|---|---|
| Tube geometry: bore, rifling, belt teeth, inlet | Measured from the STLs | `tools/charge_sim/geometry/` |
| A&D FX-300i: 20.9 readings a second, 0.02 gr a digit in GN | Measured, documented | ADR-013; A&D manual |
| Today's loop | Ported line for line | `charge_mode.cpp` -> `legacy_pid.py` |
| The Pico 2 W's AR2209 profile | Read from a settings copy | `logs/adr004/pico2w/before_review` |
| AR2209 kernel mass 0.020 to 0.025 gr, length 1.39 mm | Community figures, one published measurement | `sources/powders_physical.csv` |
| Every scale's lag behind the pan | Estimated; nobody publishes it | `sources/scale_dynamics.md` |
| Flow per turn of each tube, the fine inlet's ceiling | Assumed, order of magnitude from geometry | `rigs.py` |
| Kernels per release, how regular the releases are | Assumed | `rigs.py` |
| Kernels that fall after a stop | Assumed | `rigs.py` |
| That reversing a tube takes back what hangs at its lip | Assumed, from the coarse back-off's purpose | `plant.py` |

One check against reality was possible. The owner sees about 1 overcharge in 10 to 15 throws with AR2209 on the A&D; the simulated loop with the assumed plant gives 12 to 15% (1 in 7 to 8). The assumptions were chosen with that in view, so it is a calibration, not a proof, and several different sets of assumptions meet it.

## The owner's seven points

### 1. Adapting to different powders

- **The GRT repository does not help much.** `zen/grt_databases` holds 12 powders shared from Discord, not GRT's database, and not AR2209. Its only physical fields are bulk density (`pcd`) and solid density (`pc`); it has no shape, no dimensions and no kernel mass. Lee's VMD chart (about 170 powders, one method) is a better source for bulk density.
- **What decides dosing is the kernel's mass**, which spans 0.0004 gr (fine ball) to 0.08 gr (N570), and how many leave together. A table of 57 powders with kernel mass and dimensions where found is in `sources/powders_physical.csv`.
- **A profile should hold what the unit measured, not gains:** each tube's flow at five or six speeds, the highest speed worth using, the kernel's mass, what a nudge releases, and the share of nudges that release three or more kernels. The last is the powder's pass or fail for the tolerance asked.
- **Hard cases for these tubes:** long or thick sticks (IMR 4350, H1000, N570: a kernel is the whole band, so ±0.02 gr is out of reach by any control), short-cut lots with a tail of long kernels, and very fine ball that keeps running after the stop. In simulation a 0.04 gr stick gives about 50% over with either controller.

### 2 and 7. The simulation

`tools/charge_sim/` (pure Python, a millisecond a step, about 0.05 s a throw):

- **The plant** (`plant.py`): each tube holds powder; outflow is speed x held x a share per turn; the inlet refills it, with a ceiling for the small tube's one window. Releases are whole kernels, in clumps, with gaps that are more or less regular; slow turning gives avalanche-sized releases; late kernels fall after a stop; reversing retreats the front. Kernels fall 50 mm and land with their impact.
- **The scale** (`scale.py`): dead time, sensor lag, a filter with two speeds (open while the weight moves, closed when it nearly rests), zero tracking, rounding to the digit, a report every interval, the link and the driver's poll, draughts. Four parameter sets: the A&D at FAST and at SLOW, a G&G JJ-B as the driver polls it, a strain-gauge scale at 2 readings a second.
- **Controllers** (`legacy_pid.py`, `flow_dose.py`), a calibration written as a controller (`calibrate.py`), one throw and a session (`throw.py`), many throws (`mc.py`), the comparison (`matrix.py`).

To make it mimic the real drop, in order of worth:
1. the bench's open-loop recordings (below) fitted to the plant's dozen numbers per tube;
2. each scale's lag measured without powder: a small mass placed by hand, and a fine chain lowered into the cup by the trickler's own motor (`sources/scale_dynamics.md`, section 5);
3. a recorded real throw replayed against the simulated one: the whole chain's check.

### 3. Control beyond PID

The loop's faults, as found in the code and the research:
- **The fine tube turns at its least speed until a lagged reading is in band.** What lands after is that flow times the scale's delay, plus a kernel or a clump. That is the blind window.
- **The gains are in rev/s per grain**, so they mix the powder's flow with the scale's delay; a profile tuned on one scale is wrong on another.
- **I and D do nothing useful here.** The plant integrates already, the error never changes sign, and a derivative of a reading rounded to one kernel is noise.
- **The loop's pace is the scale's**, and the verdict is one reading a second after the stop.

The candidate, `flow_dose`:
- **Each tube is asked for a flow, not a speed:** flow = 0.37 / (the scale's delay + its reading interval) x (weight still to go - that tube's margin), through the tube's measured flow map. The 0.37 is the largest gain a proportional loop round a delay can have without overshoot. The scale's delay and the powder's flow are separate numbers, so a new scale needs no new powder tuning.
- **A tube stops for good** when its allowed flow falls under the least it delivers regularly. No floor speed.
- **The finish is nudges on settled readings:** a small turn of the fine tube, a pause for the scale, a judgement on the mean of the readings in the pause (finer than one digit). It ends at the first settled weight no more than 0.02 gr under the target.
- **What a nudge gives is learned** as it goes, as two sums, so one reading's rounding cannot swing it.
- **Sticky powder, print variation, a slow scale** each show as a number the calibration measures (a flatter flow map, a lower yield per nudge, a longer delay), and the same law runs slower. Nothing is tuned by hand.

Tried and dropped in simulation: counting the powder pushed to the lip and not yet seen as already delivered, with a reverse turn when too much hangs there. It gave 14% over against 12 to 16% without it, for more code.

### 4. Avoiding overcharge

- **Never feed into the blind window:** the flow law above, and decisions only on settled readings.
- **Leave the last kernels to nudges**, each sized to release less than one kernel on average.
- **Then the floor is the clump:** the chance that the release which crosses into the band carries three or more kernels. The control agent's sketch and this simulation agree: about 2% over with single kernels and small nudges, about 30% with clumps of two.
- **So the lever after the controller is mechanical or procedural:** the lip, an insert, tilt, or a release aid, each judged by the nudge test. That is outside a controller's reach and should be said to the user as the powder's verdict.
- **Read the verdict when the scale has settled**, not one second after the stop: on a slow scale today's verdict misses most overcharges (shown 14%, true 36% in simulation at the A&D's SLOW).

### 5. High-frequency oscillation

- **For it:** vibration breaks arches and lets grains pass openings that would jam; vibratory feeders are industry's answer for a uniform fine discharge; one patent doses granules with a rotor swinging back and forth.
- **Against, or missing:** no published use of a fast angular dither on a slowly turning tube, and none measuring single-kernel release; vibration beside a balance costs reading noise; in one micro-feeder study constant settings beat a modulated drive.
- **What the hardware allows:** an acceleration of 1 g at the small tube's wall needs ±1.6° at 50 Hz, about half a full step at the motor and within its torque. The belt's elasticity is unknown.
- **Verdict:** do not design around it. Design around nudges, and test oscillation and a forward-then-back flick as kinds of nudge on the bench.

### 6. Failure modes

| Mode | Cause | What answers it |
|---|---|---|
| Over, by a kernel or two | Flow into the blind window | The flow law, the nudged finish |
| Over, by a clump | Kernels leaving together | Not the controller: the lip, or a wider band for that powder |
| Over, after the stop | Late kernels from either tube | Margins sized from the calibration's stops; the coarse back-off |
| Under, shown but not real | A draught, or a reading read too early | Judge on a settled window; adding powder needs readings that agree |
| Under, real | A draught read as weight, so the feed stopped early | The same; a second look before the verdict |
| Slow, or never ends | A nudge that yields nothing (tube starved, hopper empty) | A limit on nudges, then a named end reason |
| Wrong on another scale | Gains that hold the scale's delay inside them | Delay and flow kept apart |

In simulation, six gusts a minute of 0.06 gr leave the candidate's true overcharges where they were and give 12% true unders (6% shown), about what today's loop shows without any draught. The draught model's size is a guess.

## Which assumptions decide the result

400 throws each, 42.5 gr, `python matrix.py`. "Over" and "under" are the unit's verdict rule on the settled truth; time is the median.

| Plant | Today's loop: over, under, time | Candidate: over, under, time |
|---|---|---|
| As assumed (clumps of 1.3, irregular) | 15%, 12%, 18.7 s | 16%, 0%, 12.1 s |
| Single kernels, regular | 1.5%, 11%, 18.7 s | 0.5%, 0%, 10.5 s |
| Single kernels, irregular | 6%, 13%, 18.7 s | 3.5%, 0%, 11.0 s |
| Clumps of 2 | 29%, 11%, 18.6 s | 29%, 0%, 11.1 s |
| Fine tube flow halved | 13%, 17%, 19.9 s | 12%, 0%, 9.3 s |
| Fine tube flow tripled | 15%, 7%, 17.2 s | 12%, 0%, 9.4 s |
| Draughts, 6 a minute | 16%, 16%, 18.7 s | 13%, 12%, 12.3 s |
| A&D at SLOW (5 readings a second) | 36%, 0%, 17.9 s | 12%, 0%, 14.0 s |
| G&G JJ-B, polled | 55%, 2%, 16.1 s | 13%, 1%, 30.9 s |
| Strain gauge, 2 readings a second | 78%, 0%, 15.8 s | 10%, 6%, 30.5 s |
| Fine ball, 0.004 gr | 3%, 4%, 12.3 s | 4%, 0%, 4.8 s |
| Long stick, 0.04 gr | 52%, 12%, 26.0 s | 48%, 0%, 9.4 s |

Read:
- **Clumping moves the overcharge rate from 1% to 29%; nothing else moves it much.** The fine tube's flow, which the first sweep suspected, hardly matters once the powder held in the tube is modelled.
- **The candidate's gain is time, unders, and holding its figure on any scale.** On overs it equals today's loop on the A&D at FAST.
- **A slow scale is slow by physics:** about 30 s with nudges and long settles. The alternative is not a faster good throw.
- The first row moved between 13.5% and 16% for the candidate across two runs of 400 throws: the last digit of these figures is noise.

## The first bench session

Before any controller is built. All of it is open-loop and needs no firmware change: the console's `motor <tube> <rev/s>` gives the speed and the steps counted, `scale tap on` gives every A&D frame with its time. The cup holds about 300 gr and is zeroed between batches; the owner empties it.

1. **The scale alone, no powder:** five minutes at rest, then with each motor turning: noise, drift, what the motors' vibration does to the reading.
2. **The nudge test, fine tube:** 100 small turns with a pause after each, at two sizes. It gives the kernel's mass as the unit sees it, the yield per turn, and the share that release three or more kernels. **This one number says whether 1 in 50 is the controller's to reach or the tube's.**
3. **The flow map, both tubes:** 0.05 to 5 rev/s, each speed until 20 gr or 60 s, three times, with a stop and a settle after each. It gives flow against speed, where it levels off, the scale's delay as the controller meets it, and the late kernels.
4. **History:** a minute slow then a step to fast, against a minute fast. It says whether the powder held in the tube matters (the owner's "speed does not correlate with charge speed").
5. **Ten real throws with today's loop, recorded,** to replay against the simulation.
6. **Optional:** reverse turns (does a retreat take back what hangs?), and the oscillation and flick variants of the nudge.

About two to three hours with powder, most of it step 3. Steps 1, 2 and 5 answer the main question in under an hour.

## Proposed architecture

To be weighed in ADR-009's revision; it keeps the draft's seam (a controller with no hardware in it, a tuner that hands out trials and takes results).

- **A controller module:** readings and times in, two speeds out, a state machine of feed, settle, nudge, done. The simulation's `flow_dose.py` is its reference, with its tests on a PC against the simulated plant.
- **A scale record, kept with the scale:** its reading interval (measured already), its delay and settle time (measured by any stop).
- **A powder profile of measurements:** the two flow maps, kernel mass, nudge yield, nudge quality. It replaces six gains and four speed limits, so the stored struct changes (ADR-014 is to migrate profiles in any case).
- **A calibration job** of about two minutes into the cup, ending in a verdict about the powder, started from the screen and the web.
- **Every throw recorded,** as the draft and api_v1.md already plan, with the stage it was in: the same rows the simulation writes.
- **The verdict on a settled weight,** and an end reason when a nudge limit is hit.

## Roadmap

Each gate ends in a pass or fail on a unit.

1. **Bench session 1** (above). Pass: the simulation, refitted, reproduces the recorded throws' time and overcharge rate. It also settles whether the target is the controller's or the tube's.
2. **Recordings and the open-loop test functions in the firmware** (the draft's "every throw recorded" and "a motor at a speed for a time"), so later sessions need no probe.
3. **The nudged finish alone,** put after today's loop with its floor speed removed. Pass: from 0.1 gr under, 50 throws end in band with no more overs than the nudge test predicts.
4. **The flow law and the calibration** for both tubes. Pass: the continuous feed ends two to five kernels short in nearly all throws; a 42.5 gr throw in 10 to 12 s on the A&D.
5. **Scales:** the G&G's driver read at once after its ask (a quarter second of its lateness is ours), the same controller on it. Pass: no worse overs than on the A&D, time as predicted.
6. **Automatic tuning as refinement in use,** the profile trimmed throw to throw with a cap, a dead band and no learning from a disturbed throw.
7. **If gate 1 says clumps are the limit:** the lip, an insert, or oscillation, each judged by the nudge test. A hardware question for the owner.

## Open questions for the owner

- **How long does a throw take today,** for which target? The simulation says 19 s for 42.5 gr; the owner aims at 10 and accepts 15. It anchors the coarse tube's assumed flow.
- **Which small tube and pulley are on the Pico 2 W?** Its stored gear ratio is 40:19; the low-flow STL is 22 teeth.
- **The A&D's settings:** response FAST, `SPd 2`, and `trc` (zero tracking, which the maker says to turn off for very light samples and which neither set-up guide mentions).
- **Is a mechanical change to the fine tube's lip in scope** if the nudge test says clumps are the limit?
- **PR #113's measured stick-powder data** was not read (the owner asked for a judgement independent of the PRs). It may hold real flow figures. Read it now for its data only, or leave it?

## Sources

In `sources/`, each with its own list of references and what it could not verify:
- `tube_flow_physics.md`: grains through a rotating rifled tube (flow dynamics only, by the owner's direction);
- `dosing_control.md`: industry's batch weighing, control theory for a delayed one-way integrator, other dispensers, three architectures ranked;
- `scale_dynamics.md`: each driver's timing from the code, each make's manual, a scale model, a bench procedure;
- `powders_physical.csv`, `grt_powders.csv`: the powder tables. The powder report itself was returned as text and is summed up in point 1 above.
