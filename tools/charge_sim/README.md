# charge_sim: a throw on the PC

A simulation of one charge: two rotary tubes, falling kernels, a scale, and a controller, a millisecond a step. Made for ADR-009's research (`docs/developments_plans/ADR-009-controller_and_tuning/research/README.md`), which says what in it is measured and what is assumed. Pure Python; run it with the venv's interpreter from this folder.

```
..\..\venv\Scripts\python mc.py --controller legacy --throws 500
..\..\venv\Scripts\python mc.py --controller flow_dose --scale gng --powder ar2209
..\..\venv\Scripts\python matrix.py --throws 400
```

| File | What it is |
|---|---|
| `plant.py` | A tube (the powder it holds, its releases in kernels and clumps, late kernels, a retreating front) and the cup |
| `scale.py` | A scale as the charge loop meets it, and four parameter sets |
| `rigs.py` | The two tubes ADR-009 uses, the powders, the unit as a rig. Every flow figure is a prior |
| `legacy_pid.py` | Today's loop, ported from `charge_mode.cpp` |
| `flow_dose.py` | The candidate: flow by the weight still to go, then nudges on settled readings |
| `calibrate.py` | The calibration the candidate needs, written as a controller so the harness runs it |
| `controllers.py` | The controllers by name |
| `throw.py` | One throw to its verdict; a session of throws on one rig |
| `mc.py` | Many throws summed up: over, under, error, time |
| `matrix.py` | Today's loop against the candidate as the plant's unknowns are varied |
| `geometry/` | The scripts that measured the tubes from their STLs |

A controller has `start(target, now_ms)`, `on_tick(now_ms)` once a millisecond, `on_reading(now_ms, weight)` on each reading, and the attributes `coarse_rps`, `fine_rps` and `done`. That is the seam ADR-009's controller module would have.

The same seed gives the same throws. A session keeps the tubes' state from throw to throw, as a loading session does.
