# ADR-009: the bench trial of the two proposals, run from the PC

2026-11-21. The owner, on being asked: "Yes, prepare it". For whoever runs it, person or agent. It is the first contact of `proposals_2026-11-21.md` with a real tube.

**Nothing here has touched a unit.** The tool was tried against a made-up unit, which is the simulation answering the console's commands (`trial.py --made-up`). The bench rules hold (AGENTS.md): no run before the owner says the bench is ready.

## What it is

The PC is the controller. `tools/charge_sim/bench/trial.py` reads the weight by the console's `state` and turns the tubes by `motor`, as the recorder does, and runs on that the very code the simulation ran (`tools/charge_sim/staged.py`). No firmware is changed and none is flashed.

1. **The calibration's measurements,** once: prime, the probe, grains a turn, the stops by flow, the small moves (`proposals_2026-11-21.md`, 3.2, steps 2 to 6).
2. **Four sets,** in this order: proposal 1 within 0.02 gr, proposal 2 within 0.02 gr, proposal 1 within 0.04 gr, proposal 2 within 0.04 gr. Each set is ten rehearsals of 1 gr and then 25 throws of 10 gr.
3. **The scale is never zeroed.** Each throw is counted from the weight the cup rests at, so the cup fills as in a recording.

**What a unit's own firmware would do better:** it counts its steps, where the PC counts the speeds it asked for; and it acts every millisecond, where the PC acts once a console command (20 to 45 ms). A proposal that holds from the PC holds there.

## What it needs from the owner

- The unit on the probe, at its menu (not in a charge or a cleanup), its scale connected, in grains, at FAST, warm.
- Powder in the hopper, and its name. A forecast is written for the bench's two: ADI BM2 and the air-exposed AR2209.
- **An empty cup on the pan and the scale zeroed with it,** so that the reading is the powder in the cup. The tool stops before the cup would pass 600 gr.
- **Two emptyings of the cup** in a whole trial: after the first set (about 410 gr in it) and after the third (about 520 gr). The tool stops and says so; the cup goes back empty and the trial goes on from where it stopped.
- **About an hour and ten minutes, and the cup's two emptyings.** Nobody at the bench meanwhile, and no other console session: a second one takes the first one's commands (the bench trap of 2026-11-21).

## The commands

From the main checkout, with its venv (its `rc_run.py` and `build/` match the unit's firmware):

```
venv\Scripts\python .claude\worktrees\adr009-research\tools\charge_sim\bench\trial.py --repo C:\workspace\OpenTrickler-RP2040-Controller --name pico2w_bm2
```

After the cup was emptied, with the folder the first run named:

```
venv\Scripts\python .claude\worktrees\adr009-research\tools\charge_sim\bench\trial.py --repo C:\workspace\OpenTrickler-RP2040-Controller --resume logs\adr009\<stamp>_trial_pico2w_bm2
```

`--name pico2w_ar2209` for AR2209. The name picks the forecast the trial is set beside at its end.

## How it stops, and how it is stopped

- **By itself,** with both motors stopped: when the cup needs emptying; when a throw is not done in 120 s; when a reading is 3 gr over its target; when a tube has turned further than a throw can need; when the scale gives no weight or the console no reply. It prints `STOPPED:` and the reason.
- **By the keyboard:** Ctrl-C stops both motors and ends the session.
- **If the PC or the console dies, a motor may be left turning,** as with the recorder. Then at once:

```
venv\Scripts\python tools\remote_control\rc_run.py --board pico2_w send "motor coarse 0" "motor fine 0"
```

- **A held tube is not switched off.** The console's `motor <tube> 0` switches the driver off, which a charge never does; so through a throw a stopped tube is asked for a speed too slow to pour (in a minute a thousandth of a coarse turn, six thousandths of a fine one). At a throw's end both are switched off.

## What is kept

In the trial's folder, `logs\adr009\<stamp>_trial_<name>` of the main checkout: `calibration.json` (what was measured), `throws_<finish>_<band>.json` (the set's calibration, and every throw's readings, speeds and forecast), `trial.json` (the figures), and `runs.json` (the calibration's stops and moves, as the recorder keeps them). `bench\keep.py` copies them into the repository after the session, once the session is added to `recordings/sessions.json`.

## The forecast, written first

`trial.py --forecast <record>`: the same trial against the simulation sixteen times, each with its own calibration; for each figure the middle one, and the range all but the lowest and the highest fell in. Kept in `tools/charge_sim/fit/trial_forecast_<record>.json`. 25 throws of 10 gr a set, the PC's pace included.

