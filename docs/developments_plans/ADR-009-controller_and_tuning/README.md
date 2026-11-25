# ADR-009: calibration and automatic tuning

Status: **Open** 2026-11-25: step 1, the calibration, approved by the owner (recorded 01:22) and built in the gates under "What we do". Its test on a unit ran on the Pico 2 W the same evening (progress, 20:01 to 23:20): gates 1 to 4 pass, and the owner's redesigns found there (the knob's fill, the failure's words, a lifted cup and a silent scale mid-run) are built and seen on the unit; "The functional side is already verified and I'm satisfied" (the owner, 23:20). The web's steps then went into a sheet over the page, approved as drawn (23:37). Step 1's last items were closed at 23:50, and **step 1 was signed off on the Pico 2 W by the owner** (23:51). Every check on the Pico W is left to the ADR's end ("Last: the Pico W", below). Step 2, the tuning, approved by the owner (recorded 20:01) after gate 1 proved it in the simulation; its gate 2 built and judged against the Python on 2026-11-26 (progress, 2026-11-25 23:51 to 00:08); gate 3 next.

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
- The Pico W's work comes last, after step 2 and before the ADR closes (the owner, 2026-11-25, recorded 23:40: "Pico W work to leave to last of ADR-009"). Each step is signed off on the Pico 2 W; the Pico W's checks of both are under "Last: the Pico W".
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
5. **Step 1's sign-off on a unit:** the list under "How the unit shows it works", on the Pico 2 W; the Pico W's part is the ADR's last ("Last: the Pico W").

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

