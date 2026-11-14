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

## Step 3: the motors

2026-11-15, on the Pico 2 W, the hopper empty. The Pico W is still to do.

**What was built** (commit bfdc11c; the image 1.20.117)
- `motors.c`: one task, priority 10, every tick. It moves each motor's speed toward its target by the acceleration and feeds the step pulses. The two per-motor tasks, their queues and the ramp that looped until its time was up are gone, and with them the feed task of step 2.
- `motor_set_speed` stores the target and returns. A speed that is not a number is refused and said on the console; one past the motor's limit is held to it.
- A change of direction goes through a stop: the speed is held at zero for the tick in which the pulses end, and the direction pin changes in the next.
- **Settings are checked.** A set of settings is taken only if every one is in range, at the web and when the stored ones are loaded; a stored set out of range gives that motor the defaults, in RAM only. The ranges are in `motors.c`. A refused set changes nothing and stores nothing, and the web's reply shows the settings as they still are.
- **Settings are applied when they change.** A new current, microstep count or sense resistor is written to the driver at once.
- `motor_get_velocity` gives the speed the motor turns at now, ramps included. The simulated scale asks for it (`motor_get_shaft_velocity`) and no longer reads the motors' structs.
- `cleanup_mode.cpp`: the speed stays a number within the lower of the two motors' limits, from the encoder and from the web (audit M4).
- `servo_gate.c`: the ramp moves by ticks and sleeps between; its time is a difference of tick counts (audit G1, G2). No servo is on the bench: built and read, not seen to move.
- `motors.h` lost `motor_task`, declared and never written.

**What the unit showed**

