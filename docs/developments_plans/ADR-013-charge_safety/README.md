# ADR-013: the tricklers move only on a real, fresh reading

Status: **Draft** 2026-11-15. Not approved. Captures three faults read in the code that make the tricklers run when they should not, how to fix them, and how the unit shows it. Nothing has been built.

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
- With a NaN reading both errors are NaN, and every comparison with NaN is false, so neither stop condition fires (`:328`, `:337`). The PID terms are NaN; each trickler's integral keeps the NaN for the rest of the throw (`:358`, `:370`); and `fmaxf(min, fminf(NaN, max))` gives the maximum (`:365`, `:377`). So one bad frame puts the tricklers at full speed until a later good reading crosses the stop threshold. The overthrow is what full speed pours in that time.
- With a reading of 0.0 in the middle of a charge, that pass's error is the whole target: the P term jumps and the integral takes the whole target once, for the rest of the throw.

**The cleanup speed from the web**
- `/rest/cleanup_mode_state` with `s1` sets the cleanup speed and calls `motor_set_speed(SELECT_BOTH_MOTOR, ...)` whatever the mode (`cleanup_mode.cpp`). During a charge both motors are enabled, so one request sets both tricklers to the cleanup speed. The charge loop sets the fine trickler again at its next reading. The coarse one, once it has stopped for the throw, is not set again, and runs on until the charge ends.

**Three ways a scale fails the controller, and which are this ADR's**
- **Slow by nature.** Scales report at different rates. The A&D streams on its own; the G&G driver asks for each reading and waits 250 ms between asks (`gng_scale.c`), so it gets at most 4 a second (the owner: "about 4 samples per second"). That is the scale working. Its control is ADR-009's.
- **On time but late.** A scale that averages reports at a steady rate a value that trails the pan. The owner's G&G manual (`manuals/configure_g&g_scales.md`) sets the JJ223BF's averaging window (C2) to 1, where its default 4 is the longest, and its sensitivity (C1) to 1. Its timing cannot show it, so no threshold here sees it. ADR-009's.
- **Missing.** Readings that stop coming, for a moment or for good: a loose cable, a scale switched off, a dead line. Or that come and do not parse. **This ADR's**, and only this.

**What the bench's real scale sends** (the Pico W's A&D FX-300i; `state` replies in ADR-011's logs, 2026-11-14)
- In steady running its readings are numbers: 34 of 34 in a 30-minute run with no restart.
- For its first readings each time its driver starts, at boot and when the simulated scale hands back to it, it reads `inf`: 287 of the `state` replies of the day, all with no charge running. The field starts as NaN, so `inf` is a frame that was parsed. What those frames hold is not known: the firmware does not keep them.
- They are harmless where they were seen: a charge first waits for ten steady readings near zero, and `inf` never passes.
- No charge ran on the real scale that day; every charge used the simulated one. So whether this scale sends a frame that does not parse in the middle of a charge has not been seen either way.
- The Pico 2 W's unit has no scale.

**What can show it on the bench**
- The simulated scale publishes a reading every 100 ms through the same field. It cannot yet send a bad frame or go quiet.
- The real A&D cannot be made to send a bad frame. Pulling its cable is the real test of silence.

## What we will do

**One rule, kept at one door.** The motors move only on a real, fresh reading, and only as the running mode commands. "Fresh" is judged by the scale's own rhythm, so a slow scale is not a failed one.

1. **One way to publish a reading.** A function in `scale.c` that every driver and the simulated scale call in place of writing the field. It is given whether the frame parsed and the value. A frame that did not parse, or a value that is not a finite number, is not published, and is counted (`bad_frames` in `stats`). So a bad frame becomes a frame that never came. The Sartorius driver says when it parsed nothing, as the others do.
   - The same function notes when each reading came, and keeps the scale's usual interval: the median of its recent intervals. A charge waits for ten steady readings near zero before it pours, so the interval is known before the motors start, for whichever scale is fitted.
2. **Missing readings: pause, then stop.** In the controller's step of the charge loop, the part ADR-009 lifts into its controller module:
   - **Stale:** no reading for 3 of the scale's usual intervals. Both tricklers are commanded to 0 and the charge waits. The next reading resumes it where it was. A scale that drops out for a moment makes the trickler stutter, not overthrow.
   - **Lost:** no reading for 10 usual intervals, and never under 1 s. Both stop, the servo gate goes to its closed position if it is enabled, and the charge ends with "Scale: no reading" on the screen and the LEDs in the not-ready colour. A press of a button goes back to the menu.
   - For the A&D at about 10 a second, that is 0.3 s and 1 s; for the G&G at 4 a second, 0.75 s and 2.5 s. The bench's figures are measured first.
   - This is a floor, not the controller's answer: on a reading that is not fresh, nothing moves. How the controller should act on missing, slow or late readings beyond that, such as slowing rather than stopping, or allowing for a scale's lag, is ADR-009's.
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

On the bench's Pico W. The change is the same code on both chips, so no Pico 2 W pass unless the owner asks for one.

- **A bad frame mid-charge:** the charge goes on as if it never came. The throw ends within the stop threshold, as a clean one does, and `stats` counts one bad frame.
- **A short gap mid-charge** (`sim quiet` for 5 intervals): both motors at 0 within 3 intervals and a ramp of the last reading, then the charge resumes when readings return, and ends within the stop threshold.
- **A long gap mid-charge** (`sim quiet` for 3 s): the pause as above, then the stop at the lost figure; the gate closed if it is enabled, the message on the screen (`snap`), the menu back after a press, and the next charge passes.
- **A slow scale is not a failed one:** charges with the simulated scale at 4 readings a second pass with no pause and no stop. A gap of 2 intervals at either rate pauses nothing.
- **A cleanup speed sent from the web mid-charge:** no motor changes speed. In cleanup mode it applies as before.
- **The same three on the code before the fixes fail,** recorded in `progress.md`.
- **The real scale's frames,** idle, across restarts and through a minute of charging, summed up in `progress.md`. With the fixes in, the same run shows each frame that does not read as a number counted in `stats` and never reaching the charge.
- `charge_cycle` passes, `menu_tour --compare` finds every form equal (the message is drawn by charge mode, not a menu form), and the stored settings are unchanged: no stored struct changes.
- The owner's check, which the owner may waive: the scale's cable pulled during a charge with the hopper empty; the motors stop and the screen says why.

## Not in this ADR

- The motors when the firmware itself stops (ADR-012, fault 1), and the rest of the motor code.
- The audit's other findings, such as the web form cut at 127 characters, storage and Wi-Fi. The cleanup's scope is proposed on its own.
- The control loop's own faults (ADR-009): the D term's first sample, the I term not scaled by time. No bad reading reaches the loop after this ADR, but its sums are not changed here.
- Control for slow scales and for scales whose value lags, and how the controller should act on stale readings beyond the pause (ADR-009, by the owner's word).
- Telling the user outside a charge that the scale is silent.

## To settle before approval

- **The figures:** pause at 3 of the scale's usual intervals, stop at 10 and never under 1 s, as a first view. They follow each scale's rhythm, so the G&G's 4 a second trips neither. The A&D's rhythm and its longest normal gap, with the motors turning, are measured first; the pause's figure must sit above that longest normal gap.
- **What the unit does at a lost scale:** first view, stop, close the gate, show why, and back to the menu on a press. A short gap only pauses and goes on (the owner: "pausing is a good start"); a long one is a scale that has failed, and going on after it is the risk this ADR removes.
- **Where the bad-frame count shows** beyond the console's `stats`: the web, the screen, or nowhere.