**Approved by the owner (recorded 20:01),** after gate 1 proved it in the simulation (2026-11-25, done 19:52; the owner, 17:45: unapproved until its idea is proven in the simulation). Drafted 2026-11-25 at the owner's word while step 1's test on a unit is deferred. Nothing of it is built until step 1 is signed off. It is drafted on step 1's records as built; their figures on a unit (the burst's settle time among them) come with step 1's test, and the draft is read again against them then.

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
- **The owner, on the forecast check (recorded 18:05):** "Predictor should only matter for last grain/gram of throw, so predictor and your simulation should only run with fine trickler, below coarse trickler stop threshold." Read here as: the coarse tube is today's (its PID on the reading, stopped once the reading reaches the target less its threshold), and the predictor serves the fine tube alone, which finishes the throw below that threshold.
- **The owner, on a confirmation after the cup is put back (recorded 20:59, at the bench):** "Only the part that needs human to put back the cup should have the confirmation." Its scope (the owner, 2026-11-26): "My scope, was just that single step of the calibration. Nothing more, nothing less." So the charge's throws go on as before: once the cup is back, the zero's wait (ten readings 300 ms apart, steady at zero) and the next throw starts. The calibration's cup step already waits for the person (the fill pours only as the knob turns), so nothing is added there. A first reading widened the words to a press before each of the charge's throws; it was built in gate 3 and withdrawn (progress, 2026-11-26).
- **The owner, on today's loop and the judgement (recorded 2026-11-26 00:25):** "There will be no selection. The user will use the charge control just like the old controller. In terms of the performance comparison, I guess the key criteria is if the auto tune can optimize the PID towards the "better" direction. I don't want to set a "target goal", as we don't touch the controller mechanism, only the parameters are adjusted." Read here as: step 2's charge replaces today's loop, with no setting to choose between them; the bench judges whether the tuner moves the gains the better way, by a good charge's cost from window to window, not against today's loop or a number.
- **The owner, on gate 4's baseline (recorded 2026-11-26 00:28):** "Approve, with my correction: we have the baseline. The baseline is the current untuned (or aka default PID stored on this unit). And the check is, if the auto tune can slowly optimize the charge towards the refined PID, i.e,. shorter time to trickle, and less over and undershoot." Read here as: the baseline is the charge on the gains stored on the bench's unit, before the tuner has moved them (its first window); the bench judges whether the tuner, window after window, brings shorter times to the stop and fewer throws over and under.
- **The owner, on the tuner's objective (recorded 18:54):** the time per good charge, chosen over the tolerance first and then the speed. Read here as: a window's median time to the stop over its share of throws within the user's tolerance (the fine threshold): what a good charge costs when every bad one is thrown again.

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
- **Step 1 keeps no grains a turn.** Its powder part measures it (the prime's stream rate) and drops it. The forecast needs the fine tube's, and the throw gives it (gate 1's check, below): with the coarse tube quiet, the reading's slope on the fine turns shown is as good at the stop as the first build's stored figure; while both tubes turn, the readings cannot tell their two weights apart.

### What it does

**The charge controller, a module.** Its core is plain C with nothing of the RTOS, the motors or the scale in it, as step 1's method (`calibration_method.c`); the charge mode drives it and the simulation runs the same C.
- **What it is given:** the target and the user's tolerance; the profile's gains and speed limits; step 1's records (the lag by flow, the settle time); and its channels, each a feed with its own rate (a tube's in turns a second), its least and most rate, and the stop point it works to (the coarse's, the fine's).
- **What it is fed:** each reading with its time, and the rate each channel was last set to.
- **What it gives:** each channel's rate, when each stops, and at the throw's end its record (below).
- **What stays outside it, in the charge mode:** the throw's states (the zero, the cup off and back), a missing reading's pause, a driver's fault, the verdict's wait, the lifted cup, the events and the lights. A design other than tubes replaces the channels and keeps the rest.

**The coarse tube** is today's: its PID on the reading's error to the target less its threshold, stopped once the reading reaches it.

**The predictor** serves the fine tube alone, below the coarse tube's threshold, where the throw's last grain or gram is decided. Each reading, the forecast is the reading plus the fine tube's turns turned out and not yet shown, in 10 ms parcels of its rate, each due after the lag of the flow it was turned out at (step 1's table, straight between its points and the end's value beyond them), times its weight a turn. The weight a turn is carried from throw to throw in RAM, nothing stored: the median of the last five throws' own, each taken once the throw is at rest, from its verdict's reading (the fine tube's powder since the coarse tube's powder went quiet, over its turns not yet shown then). The first throw after a start has none and takes the throw's own once a few tenths of a turn are shown (the slope of the reading on the fine turns shown, solved with the lags at its own flow); before that, the reading alone, as today. A powder's change of lot, a cold tube or a scale in grams is met within five throws. The fine tube's PID works on the forecast's error and stops the moment the forecast reaches the target (each millisecond, not only on a reading). The turns are counted from the rates set, not the step count, which has no sign.

**The PID's envelope:** the integral held while a channel runs at its least or most rate (anti-windup), and the fine channel's rate held under what the forecast can still take before the stop. The first build found it the difference between 35 to 66% and 85 to 90% good from bad starting gains. Part of the PID, not an algorithm beside it.

**The verdict** is read once the slowest lag and the settle time have passed after the stop, not after 1 s.

**Its safety:** a reading that falls by more than half the empty cup's weight during a throw is a lifted cup, not a light charge: the tubes stop and the throw ends, never pouring as if it were short. Never calibrated: today's 10 below zero.

**The auto-tuner**, after each throw, per powder: `new_pid = tuner(old_pid, charge_data)`.
- **Its data:** each throw's final weight less the target and its time to the stop, in RAM: the window being thrown (25 throws) and the throws thrown on the kept gains (the latest 75); a restart begins afresh from the profile's gains.
- **Its judgement,** the owner's objective: a window's time per good charge, its median time over its share within the user's tolerance (the fine threshold). Ratios only, so grams and grains are alike.
- **Its change:** the fine tube's kp alone, by a factor: each window tries the kept kp moved one way (by 1.5 at first). A trial whose time per good charge is lower than the kept gains' is kept, and the same way tried again at the full step; a trial no better is put back and the other way tried at once; both put back, the step's logarithm halves (down to 1.1) and the tuner throws three windows on the kept gains. A wrong keep is cheap (the cost is flat over a wide range of kp, 0.8 to 3.2 on the twin's rigs) and a wrong put-back dear (a slow kp costs three times), so a lower cost is enough. kd and ki are left as the user has them: on the forecast the D term only scales kp down (the error falls at the weight a turn times the speed), which the windows could not see.
- **Stored** into the powder's profile, the kept kp only (a trial stays in RAM), as a write through the settings, so the web and the export show the gains it reached; at most one write a window.
- **The switch,** `auto_tune` in the charge settings, on by default: off, the profile is left as it is.

### What we do

