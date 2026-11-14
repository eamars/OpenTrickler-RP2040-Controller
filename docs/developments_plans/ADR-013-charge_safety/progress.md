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
