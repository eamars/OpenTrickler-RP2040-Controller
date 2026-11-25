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

## 2026-11-25, 02:35: the night, the fine tube alone

With the owner away and the cup not emptied: the fine tube's speeds part twice more (02:12 and 02:16, 3.2 min and about 17 gr each) and the landing's spread at three slow speeds, 10 stops each (02:19, 8.7 gr). No coarse run. The cup: counted 418 of 600 gr. Its reading fell from 350 to 0 at 02:11 with no word that it was emptied, so the tool carries the drop. Kept in the recordings, session `pico2w_fine_night`.

**The speeds part, three runs alike, by `judge`:**

| Run | Least (rev/s) | Most (rev/s) | Speeds that failed to register |
|---|---|---|---|
| 01:48 | 0.069 | 5 | 0.010 |
| 02:12 | 0.020 | 1.443 | 0.035 |
| 02:16 | 0.020 | 5 | 0.120 |

**The least's test is a count of kernels, not of the tube.** Below 0.2 rev/s each run is 0.1 turn, about 0.08 gr, about 4 kernels. A run of 4 kernels falls under half its speed's median by chance alone, about one speed in four, so a speed fails at random: 0.010 in one run, 0.035 in the next, 0.120 in the third, while a slower one passes. By each speed's sum (its 4 runs, about 16 kernels) the tube pours at its usual rate at every speed of the ladder, in every run:

| Speed (rev/s) | 01:48 (gr a turn) | 02:12 | 02:16 |
|---|---|---|---|
| 0.010 | 0.75 | 0.95 | 0.65 |
| 0.035 | 0.35 | 0.84 | 0.69 |
| 0.120 | 1.31 | 0.70 | 1.04 |
| 0.416 | 0.60 | 0.86 | 0.86 |
| 1.443 | 0.84 | 0.82 | 0.80 |
| 5.000 | 0.87 | 0.87 | 0.82 |

- **Proposed, not changed:** a speed registers when its runs' sum is at least half of what the tube pours a turn at its fastest speeds. On the three runs the slowest speed that registers is 0.010 rev/s, the ladder's floor, each time, so the least is 0.020 rev/s each time. That is below the owner's 0.08 tuned by hand on the Pico W: the fine tube pours at the motor's least speed, so the ladder cannot find a floor above it.

**The landing's spread, 10 stops a speed** (`stop_spread.py`; what lands after the stop, its spread over all 10 stops):

| Speed (rev/s) | After the stop, mean (gr) | Its spread (gr) | Its most (gr) | Gr a turn |
|---|---|---|---|---|
| 0.035 | 0.050 | 0.051 | 0.14 | 0.80 |
| 0.069 | 0.032 | 0.026 | 0.08 | 0.72 |
| 0.120 | 0.120 | 0.076 | 0.24 | 0.84 |

- **Four stops with the farthest left out read low:** at 0.035 rev/s the three runs gave 0.023, 0 and 0 gr against 0.051 over 10 stops; at 0.120, 0.031 to 0.064 against 0.076. With the bound at 0.11 gr every slow speed passes, and the most rests on the 5 rev/s stops, whose spread over 4 stops was 0.053, 0.145 and 0.035 gr in the three runs: 5 rev/s twice, 1.443 once.

**The screens mocked for the owner:** the 12864's 21 screens on one sheet (`mockups/calibration_12864.png`); the web's Settings › Calibration on the stand-in (`tests/web_test.py`, the API provisional in `tests/calibration_mock.py`), driven through each part and a failure at 390, 768 and 1280 px and in the dark.

## 2026-11-25, 02:48: the least by each speed's sum

- **Decided by the owner (recorded 02:48):** the proposal of 02:35 taken. `calibration.py`'s `judge`: a speed registers when its runs together pour at least half the tube's grains a turn at its fastest speed, the run farthest from their median left out as before. Each stop's row now ends with its grains a turn.
- **On the kept runs:** the fine tube's least 0.020 rev/s in each of the three runs, every speed of its ladder registering; the coarse tube's 0.16 rev/s as before. On the twin, 4 seeds: the fine 0.020 and the coarse 0.16 in each. The most is unchanged: the fine tube's 5 or 1.443 rev/s, run to run, on the bench and the twin alike.

## 2026-11-25, 13:49: the most speed is the motor's

