# charge_sim: a throw on the PC

A simulation of one charge: two rotary tubes, falling kernels, a scale, and a controller, a millisecond a step. Made for ADR-009, whose algorithm is developed here and verified by experiment (the owner, 2026-11-20). Pure Python; run it with the venv's interpreter from this folder.

**A rig is a record of what was measured on a unit,** one JSON file in `records/`: a scale, a powder's kernel, and each tube with that powder in it. No number in it is set by hand. Each names its source:
- **measured:** read straight off a recording (a tube's grains a turn, the scale's rhythm, what a reverse turn spills);
- **solved:** the one value at which the simulation, run through the bench's own test, gives the bench's figure (the scale's delay, a tube's bursts at each speed, how late its powder shows at each speed);
- **taken:** the scale of another record, for a session the same scale weighed;
- **stated:** known of the rig without a recording (the scale's digit, the kernel's mass, the fall to the cup);
- **assumed:** no recording gives it yet. `pinned.py` says which of these matter.

Another unit, powder or scale is another record, made by the same command. Nothing is tuned to make it fit.

**The bench's recordings are in `recordings/`** (its README says what a folder and a trial hold): every session so far, copied from the bench's own folder by `bench/keep.py`. `recordings/sessions.json` names each session's unit, powder and scale; a record is built from one session.

## From a bench session to a rig

```
..\..\venv\Scripts\python build_rig.py --session pico2w_ar2209
..\..\venv\Scripts\python build_rig.py --session pico2w_bm2 --scale-from pico2w_ar2209
..\..\venv\Scripts\python fit_report.py --rig pico2w_bm2
..\..\venv\Scripts\python pinned.py --rig pico2w_ar2209
```

1. **The manifest** (`fit/<session>.manifest.json`) names the folders no rule finds (the watched nudges, the speed plans with no controller, the throws and their settings, the tapped maps where the session has them) and what is stated of the rig. A session's stops are every `*_creepstop` folder in its time span.
2. **`build_rig.py`** sums the session up as figures (`fit/<session>.json`, by `bench/measure.py`) and makes the record (`records/<session>.json`) from the maps, the stops, the steady runs and the nudges. A second powder weighed by the same scale takes that scale from the first record (`--scale-from`) and solves its tubes alone.
3. **`fit_report.py`** sets every figure the bench measured beside the simulation's. The speed plans turned with no controller and the throws of today's loop were not used to make the record: they are the check, counted apart.
4. **`pinned.py`** moves each number of the record by a half and says which the recordings pin down, and which of the others a throw would notice: those need a recording at the next session.

## What to ask of a rig

```
..\..\venv\Scripts\python mc.py --rig pico2w_ar2209 --controller legacy_slow_finish --throws 500
..\..\venv\Scripts\python scale_map.py --rig pico2w_ar2209
..\..\venv\Scripts\python evenness.py --rig pico2w_ar2209
..\..\venv\Scripts\python scale_step.py --rig pico2w_ar2209
..\..\venv\Scripts\python bands.py --controllers jog,glide,legacy_slow_finish --scales measured,slow
```

**The two proposals of 2026-11-21** (`docs/developments_plans/ADR-009-controller_and_tuning/research/proposals_2026-11-21.md`) are the controllers `jog` and `glide` (`jog_loose` and `glide_loose` for the band of 0.04 gr). Each session of throws begins with its own calibration on the rig.

