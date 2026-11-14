# ADR-013: the tricklers move only on a real, fresh reading

Status: **Open** 2026-11-15. Approved by the owner the same day, with the first views below. The real scale is measured; left: the faults shown, the three fixes. What is built and measured is in [progress.md](progress.md).

## What we want

- During a charge the tricklers move only on a weight that was really read, and read recently for that scale.
- They move only as the running mode commands.
- A unit whose scale sends nonsense or stops sending pauses, and stops for good if it does not come back, and says why.
- A scale that is slow by nature is not taken for a failed one.

## Owner's direction (2026-11-15)

- A small charge-safety ADR, run before ADR-012. Asked "Draft a small charge-safety ADR to run before ADR-012?": "Yes, draft it".
- The real scale measured first: "Yes, add it".
- **Thresholds for the real failure, not for a slow scale:** "Not all scale can report the same speed like A&D, some slow scales, for example, G&G, can only go about 4 samples per second. Therefore your threshold should be set to accommodate the real failure, not a slow response scale."
- **Slow and lagging scales are ADR-009's:** "The slow response scale tuning should be a part of ADR-009, for example, the note about G&G JJ223BF note I left in the script. Some scale may still report at the constant speed, but unable to report its up-to-date measurement."
- **Pausing as a start, bounded to the controller:** "Yes I think pausing is a good start, but the action MAY needed to be bounded to the controller, where ADR-009 will address."
- **A pause still lets powder fall, and takes time to slow down:** "You know the powder will still drop due to your pause action, and your pause take time due to the deacceleration." So a resume waits for the scale to show what fell, and an intermittent scale has a limit. Asked "Write this resume rule into ADR-013?": "Yes, write it".

## Why here

- **These faults can overthrow powder on main today,** in normal use, with the scales users have. The fixes are small and stay in the scale drivers, charge mode and cleanup mode's web handler.
- **Before ADR-012.** The motor cleanup is the largest of the cleanups; done first, it would leave this risk on main for its whole length. Its "motors that stop when the firmware stops" is the other half of the same rule: this ADR covers a firmware that runs but is fed a bad reading or none; ADR-012 covers a firmware that stopped.
- **Before ADR-002.** `charge_mode.cpp` is the one file every live PR touches. A small change to it now costs them little.
- ADR-001's tools prove it, on ADR-011's base.

## What we know (read in the code, 2026-11-15)

**How a reading reaches the charge**
- Eleven files write the weight into one shared field, `scale_config.current_scale_measurement`, and then give the semaphore that says a reading is ready: each of the nine scale drivers, the simulated scale, and `scale.c`. Nothing between them and charge mode looks at the value.
- A frame that does not parse:
  - is published as NaN by seven drivers: A&D, Creedmoor, G&G, JM Science, Radwag, Steinberg, US Solid (`return nanf(...)`);
  - is published as 0.0 by the Sartorius driver: `strtof` converts nothing and returns 0, and the driver does not check. To the charge it looks like an empty pan;
  - is not published by the generic driver, which checks. But `strtof` also reads the text "inf" and "nan", so a frame holding either is published as such.
- The field is NaN at power-up and when the simulation stops (`scale.c:213`, `:271`).

**What the charge loop does** (`charge_mode_wait_for_complete()`)
- Each pass waits up to 200 ms for a reading (`charge_mode.cpp:312`). With none, it polls the buttons and waits again, with no limit. Both motors keep their last speeds the whole time. Only the reset button ends it; that exit disables both motors (`:627`).
- **What a NaN reading does depends on the chip** (found while measuring, 2026-11-15; to be shown on both by `sim bad`):
  - **RP2350 (Pico 2 W), with a floating-point unit that follows IEEE:** both errors are NaN, and every comparison with NaN is false, so neither stop condition fires (`:328`, `:337`). The PID terms are NaN; each trickler's integral keeps the NaN for the rest of the throw (`:358`, `:370`); and `fmaxf(min, fminf(NaN, max))` gives the maximum (`:365`, `:377`). So one bad frame puts the tricklers at full speed until a later good reading crosses the stop threshold. The overthrow is what full speed pours in that time.
  - **RP2040 (Pico W), with no floating-point unit:** the SDK's float code, built without `PICO_FLOAT_PROPAGATE_NANS` (it is not set anywhere here), treats a NaN as infinity. A NaN as the drivers make it, positive, reads as +∞; the fine error is then −∞, the fine stop fires at that reading, and the throw ends short and is taken as done. The reading a second later is a fresh one, so the LEDs would say under. Wrong, but an undercharge, not an overthrow.
