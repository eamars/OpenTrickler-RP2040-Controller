# charge_sim: a throw on the PC

A simulation of one charge: two rotary tubes, falling kernels, a scale, and a controller, a millisecond a step. Made for ADR-009, whose algorithm is developed here and verified by experiment (the owner, 2026-11-20). Pure Python; run it with the venv's interpreter from this folder.

**A rig is three sets of numbers:** a powder (`Powder`), each tube (`Tube`) and a scale (`ScaleModel`), put together in `rigs.py`. Another unit, powder or scale is another set, not other code. The rig `bench_pico2w` is fitted to the bench's recordings of 2026-11-19 and 20; `reference` is the plant assumed before any bench session and is not to be quoted.

**Whether the simulation is right is a table, not a claim:**
```
..\..\venv\Scripts\python fit_report.py                     every figure the bench measured, beside the simulation's
..\..\venv\Scripts\python fit_search.py fine.slump_deg=16,20 powder.regularity=1.4,2 --match "fine,nudges"
..\..\venv\Scripts\python bench\measure.py --logs <checkout>\logs\adr009 --out fit\bench_pico2w.json
```
`fit/bench_pico2w.json` holds the measured figures and the recordings each came from; `fit/report_seed*.txt` the last comparison. The fit of 2026-11-20 puts 70 to 72 of 90 figures within tolerance (18 of 58 on the plant assumed before the bench); what it does not reproduce is listed in the ADR's `progress.md`.

```
..\..\venv\Scripts\python mc.py --controller legacy --throws 500
..\..\venv\Scripts\python mc.py --controller flow_dose --scale gng --powder ar2209
..\..\venv\Scripts\python matrix.py --throws 400
```

| File | What it is |
|---|---|
| `plant.py` | A tube (its flow a turn, releases in kernels and bursts that grow as it slows, what falls after a stop and is owed at the next start, what a reverse turn spills, a variation by angle once a turn and once a rifling start) and the cup |
| `scale.py` | A scale as the charge loop meets it: a delay, then a reading that follows a change at once or holds a small one before taking it; the bench's A&D and four sets of priors |
| `rigs.py` | The rigs by name: `bench_pico2w`, fitted; `reference` and the stand-in powders, priors |
| `bench_twin.py` | The bench's tests (a steady turn and a stop, nudges, today's loop's throws) run against the simulation, in the shape the bench saves |
| `fit_report.py`, `fit_search.py` | The simulation beside the bench, figure by figure; sets of parameters tried and ranked |
| `fit/` | The bench's figures and the last comparison |
| `bench/` | The bench's own tools: `record.py` (recordings through the debug probe), `stats.py` (a recording as figures, for the bench and the simulation alike), `measure.py`, and the looks at a recording (`release_events.py`, `fold_by_angle.py`, `creep_detail.py`, `throw_trace.py`, `steady_release.py`) |
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