| ADI BM2 | Good | Over | A throw, median | Moves a throw | First rest against the target, digits; its spread |
|---|---|---|---|---|---|
| Proposal 1 within 0.02 gr | 96%; 80 to 100% | 4%; 0 to 20% | 11.4 s; 8.9 to 14.3 | 2.4; 1.7 to 3.8 | -3.3; 1.3 (1.0 to 3.0) |
| Proposal 2 within 0.02 gr | 84%; 76 to 96% | 16%; 4 to 24% | 11.2 s; 7.5 to 13.0 | 0.6; 0.3 to 0.8 | -0.9; 1.6 (1.1 to 4.1) |
| Proposal 1 within 0.04 gr | 100%; 96 to 100% | 0%; 0 to 4% | 8.3 s; 7.4 to 10.6 | 1.2; 0.7 to 1.5 | -3.2; 1.6 |
| Proposal 2 within 0.04 gr | 100%; 92 to 100% | 0%; 0 to 8% | 8.5 s; 7.2 to 10.4 | 0.1; 0.0 to 0.4 | -1.0; 1.3 |

| AR2209, air-exposed | Good | Over | A throw, median | Moves a throw | First rest against the target, digits; its spread |
|---|---|---|---|---|---|
| Proposal 1 within 0.02 gr | 76%; 60 to 84% | 16%; 4 to 32% | 14.5 s; 13.8 to 21.8 | 3.6; 3.2 to 5.4 | -7.2; 4.3 (2.8 to 12.8) |
| Proposal 2 within 0.02 gr | 74%; 52 to 84% | 26%; 12 to 48% | 10.9 s; 7.5 to 14.5 | 1.2; 0.5 to 1.8 | -0.9; 2.6 (1.8 to 6.6) |
| Proposal 1 within 0.04 gr | 90%; 84 to 96% | 8%; 4 to 16% | 12.0 s; 9.7 to 17.7 | 2.6; 1.6 to 4.2 | -6.9; 3.7 |
| Proposal 2 within 0.04 gr | 88%; 84 to 96% | 10%; 4 to 16% | 9.1 s; 7.9 to 11.8 | 0.5; 0.2 to 0.8 | -0.9; 2.1 |

With 25 throws one throw is four points, so the ranges are wide.

## Where I expect the bench to differ from the forecast, said before the run

1. **Slower.** The simulation's scale shows nine small moves in ten within 2 to 2.5 s; the bench's A&D took 4 s. The probe will set longer waits. Each move and each rest then costs about 1.5 s more: 5 to 8 s a throw for proposal 1 within 0.02 gr, 2 to 4 s for proposal 2.
2. **Slower again, by 1 to 3 s,** because the bench's coarse tube stops less tightly than the simulation's (a spread of 0.12 to 0.5 gr against 0.05 to 0.13 gr): its margin will be nearer 1 to 2 gr, and the fine tube carries that at the 0.5 rev/s the trial holds it to.
3. **Proposal 1's good share nearer the recordings' own ceiling** than the forecast's middle: 82 to 87% with BM2, 74 to 78% with AR2209.
4. **Proposal 2's first rests are the open question,** and I do not know which way: tighter than forecast if the simulated tube's long gaps are its own fault (the bench's gaps were shorter), wider if the A&D's showing of a two-digit release a digit at a time catches a stop triggered by the reading.

## What the trial is judged on

Each passes or fails, for the powder loaded.

| | The check | Pass |
|---|---|---|
| A | The tool and both controllers run on a real unit | The calibration completes; every throw ends by its own verdict; no limit trips |
| B | Proposal 1 within 0.02 gr | Good in at least the forecast's low end: 80% (BM2), 60% (AR2209) |
| C | Proposal 2 within 0.02 gr | Good in at least 76% (BM2), 52% (AR2209), and over in no more than 24%, 48% |
| D | The stream's stop itself | Proposal 2's first rest within a digit of its aim (a digit under the target) in at least 20 throws of 25. The forecast has 76% (56 to 88%) with BM2 and 52% (44 to 60%) with AR2209, so passing it with AR2209 means the bench is tighter than the simulation |
| E | Within 0.04 gr | Good in at least 96% and 92% (BM2), 84% and 84% (AR2209) |

**Time is set beside the forecast and not judged:** the forecast's scale is known to be the quicker (points 1 and 2 above).

**What each outcome would mean:**
- **B passes, C or D fails:** proposal 1 stands as ranked; proposal 2 stays an experiment or is dropped.
- **B, C and D all pass:** the stream's stop is as tight on a real scale as the bench's stops by the clock suggested. Proposal 2 is then the quicker at no loss, and the ranking is the owner's to turn.
- **B fails:** the simulation is kinder to small moves than the bench by more than I allowed; read the throws' moves and waits before anything is built.
- **A fails:** a fault of the tool or the controller, mended at the desk and the trial run again.

## Not in this trial

- Today's loop on the same day: `record.py throws` gives it, with the unit's address and a powder profile, as on 2026-11-20. Worth a set of 12 when the powder is BM2, whose forecast (79% good, 15 s) no throw has yet checked.
- A second scale, a second unit.
- The stops this afternoon asked for (in blocks by speed; from 0.7, 1.5 and 2 rev/s): they are the recorder's, not this tool's.
