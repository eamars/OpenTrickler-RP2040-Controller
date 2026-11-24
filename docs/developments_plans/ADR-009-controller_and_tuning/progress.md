# ADR-009: progress

The earlier progress is phased out and moved to [`legacy/progress.md`](legacy/progress.md).

## 2026-11-25, 01:22: step 1 approved

Step 1, the calibration, approved by the owner as the README states it (recorded 01:22). Its gates are in the README's "What we do"; gate 1, the method, starts in the simulation.

## 2026-11-25, 01:28: gate 1, the method in the simulation, first runs

`tools/charge_sim/calibration.py`: the scale part and the speeds part, each run alone on the twin (`TwinBench`, the rig `pico2w_ar2209_night`), the cup's capacity 100 gr. The cup's part is the unit's screens and is given as numbers.

- **The scale part repeats and recovers the twin's own scale** (3 seeds, about 4 minutes and 58 gr each): the lag about 0.6 s behind a flow of 2.5 to 10 gr/s (the twin's dead time and fast filter, 0.54 s, and the powder's own fall), 0.7 to 1.9 s behind 0.03 to 0.4 gr/s (its slow filter). A small move first shows 0.2 to 1.4 s after its stop; when it settles is noisy, the twin's powder still arriving late.
- **The speeds part's rule does not hold with 4 stops a speed.** A speed judged against the part's own best (within 3 times): over 4 seeds the fine tube's least came out 0.010, 0.022, 0.048 and 0.105 rev/s, and the coarse tube's most 0.418 rev/s in three seeds of four, one speed's lucky small spread setting the bar for the rest. Against the part's median instead, every fine speed passed. The rule's measures, kept: the least by how alike the runs' registered powder is (its spread as a share, every run 0.1 turn); the most by the spread of what lands after the stop.
- **The coarse part pours about 205 gr** (2 pauses for a 100 gr cup), mostly its stops at 2 and 5 rev/s; the fine part about 7 gr.
- **The twin cannot judge the fine tube above 0.5 rev/s:** it pours in gaps there that the bench never showed (`staged.py`), so the fine ladder was cut at 0.5 rev/s in the simulation.
- **Decided by the owner (recorded 01:29):** each spread is held to a fixed bound, the A&D's own on the bench times 3, kept as settings the user can change. Gate 1's bench run measures the A&D's spreads.

## 2026-11-25, 01:57: gate 1 on the bench, the three parts from the PC

The Pico 2 W on the probe, the A&D FX-300i in grains, the hopper's powder not stated (AR2209 on 2026-11-24). `bench/calibrate_unit.py`, one part a run, each into a cup of 250 gr; the session `pico2w_calibration_gate1`, its recordings kept.

**Before the runs that count:**
- The first scale and fine runs (01:33 to 01:41, on the image of `1811ea6`) were discarded: the owner saw the fine motor jam and not turn. Their folders were deleted.
- The unit's settings were copied over the web (26 resources), and `adr009_restart`'s firmware (2.0.3-162 at `41f9189`) flashed by the probe. Compared after: every field of its 17 resources unchanged; only the old firmware's own resources are gone (its calibration records and the auto-tuner's switch).
- The fine motor's current from 240 mA to 500 mA (`settings/motors/fine`, saved; its driver gives 490). The coarse motor stays at 240 mA.

**The scale part** (01:43, 4.6 min, 62.3 gr):
- **Response to a small move** (grains landed: first shown, settled): 0.08 gr: 0.89 s, 2.0 s; 0.09 gr: 0.70 s, 2.0 s; 0.21 gr: 0.19 s, 6.5 s.
- **Lag by flow** (gr/s: s): 0.05: 0.80; 0.09: 1.21; 0.17: 0.84; 0.40: 0.97; 2.9: 0.69; 10.2: 0.59.
- No run shows a jam: the one that registered nothing was a move of 0.05 turn (on 2026-11-21 such a nudge gave nothing two times in three); every longer run poured 0.4 to 1.2 gr a turn. The unit counts the steps it feeds, not the turns made, so only a run that pours far less than its turns could show one.

**The fine tube's speeds** (01:49, 3.2 min, 17.2 gr), 4 stops a speed, 0.1 turn a run or half a second:

