# ADR-009: calibration and automatic tuning

Status: **Open** 2026-11-25: step 1, the calibration, approved by the owner (recorded 01:22) and being built in the gates under "What we do". Step 2, the tuning, is drafted once step 1 is complete.

The earlier README and progress are phased out and moved to [`legacy/`](legacy/).

## What we want

ADR-009 is two steps, in this order:
1. **The calibration:** the user runs it for the unit and for each powder, from the screen or the web. It measures the cup, the scale, and each tube's least speed for the powder, and keeps them.
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
- No retries and no pass criteria inside the calibration: an acceptable result is good enough. Each run pours as little powder as its measurement allows.
- A step that fails says what failed and retries nothing unless the user asks; its item keeps its default value.
- Each part runs on its own: the cup, the scale and each powder are calibrated independently, and each item can be run alone. A part uses what the others have stored, or their defaults where they were never run.
- The servo gate's tuning is not part of ADR-009.
- The most speed is not measured, and nothing is built for it: each powder profile's maximum speed stays as the user has it, and the charge already holds each tube to the smaller of it and the motor's maximum speed (Settings, Motors).

## Step 1: the calibration

### What it records

| # | What | Kept in | How it is found |
|---|---|---|---|
| 1 | The cup's capacity | the charge settings, appended | The user trickles with the knob until the cup holds as much as they want it to; the reading, shown and confirmed, is the capacity. A pour of the calibration that would pass it pauses the calibration and asks for the cup to be emptied, then goes on |
| 2 | The empty cup's weight | the charge settings, appended | The cup lifted off the zeroed scale: the reading falls by its weight, so a lifted cup can be told from a light reading |
| 3 | The scale's settle time | the scale settings, appended | How long a small change takes from the reading's first move to its rest: when it first shows its final value and never again strays more than one step from it. One number, from small moves of the fine tube |
| 4 | The scale's lag | the scale settings, appended | How far the reading runs behind the powder poured, in seconds, by flow rate: over a steady run and its stop, the powder poured but not yet shown, divided by the powder poured. A table by flow |
| 5 | Each tube's least speed | the powder profile's own field (its minimum speed) | Twice the slowest speed at which the tube pours |

**The least speed** (item 5), each tube judged on its own. The tube is first turned at the middle of its motor's limits in ratio until two runs pour alike (the prime), which gives its grains a turn where it streams; then a ladder of speeds from the motor's least up to the prime's speed, no faster:
- it is twice the slowest speed at which the tube pours: that speed's runs together pour at least half the tube's grains a turn at the prime's speed. A confident value, the measured minimum doubled. The runs are judged by their sum, as one slow run is a few kernels, and at each speed the run farthest from the speed's median is left out as a clump or a lull;
- a tube whose speeds give no least leaves it as it was, and says so.

On the bench (2026-11-25) this gives the fine tube 0.020 rev/s in each of three runs (it pours at its motor's least, 0.010), the coarse tube 0.16 rev/s. The tubes register ever more alike with speed, kernel by kernel to a stream, with no knee, so the least is not judged by how alike the runs are (progress, 2026-11-25, 01:57).

Item 5 belongs to this powder on this unit's tubes and scale, not to the powder alone. The coarse and fine stop thresholds, each profile's maximum speeds and the PID stay as the user has them; today's loop charges as it does now.

### Where each item is used

| # | Item | Used now | To be used |
|---|---|---|---|
| 1 | The cup's capacity | The calibration: a run that would pass it pauses for the cup to be emptied (`calibration.py`'s `cup_room`) | The calibration as a job on the unit (gate 3). Nothing outside the calibration |
| 2 | The empty cup's weight | Nothing | The calibration's job (gate 3): a cup lifted mid-run told from a light reading. Not in the plan: the charge's own check that the cup was lifted, today a steady reading 10 gr below zero (`charge_mode.cpp`'s `charge_mode_wait_for_cup_removal`), so a cup under 10 gr is never seen lifted |
| 3 | The scale's settle time | The calibration: how long each stop is watched, after the slowest lag (`watch_after_stop`) | Step 2's predictor: when a reading is final. Not in the plan: the charge reads a throw's result 1 s after its stop, and waits for zero over 10 steady readings 300 ms apart |
| 4 | The scale's lag | The calibration: how long each stop is watched, with the settle time | Step 2's predictor: the powder still to show at a stop, its flow times its lag |
| 5 | Each tube's least speed | Today's charge, once saved into the profile: each tube's PID never asks for less (the larger of it and the motor's minimum), and the coarse back-off turns at it when its own speed is 0 | Step 2's PID auto-tuner, which takes step 1's records (which ones is drafted with step 2) |

Item 5 changes how today's charge finishes as soon as it is saved: the fine tube's least on the bench, 0.020 rev/s, is a quarter of the profile's 0.08, so its last powder comes four times slower.

### What we know

