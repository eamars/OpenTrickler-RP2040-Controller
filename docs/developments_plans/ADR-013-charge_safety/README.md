# ADR-013: the tricklers move only on a real, fresh reading

Status: **Draft** 2026-11-15. Not approved. Captures three faults read in the code that make the tricklers run when they should not, how to fix them, and how the unit shows it. Nothing has been built.

## What we want

- During a charge the tricklers move only on a weight that was really read, and read recently.
- They move only as the running mode commands.
- A unit whose scale sends nonsense or nothing stops pouring by itself, and says why.

## Owner's direction (2026-11-15)

- A small charge-safety ADR, run before ADR-012. Asked "Draft a small charge-safety ADR to run before ADR-012?": "Yes, draft it".

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

**What can show it on the bench**
- The simulated scale publishes a reading every 100 ms through the same field. It cannot yet send a bad frame or go quiet.
- The real A&D cannot be made to send a bad frame. Pulling its cable is the real test of silence.

## What we will do

**One rule, kept at one door.** The motors move only on a real, fresh reading, and only as the running mode commands.

1. **One way to publish a reading.** A function in `scale.c` that every driver and the simulated scale call in place of writing the field. It is given whether the frame parsed and the value. A frame that did not parse, or a value that is not a finite number, is not published, and is counted (`bad_frames` in `stats`). So a bad frame becomes a frame that never came. The Sartorius driver says when it parsed nothing, as the others do.
2. **A deadline in the charge loop.** No reading for 1 s while the motors may turn: both motors stop at once, the servo gate goes to its closed position if it is enabled, and the charge ends with "Scale: no reading" on the screen and the LEDs in the not-ready colour. A press of a button goes back to the menu. This covers a scale that went quiet and one that sends only nonsense, since its frames are no longer readings.
3. **The cleanup speed applies only in cleanup mode.** The web may set it at any time; it reaches the motors only while cleanup mode runs, as from the screen.

**The faults shown first, then fixed.** Each fault is shown failing on the code as it is, so the proof shows a change:
- The simulated scale learns `sim bad` (its next reading is a frame that does not parse) and `sim quiet <ms>` (it publishes nothing for that long). Console commands only, never stored, as `sim on` is.
- `rc_run.py` learns to run a charge with a fault put in at its middle: a bad frame, a quiet scale, a cleanup speed from the web. It reports the motors' speeds through it.
- These are built and run first, alone, on the code before the fixes: they must fail there. Then the three fixes, one commit each, each turning its test from fail to pass.

## How the unit shows it works

On the bench's Pico W. The change is the same code on both chips, so no Pico 2 W pass unless the owner asks for one.

- **A bad frame mid-charge:** the charge goes on as if it never came. The throw ends within the stop threshold, as a clean one does, and `stats` counts one bad frame.
- **The scale quiet for 3 s mid-charge:** both motors at 0 within 1.2 s of the last reading (the deadline and a ramp down), the gate closed if it is enabled, the message on the screen (`snap`), the menu back after a press, and the next charge passes.
- **A cleanup speed sent from the web mid-charge:** no motor changes speed. In cleanup mode it applies as before.
- **The same three on the code before the fixes fail,** recorded in `progress.md`.
- `charge_cycle` passes, `menu_tour --compare` finds every form equal (the message is drawn by charge mode, not a menu form), and the stored settings are unchanged: no stored struct changes.
- The owner's check, which the owner may waive: the scale's cable pulled during a charge with the hopper empty; the motors stop and the screen says why.

## Not in this ADR

- The motors when the firmware itself stops (ADR-012, fault 1), and the rest of the motor code.
- The audit's other findings, such as the web form cut at 127 characters, storage and Wi-Fi. The cleanup's scope is proposed on its own.
- The control loop's own faults (ADR-009): the D term's first sample, the I term not scaled by time. No bad reading reaches the loop after this ADR, but its sums are not changed here.
- Telling the user outside a charge that the scale is silent.

## To settle before approval

- **The deadline:** 1 s, as a first view. The simulated scale reads every 100 ms. The real A&D's rate is measured on the bench before the code, and the deadline is at least five of its intervals.
- **What the unit does at the fault:** first view, stop, close the gate, show why, and back to the menu on a press. The other way is to wait for readings to come back and go on. Going on after a silence is the risk this ADR removes, so the first view is stop and say why.
- **Where the bad-frame count shows** beyond the console's `stats`: the web, the screen, or nowhere.
