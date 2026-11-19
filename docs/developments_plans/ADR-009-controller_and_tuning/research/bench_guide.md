# ADR-009: the first bench session, for the agent who runs it

Written 2026-11-19 by the research session, which never touched a unit. It says what to measure, in what order, with which commands, and what each result decides. Read `README.md` beside this first (what is assumed), and AGENTS.md's bench rules.

Nothing here needs a firmware change except step 7. Every command below exists on the branch's firmware today (`AGENTS.md`, "Proving a change on a unit"); none was tried by the author of this guide, so step 0 proves the recording chain before powder is spent on it.

## Learned at the first session (2026-11-19), read before following the steps

- **Pause 3 s after a nudge, not 1.5 s:** this scale takes up to 2.6 s to show one kernel. Shorter pauses count kernels to the next nudge.
- **Judge clumps per release, not per nudge:** most small nudges release nothing, which waters the share down. The number is "of the trials that released anything, how many released three kernels or more".
- **Plan for several hundred trials a kind:** a release comes in about one trial of four.
- **Do not leave `scale tap on` running for long:** the scale task's free stack fell to 48 during the session (`stats`). `record.py flick` reads weights by `state` instead.
- **The recorder exists:** `tools/charge_sim/bench/record.py` (`chain`, `nudges`, `runs`, `steps`, `flick`, `show`), with `lag.py`, `stop_shape.py` and `flick_summary.py` beside it.
- **Done already:** steps 0 (with powder), 2, 3, 4 (one pass, coarse to 3 rev/s) and a console-only part of 7. Their results and what is left are in `HANDOVER.md` and `../progress.md`.

## Before any contact

1. **Ask the owner that the bench is ready**, which unit is on the probe, and whether powder is in the hopper. The session wants the Pico 2 W with the A&D FX-300i, the large tube and the low-flow small tube, and AR2209.
2. **Copy the unit's settings** (`rc_run.py settings --host <address> --save <folder>`).
3. **Check the fine motor's gear ratio.** The Pico 2 W stored 2.105 (40:19, a generation-1 figure, the owner 2026-11-19); the generation-2 low-flow tube has 22 teeth, 1.818. With 2.105 the tube turns at 86% of the speed asked. Ask the owner before changing it; record which value every recording was made with.
4. **Ask the owner for the A&D's settings:** response (FAST wanted), `SPd` (2 wanted: 20 readings a second), `trc` (zero tracking: off wanted, the maker's advice for very light samples). A usual interval near 200 ms in `GET /api/v1/scale` means the response was set to MID or SLOW.
5. **The cup holds about 300 gr.** Keep a running total of what was poured; ask the owner to empty it before it passes 250 gr. The scale may be zeroed between batches.
6. **Stop and say so** on: an `@err`, a reading that is not a number, a motor fault, a weight that falls by more than 0.1 gr (the cup was touched), or powder anywhere but the cup.

## What a recording is

Two streams on the unit's own clock, kept whole in the session's log:
- `scale tap on` prints each A&D frame as `@sf <ms since boot> <value> <frame in hex>`; the frame's header (`ST` stable, `US` unstable) is in the hex.
- `motor` alone prints `@ok motor t_us=... coarse_made=... fine_made=...`: the steps made, with the time. Two replies give the turns between them. **Take the angle turned from these counts, never from the pause asked for**: a pause is kept on the PC and through the probe, and may be tens of milliseconds off.

A run is: `scale tap on`, `motor`, `motor <tube> <rev/s>`, a pause, `motor <tube> 0`, `motor`, a pause to settle, `motor`, `scale tap off`. Write a small recorder that does this through `rc_run.py`'s session and saves one CSV a run (`t_ms, weight, stable`) and one line of facts (tube, speed asked, steps and times before and after). Put it in `tools/charge_sim/bench/` with its analysis. Keep the raw logs in `logs/adr009/`.

## The session

Each step says what it decides. Steps 0 to 3 and 6 are the hour that answers the main question; 4 and 5 fill the model in.

### 0. The chain, hopper empty (10 minutes)

- Five runs of the fine motor at 0.5 rev/s for 2 s with the tap on.
- **Pass:** about 21 `@sf` lines a second with no gap over 62 ms; the step counts give 1.00 turn within 2%; the weight stays within one digit. If the count is off by the gear ratio, step 3 above was the cause.
- Then 5 minutes at rest and 2 minutes with each motor turning: the noise, the drift, and what the motors' vibration does to the reading. **This is the scale's floor;** every later judgement of "nothing fell" rests on it.

### 1. The measurement's lag, no powder (15 minutes)

What the owner asked on 2026-11-19: how long after the weight changes does the reading change.
- **By hand:** a snip of wire of about 20 mg lowered onto the cup with tweezers, ten times on and ten off, tap on. It gives the response's shape from its first movement (half-way time, time to within a digit, whether the stable flag drops) but not the moment of contact.
- **With a time zero**, if the owner has a fine chain: hang it from a thread wound on the fine tube's pulley so that turning lowers it into the cup (`sources/scale_dynamics.md`, section 5, test C). The motor's steps give the mass lowered at every millisecond: the reading's delay behind it is the lag, at three speeds.
- Without a chain, the lag comes from step 3's stops, lumped with the fall (about 0.1 s for 50 mm).
- **The simulation's figures to replace** (`python scale_step.py`): A&D at FAST, one kernel shows after 0.25 s; a grain is half shown at 0.21 s and within a digit at 0.5 s. They follow from estimates.

