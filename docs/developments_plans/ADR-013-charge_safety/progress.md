# ADR-013: progress

What was built and measured. The plan is in the [README](README.md). Logs are kept on this PC under `logs/adr013/`, which git does not track.

## The real scale measured first

2026-11-15, on the bench's Pico W with its A&D FX-300i, the hopper empty.

**What was built** (commit 1101fdb)
- `scale tap on|off`, a console command, never stored. While it is on, the A&D driver prints each frame it decoded as `@sf <ms since boot> <value> <the frame in hex>`, one line built whole and printed with one call. Only the A&D calls it.
- `rc_run.py scale_frames`: the frames recorded by phase and summed up: how many, how many read as a number, as `inf`, as NaN, the texts of those that did not, and the time between frames where the scale runs on. The tap's lines are kept apart from the replies to commands.
- The image: 1.20.82 at 1101fdb. `flash` passed; the stored settings as before.

**The frames** (`scale_frames --idle 120 --handovers 5 --reboots 2 --motors 60`, `VERDICT PASS`)

| Phase | Frames | Numbers | `inf` | NaN | Between frames |
|---|---|---|---|---|---|
| Idle, 2 minutes | 2,512 | 2,512 | 0 | 0 | 41 ms typical, 62 ms longest |
| Five hand-overs from the simulated scale | 417 | 417 | 0 | 0 | |
| Two restarts | 231 | 231 | 0 | 0 | |
| Both motors turning, a minute (coarse 5 rev/s, fine 2) | 1,258 | 1,258 | 0 | 0 | 41 ms typical, 62 ms longest |

- A frame reads `ST,+00000.00 GN` and a CR LF. About 21 a second; the driver looks every 20 ms, so the times between frames come in steps of it.
- The motors turning changed nothing: no frame broke, the rhythm stayed.

**The `inf` of ADR-011's logs**
- 287 `state` replies read `inf` that day from the real scale, each right after the simulated scale handed back. The tap saw no frame that read as `inf`.
- Put to the test: three hand-overs of 30 s each, with the tap on. After the first, the two `state` replies sent at once read `inf`, and the scale's first frames after it all read `ST,+00000.00 GN`, 0.0. After the other two, the first frame had come before `state` asked, and it read 0.000.
- So the `inf` is the NaN the firmware puts in the field when the simulation stops (`scale.c:271`), shown before the scale's next frame. The Pico W prints a NaN as `inf`: the SDK's float code for the RP2040 is built here without `PICO_FLOAT_PROPAGATE_NANS` and treats a NaN as infinity (`float_math.c`: `fnan_or(FPINF)`). The Pico 2 W printed its NaN as `nan` (ADR-011).
- **What follows for fault 1:** a NaN reading does not act the same on the two chips. On the RP2350 it is the overthrow the README describes. On the RP2040 a positive NaN reads as +∞, the fine error as −∞, and the fine stop fires at once: the throw ends short and is taken as done. Reasoned from the above, then shown on the RP2040 with `sim bad` (below); the RP2350 is still to be shown.
- **What follows for this scale:** a frame that does not parse has not been seen on it, in any phase. Fault 1 stays for the other drivers, for a frame broken on the line, and for the firmware's own NaN.

**The figures for the pause, from the rhythm:** 3 of the A&D's 41 ms is 0.12 s, above its longest normal gap of 62 ms; the stop at 10 intervals is under 1 s, so 1 s.

## The faults shown on the code as it is

2026-11-15, on the Pico W, image 1.20.84 at a21d14d: the code before any fix, with the test tools.

**What was built**
- In the simulated scale, console commands only, never stored: `sim bad` (its next reading is a frame that did not parse, a NaN as the drivers publish one), `sim quiet <ms>` (nothing published for that long; its powder keeps falling) and `sim rate <per second>` (its step follows the interval; 100 ms until set).
- `rc_run.py charge_fault <bad|quiet|cleanup> --expect seen|not-seen`: a simulated charge, the fault put in at a weight, `state` read every 50 ms, each change logged, and a judgement of whether the fault was seen.
- `charge_cycle` passed on this image, as before: the simulated scale's change of step altered nothing at its usual 100 ms.

**The three faults** (target 30, fine stop threshold 0.03, the simulated scale at 10 readings a second; all `--expect seen`, `VERDICT PASS`)

