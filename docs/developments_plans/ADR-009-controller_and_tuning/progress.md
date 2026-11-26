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

## 2026-11-25, 14:29: where each item is used

- The README maps each item to where it is used now and to be (its "Where each item is used"), checked in the code.
- **The owner (recorded 14:29):** the empty cup's weight (item 2) is the safety of the later charge controller, step 2's: a cup lifted mid-charge told from a light reading. Today's charge keeps its own check, a steady reading 10 gr below zero.

## 2026-11-25, 14:35: gate 1 from the kept recordings, no bench

The owner away and no bench run: gate 1 continued from what the bench recorded, yesterday's runs and the six sessions before them (the same Pico 2 W, tubes, AR2209 and A&D, 2026-11-19 to 25). `bench/records_spread.py` reads them all and gives by session what each part would record.

**The shorter watch holds.** Each kept speeds run cut to the 2.5 s the method now watches (they watched 4 s): the least is unchanged in all four runs. Of 36 slow stops, one came to rest after 2.5 s (3.6 s, one kernel). One speed's rate halved, 0.92 to 0.46 gr a turn at 0.035 rev/s: not powder lost, but the run left out as a clump or a lull changing with one step of flicker at the end of a watch. On runs of about 4 kernels that rule swings a speed's rate by half; the least never moved with it, as the motor's minimum always pours.

