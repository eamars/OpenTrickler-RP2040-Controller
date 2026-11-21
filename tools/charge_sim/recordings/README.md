# The bench's recordings

What a real OpenTrickler did on the bench, for ADR-009: a tube turned by a plan with no controller, and the scale's reading beside it. Kept here so that anyone can analyse it, with this folder's tools or their own. The simulation's records (`../records/`) are made from these.

- **`sessions.json`:** each session's unit, powder and scale, written by hand from the session's notes. A folder belongs to the session whose time span holds its stamp.
- **`index.md`:** a line a folder: its powder, test, tube, trials and speeds. Made by `bench/keep.py`.
- **A folder** is one recording, named by when it began (`YYYYMMDD_HHMMSS`, New Zealand time) and its test. `runs.json` holds its trials. `frames.csv` holds the scale's own frames where the tap was on (the first session only; later ones read the weight by `state`, about 47 times a second, and their `frames.csv` is a header alone).
- **`settings_before/`:** the unit's stored settings before the first session, without its Wi-Fi, its update source and its id.

What each session found is in `docs/developments_plans/ADR-009-controller_and_tuning/progress.md`, under the headings `sessions.json` names.

## A trial in `runs.json`

Weights are in grains; the scale's digit is 0.02 gr. Times inside a trial are seconds from the moment the tube was told to stop, so a time before the stop is negative. `turns` is the tube's own turns, from the unit's count of its step pulses.

| Trial (`note`) | What was done | What it holds |
|---|---|---|
| `creepstop` | The tube turned at `speed_asked_rps` for `run_s`, then stopped; `stop` says how (`held`: the driver left on, as in a charge; `released`: switched off; `backoff`: the charge's reverse quarter turn) | `during` and `after`: (time, weight) while turning and after the stop |
| `ramp` | The tube taken through `plan`, a list of (speed in rev/s, seconds), a negative speed backward, then stopped | `marks`: (time, speed) at each change; `during`: (time, weight, the tube's speed); `after`: (time, weight) |
| `throw` | A charge by the firmware's own loop to `target` | `samples`: (time, the charge's state, weight, coarse speed, fine speed); `at_stop`, `at_verdict`, `at_rest`: the weight at each |
| `flick` | The fine tube nudged in one of four ways (`variant`) | `gained_gr`, `steps`, `moves_ms` |
| `map`, `nudge N`, `chain`, `step` | The first session's tests, with the tap on | The unit's time before and after (`t_before_ms`, `t_stopped_ms`, ...), to read the weights from `frames.csv` |
| `precondition` | A turn at 1 rev/s before a recording's trials, to fill the tube | Not a trial |

A recording named `nudgewatch` is `creepstop` trials of 0.05 s each: one small move, the reading watched for 5 s after. A recording named `prime` or `kernelcount` is one `ramp` trial.

## Reading them with this folder's tools

A session's figures, and its record for the simulation (`../README.md`):

```
..\..\venv\Scripts\python bench\measure.py --session pico2w_bm2 --out <a file>
..\..\venv\Scripts\python build_rig.py --session pico2w_bm2 --scale-from pico2w_ar2209
```

The looks in `bench/` take a recording's folder, or `--logs` this folder.

## Adding a session

A bench session writes into `<repo>/logs/adr009`, which git does not track. After it, add the session to `sessions.json` and run `bench\keep.py --logs <repo>\logs\adr009`. It leaves out the console's own logs (four times the size; no tool reads them).

## What is not here

- The second unit (the Pico W's build), fresh AR2209 and AR2208: not yet recorded.
- Throws of the firmware's loop with BM2: not recorded; AR2209 has 29.
- The scale with a known weight on its pan: not recorded.