- With a reading of 0.0 in the middle of a charge, that pass's error is the whole target: the P term jumps and the integral takes the whole target once, for the rest of the throw.

**The cleanup speed from the web**
- `/rest/cleanup_mode_state` with `s1` sets the cleanup speed and calls `motor_set_speed(SELECT_BOTH_MOTOR, ...)` whatever the mode (`cleanup_mode.cpp`). During a charge both motors are enabled, so one request sets both tricklers to the cleanup speed. The charge loop sets the fine trickler again at its next reading. The coarse one, once it has stopped for the throw, is not set again, and runs on until the charge ends.

**Three ways a scale fails the controller, and which are this ADR's**
- **Slow by nature.** Scales report at different rates. The A&D streams on its own; the G&G driver asks for each reading and waits 250 ms between asks (`gng_scale.c`), so it gets at most 4 a second (the owner: "about 4 samples per second"). That is the scale working. Its control is ADR-009's.
- **On time but late.** A scale that averages reports at a steady rate a value that trails the pan. The owner's G&G manual (`manuals/configure_g&g_scales.md`) sets the JJ223BF's averaging window (C2) to 1, where its default 4 is the longest, and its sensitivity (C1) to 1. Its timing cannot show it, so no threshold here sees it. ADR-009's.
- **Missing.** Readings that stop coming, for a moment or for good: a loose cable, a scale switched off, a dead line. Or that come and do not parse. **This ADR's**, and only this.

**What the bench's real scale sends** (the Pico W's A&D FX-300i; measured with `scale tap` and `rc_run.py scale_frames`, 2026-11-15; [progress](progress.md))
- **Only numbers.** 4,418 frames, every one parsed as a number: 2,512 idle over 2 minutes, 417 across five hand-overs from the simulated scale, 231 across two restarts, 1,258 over a minute with both motors turning (coarse 5 rev/s, fine 2, the hopper empty). A frame reads `ST,+00000.00 GN` and a CR LF.
- **About 21 a second:** 41 ms between frames typically, 62 ms at the longest, the same with the motors turning. The driver looks every 20 ms, so the times come in steps of it.
- **The `inf` of ADR-011's logs was not the scale.** 287 `state` replies read `inf` that day, each right after the simulated scale handed back. The firmware puts NaN in the field then (`scale.c:271`), and the Pico W prints a NaN as `inf` (above). Shown: after a hand-over of 30 s, two `state` replies read `inf` before the scale's first frame, which was 0.0, came. The Pico 2 W prints the same as `nan`.
- So on this scale a frame that does not parse has not been seen, mid-charge or ever. Fault 1 stays: for the other drivers, for a frame broken on its way, and for the NaN the firmware itself writes.
- The Pico 2 W's unit has no scale.

**What can show it on the bench**
- The simulated scale publishes a reading every 100 ms through the same field. It cannot yet send a bad frame or go quiet.
- The real A&D cannot be made to send a bad frame. Pulling its cable is the real test of silence.

## What we will do

**One rule, kept at one door.** The motors move only on a real, fresh reading, and only as the running mode commands. "Fresh" is judged by the scale's own rhythm, so a slow scale is not a failed one.

1. **One way to publish a reading.** A function in `scale.c` that every driver and the simulated scale call in place of writing the field. It is given whether the frame parsed and the value. A frame that did not parse, or a value that is not a finite number, is not published, and is counted (`bad_frames` in `stats`). So a bad frame becomes a frame that never came. The Sartorius driver says when it parsed nothing, as the others do.
   - The same function notes when each reading came, and keeps the scale's usual interval: the median of its recent intervals. A charge waits for ten steady readings near zero before it pours, so the interval is known before the motors start, for whichever scale is fitted.