Measured on the bench (the Pico 2 W, an A&D FX-300i, AR2209) unless said; the figures are in `legacy/progress.md` up to 2026-11-21 and in branch `adr009_controller`'s progress after.
- **The scale is late by an amount that depends on the change** (2026-11-21): about 0.5 s behind a fast stream, about 1 s behind a slow one, about 2 s for a single digit; the same for two powders and two tubes, so mostly the scale's. It calls a weight stable 0.38 s after its last change; one kernel first shows 1.7 s after a nudge, three and more 0.9 s.
- **What lands after a fine stop scatters by about 2.5 digits at every speed tried** (2026-11-20), in stop tests and in real throws alike.
- **The A&D flickers by one step (0.02 gr) at rest,** seconds after it settled (2026-11-25): a flicker leaves the rest and comes back, while a change's last step arrives at the rest and stays.
- **The two scale records are apart** (2026-11-25): the lag is the delay, the settle time what follows the first move. Together they say how long a stop is watched: the slowest lag, then the settle time. On the bench the A&D's lag is 0.6 to 1.2 s by flow and its settle time 0.27 s.
- **Today's stored settings** (`common.c`'s `load_config`): each module's record is read as its struct's size and a CRC32. A struct that grows fails the check, and the module's defaults are applied. The charge settings hold the thresholds, margins, back-off, precharge and colours; the scale settings hold the scale's make, baud rate and frame format.
- **The charge holds each tube to the smaller of the profile's maximum speed and the motor's** (`charge_mode.cpp`), and to the larger of their minimums. A profile has no maximum that is not set: a maximum of 0 holds the tube at its minimum speed.
- **The first build's calibration** ran on the PC through the console (`tools/charge_sim/staged.py`'s `measure_on`, driven by `bench/trial.py`): it measured the scale's settle time and, at each speed, its lag and what lands after a stop. It took 8 to 13 minutes and poured 164 to 252 gr on the bench. Those tools and every recording are kept in `tools/charge_sim`.

### To settle before approval

1. **Appending without a reset:** today's loader would reset the charge settings and the scale settings (the scale's make and baud rate with them) on the update that appends items 1 to 4. Proposed: a record whose CRC32 fails at the struct's size is tried at each shorter size, from one byte less down to one, until a CRC32 matches; that size is the previous version's. Its bytes are kept, the appended fields take their defaults, and it is written once the update is kept, as defaults are today. It holds when:
   - fields are only appended at the end of a flat stored struct (never inside an array's element, such as one powder profile of eight);
   - the struct grows by them: `sizeof` checked before and after, since a field that fits in the old struct's tail padding leaves the size as it was and reads that padding;
   - a record that matches at no size is taken as damaged and gets the defaults, as today. A wrong match is a chance of one in about four billion for each size tried.

   Shown by an update from today's release that keeps every setting.

### What we do

The method is developed from the PC on the bench, where the tools exist; for users it is a job on the unit, run from the screen and the web. The gates, in order, each started once the one before it is done:
1. **The method,** in the simulation and then from the PC on the bench (`tools/charge_sim`, `bench/record.py`): the cup, the scale and each tube's speeds, each part run alone. Its recordings are kept, and they fix the records' form (how many points the lag holds).
2. **The records stored:** the loader's migration (above) and the fields appended to the charge and scale settings, each described once in its module's table, so the API, the console, the export and the backup follow. Shown by an update from today's release that keeps every setting, on both boards.
3. **The calibration as a job on the unit,** started from the console and the API: each part alone, a failure said and not retried, the cup's pause. On the bench it records what gate 1's method recorded from the PC, within the spread each measured.
4. **The screens and the web:** mock-ups for the owner first, then built.
5. **Step 1's sign-off on a unit:** the list under "How the unit shows it works".

### How the unit shows it works

No throw's result is judged.
- A user runs the calibration on a unit from the 12864 and from the web, from nothing to its records, each of its steps shown, and can stop it.
- Each part runs alone and in any order: a powder calibrated on a unit whose scale and cup were never calibrated runs on their defaults; a scale calibrated again leaves every powder's record as it was; each item can be run by itself.
- No pour passes the cup's capacity: one that would pauses the calibration and asks for the cup to be emptied.
- Every item of the table is recorded, kept across a restart, held in the backup, and given back by a restore; an update keeps every setting the user had.
- The values it records are those its runs' recordings give, checked on the PC.
- A step made to fail (no reading from the scale, a tube that pours nothing, the cup lifted mid-run) says what failed, retries nothing, and leaves its item at its default value.
- Its time and the powder it poured are reported, not judged.
- A charge then throws with them.

## Step 2: the tuning

- **The predictor:** what the reading will show once the powder still falling has landed, from step 1's records.
- **The PID auto-tuner:** each tube's PID made from step 1's records, then kept right throw by throw.
- **Nothing else:** no glide, and no other algorithm in parallel.

Its input is step 1's records. It starts only once step 1 is fully complete; its details are drafted then.
