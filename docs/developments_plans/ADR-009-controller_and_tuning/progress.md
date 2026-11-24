# ADR-009: progress

The earlier progress is phased out and moved to [`legacy/progress.md`](legacy/progress.md).

## 2026-11-25, 01:22: step 1 approved

Step 1, the calibration, approved by the owner as the README states it (01:20). Its gates are in the README's "What we do"; gate 1, the method, starts in the simulation.

## 2026-11-25, 01:28: gate 1, the method in the simulation, first runs

`tools/charge_sim/calibration.py`: the scale part and the speeds part, each run alone on the twin (`TwinBench`, the rig `pico2w_ar2209_night`), the cup's capacity 100 gr. The cup's part is the unit's screens and is given as numbers.

- **The scale part repeats and recovers the twin's own scale** (3 seeds, about 4 minutes and 58 gr each): the lag about 0.6 s behind a flow of 2.5 to 10 gr/s (the twin's dead time and fast filter, 0.54 s, and the powder's own fall), 0.7 to 1.9 s behind 0.03 to 0.4 gr/s (its slow filter). A small move first shows 0.2 to 1.4 s after its stop; when it settles is noisy, the twin's powder still arriving late.
- **The speeds part's rule does not hold with 4 stops a speed.** A speed judged against the part's own best (within 3 times): over 4 seeds the fine tube's least came out 0.010, 0.022, 0.048 and 0.105 rev/s, and the coarse tube's most 0.418 rev/s in three seeds of four, one speed's lucky small spread setting the bar for the rest. Against the part's median instead, every fine speed passed. The rule's measures, kept: the least by how alike the runs' registered powder is (its spread as a share, every run 0.1 turn); the most by the spread of what lands after the stop.
- **The coarse part pours about 205 gr** (2 pauses for a 100 gr cup), mostly its stops at 2 and 5 rev/s; the fine part about 7 gr.
- **The twin cannot judge the fine tube above 0.5 rev/s:** it pours in gaps there that the bench never showed (`staged.py`), so the fine ladder was cut at 0.5 rev/s in the simulation.
- **Decided by the owner (01:33):** each spread is held to a fixed bound, the A&D's own on the bench times 3, kept as settings the user can change. Gate 1's bench run measures the A&D's spreads.
