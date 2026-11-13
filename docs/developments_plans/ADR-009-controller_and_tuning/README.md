# ADR-009: one controller, every throw recorded, automatic tuning on top

Status: **Draft** 2026-11-13. Not approved. Captures how the charge is controlled today, what is known to be wrong, and ideas. Nothing here has been tried on powder.

## What we want

- One controller with no hardware in it, used by charge mode and by tuning.
- Every throw recorded, so "is it better" is answered from data.
- Test functions a user can run: a test throw, a motor at a speed, the scale's readings.
- Automatic tuning built on that controller.

## Owner's direction (2026-11-13)

- Implement better control tuning and a testing function.
- PRs are not merged as they stand. Two of them are about tuning (#113, #101); what is kept of them comes from ADR-002.

## Why here

It needs ADR-003's server for its endpoints and ADR-008's page for its chart. Its first half, the controller as a module with its recordings, depends on neither and can move earlier if the owner wants tuning sooner.

## What we know (checked 2026-11-13, in `charge_mode.cpp` and `profile.h`)

**The charge**
- Four states: wait for zero, charge, wait for the cup's removal, wait for its return.
- Zero is ten readings 300 ms apart whose spread and mean are both under 0.02 (the default margins).
- The loop runs once per scale reading. If none comes in 200 ms it polls the buttons and waits again.
- Two PID loops, on the same weight:
  - coarse aims at the target less the coarse stop threshold (5 gr by default) and is switched off for good the first time its error reaches 0. The servo gate then moves to a set position;
  - fine aims at the target and runs from the start.
- Each output is held between a minimum and a maximum speed from the profile and the motor.
- Everything stops when the weight is within the fine stop threshold (0.03 gr by default) of the target.
- One second later the weight is read once more and the LEDs show under, over or good. There is no top-up when it is under.
- A profile holds Kp, Ki, Kd and the speed limits for each trickler. Eight profiles. The two stop thresholds are global.

**Known faults**
- The D term divides by the milliseconds since the last reading. That can be 0, and at the first reading the last error is 0, so the first D term is a spike. PR #113 reports both tricklers briefly at full speed at the start of a throw and fixes the division.
- The I term adds the error once per reading, not scaled by time, so its weight changes with the scale's reading rate. It is not limited while the output sits at its maximum.
- The loop's pace is the scale's. A scale that reports half as often is controlled half as often, with the same gains.
- Nothing records a throw. The only trace is the elapsed time on the screen.
- The loop is inside `charge_mode_wait_for_complete()` with the button polls, the LED colours, the servo gate and the title on the screen.

**What others have built**
- PR #113: tuning from start values by powder shape and grain size, ten proving throws, a suggested fine threshold from the weight that still falls after the stop, a test throw with every sample recorded and exported, and a simulated powder with tests that run on a PC.
- PR #101: another tuning routine, ported from a fork.
- Open requests: a calibration routine (#12), PID values from an online source (#10), ladder loading (#106), reversing the coarse trickler (#105; the code has a TODO for it at the coarse stop), a press to skip the current charge (#75).

## Ideas

**The controller as a module.** Readings and times in; two speeds and the gate's position out. No FreeRTOS, no motors, no screen in it. Charge mode keeps the states, the buttons and the LEDs, and calls it.

**Tests on the PC.** The module against a simulated powder, in CI. ADR-001's simulated scale and PR #113's powder model are two starts for it; one is kept.

**Every throw recorded.** Time, weight, both speeds and the three terms of each loop, per reading, in RAM for the last throw; downloadable, and drawn on the tuning page. How many throws are kept, and where, depends on the board (ADR-005).

**The faults fixed one at a time,** each against recordings of the same powder before and after: the D term's first sample and its division, the I term scaled by time and limited, gains that do not depend on the scale's reading rate.

**Ideas to try, not promised:**
- a top-up when the throw ends under the target;
- the stop thresholds per profile;
- a short reverse of the coarse trickler at its stop;
- stopping early by the amount known to still fall.

**Test functions,** on the screen and the web: a test throw that is recorded, a motor at a set speed for a set time, the scale's readings and their rate.

**Automatic tuning,** from what ADR-002 keeps of #113 and #101, run through the same controller and judged by the same recordings.

## How the unit shows it works

- The owner's recorded throws before and after, same powder, same target, at least ten each: time to target, overshoot and spread no worse, and better where a fault was fixed.
- The PC tests in CI.
- `charge_cycle` passes.
- A tuning run completes, saves a profile, and ten charges with it land inside the threshold.
- On each supported board.

## Not in this ADR

- Ladder loading and other new modes.
- Profiles from an online library.

## To settle before approval

- What "better" is, as numbers the owner cares about: time to target, overshoot, spread.
- Whether the controller and its recordings move ahead of ADR-008.
- Which powders and which scale the proof is made with.
