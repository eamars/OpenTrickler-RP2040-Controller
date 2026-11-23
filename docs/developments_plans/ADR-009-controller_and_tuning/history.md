# ADR-009: the plans before the build plan (a record)

**A record, not a plan.** These three sections stood in the README until 2026-11-23 and are moved here word for word. Nothing is built from this page: the one plan is the README's "What we do: the build plan". Where this page says otherwise, the README is right. What each section was:
- **"Decisions proposed by the research (2026-11-19)":** proposed, never approved.
- **"What we do (planned 2026-11-21)":** approved 2026-11-21; gates 0 to 2 were built from it. Its two loops behind a setting, its refusal of a powder with no calibration and its tune by hand are superseded by the sign-off.
- **"Proposed for approval (2026-11-22, 21:45)":** approved 2026-11-22 with two changes, then held behind gate N. Its gates are gathered into the build plan.

## Decisions proposed by the research (2026-11-19)

**After the first bench session (2026-11-19, `progress.md`): D2 and D4 are open again.** This scale takes one to two seconds to show a kernel, so a finish that waits for it after every nudge does not fit a 7 to 15 s throw; and the owner's AR2209 clumps enough that clumps, not the controller, are the limit. `research/HANDOVER.md` has the findings, the interim conclusion and the next actions. D1, D3, D5, D6 and D7 stand.

Proposed, for the owner to weigh; none is approved. They come from the research in `research/README.md`, whose simulation rests on an assumed powder model, and they stand or fall with the first bench session (`research/bench_guide.md`). The owner left the design to the agent on 2026-11-19 and gave the figures in `progress.md`.

**D1. One demand, shared between the tubes; not two loops on one weight.**
- **Why two PID loops on one reading are poor practice.** Two controllers with integral action on one measured value have no settled state: each takes the other's work for its own error, and they drift against each other. Today's loops avoid that only because Ki is kept at 0 and the two aim at different weights, which is a split of the range by hand. What remains wrong with it:
  - each tube's gains were found with the other tube running, so they are tied together: a change to the fine tube's top speed moves where the coarse one stops;
  - the coarse loop's D term reads the fine tube's flow as its own;
  - nobody owns the whole flow, so "no more flow than the scale can show in time" cannot even be stated;
  - a third actuator (the servo gate) has no place in it.