| Fault | Put in at | What the tricklers did |
|---|---|---|
| Bad frame | 29.07, the fine trickler at 0.74 rev/s | 0.11 s later the charge ended (state 3, waiting for the cup's removal) at 29.07, its weight shown as `inf`. Short, and taken as done: the RP2040's way, as reasoned |
| Quiet 1.5 s | 29.02, the fine trickler at 0.78 rev/s | It kept 0.78 rev/s through the silence. When readings came back the weight was 29.69: 0.67 poured on no reading |
| The web's cleanup speed, 0.5 rev/s | 27.58, after the coarse trickler's stop and back-off | 0.03 s later the coarse trickler turned again at 0.5 rev/s, and ran on until the charge ended a second later at 30.00. The fine one took 0.5 too, until the charge loop set it again at its next reading |

- **The cleanup fault needs the coarse trickler's stop and back-off to be over.** Sent at 26.2, during the back-off (`coarse -1`), the charge loop set the coarse trickler to 0 at the back-off's end, over the cleanup speed; sent 1 short of the target at 3 rev/s, the simulated powder reached the target within one step, before a 50 ms read could see the speed. Both runs are kept in the logs; the one above is the test.
- The RP2350's way with a bad frame, full speed, is shown when the Pico 2 W is on the bench. Its image of this code is kept (`logs/adr013/before_fix/pico2_w/`).

## Fix 1: one door for a reading

Built 2026-11-15 (599c24f), proven on the Pico W.

**What was built**
- `scale_publish_measurement()` in `scale.c` is the one way a reading is published. All nine drivers and the simulated scale call it, in place of writing the shared field and giving the semaphore. A value that is not a finite number is not published, the charge is not told of a new reading, and it is counted: `bad_frames` in the console's `stats`.
- A driver says a frame did not parse by giving NaN, as seven did already. The door is where the NaN stops: none reaches the charge.
- The Sartorius driver now gives NaN for a frame with no number in it, where it gave 0.0.
- Only `scale.c` writes the field now: the door, and the two places that mark it unknown (at power-up and when the simulation stops), which tell no charge of a reading.

**On the unit** (image 599c24f; marked dirty for a document not yet committed, the code as committed)
- `flash`, and `charge_cycle`: passed, 30.000 for 30.
- `charge_fault bad --expect not-seen`: `VERDICT PASS`. After the bad frame the charge went on as if it had not come: the fine trickler at 0.2 to 0.25 rev/s near the target, no full speed, no early end.
- `stats`: `bad_frames=2` after it. One was `sim bad`; the other, a real one, is below.

**A real broken frame, across a hand-over**
- Thirty hand-overs from the simulated scale, half a second each, with the tap on: one stitched frame, `SST,+00000.00 GN\r`. A stale `S` from before the hand-over stood in front of a new frame, the number's field moved, and the frame did not parse. The door dropped it and counted it (2 to 3). It is the second count of the run above.
- **Why:** the A&D driver takes 17 bytes as a frame, not a frame as what ends in its CR LF. While the simulated scale runs, the driver's task is suspended with a frame half read; on its return the line's input is emptied (`scale.c`), but the driver keeps its count. In use, only a stall of its task long enough to lose input would do the same; that has not been seen.
- **Not this ADR's to fix:** this one gave a frame that does not parse, which the door now stops. A shift elsewhere could give a wrong number that reads as one, which no door sees. Its fix is in the driver, a frame taken only where its line ending is, and belongs to the cleanup of the scale drivers (the audit's verdict), with the other drivers looked at for the same.

## Fix 2: a missing reading pauses; a lost scale ends the charge

Built 2026-11-15 (13b4b9c, and three small commits after it: 4830c51, 73c55b5, a04540b), proven on the Pico W with image 1.20.93 at a04540b.

