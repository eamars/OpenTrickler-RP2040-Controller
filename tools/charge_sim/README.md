# charge_sim: a throw on the PC

A simulation of one charge: two rotary tubes, falling kernels, a scale, and a controller, a millisecond a step. Made for ADR-009, whose algorithm is developed here and verified by experiment (the owner, 2026-11-20). Pure Python; run it with the venv's interpreter from this folder.

**A rig is a record of what was measured on a unit,** one JSON file in `records/`: a scale, a powder's kernel, and each tube with that powder in it. No number in it is set by hand. Each names its source:
- **measured:** read straight off a recording (a tube's grains a turn, the scale's rhythm, what a reverse turn spills);
- **solved:** the one value at which the simulation, run through the bench's own test, gives the bench's figure (the scale's delay, a tube's bursts at each speed, what falls after a stop from each speed);
- **stated:** known of the rig without a recording (the scale's digit, the kernel's mass, the fall to the cup);
- **assumed:** no recording gives it yet. `pinned.py` says which of these matter.

Another unit, powder or scale is another record, made by the same command. Nothing is tuned to make it fit.

## From a bench session to a rig

```
..\..\venv\Scripts\python build_rig.py --logs <checkout>\logs\adr009 --manifest fit\bench_pico2w.manifest.json
..\..\venv\Scripts\python fit_report.py --rig bench_pico2w
..\..\venv\Scripts\python pinned.py --rig bench_pico2w
```

1. **The manifest** (`fit/<name>.manifest.json`) says which folder of the session is which test, the settings of any throws recorded, and what is stated of the rig.
2. **`build_rig.py`** sums the recordings up as figures (`fit/<name>.json`, by `bench/measure.py`) and makes the record (`records/<name>.json`) from the open-loop tests alone: the maps, the stops, the steady runs and the nudges. About ten minutes on a PC with many cores.
3. **`fit_report.py`** sets every figure the bench measured beside the simulation's. The throws of today's loop were not used to make the record: they are the check, counted apart.
4. **`pinned.py`** moves each number of the record by a half and says which the recordings pin down, and which of the others a throw would notice: those need a recording at the next session.

## What to ask of a rig

```
..\..\venv\Scripts\python mc.py --rig bench_pico2w --controller legacy_slow_finish --throws 500
..\..\venv\Scripts\python scale_map.py --rig bench_pico2w
..\..\venv\Scripts\python evenness.py --rig bench_pico2w
..\..\venv\Scripts\python scale_step.py --rig bench_pico2w
```

| File | What it is |
|---|---|
| `plant.py` | A tube with a powder in it, as tables a calibration measures (its flow a turn, the size of its releases and what falls after a stop, each by speed), and the cup |
| `scale.py` | A scale as the charge loop meets it: a delay, then a reading that follows a change at once or holds a small one before taking it. Its sets of estimates for other makes are priors, never measured |
| `rigs.py`, `records/` | The rigs: a record read into the simulation's parts |
| `build_rig.py` | A record from a session's figures |
| `fit_report.py` | The simulation beside the bench, figure by figure; `fit/` holds the figures and the last comparisons |
| `pinned.py` | Which numbers the recordings pin down, and which of the rest a throw notices |
| `bench_twin.py` | The bench's tests (a steady turn and a stop, nudges, today's loop's throws) run against the simulation, in the shape the bench saves |
| `bench/` | The bench's own tools: `record.py` (recordings through the debug probe), `stats.py` (a recording as figures, for the bench and the simulation alike), `measure.py`, and the looks at a recording |
| `mc.py` | Many throws summed up: over, under, error, time, and the seconds a good throw costs |
| `scale_map.py` | A controller across scales: overs and time against a scale's delay, reading period and hold |
| `evenness.py` | How evenly the fine tube must pour at low speed for a finish to hold the tolerance |
| `scale_step.py` | What a scale model shows of a weight set on its pan |
| `legacy_pid.py` | Today's loop, ported from `charge_mode.cpp`, with the three settings thrown on the bench |
| `flow_dose.py`, `calibrate.py` | The first candidate and its calibration, written before any bench session: they run on a record, their old figures are void, and the design that follows starts from them |
| `controllers.py` | The controllers by name |
| `throw.py` | One throw to its verdict; a session of throws on one rig |
| `geometry/` | The scripts that measured the tubes from their STLs |

A controller has `start(target, now_ms)`, `on_tick(now_ms)` once a millisecond, `on_reading(now_ms, weight)` on each reading, and the attributes `coarse_rps`, `fine_rps` and `done`. That is the seam ADR-009's controller module would have.

The same seed gives the same throws. A session keeps the tubes' state from throw to throw, as a loading session does.
