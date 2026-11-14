# ADR-012: the motor drivers cleaned up

Status: **Open**. Approved by the owner 2026-11-15. All five steps are built, and proven on both units that day, but for the Pico W's soak, which fails on the web alone (its fix is written into ADR-003). Left: the owner's word on that; two proofs on the Pico W with the A&D sending; three by the owner's hand (a driver module lifted before a start, a phone on the unit's access point, real throws). What is built and measured is in [progress](progress.md), which ends with where it stands. The faults were read in the code; two of them have since been seen on the bench (ADR-011's runs).

## What we want

- Motor code a person can read and an agent can change safely: one job a file.
- Motors that stop when the firmware stops.
- Speeds and turns that are what was asked for, and a unit that can show it.
- A driver chip that is checked: every setting it is sent is seen to arrive, and a fault it reports stops a charge.
- The seam to the chip that ADR-006 and the ESP32 port keep.

## Owner's direction

- 2026-11-14: the TMC2209 driver and the PIO stepper driver are, in the owner's words, badly written. They are cleaned up after ADR-001, before the changes of future PRs are merged.
- 2026-11-15: Klipper is a good reference for a driver. The Trinamic library can go if there is a better way. Look at whether the board can take other drivers, the TMC5160 and the TMC2240.
- 2026-11-15, decided after the research below:
  - **The driver code is our own, on Klipper's model.** The Trinamic library is dropped.
  - **The stored motor current becomes the real current.** The driver module's trimmer is ignored, as Klipper and the Trinamic library both do.
  - **The TMC2209 only, shaped as a table,** so a second chip is a second table when someone has one on a bench.
- 2026-11-15, on the first full draft:
  - **The trimmer stays as the factory set it.** Nobody is asked to turn it, and nobody is encouraged to: Klipper no longer uses it either.
  - **A unit without its drivers reaches the menu and refuses to charge; a driver's fault ends a charge; no watchdog in this ADR.** All three taken as proposed.
  - **Units keep the current they run at today.** The stored figures are converted once, and the default becomes 400 mA ("The stored settings").
  - **The cleanup's scope is the agent's to decide, and a small cleanup gets no ADR of its own.** So the small fixes found by the audit of 2026-11-14 that cannot wait are a step of this ADR (step 5), and every other finding is given to an ADR that exists ("Where the rest of the audit goes").

## Why here

- **After ADR-001.** Its `motor` command, `charge_cycle` and `reset` make the proofs below possible with no hands.
- **After ADR-011.** A functional change on the updated base. ADR-011 left the Trinamic library for this ADR to decide.
- **After ADR-013** (the owner, 2026-11-15). ADR-013 stops the tricklers when the firmware runs but has no real reading; this ADR stops them when the firmware itself stops (fault 1).
- **Before ADR-002.** Of the open PRs only two touch the motor files: #101, by one line of `stepper.pio`, and the old #28. Cleaning now costs the PRs almost nothing, and the two that drive motors for tuning (#113, #101) are judged against the cleaned interface.
- **Before ADR-006.** The motor module is the first one split by role. Its seam is the one ADR-006 keeps.
- **Before ADR-009.** The control loop's faults are measured on motors that do what they are told.

## What we know

### How it works today (read in `motors.c`, `motors.h`, `stepper.pio` and the library, 2026-11-14)

- Two TMC2209 drivers share one serial line (UART1, 250,000 baud), at addresses 0 (coarse) and 1 (fine). Their registers are written once at start-up, through terjeio's Trinamic-library. After that the firmware never talks to them again.
- Each motor's STEP pin is driven by a PIO state machine (`stepper.pio`, seven instructions). It is given a period and repeats it on its own until it is given another. DIR and EN are plain pins.
- One task a motor, at priorities 9 and 8, takes a speed in rev/s from a queue, ramps to it at the stored acceleration (50 rev/s² by default), and reverses through zero.
- Charge mode, cleanup mode and the remote control call `motor_enable` and `motor_set_speed`. The remote control reads each motor's last speed through `motor_get_velocity`; the simulated scale reads it straight out of the module's structs.
- `motors.c` is 849 lines and does seven jobs: the serial transport to the drivers, a copy of the library's start-up, the PIO set-up, the ramps and tasks, the stored settings, an error screen drawn with u8g2 and the LEDs, and two REST handlers.
- With `-Wall` the file compiles without a warning. None of the faults below is one a compiler sees.

### What the board can take (read in the v2 board's layout, hardware repository at `7dda850`, 2026-11-15)

Each driver socket has:
- STEP, DIR and EN from the Pico;
- its UART pin on one wire shared by both sockets, to GPIO5 directly and to GPIO4 through a 1 kΩ resistor;
- MS1 and MS2 soldered to ground or 3.3 V: that is the address, 0 for the coarse socket and 1 for the fine;
- PDN and CLK not connected;
- the motor supply from the USB-C input (12 V), the logic supply at 3.3 V.

There are no SPI wires to the sockets. So:

| Driver | On this board | Why |
|---|---|---|
| TMC2209 | Yes | What the board was made for |
| TMC5160 | No | Its modules are set up over SPI (BIGTREETECH's TMC5160T Pro: "SPI only"). The SPI data-in and clock pins sit where this board has MS1 and MS2, which are soldered to a supply rail |
| TMC2130, TMC2160 | No | Set up over SPI, the same |
| TMC2240 | On paper, not tried | In UART mode its one wire and its address pins sit where this board has the UART wire, MS1 and MS2 (Fysetc's S2240 lists the pins as `SDI/AD0`, `SCK/AD1`, `CSN/AD2/UART`). The module must be reworked to UART mode first: resistors moved on Fysetc's, a solder bridge on BIGTREETECH's, whose UART pin its pages show only in a picture |

No TMC2240 has been in this board. Until one has, it is not supported.

### What Klipper does (read in `klippy/extras` at `29d4ec2d`, 2026-11-10: `tmc_uart.py`, `tmc.py`, `tmc2208.py`, `tmc2209.py`, `tmc2130.py`, `tmc2240.py`)

- **A chip is a table.** Its registers' addresses and its fields' masks. `tmc2209.py` is about 110 lines on top of the TMC2208's table. The TMC2240 uses the same serial transport with another table and another way to work out the current.
- **Every write is checked.** The driver counts the writes it accepts (`IFCNT`, 8 bits). Klipper reads the count before and after each write, expects one more modulo 256, and tries up to five times. A read is tried up to five times, and an answer counts only if it is exactly the frame that register and value would make.
- **Registers are built from zero,** not read from the chip and changed. `GCONF` is written with `pdn_disable`, `mstep_reg_select` and `multistep_filt` set and everything else clear. That clears `I_scale_analog`: the current comes from the driver's own reference and the trimmer does nothing.
- **The current is RMS,** worked out from the sense resistor plus 0.020 Ω, with the driver's `vsense` range chosen by which gives the closer figure.
- **The drivers are read once a second.** `DRV_STATUS` and `GSTAT`. Over-temperature and a short to ground or to supply (`ot`, `s2ga`, `s2gb`, `s2vsa`, `s2vsb`) stop the printer; the temperature warnings are logged. A driver that has reset itself is noticed and stops the printer too.
- **Steps are queued with a count.** The chip is sent "this interval, this many steps", and makes no step it was not sent. A host that stops sending is a motor that stops.

Klipper and this firmware are both GPLv3, so its tables can be carried over with credit.

### The Trinamic library, and what else was looked at

- **terjeio's Trinamic-library** (at `d7debfa`; upstream is five commits on, the last 2026-04-23). Its own TMC2209 start-up allows for the counter's wrap and clears `I_scale_analog`; the firmware does not use it but a copy that does neither (faults 7 and 8). It keeps one struct of every register for each driver, on the heap. Its seam is two functions the firmware supplies, and the read one returns a pointer to a buffer: it cannot say "no answer", which is where the stale read below lives. It checks the write count once, at start-up, and has nothing that reads status while running.
- **Analog Devices' TMC-API** (MIT). Register and field names for each chip, read and write through two functions the firmware supplies, a copy of the registers in RAM. No start-up, no current maths, no check of a write. A large repository for what would be a table here.

### The current today

- The stored figure (800 mA by default) is taken as RMS by the maths, though its comment in `motors.c` says peak.
- With `I_scale_analog` left set, the driver scales that by the voltage on its VREF pin over 2.5 V (BIGTREETECH's formula for its TMC2209 module carries the same factor; the datasheet is re-read in step 1). So today's real current is the stored figure only with the module's trimmer at full scale, and differs from module to module.
- **A stock module's trimmer:** BIGTREETECH's pages give 1.2 V ± 0.1 V as the factory setting of its TMC2209, which is 0.48 of full scale. The stored 800 mA works out to 0.795 A RMS at full scale (the library's maths, by hand), so a stock BIGTREETECH module runs its motor at about **0.38 A RMS** today. Fly's factory setting was not found.
- So with the trimmer ignored and the stored figure left at 800, the motor current on such a unit rises about 2.1 times, and the heat in the motor's windings about 4.3 times.

### Seen on the bench (ADR-011's runs, 2026-11-14; its [progress](../ADR-011-dependency_update/progress.md))

- **Fault 1, measured on both chips:** with the coarse trickler running, a hard fault halts the cores and its STEP pin keeps pulsing (`rc_run.py pulses`, part c: high in 99% to 100% of samples), on the Pico W and the Pico 2 W alike. A reset through the probe and an assert do stop it.
- **Fault 7, very likely seen:** in step 2's first `pulses` run, two starts in a row printed up to the motors' settings and then nothing, with no console; the next fourteen starts were normal. That is what fault 7 predicts: the check is `new_cnt - ifcnt == 7` on two `uint8_t`s, which C turns into `int`, so a counter that wraps (252 to 3, say) gives -249 and fails, about one start in 37 with 7 writes a start. The coarse driver fails first and returns before the fine one is set, so the next start fails on the fine driver. `handle_motor_init_error()` then loops for ever before the scheduler starts and prints nothing, so the console never comes. Not confirmed by the screen: the console was not there to snap it.

### The faults (read in the code, 2026-11-14)

**Faults that can hurt a charge**

| # | Fault | What it does |
|---|---|---|
| 1 | The PIO repeats the last period for ever, and no watchdog runs | A firmware that stops during a charge leaves the motors turning until the power is cut |
| 2 | The speed queue is made for 12-byte items and used with 4-byte floats | Every receive writes 8 bytes past a variable on the motor task's stack; every send reads 8 bytes past its argument. It works by the luck of what lies beside them |
| 3 | A ramp is a loop that never yields | For the ramp's length, up to about 0.1 s at the defaults, the task holds one core and rewrites the period as fast as it can. Two motors can hold both cores. The charge loop, the screen and Wi-Fi run at lower priorities and wait |
| 4 | `motor_set_speed` waits with no time limit on a queue of two | The caller is the charge loop. It stands still while that motor is in a ramp |
| 5 | A setting changed while running reaches the speed maths at once and the driver at the next start | After a change of microsteps the motor turns at the wrong speed, by the ratio of old to new, until the unit is restarted |
| 6 | No setting is range-checked | An acceleration, gear ratio or microstep count of 0 goes straight into a division |

**Faults at start-up**

| # | Fault | What it does |
|---|---|---|
| 7 | The check that the drivers took their settings compares an 8-bit write counter without allowing for its wrap | If the drivers stay powered across restarts, about one restart in 37 ends at "COARSE DRV ERR" and the restart after it at "FINE DRV ERR" |
| 8 | The firmware's copy of the start-up leaves the driver's `I_scale_analog` bit as the chip starts, set | The stored "current, mA" is the real current only with the module's trimmer at full scale |
| 9 | A driver that does not answer stops the unit at the error screen before the scheduler starts | No menu, no Wi-Fi, no console, no way to change the setting that may be the cause |

**Faults in the numbers**

| # | Fault | What it does |
|---|---|---|
| 10 | The PIO holds STEP high for the value it is given plus 2 clock cycles; the maths takes it as the value | Under 0.5% slow at top speed, less below |
| 11 | The STEP low time is 13 clock cycles, counted for a 125 MHz clock as 104 ns | On the Pico 2 W's 150 MHz clock it is 87 ns. To check against the driver's minimum in step 2 |
| 12 | Any speed under 100 steps a second is taken as stop | Nothing at the default 256 microsteps. With fewer microsteps a slow speed becomes no speed, silently |
| 13 | A ramp's end is found by comparing a 32-bit microsecond clock with `>` | A ramp that crosses the clock's wrap, every 71.6 minutes, ends at once: the motor jumps to the new speed |
| 14 | Nothing counts steps | The coarse back-off's "quarter turn" is a speed held for a time, less what the ramps take |

**Smaller**
- A read that gets no answer returns the previous answer, from a buffer that is never cleared.
- Nothing reads the drivers' status after start-up: over-temperature, a short and an open coil go unseen.
- The second LED's colour on the error screen has a digit too many (`0xFFA5000`).
- `motor_task` is declared and does not exist; `_block_wait_for_sync` is never called.
- The build compiles the library's TMC2130 and TMC5160 drivers, which no board can take.

## What we do

Three parts by role, and only the first two know the chip. One step each, then the drivers' status, then the small fixes outside the motors; each step is built, flashed and proven on the unit before the next starts.

| Part | Files | What it is | What it knows |
|---|---|---|---|
| The driver chip | `tmc_uart.c`, `tmc2209.c` | Set-up and status over the serial line | Registers. No RTOS, no pins: the platform hands it one function, "send these bytes, give me that many back within this time" |
| The step pulses | `step_pulses.c`, `stepper.pio` | "This many steps at this period, in this direction", "stop", "steps made so far" | The PIO on the RP chips; the ESP32's own pulse hardware later (ADR-007) |
| The motors | `motors.c` | What the application calls: speeds in rev/s at the trickler, the gear ratio, the limits, the ramps, enable, the settings | Neither chip |

### Step 1: the driver, our own

- **The transport** (`tmc_uart`): the frame and its CRC, a read tried up to five times, a write checked by the driver's count going up by one modulo 256 and tried up to five times. The echo of our own bytes on the shared wire is read and checked, not waited out. A read that fails says so; nothing keeps an old answer.
- **The chip** (`tmc2209`): its registers and fields as one table, carried over from Klipper with credit. Each register is built from zero and written whole.
- **The current** is RMS, by Klipper's formula. `I_scale_analog` is clear. The pages and the comment say RMS. Stored figures are converted once and the default becomes 400 mA, so a unit runs at the current it runs at today ("The stored settings").
- **A driver that does not answer is a state, not a halt.** The unit starts, the menu, Wi-Fi and the console work, and the fault is shown; a charge and a cleanup refuse to start until both drivers answer. The same for motor settings that cannot be read. The error screen that loops for ever goes.
- `profile.c`'s dead `profile_update_checksum` goes: it is the one caller of the old transport's CRC outside the motors (audit P4).
- **The console** gets `tmc`: the registers that can be read, as the chip holds them, by field; and the count of writes and of tries that failed.
- **The Trinamic submodule goes,** with its lines in `CMakeLists.txt`.

Fixes faults 7, 8 and 9, the stale read, the LED colour and the unused build of other chips.

### Step 2: step pulses that stop by themselves

- **The PIO is given a period and a count,** makes that many steps and stops. The firmware feeds it a few milliseconds at a time, so a firmware that stops for any reason (a hard fault, a hang, a debugger) is a motor that stops within that time. This is Klipper's rule: no step that was not sent.
- **The times are worked out from the chip's clock,** not written for 125 MHz: STEP high and low each at least the driver's minimum on both chips, and the period exact.
- **A slow speed is a long period,** not a stop.
- **The unit counts its steps.** The count fed to the PIO is the count made. Where the PIO has a state machine to spare, a second one counts the pulses on the pin itself, so the figure does not rest on the code it checks; whether there is room beside the LEDs' two is found in this step.
- `motor` on the console reports steps asked and steps made.

Fixes faults 1, 10, 11, 12 and 14.

### Step 3: the motors

- **One task moves both speeds toward their targets once a millisecond,** by the acceleration, and feeds the pulses. Setting a speed stores the target and returns: no queue, no wait.
- **A setting is checked when it is given** (a value out of range is refused) **and applied when it changes:** the driver is written then, not at the next start.
- **A speed is checked too:** one that is not a number is refused, and one past the motor's limit is held to it. Cleanup mode's ±1 rev/s a click and the web's speed have no limit today (audit M4).
- **The servo gate's ramp is the same fault twice** (audit G1, G2): a loop that never yields, 0.2 s to 0.33 s of one core just as a charge starts, ended by the same clock comparison as fault 13. It moves toward its target by ticks, as the motors now do.
- The error screen's drawing leaves the module. The two REST handlers stay until ADR-003 moves them.
- `motor_set_speed`, `motor_enable`, `motor_get_velocity` and the limits keep their names and meanings, so charge mode, cleanup mode and the remote control are not rewritten here. The simulated scale stops reading the module's structs and asks `motor_get_velocity`.

Fixes faults 2, 3, 4, 5, 6 and 13, and the two dead declarations.

### Step 4: the drivers read while running

- Once a second, from a task below the motors': `DRV_STATUS` and `GSTAT` of each driver.
- Over-temperature or a short ends a charge with a message that names the driver and the fault; the warnings go to the log.
- A driver that has reset itself (it lost its supply and came back) is written again before it is used.
- `state` on the console shows each driver's last status.

### Step 5: the small fixes that cannot wait

The audit of 2026-11-14 read every module outside the motors. A finding is fixed here if all three hold: it can hurt a unit or a charge; its fix is a few lines; the bench can show it. Each is read again in the code before it is touched, and one that turns out wrong is dropped and said so in progress. One commit each.

| Audit | Fault | Fix |
|---|---|---|
| H1, H2 | On main since PRs #108 and #112, in no release: a web form is cut at 127 characters and at 16 values. The charge-mode page sends 17, so its "Save to EEPROM" never arrives and a value can be cut in the middle | Room for the longest form the pages send, in both limits |
| S1 | The Generic scale driver has no zero function, and its three callers call it unchecked: a zero press jumps to address 0 and the unit is dead until its power is cut | The driver gets a zero that does nothing |
| ADR-013's | The A&D driver takes any 17 bytes as a frame. A frame stitched from two was seen on the bench, 1 in 30 at a hand-over | A frame is taken at its line ending |
| M5 | The cleanup screen writes a weight into 8 bytes with no length: a tared container of 1000 units or more, lifted, overruns it. With an unknown number of decimals it prints a buffer never set | The weight is written with its buffer's length; the unknown case prints nothing |
| W1 | In access point mode the DNS server is handed the address of a variable that is gone when the function returns | The server that is kept is the one handed over |
| W2 | A 32-character Wi-Fi name with a key is copied without its ending: memory is overrun at every start, and the key comes back in the web reply. Two files declare the same buffers as 32 and 35 bytes | Names and keys copied with their endings; one declaration; no key in a reply |

`charge_mode.cpp` is not touched by this step, and by steps 1 and 4 only where a charge must refuse to start or must end: every live PR changes that file. Before this ADR closes, PRs #109 and #113, which merge cleanly today, are merged into its branch on this PC as a test, pushed nowhere, and what that shows is recorded.

### Where the rest of the audit goes

No finding is left without an ADR. Before this ADR closes, each line below is written into the ADR named, in that ADR's own words; the audit's files are on one PC only and are not the record. The ids are the audit's; those not read a second time are marked.

| To | Findings |
|---|---|
| ADR-003, the REST server | Values from the web checked before use: the scale's indexes (S6), the gate's duty and speed (G3), the LEDs' order and count (N1), the charge target (M5's other half), the precharge's time (M6), the number of decimals that sends the menu to the wrong form (D2). A GET that changes nothing: reading a profile selects it (P2, P3). A profile name stored without its ending (P1). State variables written straight from the web (M10). The 404 reply cut to 13 bytes (H3), handlers that block the network (H4), the 5 s erase (E4), erase-and-restart with no reply (Y3). Only P2 and the charge target were read a second time |
| ADR-004, update over Wi-Fi | A unit that starts before its router stays in access point mode until restarted (W3) |
| ADR-005, budgets | 1 KB stacks for tasks that format floats and draw, unmeasured (Y4, unchecked) |
| ADR-006, the platform split | Storage: defaults written at once on a CRC mismatch (E1), old data taken without a check (E2), every start-up result ignored so that a missing EEPROM is a blank, dead unit (E3), I2C with no time limit (E5), a failed save reported as done (E6), ids printed without their endings (E7). A radio that fails stops a core (W4): with E3, the class of this ADR's fault 9. The scale drivers as one reader with a parser a make: the task is not swapped when the make is (S4), a sign apart from its digits (S7), the loop copied seven times (S8), the other makes' frames taken at their line endings. One-byte writes into enums (D3). All but E1 and E3 unchecked |
| ADR-009, the controller | The modes' cleanup is this ADR: the loop comes out of charge mode. `FloatRingBuffer` replaced by a C ring buffer with a median (handover). The back-off ended by a count of steps, which step 2 here provides (M7). Over and under judged on one sample (M8), the fixed cup-removal weight (M9), readings up to 20 ms late (S5), "gate in use" read from two fields (G5), charge mode's own weight buffers (M5). Unchecked but for M5 |
| ADR-010, the screens | Button interrupts set up before their queue (D1), a unit label never drawn (D4), the mirror serving a buffer being drawn (D5), the A&D calibration offered for every make (S9), "Wifi Info" before the radio is up (W5). Unchecked |
| A servo on the bench | Whether "shutter0" and "shutter1" are crossed or the wiring is (G4): checked when a servo is next connected, and fixed in step 3 if it is the code |

Done already: a reading that does not parse, a silent scale and the web's cleanup speed (S2, S3, M1, M2, M3: ADR-013); a console (Y2: ADR-001); motors that run on behind a failed assert or a stack overflow (Y1: step 2 here).

### The stored settings

The stored struct's layout does not change, so no unit's motor settings are reset. Two fields change their use, in step 1:

- **`current_ma` becomes the real RMS current.** On its first start with this firmware a unit's two stored figures are multiplied by 0.48, the factory trimmer's share of full scale, and rounded to 10 mA: 800 becomes 380. The driver's current has 32 steps of about 31 mA, and both 380 and the new default of 400 land on the step that gives 0.40 A, against 0.38 A today.
- **`motor_data_rev` marks the conversion,** in its upper two bytes. The lower two must stay 0: `load_config` takes anything else there as an EEPROM never written and stores the defaults. A unit with the mark is not converted again.
- **A unit whose trimmer was turned** gets a figure that is off by as much as it was turned. The figure is now what the motor gets, so it is seen and set right in the menu or the page.
- **Going back to an older firmware** is the one case not covered: it reads the converted figure as before the trimmer (380 gives about 0.18 A), and its first save clears the mark, so coming forward again converts a second time. Set the current by hand after a downgrade. `r_sense` stays, as the formula needs it.

## How the unit shows it works

On the Pico W and on the Pico 2 W:

- **The driver's set-up:** `tmc` shows `GCONF` with `I_scale_analog` clear, and every readable register as the table says. With a motor turning, `DRV_STATUS` shows the current scale the stored mA works out to.
- **40 restarts in a row from the console, the drivers powered throughout:** no driver error. A start writes each driver at least 7 times, so that is 280 writes or more to each, and each write counter passes its wrap.
- **A driver that does not answer** (the owner lifts one module, power off): the unit reaches the menu and the console, shows which driver, and refuses a charge.
- **`pulses`, all three parts:** the pulses stop on a probe reset, on an assert and on a hard fault.
- **`motor` at set speeds for set times:** the steps made are the steps asked, and the speed is within 0.5%.
- **A ramp from rest to full speed while the scale is read:** the readings keep their pace, and `stats` shows no core held.
- **A setting changed while stopped** (microsteps, current): `tmc` shows it in the driver at once, and the next `motor` run turns at the right speed.
- **`menu_tour --compare` and `charge_cycle` pass,** the charge no slower than its run before this ADR.
- **`settings --compare`:** the stored motor settings are as they were, but for the current: 800 has become 380, once. A second start and a second flash leave it at 380.
- **The small fixes** (step 5), each shown failing first where the bench can:
  - the charge-mode page's own 17 values sent as the page sends them: all arrive, "Save to EEPROM" saves, and they are there after a restart. The unit's settings are copied before and put back after;
  - the Generic scale chosen and zero pressed at "Waiting for Zero": the unit lives;
  - `scale_frames --handovers 30`: no frame stitched from two;
  - the cleanup screen at 1000.00 and at -1543.24 from the simulated scale (a console command sets its weight, never stored): drawn whole, nothing overrun;
  - a 32-character Wi-Fi name with a key stored, then a restart: the unit starts, and the web reply holds no key. Put back after;
  - access point mode, the owner's phone joins: the unit's page opens by its name.
- **PRs #109 and #113 merged into the branch as a test,** on this PC only: clean, or the conflicts named.
- **A soak after step 3,** which changes the tasks and their priorities: charges back to back under web load, against the limits of ADR-013's last soak.
- **The owner's real throws,** same powder and target, before and after: no worse.

## Not in this ADR

- The TMC2240. The table's shape leaves room for it; it is written when a reworked module is on a bench.
- A watchdog (the owner, 2026-11-15). It restarts the unit, which a user sees, and step 2 already stops the motors of a firmware that has stopped.
- The driver's stall detection, to notice a jammed trickler.
- The control loop (ADR-009), the ESP32's step pulses (ADR-007), the shape of the REST API (ADR-003).
- The servo gate, but for its ramp (step 3).
- The audit's other findings: each has its ADR in "Where the rest of the audit goes".

## Settled before approval

Everything: the owner's decisions of 2026-11-15 are under "Owner's direction".