- **The usual form** in process control is one controller and an allocation: split range for a coarse and a fine actuator, bulk and dribble in batch weighing.
- **Proposed:** one law gives the flow the weight still to go can absorb, flow = 0.37 / (the scale's delay + its reading interval) x (weight to go - margin). An allocation turns it into two speeds through each tube's measured flow map: the coarse tube until its share falls under the least it delivers regularly, then stopped for good; the fine tube for the rest; then nudges.
- **What the simulation says of it:** the three ways of sharing tried (both tubes from the start, the fine one waiting, one demand split) differ by less than the noise of 800 throws. So the gain is one invariant and gains that are not tied together, not seconds. Both tubes feed together early, as now: it costs nothing and is the quickest.

**D2. The finish is nudges judged on settled readings.** No floor speed. A nudge is a small turn of the fine tube sized to release under one kernel on average; the judgement is on the mean of the readings in a pause, which is finer than a digit; the throw ends at the first settled weight no more than the tolerance under the target. The pause is the scale's, so a slow scale gets a slow, sound finish by itself.

**D3. The scale and the powder are kept apart.** A scale's record holds its reading interval (measured already), its delay and its settle time. A powder's profile holds measurements, not gains: each tube's flow at six speeds, the highest speed worth using, the least regular speed, the kernel's mass, what a nudge releases, and the share of nudges that release three kernels or more. A profile then moves between scales, and a new scale needs no new powder tuning. The stored profile changes shape; ADR-014 migrates profiles in any case.

**D4. Oscillation before a changed lip** (the owner, 2026-11-19: a fine lip is hard to print). If the nudge test says clumps are the limit, oscillation and a forward-and-back flick are tried as kinds of nudge, each judged by the share of releases of three kernels or more and by the reading's noise while it swings. It needs one test command in the motor task. It is not designed in until the bench shows it helps.

**D5. The verdict is read when the scale has settled,** not one second after the stop, and a throw that hits its limit of nudges ends with a reason of its own.

**D6. Automatic tuning in two modes, on one set of building blocks** (the owner's second goal, 2026-11-19: a community whose printed tubes differ).

The blocks are jobs the firmware runs and bounds by itself, each recorded: a motor run (a tube, a speed, a time or a weight), a nudge series, a scale recording, a test throw, and the cup's account of what was poured (api_v1.md reserves all of them). A tuner hands out the next trial and takes its result; it never turns a motor itself. That is the seam both PRs arrived at.

- **Mode 1, autonomous: the unit decides, by fixed rules on the trials' statistics.** One button, about two minutes, into the cup:
  1. prime both tubes;
  2. each tube at six speeds with a stop after each: the flow map, the scale's delay, the settle time;
  3. thirty nudges: the kernel as the unit sees it, the yield, the nudge quality;
  4. the rules: the highest useful speed is the first that gives 95% of the most; the least regular speed is the lowest whose flow repeats within 30%; the nudge is the turn that gives 0.4 of a kernel; the coarse margin is three times the largest fall after a stop;
  5. **a verdict about the powder in words:** nudge quality under 3%, holds the tolerance; 3 to 10%, holds it slowly; over 10%, this powder in this tube will not, with what to try (the oscillating nudge if the build has it, or a wider band);
  6. ten throws to prove it, then trimming in use: a share of each throw's first settled error into the margins, with a cap, a dead band, and nothing learned from a throw that was disturbed or topped up.
- **Mode 2, assisted: an external agent decides, with the same trials.** It reads the unit's own description (`GET /api/v1` and its schema), starts trials, downloads their recordings, and writes the profile through the settings' paths, checked by the same tables as any write.
  - **What an agent adds:** choosing the next most telling trial instead of a fixed list; seeing what the rules do not look for (a once-a-turn ripple from an eccentric tube, an inlet that bridges, a gear ratio left from another generation, a scale left at 5 readings a second); fitting the PC simulation (`tools/charge_sim`) to the unit's recordings and trying settings there before spending powder; saying in words what it found.
  - **What it cannot do, by the firmware and not by its good will:** run a motor without a bound in time and weight, pour past the cup's account, write outside a field's limits, act during a user's charge. Every trial ends by itself.
  - **The unit never needs it.** Mode 1 is the product; mode 2 is the same blocks under another decision-maker, and what it does is in the event log under its name.
- **Simulated so far:** mode 1's steps 1 to 4 (`calibrate.py`), on three differently printed tubes and four scales: the candidate calibrated on each holds 11 to 16% over where today's loop with one fixed profile gives 20 to 100%. Steps 5 and 6 are designed, not simulated.

**D7. The simulation is kept, and refitted from each bench session.** Its controller interface (`start`, `on_tick`, `on_reading`; two speeds and `done` out) is the module's. The module's PC tests run against it; a recorded real throw replayed against it is its own check.

**Not weighed in D1 to D7: what it costs today's users to move** (the owner's direction of 2026-11-19, above). What these decisions would change for a unit in use, to be costed at "what to keep":
- a powder profile of measurements in place of six gains and four speed limits (D3): every stored profile, and every gain a user tuned by hand, has no place in it;
- the two stop thresholds and the floor speeds lose their meaning (D1, D2); a tolerance takes their place;
- a calibration run before a powder's first throw (D6), where today a profile is typed in;
- the verdict's moment and the throw's end reasons (D5), which the API and the event log give out today;
- a throw that sounds and looks different: the tubes stop and nudge at the end where they now creep.

Whether today's loop stays beside the new one, for how long, and whether old profiles are kept, converted or dropped is that step's to decide, not this research's.

**The order proposed,** each gate ending in a pass or fail on a unit: (1) the bench session; (2) recordings and the open-loop jobs in the firmware; (3) the nudged finish alone; (4) the flow law and the calibration; (5) the G&G's driver read at once after its ask, and the same controller on it; (6) mode 1's trimming, then mode 2's paths; (7) oscillation, if the bench says clumps.

## What we do (planned 2026-11-21, 20:58; approved the same evening)

The controller is the research's frame with proposal 2's finish by default (`research/proposals_2026-11-21.md`, sections 2 and 4), ported from its reference, `tools/charge_sim/staged.py`, as it stood after the bench trial. Its calibration is the routine of the same file's section 3.2. Nothing is designed again here: what the bench found weak is mended where the gates below say, and nothing else is added. The gates are built in order, each ending in a pass or a fail written before it is run. The early gates go for speed: the controller is proven end to end on the rig (gates 1 to 3) before anything is polished (gates 4 to 6). One PR, when the ADR closes, unless the owner picks a gate to merge on its own.

**Gate 0: the research onto main, and the branch.** The research branch (`adr009_research`: documents, `tools/charge_sim`, the recordings; nothing under `src/`) is merged into main as an ADR change, with one push. The build's branch starts from `origin/main`.

**Gate 1: the controller in C, checked at the desk.**
- `src/charge_control.c` and `.h`: `staged.py`'s controller and its rules (`Staged`, `set_rules`) in C. In: a start (the target, the time), each tube's turns since the start (the unit's own step count), a tick, each reading with its time. Out: each tube's speed, a small move as a number of turns, done, the verdict and its reason, and the marks a recording keeps (the coarse stop, the fine stop, each rest, each move, a return to the stream, the verdict). No FreeRTOS, no motor, no scale, no allocation: the powder on its way is a fixed array of parcels.
- The calibration is one C struct, the measured numbers and those the rules make from them; the rules run on the unit, so a calibration is stored as measured.
- **The PC twin** (the owner's answer): a host C compiler, its version written once in `toolchain.env` and fetched by both setup scripts, builds the module as a library that the simulation loads (`tools/charge_sim`, beside `staged.py`). It is a tool for the PC's checks only: the firmware's build and its compiler do not change. Read here as within the rule that keeps a dependency update out of a functional ADR, since no dependency of the firmware changes.
- **Pass:** on both records (`pico2w_ar2209`, `pico2w_bm2`), both finishes and both bands, 200 throws each on the same seeds: the C controller's verdict is `staged.py`'s in at least 99 throws of 100, and every throw's weight at rest within a digit of it; the trial's kept calibration (`recordings/20261121_162403_trial_pico2w_ar2209`) read into the C rules gives `staged.py`'s numbers. CI runs it, in about a minute.

**Gate 2: the controller in the charge, tuned on the rig.**
- **The motors** gain two things: a move made as a count of steps by the motors' task (the PIO makes only the steps it is fed, so the count is exact), and each tube's turns since the throw began, from the steps counted on its pin, read every millisecond. A tube between moves stays enabled at a rate of zero, never at a creeping speed. A start from zero is measured once on the unit, step count against command, before a move's timing is trusted (the bench's 0.7 s late start came from a held creeping speed, `step_pulses.c`).
- **The charge:** with the new controller, the job task runs it every millisecond, hands it each reading as it comes, and sets the two speeds. Kept from today, for both loops: the zero, the cup's removal and return, ADR-013's pause on a missing reading (in the new controller's step: no fresh reading for three of the scale's intervals, both tubes at zero), ADR-012's end on a driver's fault, the LEDs, the API's throw and its states.
- **Two loops, one setting** (the owner, revising): the charge's settings gain `controller`, `staged` (the new, by default) or `pid` (today's loop, as it is: its two PID loops, stop thresholds, back-off and precharge, untouched and not mended). The charge's state machine calls one or the other for the stretch from zero to verdict; everything around it is shared. The new field is a change of charge mode's stored layout, said here; it is checked against the struct's filler (AGENTS.md) and the record's size before and after. Each throw's recording names its controller (api_v1.md's `controller`, `pid` today).
- **A powder with no calibration is refused at the start under the new controller,** in words, with the two ways on: calibrate it, or choose the PID loop. The simulated scale gets a calibration of its own for `charge_cycle`, and stops holding its weight at the target within 0.1 gr, which would hide an over.
- **The calibration's stored form,** a change of the stored layout said here: one EEPROM slot for each of the eight powder profiles' calibrations (13 to 20 K; 12 K is the highest in use, and the CAT24C256 holds 32 K), each the whole calibration of that powder on that scale, with its CRC. No existing record changes. In this gate the trial's calibration is written into it from the PC; gate 3's job writes it on the unit.
- **Every throw recorded,** as api_v1.md reserves it (`/api/v1/recordings`: a summary, the marks, the samples): the newest throws' summaries and the last throw's samples in a fixed RAM budget, measured on the Pico W in gate 7. The PC's trial tool reads them into `tools/charge_sim/recordings/`, and a recorded throw is replayed in the simulation against the C controller.
- **The tune, hands on** (the owner's answer): the frame's constants against the bar below, each change committed with its throws and replayed in the simulation. Two weak points of the trial are looked at here, as constants or by one mend each: the coarse tube's last powder at its floor (5 throws in 100 ended 6 to 26 digits short after it) and the slow start of a small charge (2 gr of a 10 gr charge left to the fine tube).
- **Pass, on the rig** (Pico 2 W, A&D FX-300i, the air-exposed AR2209, 10 gr, 25 throws a set): within 0.02 gr at least 68% good with a median at most 13 s; within 0.04 gr at least 84% good; no throw short because of a fault. Every throw recorded and read back. `charge_cycle`, `pulses` and `menu_tour --compare` pass, and `motor` shows no late feed over the 50 throws.

**Gate 3: the calibration as a job on the unit.**
- A job like a cleanup (`jobs.c`), started from the powder's profile, with the cup step before it (a safety step, kept). It pours into the declared cup of api_v1.md and is bounded in time and in powder; each of its runs is a recording. Its steps are 3.2's, prime to rehearsals, with one mend: a rehearsal is a throw's last stretch, not a short stream from rest (its rehearsals spread twice as widely as real throws' stops). It ends with its verdict, the share of throws expected in the band and the seconds a throw, and stores the calibration on the user's yes.
- **Pass, on the rig:** its numbers against the trial's (grains a turn within 3%, each lag within 0.1 s); 25 throws a band on its own calibration meet gate 2's bar; under 15 minutes and 200 gr. Its recordings build a simulation record on the PC (`build_rig.py`), so any user's unit can be made a record the same way.

