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