2. **Missing readings: pause, settle, then go on or stop.** In the controller's step of the charge loop, the part ADR-009 lifts into its controller module.
   - **Stale:** no reading for 3 of the scale's usual intervals. Both tricklers are commanded to 0 with their usual ramp, and the charge pauses. The servo gate is left where it is.
   - **What a pause costs.** It cannot take back powder: the tricklers have poured for 3 intervals with no news (one of them as in any running, between two readings), they turn on through the ramp down (0.1 s from the coarse trickler's 5 rev/s at 50 rev/s², about a quarter turn, on the bench's settings), and powder in the air or on the tube's lip lands after they stop, as at every stop. On the bench's settings, about 0.6 coarse turns before the stop on the A&D (3 of its 41 ms) and 3.75 on a G&G (3 of its 250 ms), and under half that on the fine trickler.
   - **Resume, never on the first reading back.** The charge goes on only when:
     1. readings are back on the scale's rhythm, several in a row at its usual interval; and
     2. the weight is steady, by the charge's own zero test less its zero: ten readings taken 300 ms apart (about 3 s), their spread under `set_point_sd_margin` (0.02 by default). The test's mean margin, which asks for zero, does not apply. Then what fell has landed, and a scale that lags has caught up. Each pause so costs the throw at least 3 s.

     The settled weight decides. At or past the fine stop, the throw ends as a normal one, with its verdict. Past the coarse target, the coarse trickler stays stopped and the fine one goes on. Otherwise the charge goes on: the controller's D term starts again as on a first sample, so the gap makes no spike; the I term took nothing during the pause, since no reading came, and goes on as it was.
   - **Lost:** no reading for 10 usual intervals, and never under 1 s. Both stop, the servo gate goes to its closed position if it is enabled, and the charge ends with "Scale: no reading" on the screen and the LEDs in the not-ready colour. A press of a button goes back to the menu.
   - **Unsteady:** readings came back but are not steady within twice the steady test's time (about 6 s). The throw ends as at a lost scale, with "Scale: unsteady".
   - **Intermittent:** pauses are counted in a throw. Two are borne; a third ends the throw as at a lost scale, with "Scale: unreliable". A scale that drops out again and again in one throw is not trusted to finish it.
   - For the bench's A&D, measured at 41 ms between frames (about 21 a second, 62 ms at the longest), the figures are 0.12 s and 1 s; for a G&G at 4 a second, 0.75 s and 2.5 s. The pause's 0.12 s sits above the A&D's longest normal gap, 62 ms.
   - **This is a floor, not the controller's answer:** on a reading that is not fresh, nothing moves, and nothing moves again until the weight is known. How much a pause adds on real powder, and how the controller should act beyond the floor, such as slowing rather than stopping, backing the coarse trickler off at a pause, or allowing for what is still falling or for a scale's lag, is ADR-009's.
   - It covers a scale that went quiet and one that sends only nonsense, since its frames are no longer readings.
3. **The cleanup speed applies only in cleanup mode.** The web may set it at any time; it reaches the motors only while cleanup mode runs, as from the screen.

**The real scale measured first** (the owner, 2026-11-15). Before any fix, what the bench's A&D sends is recorded, so this ADR says how real the bad frame is for it, and not only that the code allows it:
- A console command, `scale tap on|off`, never stored: while it is on, the A&D driver copies each frame it decodes to the console as it came, with the value it made of it. A test tool, as `sim on` is.
- `rc_run.py` learns to record the frames for a time and sum them up: how many, how many read as a number, as `inf`, as NaN, and the text of each that did not read as a number; and the time between frames, its median and its longest, idle and with the motors turning. That is the bench scale's rhythm, which the pause's and the stop's figures are checked against.
- Recorded in three conditions, with the hopper empty:
  - idle, for some minutes;
  - across restarts and hand-overs from the simulated scale, where `inf` was seen;
  - through a minute of a charge on the real scale, with both motors turning, which is the condition of a real throw, noise of the motors on the serial line included. The charge is stopped by the console at the end.
- What is found is written into "What we know" before the code changes. If the A&D never sends a bad frame mid-charge, fault 1 stays, for the other scales' drivers and for a noisy line, but this ADR says so plainly.