The gates, in order, each started once the one before it is done:
1. **The method in the simulation** (`tools/charge_sim`, Python first), the proof the draft's approval waits for. It is proven when:
   - **the forecast,** its weight a turn estimated within each throw, on the kept bench series: the rest less the forecast at each stop has a mean within a digit (0.02 gr) and a spread no more than a tenth wider than the first build's forecast on its stored weight a turn (0.036 to 0.045 gr);
   - **the tuner,** on the twin over the rigs kept: from the owner's gains, the final weight's spread narrower than today's loop's with them and the throws no slower; from gains set far off, the same spread within a few windows. (The owner set its objective at 18:54: the time per good charge. Judged then as: from the owner's gains, a good charge no dearer than today's loop's; from gains set far off, the same within a few windows.)

   Its window, its step and kd's rule are chosen there. If the forecast does not hold on an estimate within the throw, the owner hears it before anything else is done.

   **The forecast's check: passed for the fine tube alone** (progress, 2026-11-25, 17:58 and 18:05). Run first with both tubes on the forecast, it failed: while both turn, the readings cannot tell their weights apart. The owner then put the predictor on the fine tube alone, below the coarse threshold; there the throw's own weight a turn holds (the rest less the forecast +0.013 gr, sd 0.064, against the stored figure's +0.002, sd 0.063).

   **The controller and the tuner on the twin (progress, 2026-11-25, 18:21 to 19:52): passed on the owner's objective.** At the owner's gains step 2 is narrower than today's loop on every rig and centred, and slower; faster costs spread even with the rig's true weight a turn. The tuner first built on the spread first, then the time, did not converge; on the owner's time per good charge it does: from the owner's gains a good charge costs 14 to 19 s against today's 41 to 51 s on the AR2209 rigs and 15.5 against 16.1 s on BM2; from a quarter of the owner's kp, 10 sessions of 12 reach kp 1 or more within 300 throws (f2's first in four windows), one is at 0.55 and one still at 0.22; from four times it, all 12 end where the cost is flat.
2. **The controller and the tuner in C,** behind the interface above, judged against the Python on the same runs (step 1's `calibration_sim.py` pattern).
3. **The charge on the unit:** the charge mode on the controller, the verdict's wait, the lifted cup, the switch, the tuner's writes into the profile.

   **Gate 3's plan** (drafted 2026-11-26 00:21, **approved by the owner 00:28**). What we know from the code: today's charge loop runs in the job task and wakes only on a reading (`charge_mode_wait_for_complete`); the controller needs a tick each millisecond. The FreeRTOS tick is 1 ms; the heap is 144 KB on the Pico 2 W, 112 KB on the Pico W. The controller holds some 30 KB, nearly all of it the throw's parcels and readings. The charge settings have a revision and the loader's migration (step 1's gate 2 appended the cup's records the same way).
   1. **The controller's room:** its state split in two. The throw's room (parcels, readings, the air, about 29 KB) is taken from the heap when a charge starts and given back when it ends, as the calibration's runs are. A start that cannot have it is refused and says so. The carried weight a turn and the tuner (about 2 KB) are kept in RAM for good, so they last from one charge to the next.
   2. **The charge loop on the controller:** the loop in the job task wakes each millisecond, ticks the controller, and gives it each reading as it comes; it sets both tubes' speeds from it. What stays as today: the zero, the cup off and back, a missing reading's pause and a lost scale, a driver's fault, the servo gate at the coarse tube's stop, the precharge, the events and the lights. The fine tube's least is the larger of the motor's and the profile's (step 1 writes the profile's). The coarse tube's turns are not counted: the predictor serves the fine tube alone.
   3. **The verdict:** read once the slowest lag and the settle time have passed after the stop (about 1.9 s on the twin, 1.56 s on the bench's Pico 2 W by its step 1 records; today 1 s). The owner's original 1 s was evaluated at the owner's asking and no floor kept (progress, 2026-11-26). Then the throw at rest gives its own weight a turn to the carry.
   4. **The lifted cup:** a reading that falls by more than half the empty cup's weight during a throw stops both tubes and ends the throw, said as "Cup lifted" (a new reason, `cup_lifted`, in the events and the API); never calibrated, at today's 10 below zero.
   5. **The tuner:** after each throw's verdict, its weight less the target and its time to the stop; a window closed, the next window's kp is the tuner's. A trial's kp lives in RAM only, given to the controller and never written into the profile. A kept kp is written into the powder's profile and stored, as a write through the settings by `unit` (the unit itself; only the tuner writes a profile so), so the web and the export show it; at most one write a window. The carry and the tuner begin afresh at a start of the unit, at a change of powder, and when the scale's unit changes (a weight a turn in grains is not one in grams).
   6. **The switch:** `auto_tune` in the charge settings, on by default, on the screen and the web through its table. Off: no window is judged and the profile is never written. **A stored struct's layout changes:** the field is appended to the charge settings, its revision raised, and the loader keeps every earlier setting (the host test extended).
   7. **Withdrawn** (2026-11-26): a press before each next throw, built on a reading of the owner's 20:59 words that went past their scope (above); unwound, the charge's flow between throws as before. What stays of it: the owner's rule for the web, one key, Start or Stop, never both ("For the web UI, given stop and start are mutually exclusive, they can share the same button (not refering to the implementation. This is from user's experiences perspective). I don't think at any time both button should be enabled."), already the web's (`web-ui-design`, rule 17).
   8. **What it costs, measured:** the controller's time each millisecond and each reading, its most and its mean, and the heap's low mark during a charge, given by `stats`.

   **How the unit shows it, on the Pico 2 W** (the bench, when the owner says it is ready):
   - `step2_sim.py` and the host tests pass; both boards build.
   - flashed by the probe, the settings copy equal before and after, `auto_tune` on;
   - `menu_tour` against its baseline (pages meant to change saved again), `reach`, and `charge_cycle` with the simulated scale;
   - real throws of 10 gr on the bench's powder, each stopping on the forecast, the verdict read after its wait, and the carry renewed (the console's log);
   - a cup lifted mid-throw by the owner: both tubes stopped, the throw ended as "Cup lifted";
   - a window of 25 throws closed, the kept kp in the profile (the web and the export), and with the switch off no write;
   - the controller's time each tick and each reading, and the heap's low mark.

   **Today's loop goes** (the owner, recorded 00:25): no choice of controller; the user charges as before, on step 2's. Gate 4 is judged by the tuner's direction, not against today's (below).
4. **Bench series:** the charge with the tuner on, on the bench's powder, from the PID stored on this unit, untuned: its first window is the baseline (the owner, 00:28). In windows of 25 throws, each reported as its median time to the stop, its shares over and under, the final weight's SD and mean offset, a histogram in 0.02 steps, its time per good charge, and the kp it was thrown on. Judged by its direction: the tuner moves the charge from the baseline toward a refined PID, its throws quicker and fewer of them over or under, window after window; no target is set. Step 2's sign-off.

### How the unit shows it works

- From the PID stored on the unit, untuned (the baseline, its first window), the tuner moves the charge toward a refined PID: shorter times to the stop and fewer throws over and under, window after window, the kp it reached in the profile. No target is set (the owner, 2026-11-26).
- In grams as in grains, the same throws.
- A cup lifted mid-throw stops the tubes and ends the throw.
- With the switch off, the profile is never changed.

## Last: the Pico W

Every check of ADR-009 on the Pico W is left to its end (the owner, 2026-11-25, recorded 23:40: "Pico W work to leave to last of ADR-009"): after step 2's sign-off on the Pico 2 W, and before the ADR closes. The ADR does not close without them. Read here as the bench's work on the Pico W; both boards are still built on every change, here and in CI, so an image that no longer fits the Pico W's slot is met when it happens, by AGENTS.md's order of remedies.
1. **Step 1's gate 2 on the Pico W:** an update from v2.0.3 keeps every setting (`rc_run.py update`, its check field by field). v2.0.3 is built in the worktree `C:\workspace\OpenTrickler-v2.0.3`; it is flashed with that worktree's own `rc_run.py`, which matches its version.
2. **Its menu:** `menu_tour --compare tests/menu_baseline/pico_w`, its baseline saved again for the pages the calibration and step 2 change; `reach`.
3. **A calibration from its screen:** the cup's part, with the knob's fill, and a powder's.
4. **Step 2's charge on it:** what AGENTS.md asks of a change in memory and timing on both boards, at least a charge with the tuner on and `stats tasks`.
