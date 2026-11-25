# ADR-009: calibration and automatic tuning

Status: **Open** 2026-11-25: step 1, the calibration, approved by the owner (recorded 01:22) and being built in the gates under "What we do": gates 2 to 4 built with no unit. **Their test on a unit is deferred by the owner (recorded 16:37)**, until the owner says the bench is ready for it; then the list in progress, 2026-11-25, "What the bench owes", is run in its order, and step 1 is signed off by gate 5. Step 2, the tuning, is drafted once step 1 is complete.

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
| 3 | The scale's settle time | the scale settings, appended | How long a small change takes from the reading's first move to its rest: when it first shows its final value and never again strays more than one step from it. One number, from bursts of the fine tube: its powder turned out within about 0.1 s, several steps of it, so what follows the first move is the scale's (the owner, 2026-11-25) |
| 4 | The scale's lag | the scale settings, appended | How far the reading runs behind the powder poured, in seconds, by flow rate: over a steady run and its stop, the powder poured but not yet shown, divided by the powder poured. A table of five flows: about 0.2, 0.4 and 0.8 gr/s on the fine tube, 3 and 10 on the coarse |
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
| 2 | The empty cup's weight | Nothing | Step 2's charge controller, as its safety: a cup lifted mid-charge told from a light reading (the owner, 2026-11-25). Also the calibration's job (gate 3), for a cup lifted mid-run. Today's charge keeps its own check: a steady reading 10 gr below zero (`charge_mode.cpp`'s `charge_mode_wait_for_cup_removal`) |
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
- **The lag holds from day to day where it matters** (six sessions, 2026-11-19 to 25, `bench/records_spread.py`): the coarse tube's flows 0.55 to 0.65 s; the fine tube's 0.15 to 0.4 gr/s 0.8 to 1.1 s, about 0.8 gr/s 0.63 to 0.83 s. Below 0.1 gr/s it is noise (session to session 0.56 to 1.54 s at 0.04 gr/s), over less than two steps of powder unseen.
- **The least is twice the motor's minimum on this unit and powder:** in each of seven sessions both tubes pour at the slowest speed tried at 0.61 to 1.35 of their stream's rate.
- **A small move's settle time follows its own length** (seven sessions): 0.03 s for moves run 0.1 s (111 moves), 0.22 to 0.53 s for moves run 0.3 to 0.9 s. It is the move's powder arriving over its run; the A&D's own settling is too short to see beside it.
- **Today's stored settings** (`common.c`'s `load_config`): each module's record is read as its struct's size and a CRC32. A struct that grows fails the check, and the module's defaults are applied. The charge settings hold the thresholds, margins, back-off, precharge and colours; the scale settings hold the scale's make, baud rate and frame format.
- **The charge holds each tube to the smaller of the profile's maximum speed and the motor's** (`charge_mode.cpp`), and to the larger of their minimums. A profile has no maximum that is not set: a maximum of 0 holds the tube at its minimum speed.
- **The first build's calibration** ran on the PC through the console (`tools/charge_sim/staged.py`'s `measure_on`, driven by `bench/trial.py`): it measured the scale's settle time and, at each speed, its lag and what lands after a stop. It took 8 to 13 minutes and poured 164 to 252 gr on the bench. Those tools and every recording are kept in `tools/charge_sim`.

### To settle before approval