**The lag, session to session** (each session's mean at a speed):

| Flow | Sessions | Lag |
|---|---|---|
| coarse 1 rev/s, about 10 gr/s | 5 | 0.55 to 0.59 s |
| coarse 0.08 to 0.3 rev/s, 0.9 to 3 gr/s | 1 to 3 | 0.58 to 0.82 s |
| fine 1 rev/s, 0.8 gr/s | 4 | 0.63 to 0.83 s |
| fine 0.5 rev/s, 0.4 gr/s | 4 | 0.80 to 0.93 s |
| fine 0.2 rev/s, 0.15 gr/s | 4 | 0.89 to 1.06 s |
| fine 0.1 rev/s, 0.08 gr/s | 6 | 0.72 to 1.27 s |
| fine 0.05 rev/s, 0.04 gr/s | 5 | 0.56 to 1.54 s |

- **Changed:** the lag's runs are five flows: fine 0.25, 0.5 and 1 rev/s, coarse 0.25 and 1 rev/s. The fine tube's 0.05 and 0.1 rev/s are gone: two runs there are noise, and the powder they would say is unseen is under two steps. Fine 1 rev/s is added: the lag falls from about 0.9 s at 0.4 gr/s to 0.6 s by 1 gr/s, and without it the table read at 0.8 gr/s would be about 0.2 s slow, about 0.18 gr of powder still to show. On the twin the scale part takes 1.8 min (2.1); its fine 1 rev/s point is past where the twin holds (0.5 rev/s).

**The settle time** follows the small move's own length: moves run 0.1 s rest 0.03 s after their first move (111 moves), moves run 0.3 to 0.9 s 0.22 to 0.53 s. It is the move's powder arriving over its run, and the A&D's own settling is too short to see beside it: the 0.27 s of 2026-11-25 is the move. The measure is the owner's to settle (README, To settle, 2).

**The least** holds in all seven sessions: both tubes pour at the slowest speed tried at 0.61 to 1.35 of their stream's rate, so on this unit and powder the least is twice the motor's minimum.

**Gate 1, where it stands:** the records' form is proposed for gate 2 (README, What we do, 2). The repeat bench runs are no longer needed for the spread, which the kept sessions give. Left: the settle time's measure (the owner's), and the method as changed today run once on the bench (the ladder below the prime, the shorter watch, the lag's five flows), at the next bench session.

## 2026-11-25, 14:54: gate 2, the records stored, built (no unit)

The owner (recorded 14:54): gates 2, 3 and 4 in their order; the firmware built and the method run in the simulation, never trickled or tested on a unit. So gate 2 is built and checked on the PC, and its proof on the units waits for the bench.

**The stored structs, as built** (`sizeof` from both boards' images, the same on each):

| Struct | Before | After | Appended |
|---|---|---|---|
| `eeprom_charge_mode_data_t` | 64 bytes | 72 | `cup_capacity`, `cup_weight` at 64 and 68 |
| `eeprom_scale_data_t` | 6 bytes | 52 | `settle_ms` at 8, then `lag[5]` of `flow_per_s` and `lag_ms` |

Each appended field is -1 when never calibrated, and null through the API (`nullable`, as the gate's ratio): `cup_capacity` and `cup_weight` in the scale's unit; `settle_ms`; `lag_1_flow_per_s` to `lag_5_flow_per_s` (a weight a second) and `lag_1_ms` to `lag_5_ms`. Named as the API names a quantity with a unit (api_v1.md, its settings tables grown to them). Each is in its module's table, group Calibration, so the API, the console's `setting`, the export, the backup and the web's forms carry it; the 12864's menu lists its rows itself and shows none of them. The scale's old record ends at 6 bytes and its new fields start at 8, past its filler.

**The loader** (`common.c`): a record whose CRC32 fails at the struct's size is tried at each shorter size, the longest first, down to past its revision; where one matches, its bytes are kept, the appended fields take their defaults, and it is stored at the new size, held on an update's trial as every write is, and not said to be a reset. A record that matches at no size gets the defaults, as before.

**Checked on the PC:** `tests/host_test.py` compiles `common.c` for the PC (`zig cc`; the runner's gcc in CI, a step added to it) over an EEPROM in RAM: an earlier version's record kept with the appended fields at their defaults and stored at the new size, read as it is at the next start, a record of the new size read whole, a damaged one and an erased chip given the defaults, and on a trial the record kept in RAM, the EEPROM untouched until the confirmation, no reset said. 11 checks, all pass. Both boards build.

**Not proven, waiting for the bench:** the gate's own proof, an update from today's release that keeps every setting, on both boards.

## 2026-11-25, 15:10: gate 3, the calibration as a job on the unit, built (no unit)

**The method in the firmware** (`calibration_method.c`): `calibration.py`'s scale part and speeds part in C, nothing of the RTOS, the motors or the scale in it; it turns the tubes and reads the scale through a bench. `tests/calibration_sim.py` compiles it for the PC and runs it two ways on the simulation, two seeds:
- on Python's own runs given back call for call: the C asks for the same runs in the same order (18 for the scale, 26 for each tube) and makes the same records, the settle time, the five lag points and both least speeds equal to four decimals;
- alone, its bench the twin's, as the job runs it on a unit: the scale part 1.8 min and 58 gr, the fine tube 3.3 min and 7 gr, the coarse 1.6 min and 53 gr, the least 0.020 and 0.16 rev/s.

**The job** (`calibration.c`, `ACTIVITY_CALIBRATION` in the job task): each part begins with the empty cup put on the scale and the scale zeroed (steady within 0.04 within 8 s, or it fails); the cup's part then waits for the cup lifted (2 below zero, steady: its weight), put back, filled with the tubes' speeds set by the person, its capacity read steady and kept on `keep` into `settings/charge`, and emptied; the scale's and a powder's run the method, pause where a run would pass the cup's capacity (the stored one, or 100), and at their result keep the records on `save` into `settings/scale` or the selected powder's profile, each through the settings' own writes and stored. A stop or a driver's fault ends it at once, the motors stopped. The API (`/api/v1/calibration`, its `start`, `answer`, `fill`, `save`, `stop`; in the index and the schema, the activity `calibration`, its steps and its events) and the console's `calibrate` drive it; api_v1.md has its section. The 12864 shows a plain job screen for it (its step, the weight or how far it is, what it does, Stop and the step's answer): gate 4 draws each step as mocked.

**The Pico W's image:** the first build overflowed its slot by 13.7 KB of flash and 3.8 KB of RAM. Not cut: `sscanf`, pulled in by the console's parsing alone, was 15 KB of the scanf family, and is gone; the method's 3 KB of readings come from the FreeRTOS heap while a part runs, given by the bench. Now 990.6 of 1016 KB of flash and 252.6 of 256 KB of RAM; the Pico 2 W 961.5 of 1792 and 285.2 of 512.

**Not proven, waiting for the bench** (nothing run on a unit, the owner's direction): the job's steps on the 12864, the API and the console; the A&D's rate against the room for readings (128 in a run, 256 in a watch); the zero's wait, the lifted and put-back thresholds; a stop and a driver's fault in each step; the records kept as the method found them, against gate 1's within the spread the kept sessions give.

## 2026-11-25, 15:20: gate 4, the screens and the web, built (no unit)

**The 12864** (`menu_nav.c`): Settings › Calibration first in Settings, its rows Cup (the capacity kept, or Not done), Scale (Done or Not done) and Powder (the charge's powder), each starting its part; Calibrate this unit on the main menu while the cup or the scale was never calibrated, as Set up this unit is offered. In the job, each step its screen as mocked: the cup on the scale (Cancel, and Next for the cup's part, Start for the others), Zeroing, Lift the cup and Put the cup back (the weight as the figure), Fill the cup (the purge's control: the weight and its flow, the fine and coarse tubes' speeds set in place a detent at a time, 0.1 and 0.5 rev/s; Cancel, Full), its capacity to keep, Empty the cup, Calibrating (how far, what it does; Stop), the pause (Stop, Go on), the records found as notes ending in Keep or Save, and a failure (OK). Back stops it at any step. One change from the mock: the view gives notes a label and a value on one line and clips the label, so the powder's result is headed lines, Fine tube then Minimum and Was, where the mock wrapped "Coarse minimum" over two lines.

**The web** (`settings.js`) on the unit's own API, the stand-in's model (`tests/calibration_mock.py`) answering as `calibration.c` does: each part driven through on the stand-in at a phone's width, the cup's (zeroing to empty, its capacity kept into the Cup card), the scale's (its result, Keep, its card), a powder's (the pause, the result beside what was, Save) and a failure (OK); no error in the console.

**The images:** the Pico W 995.4 of 1016 KB of flash and 252.6 of 256 KB of RAM; the Pico 2 W 966.5 of 1792 and 285.2 of 512.

**What the bench owes, gate by gate** (nothing of gates 2 to 4 has run on a unit):
- gate 1: the method as changed on 2026-11-25 run once (the ladder below the prime, the shorter watch, the lag's five flows), and the settle time's measure (README, To settle, 2);
- gate 2: an update from today's release that keeps every setting, on both boards;
- gate 3: each part on a unit through the console and the API; the A&D's rate against the room for readings; the zero's wait and the cup's thresholds; a stop and a driver's fault at each step;
- gate 4: the screens seen and reached on the unit (`snap`, `reach`), and the menu's baselines saved again for the pages this changes (main, settings, and the new settings/calibration); the web on a unit;
- gate 5: step 1's sign-off.

## 2026-11-25, 15:38: the settle time from bursts

- **Decided by the owner (recorded 15:38):** the settle time is measured with bursts: the fine tube at 2 rev/s for 0.1 s, four times, about 0.2 turn and 0.16 gr each (8 of the A&D's steps), its powder turned out within about 0.14 s at the motor's 50 rev/s² (0.04 s to 2 rev/s). What follows a burst's first move is then the scale's settling, not the powder still arriving. In `calibration.py` and `calibration_method.c` alike; the screen's line while they run reads Bursts.
- **On the simulation:** the C and the Python agree on the bursts' runs and the settle time (0.781 and 0.143 s on two seeds). The twin's figure itself means little: its fine tube holds only to 0.5 rev/s. Kept recordings have no bursts, so `scale_kept` gives no settle time for them. The A&D's own is measured at the next bench session (gate 1's run of the method as changed).
- Both boards build: the Pico W 995.4 of 1016 KB of flash.

## 2026-11-25, 15:55: a powder calibrated from its own page; the bench tools know the calibration

- **The bench tools** (owner's go): `rc_run.py`'s cup check had no most for the console's `calibrate`, so a send of it ran unchecked, against the bench rule that a new command turning a motor gets its most in the same commit (missed in gate 3). Now `calibrate scale` and `calibrate powder` count their method's own turns at the rule's 12 gr a turn (355 and 307 gr; `turns_most` in `calibration.py`), and `calibrate fill` its speeds until the cup is said full. `api_check` calls the calibration's six actions (a part started and read at its first step, where nothing turns; the others refused; stopped), and the schema lists the codes they give.
- **The owner (recorded 15:55):** the web's powder calibration was the charge's powder alone, AR2208, with no way to choose; perhaps a powder is calibrated from its own page in Powders. Taken: its records are that powder's own fields, and any powder can be calibrated so, the one in the hopper. `POST /api/v1/calibration/start` takes a `profile` (without it the charge's, as the 12864's Powder row starts it), `GET` gives `profile` and `profile_name`, the records are saved into that profile. The web: Settings › Calibration keeps the cup and the scale and points to Powders; each powder's page ends in Calibrate this powder, its steps there while it runs ("With AR2209 in the hopper: it is what is calibrated"), and the page read again once its records are saved. The 12864's first step of a powder reads "AR2209 in the hopper" over the cup's two lines (the mock sheet the same). On the stand-in: AR2209 (not the charge's powder) calibrated from its page, paused, its result beside what was, saved, its form then 0.02 and 0.16 rev/s, the charge's powder untouched; no error in the console. Both boards build, the Pico W at 997.0 of 1016 KB.
- **The owner (16:22):** a powder's Calibrate grouped with its Export and Import. Its card is gone: the first step already says the powder is the one in the hopper. The three are one row of the powder's actions, Calibrate off while any part runs; the steps show above the row while this powder's runs. Checked on the stand-in at a phone's width.

## 2026-11-25, 16:37: the test on a unit deferred

- **The owner (recorded 16:37):** the calibration's test with the new code on a unit is deferred to later. Its exit: the owner says the bench is ready for it. Then, in order: the list under "What the bench owes" (gate 4's entry, 15:20), with the burst's settle time measured in gate 1's run, the bench tools' calibration covered by `rc_run.py` (its cup check and `api_check`), and step 1's sign-off by gate 5. Nothing of gates 2 to 4 has run on a unit until then.
- **Where the work is:** branch `adr009_restart`, local and not pushed (the owner's rule: pushed when its PR is ready). Both boards build from it; `tests/host_test.py` and `tests/calibration_sim.py` pass on the PC.

## 2026-11-25, 17:01: step 2's design drafted

- **The owner (recorded 17:01):** start step 2's design while step 1's test on a unit is deferred. Taken as the design alone: the README keeps that step 2 is built only once step 1 is signed off.
- The first build's record was read for it (its legacy README and progress, branch `adr009_controller`'s progress and research, the simulation's tools): what it learnt of the forecast, the gains and learning throw by throw, and its traps, are in the draft's "What we know". The draft is in the README's step 2, with four questions for the owner before approval: where the grains a turn lives (step 1 keeps none, and the forecast needs it each throw), the gains while the tuner is on, I and D, and room on the Pico W.

## 2026-11-25, 17:40: step 2's draft to the owner's answers

- **The owner (recorded 17:40):** the tuner is `new_pid = tuner(old_pid, charge_data)`, stored back into the powder's profile, no storage of its own; applied by itself when enabled; gains changed per window of throws; off, the profile left as it is; kd tuned; the same mechanism in grams and grains; the charge controller a module with a clear interface, so another design can replace it; the Pico W's room not step 2's concern.
- **The draft rewritten:** the extra storage proposed before was the predictor's (each powder's weight a turn, and a learnt landing), not the tuner's. Dropped: the weight a turn is estimated within each throw from its own readings, and the landing needs no store (the first build's forecast lay 0.003 gr from the rest at the stop). The tuner changes gains by factors and judges throws against the target and the tolerance, so nothing stored changes meaning and grams and grains are alike. The controller's interface, the envelope (anti-windup and the fine's cap, as part of the PID) and ki left as the user has it are in the draft.

## 2026-11-25, 17:45: step 2 unapproved until proven in the simulation

- **The owner (recorded 17:45):** step 2's draft stays unapproved until its idea is proven in the simulation. Its exit is gate 1's two checks, written into the README: the forecast on an estimate within each throw against the first build's on its stored weight a turn (mean within a digit, spread no more than a tenth wider), and the tuner on the twin against today's loop (narrower and no slower from the owner's gains; the same spread from gains far off within a few windows).

## 2026-11-25, 17:58: gate 1's forecast check, failed on a weight a turn from the throw

**The tool,** `tools/charge_sim/bench/forecast_in_throw.py`: every kept bench series with the calibration the unit echoed (61 series, 756 throws; 8 with no echo left out). Each throw's rows are replayed with the first build's forecast (10 ms parcels of the speeds asked, each shown after the lag of its speed, the unit's own lag table), which it makes again within 0.035 gr rms (median). Only the weight a turn differs between the ways:
- **stored:** the calibration's, as the unit held it (the first build's forecast);
- **slope:** none: the reading plus its rise over the last 0.25 s times the lag (the draft's fallback "until it is known");
- **in-throw:** from the throw's own readings alone (the coarse tube's from the reading over its turns shown, the fine tube's from the rise once the coarse tube is quiet; the slope until each is known);
- **carried:** the mean of the series' earlier throws' own weights a turn, each in hindsight: what RAM could carry from throw to throw, nothing stored. The first throw of a series has none.

| Way | Fine stop, coarse quiet (746) | From the creep (554) | Coarse stop: forecast less truth (563) |
|---|---|---|---|
| stored | +0.001, sd 0.063 | +0.014, sd 0.048 | +0.018, sd 0.554 |
| slope | −0.250, sd 0.422 | −0.149, sd 0.264 | +1.245, sd 1.649 |
| in-throw | −0.017, sd 0.157 | +0.012, sd 0.074 | −0.763, sd 1.136 |
| carried (its own throws) | +0.013, sd 0.057 (513) | +0.019, sd 0.044 (361) | −0.070, sd 0.622 (512) |
| stored, on the carried's throws | +0.002, sd 0.058 | +0.013, sd 0.044 | +0.017, sd 0.547 |

(the rest less the forecast at the fine stop; the forecast less the truth at the coarse stop, the truth being the settled weight less what the fine tube still turned out, at its own weight a turn in hindsight; gr)

**Verdict: the forecast on a weight a turn from the throw fails** the bar written for it (mean within 0.02 gr, spread no more than a tenth wider than the stored one's): 2.5 times as wide at the fine stop, half as wide again from the creep, and 0.76 gr low at the coarse stop. The cause is in the throw's shape, not the estimate's method:
- **both tubes turn together until the coarse stop,** so the readings cannot tell the two weights a turn apart; the fine tube's powder still unseen at the coarse stop (about 0.85 turns at 1 rev/s, 0.7 gr) has no weight to be counted at;
- **in a short throw the fine tube stops as the coarse tube goes quiet,** before any stretch of the fine tube alone: 12 stops had no weight a turn of their own and fell back to the slope, about 1 gr out each;
- **the slope is no fallback:** a quarter second of rounded, lumpy readings times a lag of 0.85 s is noise (sd 0.26 to 0.42 gr at the stop);
- **the coarse tube's from its own first second is worse than the stored one** even with the fine tube's weight given (sd 0.90 against 0.55 at the coarse stop): its first turns and the lag weigh on it more than on the forecast itself.

**What holds:** a weight a turn known before the throw. The stored one is the best; one carried in RAM from the throws since start is as good at the fine stop (0.057 against 0.058 on the same throws) and 14% wider and 0.09 gr low at the coarse stop, and has nothing for the first throw after each start or of each powder. Step 1's powder part already measures each tube's weight a turn where it streams (`stream_per_turn`, its prime's) and keeps only the least speed.

**The question for the owner:** where the forecast's weight a turn comes from. Gate 1's controller and tuner wait for the answer.

## 2026-11-25, 18:05: the forecast for the fine tube alone, passed

- **The owner (recorded 18:05), asked where the weight a turn comes from:** "Predictor should only matter for last grain/gram of throw, so predictor and your simulation should only run with fine trickler, below coarse trickler stop threshold." Read as: the coarse tube is today's (its PID on the reading, stopped at the target less its threshold, as `charge_mode.cpp` does on main), and the predictor serves the fine tube alone below that threshold.
- **The check re-run in that scope,** `forecast_in_throw.py` rewritten for it (the 17:58 version is in commit 23d85a3): the fine tube's stops with the coarse tube quiet and at least half a turn of the fine tube shown alone since (710 of the 746; the rest are the first build's stops right after the coarse tube's quiet, which a finish below a threshold does not make). The fine turns unseen at the stop: median 0.064, nine in ten under 0.27.

| Fine tube's weight a turn | All stops (710) | From the creep (541) |
|---|---|---|
| stored (the first build's) | +0.002, sd 0.063 | +0.014, sd 0.047 |
| stored, 15% under | +0.014, sd 0.057 | +0.022, sd 0.046 |
| stored, 15% over | −0.009, sd 0.072 | +0.007, sd 0.051 |
| none (the reading alone) | +0.078, sd 0.082 | +0.065, sd 0.069 |
| **the throw's own: the reading's slope on the fine turns shown since the quiet** | **+0.013, sd 0.064** | +0.019, sd 0.052 |
| the throw's own, from the stretch's two ends | +0.005, sd 0.070 | +0.016, sd 0.052 |

(the rest less the forecast, gr; with a whole turn alone, 603 stops, the same within 0.003)

- **Verdict: pass.** On the throw's own weight a turn the rest lies +0.013 gr from the forecast (within a digit), its spread 2% wider than on the stored figure (the bar: a tenth). From the creep it is 11% wider, as wide as a stored figure 15% over; the calibration's own figure was 11 to 29% over the throws' at the finish in the first build (0.797 to 0.814 against 0.63 to 0.715), so a stored figure from step 1's prime would do no better there.
- **What it shows of the predictor:** the forecast is needed (the reading alone is 0.08 gr late and a third wider), but its weight a turn only roughly: 15% either way moves the spread by a tenth. The weight a turn depends on the speed (more a turn in the stream than at the creep), which a window of the latest turns alone did not follow better: it was noisier (sd 0.10 to 0.22 on the last 0.5 to 0.2 turns).
- **Open, for the twin:** the fine tube's forecast before the throw's own weight a turn is known, from the coarse tube's stop to a few tenths of a turn after its powder's quiet. Today's threshold (5 gr by default) puts the last gram seconds after it; a small threshold does not, and the twin is run with thresholds down to what a user may set.
- **Next, gate 1's controller:** the fine tube's PID on this forecast with its envelope, below the coarse tube's threshold, on the twin, then the tuner against today's loop.

## 2026-11-25, 18:21: gate 1's controller on the twin

**The tool,** `tools/charge_sim/step2.py`: step 2's charge against today's loop (`legacy_pid.py`), the owner's Pico W AR2209 profile for both (fine kp 0.8, kd 15, 0.08 to 2 rev/s; coarse kp 0.05, kd 0.3; the coarse threshold 5 gr, judged at 0.03 gr), 10 gr, 300 throws a case in sessions of 25, on four rigs. Step 1's records are made by `calibration.py`'s parts on each rig, once. Step 2:
- the coarse tube today's (its PID on the reading, stopped at the target less its threshold, its back-off);
- the fine tube's PID on its forecast (the profile's gains, step 1's least speed, the profile's most), within its envelope (the integral held at a limit; the speed held under its least plus the room over the weight a turn and the lag), stopped the moment the forecast reaches the target, no top-up;
- the forecast's weight a turn and its lags solved together (the lag table is by flow, and the flow is the speed times the weight a turn: a lag looked up before the weight was known, at no flow, halved the estimate).

| Rig | Today's loop | Step 2, the throw's own weight a turn only | Step 2, carried (RAM) | Step 2, carried, fine kp 1.2 |
|---|---|---|---|---|
| night (AR2209) | sd 0.153, +0.133, 9% good, 6.0 s | 0.182, −0.004, 28%, 5.1 s | **0.064, +0.012, 59%, 9.1 s** | 0.172, +0.090, 32%, 6.0 s |
| f2 (AR2209) | 0.074, +0.083, 13%, 6.4 s | 0.096, −0.012, 55%, 8.6 s | **0.043, +0.004, 64%, 9.9 s** | 0.063, 0.000, 51%, 7.2 s |
| BM2 | 0.092, +0.035, 68%, 9.3 s | 0.028, +0.017, 78%, 17.5 s | **0.024, +0.017, 74%, 17.7 s** | 0.032, +0.008, 83%, 12.6 s |
| night, fine by speed | 0.116, +0.112, 10%, 5.6 s | 0.175, +0.048, 30%, 4.9 s | **0.083, +0.018, 54%, 9.2 s** | 0.160, +0.109, 22%, 4.8 s |

(the final weight less the target: its sd and mean in gr, good within 0.03 gr, the median time to the stop)

**What it shows:**
- **At the owner's gains, step 2 is narrower than today's loop on every rig** (by 30 to 74%) and lands centred (+0.004 to +0.018 gr, where today's lands 0.035 to 0.133 over), **and it is slower,** by 50 to 90%: on the forecast the fine tube's PID closes the last grains exponentially (at kp 0.8 and 0.78 gr a turn, about 1.6 s an e-fold), where today's, on a late reading, runs on and overshoots.
- **The weight a turn must be carried from throw to throw** (the median of the last five throws' own, at their stops; RAM, nothing stored). The throw's own alone fails on the twin: the fine tube alone runs only half a turn to a turn after the coarse tube's powder is quiet, and the twin's fine tube pours unevenly (30% by its angle in a turn, clumps: one stalled 0.75 s and dropped 1 gr, which a fit over 0.54 turns read as 2.4 gr a turn). The first throw after a start has nothing carried and throws on the reading alone, as today.
- **Faster costs spread, and not by the estimate alone.** Given the rig's true weight a turn (an oracle, for the study only), the night rig gives 0.057 gr at kp 0.8 in 10.0 s, 0.116 at kp 1.2 in 7.5 s, 0.154 at kp 1.6 in 6.4 s: at today's time the spread is today's, only centred. f2 the same (0.062, 0.068, 0.072 at 10.4, 7.2, 6.1 s). The faster the fine tube arrives, the more powder is in the air at its stop, and its clumps and lateness widen the landing.
- **So gate 1's bar, "narrower and no slower" from the owner's gains, is not met on the twin by the controller alone.** By the owner's ranking (the spread first, then the time) the owner's gains already give the narrower throw; the tuner is what moves along the trade.

### 18:25: where the time goes: the coarse tube's threshold

Step 2's fine tube pours the last threshold's worth alone, so a smaller coarse threshold would be quicker. `step2.py --threshold`, 200 throws, the owner's gains otherwise:

| Rig | Threshold | Today's loop | Step 2, carried |
|---|---|---|---|
| night (its back-off spills 0.83 gr) | 5 gr | sd 0.153, +0.133, 6.0 s | 0.064, +0.012, 9.1 s |
| | 3 gr | 0.250, **+0.344**, 3.5 s | 0.241, **+0.349**, 3.5 s |
| | 2 gr | 0.369, **+0.822**, 3.2 s | 0.357, **+0.798**, 3.2 s |
| f2 (no spill) | 5 gr | 0.074, +0.083, 6.4 s | 0.043, +0.004, 9.9 s |
| | 3 gr | 0.080, +0.094, 5.2 s | 0.083, +0.043, 5.4 s |
| | 2 gr | 0.153, +0.160, 3.8 s | 0.108, +0.144, 4.0 s |
| BM2 (no spill) | 5 gr | 0.092, +0.035, 9.3 s | 0.024, +0.017, 17.7 s |
| | 3 gr | 0.054, +0.034, 7.7 s | 0.030, +0.015, 15.4 s |
| | 2 gr | 0.085, +0.060, 5.8 s | 0.088, +0.052, 6.0 s |

Below 3 gr the two loops land alike and over: **the coarse tube's own powder overruns the threshold.** It stops on a late reading, so what it turned out in the last lag (and on the night rig its back-off's spill) lands after its stop, past where the fine tube could still help; the fine tube's forecast counts none of it. The threshold has to hold all of that, and the fine tube alone then pours it: that is where step 2's time goes. A forecast at the coarse tube's stop, on its weight a turn carried from throw to throw as the fine tube's is, would let the threshold shrink (the bench's replay at the coarse stop, carried: −0.070 gr, sd 0.622, against the stored figure's +0.017, sd 0.547). Not built: the owner put the predictor on the fine tube alone, so this is the owner's to decide.

## 2026-11-25, 18:51: gate 1's tuner on the twin, failed

**The tool,** `tools/charge_sim/step2_tuner.py`: `new_pid = tuner(old_pid, charge_data)` a window of 20 throws at a time, by the owner's ranking. It holds in RAM the gains it last kept and every window thrown on them; it tries one move on the fine tube's gains (kp up or down by 1.25, kd up or down by 2) and keeps it when the window's spread (the half-width of its middle 68%) is narrower than the kept gains' by a quarter, or within a quarter and its median time 5% shorter; otherwise it puts the kept gains back. Four sessions of 400 throws a case from three starts (the owner's gains, kp a quarter of them, kp four times them), tuned and fixed, against today's loop; the last 100 throws of each:

| Rig | Today's loop | From the owner's, fixed | tuned | From kp/4, fixed | tuned | From kp x4, fixed | tuned |
|---|---|---|---|---|---|---|---|
| night | 0.129, +0.119, 6.1 s | 0.083, +0.021, 9.5 s | 0.092, 8.7 s | 0.050, 30.2 s | 0.048, 19.0 s | 0.229, +0.121, 4.3 s | 0.195, +0.131, 4.3 s |
| f2 | 0.095, +0.087, 6.3 s | 0.042, +0.006, 10.1 s | 0.075, 6.9 s | 0.036, 31.3 s | 0.034, 26.9 s | 0.077, 5.9 s | 0.111, 5.8 s |
| BM2 | 0.075, +0.041, 9.4 s | 0.044, +0.018, 17.6 s | 0.047, 13.4 s | 0.041, 51.4 s | 0.034, 46.6 s | 0.100, 7.7 s | 0.102, 7.6 s |
| night, fine by speed | 0.127, +0.117, 5.6 s | 0.103, +0.018, 9.4 s | 0.055, 11.1 s | 0.042, 32.5 s | 0.053, 24.2 s | 0.231, +0.262, 4.2 s | 0.232, +0.272, 4.2 s |

(the final weight's sd and mean less the target, gr, and the median time; a tuned case's mean is its fixed case's within 0.03 gr)

**Verdict: the tuner fails** gate 1's bar. From the owner's gains it ends where it began, its gains scattered (kp 0.64 to 1.95, kd 1.9 to 60 across sessions). From gains far off it does not come back within 20 windows: from kp/4 the throws stay at 19 to 47 s, from kp x4 the night rigs stay 0.2 gr wide and over.

**Why, each checked in the windows:**
- **A window of 20 throws is too noisy.** On fixed gains its spread runs 0.025 to 0.14 gr (a first throw with nothing carried, a clump); a quarter's margin is inside that. On the night rig the first window at kp 0.8 came out 0.139 and kp 1.0 was kept as "narrower" on it; kp 0.8 at 0.025 was then put back.
- **A slower throw is always a little narrower.** kp/4 is narrower than the owner's on every rig (0.036 to 0.050 against 0.042 to 0.103) at three times the time. "The spread first, then the time" has no point to stop at without a number: how narrow is narrow enough, or what a second is worth against the spread.
- **kd does next to nothing on the forecast.** The forecast moves smoothly with the speeds asked, so its error falls at the weight a turn times the speed: the D term only scales kp down (by 1 + kd x weight a turn / 1000 in today's units, 1.2% at kd 15). The windows saw no effect of kd from 1.9 to 60.

**What a tuner could be instead, for the owner's decision:** one number to tune rather than a search, and an objective with a stopping point.
- kp made from what the throws measure: kp = c / (weight a turn x lag), the first build's offer, so a powder's weight a turn and a scale's lag are met by the formula, and the tuner moves the one dimensionless c.
- the objective either the user's tolerance as the spread wanted (narrow enough when, say, nine throws in ten are within it; then quicker), or the time per good charge (the median time over the share within the tolerance: what a good charge costs when every bad one is thrown again), which puts a price on both.
- windows of 50 throws or more, or the trial judged against the kept gains' pooled throws by a test of its own noise, so a move is kept on evidence.

## 2026-11-25, 18:54: the tuner's objective, the owner's

- **The owner (recorded 18:54):** the tuner optimises the time per good charge (the median time over the share within the tolerance), chosen over the tolerance first and then the speed. The tuner is rebuilt on it in `step2_tuner.py`: one number moved (the fine tube's kp, by a factor that halves in its logarithm when both ways are put back), each window of 25 throws against the kept gains' pooled throws, a move kept only when its cost is lower by more than one and a half of the two windows' noise (from their shares of good throws). kd stays as the user has it: on the forecast it only scales kp (18:51), so the windows cannot see it.

## 2026-11-25, 19:52: gate 1's tuner on the owner's objective, passed

**Two mends found on the way, each a root cause:**
- **The carried weight a turn renewed only on slow throws.** Each throw's own was taken at its stop from at least half a turn of the fine tube alone; a quick throw stops before that, so the carry stayed empty and the forecast fell back to the reading (+0.5 gr over at kp 3.2 on the night rig). Now each throw's own is taken at rest, from its verdict's reading (the slowest lag and the settle time after the stop, as the draft reads its verdict), over all its fine turns since the coarse tube's quiet: the landing is centred at every kp (kp 1.6: +0.001 to +0.038 gr, where it was +0.09 to +0.11 at kp 1.2). `throw.py` takes the verdict's wait as an argument for it (today's 1 s by default).
- **The tuner put good moves back.** A window's cost is noisy in its share of good throws (25 throws) and precise in its time; with a margin of its noise, kp 0.2 to 0.3 (31.5 to 23.2 s) was put back, both ways were, and the step shrank to 1.1 for good. The cost is flat over kp 0.8 to 3.2 and steep below, so a wrong keep is cheap and a wrong put-back dear: a trial is now kept when its cost is lower, and a kept move restores the full step.

**The tuner, `step2_tuner.py`,** three sessions of 300 throws a case, the last 75 of each, seconds a good charge (the median time over the share within 0.03 gr):

| Rig | Today's loop | Step 2, from the owner's, fixed | tuned | From kp/4, fixed | tuned | From kp x4, fixed | tuned |
|---|---|---|---|---|---|---|---|
| night | 51.2 (sd 0.109, +0.108, 6.1 s) | 15.8 (0.065, 10.0 s) | **15.2** (0.065, 8.8 s) | 59.1 | **16.7** | 17.3 | **14.3** |
| f2 | 41.2 (0.085, +0.091, 6.4 s) | 15.0 (0.042, 10.2 s) | **14.2** (0.067, 7.1 s) | 45.1 | **15.0** | 15.3 | **12.5** |
| BM2 | 16.1 (0.075, +0.035, 9.4 s) | 23.6 (0.027, 18.1 s) | **15.5** (0.101, 10.3 s) | 84.3 | **16.9** | 20.2 | **17.5** |
| night, fine by speed | 49.2 (0.122, +0.117, 5.7 s) | 15.4 (0.052, 9.8 s) | **18.9** (0.053, 10.5 s) | 54.9 | **17.0** | 17.8 | **15.1** |

(the fixed cases are of the run before the second mend, the same seeds; the tuned ones landed within 0.015 gr of the target on average)

**Verdict: pass.**
- From the owner's gains a good charge costs less than today's on every rig: about a third of today's on the AR2209 rigs, the same on BM2 (15.5 against 16.1 s); kp ended between 0.65 and 2.2.
- From a quarter of the owner's kp, 10 sessions of 12 reached kp 1 or more within 300 throws (f2's first: 0.2, 0.3, 0.45, 0.68, 1.01 in four windows, the cost 46 to 10 s); one on the night rig ended at 0.55, one on f2 still at 0.22.
- From four times it, every session ended where the cost is flat (12.5 to 17.5 s), one at kp 13 on the night rig with the fine tube by speed (its case 15.1 s).
- **What it costs:** the throws are slower than today's (7 to 11 s against 6 to 9 s) and step 2's verdict waits the slowest lag and the settle time (about 1.9 s on the twin) against today's 1 s; a good charge is cheaper because three to five times as many land within the tolerance on the AR2209 rigs. The tuner's step, window and rule are those above; kd and ki stay the user's.

**The draft brought to what gate 1 built** (the README's step 2): the weight a turn carried in RAM from each throw's verdict, the tuner on kp alone by the time per good charge, the unit's proof in the same terms. Step 2 now waits for the owner's approval; it is built only once step 1 is signed off on the bench.

## 2026-11-25, 20:01: step 2 approved

- **The owner (recorded 20:01):** step 2 approved as gate 1 proved it in the simulation (the README's step 2 as of 19:52): today's coarse tube, the fine tube's PID on its forecast with its envelope, the weight a turn carried in RAM from each throw's verdict, the tuner on the fine kp alone by the time per good charge, kd and ki as the user has them. Its gates 2 to 4 are built once step 1 is signed off on the bench (the README: nothing of it is built until then). Still the owner's, not built: a forecast at the coarse tube's stop too (its threshold cannot go under about 3 gr otherwise; 18:25).

## 2026-11-25, 20:01 to 20:59: step 1 on the bench, the Pico 2 W: gates 1 to 3

The owner said the bench was ready (20:01): the Pico 2 W (FA0C, 192.168.2.166) and the A&D on the probe, the cup emptied, AR2209 in the hopper ("the stored profile IS wrong, so a good candidate to run calibration and tuning against"), the owner at the bench. Its settings copied first (`logs/adr009/restart_bench/before_bench`).

**Gate 1, the method from the PC** (`calibrate_unit.py`, on the image the unit ran, `41f9189`; capacity 250 gr): pass.

| Part | Records | Time, powder |
|---|---|---|
| Scale | settle 0.469 s (four bursts); lag 1.26 s at 0.20 gr/s, 0.93 at 0.40, 0.68 at 0.79, 0.64 at 2.7, 0.56 at 10.2 | 1.7 min, 61 gr |
| Fine tube | least 0.020 rev/s, stream 0.752 gr a turn | 3.8 min, 11.6 gr |
| Coarse tube | least 0.160 rev/s, stream 10.44 gr a turn | 1.5 min, 54.7 gr |

**Gate 2, an update from today's release that keeps every setting** (the Pico 2 W; the Pico W's pass owed):
- v2.0.3 built from its tag in a worktree (`C:\workspace\OpenTrickler-v2.0.3`) and flashed by the probe with that worktree's `rc_run.py` (ours checks an image against this checkout's version). Every stored struct the same size in v2.0.3 and the unit's image (the Wi-Fi's `setup_done` was put in its filler), so v2.0.3 read every module's record; its settings equal the copy before, field by field (`on_v203`).
- The update to this branch's release (`build_release.py`, 2.0.3-196) through the API, as a unit takes one: the image sent in 6.7 s, the trial confirmed, slot B. The loader kept both records that grew ("the record at 0x0400 was stored 6 bytes long, before fields were appended: kept", the scale's; 0x1400, 64 bytes, the charge's), and their writes waited for the confirmation. Every setting as on v2.0.3, field by field (`after_update`), the new fields null and stored. **Pass.**
- `rc_run.py update` failed it: it required the whole export equal before and after, which no update that appends a field can pass. Mended: it now compares a field at a time as `settings --compare` does (`settings_differences`), a field only one copy has counting as null in the other; it runs first on the Pico W's pass.

**Gate 3, the calibration as a job on the unit** (2.0.3-196): pass.
- **The cup, over the API** (driven by `cup_part.py`: the owner lifted the cup and put it back, the fill run by the reading to 550 gr, under the bench's 600; the cup holds 800 gr): zeroing 0.6 s; the lifted cup read at −444.9 and kept as its weight; filled to 541.48 gr, kept as the capacity, stored into `settings/charge`; the part ended by itself once the emptied cup was back.
- **The scale, through the console**: settle 0.55 s; lag 0.89 s at 0.22 gr/s, 0.75 at 0.41, 0.60 at 0.87, 0.55 at 2.6, 0.52 at 10.1; 1.8 min, 62 gr; kept from the screen's Keep (below), stored.
- **AR2209's powder, over the API** (profile 1, not the charge's): the fine tube's least 0.0336 rev/s (was 0.08), the coarse's 0.16 (was 0.10); 5.0 min, 66 gr; saved into profile 1 alone, stored.
- **Against gate 1:** the unit's lags are at or 0.03 to 0.05 s under the PC sessions' range at each flow (`records_spread.py`: the fine tube's 1 rev/s 0.63 to 0.83 s against the unit's 0.60; the coarse's 0.55 to 0.59 against 0.52), the PC's known lateness (about 0.055 s, the first build's), and within one session the lag varies by 0.08 to 0.34 s. The coarse tube's least is the same; the fine's 0.034 against 0.020 is its slowest step, 0.010 rev/s, failing the test this time, which sits near its bar (0.61 to 1.35 of the stream's pour across sessions, against 0.5). The settle time, 0.55 s against 0.47, has one burst measure each.
- **Stops** (`stop_tests.py`, `fill_stop.py`): at the first step, while zeroing, while the tubes turn, at the result, and in the cup's fill with both tubes turning: each time the job idle, both motors still, the event `stopped`, nothing kept.
- **A driver's fault:** the fine driver made to report a short mid-run: the motors' fault event, then 28 ms later the calibration stopped with reason `fault`, the motors still. With a driver faulted a start is refused (`driver_fault`); the next start reads the drivers afresh and clears the latched fault.
- **The A&D's rate against the room for readings:** at 21 a second the lag runs (2 s at most) take about 42 of a run's 128; only the least ladder's slow runs fill it, and they use only their first reading and their rest. Past the room, readings are dropped with nothing said: harmless today, a trap for a longer run (noted, not mended).
- **Found and mended (in the next image):**
  - **Keep did nothing when pressed** (the owner, 20:37): the result screen took a click only once turned to its end, and counted the end one turn late, so Keep was shown but not reached; and a click before it did nothing. Now as every notes page does it: a click before the end turns the page, and the button is reached the moment it shows (`menu_nav.c`).
  - **The calibration's writes were logged `by` button** whoever kept them (the API's cup and powder, the console's): it wrote through the screen's own helpers. Now `settings_write_by` and `settings_store_by` take who wrote, and the calibration's answer and save carry it from the API, the console and the menu.
- **The owner's words at the bench (recorded 20:59):** no confirmation added to the calibration's cup part (its fill waits for a speed the person sets: a click to edit, then a turn); "Only the part that needs human to put back the cup should have the confirmation": the charge's next throw, in step 2's gate 3 (the README's step 2).

## 2026-11-25, 21:00 to 21:58: step 1 on the bench, the Pico 2 W: gate 4, and gate 5 as far as it goes

**Gate 4, the screens and the web** (2.0.3-197, flashed by the probe): pass on the Pico 2 W.
- `menu_tour --compare`: two pages changed, as meant: Settings (its Calibration row first) and Settings › Calibration (new: Cup, Scale, Powder). Both saved into `tests/menu_baseline/pico2_w`; the main page matches (with the cup and the scale calibrated it does not offer "Calibrate this unit").
- `reach`: every element of every screen reached by turning. Its cup budget mended: its simulated charge poured 55.4 gr of AR2209 for the 30 the stand-in scale counted, against a flat 40 stated; now twice its target and a throw's start.
- The result screen's Keep, mended at 20:59: two clicks with no turn, the first turning the page so Keep is reached, the second keeping the records (`by` button, from the console's `press enc`).
- **The web on the unit:** the scale part run from the unit's own page end to end (the empty cup, Calibrating with its Stop, the result with Discard and Keep, the records read again into the card); the keep logged `by` api; no error in the browser's console.
- **The owner, at the bench (recorded 21:58):** "Should the STOP has the same attribute as the "Calibrate xxx" button?", then "the same design language should apply to the entire web frontend, that if a button is replaced by another feature then the attribute should stay, so the user will not see the shift of the element." Done: a step's lone key spans the card (`settings.js`, checked on the stand-in); the rule written as rule 17 of the `web-ui-design` skill. **Owed:** the calibration's steps shown in the card whose button started them, its Stop in the button's place (today a card appears at the top and pushes the page down; a powder's steps show above its row the same way), and the other pages read against the rule (the charge's and the purge's Start and Stop, the update).

**Gate 5, step 1's sign-off** ("How the unit shows it works"), on the Pico 2 W:

| Item | Shown tonight |
|---|---|
| From the 12864, from nothing to its records, and stopped | AR2209's powder part run by the owner from the screen on a unit made never calibrated (the cup and the scale records cleared): its steps, the result, Save. **The cup part from the screen is owed** (below). A stop from the screen's Back not tried; the stops were proven over the API (gate 3) |
| From the web | the scale part, end to end (gate 4) |
| Each part alone, in any order, on defaults | the powder part ran on the cup's and the scale's defaults (the unit made never calibrated); the scale's parts left AR2209's record as it was |
| No pour passes the cup's capacity | the capacity set live to 40 gr: the scale part paused at 38.7 ("It holds 39 of 49"), the owner emptied the cup and went on; the stored capacity put back |
| Kept across a restart, in the backup, given back by a restore | the export holds every record; a restart kept them field by field; the copy restored whole and compared equal |
| An update keeps every setting | gate 2 on the Pico 2 W; **the Pico W's pass owed** |
| The values its recordings give | gate 3, against gate 1's spread |
| A step made to fail | **fails**: below |
| Its time and powder reported | every part |
| A charge throws with them | one real 10 gr throw on AR2209 (its new least speeds, its stored gains, today's loop): 12.2 gr, over, as its gains make it |

**Found, to be mended:**
- **A lifted cup and a silent scale go unnoticed during a run.** The cup lifted for 5 s in the scale part's lag runs: no failure; the part reached a result whose lag table held a point at 235 gr/s and 0 ms (the lift and the return read as a flow), its slowest point gone; kept, it would have stored a false table (it was discarded). The job trusts every reading; a reading that does not come is skipped with nothing said (`bench_watch`). The mend, in the unit's watch, not the method: a reading more than the cup's own 2 gr under the part's zero ends the run as "The cup was lifted"; no reading for longer than the scale's lost-reading time ends it as "No reading from the scale"; the item stays as it was, nothing retried. Both tried on the unit after.
- **The pause's line** reads "It holds 39 of 49": the cup's weight and what the next run could make it, read as a capacity of 49. To say the capacity: "It holds 39 of 40".
- **The fine tube's least moves from run to run:** 0.020 (the PC's run), 0.034 and 0.056 rev/s (the unit's, the last on the scale's defaults) on the same powder and evening: each time the next step of the ladder's slow end (twice the slowest step that passes, those steps near the test's bar on this rig: 0.61 to 1.35 of the stream's pour across the kept sessions, against 0.5). Step 2 takes the least as the PID's floor, so a factor of 2.8 matters. For the owner, with its evidence, before step 2 is built.
- Small: the calibration's idle status keeps the last part's line; the web's card is blank for a second or two after Keep while it reads the records again; a run's readings past its room are dropped with nothing said (gate 3).

**The owner's decisions at the bench (recorded 21:58):**
- **The cup's fill, redesigned on the owner's idea:** "My original idea was to tie the rotary button to the motion of the two tubes. So the rotary button will not control the speed, but position, when filling the cups. And on the screen you should show what the user should do. Currently the Fill the cup UI design is misleading." Agreed as proposed: the knob moves the coarse tube (a detent a quarter turn, about 2.5 gr; turned quickly, a whole turn), clockwise only, at most about a second's motion queued so the tube stops when the knob stops; the screen says "Turn the knob to pour" and "Click Full when the cup is full", the weight its figure; the web a "Hold to pour" key in the same place. **Mock-ups first**, then built.
- The calibration's cup part needs no added confirmation after the cup is back (its fill waits for the person); the charge's next throw after the cup is put back waits for a press (step 2, gate 3).
- The rest of gate 5 continued that evening; what it could not finish is owed.

**What the bench owes for step 1's sign-off,** in order once built:
1. The mends above (the lifted cup and the silent scale; the pause's words), the fill as mocked and approved, the web's steps in place (rule 17); built, the host tests and the stand-in, both boards built.
2. On the Pico 2 W: the cup part from the 12864 and from the web; a lifted cup and an unplugged scale mid-run, each said and nothing kept; a stop from the screen's Back; `menu_tour` and `reach` again.
3. The Pico W on the probe: gate 2's update from v2.0.3 (with `rc_run.py update`'s field-by-field check), its menu baseline, a part from its screen.
4. The fine tube's least: the owner's decision on its spread.

## 2026-11-25, 22:00 to 22:15: the knob's fill, the mends, the web's steps in place (built, no unit)

- **The owner (recorded 22:15):** "have you implemented my rotary button controlled cup volume calibration?" It had not been: the approved plan said "The user gives its capacity by filling it with the knob" and "trickles with the knob", and gate 3 built it as the purge's control, the knob setting each tube's speed; the mock-up sheet the owner saw that afternoon showed that, and did not say it had turned the idea into speeds. A misreading of the owner's idea, not a gap in the record.
- **The mock-up, approved as drawn** (`tools/ui_mock/calibration_12864.py`, the sheet `mockups/calibration_12864.png`): "Fill the cup", the powder in the cup as the figure, "Turn the knob to pour" under it, Cancel and Full with Full focused. Its two exceptions to the screen's rules (a line naming a control; the knob not moving between the softkeys) are written into `embedded-ui-design` with the owner's words.
- **Built:**
  - **The fill** (`calibration.c`): a pour of the coarse tube's turns, held at most 2 (`CALIBRATION_POUR_MOST_TURNS`), poured at 2 rev/s while any are left, so the tube stops within about a second of the last; the knob's detent a quarter turn, a whole one when it follows the last within 80 ms; counter-clockwise nothing. The screen a job whose turn pours (`menu_nav.c`). The API `POST /api/v1/calibration/pour` `{"turns"}` (400 `wrong_type`, `out_of_range`; 409 `not_filling`) in place of `PATCH fill`, the status `pouring` and `pour_turns` in place of the two speeds; the console `calibrate pour <turns>`; `rc_run.py`'s cup check counts a pour by its turns, `api_check` calls it.
  - **The web** (`settings.js`): Hold to pour, a full-width key that sends half a turn every quarter second while held (a finger, the mouse, Space or Enter) and stops when lifted, slid off, or replaced by the next step. On the stand-in: held 2 s, 45 gr (40 at 20 gr/s and the half turn held), nothing more after the release.
  - **The steps in place** (`web-ui-design`, rule 17): a part's steps run in its own card where its Calibrate button was, the records above them still; a powder's in the place of its actions row, hidden meanwhile, its steps starting at the row's own top (504 px, 504 px); the page's scroll anchoring off inside the calibration, so a step's card changing height does not scroll the page (scrolled 90 px through fill and keep: unmoved). At a phone's width the key is 317 by 60 px, nothing wider than the screen.
  - **A lifted cup and a silent scale** (`bench_watch`): a reading 2 under the part's zero ends the run as "Cup: lifted mid-run"; no reading for ten of the scale's intervals (never under 1 s, the charge's rule) as "Scale: no reading"; the part fails with it, nothing kept, nothing retried.
  - **The pause's words:** "It holds 39 of 40", against the cup's capacity, as the mock-up always had them.
- **Checked:** both boards build (the Pico W 98.3% of its flash, 99.6% of its RAM); `tests/host_test.py` and `tests/calibration_sim.py` pass; the web walked through on the stand-in with no console error.
- **Owed on the unit:** the cup's part from the 12864 (the knob pouring, Full, Keep) and from the web (Hold to pour); a lifted cup and an unplugged scale mid-run; the pause's words; `menu_tour` and `reach` again; then the Pico W's passes (progress, 21:58).

## 2026-11-25, 22:17 to 23:20: the knob's fill and the mends on the Pico 2 W

Flashed by the probe, each settings copy equal before and after (`before_fill_flash`, `after_fill_flash`); the last image 2.0.3-207 (`04bf47c`).
- **The pause's words:** "It holds 30 of 40 gr", against the capacity. Pass.
- **A cup lifted mid-run** (the owner): the part ended under 5 s after the call with "Cup: lifted mid-run", the motors still, the scale's records unchanged. Pass.
- **The scale unplugged mid-run** (the owner): "Scale: no reading", the records unchanged. Pass.
- **The failure's words** (the owner, at the bench: a failed part "quited ... which ... will trick the user thinking the calibration is done", and "Its records are kept" read as success): every part's failure is now "Not calibrated" / its cause / "Nothing changed" / OK, on the 12864 and the web (chosen by the owner, 23:20); a stop at a failure keeps the reason `failed`. Seen on the unit with the simulated scale made quiet.
- **The knob's fill from the 12864** (the owner), in three images:
  - first, turns added to a queue of 2: "the rotary encoder input queue can be lagging behind ... The excessive speed can cause safety issue ... when the tube is moving the excessive input event will be discard, instead of being queued." The API's log showed 1.9 to 2.0 turns held after a quick spin, the tube running on about a second;
  - then a pour that extends, never adds (a detent a quarter turn from now, the API at most half a turn): no pour held past about 0.21 turns, each stopping with the knob. "The lag is resolved";
  - then the owner's three points: no way to quit that showed, a Cancel drawn but out of the knob's reach, and reverse motion. The owner's choice: "I'd allow the reverse motion, and remove the cancel button": right pours, left turns the tube back (the log: −0.21 to −0.25 turns), only the right softkey, the title's "<" for Back; then "simplify the tip": "Rotate to dispense"; then "the full button is such confusing": the key is "It's full" (the owner's choice among It's full, Cup is full, Already full). The last seen on the unit through the simulated scale: "< Fill the cup", "Rotate to dispense", "It's full".
  - The owner filled to 623.57 gr once (24 gr past the bench's 600, by hand), and later kept 249.65 gr; the stored capacity is 249.65 gr, the cup 444.93 gr.
- **The cup's part from the unit's web page:** started there, the cup lifted and put back by the owner, Hold to dispense held 1.5 s: pouring while held, stopped 82 ms after the release; finished with the web's Done. Full and Keep were pressed at the unit meanwhile (the owner), and the knob poured alongside: the API cannot tell the two apart, by design.
- **`menu_tour`:** every page as its baseline but Settings › Calibration, whose data changed (the cup's capacity, the charge's powder now AR2209, the hopper's): saved again. **`reach`:** every element reached; its simulated charge poured 56 gr, inside its 70.
- **The owner (recorded 23:20):** "The functional side is already verified and I'm satisfied"; further UI changes are shown as mock-ups, not run on the unit.

**Found and noted, not mended:**
- The Settings page goes blank for a second or two when it reads a setting again (after a part keeps its records): a shift under rule 17; it should keep what it shows until the new arrives.
- One reply from the unit cut off mid-way (`ERR_INCOMPLETE_CHUNKED_ENCODING`) while the page asked for a pour every 150 ms and polled: a web-server matter (ADR-003), seen once.

**The owner's idea for the web (recorded 23:20), being mocked:** "a popup dialog for each calibration item (like a wizard) works better than a random embedded element." Recommended: a sheet per part over the page, its keys in fixed places, opened by the part's button or by itself while a part runs, closed only by its own keys; on the stand-in for the owner's look.

## 2026-11-25, 23:20 to 23:37: the web's calibration as a wizard (the stand-in, no unit)

- **The owner's idea** (23:20): "a popup dialog for each calibration item (like a wizard) works better than a random embedded element"; and "For the further UI change, you don't need to run on the actual device, you can simply show with mockup."
- **Built** (`settings.js`, `powders.js`, `plain.css`): each part's steps run in one sheet over the page (`<dialog class="sheet wizard">`), the page's own sheet (the cup's question, the confirmation) at a fixed size: 440 by 560 px, the whole screen on a phone, the page under it held still. The part's name small at the top, the step's words as the heading, the figure and the words under it, the keys at its foot: each key at the same place and size on every step (a lone Cancel as tall as a pair's; 530 px from the top, 60 px tall, on every step of the cup's part at 800 by 660). It opens with the part's first step, also when the part was started at the unit or in another browser (on a powder's page, that powder's), and closes when the part ends; Esc does the step's way out (Cancel, Stop, Discard). The cards in Settings keep only the records and the Calibrate buttons; a powder's page keeps its Calibrate button among its actions.
- **Found on the way and mended:** the sheet's styles first reached the page's two other dialogs (now `.sheet.wizard` only; the cup's question keeps its 24 px padding and its height); the figure's unit cut off at the sheet's edge (the figure's line stretched across it); a lone key 8 px shorter than a pair's (52 against 60); a step's heading cut at a phone's width (the status pill does not wrap; in the sheet it is a heading that does).
- **Walked on the stand-in** at 800 by 660 and at a phone's 375 by 812: the cup's seven steps, the scale's run and result, a powder's run, pause and result, a powder's failure, a start from a powder's page, Esc and Cancel each ending the part; no overflow either way, no console error. The sheet: `mockups/calibration_web_sheet.png`, the phone: `mockups/calibration_web_sheet_phone.jpg`.
- **Both boards build** with it (the Pico W 98.4% of its flash, 99.6% of its RAM).
- **Approved as drawn** (the owner, 23:37); written as rule 18 of the `web-ui-design` skill with the owner's words, rule 17 read with it (a job's steps go in its button's place or in its sheet, never in a card pushing the page down).

**Noted, not changed:** the first step of the cup's own part says "To catch the powder. The cup next.", whose "The cup next" names the part it is already in.

## 2026-11-25, 23:40: the Pico W's work left to the end

- **The owner (recorded 23:40):** "Pico W work to leave to last of ADR-009." Written into the README: the owner's direction, gate 5 (step 1 signed off on the Pico 2 W) and a section of its own, "Last: the Pico W", which the ADR does not close without.
- So item 3 of "What the bench owes for step 1's sign-off" (21:58; the Pico W on the probe: gate 2's update from v2.0.3, its menu baseline, a part from its screen) moves there, with step 2's charge on the Pico W.

**Step 1's sign-off: what is left** (the Pico 2 W):
1. The fine tube's least: the owner's decision on its spread from run to run (0.020, 0.034 and 0.056 rev/s; progress, 21:00 to 21:58).
2. The Settings page going blank for a second or two when it reads its settings again (after a part keeps its records): a shift under the `web-ui-design` skill's rule 17. Mended and shown on the stand-in.
3. The cup's own part says "To catch the powder. The cup next." on its first step, naming the part it is in: its words, on the stand-in.

## 2026-11-25, 23:40 to 23:50: step 1's last items (the stand-in, no unit)

- **The fine tube's least.** The owner: "Should it be a fixed number after calibration?" It is: a calibration stores one number in the powder's profile, and nothing changes it until that powder is calibrated again (step 2's tuner moves kp alone). Three runs measured 0.020, 0.034 and 0.056 rev/s. Step 2's controller in the simulation at each (`step2.py --controllers step2:last@least=...`, 300 throws of 10 gr, judged at 0.03 gr), the time per good charge:

  | Rig | 0.020 | 0.034 | 0.056 |
  |---|---|---|---|
  | AR2209, night | 17.0 s | 17.5 s | 17.5 s |
  | AR2209, f2 | 14.0 s | 15.7 s | 14.2 s |
  | BM2 | 23.1 s | 24.5 s | 18.0 s |
  | AR2209, night, the fine tube's pour by its speed | 18.3 s | 17.5 s | 18.5 s |

  No least is better on every rig; a higher least is a little quicker each throw, and the last rig's spread is wider at 0.056 (sd 0.152 against 0.094) at the same cost. **The owner: "Accept as measured"** (23:50): each calibration keeps what its run measured; nothing repeats or averages it. Step 2's bench series (its gate 4) would show a floor that matters on real powder.
- **The Settings page going blank on a reread.** The owner: "it appears breaking the MVC guideline, that UI is bending on the data. Perhaps an indicator at the corner of page can be used to determine the frontend and backend connection status." The cause: Settings and Powders, read again (a calibration kept, another client's write), put their list in place of the page first and the part only once its reads were done, and the calibration's cards filled in after that; two reads at once could show the older last. Measured on the stand-in before the mend: on a reread the part was gone, then the calibration empty, the page 610 px tall in place of 771. **Mended** (`settings.js`, `powders.js`): the part shown, read again, stays until the new one is read whole (its forms and the calibration's first read), then the two are swapped at once, and only the latest read is shown; a read that fails keeps what is shown, the link's banner saying the unit was not reached; the calibration's poll keeps its cards when a read fails. After: two rereads at once, a cup part kept (its capacity 30.92 to 20.00 gr in place), the Powders page read twice: never blank, the height unchanged, one part on the page; the link cut: both pages unchanged under "Unit not reached. Trying again..."; a part started elsewhere still opens the sheet (0.55 s). A part newly asked for shows its list first, as before. **The corner mark:** weighed against rule 9 (the owner, 2026-11-18: a permanent "Connected" was a waste of space; normal takes no room), the banner already saying a lost link on every page; **the owner: "No corner mark"** (23:50). Written into the `web-ui-design` skill, rule 17.
- **The cup step's words.** The owner: "Fix if it breaks the user experience flow." It did: the cup's own part said "To catch the powder. The cup next.", naming the part it is in. Now "To catch the powder." for the cup and the scale; a powder's stays "With <powder> in the hopper: it is what is calibrated." The 12864 says neither (its steps are "Put the empty cup on the scale", with "<powder> in the hopper" for a powder).

Both boards build with it (the Pico W 98.5% of its flash, 99.6% of its RAM).

**Step 1's sign-off on the Pico 2 W: every item closed**, for the owner to sign off. One check is not in the record: a stop by the 12864's Back. The stop itself is proven from the console and the API at every step, the fill with both tubes turning among them (20:01 to 20:59, `stop_tests.py`, `fill_stop.py`), and Back is the menu's own key. The Pico W's checks are the ADR's last ("Last: the Pico W").

## 2026-11-25, 23:51: step 1 signed off; step 2's gate 2 started

- **The owner:** "Sign off, start gate 2." Step 1 is signed off on the Pico 2 W, with the one check not in the record said (a stop by the 12864's Back). Its Pico W checks are the ADR's last.
- Step 2's gate 2 starts: the controller and the tuner in C, behind the interface in the README, judged against the Python on the same runs.

## 2026-11-25, 23:51 to 2026-11-26, 00:08: step 2's gate 2, the controller and the tuner in C (no unit)

**Built:**
- `src/charge_controller.c` and `.h`: `step2.py`'s Step2 and the coarse tube of `legacy_pid.py`, line for line, plain C with nothing of the RTOS, the motors or the scale. Given its config (each tube's gains and least and most speed, the coarse threshold, the back-off, step 1's settle time and lag table, the motors' ramp) and fed `start`, a `tick` each millisecond and each `reading`; it gives each tube's speed and `done`, then `verdict_after_s` and `at_rest` (the weight a turn carried to the next throws, median of the last 5, RAM only). Its times are doubles, as the Python's, so the two can be judged call for call.
- `src/charge_tuner.c` and `.h`: `step2_tuner.py`'s Tuner, a throw at a time (`charge_tuner_throw`), the window of 25 and the kept kp's pool of 75 in RAM, on the README's rule: a cheaper trial is kept.
- `tests/step2_sim.py` and `tests/host/charge_lib.c`: the C built for this PC as a library (as `calibration_sim.py` does) and judged on the twin's four rigs.
- Neither is called by the firmware yet (gate 3): both boards build with them and no warning in them, the images unchanged in size.

**Found on the way and mended:**
- **The carry was lost after a long throw.** The first judged run stopped at the same millisecond as the Python on 25 throws of 25, but a 24.7 s throw outran the C's room for parcels; the C then carried nothing from it and every later throw's weight a turn drifted from the Python's (up to 12%). The root: the carry at rest was counted from the whole store of parcels, which it does not need. Now the parcels within the lags before the coarse tube's quiet are kept apart (their rest depends on the weight a turn) and every parcel after is summed as it comes: the carry is exact at any length. The room bounds only the throw's own fit, which is used only with nothing carried (the first throw after a start): past 20 s of parcels (or 512 readings) the own weight a turn is held as last found.
- **The tuner's rule in the tool.** `step2_tuner.py` as committed at gate 1 (f680748) kept a trial only past 1.5 times its noise (Z 1.5, its default), while the README and the 19:52 record say a lower cost is enough. The C and the judge follow the README (z 0); the tool's default is now 0. The 19:52 table's runs are not rerun here.
- **The README said two windows** on the kept gains once both ways are put back; the Python, which passed gate 1, throws three (the window after the second put-back, then two resting). The README now says three; the C does the same.

**Judged** (`tests/step2_sim.py`, two sessions of 25 throws a rig, the tuner 300 throws from the owner's kp and from a quarter of it):

| Rig | Throws | The same stop | Speeds within | Weight a turn within | Carried within | Most parcels / readings |
|---|---|---|---|---|---|---|
| AR2209, night | 50 | 50 | 1.1e-6 rev/s | 8.9e-6 | 1.4e-7 | 2048 (one throw past it, 24.7 s) / 428 |
| AR2209, f2 | 50 | 50 | 7.5e-7 | 1.6e-6 | 1.0e-7 | 1446 / 268 |
| BM2 | 50 | 50 | 2.6e-6 | 5.7e-6 | 5.1e-8 | 2024 / 389 |
| AR2209, night, the fine tube's pour by its speed | 50 | 50 | 1.4e-7 | 2.2e-6 | 7.6e-8 | 1303 / 233 |

- **The tuner:** the same step to the same kp in every window of all 8 cases (12 windows each).
- **The C alone on the twin** (its own speeds driving the plant), against the Python on the same seeds: the same sd, mean, share good and time to the digit on three rigs; on the fourth sd 0.125 against 0.133, 56 against 58% good: its speeds differ from the Python's in the sixth decimal, and that rig's plant carries the difference on.
- **Verdict: pass.**

**For gate 3:** the controller holds about 30 KB (31,160 bytes on the PC, most of it the parcels and readings for the first throw's own fit); where it lives on the unit, and its time each millisecond and each reading on both chips, are gate 3's to place and measure. The Pico W's are the ADR's last.

## 2026-11-26, 00:10 to 00:25: gate 3's plan, drafted

- The owner: "Yes, plan gate 3." Read first: the charge loop (`charge_mode_wait_for_complete`) runs in the job task and wakes only on a reading; the FreeRTOS tick is 1 ms; the heap is 144 KB on the Pico 2 W and 112 KB on the Pico W; the charge settings have a revision and the loader's migration. The plan is in the README, under gate 3: the controller's throw room from the heap during a charge, the carry and the tuner kept in RAM; the loop waking each millisecond; the verdict after the slowest lag and the settle time; the lifted cup; the tuner's kept kp written through the settings by `tuner`, a trial's never; `auto_tune` appended to the charge settings (a layout change, said there); the press after the cup's return, mock-ups first; the controller's time measured.
- **Today's loop** (asked: kept behind a setting for gate 4's same-day comparison, as the first build's revision had it, or step 2 alone): the owner, "There will be no selection. The user will use the charge control just like the old controller. In terms of the performance comparison, I guess the key criteria is if the auto tune can optimize the PID towards the "better" direction. I don't want to set a "target goal", as we don't touch the controller mechanism, only the parameters are adjusted." Today's loop goes; gate 4 and the step's proof are judged by the tuner's direction, window after window, with no target. Written into the README.
- The plan waits for the owner's approval.

## 2026-11-26, 00:28: gate 3's plan approved

- **The owner:** "Approve, with my correction: we have the baseline. The baseline is the current untuned (or aka default PID stored on this unit). And the check is, if the auto tune can slowly optimize the charge towards the refined PID, i.e,. shorter time to trickle, and less over and undershoot."
- Gate 3 is built as planned (the README). Gate 4's baseline is the charge on the gains stored on the bench's unit, untuned (its first window); its check, whether the tuner brings shorter times to the stop and fewer throws over and under, window after window. The README's gate 4 and the step's proof say so.
- First of gate 3: the mock-ups of the press after the cup's return, for the owner's look while the rest is built.

## 2026-11-26, 00:28 to 00:39: gate 3, the press mocked and chosen

- **The 12864** (`tools/ui_mock/charge_12864.py`, `mockups/charge_12864.png`): from one throw's end to the next one's start, today's screens and the new "Ready" between Zeroing and Charging, softkeys Stop and Start.
- **The web, first mock:** Stop and Start side by side for good, each enabled when it could act. **The owner:** "For the web UI, given stop and start are mutually exclusive, they can share the same button (not refering to the implementation. This is from user's experiences perspective). I don't think at any time both button should be enabled."
- **At Ready** (asked: the web then has no Stop): the owner chose "Another job ends it": a charge waiting at Ready ends when another job is started, so the Purge view is open meanwhile; the 12864 keeps its Stop.
- **The web, built on the stand-in** (`trickler.js`; `tests/web_test.py` plays `waiting_for_start` and `POST /api/v1/charge/next`, and a purge started at Ready ends the waiting charge): one key, Stop while a throw runs or the cup is out, Start at Ready (and idle); at Ready the powder and the target open as when idle. Walked at a phone's width: Start at Ready begins the next throw; a purge started at Ready leaves the charge idle and the purge running; no console error. The sheet: `mockups/charge_web_press.png`.
- Written into the `web-ui-design` skill, rule 17, with the owner's words.

## 2026-11-26, 00:39 to 00:52: gate 3 built (no unit)

**Built** (the plan's items in the README):
1. **The room:** the throw's room (`charge_room_t`, the parcels, readings, air and edge) apart from the controller; the charge borrows it from the heap at its start and gives it back at its end. The controller and the tuner (`charge_step2_t`, about 2 KB) are taken from the heap at the first charge and kept: the Pico W had no static RAM for them (the image linked at 100.5% of its RAM with them static). No room to have: the charge ends at once, "No memory".
2. **The charge loop** (`charge_mode_wait_for_complete`): wakes each millisecond (`vTaskDelayUntil`, 1 tick), ticks the controller, gives it each reading as it comes (`scale_take_new_measurement`, a new non-blocking read: the old wait with 0 waits for ever), and sets both tubes' speeds as they change. The coarse tube's stop moves the servo gate as today; the precharge, the lights and the events as today. A pause (ADR-013) stops both motors as before; at its end the controller is told (`charge_controller_resume`: the fine tube counted from standing, the D term begun again) and given the settled weight. The fine tube's ramp is the motor's own, in the trickler's turns (`motor_get_acceleration`). Step 1's scale never calibrated: the simulation's defaults (settle 1 s, a lag of 0.8 s at 1 a second). Today's loop and its back-off code are gone.
3. **The verdict** after the controller's wait (the slowest lag and the settle time), then the throw at rest: its weight a turn carried (the console says it).
4. **The lifted cup:** a reading under half the empty cup's weight (never calibrated, 10 under zero) in a throw stops both tubes and ends the throw as `cup_lifted` ("Cup lifted"), through the same path a lost scale takes: the charge waits to be acknowledged.
5. **The tuner:** after each verdict, out of the throw; a window closed, the next kp is the tuner's (a trial in RAM only); a kept kp written into the profile and stored by `unit` (`ACTIVITY_BY_UNIT`, the unit itself: only the tuner writes a profile so, and its path says which), the console's `Tuner:` line each window. The carry begins afresh at a change of powder or of the scale's unit, the tuner also when the profile's kp is changed elsewhere or the switch is turned.
6. **`auto_tune`**, on by default, appended to the charge settings (the loader keeps an earlier version's record, its default for the new field: the host test of the mechanism passes); in the settings table, so the screen, the web, the export and the schema follow.
7. **The press:** the state `CHARGE_MODE_WAIT_FOR_START` after the cup's return: the zero's wait (`zeroing`), then, steady, `waiting_for_start` ("Ready"); a press taken only then, under the zero's lock, and the throw begins at it (straight into charging: the pan was just read steady). Not in a throw: what to throw may be changed at Ready, as the web now allows. The 12864: Ready, Start on the right softkey; the API: `POST /api/v1/charge/next` (409 `not_now` otherwise), in the index, the schema and api_v1.md; the console: `charge next`. A purge, a calibration or an update asked while a charge waits between throws ends the charge first (`charge_mode_yield`, up to 2 s), as the owner chose.
8. **The cost:** `stats` ends with `charge_tick_us` and `charge_reading_us` (most/mean since the start) and `charge_heap_low`.

**Tools:** `rc_run.py`'s `charge_cycle` gives the press once the unit takes it (its states now 1, 2, 3, 4, 5, 2), so `soak` throws back to back as before; `api_check` calls `charge/next` where it must be refused. `reach` does not sweep the Ready screen yet: a click there starts a throw.

**Checked with no unit:** both boards build, no new warning; `tests/host_test.py` and `tests/step2_sim.py` pass (the C after the room's split and the resume, call for call as before); the web on the stand-in as in the press's entry.

**The Pico W's room** (the ADR's last, the owner's): its image is at 99.96% of its flash (368 bytes free) and 99.7% of its RAM at link time; a throw's room (about 28 KB) must come from its heap at each charge. Both are for "Last: the Pico W", by AGENTS.md's order of remedies.

**Owed, on the Pico 2 W when the owner says the bench is ready:** the README's gate 3 proof (flash, settings copy, `menu_tour`, `reach`, `charge_cycle`, real throws on the forecast with the carry renewed, a cup lifted mid-throw, a window kept into the profile and none with the switch off, the press, the cost in `stats`).

## 2026-11-26, 00:52 to 01:24: gate 3 on the Pico 2 W, begun; the press withdrawn

**On the unit** (the owner: "Bench is ready"; the Pico 2 W FA0C, the A&D, AR2209 in the hopper):
- Flashed by the probe (2.0.3-216, then 2.0.3-217 at `b289f49`); the settings copy before and after equal in every stored field, the new `auto_tune` true, its default (`logs/adr009/gate3/before_flash`, `after_flash`).
- `menu_tour`: every page as its baseline but Settings › Charge, which lacked the switch (that page lists its rows by hand): the row Auto-tune (On, Off) added, the page saved again (`b289f49`). `reach`: every element reached. `charge_cycle`: passed; the charge ran on the controller (its own weight a turn, then 0.19 and 0.23 gr a turn carried on the simulated scale).
- **Three real throws of 10 gr** (throws 5 to 7, the owner lifting, emptying and returning the cup): 11.56, 11.28, 11.38 gr, each in 2.3 to 2.4 s, each over. **Found:** this unit's AR2209 profile (fine kp 2, fine top 5 rev/s, coarse kp 0.02) pours both tubes flat out, and each throw ends before the coarse tube's quiet (its stop, the slowest lag and the settle time, about 1.6 s), so no throw finds or carries a weight a turn and the forecast never comes into play: every throw stops on the reading alone, as today's loop, over by what is in the air. The twin's rigs threw for about 10 s on the owner's gains, so it never showed there. For the owner, before the rest of gate 3's proof.
- At the owner's word the coarse tube's back-off was turned off for the rest of the test, live and not stored (`coarse_stop_backoff_enable`).

**The press withdrawn.** The owner: "For #2, you overread my statement. My scope, was just that single step of the calibration. Nothing more, nothing less. So I need you to evaluate the impact radious." The words of 20:59 answered a question about the charge, and were read as a press before each of the charge's throws, written as the owner's direction, put into gate 3's plan as "the owner's", mocked, chosen on and built. Its radius, mapped before anything was undone: the charge mode's Ready state and its press, the other jobs' starts ending a waiting charge, the menu's Ready and Start, the console's `charge next`, the API's `POST /api/v1/charge/next` (index, schema), the web's Ready and the stand-in's, `rc_run.py`'s press in `charge_cycle` and `api_check`, api_v1.md, AGENTS.md, the README, the mock-up sheets, rule 17's example. **Unwound** (the owner: "Yes, go ahead"), the rest of gate 3 kept: the cup's return goes to the zero's wait again, as the original; the web page, the stand-in and `rc_run.py` are as they were at `b14159f`; the mock-up sheets and `charge_12864.py` removed; the owner's rule for the web (one key, never both) kept, it was already the web's; the lesson kept in the agents' memory (an answer holds for the step it was given about).

**The delays, at the owner's word** ("be extremely careful with the delay I had in my original code. Delays were there for purpose"): each of the original charge's waits checked against the unwound code: the zero's wait (ten readings 300 ms apart, steady, none from before a zero: after the cup's return, the guard against a bump), the cup's return (readings 20 ms apart), its removal (five steady readings 300 ms apart), the gate's close waited for then 500 ms before the precharge, the precharge's time, 20 ms after a throw with no precharge, a pause's readings 300 ms apart, a fault's acknowledgement each 50 ms: each as the original. In a throw the original waited up to 200 ms for each reading; the controller's loop wakes each millisecond, so the stop is seen sooner, never later.

**Issue #135** ("Add some delay between the preload and dispense"; the owner: "500ms between the closure of the servo gate, and the preload. And enable only if the preload feature is enabled. (of course the servo gate must be enabled too)"): already in the code since `1952dfd` (2026-01-03, "Add a fixed delay before precharge"), in every release from v1.16 to v2.0.3: the gate's close waited for (its ramp's end), then 500 ms, then the precharge, only with the precharge and the gate on. Gate 3 kept the block word for word. Not shown on the bench: its unit has no gate fitted. The issue is the owner's to close.

**The verdict's wait** (the owner: "I want you to evaluate IF my original statement was correct"): the original waits 1000 ms ("Wait for other tasks to complete") before the verdict's reading.
- A wait is needed: powder still lands after the stop (2026-11-20's throws: the reading rose after the stop by 4.6 to 58.8 digits on average over three sets).
- Its reason does not hold: after the stop the gate's close is already waited for, and the precharge (or 20 ms); the motors stop within about 0.1 s; no task was found that needs the second.
- Its length is short on this scale at a slow finish: small amounts first show 0.8 to 1.6 s after they fall (2026-11-20, 24 releases of two digits: half after 1.17 s, a quarter after 1.5 s); this unit's step 1 records give a slowest lag of 0.99 s and a settle time of 0.57 s, 1.56 s.
- **The owner chose** the scale's own lag and settle time, no floor of 1 s ("Yes, go ahead").
- **After the unwind, on the unit** (2.0.3-218 at `d847a8b`, flashed by the probe; the back-off turned off again, live, for the rest of the test): `charge_cycle` passed in the original's order, the charge, the cup off and back, then the zero's wait and on by itself (states 1, 2, 3, 4, 1); the cup 53.96 gr after it.

## 2026-11-26, 01:33 to 01:38: the lifted cup's check dropped

- **The lifted-cup throw** (the owner at the bench, a 30 gr charge so the pour lasts): two throws ran to their end (31.38 gr in 4.53 s, 31.06 gr in 4.58 s, both over), the second started by itself once the cup was back: no "Cup lifted", the cup lifted only after each throw had stopped.
- **The owner:** "Why do we need to test cup lifted?" Answered from the record: the owner's words (2026-11-25, 14:28) were "Item 2 is the safety measure for the later powder charge controller."; the check (a reading under half the empty cup's weight in a throw stops both tubes and ends it) was my reading of them. Its reason: a cup lifted mid-throw reads as a charge far short, and the tubes would run on onto the bare pan; today's loop has the same exposure (its 10-below-zero check is only between throws). A simulated proof was offered (`sim weight -300`). **The owner: "Drop it."**
- **Dropped:** the check, its define and its event reason (`cup_lifted`) out of the firmware; the cup weight's description no longer names it; api_v1.md and the README's step 2 (its safety, gate 3's item 4, the proofs) say so. The cup's weight stays recorded by step 1, and the calibration's own lifted-cup stop (step 1, seen on the unit) is untouched. The charge's 10-below-zero check between throws is the original's.

## 2026-11-26, 01:45 to 01:47: the baseline's deadlock, found on the twin (no unit)

The owner, going to sleep: re-arrange what is left "based on 1. Real priority 2. Available resources", and the principle "the normal workpath is always the high priority, then the failure modes and exceptions". The README's "What is left, in order" puts first the forecast and the carry on the baseline profile, the bench's AR2209 profile as stored, which the owner set as gate 4's baseline. Done tonight with no unit and no one at the bench; the back-off put back to its stored value, the unit idle.

**Reproduced** (`tools/charge_sim/step2_nocarry.py`: the bench's profile, fine kp 2, kd 10, top 5 rev/s, coarse kp 0.02, kd 0.3, the back-off on; its step 1 records as stored): on the twin's two AR2209 rigs every throw ends before the coarse tube's quiet and none takes a weight a turn: 10 gr in 2.5 to 2.6 s, over by 1.0 to 2.0 gr; 30 gr in 5.0 to 5.4 s, over by 1.0 to 2.0. The unit's own (throws 5 to 7): 2.3 to 2.4 s, over by 1.3 to 1.6 gr at 10 gr.

**The cause, a deadlock:** the forecast's weight a turn is learnt only from readings after the coarse tube's quiet (its stop, the back-off, the slowest lag and the settle time: about 1.8 s here), so only a throw whose fine tube is still pouring then can teach it. With none known there is no forecast and no envelope (the fine tube held under what the forecast can still take), so the fine tube runs at its top to the end, the throw ends before the quiet, and none is ever learnt. The tuner cannot break it either: a window with no good throw costs infinity, and infinity against infinity is never cheaper, so it puts every trial back (`step2_nocarry_tuner.py`, 300 throws from kp 2: the f2 rig 0% good in all 12 windows, no weight a turn ever; the night rig stuck four windows, until one throw by chance ran slow enough, after which the forecast engaged and the tuner moved, good 8% then 24% then 36%). From kp 0.5 both rigs work from the first window.

**The root fix, tried** (`step2_nocarry_seed.py`, 50 throws a case): step 1's powder part already measures the fine tube's weight a turn where it streams (the prime's, `stream_gr_turn`), and today drops it. The carry seeded with it:

| Rig, target | As built | Seeded with step 1's |
|---|---|---|
| AR2209 night, 10 gr | 0% good, mean +1.655, sd 0.323, a good charge ∞ | 4% good, mean +0.357, sd 0.299, 58.8 s |
| AR2209 night, 30 gr | 0% good, mean +1.629 | 12% good, mean +0.226, 41.0 s |
| AR2209 f2, 10 gr | 0% good, mean +1.153 | 36% good, mean +0.018, sd 0.104, 8.3 s |
| AR2209 f2, 30 gr | 0% good, mean +1.121 | 36% good, mean +0.022, 16.0 s |
| BM2, 10 gr | 0% good, mean +1.043 | 36% good, mean −0.007, sd 0.094, 12.9 s |
| BM2, 30 gr | 0% good, mean +1.051 | 20% good, mean +0.007, 39.2 s |

Every throw seeded has a weight a turn; step 1's figure was within 1% of each rig's own (0.787 against 0.781, 0.807 against 0.810). With the costs finite, the tuner moves from the baseline (`step2_nocarry_tuner_seeded.py`, 300 throws): on f2 and BM2 a good charge costs 7 to 12 s within a few windows, 40 to 68% good; on the night rig it stays 70 to 96% over.

**A second gap, on the night rig** (`step2_nocarry_quiet.py`): the fine tube, fast on this profile, finishes within about the coarse tube's lag of its stop, so the coarse tube's powder still in the air is not yet shown, and the forecast counts only the fine tube's (the owner, 18:05: the predictor serves the fine tube alone). Tried: the fine tube held at its least, and no stop, until the coarse tube's quiet. Night rig: sd 0.323 to 0.098, good 6% to 30%, mean +0.260 to +0.042, a good charge 39.6 to 18.3 s, its time 2.4 to 5.5 s. On f2 it only costs time (7.6 to 16.2 s a good charge); BM2 about even (12.9 to 13.7 s).

**Options for the owner** (nothing changed):
- **(a) Recommended: the carry seeded from step 1.** The powder part keeps its measured weight a turn in the powder's profile, and a charge's first throw on that powder starts from it; each throw's own then renews the carry as built. It breaks the deadlock at its root, for any profile, from the first throw. Its cost: a field in the powder profile (a stored struct's layout, the loader keeping earlier records); a powder never calibrated has none and throws as today until a throw teaches one; step 1, signed off, keeps one more of its measurements.
- **(b) The tuner told what to do with no good throw** (a window with none compared by its spread, or kp moved down): no stored field, but the deadlock lasts until the tuner has walked kp slow enough, some windows of 25 throws each, and on f2 none came by chance in 300. Not simulated.
- **The second gap:** after (a), the unit's own throws show whether its powder is like the night rig's or the f2's; holding the fine tube until the coarse tube's quiet costs 2 to 3 s on every throw, so it is decided from the unit's data, not now.

## 2026-11-26, 01:50 to 02:10: step 2 built for the bench, proven on the twin (no unit)

**The owner, going to sleep (01:50):** "I won't sign off anything today. So, i want you to, similar to stage 1, build everything without my approval for stage 2 first, run the test in simulation, then present me the finished product, with your confident in simulation and pile up the validations on bench tomorrow, off one go." Every choice below is mine, made on the twin's evidence, for the owner to see before anything is signed off.

**Built:**
1. **The seed** (option (a) of 01:47; step 1's item 6). The fine tube's weight a turn where it streams, which step 1's powder part measured and dropped, is kept in the powder profile as `fine_weight_per_turn`, in the place of `compatibility` (used by nothing, 0 in every release: the stored layout as it was, no migration). A stored profile reads it as null, never calibrated (`nullable`, 0 held, as step 1's cup fields), so a settings copy from before the flash compares equal. The powder part's result shows it (the 12864's "A turn" row under the fine tube's, `mockups/calibration_screens/18.png`; the web's "Fine weight a turn", seen on the stand-in: "0.787 gr"; the API's result, `was` and `records.powder`), and Save stores it. The controller's carry, with no throw of its own yet, starts from it (`charge_controller_carried`); the throws' own replace it as before.
2. **The tuner's first trial** the way the first window misses (over on average, a slower kp; else a quicker one), in C and Python. Gate 1's studies keep the rule they ran with (always quicker first).
3. **Each throw's verdict on the console** (`Charge: throw N at rest, W of T in S s`) and `tools/charge_sim/bench/step2_windows.py`, which reads a session's log into gate 4's window report (each window's kp, median time, shares, mean, SD, histogram in 0.02 steps, time per good charge, the tuner's step) and ends with the mechanics' verdict (every throw on a carried weight a turn, every kept kp stored). Tried on a made-up log.

**The study** (`tools/charge_sim/step2_variants.py`; 4 rigs, from the bench's stored gains and from the owner's, 2 sessions of 300 throws at 10 gr with the tuner on). A good charge's cost, the last 100 throws of each pair of sessions (the first 50 in brackets), and the share over:

| From the bench's gains | As gate 1 built it | Seeded | Seeded, held until landed | Seeded, held until quiet |
|---|---|---|---|---|
| AR2209 night | 83.2 s (∞), 92% over | 12.8 s (39.3), 45% | 12.7 s (16.8), 36% | 16.0 s (20.6), 24% |
| AR2209 f2 | ∞ (∞), 100% | 9.5 s (16.7), 48% | 15.7 s (16.2), 20% | 14.9 s (14.9), 18% |
| BM2 | 17.6 s (∞), 63% | 11.7 s (17.8), 20% | 15.2 s (14.1), 22% | 16.4 s (16.4), 24% |
| AR2209 night, weight a turn falling with speed | ∞ (∞), 100% | 15.5 s (75.0), 73% | 16.9 s (16.9), 32% | 17.4 s (18.0), 42% |

From the owner's gains the seed's effect is mixed, as built against seeded: night 15.4 and 15.4 s; f2 14.0 and 16.1; BM2 14.4 and 17.9; the rig falling with speed 20.6 and 15.4. Two sessions each: not settled, and gate 4 starts from the bench's gains. The tuner's rule for two windows with no good throw (blind) never fired once the carry was seeded: its throws were the seed's, throw for throw.

**Decided (mine):**
- **The seed ships.** It ends the deadlock on every rig from the first throw, and it is a measured parameter given to the controller, not a change to how the controller works. From the owner's gains its effect is mixed (above).
- **The hold is not built.** At the tuner's objective it is no cheaper once settled (night 12.8 against 12.7 s; f2 9.5 against 15.7; BM2 11.7 against 15.2), and it adds 2 to 3 s to every throw. Its gain is the start from an over-heavy baseline (night 39 to 17 s, the falling rig 75 to 17 s) and a narrower spread on the night rigs (sd 0.257 to 0.136, and 0.355 to 0.149: fewer over, 45 to 36% and 73 to 32%). **For the owner:** "What we want" puts the spread first and the tuner's objective is the time per good charge; on the night rigs these two disagree about the hold. The bench's first two windows show which rig the unit is like (the README's next bench run).
- **The blind-window rule is not built:** with the seed it has nothing to do.
- **The tuner's first trial the way the first window misses** (checked with `first_dir.py` in the session's scratch, 12 sessions of 200 throws from the bench's gains):

| Rig | Window 2's cost: quicker first, the way it misses | A kept kp under 2 after window 2 | The last 50 throws |
|---|---|---|---|
| AR2209 night | 32.3 s, 19.0 s | 0 of 12, 9 of 12 | 11.1 s and 11.2 s; over 55%, 46%; mean +0.21, +0.07 |
| AR2209 f2 | 10.2 s, 9.2 s | 0, 2 | 10.3 s, 9.9 s |
| BM2 | 26.0 s, 13.5 s | 0, 8 | 12.3 s, 11.8 s |
| AR2209 night, falling with speed | 111.3 s, 17.7 s | 0, 11 | 14.6 s, 14.2 s; over 71%, 60% |

  It saves about one window (25 throws) of a wrong first step from an over-heavy baseline and ends where the built rule ends. On the bench each of those throws is the owner's hands, so it is built.

**Proven with no unit:**
- `tests/step2_sim.py`: `VERDICT PASS`. The C controller against the Python, call for call, 100 throws on each rig, the same stop on all; the C throwing alone from both starts as the Python; the tuner, given the Python's throws, the same steps in 12 windows from both starts on every rig.
- `tests/host_test.py` and `tests/calibration_sim.py`: `VERDICT PASS`.
- Both boards build with no new warning: the Pico 2 W's image 54.93% of its slot, RAM 56.22% at link; the Pico W's 99.87% of its slot (1,336 bytes free) and RAM 99.68% at link. The Pico W is the ADR's last ("Last: the Pico W"): its room is for AGENTS.md's order of remedies then.
- The web on the stand-in: the powder part to its result, "Fine weight a turn 0.787 gr", no console error.

**What the twin does not settle, and the bench must:**
- The unit's baseline is over-heavy (throws 5 to 7, +1.28 to +1.56 gr with no forecast), between the night rig and f2. The README's next bench run predicts its first two windows.
- On the rig whose fine weight a turn falls with speed, the tuner's kept kp wanders where few throws are good: one judged session of 300 throws from the bench's gains ended at kp 12.4 (most likely: at 1 or 2 good throws in 25 a window's time per good charge is mostly count noise, and a quicker kp then reads as the cheaper; not yet looked into). If the unit is like that rig, kp cannot cure its over, and the hold comes back to the owner.
- The throws get longer as they get better: from the bench's gains a throw takes 2.4 s and is over three times in four; on the night rig after 600 throws, 3.8 s and over less than half the time. The owner's words of 00:28 asked for shorter times and fewer over and under; on this baseline the twin gives fewer over at a longer throw, and a cheaper good charge.

## 2026-11-26, 02:26 to 03:04: real throws on the Pico 2 W with no one at the cup

**The owner (02:21):** "You can use the bench, But I won't be able to clean it later. so capture as much data as you can, then go back to the simulation." Asked whether the cup was empty (the cup tool still counted 352 gr of drops never given a word): "Empty now" (02:26). Then, the cup's record of 249.65 gr in mind (02:35): "You can go to 500 gr today. 225 was my random number testing the rotary nub capacity with the cup."

**On the unit** (FA0C, AR2209 in the hopper, the A&D FX-300i):
- The settings copied (`logs/adr009/step2/before_flash`), 2.0.3-222 (`df8ffee`) flashed by the probe, the copy compared: every stored field as before, `fine_weight_per_turn` null on every profile. `menu_tour` against its baseline: passed. `reach` and `charge_cycle` not run: each pours some 50 gr into the cup for no data.
- **49 real throws of 10 gr**, one a session (no one lifts the cup, so each charge ends after its throw): the scale zeroed over the API between throws (a charge starts only from zero), the fine kp 2 (the stored baseline) and 1.33 (the tuner's first trial from a window over on average) in ABBA order, the seed live at 0.752 (this bench's fine weight a turn, measured from the PC on 2026-11-25; step 1's powder part not run, to keep its powder for throws), the tuner off, the scale's frames tapped. Every throw reached its verdict. Afterwards the profile and the charge settings were reverted (nothing stored) and the settings compared again: passed. The cup holds 490 gr; the scale was zeroed with it, so once the cup is emptied it reads about −480 until zeroed.
- Kept: `tools/charge_sim/recordings/20261126_bench_ab/throws.json` (each throw's kp, the weight a turn it started on and left, its verdict, its time and its trace from the frames), the traces' sheet `research/step2_bench_ab_traces.png`; the sessions' logs in `logs/rc/` (not tracked).

| On the bench | n | Mean | SD | Good | Over | Under | Median time | A good charge |
|---|---|---|---|---|---|---|---|---|
| kp 2 | 25 | +0.001 | 0.128 | 28% | 32% | 40% | 3.87 s | 13.8 s |
| kp 1.33 | 24 | −0.009 | 0.087 | 29% | 46% | 25% | 4.67 s | 16.0 s |
| fast throws (stopped within 3 s) | 20 | −0.063 | 0.138 | 25% | 10% | 65% | 2.41 s | 9.6 s |
| slow throws | 29 | +0.037 | 0.057 | 31% | 59% | 10% | 5.08 s | 16.4 s |

**What it shows:**
- **The seed works on the unit.** Every throw stopped on the forecast. At `d847a8b`, on the same gains, three throws ended +1.28 to +1.56 gr over on the reading alone; now both arms are centred (+0.001, −0.009) with an SD of 0.09 to 0.13.
- **The unit is like the twin's f2 rig, not its night rig** (`tools/charge_sim/step2_bench_ab.py`, 6 sessions of 48 a rig): f2 gives kp 2 +0.002, SD 0.123, 31% over; f2 with the fine tube pouring by speed kp 2 −0.025, SD 0.119; the night rig +0.178, SD 0.396, 59% over. **The README's prediction for the next bench run, drawn on the night rig (78% over at the baseline), was wrong for this unit:** with the seed its baseline is not over-heavy.
- **Two kinds of throw.** A fast one stops at about 2.4 s with 0.24 to 1.12 gr still unseen and lands wide; a slow one finishes on the creep after the coarse tube's powder has landed and lands within ±0.06 of where the forecast put it, with a bias of +0.037. A slow throw is what holding the fine tube until the coarse tube's powder has landed (studied, not built) would make of every throw. At the user's tolerance of 0.03 its bias puts most of them over, so by the tuner's objective it is the dearer kind (16.4 s against 9.6 s a good charge) although its spread is less than half.
- **The carry drifted from the seed:** from 0.752 to a median of about 0.92 (0.85 to 1.29), each renewal from a slow throw's creep; one throw's own was near 1.7. Either the fine tube gives more a turn at the creep than at the prime's stream (the twin's "fine by speed" variant has it so), or the coarse tube's late powder is counted as the fine tube's. As it rose, more throws ended slow (median 2.51 s in the first 24, 4.61 s in the last 25).
- **The tuner's first trial on the unit:** kp 1.33 narrower than kp 2 (SD 0.087 against 0.128) but no cheaper a good charge (16.0 against 13.8 s), within the noise of 25 throws. The tuner would most likely put it back.
- **Powder after a zero:** in the second session about 0.2 gr arrived some 1.1 s after the charge started, the scale zeroed just before; the charge waited off zero and the API zeroes only a drift then (0.1 gr). Once in 49. Not known why: powder perched after a fast throw shaken loose, or the coarse motor snapping to a step at power-up (a fiftieth of a turn is about 0.2 gr here).
- **What it costs** (`stats`, the Debug build at `-O0`): the controller's tick 49 µs on average, 1,764 at most; a reading 1,831 µs on average, 11,024 at most (the loop ticks each millisecond, so the stop can be that late after a reading); the heap's low mark 53,936 bytes. With the scale tapped, the Scale Task's stack had 96 bytes never touched.

**The cup tool:** a drop between runs is now seen from 1 gr, not 20: each throw's zero of about 10 gr went past 20 unseen (`rc_run.py`, `CUP_EMPTIED_GR`).

**The owner, after (03:01):** "Make sure you are not just tuning the Kp, consider Kd as well. If you don't know how, then do some research on how to auto tune the PID.." The owner's answer of 2026-11-25, 17:40 was already "its kd is tuned with its kp"; gate 1 left kd out on the reading that on the forecast the D term only scales kp down. That holds while the fine tube pours steadily (the speed is kp ÷ (1 + kd × the weight a turn), about kp ÷ 8.5 at kd 10), but not when powder the forecast does not hold lands: the coarse tube's, the second gap. There the D term brakes the fine tube by itself, and the bench's two kinds of throw may be that braking or its absence. Next: kd in the simulation.

## 2026-11-26, 03:01 to 03:26: kd, and whether any tuner improves this unit (the twin, no unit)

**The owner (03:01):** "Make sure you are not just tuning the Kp, consider Kd as well. If you don't know how, then do some research on how to auto tune the PID.."

**What the D term is in the code.** The derivative is taken per millisecond: the original's `error_derivative` ((error − last error) / elapsed ms, v2.0.3), kept by the controller (`derivative_of`) and the twin. On the forecast, a fine tube pouring steadily makes the forecast rise by the weight a turn k times the speed, so the speed asked is kp × error ÷ (1 + kd × k ÷ 1000). At every stored profile's kd (10 to 15) that divisor is 1.01: **the fine tube's D term has been close to inert, in v2.0.3 as now.** On a landing (0.3 gr of the coarse tube's over 0.3 s) kd 10 asks 0.01 rev/s less.

**kd mapped** (`tools/charge_sim/step2_kd_map.py`; the bench's other gains, the seed on, the tuner off, 2 sessions of 60 throws a cell):
- kd 0 to 40: every row flat within the noise, on the four rigs.
- kd 100 to 10,000: kd acts by itself. At an equal steady gain it brakes the fine tube on what the forecast does not hold: a slower, narrower throw. The night rig, steady gain about 1:

| kp, kd | A good charge | SD | Median time |
|---|---|---|---|
| 1, 0 | 12.1 s | 0.193 | 3.8 s |
| 2, 1,000 | 15.0 s | 0.152 | 5.4 s |
| 4, 3,000 | 14.9 s | 0.123 | 5.8 s |
| 8, 10,000 | 14.6 s | 0.102 | 7.3 s |

  f2 and BM2 the same way (f2: kp 2, kd 0, SD 0.101 in 2.7 s; kp 4, kd 1,000, SD 0.076 in 4.8 s). So kd trades time for spread, and by the time per good charge that trade is about even.

**Built, in the twin only:** the tuner's axes `kp+kd` (`step2_tuner.Tuner(axes=...)`): a compass search on two axes, the steady gain (kp moved, kd held, as today) and the braking b = 1 + kd × k ÷ 1000 (kd moved, kp with it so the steady gain holds; dimensionless, so grams and grains alike; never under 1, kd never negative). A trial put back is tried the other way, then on the other axis; all four put back, the step shrinks and the kept gains rest. With the axis `kp` it is the tuner as built, step for step (the C judge passes, below). The C is not changed.

**Methods weighed** (the owner: "research on how to auto tune the PID"):
- **Ziegler–Nichols and relay tuning (Åström–Hägglund)** need a loop that can be made to oscillate about a set point; a throw is a one-way pour that ends.
- **Model-based rules for an integrator with dead time (Skogestad's SIMC)** give a start from step 1's weight a turn and lag. The first build's offer was of that kind and gave more overs.
- **Batch-to-batch search on the measured cost** is what fits a throw: the tuner as built (one axis), a compass search (two), SPSA or Bayesian optimisation. The limit found tonight is not the search but what it is judged by: a window of 25 throws at a tolerance of 0.03 gr holds 6 to 12 good throws, ±30% of noise, against differences of 10 to 20% across a flat region.

**The tuners against no tuner** (`step2_variants.py`, 3 sessions of 600 throws a case; the last 100 throws: a good charge, SD):

| From the bench's stored gains | No tuner | kp | kp+kd | kp, the share modelled | kp+kd, modelled | kp, a margin of its noise (z 1) |
|---|---|---|---|---|---|---|
| AR2209 f2 | 11.3 s, 0.133 | 11.1 s, 0.209 | 12.6 s, 0.067 | 8.6 s, 0.120 | 9.9 s, 0.108 | 11.4 s, 0.123 |
| f2, the fine tube pouring by speed | 10.6 s, 0.095 | 10.2 s, 0.108 | 10.8 s, 0.099 | 10.7 s, 0.055 | 10.6 s, 0.086 | 11.5 s, 0.107 |
| AR2209 night | 54.4 s, 0.341 | 9.8 s, 0.227 | 9.6 s, 0.217 | 22.9 s, 0.151 | 15.1 s, 0.155 | 78.5 s, 0.357 |
| BM2 | 19.0 s, 0.135 | 11.0 s, 0.097 | 12.0 s, 0.114 | 11.5 s, 0.101 | 13.6 s, 0.081 | 9.9 s, 0.101 |

From the owner's gains (kp 0.8, kd 15, top 2 rev/s) no tuner gives the narrowest throws (SD 0.032 to 0.067 in 10 to 18 s); every tuner makes them quicker and about twice as wide (f2: SD 0.037 to 0.086, 10.4 to 7.7 s; over and under 22% to 46%), its time per good charge lower on three rigs of four.

**What it shows:**
- **On a unit like the bench's, the seeded stored gains are already where the cost is flat.** No tuner clearly beats leaving them; on f2 the kp tuner as built ended wider than no tuner (SD 0.209 against 0.133). The tuners earn their keep only from a baseline far off (the night rig: 54 to 10 s).
- **kd buys nothing by the time per good charge:** the two-axis tuner ends anywhere from kd 0 to 4,260, the walk of a search in a flat direction.
- **By the owner's check of 00:28** ("shorter time to trickle, and less over and undershoot"), no tuner does both here: from a narrow start they buy time with spread; from the bench's start the twin shows no trend a bench series could read in a few windows.

**Decided (mine):** kd's tuning is not built into the firmware. It stays a study (`axes="kp+kd"`), ready if the objective changes. The C judge after the Python's change: `tests/step2_sim.py` VERDICT PASS (its first run failed on the note's words alone, "every way" for "both ways", each window to the same kp; the one-axis words put back).

## 2026-11-26, 09:17 to 09:42: the tuner judged by every throw's weight, kp and kd (the twin, no unit)

**The owner (09:17):** "You can do all sort of attempt in simulation. Also, the cup is cleared, but I'm not around right now so I cannot clear the cup later. So I would like you to prioritize on the simulation based on data already." No unit was touched.

**The twin against the bench's 49 throws** (`step2_bench_ab.py`, its throws now read from the recording; 6 sessions of 48 a rig):

| | kp 2: mean, SD, time | kp 1.33: mean, SD, time | Fast throws | Slow throws: mean, SD | Carry, second half |
|---|---|---|---|---|---|
| The bench | +0.001, 0.128, 3.87 s | −0.009, 0.087, 4.67 s | 41% | +0.037, 0.057 | 0.925 (seed 0.752) |
| f2 | +0.002, 0.123, 2.96 s | −0.015, 0.064, 4.93 s | 29% | −0.008, 0.070 | 0.847 (seed 0.807) |
| f2, the fine tube pouring by speed | −0.025, 0.119, 4.33 s | −0.002, 0.059, 5.15 s | 12% | +0.001, 0.057 | 0.810 |
| night | +0.178, 0.396, 2.41 s | +0.042, 0.267, 2.48 s | 80% | +0.091, 0.390 | 0.924 |
| AR2209 (11-21), 1122 | +0.326 and +0.071 at kp 2 | | 85%, 20% | | |

f2 stays the twin nearest the unit; f2 by speed and night bracket it. Two of the unit's ways the twin does not have: its slow throws land +0.037 over (the twin's centred), and its carry rose 23% above its seed (the twin's 5%). Tried and dropped: (a) the forecast's lag too short at the creep (the table's slowest point is 0.24 gr/s at 0.84 s): no, a slow throw's last powder showed a median 0.54 s after its stop, and what came after 0.84 s averaged 0.005 gr; (b) the fine tube giving more a turn at the creep (a variant, 1.2 times at 0.1 rev/s): no, the twin's own step 1 then seeds from the creep, its carry rose 8%, and on f2 every throw went slow (the unit's 41% fast). The variant was taken out again.

**The landscape and the judge** (`step2_objective_study.py`: fixed gains, the seed on, 200 throws a cell; the judge from 600 at the bench's gains and each neighbour):
- On f2 the cost of a good charge, its share modelled, is least at kp 2, kd 10: the bench's stored gains (8.6 s; 3 of 28 cells within 15%). More kd costs there (kd 300, 11.6 s; 3,000, 14.0 s): it buys spread with time the objective does not pay back.
- On night kd matters: at kp 2, 53 s at kd 10 to 15 s at kd 3,000 (SD 0.333 to 0.067).
- A window of 25 against the pool of 75 picked the truly cheaper of the bench's gains and a neighbour (kp 1.33 or 3, kd 300 or 1,000) 56 to 80 times in 100 by the count of its good throws on f2 and f2 by speed, 71 to 92 by the share modelled; 71 to 100 and 98 to 100 on night. **Counting is what made the tuner wander:** its decisions between near gains were not much better than a coin.
- Counting and modelling disagree where the throws split into fast and slow: on night counting finds kp 1, kd 10 best (9.3 s, SD 0.181, 30% of throws good and the rest far off), modelling kp 2, kd 3,000 (SD 0.067).

**The tuners against no tuner** (`step2_variants.py`, 4 sessions of 600; the last 100 throws, a good charge with its share modelled, s):

| From the bench's stored gains | No tuner | kp, counted (as built) | kp, modelled | kp and kd, modelled |
|---|---|---|---|---|
| f2 | 12.6 (SD 0.124) | 23.4 (0.194) | 12.6 (0.120) | 11.7 (0.127) |
| f2 by speed | 11.9 (0.096) | 12.7 (0.108) | 11.1 (0.072) | 11.3 (0.076) |
| night | 60.4 (0.340) | 23.6 (0.212) | 31.2 (0.162) | 20.5 (0.159) |
| BM2 | 20.7 (0.133) | 13.4 (0.092) | 12.6 (0.095) | 14.5 (0.092) |

From the owner's gains (kp 0.8, kd 15, top 2 rev/s): no tuner 14.2, 14.7, 18.1, 26.9 s (SD 0.036, 0.038, 0.063, 0.032); kp and kd modelled 14.8, 12.2, 17.1, 16.1 s (SD 0.071, 0.062, 0.064, 0.052).

**Decided (mine, on the owner's "all sort of attempt"):** the tuner ships judging by the share modelled from every throw's weight, on two axes, kp and kd (the braking). Of the four it is the one that never does badly: at worst 4% dearer than leaving the gains (f2 from the owner's), 1.4 to 3 times cheaper from a start far off, and the only one that cures the night rig's landing powder. **The owner's to see:** from narrow hand-tuned gains it still trades spread for time (SD 0.036 to about 0.07, a throw some 25% quicker): at a tolerance of 0.03 gr the owner's objective says that pays.

**Built:**
- `charge_tuner.c/.h`: two axes (the steady gain, the braking 1 + kd x k / 1000, never under 1), a compass search (the other way, then the other axis, then a smaller step), the first way on each axis the one the throws miss by, the share modelled (a sort of 75 at most on the stack). `erf` is the tuner's own (Abramowitz and Stegun 7.1.26, under 1.5e-7) on the SDK's `exp`: libm's `erf` and its `exp` are placed in RAM on the Pico W, 2.6 KB it has not got. The Python uses the same expression.
- `charge_mode.cpp`: a trial's kd from RAM as its kp; the tuner afresh when the profile's kp or kd is changed elsewhere; both kept gains written and stored once a window; the throw's start line and the `Tuner:` line name kd. `step2_windows.py`, `step2_traces.py` and `step2_bench_ab.py` read both forms.
- The switch's description, api_v1.md and the README's tuner say Kp and Kd.

**Proven with no unit:** `tests/step2_sim.py` VERDICT PASS (the controller call for call; the tuner the same steps in 12 windows from both starts on every rig, kd moving on three: 636, 446, 0); `tests/host_test.py` and `tests/calibration_sim.py` PASS; the Pico 2 W's image 55.06% of its slot, RAM 56.23%. The Pico W's misses its slot by 1,008 bytes (RAM 99.69%): **the owner (09:37): "Also, don't build for Pico W yet. That's the optimization domain."** Left to that work; CI builds both boards, so nothing is pushed meanwhile.

**Sheets, as the bench's of 02:26** (the owner, 09:37: "Can you plot the result of the simulation of charge weight versus time diagram similar to what you plotted last night?"; `tools/charge_sim/step2_twin_traces.py`): `research/step2_twin_ab_f2.png`, the bench's A/B on f2 (one session of 48: kp 2 SD 0.064, kp 1.33 SD 0.046, narrower than the six sessions' 0.123 and 0.064 and the bench's 0.128 and 0.087; the same two kinds of throw); `research/step2_twin_tuner.png`, the tuner from the bench's gains, its first window and its 24th: f2 SD 0.106 to 0.076 (kp 2 to 1.81), night mean +0.213, SD 0.260 to mean +0.058, SD 0.185 (a trial at kp 1.20, kd 191).

## 2026-11-26, 09:45 to 09:49: never cut the throw early (the twin, no unit)

**The owner (09:45):** "Can you also run the simulation without the predictor (perhaps just don't cut the throw)? So we basically push all potential failure mode to overcharge? And will it help improving the grouping?"

**Built in the twin only** (`step2.Step2(cut=...)`; the C unchanged, `cut="forecast"` the stop as before, the judge's Python the same): what stops the fine tube. `forecast`, the moment the forecast reaches the target (as built); `reading`, the forecast still drives the PID and its envelope but the throw is never cut early, the fine tube creeping at its least until the reading itself reaches the target; `none`, no forecast at all, the PID on the reading, stopped once the reading reaches the target.

**Run** (`tools/charge_sim/step2_cut_study.py`: fixed gains, no tuner, the seed on, 4 sessions of 150 throws at 10 gr):

| Rig, start | Stop | SD | 5% to 95% | Mean | Worst | Under | Median time (95%) |
|---|---|---|---|---|---|---|---|
| f2, the bench's gains | forecast | 0.135 | −0.17 to +0.19 | +0.014 | +1.49 | 34% | 2.8 s (6.0) |
| | reading | 0.098 | −0.00 to +0.23 | +0.075 | +1.00 | 0% | 5.0 s (9.4) |
| | none | 0.086 | +0.99 to +1.28 | +1.141 | +1.54 | 0% | 2.6 s (2.7) |
| f2, the owner's gains | forecast | 0.072 | −0.04 to +0.07 | +0.010 | +1.40 | 9% | 10.2 s (13.1) |
| | reading | 0.042 | −0.01 to +0.09 | +0.031 | +0.58 | 0% | 11.3 s (14.7) |
| | none | 0.115 | +0.01 to +0.20 | +0.086 | +1.96 | 0% | 6.5 s (7.7) |
| f2 by speed, the bench's gains | forecast | 0.186 | −0.20 to +0.35 | +0.044 | +1.37 | 30% | 4.1 s (6.5) |
| | reading | 0.111 | −0.01 to +0.22 | +0.066 | +1.12 | 0% | 5.7 s (9.0) |
| f2 by speed, the owner's gains | forecast | 0.080 | −0.03 to +0.09 | +0.019 | +1.61 | 5% | 10.5 s (14.7) |
| | reading | 0.042 | −0.01 to +0.09 | +0.029 | +0.49 | 0% | 11.3 s (14.8) |
| night, the bench's gains | forecast | 0.380 | −0.11 to +1.12 | +0.468 | +1.77 | 10% | 2.4 s (2.5) |
| | reading | 0.284 | +0.01 to +0.85 | +0.335 | +1.52 | 0% | 2.6 s (10.0) |
| night, the owner's gains | forecast | 0.054 | | +0.013 | +0.44 | 11% | 9.7 s |
| | reading | 0.045 | | +0.035 | +0.43 | 0% | 11.0 s |
| BM2, the bench's gains | forecast | 0.142 | | +0.038 | +0.95 | 32% | 3.9 s |
| | reading | 0.150 | | +0.150 | +0.77 | 0% | 4.4 s |
| BM2, the owner's gains | forecast | 0.036 | | +0.018 | +0.57 | 1% | 17.8 s |
| | reading | 0.038 | | +0.023 | +0.69 | 0% | 18.2 s |

(`none` from the bench's gains on f2 by speed, night and BM2: +1.05 to +1.63, every throw over.)

**What it shows:**
- **No forecast at all does not help:** from the bench's gains everything in the air lands on top, +1.1 to +1.6 gr, the unit's own throws before the seed (+1.28 to +1.56). The forecast's slowing of the approach is what makes the creep land close.
- **Never cutting early helps the grouping on the AR2209 rigs like the unit:** the SD falls by 27 to 48% on f2 and f2 by speed from both starts, every under becomes an over, and the worst over shrinks. Its price: a steady offset of +0.03 to +0.07 gr and 1 to 2 s a throw (the 95% time 3 s more from the bench's gains: a creep at the fine tube's least, 0.0336 rev/s on the bench, to close a gap the forecast had counted on).
- **Not where the coarse tube's powder is the trouble** (night from the bench's gains: still SD 0.284, mean +0.335), **nor on BM2.**
- **The bench's own throws agree:** its slow throws are the ones the forecast did not cut early (they finished on the creep after the coarse tube's powder had landed), SD 0.057, mean +0.037; its fast throws, cut early on the forecast, SD 0.138, mean −0.063, nearly all its unders.
- **By the tuner's objective** (time per good charge at ±0.03) it is dearer (f2 from the bench's gains 9.5 to 13.6 s): the offset pushes throws past the tolerance. By "What we want" ("its spread narrow first, then its time short (a steady offset matters less than a wide spread)") it is the better stop.

The sheet: `research/step2_twin_cut_f2.png` (`step2_twin_traces.py cut`): the three stops on f2 from both starts, 40 throws each. One session is noisier than the study: from the bench's gains its never-cut SD is 0.106 against the forecast's 0.105, one clump landing +0.55 after a 16 s creep.

**Not built into the firmware:** it changes how the controller stops (a mechanism, not a parameter), so it is the owner's decision; with it, the tuner's judgement and the steady offset follow (below, "What is left").

## 2026-11-26, 09:51 to 10:05: never cut the throw early, built (no unit)

**The owner (09:51):** "Yes, build it" (the stop only; the two choices it brings are still the owner's).

**Built:** `charge_controller.c`, both of the fine tube's stops (each millisecond, and each reading) on the reading reaching the target instead of the forecast; the PID and its envelope still on the forecast, so past the forecast's target the fine tube creeps at its least until the reading shows the target. The header says so. `tests/step2_sim.py`'s product: `cut="reading"`.

**Proven with no unit:** `tests/step2_sim.py` VERDICT PASS (the C's stop the Python's on every throw of the four rigs, the tuner the same steps in 12 windows from both starts; under 0% on every rig, alone); `tests/host_test.py` and `tests/calibration_sim.py` PASS; the Pico 2 W's image 55.06% of its slot. The Pico W not built (the owner, 09:37).

**The tuner with it** (`step2_variants.py`, variants `fixed+reading` and `shipped`; 4 sessions of 600, the last 100 throws, SD and mean):

| From the bench's stored gains | No tuner, cut on the forecast | No tuner, never cut | The tuner, never cut (as it ships) |
|---|---|---|---|
| f2 | 0.124, +0.032 (under 31%) | 0.110, +0.080 | 0.071, +0.048, no under, 7.0 s |
| f2 by speed | 0.096, −0.015 | 0.108, +0.084 | 0.105, +0.057 |
| night | 0.340, +0.391 | 0.275, +0.291 | 0.139, +0.089 |
| BM2 | 0.133, +0.046 | 0.133, +0.141 | 0.047, +0.034 |

From the owner's gains: no tuner, never cut, SD 0.034, 0.040, 0.047, 0.026 (11.1 to 18.2 s); the tuner, never cut, 0.047, 0.060, 0.059, 0.059 (9.3 to 12.3 s): it still trades the spread for time from narrow gains. With the stop on the reading the tuner and the steady offset do not fight: the tuner takes the offset down (f2 +0.080 to +0.048) and, from the bench's gains, groups tightest on three rigs of four.

**Still the owner's:** whether the steady offset (+0.03 to +0.09 gr) is aimed off; whether the tuner judges the spread with the offset forgiven, or protects narrow hand-tuned gains.

## 2026-11-26, 10:33 to 10:44: aiming off the steady overshoot, tried and not taken (the twin, no unit)

**The owner (10:33):** "Yes try it, but don't stress yourself too hard. Everyone has different setup, and some larger kernel powder is hard to dispense precisely. And PID is the algorithm the current users are using, we are just trying to improve it a little, so there is no hard requirement for you to improve massively."

**Tried** (`step2.Step2(aim=...)`, twin only; `step2_cut_study.py --stops ...`): each throw's landing past where it aimed, carried in RAM as the weight a turn is (the median of the last five), never aiming above the target.
- `median`: the reading's stop short of the target by that much, the forecast's PID still on the target;
- `whole`: the fine finish's whole target short by it, the PID too, so the tube creeps when the reading stops it.

| f2, the last throws: SD, mean, under, time | Never cut (as built) | Aim-off at the stop | Aim-off of the whole target |
|---|---|---|---|
| Fixed, the bench's gains | 0.098, +0.075, 0%, 5.0 s | 0.118, +0.022, 26%, 2.9 s | 0.100, +0.018, 27%, 5.2 s |
| Fixed, the owner's gains | 0.042, +0.031, 0%, 11.3 s | 0.046, +0.010, 12%, 10.2 s | 0.044, +0.009, 12%, 11.6 s |
| The tuner, the bench's gains | 0.105, +0.089, 0%, 5.8 s | 0.113, +0.012, 28%, 4.7 s | 0.117, +0.017, 28%, 3.0 s |
| The tuner, the owner's gains | 0.059, +0.040, 0%, 9.5 s | 0.064, +0.012, 18%, 7.9 s | 0.059, +0.013, 14%, 9.5 s |

The other rigs the same way: the aim-off centres the mean (to +0.01 to +0.04) and lifts the good share a little, brings back 10 to 44% unders, and widens the grouping almost everywhere (night from the bench's gains 0.284 to 0.346 or 0.427; with the tuner 0.117 to 0.248). Aiming at the stop alone stops the throw before the creep, the early cut taken out at 09:51; aiming the whole target keeps the creep, but with the tuner on it spends the margin the offset gave on speed.

**Not taken (mine):** it works against both things the stop was built for, the grouping and every miss an over. The throw stays never cut, its steady offset (+0.03 to +0.09 gr on the twin) left as it is; the code stays in the twin as a study. Nothing in the firmware changed.

## 2026-11-26, 10:47 to 10:59: the interim decision written down

**The owner:** "Let's talk before you test. So far, what is your recommendation and how does that look like?" (10:47); "Let's write down our understandings and interim decision you just explained." (10:58). Written as the README's "Interim decision (2026-11-26)": step 2 as it stands at `1fbac83`, what we understand, how it looks on the twin (`research/step2_twin_product_f2.png`: the product on f2 from the bench's and the owner's gains, first window and window 24), what a user sees, how sure, what is still the owner's, and the next bench run with its passes. Nothing signed off; no unit touched.

## 2026-11-26, 10:58 to 11:20: the use cases mapped and run on the twin (no unit)

**The owner (10:58):** "I want you to also try more experiments, for failure modes, or out of ordinary exceptions, like: 1. Starting with the absurdly wrong PID 2. Someone accidentally put integral term 3. Different powder? I want you to map all use cases against your simulation, out of the known experiment so far, then conduct the experiment with your simulation."

**Built:** `tools/charge_sim/step2_cases.py` (35 cases on step 2 as it ships) and six assumed variants in `rigs.py` (`big_kernel`, `gram_digit`, `draught`, `missing`, `fine_empty`, `coarse_empty`; variants now chain with "+").

**The map and its verdicts:** `research/step2_use_cases.md`. The normal path holds over 3 to 60 gr, ball and stick powder, grams and tolerances of 0.02 to 0.1 (SD near 0.05, no under); every failure fails towards over; lumpy powder degrades most (SD 0.18 with the tuner). Fails: an empty fine hopper never ends a throw (no limit in the charge mode, as in v2.0.3), an absurd kp (200) pins the fine tube to its envelope where the tuner cannot see its trials, a wild coarse kp overshoots every throw by about 2 gr. Degraded: an integral term (ki 0.01 to 1: mean +0.07 to +0.2). A powder never calibrated starts as the old build (its first 50 throws +0.87, every one over) and recovers within some 100 to 200 throws; the interim decision's line said it stays so, corrected. Four proposals for the failure modes' turn, none built.

## 2026-11-26, 11:20 to 11:54: the failure modes first (the twin, no unit)

**The owner (11:20):** "I'm not around at home, so I'd like you to fix failures first (based on available resource). This is the exception of the workflow I proposed yesterday so do not record as the rule." Recorded as that: today's exception to the normal path first, the rule unchanged.

**An empty hopper** (`f34ab17`): the controller stops a starved throw (`starving`, `CHARGE_STARVED_TURNS` 5 and `CHARGE_STARVED_SHARE` 0.1: after the coarse tube's stop, with a weight a turn known, each 5 of the fine tube's turns shown must raise the reading by a tenth of what they carry), and the charge ends on the fault "Fine: no powder", as a lost scale's, waiting to be acknowledged. Chosen on the twin over whole tuned sessions (`starve_tune.py` in the session's scratch): at 3 turns and a quarter big kernels tripped 9 times in 1,800 throws (a carry run high, 1.06 to 1.21 against 0.81, and one true stall before a 3.9 gr clump); at 3 and a tenth once; at 5 and a tenth never, in 9,000 throws on five rigs; an empty hopper stops in 5.8 to 6.7 s. `tests/step2_sim.py` judges an empty-hopper rig too (calibrated full): every starved stop at the same reading in the C and the Python. A first run differed by one reading on one throw: at a steady 5 rev/s the parcels are 0.05 turn and the window ends on its turns exactly, the C's parcels in floats; a millionth of a turn's room in both.

**An integral term** (`b62eb2c`): a profile with a ki has the tuner's first trial without it, judged as any trial and kept only if cheaper; the charge mode carries ki as kp and kd and stores it with them. Four sessions: ki 1 from SD 0.169 to 0.084, ki 0.1 0.082 to 0.089, ki 0.01 0.132 to 0.092; ki dropped in 9 of 12 sessions. The judge has a start with ki 0.1: the same steps.

**Proven with no unit:** `tests/step2_sim.py` VERDICT PASS (750 throws at the same stop, 150 of them starved; the tuner the same steps from three starts on five rigs); `host_test` and `calibration_sim` PASS; the Pico 2 W's image 55.12%. The Pico W not built (the owner, 09:37).

**Left for the owner:** a wild coarse tube (options in `research/step2_use_cases.md`: leave it as today, mine; a forecast on the coarse tube, against the direction of 2026-11-25, 18:05; the tuner on the coarse kp too); an absurd kp, safe, left.

## 2026-11-26, 12:03 to 12:50: grams, and the coarse tube's two knobs (the twin, no unit)

**The owner (12:03):** "Two more failures modes That I can think of and perhaps that should be covered in your failure modes. 1. If a scale reports gram, while the default parameter is grain, then I need a reliable way to tune down to support gram, with the auto tuner. Noted the calibration must NOT assume the unit. 2. Coarse trickler can be tuned two ways: Increase the stop threshold, or tune the coarse trickler PID. Which one do you prefer? Noted none of the failure modes are new, and people are still confused with the units and conversion. OpenTrickler controller is built without units, but the default profiles are assuming the unit is grain."

**A twin in grams** (`rigs.py`'s `grams`: every weight of the rig divided by 15.4324, the digit the A&D's 0.001 g). Step 1's method on it finds the same least speeds, lags within a tenth of a second of the grains' and a weight a turn of 0.052 g: the method assumes no unit. Around it, five constants do (`calibration.c`, `calibration_method.c/.h`, and `calibration.py`'s two): `CALIBRATION_STEADY_GR` 0.04 (steady within 0.04: two of the A&D's grain digits, forty of its gram digits), `CALIBRATION_BACK_GR` 0.5 (the cup back within 0.5 of zero: 7.7 gr in grams), `CALIBRATION_LIFTED_GR` 2 (still sound: any cup weighs more), `POUR_PER_TURN_MOST` 12 (the cup's check before a tube has poured: 15 times too loose in grams), `CALIBRATION_DEFAULT_CAPACITY` 100 (the cup's capacity when its part never ran: 1,543 gr in grams). Not changed: the owner's requirement, with a choice to make (below).

**Step 2 in grams with the grain defaults:** safe and slow, 49 s a throw (the use cases' map). The tuner as it was climbed out only partly in 300 throws with a gram tolerance (15.3 s), and fast only under the grain default's tolerance, fifteen times too loose in grams.

**Built** (`8806f78`): **the gains put right** on the tuner's first window. The steady gain kp / braking times step 1's weight a turn and middle lag has no unit: the bench's stored gains 1.3, the owner's 0.5, grains' gains in grams 0.08. Out of 0.2 to 5 the first trial brings it to 1, an integral term taken out with it (replacing the integral's own trial, `b62eb2c`); kept only if cheaper, and the search then goes the way its own throws miss by (a first build took the way from the window of the gains put right, and the search ran quicker and wider: kp / 100 SD 0.106 over four sessions; from its own throws 0.064). On the twin: grain gains in grams to 6.9 s from the first trial, SD 0.008 g against 0.004 g with everything converted; kp / 100 SD 0.064 at 7 s (the reference 0.057); kp x 100 0.138 (0.172 before); the references unchanged (their gains in range). `tests/step2_sim.py` judges it from a kp / 100 start too: VERDICT PASS. Pico 2 W 55.16%; `host_test`, `calibration_sim` PASS.

**The coarse tube's two knobs** (the wild coarse tube, coarse kp 0.2, no tuner): raising the stop threshold from 5 to 7 brings it to +0.076, SD 0.072 in 3.9 s, quicker than the bench's careful coarse kp of 0.02 (4.9 s); lowering the coarse kp needs 0.05 or less for the same (+0.066, 5.1 s). **Mine: the threshold.** It acts one way (too much only costs the fine tube's time, too little overshoots), what it must cover is seen on every throw (the coarse tube's powder still in the air at its stop), and the coarse PID works on a reading a lag behind, the hard part to tune; the owner kept the coarse tube today's (2026-11-25). Not built.

**Left in grain numbers on a scale in grams:** the coarse gains and the coarse threshold (5 is 5 g: the coarse tube never runs on a charge under it), the tolerance (0.03 is 0.03 g) and the zero's margins: the user's settings, their defaults grains'. With the calibration's five, the owner's choice of how to make them unit-free (below).

## 2026-11-26, after 12:50 to 13:33: units are the user's; the margins; the coarse tube's two knobs again (the twin, no unit)

**The owner (after 12:50):** "Not all scale can report unit, so for safety and liability sake I'd leave the decision to the user. Few more questions: can steady margin and zero margin be learned from calibration? Currently it is hard coded as default value in the firmware. I don't understand your conclusion on coarse trickler decision, can you explain a bit more to me, on which method works better, i.e,. between raising the stop threshold versus better tune of PID."

**Units: the user's.** The firmware takes no unit from the scale and converts none: what the user sets is in the scale's own numbers, and what the unit learns (step 1's records, the tuner's gains put right) is measured in them. Nothing to build.

**The margins, measured** (the bench's 49 throws of 02:26 to 03:04, `tools/charge_sim/recordings/20261126_bench_ab/throws.json`): the A&D's smallest step is 0.02 gr; at zero before each throw, 49 of 49 read exactly 0; at rest, from 2 s after the stop to the trace's end (1 s each), 45 of 49 never moved, SD at most 0.0099. A first count over each trace's last 3 s gave SDs up to 0.23: that window held powder still landing, not the rest. Today's 0.02 for both margins is one digit of the A&D in grains and fits that scale alone. In grams (0.001 g) it is 20 digits: a zero 0.3 gr off passes, and so does a reading still settling. On a scale of 0.1 gr digits a flicker of one digit has an SD up to 0.05, and the zero's wait does not end while it flickers.

**Can step 1 learn them: yes. Proposed, not built.** Its scale part already finds the scale's step in each watch to rest (the smallest change it sees, `calibration_method.c`) and judges the rest by it, but keeps none. Proposed: it keeps the step and writes each margin as one step into the charge's settings, as it writes its other records (the user's to change after). One step: a flicker of one digit has an SD of at most half a step and passes; a reading still climbing a digit a reading has about three and does not; a zero is within one digit. On the A&D in grains that is 0.02 and 0.02, today's values: the bench changes nothing. For a scale noisier than its digit, the larger of one step and twice the spread step 1 sees at rest. Found with it: the charge mode takes the cup as lifted on a reading below -10 (`charge_mode.cpp`, the cup's removal), a grain number too; a cup under 10 g on a scale in grams is never seen lifted. Step 1's cup part sees the cup lifted, and its weight could set that test the same way.

**The coarse tube's two knobs again** (`tools/charge_sim/step2_coarse_study.py`: coarse kp 0.02, 0.05, 0.1 and 0.2 by threshold 3, 5, 7 and 9, at 10 and 30 gr, the bench's fine gains, no tuner, 2 x 40 throws, the last 35 judged; the sheet `research/step2_twin_coarse_f2.png`). **My conclusion of 12:50 was wrong:** it came from 10 gr alone. At 30 gr the wild coarse kp of 0.2 lands +16.1 at threshold 5, +12.8 at 7, +9.6 at 9: no threshold covers it.
- The cause: the coarse PID works on a reading some 0.5 s late. A high kp is still pouring fast when the reading reaches the stop point (4.6 rev/s in its last half second at 30 gr, some 48 gr a second), and what is in the air then grows with the charge.
- The careful kp holds at any charge: the bench's 0.02 at threshold 5 gives +0.062, SD 0.100 at 10 gr and +0.099, 0.086 at 30 gr. At 10 gr it never asks more than its least speed (0.02 x 5 gr = 0.1 rev/s, under the least 0.16): the coarse tube runs at its least until the reading shows its stop point.
- A safe coarse kp has a measure with no unit: kp x the coarse tube's weight a turn x the lag. The bench's 0.02 gives 0.11; 0.05 gives 0.28, sound at both charges from threshold 5; 0.1 gives 0.57: at 30 gr +2.3 at threshold 5, +0.6 at 7, sound only at 9; 0.2 gives 1.1 and fails.
- The threshold's job: room for what a well-behaved coarse tube still has in the air at its stop (at its least, 0.16 rev/s x 10.4 gr a turn x some 0.55 s, about 0.9 gr) and the fine tube's powder in the air then; past that it only hands work to the fine tube. At the bench's kp, 5 to 7 narrowed the SD (0.100 to 0.063 at 10 gr, 0.086 to 0.057 at 30 gr) for 0.6 and 1.5 s more a throw; 9 gained nothing; 3 was over (+0.39). On big kernels (`+big_kernel`) the threshold changed nothing (SD 0.19 to 0.25: the fine tube's lumps decide it).

**Mine now: the PID first, into its safe range; then the threshold, for spread against time.** The threshold cannot stand in for the PID. Both could come from step 1, which measures the coarse tube's weight a turn and the lag: a coarse kp put right as the fine one is (`8806f78`), and a threshold from the coarse tube's air at its least. Not built: the owner's.

## 2026-11-26, 14:01: the coarse tube is for speed (the owner); the coarse tube capped by its counted turns (the twin, no unit)

**The owner:** "The coarse trickler should tune for speed, not accuracy. as long as it does not overshoot beyond the stop threshold by miles then the output is acceptable." Before the coarse tube stops, both tubes pour and the coarse tube brings in the bulk, for speed; after it stops, the fine tube pours the rest, for speed and accuracy. "Therefore based on its operating mode, Coarse trickler should find its balance to the stop threshold, with overshoot permitted as long as under the stop threshold by safe margin."

**Read here as:** the coarse tube is judged by its time, on one condition: what it pours lands under the target by a safe margin, so the fine tube always has its finish.

**Today's coarse PID cannot meet that at every charge** (the grid of 13:33): what it has in the air at its stop grows with the charge, so a kp quick at one charge overshoots a larger one. Coarse kp 0.1 at threshold 5 is 2.4 s at 10 gr (+0.135) and +2.3 gr at 30 gr. A tuner for the coarse kp would settle on one charge's kp.

**The coarse tube capped by its counted turns** (`step2_coarse_study.py`, class `Capped`, `--capped`; `step2.py`'s records now carry step 1's coarse weight a turn, which step 1 measures and the firmware drops). Each coarse turn is counted as the fine tube's parcels are: turned out at the motor's speed, due on the scale after the lag at its flow, at step 1's coarse weight a turn. The coarse tube's speed is held under least + room / (its weight a turn x the lag). The room is the target less a margin, less the reading, the fine tube's unseen powder and the coarse tube's. It runs at its most until what it has turned out fills the room, then slows, and stops as today when the reading reaches its stop point. Its kp no longer matters: kp 0.2 and 1 gave the same at 30 and 60 gr. It needs no unit: the weight a turn and the lag are step 1's, in the scale's own numbers; the margin is a share of the user's threshold.
- **A first try, with the room taken from the reading alone, failed** as today's PID does (+12.8 at 30 gr): at its most the coarse tube has some 27 gr in the air before the scale shows any of it. The turns must be counted.
- **Margin half the threshold (2.5 of 5), on f2,** the bench's fine gains, no tuner, 2 x 40 throws, the last 35 judged, against today's (kp 0.02, threshold 5):

| Charge | Time | SD | Mean | Worst |
|---|---|---|---|---|
| 10 gr | 2.7 s (5.0) | 0.085 (0.100) | +0.106 (+0.062) | +0.37 (+0.68) |
| 30 gr | 3.1 s (6.6) | 0.132 (0.086) | +0.118 (+0.099) | +0.77 (+0.33) |
| 60 gr | 4.7 s (10.2) | 0.084 (0.065) | +0.094 (+0.060) | +0.37 (+0.27) |

- **Big kernels** (`+big_kernel`): 4.9, 4.7, 5.5 s against 6.0, 8.3, 10.2; SD 0.227, 0.268, 0.242 against 0.194, 0.249, 0.291.
- **A margin of a quarter of the threshold** is quicker still (2.2, 2.3, 2.9 s) but +0.39 and +0.38 over on average at 30 and 60 gr: too thin.
- **Step 1's coarse weight a turn wrong by 15%:** taken light, the coarse tube lands nearer the target, +0.16, +0.28, +0.28 on average, worst +0.67, in 2.0 to 2.7 s; taken heavy, slower (3.7, 3.4, 5.5 s) and tighter. It fails towards over by a fraction, not by grams.
- **The fine tube's forecast counting the coarse tube's unseen powder too** (`shared`): mixed, not taken. At threshold 7 the narrowest of all (SD 0.051, 0.052, 0.051 in 5.6, 6.8, 7.1 s); at threshold 5 slower (5.7, 4.1, 4.8 s) and no narrower.
- The sheet `research/step2_twin_coarse_f2.png` has it as its fourth column (12 throws a panel, the last 8 drawn: illustration; the grid above is the record).

**Mine: build the counted cap on the coarse tube, its margin half the threshold**: the owner's coarse tube for speed, its landing bounded at any charge, with nothing to tune. It answers the owner's question of 13:33 too: with the cap, neither the coarse kp nor the threshold is tuned for speed. The threshold and its margin say where the coarse tube hands over.
- **What it needs:** step 1's coarse weight a turn stored (measured today and dropped; its place in the profile's struct is a layout question, the owner's), the coarse turns counted in `charge_controller.c` as the fine tube's, the cap, and the margin (half the threshold fixed, or a setting).
- **Not built: the owner's.** It is a forecast on the coarse tube, which the owner kept off on 2026-11-25, 18:05. The reason then was that while both tubes turn the readings cannot tell their weights apart. This cap learns nothing from the throw's readings: it takes step 1's figure.
- **Its proof is on the unit:** the twin's coarse tube pours evenly; a real one's lumps and a lot's change are what the 15% trial stands in for.

## 2026-11-26, 14:01 to 14:55: the powders each their own record; fields appended, unknown names ignored (built, no unit)

**The owner** (after 14:01, on my use of `compatibility` for the fine weight a turn, df8ffee): "I'd suggest you to not repurpose the existing field, and always append the new data to the end of the struct. And the firmware to ignore and discard the unknown fields by name. This way we can keep the forward compatibility. -- engineering trick." Then: "No firmware is released yet so no backward compatibility needed to be considered for any change within ADR-009." On my plan (each powder its own record; the loader reading a longer record too; a settings write ignoring and saying unknown names, an action still refusing them): "Yes, build it".

**Why each powder its own record:** the eight powders were one record, `profile_t` eight to an array. A field appended to `profile_t` is not at the record's end: it moves powders 2 to 8, and gate 2's loader, keeping an earlier record's bytes at their old places, would scramble them rather than reset them. With a record each, a powder's new field is appended at its own end from now on.

**Built (the stored layout changed; said here as AGENTS.md asks):**
- `profile_t` is a record of its own for each powder, a 1 KB slot each from 23 KB (`EEPROM_POWDER_BASE_ADDR`, `EEPROM_SLOT_SIZE`), slots no branch has used: 13 to 20 KB, where I first put them, and 22 KB hold the first build's records on the bench units (`adr009_controller`, never released: each powder's calibration, the unit's record of its scale), which the loader could have read as powders, so they stay unused and are marked in `eeprom.h`; the choice stays at 9 KB, a record of 4 bytes. `compatibility` is gone with the old block; `fine_weight_per_turn` and `coarse_weight_per_turn` are appended at the end (72 bytes). The powders are expected to reset once to their defaults on a unit with the old layout, and the choice to be kept (the old 9 KB record read as a longer one): to be seen on the unit. The settings' part for a powder is a slot apart (`settings_resource_t.stride`); an update's reset of a powder's record is said as `powder_profiles`.
- **The loader reads a longer record** (`common.c`, `config_longer_size`): a record a later version stored with fields appended, found by its CRC within its slot, the CRC carried byte by byte; this version's fields read, the rest ignored and nothing written until this version stores its own. `tests/host_test.py`: the longer record read, left whole, counted saved; stored back by the earlier version, the later reads its fields with its own at their defaults; a damaged longer record gets the defaults. VERDICT PASS.
- **A settings write ignores a name the resource does not have, and says so:** the API's PATCH reply ends with `"ignored": [...]`, the console gives `@ignored <name>` lines; an action's body still refuses one (400 `unknown_field`), and so does a write from the firmware's own code. `rc_run.py web_robust` now expects an unknown field written to be 200 and said, and checks both names come back; `settings --restore` logs the names a copy has that the unit does not.
- **The powder's calibration keeps the coarse tube's weight a turn** (`coarse_weight_per_turn`, measured by the same part as the fine tube's, until now dropped), in its reply's `result`, `was` and `records` too (api_v1.md). Not on a screen: the 12864's result page and the web's sheet show the fine tube's alone, as before.
- Pico 2 W: flash 55.29% (55.16% before), RAM 56.27%; no new warning. The Pico W not built (the owner, 2026-11-26). `host_test` PASS.

**Not yet on a unit** (the bench rules: asked first): the bench's Pico 2 W has the old layout. Its proof: a settings copy (`rc_run.py settings --save`), the flash, its powders seen reset and the choice kept, the copy restored by name (`--restore`, which compares), then `web_robust` (the ignored names) and `settings_roundtrip`, and a menu tour against its baseline (no screen is meant to change).

## 2026-11-26, 15:02 to 15:18: one migration for every record; the powders back in one block (built, no unit)

**The owner** (15:02): "Not ready. Also, since you removed the compatibility field, when the user upgrade the firmware from 1.2x to 2.x, what could happen to the record?" Then (15:09): "Can you recall my proposal yesterday on the migration and backward compatibility schema?" (2026-11-25, 01:13: "Between new size and 1, we can try if CRC matches any try_size - 1. until the size runs out. So if we only append new item to the list, then the non-corrupted data should always match A combination of struct, so we can do migration and we knows the length of previous struct.") Then: "Let's build the generalized migration function, to be shared everywhere, so you don't re-invent the wheel every time. Then apply the same migration function everywhere, including the power profile. Perhaps in the config loader do it automatically 1. If CRC not match, then look for a valid config with reduced struct size. 2. If matches, then fill the matched part, and rest to the be filled with default."

**What 14:55's build did wrong:** each powder in its own record at 23 KB left v1.2x's powders in the old block at 9 KB, which nothing read: an upgrade from 1.2x would keep the choice and reset all eight powders to their defaults, and a later save would leave 1.2x unable to read them either. I had read "No firmware is released yet" as no path needed from any record; v1.20 and v1.21 are released. And `compatibility` was not "used by nothing" in 1.2x: its REST API wrote it (`p1` of `/rest/profile_config`) and its menu showed it, so it is kept as stored.

**Built:**
- **One migration, in the loader every module uses** (`common.c`; the owner's scheme, gate 2's loader made general): a record whose CRC32 fails at its size is looked for at each size it could have had within its slot, shorter (an earlier version's) or longer (a later one's), the CRC carried byte by byte through the slot once; found, what it holds of each field is filled in, the rest takes the defaults, and it is stored at this version's size at once (a later version's fields discarded; on an update's trial held as any write). The longest shorter size wins over a longer one.
- **Records that are a header and an array** (the powders, the motors) declare it (`load_config_layout`: header, element, count): the sizes tried are the header and a whole number of elements, and each element's part is filled into its own place. So a field is appended at the end of each element, the struct itself.
- **The powders are one block at 9 KB again**, `profile_t` as v1.2x stored it (`compatibility` back, untouched) with `fine_weight_per_turn` and `coarse_weight_per_turn` appended to it (64 to 72 bytes; the block 516 to 580). The motors load through the same layout. The per-powder records, `settings_resource_t.stride` and the update's mapping of 14:55 are gone; the loader's reading of a later version, the unknown names ignored and said, and the coarse weight a turn kept by the calibration stay.
- **Checked on the PC** (`tests/host_test.py`, VERDICT PASS): v1.21's block of eight powders (its own struct, 516 bytes) read whole into this version's, the choice kept, the two weights a turn at their defaults, stored at 580; a later version's block (a field more on each powder, 612 bytes) read for this version's fields and stored at 580; the plain records both ways, a damaged record and an erased chip at the defaults, a trial's writes held. Pico 2 W: flash 55.30%, RAM 56.24%, no new warning.
- **The bench's units:** their 9 KB block has this branch's old layout, `compatibility`'s place holding the fine weight a turn as a float; read now as `compatibility`, it is kept and used by nothing, and the fine weight a turn starts null. Step 1's powder part, or a settings copy restored, gives it back.

**Not yet on a unit** (the owner, 15:02: the bench not ready). Its proof: a settings copy, the flash, the copy compared (every setting as before; the two weights a turn null), `web_robust`, `settings_roundtrip`, a menu tour against the baseline.

## 2026-11-26, after 15:18 to 15:33: the margins from the scale's noise, the cup's lift from its weight (built, no unit); the coarse tube's cap not taken

**The owner's answers** (after 15:18, three questions): both step 1 items in this work: the steady and zero margins from the scale's step, and the cup-lifted test from step 1's cup weight. On the coarse tube under the counted cap: "You should not implement your own algorithm and discard the current coarse PID, so use and tune PID. Reason: the PID works, and your priority is to optimize, not discard and starting from scratch which is not proven apart from my device. Risk is not on my side, but more than 1000+ users. I'd play safe, that don't change the core algorithm, but tune under the existing framework." On the cap's margin: "This should be a tuning question, so it should always fit to the existing framework." **So the counted cap of 14:01 is not taken;** the coarse tube's PID stays, and how to tune it under the existing tuner is studied on the twin first, then put to the owner.

**The margins from the scale's noise** (step 1's scale part; C `calibration_method.c`, Python `calibration.py`, judged call for call by `tests/calibration_sim.py`):
- Each run's watch after its stop now gives the scale's step (the smallest change its readings make) and its spread at rest (the standard deviation of the last 0.6 s of the readings held). The scale part keeps the smallest step over its bursts and the middle of their spreads (`step`, `rest_sd` in its record; null where not measured).
- **On Save, the charge's `steady_sd_margin` and `zero_mean_margin` become one step, or twice the spread at rest where that is more** (`calibration_rest_margin`), stored with the scale's settings. Not written when the step was not measured or passes the settings' 1. One step: a flicker of one digit has an SD of at most half a step and passes; a reading still climbing a step a reading has about three and does not; a zero is within one digit. The API's calibration reply gives `step`, `rest_sd` and `margin`; no screen shows them.
- On the twin: the A&D in grains, step 0.02 and spread 0 on every seed, so 0.02 and 0.02, today's defaults: a bench unit's scale calibration changes nothing. In grams, step 0.001 and a spread of 0 to 0.00013 on the three seeds (the review's run of the final code; 0.0004, written here first, was Python's figure before the fix below): 0.001 g, where the default 0.02 is 20 digits. `calibration_sim` VERDICT PASS on both.
- **Found on the way, and how it was met:** the twin's watch gives readings at 47 a second (the bench PC's rate of reading the console), the unit's watch one a scale frame (the A&D's 21 a second). The C holds 256 readings of a watch, so in the judge it held the first 5.4 s of a 10 s watch and Python all of it: the spread first came out 0 in the C and 0.0004 in Python, in grams. The spread is now taken from the last 0.6 s of the readings held, and Python holds a watch's first 256 as the unit does (`WATCH_READINGS_MOST`). **Left as it was, for the review:** `calibration_settled`'s reading at rest is the median of the watch's last 0.6 s by the watch's length; on a scale over about 25 frames a second the C's last stretch is past what it holds, and the rest is its last reading held. On the A&D (21 a second, 210 readings) the C holds the whole watch.

**The cup's lift from its weight** (`charge_mode.cpp`, the wait for the cup's removal): the cup counts as lifted once the reading is half the cup's weight below zero (step 1's cup part, `cup_weight`); 10, as before, when the cup was never weighed. A fixed 10 assumed grains: a cup under 10 g on a scale in grams was never seen lifted.

Pico 2 W: flash 55.39%, RAM 56.25%; no new warning. `host_test` and `calibration_sim` PASS. Not on a unit.

## 2026-11-26, 15:33 to 15:42: the coarse tube's PID under the existing tuner (the twin, no unit)

The owner's direction (after 15:18): keep the coarse PID and tune it under the existing framework; the margin a tuning question. Studied on the twin first (`tools/charge_sim/step2_coarse_tuner_study.py`): the product's tuner (`step2_tuner.Tuner`, kp+kd, the share modelled) on f2, 300 throws at 10 gr then the charge changed to 30 gr for 150, the tuner on throughout, 4 to 6 sessions an arm.

| Arm | From the bench's coarse kp 0.02 | From a wild coarse kp 0.2 |
|---|---|---|
| Today's tuner (the fine tube's gains) | 10 gr +0.054, SD 0.104, 7.0 s; 30 gr +0.039, 0.073, 10.5 s | 10 gr 998 of 1200 throws over by 0.5 gr; 30 gr **+14.5 every throw** |
| A third axis, the coarse kp, tried up first | 10 gr 7.5 s; 30 gr 9.8 s, worst +1.65: nothing gained | down to 0.089 only: 30 gr +0.35, 222 of 600 over |
| The same, bounded by kp x step 1's coarse weight a turn x middle lag <= 0.4 | as above (the bound never reached) | 30 gr +0.049, 0.072, 4 of 600 over; many wild windows first |
| **The first window's gains put right reaching the coarse kp: above the bound, brought down to it** (kept only if cheaper) | **identical to today's** (within the bound) | **10 gr +0.042, 0.058, 7.1 s; 30 gr +0.047, 0.059, 8.0 s, 0 of 600 over**; only the first window wild |
| The same, a slow kp also raised to the bound | kept in 2 of 6 sessions (10 gr decides it by noise); 30 gr 9.4 s against 10.4 | |

**Read:** tuning the coarse kp for speed under the existing tuner gains nothing from sane settings: below about 0.03 the coarse PID asks only its least speed on a 10 gr charge, and the steps the search can tell apart are within a window's noise; the speed-ups that exist (kp 0.1 and up) overshoot a larger charge. What the tuner can do is protection: a wild coarse kp brought down in the first window.

**The bound's limit:** a coarse kp at the bound (0.053 here; the shipped AR2209 default is 0.05) is sound at 10 and 30 gr (+0.060, SD 0.088; +0.068, 0.077, 5.6 s against 6.6) and **not at 60 gr** (+0.109, SD 0.172, worst +1.26, 5.0 s, against the bench's 0.02: +0.060, 0.065, +0.27, 10.2 s). The overshoot grows with the charge: a bound sound to 60 gr is about half (0.2 by it, kp 0.026), which would also bring down the shipped AR2209 and Benchmark2 defaults (0.05, 0.06) where cheaper at the user's charge. Put to the owner; nothing built.

## 2026-11-26, 15:42 to 16:13: the coarse kp put right in the tuner's first window (built, no unit)

**The owner** (after 15:42): "Protection, bound 0.4"; the independent review after this work.

**Built:**
- **The tuner's first window puts the coarse kp right too** (`charge_tuner.c`, `step2_tuner.Tuner.put_right`): where kp x the powder's coarse weight a turn (step 1's, `coarse_weight_per_turn`) x the scale's middle lag is above 0.4 (`CHARGE_TUNER_COARSE_GAIN_MOST`, `COARSE_GAIN_MOST`), it is tried at 0.4 with the fine gains put right, as one trial kept only if cheaper. Never moved up, and moved nowhere else: every other window is thrown on the kept coarse kp. With no coarse weight a turn (never calibrated) it is left as it is.
- **The charge mode carries it** (`charge_mode.cpp`): the throw's coarse kp is the tuner's, the profile's with the tuner off; a kept coarse kp that moved is written into the powder's `coarse_kp` with the fine gains and stored (`by` unit); a change of the profile's coarse kp starts the tuner afresh, as its fine gains do. The console's `Tuner: coarse kp ...` line says what the window did to it.
- `auto_tune`'s description says so, in the table and in api_v1.md.
- **Checked:** `tests/step2_sim.py` with a new start, the bench's gains with a coarse kp of 0.2 (`bench wild coarse`), the C tuner given the coarse kp, step 1's coarse weight a turn and its own next coarse kp, judged against the Python window for window: VERDICT PASS, the same steps on every rig and start; from the wild coarse kp it ended at 0.050 to 0.062 on the healthy rigs, and stayed 0.2 with an empty fine hopper (every throw short, so the gains put right were never cheaper). Pico 2 W: flash 55.44%, RAM 56.25%; no new warning.
- **On the twin** (`step2_coarse_tuner_study.py`, 15:42): from the bench's 0.02, the same as before; from 0.2, 30 gr +0.047, SD 0.059, none of 600 over, against +14.5 every throw. A coarse kp at the bound is sound to 30 gr and wider at 60 gr (SD 0.17, worst +1.26), as the shipped AR2209 default (0.05) already is.

**Not on a unit.** Its proof there: a powder calibrated (its coarse weight a turn kept), a coarse kp set high on it, the first window's `Tuner:` lines showing it tried at the bound and kept.

## 2026-11-26, after 16:13 to 17:09: the independent review, its root cause, and its fixes (no unit)

**The review** (a session of its own, read-and-run, its report `logs/review/adr009_review_2026-11-26.md`): 2 blockers, 2 should-fix, 17 notes. Tests and the Pico 2 W build passed in its runs.

**The owner's answers** (after 16:13): on B1, "Everything should be automatically migrated with the code we developped. Why there is still blocker? That concerns me. I need you to run the RCA and explain the misses." On B2: "v2.x is never released so we should merge v1.21 into the mains, and restablish the index for simulation." S2: the cup lifted a few scale steps below zero. The rest: fix them too. Then: "Remember, NEVER INSERT, always append for EEPROM record. This MUST be in AGENTS.md too."

**B1, the root cause.** The migration is right only where a record's history was append-only: a CRC proves the bytes are whole, not what they mean. The charge record's was not: v1.19 inserted `coarse_stop_gate_ratio` at offset 20 (48 to 52 bytes), v2.0.0 (`3e469cd`) the three backoff fields at offset 24 (52 to 64). v2.0.x had no migration and reset any record whose CRC failed at its size: safe. Gate 2's shorter-record search (`e10e299`, 2026-11-25) turned that reset into "keep the bytes as a prefix" for every record, and `04569c4` kept it: a v1.2x charge record is read shifted (precharge on for 4.66 hours, the backoff off, the colours moved, stored back at once).
- **The whole class, checked** (every stored struct's declarations, type and name, at v1.16 to v1.21, v2.0.0 to v2.0.3 and this branch, each a prefix of the next): the charge record is the only one with insertions, at v1.19 and v2.0.0. Every stored enum's values (each name's value at each release): the scale driver's 9 is the only one whose meaning changed (v1.21's FA-EN, v2's simulated scale), which no migration can see (B2).
- **The misses, mine:** gate 2's loader was designed and tested on invented structs, never on the released records; when the owner asked on 2026-11-26 what a 1.2x upgrade does to a record, I checked the powders alone, not every record against every release; and I wrote in AGENTS.md that every version reads every other's records, with no evidence for it. The tests passed because they tested what I assumed.
- **The fix:** not built (the owner asked for the analysis first); put to the owner.

**Built from the review** (the owner: fix the rest too):
- **S2, the cup's lift:** a lifted cup is now readings steady and their mean below minus 10 zero margins (`CHARGE_CUP_LIFT_MARGINS`), which step 1 sets from the scale's own step. A finished charge reads positive, so a steady reading that far below zero is the cup off, whatever the cup weighs and whatever the unit. Half the cup's weight (built at 15:33) waited for ever with a lighter cup, or a unit changed since the cup was weighed.
- **S1, a revert or `saved` on an update's trial:** both read a part of a record through `config_read_part` (`common.c`): the write a trial holds, else the record read with its CRC checked; a raw read gave an earlier version's bytes at this version's offsets. The stored-record table keeps each record's size for it. `host_test`: a powder read back at this version's offsets, on a trial the write held and not v1.21's bytes, a damaged record nothing.
- **N11:** a weight a turn past its setting's 100 is left out with a line, as the margins are, not a Save refused whole. **N5:** the test's stale comment.
- **Documents:** AGENTS.md has the owner's rule, never insert, always append, in its settings section and its "Do not" list, with stored enums' values likewise never renumbered or reused (mine, from B2); its claims of every version reading every other's records are narrowed to where history was append-only, with B1 named (N3, N4); the Wi-Fi's 2 KB slot (N1); an appended field not in the old tail padding (N2). api_v1.md: the margin written only up to 1 (N9), the coarse Kp tried at the first window's end through the second (N8), the ignored names under "Growing within v1" (N10), the cup's lift as now built. This file: the grams spread corrected in place (N6). **N12:** `step2_coarse_tuner_study.py` inherits the product tuner's coarse put-right since `cf67015`, so its arms 2 and 3 from a coarse kp of 0.2 no longer show the 15:33 to 15:42 table; run it at `8f3527e` to see that table again.
- **Left, recorded:** N16 (step 1's weights carry no unit: a unit changed after calibration leaves the margins off by 15 times; the owner's design question), N17 (an empty hopper lets the fine tuner walk kd to 113,184 on the twin; for step 2's sign-off). B2: the owner's direction, v1.21 merged into main and the simulated scale given a value no release used, a piece of its own on main.
