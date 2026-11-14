# ADR-012: the motor drivers cleaned up

Status: **Draft** 2026-11-14. Not approved. Captures how the motors are driven today, the faults read in the code, and ideas. None of the faults has been tried on a unit.

## What we want

- Motor code a person can read and an agent can change safely: one job a file.
- Motors that stop when the firmware stops.
- Speeds and turns that are what was asked for, and a unit that can show it.
- The seam to the chip that ADR-006 and the ESP32 port keep.

## Owner's direction (2026-11-14)

- The TMC2209 driver and the PIO stepper driver are, in the owner's words, badly written. They are cleaned up after ADR-001, before the changes of future PRs are merged.

## Why here

- **After ADR-001.** Its `motor` command, `charge_cycle` and `reboot` make the proofs below possible with no hands.
- **After ADR-011.** A functional change on the updated base. ADR-011 leaves the Trinamic library for this ADR to decide.
- **After ADR-013** (the owner, 2026-11-15). ADR-013 stops the tricklers when the firmware runs but has no real reading; this ADR stops them when the firmware itself stops (fault 1).
- **Before ADR-002.** Of the open PRs only two touch the motor files: #101, by one line of `stepper.pio`, and the old #28. Cleaning now costs the PRs almost nothing, and the two that drive motors for tuning (#113, #101) are judged against the cleaned interface.
- **Before ADR-006.** The motor module is the first one split by role. Its seam is the one ADR-006 keeps.
- **Before ADR-009.** The control loop's faults are measured on motors that do what they are told.

## What we know (read in `motors.c`, `motors.h`, `stepper.pio` and the library, 2026-11-14)

**How it works today**
- Two TMC2209 drivers share one serial line (UART1, 250,000 baud), at addresses 0 (coarse) and 1 (fine). Their registers are written once at start-up, through terjeio's Trinamic-library. After that the firmware never talks to them again.
- Each motor's STEP pin is driven by a PIO state machine (`stepper.pio`, seven instructions). It is given a period and repeats it on its own until it is given another. DIR and EN are plain pins.
- One task a motor, at priorities 9 and 8, takes a speed in rev/s from a queue, ramps to it at the stored acceleration (50 rev/s² by default), and reverses through zero.
- Charge mode and cleanup mode call `motor_enable` and `motor_set_speed`. The simulated scale reads each motor's last speed straight out of the module's structs.
- `motors.c` is 841 lines and does seven jobs: the serial transport to the drivers, a copy of the library's start-up, the PIO set-up, the ramps and tasks, the stored settings, an error screen drawn with u8g2 and the LEDs, and two REST handlers.
- With `-Wall` the file compiles without a warning. None of the faults below is one a compiler sees.