1. **Appending without a reset** (approved with gate 2 and built, 2026-11-25): today's loader would reset the charge settings and the scale settings (the scale's make and baud rate with them) on the update that appends items 1 to 4. Proposed: a record whose CRC32 fails at the struct's size is tried at each shorter size, from one byte less down to one, until a CRC32 matches; that size is the previous version's. Its bytes are kept, the appended fields take their defaults, and it is written once the update is kept, as defaults are today. It holds when:
   - fields are only appended at the end of a flat stored struct (never inside an array's element, such as one powder profile of eight);
   - the struct grows by them: `sizeof` checked before and after, since a field that fits in the old struct's tail padding leaves the size as it was and reads that padding;
   - a record that matches at no size is taken as damaged and gets the defaults, as today. A wrong match is a chance of one in about four billion for each size tried.

   Shown by an update from today's release that keeps every setting.

### What we do

The method is developed from the PC on the bench, where the tools exist; for users it is a job on the unit, run from the screen and the web. The gates, in order, each started once the one before it is done:
1. **The method,** in the simulation and then from the PC on the bench (`tools/charge_sim`, `bench/record.py`): the cup, the scale and each tube's speeds, each part run alone. Its recordings are kept, and they fix the records' form (how many points the lag holds).
2. **The records stored:** the loader's migration (above) and the fields appended to the charge and scale settings, each described once in its module's table, so the API, the console, the export and the backup follow. Proposed form: the charge settings gain the cup's capacity and its weight, in grains; the scale settings gain the settle time and the lag as five pairs of flow and lag, a point not measured kept as not a number (eleven floats, 44 bytes); each tube's least goes into the profile's own minimum speed. Shown by an update from today's release that keeps every setting, on both boards.
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

**Draft, for the owner's approval** (drafted 2026-11-25 at 17:01, at the owner's word, while step 1's test on a unit is deferred). Nothing of it is built until step 1 is signed off. It is drafted on step 1's records as built; their figures on a unit (the burst's settle time among them) come with step 1's test, and the draft is read again against them then.

### What we want

A throw lands as near its target as the powder allows, throw after throw: its spread narrow first, then its time short (a steady offset matters less than a wide spread). The user sets a target and a powder and throws; there is no tuning session. The predictor and the auto-tuner are not seen: the user sees throws, and one switch that turns the auto-tuner off.

### Owner's direction

- Step 2 is the predictor and the PID auto-tuner, and nothing runs beside them: no glide, no nudge as the final top-up, no other algorithm.
- Its input is step 1's records.
- Its safety: a cup lifted mid-charge told from a light reading, by the empty cup's weight.
- The fine threshold is the user's tolerance; nothing writes it.
- Each profile's maximum speed stays as the user has it.
- Earlier words still standing (2026-11-22, the first build): the predictor and the auto-tuner not visible; an auto-tuner switch, on by default; no tuning session. A throw is judged by the spread of its final weight, then by its time (2026-11-24).
- **The owner's answers to the draft (recorded 17:40):**
  - the tuner is `new_pid = tuner(old_pid, charge_data)`: it takes the powder's PID from its profile, judges the charges since, and stores its new PID back into the profile; no storage of its own;
  - enabled, it applies its changes by itself; it changes the gains over a window of throws, not each throw; off, it leaves the profile as it is (one controller, always);
  - its kd is tuned with its kp;
  - the same mechanism works in grams and in grains;
  - the charge controller is a module with a clear interface to the others, so a design other than tubes can replace it by a firmware change (not built: designed for);
  - the Pico W's room is not step 2's concern.

### What we know

From the first build's record (`legacy/` and branch `adr009_controller`'s progress) and step 1. On the bench's Pico 2 W, A&D FX-300i, AR2209.
- **The plant integrates:** the weight is the turns times the grains a turn (fine 0.71 to 0.88, coarse 9.4 to 10.5), shown late by the scale's lag (0.5 to 0.6 s behind a fast flow, about 1 s behind a slow one; step 1's table, 0.59 to 1.21 s, steady from day to day).
- **A forecast works where it decides:** the last reading plus each 10 ms parcel turned out and not yet due, each parcel due after the lag of the flow it was turned out at. At a stop from the creep the rest lay 0.003 gr from it, its spread 0.036 to 0.045 gr. One lag for the whole recent past double-counted a slowing tube; lags measured from the PC were 0.055 s long and stopped the coarse tube about 1 gr early (step 1 now measures on the unit).
- **The stop's spread has a floor:** what lands unseen behaves as a count of kernels, about 0.03 to 0.06 gr at the creep; part of it does not shrink with speed.
- **A gain from two measured numbers:** a fine kp under 0.37 / (lag x grains a turn), about 0.9, was stable, and 0.8 matched the owner's hand-tuned value.
- **The first build's tuner offer** (coarse kp = 1 / (grains a turn x T), fine kp = 0.6 / (grains a turn x T), T = 0.6 x the longest lag), with the PID fed the forecast inside an envelope (anti-windup, a speed cap): as good as the first build's glide within 20 throws and quicker, but more overs in two of three sets; from bad starting gains, 85 to 90% good with the envelope, 35 to 66% without.
- **Learning throw by throw:** the stop learnt at 15% of each throw's error, with a dead zone and a cap, held; it walked on noise where the stop's spread was wider than a digit, and a rule on the mean did worse. Learning the fine tube's grains a turn bought nothing measurable on the rig.
- **The PID's I and D:** the first build's research found they added nothing on the rig: the plant already integrates, the error never changes sign, and a derivative of a rounded, late reading is noise. On the forecast the D term is a different thing: the forecast moves with the speeds asked, at once and without the scale's steps.
- **Traps on record:** a cold fine tube pours short at first; the step count is unsigned, so the coarse back-off's reverse steps would count as forward turns (the forecast counts turns from the speeds asked); the throw's verdict is read 1 s after its stop today, before a slow stop's last powder shows; the rig itself drifts by about 4 good of 20 and 1 s within an hour, as large as most effects tried.
- **Step 1 keeps no grains a turn.** Its powder part measures it (the prime's stream rate) and drops it. The forecast needs it every throw.

### What it does

**The charge controller, a module.** Its core is plain C with nothing of the RTOS, the motors or the scale in it, as step 1's method (`calibration_method.c`); the charge mode drives it and the simulation runs the same C.
- **What it is given:** the target and the user's tolerance; the profile's gains and speed limits; step 1's records (the lag by flow, the settle time); and its channels, each a feed with its own rate (a tube's in turns a second), its least and most rate, and the stop point it works to (the coarse's, the fine's).
- **What it is fed:** each reading with its time, and the rate each channel was last set to.
- **What it gives:** each channel's rate, when each stops, and at the throw's end its record (below).
- **What stays outside it, in the charge mode:** the throw's states (the zero, the cup off and back), a missing reading's pause, a driver's fault, the verdict's wait, the lifted cup, the events and the lights. A design other than tubes replaces the channels and keeps the rest.

