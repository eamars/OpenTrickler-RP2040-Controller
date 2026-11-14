# ADR-012: progress

What was built and measured. The plan is in the [README](README.md). Logs are kept on this PC under `logs/adr012/` and `logs/rc/`, which git does not track.

## Step 1: the driver, our own

2026-11-15, on the bench's Pico 2 W, the hopper empty, the A&D connected. The Pico W is still to do: one swap on the probe, asked of the owner once the other steps are proven on the Pico 2 W.

**Before anything was changed** (image 1.20.99 at b2e691a, ADR-013's)
- The unit's settings copied: `logs/adr012/pico2w/settings_before/`. Its two motors had 500 mA stored, the owner's own figure.
- `menu_tour --compare` passed; `charge_cycle` passed, 14.5 s from charging to the cup prompt.

**What was built** (commits 7badd2a and ad88e1c)
- `tmc_uart.c`: the drivers' one-wire frames. A read is tried up to five times and its answer must be a whole frame for the register asked. A write is checked: the driver's count of accepted writes (`IFCNT`) is read after it and must be one more, modulo 256; if not, the write is tried again, up to five times. No pins and no RTOS in it: it is handed one function, "send these bytes, bring that many back".
- `tmc2209.c`: the chip's registers and fields as tables carried over from Klipper (`tmc2208.py`, `tmc2209.py`), each register built from zero and written whole. The chopper and stealthChop values are the ones the Trinamic library's defaults gave the units, so nothing changes in how a motor turns but the current's reference. `SLAVECONF` is new: an answer waits 3 x 8 bit times, as Klipper sets it for a shared wire.
- `motors.c`: the exchange on the wire for the RP chips. TX reaches the wire through the board's resistor and RX sits on it, so each byte sent is read back and checked before the answer is taken.
- **The current:** `I_scale_analog` is clear. Stored figures are converted once (x0.48, rounded to 10 mA), marked in the upper two bytes of `motor_data_rev`; the default is 400 mA RMS. The web page's label says "mA RMS".
- **A driver that does not answer is a state.** `motors_init` no longer returns an error and `handle_motor_init_error`, the screen that looped for ever, is gone. `motors_ready()` tries a missing driver again and `motors_get_fault()` names what is missing; charge mode and cleanup mode ask before they enable a motor, show the name in their title line, and go back to the menu on a press. Motor settings that cannot be read are the same state, with the defaults standing in.
- `profile.c`'s dead `profile_update_checksum` is gone (audit P4).
- The Trinamic submodule is gone, with its lines in `CMakeLists.txt`.
- The console: `tmc <coarse|fine>` (the registers by field, and the line's counts), `tmc <coarse|fine> setup <n>`, and for proofs, never stored, `tmc <coarse|fine> lose` and `find`: the driver is asked at an address nobody has, and found again.
- Both boards build without a warning of ours. The image: 1.20.113 at ad88e1c.

**Found on the unit: a frame sent on the heels of an answer is lost**
- With the first image (7badd2a) the fine driver needed one retry at every start: 4 starts of 4. A trace showed which: its first write after a restart was not counted, and the read after it got no answer.
- That write is the one place where nothing runs between a driver's answer and the next frame: the count is read for the first time, and the frame, built before, goes out at once. Every other frame has at least a CRC worked out before it. An image with a few instructions more in the exchange (the trace itself) lost nothing.
- So a driver that has just answered needs the wire idle for a moment before it listens again. Klipper leaves 200 us before every frame (`src/tmcuart.c`). The exchange now does the same.
- After it: 40 restarts, 80 set-ups, no write retried. One read was tried twice, once (restart 21, the fine driver).
- The old code's `FIXME: there is known issue that calling IFCNT causes target addr to be incorrect` was very likely this, seen from the other side.

**What the unit showed** (image 1.20.113 at ad88e1c)

| Proof | Result |
|---|---|
| `flash` | Passed. The first flash of this step printed "stored current 500 mA converted to 240 mA RMS" for both motors; the second flash and every restart after it did not: converted once |
| `tmc coarse`, `tmc fine` | `GCONF` reads `000001c0`: `pdn_disable`, `mstep_reg_select`, `multistep_filt`, and `I_scale_analog` clear. `CHOPCONF` `10028103` and `PWMCONF` `c80d0e24`, as the table builds them. `IOIN` gives version 33 (0x21, a TMC2209) and `ms1` set on the fine driver only: the addresses |
| The current | 240 mA asked, 245 mA given: step 7 of 31 (`irun=7`), hold step 3. Standing, `DRV_STATUS` shows `cs_actual=3`; with `motor coarse 1`, `cs_actual=7` |
| The write count's wrap | `tmc coarse setup 60` and `tmc fine setup 60`: 549 checked writes to each, the count wrapped twice, no retry, no failure |
| 40 restarts, the drivers powered | No driver error. `logs/adr012/pico2w/restarts_40.log` |
| A driver that does not answer | `tmc fine lose`, then `charge 30`: "Charge: not started, No fine driver", the screen's title says "No fine driver", a press goes back to the menu. The same for the coarse driver, and for cleanup mode started over the web ("Cleanup: not started, No coarse driver") |
| A driver that answers again | After `find`, `charge_cycle` set the driver up on its own ("Motor driver 1: set up") and passed |
| `charge_cycle` | Passed, 14.4 s from charging to the cup prompt (14.5 s before) |
| `menu_tour --compare` | Passed; no screen changed |
| `settings --compare` | Only the two currents differ: 500 became 240. The copy after: `logs/adr012/pico2w/settings_step1/` |

**What follows from the current being true**
- At the currents units run at, the driver uses a quarter to half of its 32 steps (7 here, 12 at the 400 mA default). The trimmer scaled the current in the analog part and left all 32 steps; that is gone. Klipper's users run this way. If a trickler turns rougher or louder than before, the figure to raise is the mA.

**Left for step 1**
- The Pico W: the same proofs.
- A module lifted by hand with the power off, then a start: the unit must reach the menu. `lose` shows the modes' refusal and the driver found again, not the start itself.

## Step 2: step pulses that stop by themselves

2026-11-15, on the Pico 2 W, the hopper empty. The Pico W is still to do, with step 1's.

**What was built** (commit 9c46945; the image 1.20.115)
- `stepper.pio`, the stepper program: a segment is three words, the steps to make and the counts for the time STEP is high and low. The state machine makes those steps and comes back for the next segment; with nothing to pull it waits with STEP low. No step is made that was not fed. A period is `high + low + 5` clock cycles, one cycle fine, and up to 2^32 cycles long.
- `stepper.pio`, the counter program: a second state machine counts the falling edges on the STEP pin. It found room on both chips beside the LEDs' two and the radio's one.
- `step_pulses.c`: the rate asked for is fed 2 ms at a time, and only when nothing waits in the FIFO, so two segments at most are ahead of the pin: a new rate is on the pin within 4 ms, and pulses that are no longer fed stop within 4 ms. The period is worked out from the chip's clock at each feed. A rate of zero drops what waits and stops at the next feed.
- `motors.c`: a task of its own, priority 10, feeds every tick. The old ramps hand it a rate in steps a second. A direction is changed 3 ms after the speed is set to zero.
- `fault_handler.c`: `isr_hardfault`, which the SDK leaves weak. It stops the step pulses' state machines, holds the STEP pins low and switches the drivers off, by register writes alone, and then stops at a breakpoint as the SDK's own handler does.
- The console: `motor` alone gives the steps counted on each pin, the steps fed, the feeds that came late and the rate last fed, with the unit's clock.
- `rc_run.py pulses`: the hard fault is judged, not only reported; a pin high between 10% and 90% of the samples is pulsing.

**Two things found while building it**
- **One core's hard fault does not stop the other.** The feed is a task; with one core in its fault handler the other can go on running it. Pulses that stop when they are not fed are therefore not enough for a hard fault on their own, and the handler stops the motors itself. With the probe connected both cores halt, so on the bench both mechanisms act; the handler's own part was not shown apart from the other.
- **A short STEP pulse cannot be seen through the probe.** The first program held STEP high for 16 cycles of each period, 0.4% of the time at 1 rev/s, and `pulses` took the motor as stopped. The two halves are now equal: a running motor's STEP pin is high half the time, which the probe samples as 48% to 52% and a multimeter reads as half the supply.

**What the unit showed**

| Proof | Result |
|---|---|
| Speed, coarse at 2 rev/s (81,920 steps a second asked) | 1,699,042 steps counted on the pin in 20.739979 s of the unit's clock: 81,921.1 a second, 0.001% over. The first program gave 81,921.6 over 20.76 s |
| Steps made against steps fed | While turning, the count on the pin is behind the count fed by what is ahead of the pin, 170 to 240 steps at this speed. After a stop it is short by the steps of the segment the stop cut: 5 and 6 here |
| A slow speed | Fine at 0.02 rev/s: 486.4 steps a second, a period of 2 ms. It turns; the old code took anything under 100 steps a second as a stop |
| Reverse | Fine at -3 rev/s, then +3, then 0: the counts follow, no late feed |
| Reading the counter | Ten readings with the motors still: the count does not move. An instruction put into a state machine that waits on a pin does not carry it past the wait |
| Late feeds | None: in 20 s at 2 rev/s, and in a whole `charge_cycle` with the unit on Wi-Fi |
| `pulses` | The pulses stop on a reset through the probe, on an assert and on a hard fault. `VERDICT PASS` |
| `charge_cycle` | Passed, 14.5 s from charging to the cup prompt (14.5 s before the ADR) |
| `menu_tour --compare` | Passed |
| The Pico W | Builds, without a warning of ours. Not on the probe yet |

**Not done in this step, and why**
- The steps of a segment cut by a stop are fed and not made. The count on the pin is the true one, and is what `motor` reports as made.
- The first step after a segment has 5 cycles more of low: 33 ns in 2 ms.
