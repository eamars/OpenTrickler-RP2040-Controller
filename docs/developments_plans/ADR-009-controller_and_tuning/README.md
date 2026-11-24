# ADR-009: calibration and automatic tuning

Status: **Draft** 2026-11-25. Step 1, the calibration, is drafted below; step 2, the tuning, is drafted after it.

The earlier README and progress are phased out and moved to [`legacy/`](legacy/).

## What we want

ADR-009 is two steps, in this order:
1. **The calibration:** the user runs it for the unit and for each powder, from the screen or the web. It measures the cup, the scale and the powder on this unit's tubes, keeps what it measured, and writes the charge settings that follow from it.
2. **The tuning:** the predictor and the PID auto-tuner, made from what step 1 keeps.

Step 2 depends on step 1: it takes step 1's records as its input, and has none without them. It starts only once step 1 is fully complete: a user can run the calibration from the 12864 and from the web on a unit, its records are kept and backed up, and nothing of it is left open. ADR-009 is these two steps and nothing more.

## Owner's direction

- The calibration records the cup, the scale, and each powder on this unit: the list below.
- The PID is tuning, not calibration: step 1 neither measures nor writes it.
- Step 2 is the predictor and the PID auto-tuner, and nothing runs beside them: no glide, no other algorithm in parallel.
- Step 2 starts only once the calibration is fully complete, the screen and the web included.
- Step 1 is not judged by how throws turn out: no trickle performance is a condition of its sign-off. A charge with its settings only has to trickle.
- The fine threshold is the user's expectation, the tolerance of a throw. The calibration never writes it.
- The cup comes first. The user gives its capacity by filling it with the knob; a pour that would pass it pauses the calibration.
- The servo gate's tuning is not part of ADR-009.

## Step 1: the calibration

### What it records

| # | What | Kept by | How it is found |
|---|---|---|---|
| 1 | The cup's capacity | the unit | The user trickles with the knob until the cup holds as much as they want it to; the reading, shown and confirmed, is the capacity. A pour of the calibration that would pass it pauses the calibration and asks for the cup to be emptied, then goes on |
| 2 | The empty cup's weight | the unit | The cup lifted off the zeroed scale: the reading falls by its weight. A charge then tells a lifted cup from a light reading |
| 3 | The scale's digit | the unit | The smallest step its readings show |
| 4 | The scale's response time | the unit | How long a small change takes to show and settle, by its size (one digit, a few digits) |
| 5 | The scale's lag | the unit | How far the reading runs behind a steady flow, by flow rate. It may be one curve with item 4: a smaller change takes longer to register |
| 6 | Each tube's grains a turn | the powder's profile | Steady runs of each tube, at several speeds |
| 7 | Each tube's least speed | the powder's profile | The slowest speed at which what lands after a stop is consistent |
| 8 | Each tube's most speed | the powder's profile | The fastest speed at which what lands after a stop is consistent |
| 9 | The fine stop point | the powder's profile | What still lands after the fine tube stops at its least speed: its average and its spread |
| 10 | The coarse threshold | the powder's profile | As close to the target as the coarse tube can stop without an overcharge: both tubes' powder still falling at the coarse stop, the coarse stop's spread, and the back-off's spill when the back-off is on |

The "powder" items belong to this powder on this unit's tubes and scale, not to the powder alone: they are not shared between units.

### What it sets in today's settings

| Setting | From | Rule | Seen on the bench before |
|---|---|---|---|
| Each tube's least and most speed | 7, 8 | within the motors' own limits | |
| Coarse stop threshold | 10 | the smallest that keeps clear of an overcharge | On 2026-11-20 a stop 5 gr short left 3 gr and more falling, so the fine tube had 1.5 gr of room, not 5 |
| Fine stop offset (new) | 9 | the fine tube stops when the reading is the target less this average | On 2026-11-20 an average of 4.6 digits landed after the slow finish's stop: stopping 0.11 gr short in place of 0.03 would have taken it away |
| Fine stop threshold | the user | stays the user's tolerance: the ± band of the verdict | |

The PID values in each profile are left as they are: step 2 makes them.

### What we know

Measured on the bench (the Pico 2 W, an A&D FX-300i, AR2209) unless said; the figures are in `legacy/progress.md` up to 2026-11-21 and in branch `adr009_controller`'s progress after.
- **The scale is late by an amount that depends on the change** (2026-11-21): about 0.5 s behind a fast stream, about 1 s behind a slow one, about 2 s for a single digit; the same for two powders and two tubes, so mostly the scale's. It calls a weight stable 0.38 s after its last change; one kernel first shows 1.7 s after a nudge, three and more 0.9 s.
- **What lands after a fine stop scatters by about 2.5 digits at every speed tried** (2026-11-20), in stop tests and in real throws alike.
- **The fine tube's grains a turn change with speed:** the throws of 2026-11-22 suggested 0.68 at the finish and 0.83 at 1 rev/s.
- **Today's loop** (`charge_mode.cpp`): both tubes start together, each a PID loop on the reading between its least and most speed. The coarse tube stops at the target less `coarse_stop_threshold`, one setting for every powder. Both stop at the target less `fine_stop_threshold`, which is also the ± band of the verdict, judged about 1 s after the stop.
- **Today's stored settings** (`common.c`'s `load_config`): each module's record is read as its struct's size and a CRC32. A struct whose size changes fails the check, and the module's defaults are applied: all eight powder profiles, for the profile's struct.
- **The first build's calibration** ran on the PC through the console (`tools/charge_sim/staged.py`'s `measure_on`, driven by `bench/trial.py`): it measured the data for items 4 to 9 in its own form (the settle time, each tube's grains a turn, and at each speed the lag and what lands after a stop), was given the digit, and took the tubes' least and most speeds from the user's limits. It took 8 to 13 minutes and poured 164 to 252 gr on the bench. Those tools and every recording are kept in `tools/charge_sim`.

### To settle before approval

1. **Where the records are stored, and what an update does to a user's settings:** to be discussed with the owner. Moving the coarse threshold into the profile and adding a fine stop offset would change stored structs, and so reset them on every unit.
2. **"Consistent"** for items 7 and 8, in numbers: for example `bench/stop_spread.py`'s rule, the fastest speed whose spread is within 1.3 times the smallest.
3. **How long a calibration may take.** The first build's took 8 to 13 minutes.

The method is developed from the PC on the bench, where the tools exist; for users it is a job on the unit, run from the screen and the web.

### How the unit shows it works

No throw's result is judged.
- A user runs the calibration on a unit from the 12864 and from the web, from nothing to its records, each of its steps shown, and can stop it.
- No pour passes the cup's capacity: one that would pauses the calibration and asks for the cup to be emptied.
- Every item of the table is recorded, kept across a restart, held in the backup, and given back by a restore.
- The settings it writes are those its rules give from its records, checked on the PC against the recordings of its own runs.
- A charge then throws with them.

## Step 2: the tuning

- **The predictor:** what the reading will show once the powder still falling has landed, from step 1's records.
- **The PID auto-tuner:** each tube's PID made from step 1's records, then kept right throw by throw.
- **Nothing else:** no glide, and no other algorithm in parallel.

Its input is step 1's records. It starts only once step 1 is fully complete; its details are drafted then.
