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
- **What follows for fault 1:** a NaN reading does not act the same on the two chips. On the RP2350 it is the overthrow the README describes. On the RP2040 a positive NaN reads as +∞, the fine error as −∞, and the fine stop fires at once: the throw ends short and is taken as done. Reasoned from the above, not yet run; `sim bad` will show it on both.
- **What follows for this scale:** a frame that does not parse has not been seen on it, in any phase. Fault 1 stays for the other drivers, for a frame broken on the line, and for the firmware's own NaN.

**The figures for the pause, from the rhythm:** 3 of the A&D's 41 ms is 0.12 s, above its longest normal gap of 62 ms; the stop at 10 intervals is under 1 s, so 1 s.