**The predictor**, inside the controller, each reading: the forecast is the reading plus each channel's powder turned out and not yet shown, in 10 ms parcels of its rate times the weight a turn, each due after the lag of the flow it was turned out at (step 1's table, straight between its points and the end's value beyond them). The weight a turn is estimated within the throw, from its own readings against the rates set a lag before; until it is known (the throw's first second or so), the forecast is the reading plus its own flow times the lag. Nothing of it is stored, so a powder's change of lot, a cold tube or a scale in grams is met in the throw itself. Each channel's PID works on the forecast's error, and each stops when the forecast reaches its stop point: the coarse at the target less its threshold, the fine at the target. The turns are counted from the rates set, not the step count, which has no sign.

**The PID's envelope:** the integral held while a channel runs at its least or most rate (anti-windup), and the fine channel's rate held under what the forecast can still take before the stop. The first build found it the difference between 35 to 66% and 85 to 90% good from bad starting gains. Part of the PID, not an algorithm beside it.

**The verdict** is read once the slowest lag and the settle time have passed after the stop, not after 1 s.

**Its safety:** a reading that falls by more than half the empty cup's weight during a throw is a lifted cup, not a light charge: the tubes stop and the throw ends, never pouring as if it were short. Never calibrated: today's 10 below zero.

**The auto-tuner**, after each throw, per powder: `new_pid = tuner(old_pid, charge_data)`.
- **Its data:** each throw's record from the controller (the target, the final weight, the forecast at the stop, the time, and the fine channel's rates near its stop), kept in RAM for the powder's window of throws; a restart begins a new window.
- **Its judgement,** over the window, in the target's own terms so grams and grains are alike: its spread first (the final weights' SD against the tolerance and against the stop's floor, the kernels' count), then its time. A wide spread with overs: the fine channel calmer (its kp down, or its kd up, whichever the window points to). A narrow spread and slow throws: its kp up.
- **Its change:** one gain at a time, by a factor (one step of 10%, its size chosen in gate 1), only when the window's evidence stands out of its noise; kp held under the bound from the lag and the weight a turn (kp under 0.37 / (lag x weight a turn), the first build's), ki left as the user has it.
- **Stored** into the powder's profile, its own fields, as a write through the settings, so the web and the export show the gains it reached.
- **The switch,** `auto_tune` in the charge settings, on by default: off, the profile is left as it is.

### What we do

The gates, in order, each started once the one before it is done:
1. **The method in the simulation** (`tools/charge_sim`, Python first): the controller's forecast judged on the kept bench series (its rest less the forecast at each stop: its mean near 0, its spread at the kernel floor), its weight a turn estimated within each throw against the first build's stored one; the tuner on the twin, from the owner's gains and from bad ones, against today's loop with the owner's gains, over the rigs kept; its window, step and kd's rules chosen.
2. **The controller and the tuner in C,** behind the interface above, judged against the Python on the same runs (step 1's `calibration_sim.py` pattern).
3. **The charge on the unit:** the charge mode on the controller, the verdict's wait, the lifted cup, the switch, the tuner's writes into the profile.
4. **Bench series:** the charge with the tuner against today's with the owner's gains, in alternating sets of 20 throws on the same powder and day, each reported as the final weight's SD, its mean offset, its time and a histogram in 0.02 steps. Step 2's sign-off.

### How the unit shows it works

- The final weight's spread is narrower than today's charge's on the same powder and day, and the throws no slower (the owner's bar, set in gate 1 from the twin's figures and the stop's floor).
- From a profile's gains set far off, the tuner brings the throws to the same spread within a few windows, the gains it reached in the profile.
- In grams as in grains, the same throws.
- A cup lifted mid-throw stops the tubes and ends the throw.
- With the switch off, the profile is never changed.