| Proof | Result |
|---|---|
| Full speed | Coarse at 5 rev/s: 204,918 steps a second fed, 0.06% over the 204,800 asked (the clock's rounding at a period of 732 cycles) |
| A ramp | With the acceleration set to 2 rev/s² over the web, `motor` read every 20 ms shows the rate 2,048 steps a second higher each time: 102,400 steps/s², which is 2 rev/s² |
| Reverse and stop | 5 rev/s to -5 to 0: the speeds follow, no late feed |
| A setting out of range | `m0=0`, `m7=0`, `m3=7` over the web: each refused, the reply unchanged |
| A setting applied when it changes | `m3=16`: `tmc coarse` shows `mres=4` at once, and `motor coarse 1` turns at 2,559.99 steps a second (2,560 asked). Put back to 256: `mres` 0 again |
| Cleanup mode's speed | `s1=99` gives 5, `s1=nan` leaves it, `s1=-99` gives -5; both motors follow |
| The scale while the motors ramp and turn | `scale_frames`: 40 ms typical and 61 ms longest between frames, idle and with both motors turning alike |
| `charge_cycle` | Passed, 14.5 s |
| `menu_tour --compare` | Passed |
| `settings --compare` against step 1's copy | Nothing differs: the current stays 240, not converted again |
| `soak 600` | `VERDICT PASS`. 23 charges, every one passed; no late feed in any; `heap_free` 97,464 throughout, 1,320 more than before the ADR; 125 of 2,532 web reads missed, 4.94%, longest run 2 (6.86% and 3 on this board in ADR-011) |
| The Pico W | Builds, without a warning of ours. Not on the probe yet |

**Seen, not this ADR's:** the task with the least stack to spare is the charge screen's, 64 to 90 words in these runs (audit Y4, given to ADR-005).

## Step 4: the drivers read while running

2026-11-15, on the Pico 2 W, the hopper empty. The Pico W is still to do.

**What was built** (commit 2bb7333, and the tool's fix after it; the image 1.20.118)
- `motors.c`: a task, priority 2, reads each driver's `DRV_STATUS` and `GSTAT` once a second. A short to ground or to the supply, a switch-off for heat, a fallen supply (`uv_cp`) or a driver that started anew (`reset`) is a fault when it is read three times running; a driver that answers none of three readings in a row is absent. Klipper's figures.
- **A fault stays until a mode next asks.** The first fault found is kept, so the charge it came in sees it even if the driver is well a second later. `motors_ready()`, which the modes call before they enable a motor, sets a faulted driver up again and reads it afresh.
- A driver that started anew is written its registers again at once.
- The temperature warnings go to the console when they change.
- `charge_mode.cpp`, five lines: the charge loop asks `motors_get_fault()` at each reading and ends the throw as it does for a failed scale, with the driver and the fault in the title.
- The console: `state` ends with `drivers=<coarse>,<fine>`, each `ok`, `absent`, `short`, `hot`, `reset` or `supply`. `tmc <coarse|fine> fault <short|hot|reset|supply|none>`, for proofs and never stored, puts the flags into what the driver is read to report. `lose` now changes the address and nothing else: the readings find the driver gone, as they would in use.
- `rc_run.py charge_cycle` no longer counts the state the unit shows before charge mode has set its first: the mode takes longer to start when it sets a driver up again first.

**What the unit showed.** No real short, heat or loss of supply was made: the flags were put in at the reading, and everything after the reading is the firmware's own.

| Proof | Result |
|---|---|
| A short during a charge | `tmc coarse fault short` while charging: "Motor driver 0: short", "Charge: ended, Coarse driver short", both motors at 0, the title says so, `drivers=short,ok`. A press goes back to the menu |
| The fault still there | The next `charge 30`: "Charge: not started, Coarse driver short" |
| A driver that started anew during a charge | `tmc fine fault reset`: "Charge: ended, Fine driver reset"; the driver was written again |
| A driver that stops answering during a charge | `tmc fine lose`: after three readings "Motor driver 1: no answer to 3 readings in a row" and "Charge: ended, No fine driver" |
| Heat, and a fallen supply | `drivers=ok,hot` with the warning logged; `drivers=supply,ok` |
| After each | `fault none` or `find`, then `charge_cycle`: the driver is set up again by the mode and the charge passes, 14.5 s |
| No fault that is not one | The charges of this step and `menu_tour` ran with the readings going: none ended by them, and no read of the line retried but those to the driver made absent |
| `menu_tour --compare` | Passed |

**Not done:** a fault found while charge mode waits for zero or for the cup is seen when the next throw starts, not before. The motors stand still in those states.

## Step 5: the small fixes that cannot wait

2026-11-15, on the Pico 2 W, the hopper empty, the A&D connected. The Pico W is still to do.

Each finding was read again in the code first: all six were as the audit said. One commit each (baa93ea to 4392a59). Three were shown failing first, on the image of step 4 flashed back for it.

| Fix | Before, on step 4's image | After |
|---|---|---|
| **A web form arrives whole** (H1, H2). The query's buffer is 512 bytes and no longer on the stack; a request may carry 20 values; a query longer than the buffer is refused whole | The charge-mode page's 17 values, 157 characters, with `c16=1.0` and "save": the reply showed `c14` false, where `true` was sent and cut; `c16` was not taken; after a restart nothing was stored | The same form: every value taken, and stored after a restart. The unit's settings were put back and compare equal to step 1's copy |
| **The Generic scale's zero** (S1): a function that does nothing | Generic chosen and stored, then zero over the web: no reply, the unit dead, a core halted. Recovered by flashing | The same: the reply comes and the unit lives. The A&D was chosen and stored again |
| **The A&D driver takes a frame at its line ending.** A frame is the 17 bytes that end in CR LF; a line of another length is counted as a frame that does not parse | 30 hand-overs from the simulated scale: one frame of 2,522 was `ST,ST,+00000.00 G`, stitched from two | 30 hand-overs: 12 lines that are not frames (`000.00 GN`, `GN`, `ST,+0ST,+00000.00`, ...), every one dropped and counted, none read as a weight. `ST,+0ST,+00000.00` would have read as 0 before |
| **The cleanup screen writes a weight with its length** (M5), in a buffer with room for any; every line of the screen is written with its length; an unknown number of decimals gives an empty string. `weight_to_string` in `common.c` | Not shown: an overrun of one byte on a task's stack shows nothing by itself | `sim weight -1543.24` and `sim weight 123456.789` in cleanup mode: both drawn whole (`logs/rc/snaps/`) |
| **The access point's DNS server** (W1): the one the file keeps is the one handed over | Not shown | Not shown: it needs a phone on the unit's access point. Built and read |
| **The Wi-Fi name and key keep their endings** (W2). One that does not fit with its ending (31 and 63 characters) is refused, and the form with it; stored ones are ended when loaded; the two screen lines are declared in one size | Not shown: it would have stored a name that cannot be joined | A 32-character name with a key, and a 64-character key: refused, the reply unchanged and without a key, "WiFi: a name of 32 characters is refused" on the console |

**Found while proving it: the web has almost no memory to spare**
- The first fix gave a request room for 32 values. The soak after it lost 38.6% of the web's reads, in runs of up to 63, and 32.4% when run again; every charge passed and `heap_free` did not move.
- Each value is 8 bytes in every connection's state, and that state comes out of lwIP's heap of 4,000 bytes. 32 values in place of 16 are 128 bytes more a connection.
- With room for 20, which the longest form's 17 need, a soak lost 5.7%, and the final one 4.5%. ADR-003 has the figures.

**Found while proving it: a refused Wi-Fi name left its key behind.** The first form of the fix refused the name and took the key sent with it, into RAM. The form is now refused whole. The unit was restarted, so the key in use is the stored one.

**Left alone, as planned:** `charge_mode.cpp` is not touched by this step. PRs #109 and #113 were merged into the branch as a test, on this PC and pushed nowhere (`git merge-tree`): both merge without a conflict. #109 conflicted once, in `common.h`, where the new declaration sat beside a line it adds; the declaration was moved.

**The audit's other findings** are written, in words, into ADR-003, 004, 005, 006, 009 and 010, each under "Handed on by ADR-012".

## Where it stands

2026-11-15. All five steps are built. The Pico 2 W runs the last image, 1.20.130 at 8b8395d, and on it:

| Proof | Result |
|---|---|
| `soak 600` | `VERDICT PASS`. 23 charges, every one passed; no late feed; `heap_free` 95,272 throughout; 119 of 2,643 web reads missed, 4.50%, longest run 3. The drivers were read some 2,400 times meanwhile: 8 reads were tried twice, none failed |
| `pulses` (on e5b61a0, with its fault images) | The pulses stop on a reset, on an assert and on a hard fault |
| `charge_fault none`, `bad`, `quiet` | ADR-013's proofs pass as before |
| `menu_tour --compare` | Passed; no screen changed in this ADR |
| `charge_cycle` | Passed, 14.5 s from charging to the cup prompt, as before the ADR |
| `settings --compare` | The unit's settings are as before the ADR, but for the two currents: 500 became 240, once |
| The Pico W | Builds without a warning of ours: 679,296 bytes of flash, 205,676 of RAM at link time |

**Left before the ADR can close**
- **The Pico W:** every proof above, on the other unit. It needs the owner to put it on the probe.
- **By the owner's hand:**
  - a driver module lifted with the power off, then a start: the unit must reach the menu and name the driver;
  - a phone on the unit's access point: the unit's page must open by its name (W1);
  - real throws, same powder and target, against how the unit threw before: no worse. The motors now run at the current the stored figure says, which the driver gives in a quarter to half of its 32 steps.
- **Not shown apart:** the hard-fault handler's own stop, since with the probe connected both cores halt and the pulses starve as well.
- No servo is on the bench: the gate's new ramp is built and read, not seen to move, and "shutter0" and "shutter1" (audit G4) are still unchecked.

Nothing is pushed. The branch is `adr012_motor_drivers`, on this PC.