- **The owner's direction (recorded 13:49):** the most speed lies between the least and the motor's own most (its driver's maximum speed); it matters less, being the ceiling the charge's controller is held to; a sensible number, or the motor's most when not confident. Not confident: over 4 stops a speed the fine tube's landing gave 5 rev/s twice and 1.443 once in three runs alike. `judge` now takes the fastest speed of the ladder the tube poured at, which is the motor's most unless the tube poured nothing there.
- **On the kept runs and the twin (2 seeds):** each tube's most 5 rev/s every time; the least as before (fine 0.020, coarse 0.16). Each speed's landing (its median and spread) is still recorded with the stops.
- **The landing's bound** judged the most alone. Proposed dropped, with its charge setting (README, To settle, 2).

## 2026-11-25, 13:56: the most speed not measured, the landing's bound dropped

- **Decided by the owner (recorded 13:56):** the landing's bound is dropped, with its charge setting. The most speed is not measured: each powder profile's maximum speed stays as the user has it, and a profile whose maximum is not set uses the motor's maximum speed. Item 6 is gone from the README's table; the calibration records items 1 to 5.
- `calibration.py` records each tube's least speed alone; each speed's landing is still kept with the stops, as data. The charge today holds each tube to the smaller of the profile's maximum and the motor's, and has no maximum that is not set: a maximum of 0 holds the tube at its minimum. Proposed for gate 2: not set is 0.
- **The mocks:** the 12864's sheet without the Charge settings' Landing spread (20 screens) and with each tube's minimum in a powder's result; the web's the same.

## 2026-11-25, 14:09: the method evaluated, what serves nothing removed

The owner's rule (recorded 14:09): a stage of the calibration that serves no purpose is removed. Each stage was read against its use, and the bench's kept runs replayed (`calibration.py`'s `judge_kept` and `scale_kept`).

**Removed:**
- **The speeds ladder above the prime:** with the most not measured, its fast end only gave the rate the least is judged against, which the prime already measures where the tube streams. The ladder now runs from the motor's least up to the prime's speed (the middle of the motor's limits in ratio), and the least is judged against the prime's rate. On the kept runs the least is unchanged (the fine tube 0.020 rev/s in each of three, the coarse 0.16). On the twin, 3 seeds: the fine part 6.6 gr in 3.7 min (16 gr in 3.1 before), the coarse 53 gr in 2.0 min (205 gr), the least unchanged.
- **What lands after each stop, its median and spread:** it judged the most alone.
- **Code with no caller:** a table reader, a default lag no part used, and the farthest-run removal for a list.

**Fixed: the settle time was the scale's flicker.** The A&D flickers by one step (0.02 gr) at rest, seconds after it settled, and a settle taken as the last reading that differed from the rest was that flicker's time: 2.0, 2.0 and 6.5 s for the three move sizes on the bench, so every stop of the speeds part was watched for 7.5 s. Taken as the last reading more than one step from the rest (the step read from the readings themselves): 1.1, 0.9 and 0.8 s. The scale part on the twin now takes 3.4 min (4.3 to 4.9). The lag hardly moves (at 0.09 gr/s 1.18 s against 1.21): its jumps at slow flows (0.79, 1.18, 0.82 s) are not the flicker.

**Is the calibration's development done? No.** Of step 1's five gates, the first (the method) is nearly done; the second (the records stored) and the third (the job on the unit) have not started; the fourth (the screens and the web) is mocked and waits for the owner's review before it is built; the fifth (the sign-off) comes last. Gate 1 still needs:
- **The bench again, once the cup is emptied** (418 of 600 gr counted): the scale part and the coarse part ran once each, so their spread is not known, and gate 3 checks the unit's job against it. Two more runs of each, about 230 gr with the coarse part as it is now, which also runs the changed method on the bench for the first time. The lag at slow flows is judged from those runs.
- **The cup's part** has nothing to measure from the PC: it is the unit's screens (gates 3 and 4).

## 2026-11-25, 14:14: a profile's maximum left as it is

- **Decided by the owner (recorded 14:14):** nothing is built for a profile's maximum speed. The calibration leaves it as the user has it, and the charge already holds each tube to the smaller of it and the motor's maximum.

## 2026-11-25, 14:20: the scale's settle time

- **Decided by the owner (recorded 14:20):** item 3 is the scale's settle time, not its response time, and kept apart from the lag. The lag (item 4) is how far the reading runs behind the powder poured, by flow; the settle time is how long a small change takes from the reading's first move to its rest. What the response recorded before, the first move after a stop, was the lag measured again.
- **Its measure:** the small moves are of one size, 0.1 turn (4 of them); their sizes of 0.05 and 0.25 turn are gone. A change has come to rest when the reading first shows its final value and never again strays more than one step from it: a flicker leaves the rest and comes back, a change's last step arrives and stays. Taken to the last reading more than one step away, as at 14:09, it cut a change's own last step (0.07 s on the bench).
- **On the bench's kept scale run:** the settle time 0.27 s (its four moves 0.06, 0.33, 0.22 and 0.47 s); the lag as recorded, 0.59 to 1.21 s by flow. A speeds part on these records watches each stop 2.5 s: the slowest lag, the settle time and a second. The least is unchanged on the kept runs.
- **The twin settles slower than the A&D:** 0.87 and 0.91 s over 2 seeds, against the bench's 0.27 s. The bench is the reference; the twin's scale is not changed for it.
- **The mocks:** a scale's result reads Settle time, Lag, slow flow and Lag, fast flow, on the 12864 and the web.