### 2. Prime, with powder

Run each tube at 1 rev/s until powder falls steadily, then 10 s more. Stop, wait 5 s. Zero the scale. From here every run is preceded by 5 s at 1 rev/s and 5 s still, so each starts from the same state.

### 3. The nudge test, fine tube (20 minutes). **The main question.**

- 100 nudges: `motor fine 0.5`, a pause of 40 ms, `motor fine 0`, then 1.5 s still. Then 100 more with a pause of 100 ms.
- For each: the turn (from the step counts) and the gain (mean of the readings in the last 0.7 s still, less the same before).
- **What to compute:**
  - the kernel's mass as the unit sees it: the smallest gain that repeats (the first peak of the gains' histogram above the floor);
  - the yield, gr per turn: the sum of the gains over the sum of the turns;
  - **the nudge quality: the share of nudges that gained three kernels or more.**
- **What it decides:**
  - under 3%: 1 overcharge in 50 is within the controller's reach;
  - 3 to 10%: reachable only with a wider margin and more nudges, slower;
  - over 10%: clumps are the limit. The owner prefers oscillation to a changed lip (2026-11-19): go to step 7.
- Also look for "nothing, nothing, then several": gains against the count of empty nudges before them. The simulation assumes a long wait makes a larger fall; if the data shows it, a controller can see a clump coming.

### 4. The flow map, both tubes (60 to 90 minutes, most of the powder)

- Speeds 0.05, 0.1, 0.2, 0.5, 1, 2, 3.5, 5 rev/s in a shuffled order, three times each. A run ends at 20 gr or 60 s for the coarse tube, 5 gr or 60 s for the fine. After the stop, 5 s still.
- **From each run:** the flow (gain over the time run); what showed after the stop command (the scale's delay times the flow, plus late kernels); the time to settle.
- **What it gives:** flow against speed; the speed where it levels off (the fine tube's inlet) or falls (the large tube near 5 rev/s); the delay as a controller meets it; the scatter of repeats at one speed, which says whether a flow map means anything (over 30% from run to run: it does not, and fixed stages with learned cutoffs are the fallback).
- **Set against the simulation:** it assumes 12 gr a turn for the large tube, 1.35 gr a turn and a ceiling of 2.4 gr/s for the small one, and a delay of 0.25 s.

### 5. History (15 minutes)

- A: the fine tube 60 s at 0.1 rev/s, then at once 20 s at 2 rev/s. B: 60 s at 2 rev/s. Three times each.
- **What it decides:** if A's first seconds at 2 rev/s give much more than B's steady flow, the powder held in the tube matters, and it is one cause of the owner's "speed does not correlate with charge speed". If not, the held-powder state leaves the model.

### 6. Ten throws with today's loop, recorded (10 minutes)

- 42.5 gr, the unit's AR2209 profile, tap on, `state` read every 50 ms for the speeds (`charge_fault none` records a charge this way; a real scale needs `sim off`).
- **What it gives:** the time of a throw (the owner's design: 7 to 15 s depending on clumps), where each landed, and a recording to replay in the simulation. **The simulation passes when, refitted from steps 3 to 5, it gives these throws' time within 2 s and their spread of final error within a half.**

### 7. Oscillation, if step 3 says clumps (needs a small firmware command)

The console cannot time a 20 to 50 Hz motion from the PC. It needs one test command in a research build, flashed by the probe, never merged as it stands: `motor <tube> wiggle <degrees> <hertz> <seconds> [mean rev/s]`, the tube swung about its place (or about a slow turn) by the motor task.
- The nudge test again, with each nudge replaced in turn by: a swing of ±1.6° at 50 Hz for 0.2 s with no net turn; the same at 20 Hz, ±10°; the swing on top of a 40 ms nudge; a flick (forward 5°, back 5°), which the console can already do.
- **Judge each by the same number** (the share of releases of three kernels or more) and by step 0's check of the reading's noise while it swings. Acceleration at the small tube's wall reaches 1 g at ±1.6° and 50 Hz.
- A variant that halves the share is worth a place in the controller's finish; one that only adds noise is dropped.

### 8. Reverse (5 minutes, optional)

Ten times: nudge until a gain, three more nudges that gain nothing, a reverse turn of 0.05, then nudges again. Does the reverse delay the next gain? It says whether a retreat takes back what hangs at the lip, which the coarse back-off assumes.

## After the session

1. Put the session's facts into `progress.md`: what was run, the figures above, what did not work.
2. Refit `tools/charge_sim/rigs.py` (the tubes' numbers, the powder's clump and regularity) and `scale.py` (the A&D's delay), each with its source in a comment, and rerun `matrix.py`.
3. Tell the owner the verdicts of steps 3 and 6 first.