**Seen on the bench since** (ADR-011's runs, 2026-11-14; its [progress](../ADR-011-dependency_update/progress.md))
- **Fault 1, measured on both chips:** with the coarse trickler running, a hard fault halts the cores and its STEP pin keeps pulsing (`rc_run.py pulses`, part c: high in 99% to 100% of samples), on the Pico W and the Pico 2 W alike. A reset through the probe and an assert do stop it.
- **Fault 7, very likely seen:** in step 2's first `pulses` run, two starts in a row printed up to the motors' settings and then nothing, with no console; the next fourteen starts were normal. That is what fault 7 predicts: the check is `new_cnt - ifcnt == 7` on two `uint8_t`s, which C turns into `int`, so a counter that wraps (252 to 3, say) gives -249 and fails, about one start in 37 with 7 writes a start. The coarse driver fails first and returns before the fine one is set, so the next start fails on the fine driver. `handle_motor_init_error()` then loops for ever before the scheduler starts and prints nothing, so the console never comes. Not confirmed by the screen: the console was not there to snap it.

**Faults that can hurt a charge**

| # | Fault | What it does |
|---|---|---|
| 1 | The PIO repeats the last period for ever, and no watchdog runs | A firmware that stops during a charge leaves the motors turning until the power is cut (ADR-001) |
| 2 | The speed queue is made for 12-byte items and used with 4-byte floats | Every receive writes 8 bytes past a variable on the motor task's stack; every send reads 8 bytes past its argument. It works by the luck of what lies beside them |
| 3 | A ramp is a loop that never yields | For the ramp's length, up to about 0.1 s at the defaults, the task holds one core and rewrites the period as fast as it can. Two motors can hold both cores. The charge loop, the screen and Wi-Fi run at lower priorities and wait |
| 4 | `motor_set_speed` waits with no time limit on a queue of two | The caller is the charge loop. It stands still while that motor is in a ramp |
| 5 | A setting changed while running reaches the speed maths at once and the driver at the next start | After a change of microsteps the motor turns at the wrong speed, by the ratio of old to new, until the unit is restarted |
| 6 | No setting is range-checked | An acceleration, gear ratio or microstep count of 0 goes straight into a division |

**Faults at start-up**

| # | Fault | What it does |
|---|---|---|
| 7 | The check that the drivers took their settings compares an 8-bit write counter without allowing for its wrap. The library's own start-up allows for it; the firmware uses its own copy, which does not | If the drivers stay powered across restarts, about one restart in 32 ends at "COARSE DRV ERR" and, by the same reading, the restart after it at "FINE DRV ERR". ADR-001's bench restarts a unit without cutting its power, so it is where this will first be seen |
| 8 | The copy differs from the library in one more place: the library clears the driver's `I_scale_analog` bit, the copy leaves it as the chip starts | With the bit set the driver scales its current by the voltage on its VREF pin (TMC2209 datasheet, to be re-read when this ADR is written in full). The stored "current, mA" is then the real current only with the module's trimmer at full scale. Whether that was meant is the owner's to say |
| 9 | A driver that does not answer stops the unit at the error screen before the scheduler starts | No menu, no Wi-Fi, no way to change the setting that may be the cause |

**Faults in the numbers**

| # | Fault | What it does |
|---|---|---|
| 10 | The PIO holds STEP high for the value it is given plus 2 clock cycles; the maths takes it as the value | Under 0.5% slow at top speed, less below |
| 11 | The STEP low time is 13 clock cycles, counted for a 125 MHz clock as 104 ns | On the Pico 2 W's 150 MHz clock it is 87 ns. To check against the driver's minimum |
| 12 | Any speed under 100 steps a second is taken as stop | Nothing at the default 256 microsteps. With fewer microsteps a slow speed becomes no speed, silently |
| 13 | A ramp's end is found by comparing a 32-bit microsecond clock with `>` | A ramp that crosses the clock's wrap, every 71.6 minutes, ends at once: the motor jumps to the new speed |
| 14 | Nothing counts steps | The coarse back-off's "quarter turn" is a speed held for a time, less what the ramps take |

**Smaller**
- A read that gets no answer returns the previous answer, from a buffer that is never cleared. The library rejects it unless it was the same register.
- Nothing reads the drivers' status after start-up: over-temperature, a short and an open coil go unseen.
- The second LED's colour on the error screen has a digit too many (`0xFFA5000`).
- `motor_task` is declared and does not exist; `_block_wait_for_sync` is never called.
- The build compiles the library's TMC2130 and TMC5160 drivers, which no board has.

## Ideas

**Three parts, by role.** Only the first two know the chip:
- **The driver chip:** its set-up and status over a serial line. It is handed two functions by the platform, send and receive with a time limit. No RTOS and no pins in it.
- **The step pulses:** "this many steps a second in this direction", "stop", "steps made so far". PIO behind it on the RP chips; the ESP32's own pulse hardware later (ADR-007).
- **The motors:** what the application calls. Speeds in rev/s at the trickler, the gear ratio, the limits, the ramps, enable, the settings.

The error screen goes to where screens are, and the REST handlers stay until ADR-003 moves them.

**The library kept, the copy dropped.** The library's own start-up is used, the library moved to its newest commit, and only its TMC2209 file built. First view: keep it. It is small, maintained and already free of chip code; writing our own would be about 200 lines for no gain. Fault 8 is decided by the owner before the copy goes, because it changes the motor current.

**Motors that stop by themselves.** The PIO is given a period and a count of steps, and stops when it is not fed. The firmware feeds it a few milliseconds at a time, so a firmware that stops feeding stops the motor within that time. The same count gives exact turns. A watchdog is a second question: it restarts the unit, which a user sees.

**Ramps that do not take a core.** The speed moves toward its target once a millisecond, by the acceleration, and one period is written each time. Setting a speed stores the target and returns.

**The unit counts its own steps.** The pulses on each STEP pin are counted in hardware, and the console's `motor` command reports steps asked and steps made. That is the proof of speed without an oscilloscope, and it gives ADR-009 its recordings in steps.

**A setting applied when it changes, and checked.** The driver is written when its setting changes, and a value out of range is refused.

**Ideas to try, not promised:**
- the drivers' status read while running, and a charge stopped when a driver reports a fault;
- a unit that starts without its drivers and shows the fault in the menu;
- the driver's stall detection, to notice a jammed trickler.

## How the unit shows it works

On each supported board:
- `motor` at set speeds for set times: the steps the unit counts are within 0.5% of the steps asked.
- 40 restarts in a row from the console, the drivers powered throughout: no driver error.
- The firmware stopped on purpose while a motor turns: the motor is seen to stop.
- A ramp from rest to full speed while the scale is read: the readings keep their pace, and `stats` shows no core held.
- `charge_cycle` passes, no slower than its run before this ADR.
- The owner's real throws, same powder and target, before and after: no worse.
- The stored motor settings survive, or the ADR says they do not.

## Not in this ADR

- The control loop (ADR-009).
- The ESP32's step pulses (ADR-007).
- The shape of the REST API (ADR-003).
- The servo gate.

## To settle before approval

- Fault 8, the current reference: units as they are today, or the stored mA made true.
- Whether a watchdog is part of this ADR or one of its own.
- Whether a unit without its drivers reaches the menu.
- Whether the owner's cleanup means more than the motors. The other rewrites have ADRs already: the HTTP server (ADR-003), the layout (ADR-006), the control loop (ADR-009), the screens (ADR-010). A module the owner names is added here or gets its own.