**Gate 4: the screens and the web, designed.** Mocks of both, by the skills `embedded-ui-design` and `web-ui-design`, for the owner to settle before anything is built: the calibration's row in a powder's profile, its progress and its verdict; the band and the finish as a profile's settings; a throw's new end reasons; a throw's recording drawn on the web; and what goes from both of today's screens.

**Gate 5: the screens and the web, built.** Every setting on both (screen and web parity). The menu's baselines saved again; `menu_tour`, `reach`, `api_check` and `settings_roundtrip` pass.

**Gate 6: side by side, and what to keep** (the owner's answer). The two loops on one firmware, chosen by the setting, on the same rig, powder and target: sets of 25 throws a band taken in turn (PID, new, PID, new), so a drift of the powder or the room falls on both. Then, with those figures, what is kept for users is decided with the owner: whether the PID loop stays as a choice, and the stored profiles' gains and speed limits, the two thresholds, the back-off and the precharge dropped, migrated or kept; the profile record's change, with ADR-014's, is said here.

**Gate 7: the Pico W, and a second powder.** The Pico W, since the motors' change and the millisecond's work in the job task are timing and memory: gate 2's checks and a charge set on it. BM2, when the owner loads it, its forecast written first: a calibration and 25 throws a band.

**Not in these gates, kept as ideas:** the calibration choosing the finish from its own moves and stops; the move chosen as the best play of the powder's odds (`bench/best_finish.py`); a shorter calibration that keeps the scale's lag from one powder to the next (section 3.2: within four points); the trimming of grains a turn from the throws.

## Proposed for approval: the remaining gates for the final firmware (2026-11-22, 21:45)

**Approved 2026-11-22 with two changes, then held behind gate N** ("Where ADR-009 stands", above). The two changes:
- gate S's pass is the rig's own run-to-run spread, not one throw in 20 and 1 s;
- gate F is built as steps with a pass each, the safety checks first.

The gates' text below is as proposed.

**What the night established** (every figure in `research/pid_learning_2026-11-22.md` and `progress.md`; the recordings in `tools/charge_sim/recordings/`, the 29 series of 2026-11-22 night):
- **The frame gives the accuracy, the speed law gives the time.** The frame is the forecast, a stop at a least speed set from it, one settled reading, the top-ups and the learning. Today's PID settings in that frame (`ccon speed_law=1`) threw within what 20 throws resolve of the glide's accuracy, and quicker.
- **Without the frame's forecast from the calibration,** one learnt figure overstates the fine tube's powder at speed (the A&D lags longer at a low flow), and the PID is slow.
- **Bad gains are guarded by the frame's speed envelope** (`ccon pid_envelope`). The bench unit's own stored gains, which put 10 throws of 12 over in today's loop, threw 15 of 20 good against the glide's 16.
- **Each law carries its own stop** (the PID law's list at EEPROM 21 K, a temporary form).
- **The auto-tuner's offer** (`ctune`) is 2 to 3 s quicker than the glide and costs overs. Its fine gain is a trade of time against overs, which this rig alone must not decide.
- **A fault in the carry since the 17:27 build:** first errors were translated to the placed stop room, not the rules' own, so the stop jumped between sessions. Mended and flashed at 21:26.

**The principle for what follows** (the owner): build the facility, not a tune of this unit.
- No number the controller uses is set from this rig's throws: each is a rule of the frame, or a unit's own calibration.
- The rig checks the facility; a second unit proves it.
- Where tonight's work slid (the agent's own reading):
  - **Constants of this unit in `charge_pid_learnt.c`:** a lag of 0.9 s and a rest of 2.5 s (the A&D's), a cautious start of 2.0 (raised after this rig's dry throws), a least speed of 0.02 rev/s and a coarse quiet of 1 s.
  - **`fine_top_rps=1.0`,** set by hand on the rig for every glide series since 11-21.
  - **The offer's fine rule,** chosen on this rig's two records only.
  - **The proposal to throw a slower offer on the rig,** a constant tried until the overs pass: withdrawn.

**The gates proposed,** in order. Each ends in a pass written before it is run. Gates 0 to 2 stand as built; gate 2 keeps its last piece.
1. **Gate 2's rest:** the API's recordings (`/api/v1/recordings`), as planned.
2. **Gate D, the design settled** (a design gate, before any build below):
   - The owner decides what the sign-off (below) leaves open in `controller_design_draft.md`. The sign-off already settles: one controller with no choice; the user's PID values staying where they are; the existing settings kept.
   - The register's first entries are made and approved.
   - **Closes** the README's "Open, to be discussed" item.
3. **Gate S, the simulation from the night's data.** From the recordings (`tools/charge_sim/recordings/`, the series of 11-21 and 11-22), model the two blind spots the rig found:
   - the scale's lag growing at a low flow (the rig's learnt figure 1.2 to 2.1 gr a turn of the window, the simulation's 0.75);
   - the stop's spread growing as the approach shortens (the glide 0.035, the offer 0.058 gr).
   - **Pass:** the simulation reproduces the night's ABBA results (each run's good and overs within a throw of 20 a side, its medians within 1 s), so the offer's trade can be judged there and not on a unit.