| File | What it is |
|---|---|
| `staged.py` | The two proposals as one controller with two finishes, and the calibration both take every number from, run on the rig as a unit would run it |
| `bands.py` | Controllers on rigs and scales, each judged within 0.02 and within 0.04 gr: good, over, under, time |
| `scales_like.py` | Other scales to try a controller on: the measured one with its delays stretched. Stand-ins, not any make |
| `plant.py` | A tube with a powder in it, as tables a calibration measures (its flow a turn, the size of its releases and how late its powder shows, each by speed), and the cup |
| `scale.py` | A scale as the charge loop meets it: a delay, then a reading that follows a change at once or holds a small one before taking it. Its sets of estimates for other makes are priors, never measured |
| `rigs.py`, `records/` | The rigs: a record read into the simulation's parts |
| `build_rig.py` | A record from a session's figures |
| `fit_report.py` | The simulation beside the bench, figure by figure; `fit/` holds the figures and the last comparisons |
| `pinned.py` | Which numbers the recordings pin down, and which of the rest a throw notices |
| `bench_twin.py` | The bench's tests (a steady turn and a stop, nudges, a speed plan with no controller, today's loop's throws) run against the simulation, in the shape the bench saves |
| `bench/` | The bench's own tools: `record.py` (recordings through the debug probe: maps, stops, nudges, throws, and `ramp` for speeds one after another with no stop, backward too), `stats.py` (a recording as figures, for the bench and the simulation alike), `measure.py`, and the looks at a recording (`after_stop.py`, `ramp_look.py`, `steps_look.py`, `reverse_look.py`, `nudge_watch.py` for the session of 2026-11-20's evening, not yet figures of `stats.py`); `trial.py` (both proposals run on a unit from the PC, their calibration and their throws, or against a made-up unit that is the simulation; `--forecast` writes what the simulation expects of it; `--remeasure` makes a kept trial's calibration again from its kept turns, with no tube turned; `research/trial_guide.md` has the session); `best_finish.py` (what a finish could reach with a powder, played on a trial's own measured moves and stops: the share of good throws by where the stream's stop is aimed); `keep.py` (a session's recordings copied into `recordings/`); `scale_or_powder.py` and `topup_bound.py` (two questions answered from the recordings alone, with no model: whether what arrives after a stop is the scale's, and what a finish by small moves can reach) |
| `mc.py` | Many throws summed up: over, under, error, time, and the seconds a good throw costs |
| `scale_map.py` | A controller across scales: overs and time against a scale's delay, reading period and hold |
| `evenness.py` | How evenly the fine tube must pour at low speed for a finish to hold the tolerance |
| `scale_step.py` | What a scale model shows of a weight set on its pan |
| `legacy_pid.py` | Version 1's loop, ported line for line from `charge_mode.c`, with the three settings thrown on the bench. The firmware's controller with the forecast off (its default) is judged against it |
| `flow_dose.py`, `calibrate.py` | The first candidate and its calibration, written before any bench session: they run on a record, their old figures are void, and the design that follows starts from them |
| `controllers.py` | The controllers by name |
| `throw.py` | One throw to its verdict; a session of throws on one rig |
| `workers.py` | Jobs spread over half the PC's cores: fresh processes for every batch and a failed job run once more (its head says why) |
| `geometry/` | The scripts that measured the tubes from their STLs |

A controller has `start(target, now_ms)`, `on_tick(now_ms)` once a millisecond, `on_reading(now_ms, weight)` on each reading, and the attributes `coarse_rps`, `fine_rps` and `done`. The firmware's charge controller has the same seam (`src/services/charge_control`).

## What the firmware is checked against

ADR-009 built three parts of the firmware from this folder's Python, and the PC runs the firmware's C beside it, call for call:

| Python | The firmware's C | The check |
|---|---|---|
| `calibration.py`: the calibration's method (the cup, the scale's settle time and lag, each tube's least speed) | `src/services/calibration_method/calibration_method.c` | `..\..\venv\Scripts\python ..\..\tests\calibration_sim.py` |
| `step2.py`: the charge controller (the coarse tube's PID, the fine tube's PID on the forecast) | `src/services/charge_control/charge_controller.c` | `..\..\venv\Scripts\python ..\..\tests\step2_sim.py` |
| `step2_tuner.py`: the auto-tuner, a window of throws at a time | `src/services/charge_control/charge_tuner.c` | the same |

A change to one side is made to the other, and both checks are run. The other scripts here (`*_study.py`, `*_cases.py` and the like) are ADR-009's research; each one's opening lines say what it asks.

The same seed gives the same throws. A session keeps the tubes' state from throw to throw, as a loading session does.