| Speed (rev/s) | Runs' spread, as a share | Variance over mean (gr) | After the stop, mean (gr) | Its spread (gr) |
|---|---|---|---|---|
| 0.010 | 0.67 | 0.033 | 0.00 | 0.010 |
| 0.035 | 1.18 | 0.049 | 0.04 | 0.041 |
| 0.120 | 0.54 | 0.038 | 0.13 | 0.070 |
| 0.416 | 0.35 | 0.016 | 0.13 | 0.044 |
| 1.443 | 0.04 | 0.001 | 0.61 | 0.026 |
| 5.000 | 0.04 | 0.003 | 2.13 | 0.082 |

Two runs at 0.010 and 0.035 rev/s poured almost nothing for 0.1 turn: a stall or a slow burst, which the data cannot tell apart.

**The coarse tube's speeds** (01:53, 2.0 min, 187 gr):

| Speed (rev/s) | Runs' spread, as a share | Variance over mean (gr) | After the stop, mean (gr) | Its spread (gr) |
|---|---|---|---|---|
| 0.080 | 0.53 | 0.316 | 0.73 | 0.258 |
| 0.183 | 0.17 | 0.030 | 1.09 | 0.181 |
| 0.418 | 0.06 | 0.008 | 2.05 | 0.127 |
| 0.956 | 0.04 | 0.007 | 4.92 | 0.182 |
| 2.187 | 0.03 | 0.007 | 10.78 | 0.276 |
| 5.000 | 0.02 | 0.012 | 21.34 | 0.497 |

**What the bench says of the rule:**
- **The runs register alike only at speed.** The fine tube's runs differ by 35 to 118% at 0.4 rev/s and below, and by 4% from 1.4 rev/s: kernel by kernel, then a stream. The variance over the mean, which does not grow with a run's size, splits the same way. The coarse tube registers alike from about 0.4 rev/s.
- **What lands after a fine stop grows with speed:** 0.04 gr at 0.035 rev/s, 0.61 gr at 1.4 rev/s. Held to 3 times the A&D's best, the fine tube's least speed is 1.4 rev/s, and as the profile's fine minimum, the floor of today's loop at the finish, it would put about 0.6 gr after every stop. The definition is the owner's to settle.
- **The coarse runs were too short to show what lands after their stop:** a run of half a second is shorter than the scale's lag of about 0.6 s, so all its powder showed after the stop (the two columns are equal from 0.18 rev/s). The coarse tube's most speed cannot be judged from them.
- **The cup:** 350 gr after the three parts; emptied before the next run that turns a motor.
## 2026-11-25, 02:08: the least speed, the owner's rule

- **No clumps in the bench's runs:** at 0.4 rev/s and below the reading never moved more than 3 scale steps (0.06 gr) at once. The slow runs vary because each poured about 4 kernels (0.1 turn): counting so few varies by about half, as measured. At 1.4 rev/s the reading's large steps (0.26 to 0.32 gr) are the A&D taking a held change whole.
- **Two tries that did not hold, undone:** a clump-rejecting spread by the median absolute deviation is often 0 for 4 runs on a scale of 0.02 gr steps; runs of equal powder (0.5 gr) lasting twice the lag took the fine part to 10.7 minutes and the coarse to 746 gr in the simulation.
- **Decided by the owner (recorded 02:08):** the least speed is a confident value, the measured minimum doubled. Written into `calibration.py`'s `judge`, which judges a ladder from runs live or kept: the least, twice the slowest speed at which every run registers at least half its speed's median; the most, the fastest from the least up whose landing's spread is within its bound; at each speed the run farthest from its median rejected as a clump or a lull.
- **On the bench's kept runs:** the fine tube's least 0.069 rev/s (0.010 rev/s fails: with 0.14 gr rejected, a run of 0.02 gr is under half the median 0.07; 0.035 rev/s registers), its most 5 rev/s; the coarse tube's least 0.16 rev/s (its motor's least, 0.08, registers), its most 5 rev/s. The owner's fine least tuned by hand on the Pico W is 0.08 rev/s.
- **Open:** the landing's bound in numbers. The fine landings from the least up spread 0.012 to 0.064 gr (median 0.037, so 3 times is 0.11 gr). The coarse runs of half a second were shorter than the scale's lag, so all their powder landed after the stop: the coarse landing's spread is not measured, and the coarse most needs runs that outlast the lag.