4. **Gate R, the records and the migration** (the draft's sections 6 and 7), before anything writes into them:
   - the powder profile's extension and the device record;
   - the powder-on-unit record, today's calibration slots grown, with the PID law's carried stops moved in from 21 K;
   - backup and restore of all of it through the API, the calibrations included (the console's `ccal` only, today);
   - a factory reset offering a backup first.
   - **Pass:**
     - an update from 2.0.3 keeps every profile and setting;
     - backup, factory reset and restore give the calibrations back;
     - a device record restored onto the other unit is kept "to check".
5. **Gate F, the frame made one.** One controller (`charge_control.c`) with:
   - **two predictor sources:** the calibration's, and the learnt figure of `charge_pid_learnt.c`, folded in for a powder not yet calibrated;
   - the two speed laws, the envelope, each law's carried stop;
   - the gate's waits and the preload in the forecast (issue #135), and the reverse's spill in the forecast;
   - the cup-lifted and no-powder checks.
   - **The constants above** go to the device record or the calibration: the scale's lag and rest from the device record, the frame's tops from the calibration's measured range.
   - Every debug choice goes from the user's reach (sign-off item 1): `controller`, the mode, `pid_learnt`, `speed_law`. The existing settings keep their function (item 3).
   - **Pass:**
     - the throw-for-throw check for both laws and both sources;
     - `robust_staged.py` with every case, `cup_lifted` and a new `no_powder` among them;
     - one ABBA on the rig for each law against the glide, meeting the night's bars.
6. **Gate 3, the calibration job,** writing the device record and the powder-on-unit record, as planned, and measuring as well:
   - each tube's lag at every flow, down to the creep;
   - the stop's spread against the approach time, with 40 or more throw-shaped rehearsals;
   - the reverse's spill.
   - **The PID law's offer is made from those:** its trade of time against overs chosen by the user's band and over budget, never a constant picked on this rig.
   - **Pass:**
     - as planned (its numbers against the trial's; 25 throws a band meeting gate 2's bar; `verify_series.py` on its throws);
     - and the offer made from it, thrown once against the glide, meeting the night's bars.
7. **Gates 4 and 5, the screens and the web:**
   - designed (mocks for the owner), then built;
   - adding "set" and "in use", the offer, keeping and forgetting what was learnt, a new lot, the backup and restore.
8. **Gate 6, the migration shown:** an update from 2.0.3 on both units, every setting and profile kept and working in the one controller (sign-off items 3 and 10), and today's loop's code removed once the results bar (item 11) is met against it.
9. **Gate 7, the Pico W, a second powder, and the facility's proof.** On a unit it was not built on (the Pico W at .168, the owner's own trickler with its own tubes), from nothing:
   - calibrate, take the offer, throw both laws, with no hand settings;
   - **pass:** gate 2's bar on that unit.
   - BM2 when the owner loads it.
- **Sign-off,** as before: the cup lifted and no powder flowing met on both units.

**Built tonight and to be folded or removed by these gates** (each experimental, for evaluation only):
- `controller=pid_learnt` and `charge_pid_learnt.c`;
- `ccon speed_law`, `pid_envelope`;
- the 21 K record;
- `ctune`;
- `series.py --controller pid_learnt --gains --profile-set --charge-set`.