**What was built**
- The door (`scale_publish_measurement()`) keeps the scale's rhythm: the median of the intervals between its last 16 published readings (`scale_get_usual_interval_ms()`).
- The charge takes it as charging starts and prints it: `Charge: the scale's usual interval 100 ms; a pause after 300 ms without a reading, lost after 1000 ms` for the simulated scale at 10 a second, 250, 750 and 2500 at 4 a second.
- The charge waits for a reading no longer than to the stale point. At it, `charge_mode_pause()`: both tricklers to 0, "Paused: no reading" on the screen, then ten readings 300 ms apart with their spread under `set_point_sd_margin`, all within 6 s of the first one back. The settled weight goes to the charge's normal stops; the D term starts again (its last error set to the current one, so it is 0 and nothing divides by 0); the I term goes on as it was.
- Lost, unsteady, or a third pause: `charge_mode_scale_fault()`: tricklers stopped, the gate closed if enabled, the LEDs in the not-ready colour, the reason on the screen, and any button (the console's `charge stop` is one) back to the menu.
- Each step is printed as a `Charge: ...` line; `rc_run.py charge_fault` checks them with `--want` and `--want-none`.

**Three things found and fixed on the way**
- **The simulated scale could not settle.** It added up to 0.05 of noise to every reading while charging, falling powder or not: a spread of about 0.029 against the 0.02 margin, so the first short gap ended as "unsteady". It now adds noise only while powder falls (4830c51), as a pan does.
- **A failed throw's timer ran on.** The fault screen stays in the charging state, where the screen draws a live timer from the throw's start. It now stops, as at a normal end (73c55b5).
- **The reason ran into the timer** on the top line ("Scale: no reading" over "12.73 s"). At a fault end the timer is not drawn (a04540b).

**On the unit** (target 30, the simulated scale at 10 a second unless said; every run `VERDICT PASS`)

| Test | What happened |
|---|---|
| `charge_cycle` | Passed, 30.000 for 30, no pause |
| A bad frame | No pause, no end; the charge went on |
| A gap of 500 ms at 29.04 | Fine trickler 0.756 to 0 at +0.31 s. Readings back; settled at 29.137, so 0.10 poured on no reading; resumed at +3.00 s at 0.69 rev/s, and went on |
| A gap of 3 s | Paused at 300 ms, then "Scale: no reading" at 1 s; the screen below |
| Three gaps of 500 ms from 12 | Paused and resumed from 13.222, paused and resumed from 16.108, the third: `pause 3, more than 2 in one throw`, "Scale: unreliable" |
| The simulated scale at 4 a second, a whole charge | Its rhythm read as 250 ms; no pause and no end |
| A gap of 2 intervals (100 ms quiet at 10 a second) | No pause |

- A quiet of 200 ms at 10 a second gives a gap between readings of 300 ms, not 200: it begins partway between two readings, and the gap runs to the next one after it. That is 3 intervals, and it paused, as the rule says. The test with 100 ms is the 2-interval one.
- The screen at a lost scale, as a person at the unit sees it (`snap`): the top line "Scale: no reading", the last weight 29.15, the profile.
- Not run: "unsteady". The simulated weight holds still in a pause, so that path is read in the code.

## Fix 3: the web's cleanup speed only in cleanup mode

Built 2026-11-15 (9d9b790), proven on the Pico W with image 1.20.95.

**What was built**
- `/rest/cleanup_mode_state`'s `s1` keeps the speed at any time and sets the motors only while cleanup mode's own loop runs.
- That loop alone marks it (`cleanup_mode_running`, on once it has enabled the motors, off before it disables them). `cleanup_mode_state` cannot tell: the web writes `ENTER` there as soon as it asks for the mode, before the mode has started, so a request for the mode and a speed in a charge would still have reached the motors.

**On the unit**
- `charge_fault cleanup --at 27.5 --cleanup-speed 0.5 --expect not-seen --want-none`: `VERDICT PASS`. The coarse trickler stayed at 0, the fine one went on as the charge set it.
- Cleanup mode from the web, as before: `s0=1` entered it (the menu's form -1); `s1=0.5` set both tricklers to 0.5 rev/s; `s0=0` stopped them and went back to the menu (form 1).

## All three, on the last image

2026-11-15, the Pico W, image 1.20.95 at 9d9b790, every run `VERDICT PASS`:

| Run | |
|---|---|
| `charge_cycle` | 30.000 for 30 |
| `menu_tour --compare tests/menu_baseline/pico_w` | Every form equal |
| `settings --compare` with the copy taken before this ADR's first flash | Every stored value as it was: no stored struct changed |
| A bad frame | No pause, no end; the charge went on |
| A gap of 500 ms | Paused, resumed from a settled weight |
| A gap of 3 s | Paused, then "Scale: no reading" |
| Three gaps of 500 ms | Two pauses resumed, the third "Scale: unreliable" |
| A gap of 2 intervals | No pause |
| The simulated scale at 4 a second, a whole charge | No pause, no end |
| The cleanup speed in a charge | Not applied |

- At the end the real A&D read 0.040 at rest, and `stats` counted 3 bad frames since the flash: the three `sim bad` of the runs.
- **The fault screen's snapshot first failed** ("no two snaps in a row were equal in 4 tries"). The screen was steady: five snaps in a row were equal, the sixth torn. The console copies the screen's buffer with no lock while the charge's screen redraws every 20 ms, so a copy can catch a frame half drawn. `snap` now tries 10 times; the display lock that no drawing task takes is the real cure, the screens' ADR's.
- The screens: "Scale: no reading" over the last weight 29.15, and "Scale: unreliable" over 18.02, each with the profile below.