**The faults shown first, then fixed.** Each fault is shown failing on the code as it is, so the proof shows a change:
- The simulated scale learns `sim bad` (its next reading is a frame that does not parse), `sim quiet <ms>` (it publishes nothing for that long) and `sim rate <per second>` (it reads at that rate, 4 for a G&G's). Console commands only, never stored, as `sim on` is.
- `rc_run.py` learns to run a charge with a fault put in at its middle: a bad frame, a quiet scale, a cleanup speed from the web. It reports the motors' speeds through it.
- These are built and run first, alone, on the code before the fixes: they must fail there. Then the three fixes, one commit each, each turning its test from fail to pass.

## How the unit shows it works

On the bench's Pico W. The fixes are the same code on both chips. The bad frame's fault, though, does not act the same on the two (above), so it is also shown failing on the Pico 2 W, where it is the overthrow, when the owner puts that unit on the bench; its charges use the simulated scale.

- **A bad frame mid-charge:** the charge goes on as if it never came. The throw ends within the stop threshold, as a clean one does, and `stats` counts one bad frame.
- **A short gap mid-charge** (`sim quiet` for 5 intervals): both motors at 0 within 3 intervals and a ramp of the last reading; no motor moves until readings are back on rhythm and steady; then the charge goes on and ends within the stop threshold.
- **A gap after the fine stop is passed:** when the settled weight is past it, the throw ends with its verdict and the tricklers do not start again.
- **A long gap mid-charge** (`sim quiet` for 3 s): the pause as above, then the stop at the lost figure; the gate closed if it is enabled, the message on the screen (`snap`), the menu back after a press, and the next charge passes.
- **An intermittent scale:** three short gaps in one throw: two pauses that resume, and the third ends the throw with "Scale: unreliable".
- **A slow scale is not a failed one:** charges with the simulated scale at 4 readings a second pass with no pause and no stop. A gap of 2 intervals at either rate pauses nothing.
- The simulated scale has no powder in the air and no lag, so these prove the logic: pause, settle, go on or stop, count. How much powder a pause adds is not shown here. Nor is "unsteady": the simulated weight holds still in a pause, so that path is read in the code, not run.
- **A cleanup speed sent from the web mid-charge:** no motor changes speed. In cleanup mode it applies as before.
- **The same three on the code before the fixes fail,** recorded in `progress.md`.
- **The real scale's frames,** idle, across restarts and through a minute of charging, summed up in `progress.md`. With the fixes in, the same run shows each frame that does not read as a number counted in `stats` and never reaching the charge.
- `charge_cycle` passes, `menu_tour --compare` finds every form equal (the message is drawn by charge mode, not a menu form), and the stored settings are unchanged: no stored struct changes.
- The owner's check, which the owner may waive: the scale's cable pulled during a charge with the hopper empty; the motors stop and the screen says why.

## Not in this ADR

- The motors when the firmware itself stops (ADR-012, fault 1), and the rest of the motor code.
- The audit's other findings, such as the web form cut at 127 characters, storage and Wi-Fi. The cleanup's scope is proposed on its own.
- The control loop's own faults (ADR-009): the D term's first sample, the I term not scaled by time. No bad reading reaches the loop after this ADR, but its sums are not changed here.
- Control for slow scales and for scales whose value lags, and how the controller should act on stale readings beyond the pause: slowing rather than stopping, a back-off at a pause, allowing for what still falls (ADR-009, by the owner's word).
- How much powder a pause adds on real powder: ADR-009 measures it, with PR #113's powder model and its fall after the stop, and recordings.
- A stop with no ramp. It would save about a quarter turn; the ramps are ADR-012's.
- Telling the user outside a charge that the scale is silent.

## Settled at approval (2026-11-15: "Approve")

Each first view below was approved as it stands.

- **The figures:** pause at 3 of the scale's usual intervals, stop at 10 and never under 1 s, as a first view. They follow each scale's rhythm, so the G&G's 4 a second trips neither. The A&D's rhythm and its longest normal gap, with the motors turning, are measured first; the pause's figure must sit above that longest normal gap.
- **What the unit does at a lost scale:** first view, stop, close the gate, show why, and back to the menu on a press. A short gap only pauses, and goes on from the settled weight (the owner: "pausing is a good start"); a long one is a scale that has failed, and going on after it is the risk this ADR removes.
- **How many pauses a throw bears:** two, as a first view; the third ends it.
- **How "steady" is judged:** first view, the charge's own zero test without its zero: ten readings 300 ms apart, spread under `set_point_sd_margin`, so nothing new is tuned. It costs each pause about 3 s; "unsteady" after about 6 s.
- **Where the bad-frame count shows** beyond the console's `stats`: the web, the screen, or nowhere.
