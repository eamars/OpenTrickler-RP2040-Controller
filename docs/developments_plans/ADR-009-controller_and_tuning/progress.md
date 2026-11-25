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
