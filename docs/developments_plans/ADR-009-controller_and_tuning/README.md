# ADR-009: calibration and automatic tuning

Status: **Draft** 2026-11-25. Step 1, the calibration, is drafted below; step 2, the tuning, is drafted after it.

The earlier README and progress are phased out and moved to [`legacy/`](legacy/).

## What we want

ADR-009 is two steps, in this order:
1. **The calibration:** the user runs it for the unit and for each powder, from the screen or the web. It measures the cup, the scale, and each tube's least and most speed for the powder, and keeps them.
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

## Step 1: the calibration

### What it records

| # | What | Kept in | How it is found |
|---|---|---|---|
| 1 | The cup's capacity | the charge settings, appended | The user trickles with the knob until the cup holds as much as they want it to; the reading, shown and confirmed, is the capacity. A pour of the calibration that would pass it pauses the calibration and asks for the cup to be emptied, then goes on |
| 2 | The empty cup's weight | the charge settings, appended | The cup lifted off the zeroed scale: the reading falls by its weight, so a lifted cup can be told from a light reading |
| 3 | The scale's response time | the scale settings, appended | How long a small change takes to show and settle, for a few sizes of change |
| 4 | The scale's lag | the scale settings, appended | How far the reading runs behind a steady flow, by flow rate. It may be one curve with item 3: a smaller change takes longer to register |
| 5 | Each tube's least speed | the powder profile's own field | The slowest speed at which what lands after a stop is consistent |
| 6 | Each tube's most speed | the powder profile's own field | The fastest speed at which what lands after a stop is consistent |

**Consistent** (items 5 and 6): a speed's spread of what lands after its stops is within 3 times the smallest spread this calibration measured, on this unit's own scale. On the bench's A&D the spread was about 2.5 digits at every speed tried, so 3 times is about 7.5 digits (0.15 gr). A factor, not the A&D's grains: a slower scale is held to its own best.

Items 5 and 6 belong to this powder on this unit's tubes and scale, not to the powder alone. The coarse and fine stop thresholds and the PID stay as the user has them; today's loop charges as it does now.

### What we know

Measured on the bench (the Pico 2 W, an A&D FX-300i, AR2209) unless said; the figures are in `legacy/progress.md` up to 2026-11-21 and in branch `adr009_controller`'s progress after.
- **The scale is late by an amount that depends on the change** (2026-11-21): about 0.5 s behind a fast stream, about 1 s behind a slow one, about 2 s for a single digit; the same for two powders and two tubes, so mostly the scale's. It calls a weight stable 0.38 s after its last change; one kernel first shows 1.7 s after a nudge, three and more 0.9 s.
- **What lands after a fine stop scatters by about 2.5 digits at every speed tried** (2026-11-20), in stop tests and in real throws alike.
- **Today's stored settings** (`common.c`'s `load_config`): each module's record is read as its struct's size and a CRC32. A struct that grows fails the check, and the module's defaults are applied. The charge settings hold the thresholds, margins, back-off, precharge and colours; the scale settings hold the scale's make, baud rate and frame format.
- **The first build's calibration** ran on the PC through the console (`tools/charge_sim/staged.py`'s `measure_on`, driven by `bench/trial.py`): it measured the scale's settle time and, at each speed, its lag and what lands after a stop. It took 8 to 13 minutes and poured 164 to 252 gr on the bench. Those tools and every recording are kept in `tools/charge_sim`.

### To settle before approval

1. **Appending without a reset:** today's loader would reset the charge settings and the scale settings (the scale's make and baud rate with them) on the update that appends items 1 to 4. Proposed: a record whose CRC32 fails at the struct's size is tried at each shorter size, from one byte less down to one, until a CRC32 matches; that size is the previous version's. Its bytes are kept, the appended fields take their defaults, and it is written once the update is kept, as defaults are today. It holds when:
   - fields are only appended at the end of a flat stored struct (never inside an array's element, such as one powder profile of eight);
   - the struct grows by them: `sizeof` checked before and after, since a field that fits in the old struct's tail padding leaves the size as it was and reads that padding;
   - a record that matches at no size is taken as damaged and gets the defaults, as today. A wrong match is a chance of one in about four billion for each size tried.

   Shown by an update from today's release that keeps every setting.

The method is developed from the PC on the bench, where the tools exist; for users it is a job on the unit, run from the screen and the web.

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
