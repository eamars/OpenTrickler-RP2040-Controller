# ADR-009: one controller, every throw recorded, automatic tuning on top

Status: **Open** since 2026-11-21. **Ready for the owner's approval of the parameter register; nothing more is built before it** (2026-11-23, 17:48). The sign-off, what ADR-009 delivers, is final since 2026-11-22. The one plan is "What we do: the build plan" below. Gates 0 and 1 are done; gate 2 is built and thrown but for its recordings. The plans this one was gathered from are kept in [history.md](history.md), a record. The research is in `research/` and `progress.md`. Drafted 2026-11-13.

## Where ADR-009 stands (read this first; it wins over every other page and section)

Written 2026-11-23, 17:48, by the agent who reviewed gate N for the owner. It replaces the section of 2026-11-22, 22:46. It is the agent's understanding of what is settled, not a quote.

**The state:**
- **The documents are ready for the agent who builds.** There is one plan (the next section). Every number the charge uses has its life cycle, checked against the code ([numbers.md](numbers.md)). The parameter register is drafted, each row with its recommendation.
- **One thing is waited for: the owner's approval of the register** (sign-off item 2). It closes gate N's documents and gate D together.
- **Then the first build step is N.2** of the plan, at the desk.

**What "ready for the agent who builds" means,** the test these pages were written against:
1. **One plan.** One section says what is built, in what order, and how each gate is judged. No other page gives an instruction that differs from it.
2. **Every number has its life cycle:** its default, who modifies it, who calibrates it, who uses it, and the gate that makes it so.
3. **No question in the plan is left for the owner.** Each is settled from the record, or is a row of the register with a recommendation, or is tied to a gate with its pass and a value in force until then. None stops a gate from starting.
4. **Each gate has its pass before its run,** and names the unit that shows it.
5. **It is not a promise of values.** A rule's value is proven where the sign-off says: on a unit it was not developed on (gate 7).

**Settled. Do not ask the owner again:**
- **What ADR-009 delivers:** the sign-off below, final.
- **Which algorithm:** the PID law pours every throw, inside the glide's frame. The night's experiments of 2026-11-22 decided it (`research/pid_learning_2026-11-22.md`, the fourth to sixth experiments). In that design:
  - the user's PID values are its starting point;
  - the frame's predictor runs beside it, and its speed envelope steps in where the PID would run faster than the tube can still be stopped from;
  - the glide's good parts are the frame: the forecast with lags by flow, the stop at the creep, one settled reading for the verdict, the top-ups, the bounded learning, the carried stop, the reverse with its spill counted.

  The glide's own stream law is not the output. It stays in the controller's code, checked throw for throw in CI, and runs in shadow in a throw's recording. No new algorithm is designed in ADR-009.
- **The auto-tuner writes the powder profile while it is on.** It writes the PID values it derives from the measured numbers (sign-off item 4); the calibration writes them on the user's yes.
- **After an update, the profiles are kept but not trusted** (2026-11-23): the user's PID values stay as the starting point, the user is told the old tuning will likely no longer hold and is prompted to calibrate. A charge before a calibration is allowed and has no results bar (sign-off items 4 and 10). How far it falls short is in `numbers.md` (the fallback study).
- **The auto-tuner's switch is one for the unit,** under Settings (advanced), on by default.
- **The gates and their order:** approved 2026-11-21 (gates 0 to 2) and 2026-11-22 (the rest), with two changes: gate S's pass is the rig's own run-to-run spread, and gate F is built in steps, the safety checks first. The build plan below is those gates, gathered.
- **A general-purpose controller, tuned on each user's own device.** What is measured on the bench unit shows what a calibration must measure; it is never a rule, a default or a constant in the firmware. A number is an existing setting (kept, or written by the calibration), a new parameter the owner approved (a general default, measured by the calibration, then changeable by the user and the user's agent), or a fixed number of the method that passes `numbers.md`'s test. The build plan's guideline and its checklist before each gate hold every gate to it.

**Decided from the record in the review of 2026-11-23** (the page of 2026-11-22 had left them as questions; the evidence for each is in `numbers.md`, "Decided in the review"):
- **The fine figure's learning is off** until gate 3 has measured the fine tube's lag across the user's speed range. Gate 3's study then says whether it is part of the auto-tuner.
- **The spread multipliers stay rules** at their present values. They are not made from an over budget.
- **The series' 600 gr is the bench's.** A series is started from the console only.
- **What a unit never calibrated starts from** is a general assumption, found on every stand-in scale in gate F.4: not the bench's A&D's lag and rest, and no starting figure of 2.0 gr a turn (corrected the same evening: the afternoon had kept both).
- **The 15 fine turns and the cup tests** are decided in gate F's first step, by cases written before the build.
- **Of the design draft's eight decisions:**
  - 1, 3, 4 and 8 are settled by the sign-off and the lines above;
  - 2 and 7 are the register's row 9, the stored form;
  - 5 is its row 8, issue #135;
  - 6 is decided by sign-off item 5: another unit's own data is never applied to this one, so the draft's "kept to check" goes.

**What the review found that the page of 2026-11-22 had not** (`numbers.md`, "The audit"):
- **The inventory had not been checked against the code.** Checked, it missed 34 entries, about 75 numbers, and 14 rows described the code wrongly. All are in now.
- **A second of waiting before every verdict,** in the new loops too.
- **The Pico W keeps no throw's rows** (1 row against the Pico 2 W's 800).
- **The bench's series can leak into a user's next charge** after a refused start.
- **Two forecasts cut a long lag short without saying so** (2.56 s and 1.28 s).
- **The settings take values the frame cannot use:** a tolerance of 0, a profile with every speed 0 (profiles 4 to 7 as shipped).
- **Nine numbers were the bench unit's own,** three of them in this review's first decisions (`numbers.md`, "The rows against the test"). Each is now measured, made from a user's setting, or tried at nothing.

**For the owner's word:** the register's rows, below. Those marked ★ change what a setting a user has does today. Approved so far: the auto-tuner's switch (row 1, but what "off" keeps), and that the method's fixed numbers stay fixed (row 10; 2026-11-23). Every other row waits.

**Pages that are records, not instructions:**
- [history.md](history.md): the plans of 2026-11-19, 2026-11-21 and 2026-11-22 night, word for word.
- [controller_design_draft.md](controller_design_draft.md): the draft of 2026-11-22. What survives of it is in the build plan and the register.
- `research/HANDOVER.md`: everything below its top section. Its findings and its traps at the bench still hold.
- Under "Owner's direction" below, "Four answers for the build" (2026-11-21): the jog as a second setting, two loops behind a setting and a tune by hand are superseded by the sign-off.

## What we do: the build plan (2026-11-23)

One plan, in order. It gathers the gates the owner approved on 2026-11-21 and 2026-11-22 with what gate N found. **What is new in it is marked (new)** and waits for the owner's word together with the register. Each gate ends in a pass or a fail. Where a pass leaves a figure open, the agent writes it into `progress.md` before the run, with the clock's time. One PR when the ADR closes, unless the owner picks a gate to merge by itself.

**Done:** gate 0 (the research on main), gate 1 (the controller in C, checked against its reference in CI), gate 2 but for its recordings (the controller in the charge, thrown on the Pico 2 W; `progress.md`).

**The guideline for every gate:** ADR-009 builds a general-purpose powder trickling controller, tuned on each user's own device.
- **The bench unit is one user's unit.** What is measured on it (its scale, its tubes, its powders, its throws) shows what a calibration must measure, and checks that a mechanism works. It is never a rule, a default or a constant in the firmware.
- **A number the controller uses is one of three things** (`numbers.md`, its classes and its test):
  - an existing setting, kept as it is or written by the calibration and the auto-tuner;
  - a new parameter, only with the owner's approval and its justification in the register: its default a general assumption, measured by the calibration, and afterwards changeable by the user and the user's agent;
  - a fixed number of the method, in the unit's own terms (a count, a share of something measured or of a user's setting, digits, intervals), with a general reason, and held on a unit it was not developed on.
- **No new parameter is the aim.** One is proposed only when no existing setting can hold the number.

**The checklist before each gate.** The agent answers it in `progress.md`, with the clock's time, before the gate's first change, and again before its pass is claimed:
1. **Which numbers does this gate add, move or change?** Each has its row in `numbers.md` and is one of the three things above.
2. **Does any value come from the bench unit?** A weight, a speed, a time or a figure that another scale, tube or powder would change is the calibration's to measure, or a line of the yardstick. It is not written into the code, the reference or a default.
3. **Does each fixed number pass the test?** In the unit's own terms, with a general reason, not "it worked on the bench".
4. **Is a new parameter needed?** Stop: its row in the register and the owner's approval come first.
5. **Does the gate's pass on the rig check a mechanism,** against a prediction written first? A series thrown to pick a value is tuning, and is not run.
6. **What here is still shown on the bench unit only?** It is named in `progress.md` as waiting for gate 7's unit.

**Rules for every gate:**
- **The bench:** ask the owner before any contact, in every session. After a session run `tools/charge_sim/bench/keep.py --logs logs/adr009`.
- **Nothing on a unit is set by hand** but by running the calibration itself (sign-off item 6). A value found by hand goes into the yardstick, never into the code.
- **A new mechanism goes to the owner first.** The plan builds what the experiments chose; it designs no algorithm.
- **A number added has its row** in `numbers.md` in the same commit, in its class (N.2's check holds it).
- **Simulation first:** a change to the controller is made in the reference (`staged.py`) and the firmware together, and the throw-for-throw check passes before a unit is flashed.
- **Today's loop is not mended:** it is the code this ADR replaces.
- **Every stored layout change is declared** below ("Stored layout changes"), with its sizes before and after.

### Gate N: every number's life cycle

**Before each step:** the checklist above, answered in `progress.md`.

- **N.1, the documents:** `numbers.md` and the register. **Closes** on the owner's approval of the register.
  - **Gate D,** the design gate of the plan of 2026-11-22, closes with it (new): what the sign-off left open in the design draft is the register's rows 1, 2, 8 and 9.
- **N.2, the numbers made visible in the code (new).** At the desk, changing nothing a throw does:
  - every Rule and Safety number of `numbers.md` (sections C, D, E, G) becomes a named constant in one header, named as the reference names it, each with its row and its reason in a line. No number stays inside a formula;
  - duplicates become one definition (D16, the copies in G5 and G8);
  - a check in CI lists the numbers in the controller's files and fails on one `numbers.md` does not name; each row gains its constant's name;
  - the fine figure's learning is off by default (C13), in the reference and the firmware together;
  - mended in passing, each its own commit: the bench's series cleared when a start is refused (finding 12); the zero's comment (E6).
  - **Pass:** the throw-for-throw check passes on both laws (the fine learning's default changed in both); both boards build; the CI check fails on a number planted without a row and passes without it.
- **N.3, the bench's leftovers tried at nothing (new).** Three fixed numbers are times or weights chosen on the bench's scale, and fail the test for a fixed number: the stillness's 0.4 s (C16), the rest's extra 0.3 s (D10), the coarse room's extra 0.2 gr (D9).
  - In the simulation, each is set to nothing, on both records, every stand-in scale and both tolerances, with `numbers_study.py`.
  - **Its pass, written before the run and read as the sensitivity study was** (how often and how consistently a case moves, against two seeds of the baseline): a number without effect is removed. One with an effect is made from what the unit measures (reading intervals, a measured spread). None is kept as the bench's value.

### Gate 2's rest: every throw's recording through the API

**Before it:** the checklist above, answered in `progress.md`.

- `/api/v1/recordings` as api_v1.md reserves it: a summary, the marks, the samples. The newest throws' summaries and the last throw's samples are kept in a fixed RAM budget.
- **The rows** carry the controller's own columns (`ctl_`): the forecast, each tube's speed, and the glide law's asked speeds in shadow (new: the plans before said "at most a shadow"; this is where it is built). A speed a profile allows is recorded true (E23).
- **The Pico W** keeps 1 row today (finding 11). Its budget is found by measurement; a throw longer than its rows is thinned, not dropped.
- The PC's tools read a throw through the API; the console's `ccrec` stays for the bench.
- **Pass:** a series' throws read through the API are row for row the console's; `api_check` passes with the recordings in the index; `soak --load` with a recording's real size shows no late step feed and no failed allocation; the RAM a recording takes is written in `progress.md`. The Pico W's figures are gate 7's.

### Gate S: the simulation from the night's data

**Before it:** the checklist above, answered in `progress.md`.

- From the recordings (`tools/charge_sim/recordings/`, the series of 11-21 and 11-22), model the two blind spots the rig found:
  - the scale's lag growing at a low flow (the rig's learnt figure 1.2 to 2.1 gr a turn of the window, the simulation's 0.75);
  - the stop's spread growing as the approach shortens (the glide 0.035 gr, the offer 0.058 gr).
- **Pass:** the simulation reproduces the night's ABBA results, each run's good, overs and median within the spread that like runs on the rig showed among themselves. That spread is taken from the recorded series and written in `progress.md` before the run (two glide runs alike differed by 3 good throws in 20). Then the offer's trade is judged in the simulation, not on a unit.

### Gate R: the records, and nothing lost

**Before it:** the checklist above, answered in `progress.md`.

- **The stored form** (the register's row 9):
  - the powder profiles' record at 9 K stays as it is;
  - **the unit's record,** a new slot: the scale's digit, and its lag and rest as last measured on this unit;
  - **a record for each powder on this unit,** today's calibration slots at 13 to 20 K, grown: the calibration as measured, what the auto-tuner learnt, the carried stops. One law pours, so one list of stops: the 21 K record moves in and its slot is freed;
  - out of the record: the band and the finish (B9). The debug byte of the charge settings (A18) is read as nothing and written as zero.
- **Each measured field is described once, with its bounds,** as a setting is (`settings.h`). The API reads the records (a read changes nothing), the schema lists them, the backup holds them.
- **The life cycle the code keeps:**
  - a record is read at a charge's start, checked by its CRC and version, and never read during a throw;
  - a record that fails its check is not used, and the screen says so;
  - a record is written between charges only: by the calibration on the user's yes, or by the auto-tuner at a charge's end, and then only when at least 5 throws came to rest, the scale was real, and the change is within its bound.
- **Backup and restore** of everything through the API. A factory reset offers a backup first. A restore never applies another unit's own data (the unit's id).
- **Pass:**
  - an update from 2.0.3 keeps every profile and setting (`rc_run.py settings --compare`);
  - backup, factory reset and restore give every calibration back;
  - another unit's backup imported leaves this unit's own data as it was (new: sign-off item 5 decides it, where the plan of 2026-11-22 said "kept to check");
  - a slot corrupted from the console is not used, and is said;
  - each record's size checked before and after; `settings_roundtrip` and `api_check` pass.

### Gate F: the frame made one

**Before each step:** the checklist above, answered in `progress.md`.

Five steps (new: the steps were approved, their list is this plan's). Each is made in the reference and the firmware together and ends in its own pass: the throw-for-throw check and `robust_staged.py` at the desk, then the proofs on the unit on the probe.

- **F.1, safety first.**
  - **The cup:** lifted mid-throw stops both tubes at once; its return is judged within the zero's mean margin; its removal loses the fixed 10 (E7, E8, E19, E20).
  - **No powder flowing** (an empty hopper, a jammed tube, a gate that did not open): the forecast holds powder that has not shown for longer than its lag; both tubes stop, the throw ends "no powder", nothing is learnt from it. The 15 fine turns (C19) get their form with it.
  - **No weight in grains, and none of the bench's:** the far-over stop is 100 times the user's tolerance (C20); the coarse room's extra is what N.3 left of it (D9).
  - **The safety bounds are out of reach** of the calibration, the learning and the API.
  - **Pass:** `robust_staged.py` with every case, among them `cup_lifted`, `no_powder`, a cup lighter than 10 of the scale's unit, a scale set to grams, a scale drifting a digit in a throw (no false stop), a tube pouring a third of the bench's grains a turn (no false limit). Then `charge_cycle`, `pulses` and `menu_tour --compare` on the unit.
- **F.2, the user's settings in the frame.**
  - **One tolerance:** the fine stop threshold is the band of every law (A7). The debug mode's bands go from the controller (E14).
  - **The user's limits:** each tube runs between the higher of the profile's and the motor's least speed and the lowest of the profile's max, the motor's max and the fastest speed the calibration measured (A2, A4, A5, A13, A14). `fine_top_rps`, `coarse_top_rps` and `coarse_floor_rps` go (C3 to C5).
  - **The calibration's ladder follows the same limits,** in the reference and the PC's trial tool (F1, F2, F7). The bench unit is calibrated afresh with it before this step's throws: until gate 3 the PC's trial is the calibration, and running it is not tuning.
  - **The coarse stop threshold** is the coarse stop of a powder with no calibration (A6).
  - **Two checks at a charge's start,** refused in words: a tolerance under a digit, and a profile whose top speed is 0 for a tube the throw needs (A20).
  - **Pass:** the throw-for-throw check; in the simulation, a user's max below the fastest measured speed is kept, and a least above the creep holds it and is said; on the unit, with no `ccon` set, a short series' recordings show each tube inside the user's limits and the tolerance taken from the setting. The rig's ABBA is F.5's.
- **F.3, the features a user has.**
  - **The back-off** made by the frame, its spill in the forecast as the calibration measured it at the user's turns and speed. With no spill measured as it is now set, the coarse tube stops by the threshold (A11).
  - **The precharge** made once the gate has closed, its powder counted in the next throw's forecast (A12).
  - **The gate** (issue #135): every move waited for by its own travel, and 500 ms more after a close; the tubes start when it is open; the move at the coarse stop waited the same way (A10, A17, E10).
  - **Pass:** the simulation with a gate (a store with a travel time) and a precharge; `charge_cycle` with each feature on; a servo on the connector seen to move at each point. A gate fitted is sign-off item 9's.
- **F.4, one predictor, two sources.**
  - **Calibrated:** the powder's record, lags by flow.
  - **Learnt:** `charge_pid_learnt.c`'s figure, folded into the frame for a powder with no calibration. Its scale numbers come from the unit's record when the unit has one. The controller `pid_learnt`, its constants and its copies go (G3, G5, G6, G8, G10).
  - **A unit never calibrated starts from general assumptions, none of them the bench's** (G1, G2, G4):
    - the digit is the smallest step its own readings show (B1);
    - the figure starts from what the throw itself shows, the weight risen for the turns made; until then the frame forecasts nothing, as today's loop does. The 2.0 gr a turn goes;
    - the scale's lag and rest are assumed on the slow side, so an unknown scale errs short. Their values are found in the simulation on every stand-in scale, the pass written first: on none are a fresh powder's first three throws over more often than a calibrated unit's. The A&D's 0.9 s and 2.5 s go.
    - If no such values exist, or the figure's start is not enough, that is a finding for the owner, not a number to tune.
  - **One interval:** the scale's rhythm measured live (B2).
  - **The forecast's window** covers the longest lag in the record, and a lag beyond it is said, never cut silently (C23).
  - **The simulated scale teaches nothing.**
  - **Pass:** the throw-for-throw check for both sources; a powder with no calibration thrown in the simulation on every stand-in scale ends each throw safely (no limit passed, no fault); the slowest stand-in scale's lag kept whole; on the unit, a powder with no calibration thrown, and `charge_cycle`.
- **F.5, one controller to the user.**
  - **Out of the API and the screens:** `controller` and `mode` (A18). The envelope is always on (C22).
  - **Gone:** the jog finish (C6, D5, D11); the clamps that never act (D1, D7's and D8's limits; D3 in words); the second of waiting before the verdict (E9); `ccon`'s writes (it lists).
  - **On the console only, never stored, for the comparisons the gates ask:** today's loop, and the glide's pour.
  - **In:** the auto-tuner's switch, a setting of the unit (the register's row 1).
  - **Pass:** the API's index and schema, `menu_tour` and the web's pages carry no such choice; `api_check`, `settings_roundtrip`, `reach` pass; one ABBA on the rig, the controller against the glide's pour, meets the night's bars (settled good at most 2 fewer in 20, overs at most 1 more, the median at most 2.5 s longer).

### Gate 3: the calibration as a job on the unit

**Before it:** the checklist above, answered in `progress.md`.

- **A job like a cleanup** (`jobs.c`), started from the powder's profile, the cup step before it (a safety step, kept). It pours into the cup the user declares (api_v1.md, "The cup"), is bounded in time and in powder, and can be stopped. Each of its runs is a recording.
- **Its steps** are the research's (`research/proposals_2026-11-21.md`, section 3.2), prime to rehearsals, in the firmware's own time, with what the rig showed must be measured as well:
  - each tube's lag at every flow of the user's range, down to the creep (B6, F1, F2);
  - the grains a turn as a throw uses them (B3);
  - the stop's spread against the approach time, with 40 or more rehearsals, each a throw's last stretch (B8, F5);
  - the back-off's spill at the user's turns and speed (A11);
  - the small moves, made at a speed from this unit's own measured ladder, which the top-ups then use; not the bench's 0.2 rev/s (B7, C1);
  - its prime and its rehearsals in the unit's own terms: speeds inside the user's range, a rehearsal the size of the coarse room (F5, F6).
- **It writes** the powder's record and the unit's record, and ends with its verdict in words: the share of throws expected in the tolerance, the seconds a throw, and what holds it back (a least speed above the creep, clumps).
- **The offer:** the PID values made from those (D19). The fine factor comes from this unit's measured trade of approach time against spread, the user's tolerance and the over budget of one in fifteen. Written to the profile on the user's yes.
- **One study, its pass written first, on the job's own measurements:** the fine figure's learning, part of the auto-tuner or not, with one figure or two (C13, C15).
- The PC's trial tool and its lag shift go (F8).
- **Pass, on the rig:**
  - its numbers against the PC trial's: grains a turn within 3%, each lag within 0.1 s;
  - on its own calibration, the offer taken: 25 throws at each tolerance meet gate 2's bar (within 0.02 gr at least 68% good with a median at most 13 s; within 0.04 gr at least 84% good);
  - one ABBA against the glide's pour meets the night's bars;
  - under 15 minutes and 200 gr;
  - `verify_series.py` on its throws;
  - the yardstick (below) reached with no hand settings;
  - its recordings build a simulation record on the PC (`build_rig.py`).

### Gates 4 and 5: the screens and the web, designed, then built

**Before it:** the checklist above, answered in `progress.md`.

- **Gate 4, mocks for the owner** (the skills `embedded-ui-design` and `web-ui-design`), before anything is built:
  - the calibration: its row in a powder's profile, the cup step, its progress, its verdict, the offer and the yes;
  - after an update: the notice that the old tuning will likely no longer hold, and the prompt to calibrate (sign-off item 10);
  - Settings (advanced): the auto-tuner's switch; this unit's own data, read and changed there, with its export, import, backup and restore; "forget what was learnt" for a powder (sign-off item 5). How a table of measurements is changed on the 12864 is this gate's to draw;
  - what is in use where a setting is not: the coarse room beside the coarse stop threshold, a least speed holding the creep;
  - a throw's new end reasons, and a throw's recording drawn on the web;
  - an agent's change to this unit's data, and the user's yes to it (new: sign-off item 7's place in the plan);
  - the servo gate tuned by eye (new: its design gate, "A gate: the servo gate tuned by eye" below, is taken with this one).
- **Gate 5, the build:** every setting on both the screen and the web. The agent's writes: each checked against its bounds, backed up first, logged as the agent's, revertable, stored on the user's yes.
- **Pass:** the mocks approved by the owner; the menu's baselines saved again; `menu_tour`, `reach`, `api_check` (the agent's actions among them: a write outside its bounds refused, a write reverted, the event log naming the agent) and `settings_roundtrip` pass.

### Gate 6: the migration shown, and today's loop gone

**Before it:** the checklist above, answered in `progress.md`.

- An update from 2.0.3 on both units: every setting and profile kept (`rc_run.py settings --compare`), the notice and the prompt seen, a charge thrown before a calibration ending safely.
- **The results bar** (sign-off item 11), its ABBA series written first: the controller against today's loop with the user's own settings, on the same unit.
- Then today's loop's code is removed, and its numbers with it (E18, the 20 ms of E10).

### Gate 7: the Pico W, a second powder, and the facility's proof

**Before it:** the checklist above, answered in `progress.md`.

- **On the unit it was not built on** (the Pico W at .168, the owner's own trickler with its own tubes), from nothing: calibrate, take the offer, throw, with no hand settings. **Pass:** gate 2's bar on that unit.
- **This is where a fixed number is shown to be one.** A fixed number that needs another value on this unit is not a rule: it becomes a number the calibration measures, and the calibration is mended. It is never tuned to the second unit.
- **The Pico W's memory and timing:** the recordings' rows, the millisecond's work in the job task, `soak`.
- **BM2,** when the owner loads it, its forecast written first: a calibration and 25 throws at each tolerance.
- **The proofs on both units** (sign-off item 14), and the safety cases on both (item 8).

### The close

- The design overview, its outline approved by the owner first (sign-off item 16).
- The register complete against the stored records and the settings' tables (item 2).
- The two items of "To settle before approval": ADR-012's check, and PR #113's end.
- The ADR closed (Built, in this README, its progress and the index) before its PR.

### Stored layout changes (sign-off item 15)

- **Made by gate 2,** each a temporary form until gate R:
  - the charge settings' filler holds `controller` and `mode` (no size change);
  - a calibration slot for each powder profile at 13 to 20 K (new slots; no existing record changed);
  - the PID law's carried stops at 21 K (a new slot).
- **Gate R's,** declared here with their sizes when they are made: the unit's record (a new slot); the powder's record grown; the 21 K slot freed; the filler's bytes read as nothing and written as zero.
- No existing record changes its size, so no unit's settings reset at the update.

## What we want

- One controller with no hardware in it, used by charge mode and by tuning.
- Every throw recorded, so "is it better" is answered from data.
- Test functions a user can run: a test throw, a motor at a speed, the scale's readings.
- Automatic tuning built on that controller.

## Owner's direction (2026-11-13)

- Implement better control tuning and a testing function.
- **Backward compatibility is on the table, but not first** (the owner, 2026-11-19): "backward compatible should not be considered during the initial research. Later when designing 'what to keep', then we will evaluate the cost to transit the system over, as we have 600+ users around the world now." Read here as: the research and the first design are not bent to fit today's profiles, settings or API; a later step, "what to keep", weighs for each change what it costs those users to move (their stored profiles and tuned gains, their habits at the screen and the web, their scales' set-up) and decides what stays, what is migrated and what is dropped.
- **The algorithm is developed in a simulation and verified by experiment, not developed on the device** (the owner, 2026-11-20, to the research's bench sessions): "your session, as well as previous session is to capture data so you can design the simulation. I don't want you to slip into the live debug or tuning session. The goal is to design the simulation with adjustable parameter so we don't need to develop the algorithm solely on the device, but based on the simulation and verify by the experiment." Read here as: a bench run measures a parameter of the simulation or checks one of its predictions; a powder, a tube and a scale are each a set of its parameters.
- **The next stage is the simulation, built to answer the design's questions without the units** (the owner, 2026-11-20, after the night's notes): "I don't want you to enter the tuning mode. Your job is to build simulation to answer my above questions without touching the physical units later on." The questions: what the simulation still needs (fresh AR2209, AR2208 and ADI BM2 are on offer, no ball powder; "build the simulation based on the real world measurement first, then use the kernel data shared online ... to 'predict' the kernel characteristics"); how it is calibrated to another setup (the Pico W's own build can take the same powder, for the variation from unit to unit); control beyond PID; other scales by their lag and period; speed against accuracy as modes the user picks; whether a tube must always turn one way.
- **Calibration in three parts** (the owner, the same message): the scale ("how fast the scale can respond to the change of weight, perhaps using the fixed weight"), the tube ("its forward and reverse characteristics"), the powder ("how the powder respond to the tube rotation"). "At the end of the day the calibration should be built to the OpenTrickler function, preferably without the assistance of external AI agents, but the AI agents can be tapped in to provide additional help." And: "the OpenTrickler is a community project the simulation should be designed flexible enough to be adopted by other users." Read here as: what a calibration measures on a unit is what the simulation runs on (`tools/charge_sim/records/`), so another user's unit is another record made the same way.
- **A stream of change is seen sooner than a single small change, and how much sooner differs by scale** (the owner, 2026-11-20, noted for later, not an immediate task): "Most lab grade scale takes time to settle down, and the measurement lag may not consistent. Therefore a consistent stream of measurement change is more responsive compare to discrete weight change. A&D don't have much of this problem but on G&G, especially JJ223BF (the one I tested), tiny weight change takes about 5 seconds or more to register. While on A&D the small weight change may have registered as unstable change already, but still get reported." Read here as: a scale has two responses, to a flow and to one small change from rest, and the design must not assume the second from the first. A finish that adds a kernel and waits for the scale to show it is the worst case on such a scale; the scale's calibration measures both. The bench's A&D, the same night: a flow is followed 0.6 s behind; one digit from rest first shows after a median of 1.95 s, one in ten after more than 4 s (`progress.md`).
- **The control may be a hybrid, and nothing is decided while data is being captured** (the owner, 2026-11-20, during the evening's capture): "I'm open to hybrid control, for example depending on the delta weight to target, the control mode may change depending on the characteristics of the powder and/or scale etc. I don't want to make decision on data capture phase. I'd like you to still focus on the data capture, and not drift to the tuning for tonight." Before it, the owner had asked whether speed against charge weight is the right relationship at all ("I had my controller turned between this relationship but I never doubt if this is correct"; steps against weight and step frequency against weight named as others), and how a step parameter would be tuned for each powder. And, the same evening: "I still encourage you to make assumption, and use the actual trickler to verify the assumption, to capture data for simulation and analysis purpose, not to build the final product. We are not there yet." Read here as: a capture states an assumption about the tube, the powder or the scale, says what the recording will show if it holds, and reports the verdict; the capture measures what each way of controlling would lean on (what a turn pours and how surely, at each speed and size of move; what the scale shows of a flow and of a small change); the choice among them, or a mix by how far the weight is from its target, is the design's, later.
- **The research may turn to the fine tube** (the owner, 2026-11-21, 03:10, as a direction not yet settled): "Perhaps we can pivot the research to fine trickler more, as the accuracy is purely determined by the fine trickler. While the coarse trickler can somehow be used to accelerate the trickling process, it is less accuracy and lesser important to model. Do you agree?" Settled the same night, the owner: "Yeah I agree with you. I was comparing the fine with coarse tube, and I agree the scale is the most important factor." The agent's answer he agreed to: yes, with two things kept. The accuracy is the fine tube's and the scale's together, not the fine tube's alone. And the coarse tube is kept as four measured numbers and not as kernels: its grains a turn, what arrives after its stop, how far a move of it is off (which sets how short of the target it must stop, and so the fine tube's seconds), and that nothing falls from it late, during the fine tube's finish, which is the one way it can spoil a charge.
- **Talk is not a decision, and a record of talk is not a reference** (the owner, 2026-11-21): on the night's questions and answers about the design, "that's the chattering between you and me, no final decision made without backing with evidence." And, closing the night: "I might continue with another agent so please record out chattering and leave only as the record, and not to be directly cited as the reference." Read here as: what the owner and an agent say to each other while thinking is kept, in a file that says so at its top (`research/talk_2026-11-21.md`), and nothing in it stands as the owner's direction, a finding or a decision. A finding is in `progress.md` with its recording; a decision is in this README with the evidence behind it.
- **The tolerance, and a looser one** (the owner, 2026-11-21, starting the design questions): "AR2209 throw is much harder than BM2 due to its kernel size. In most cases +/-0.02 gr is acceptable, and I'd accept +/- 0.04 gr for loads that doesn't need to be that accurate." And on the work: "don't over do, if you need any help, let me know, and also let me know if you lacks of any input." Read here as: a throw is good within 0.02 gr of its target (a digit of the A&D either way, which is the firmware's rule today: over or under from 0.03 gr), and a second, looser band of 0.04 gr is one a user may choose; every figure for a controller is given at both. A kernel of AR2209 is a whole digit and one of BM2 a little over half, so the same finish is judged harder on AR2209.
- **No scale is characterised by hand; the calibration finds each one's lag** (the owner, 2026-11-21, on being shown what a finish by small moves costs on the bench's A&D): "There are more than 7 scale to be supported, so I'm not planning to evaluate each of them to account for individual measurement lag. Therefore the later calibration routine must account for that, and your simulation today should assume the inconsistent measurement lag subject to the weight change speed." Read here as: the firmware carries no table of scales' delays. A unit measures, with its own tubes and powder, how late its scale shows a flow at each rate of flow and how late a single small change, and the controller takes those from the calibration. The simulation treats a scale's lag as depending on how fast the weight changes, and a controller is tried on scales it was not designed on.
- **Two proposals, for any build, powder and scale** (the owner, the same message): "Instead of letting me to review, I'd like you to make two proposals based on the knowledge and data to date, with confident ranking from high to med. Each proposal shall include the detailed procedure for both trickle control, as well as calibration routine. And remember, those proposals will be used on ANY opentrickler build/material/finish, and ANY powder, and ANY scale. therefore the proposal must be flexible and consider general use cases". They are in `research/proposals_2026-11-21.md`. Neither is approved.
- **The controller and its calibration go into the firmware next, tuned hands on, on the bench's rig** (the owner, 2026-11-21, 17:29, after the bench trial of the two proposals with AR2209): "BM2 is low risk, so the next won't run the BM2 bench test. Instead, I'd expect the next agent to start to implement the controller and calibration in the firmware, and perform basic tune, against the calibration on the same rig based on proposal today. The next agent will be hands on, based on your research and simulation today." So no BM2 trial comes first, and for this stage a tune on the unit is his word, where the research's bench sessions only captured data. `research/HANDOVER.md` has what the trial found and where the next agent starts. The plan made from it is in `history.md` ("What we do", 2026-11-21); the plan in force is the build plan above.
- **Four answers for the build** (the owner, 2026-11-21, just before 20:58, to the implementing agent's questions). Superseded in part by the sign-off: one controller with one finish, no choice of loop for a user, and no tune by hand on the unit:
  - **The finish:** glide and stop (proposal 2) is a unit's default; stop short and jog (proposal 1) is the other setting of the same two numbers.
  - **The PC twin:** a host C compiler is added, pinned in `toolchain.env`, so the simulation runs the firmware's own controller code on the PC and in CI.
  - **Today's loop:** replaced by the new controller, "and I will want to run a side-by-side comparison later". Read here as: the two PID loops go; a gate throws today's firmware and the new one on the same rig, powder and target, and what is kept for users is decided with its figures.
  - **Both loops, the new one the default** (the owner, the same evening, revising the answer above): "Perhaps in the mean time I may going to run two types of charge control in parallel, so perhaps for now, run the new algorithm in parallel with the legacy PID system ... just redirect the default charge controller to the new one (without needing to 'delete' the old code". Read here as: one setting chooses the charge's controller, the new one by default; today's PID loop stays as it is, neither deleted nor mended (it is the code this ADR replaces); gate 6's side by side is the two on one firmware, and what is kept for users is still decided there.
  - **Hands on:** the agent tunes the frame's constants on the unit (the margins' multipliers, the creep's share of the band, the rest and watch times), each change committed with its recorded throws and replayed in the simulation; a new mechanism goes to the owner first.
- **The legacy PID with the glide's learning** (the owner, 2026-11-22 evening, to the agent): "If the agent can pickup the existing legacy PID system, and integrate with the continuous learning algorithm adopted in glide algorithm, and achieve the same, or maybe better performance?" Answered by experiment the same night (`research/pid_learning_2026-11-22.md`).
- **Three initiatives** (the owner, 2026-11-22 night):
  - "Use legacy PID parameter that people already uses, so they don't need to re-tune everything."
  - "Build the autonomous predictor, auto PID tuner (calibration) and learning into the hybrid PID controller."
  - "Make the parameter (at least the initial parameter) portable, and let the learning to autonomous overwrite the user's default parameter."
- **Split by physics; the glide kept** (the owner, the same night, after the agent's answer):
  - "I agree that we should split by physics, and share the powder specific profile, and leave the scale, printed part related property to the OpenTrickler, and that part is not sharable so I can live with keeping the device specific parameter none configurable (i.e., not through the interface, but we should allow the user to backup and restore)."
  - "And I don't want to throwaway the glide controller. We can always run two controllers in parallel, especially the predictor, and other non PID related, like reverse control and other advanced feature."
  - The design drafted from it: `controller_design_draft.md` (Draft, not approved).
- **Bad gains** (the owner, the same night): "own carried stop, also, consider what if the PID value is bad to begin with."
- **The facility, not this unit** (the owner, the same night): "I want you to make sure you are building the facility, not sliding into the tuning against this particular unit. Any data captured today should be used for future calibration." The agent agreed, and named where it had slid (`history.md`, "Proposed for approval").
- **The controller's document** (the owner, the same night): a live document of how the charge controller works, "consumed by human and AI agents". One was written that night (`docs/charge_controller.md`) and **deleted at the owner's word the same night**. It described the branch's debug switches as a user's choices and one unit's calibrated numbers as the controller's, and so misled. Not to be revived from git as it was.
- **What ADR-009 must deliver** (the owner, 2026-11-22, about 22:00, his hard gates; the agent's comments and the owner's answers settled them into the sign-off list below). The final controller SHALL:
  1. "From user's perspective, one controller. You can run multiple controllers behind the sense in parallel, including the auto tuner and predictor, but NONE should be visible to the user."
  2. "New parameters or settings should be organized, approved explicitly by myself, with justification."
  3. "Auto learned data, and calibration data shall allow the export and import, but not exposed to the average user. Also, be VERY CAREFUL with what makes people thinking the parameter is portable and sharable. Advanced settings exclusive one particular opentrickler shall be nested under settings, not to be confused with the powder profile, and I don't want you to explicit write anything in the settings page or sorts 'this is not portable'. No, this is bad UI design."
  4. "Use common sense when delivering. DO NOT slip into the tuning session. Tuning must through calibration, no other routes during development."
  5. "Tuning shall allow the help from external AI agent for an average user."
- **The owner's answers to the agent's comments** (the same exchange):
  - "One slip of #5 per your understanding: You are forbidden to tune on my unit, but firmware should allow the tuning of calibration by user's AI agent in the future."
  - "User's existing PID data can live as is, and I'd expect two ways can be used to correct: the auto tuner and calibration. The autotuner should automatically correct the value over the coarse of the trickle, and the calibration should fix the PID, and other 'hidden' variables off one go." Read here, as the agent answered and the owner did not correct:
    - the auto-tuner corrects, throw by throw, the measured numbers each throw shows (the stop, the grains a turn, the lag);
    - the PID values are derived from those by the calibration's own rule, never learnt as free numbers.
  - "You should not need to touch the existing settings, and they are not legacy. Its function must also be preserved (with bug fixed of course)."
  - "You can record your hand tuned value, this is a good indication of what the auto tuner and calibration should achieve at the end of the calibration."
  - "Agent can override values with agreement from the user. Just like what you did today to help with calibration, but NEVER to be done during the development. The development should aim to improve the auto tuner and calibration so they will automatically derive the hand tuned values, without the help from AI. If not, then the calibration routine needed to be fixed. But again, the procedure should be made generic, not tuned towards MY unit."
- **The auto-tuner's switch, and the design overview** (the owner, the same exchange):
  - "I'd like to add the switch to the auto tuner so user can disable it on demand. And the auto tuner should be turned on."
  - "At the end of ADR-009, the new controller design overview should be drafted, with correct level of information disclosed, NEVER drift the document into a development note."
- PRs are not merged as they stand. Two of them are about tuning (#113, #101). Since 2026-11-15 nothing is kept of them ahead of this ADR's design ("Handed on by ADR-002").

## Why here

It needs ADR-003's server for its endpoints and ADR-008's page for its chart. Its first half, the controller as a module with its recordings, depends on neither and can move earlier if the owner wants tuning sooner.

## What we know (checked 2026-11-13, in `charge_mode.cpp` and `profile.h`)

**The charge**
- Four states: wait for zero, charge, wait for the cup's removal, wait for its return.
- Zero is ten readings 300 ms apart whose spread and mean are both under 0.02 (the default margins).
- The loop runs once per scale reading. If none comes in 200 ms it polls the buttons and waits again.
- Two PID loops, on the same weight:
  - coarse aims at the target less the coarse stop threshold (5 gr by default) and is switched off for good the first time its error reaches 0. The servo gate then moves to a set position, and the coarse trickler is run backwards for a quarter turn at 1 rev/s (the defaults; PR #108) so powder does not trail. That back-off has three stored settings: on or off, the turns, the speed;
  - fine aims at the target and runs from the start.
- Each output is held between a minimum and a maximum speed from the profile and the motor.
- Everything stops when the weight is within the fine stop threshold (0.03 gr by default) of the target.
- One second later the weight is read once more and the LEDs show under, over or good. There is no top-up when it is under.
- A profile holds Kp, Ki, Kd and the speed limits for each trickler. Eight profiles. The two stop thresholds are global.

**Known faults**
- The D term divides by the milliseconds since the last reading. That can be 0, and at the first reading the last error is 0, so the first D term is a spike. PR #113 reports both tricklers briefly at full speed at the start of a throw and fixes the division. **Fixed by ADR-002** (2026-11-15) with #113's fix: no D term on a throw's first reading, nor when no time has passed.
- The I term adds the error once per reading, not scaled by time, so its weight changes with the scale's reading rate. It is not limited while the output sits at its maximum.
- The loop's pace is the scale's. A scale that reports half as often is controlled half as often, with the same gains.
- Nothing records a throw. The only trace is the elapsed time on the screen.
- The loop is inside `charge_mode_wait_for_complete()` with the button polls, the LED colours, the servo gate and the title on the screen.
- Setting a motor's speed can hold the loop: the call waits, with no time limit, while that motor's task is in a ramp. ADR-012 fixes it, with the motor module's other faults.

**Scales that are slow, or late** (the owner, 2026-11-15, when ADR-013 was drafted: "The slow response scale tuning should be a part of ADR-009")
- **Slow by nature.** Scales report at different rates. The A&D streams; the G&G driver asks for each reading and waits 250 ms between asks (`gng_scale.c`), about 4 a second at most. With the loop's pace the scale's, a G&G is controlled at under half an A&D's rate with the same gains, and each stop can land up to one of its intervals late.
- **On time but late.** A scale that averages reports at a steady rate a value that trails the pan: "Some scale may still report at the constant speed, but unable to report its up-to-date measurement." The owner's G&G manual (`manuals/configure_g&g_scales.md`) sets the JJ223BF's averaging window (C2) to 1, where its default 4 is the longest, and its sensitivity (C1) to 1. Timing cannot show such a lag; only the controller, knowing the scale, can allow for it, for instance by stopping early by the weight still to come.
- **Missing readings** are ADR-013's floor: no motion on a reading that is not fresh by the scale's own rhythm, a pause after 3 usual intervals, a stop after 10. "The action MAY needed to be bounded to the controller, where ADR-009 will address": the pause sits in the controller's step, and this ADR decides what the controller does on a missing, slow or late reading beyond it.

**What others have built**
- PR #113: tuning from start values by powder shape and grain size, ten proving throws, a suggested fine threshold from the weight that still falls after the stop, a test throw with every sample recorded and exported, and a simulated powder with tests that run on a PC.
- PR #101: another tuning routine, ported from a fork.
- On main since July: a simulated scale (PR #107). Its weight follows the motors' speeds but is held at the target once within 0.1 gr, so it shows that a charge runs and cannot say how well it was controlled.
- Open requests: a calibration routine (#12), PID values from an online source (#10), ladder loading (#106), a press to skip the current charge (#75). Issue #105 (reverse the coarse trickler) is still open, though PR #108 built it.

**Handed on by ADR-012** (2026-11-15). Found by the audit of 2026-11-14, which read every module outside the motors for the kind of faults ADR-012 lists for them. Not read a second time, but for the weight buffers. Nothing here was tried on a unit unless it says so.
- **The modes' cleanup is this ADR's.** `charge_mode.cpp` is the one file every open PR changes, and ADR-012 touched it only where a motor driver's fault must refuse a charge or end it: the check first thing in `charge_mode_menu`, the fault looked at in each of the charge's states, and a flag that keeps the fault's name alone on the title line. PRs #109 and #113 merge into ADR-012's branch without a conflict (tested 2026-11-15).
- **The owner's real throws come here** (the owner, 2026-11-15: "I won't run the real throw until you have reached the tuning stage"). ADR-012 changed what the motors get:
  - the stored current is the real RMS current, given in a quarter to half of the driver's 32 steps; units' figures were converted once, 800 mA to 380 on the bench's Pico W, 500 to 240 on the Pico 2 W;
  - the step pulses come from the PIO, fed a millisecond at a time, and a speed ramps by the acceleration every millisecond.
  
  So this ADR's first recorded throws also answer ADR-012's check: same powder and target, against the firmware before ADR-012 (main at f4e7d1b, ADR-013's merge), no worse. The older firmware reads a converted figure as a trimmer-scaled one (380 gives about 0.18 A), so for its throws the current is set back to the old figure by hand, and set again after coming forward (ADR-012, "The stored settings"). If the throws are worse, the mA is the first figure to raise.
- **`FloatRingBuffer`** (read 2026-11-15; its faults are latent, no caller meets them today):
  - its sum, mean and deviation run over every slot, not the readings it holds, so they read memory never set until the buffer is full;
  - `last()` gives the slot to be written next, not the newest reading;
  - its lock is a plain bool and locks nothing; its `malloc` is not checked;
  - it is C++ and has no median, so the scale's rhythm in `scale.c` keeps a window of its own. One ring buffer in C, its sums over what it holds, with a median, and the rhythm moved onto it.
- **The coarse back-off is a speed held for a time,** ended only when a reading arrives: its length is rounded up to the scale's period and runs on if the scale stops. Since ADR-012 the unit counts its steps on the STEP pin (`motor`, `motor_get_step_stats`): a back-off can be a count of steps.
- Over and under are judged on one sample, and a reading that is not a number falls through to "normal": a green LED on no reading.
- A lifted cup is a mean below -9.98, fixed: a cup lighter than 10 units is never seen to go.
- Every scale reader looks at its UART once in 20 ms (the G&G's in 250 ms): a reading is up to that late.
- "The gate is in use" is read from two different fields by cleanup mode and by charge mode: the menu's "Disable" does not stop charge mode from driving it.
- Charge mode writes weights into 8 bytes with no length, as the cleanup screen did. `weight_to_string` (ADR-012) writes with one.

**Handed on by ADR-002** (2026-11-15). What is said of PR #113 is from its file list, its includes and its charge loop; of PR #101, from its header. Neither was read whole: this ADR reads them, against its own design.
- **The design comes first, without the PRs** (the owner: "I'd keep your judgement independently from the influence of PRs"). The controller, its recordings and the tuning are designed here as if no PR existed. Then #113 ends one of three ways: this ADR's branch starts from its head and it is merged with the ADR; or parts are taken in commits that carry a `Co-authored-by:` line and it is closed with a note that names them; or it is closed with thanks.
- **This ADR is not brought forward for #113** (the owner, 2026-11-15: the order of the ADRs stays).
- **PR #113 is open,** with a comment that says it is decided here. Its author is a first contributor, whom the owner wants encouraged.
- **Its candidates,** the three parts with no hardware in them (they include only the C library and each other):
  - the tuner: `src/powder_analysis_tuner.cpp` (750 lines) and `.h` (263), `src/powder_analysis_config.h` (207). It hands out the next trial's settings and takes the trial's result;
  - its tests on a PC: `tests/powder_analysis/test_powder_analysis_tuner.cpp` (1,318 lines), with a Makefile that calls the PC's own `c++`. CI has no compiler for a PC that `toolchain.env` names (ADR-011); "the PC tests in CI" needs one;
  - a simulated powder: `tests/powder_analysis/simulated_powder.cpp` and `.h` (168 lines). "Tests on the PC" above already names it as one of two starts.
- **What its author says is missing:** the thresholds are global and not per profile, fine Kd is not tuned, only stick powders were measured, only an A&D scale was tried.
- **Not taken, whatever this ADR decides:**
  - its charge loop, a copy of charge mode's inside `src/powder_analysis.cpp` (2,326 lines). The copy has neither ADR-013's pause on a missing reading (it waits again with the motors at their last speed, until its own 30 s limit for a trial) nor ADR-012's end on a driver's fault. One loop moves the tricklers: this ADR's controller;
  - its u8g2 screens (ADR-010 builds screens), its page on today's server (ADR-008, ADR-003), and the tools that draw those screens on a PC.
- **Read it with care.** Its second commit is "Remove concept files created by the ai", and the owner takes much of it to be an agent's work. Whether its tests test what the tuner should do, or only what it does, is for the reading.
- **The D-term fix is in** (ADR-002, step 1; commit adc5f56, credited to #113's author). On the Pico 2 W with the A&D, the first reading's D term gave the coarse trickler 0.79 to 1.88 rev/s where its P term asks for 0.625, in 20 throws of 20; after the fix, 0.625 in all 20. The division by 0 was not seen in 42 throws: the first reading came 5 to 36 ms into the throw, never at 0.
- **The simulated scale at 10 readings a second is in step with the wait for zero:** its first reading comes 74 ms into every throw. So `charge_cycle` showed that fault least, and no proof before ADR-002's showed it. A simulated powder for this ADR's tests on a PC should not be in step with the loop.
- **PR #101 is closed.** Its tuning, as its header gives it (`src/ai_tuning.h` on the PR's branch):
  - two phases, the coarse trickler alone and then the fine one, each throw judged by its overthrow and its times;
  - a search that halves its step, for Kp and then for Kd, inside set limits;
  - the last 50 ordinary charges kept in flash, and new gains suggested from them with no tuning run;
  - a learned allowance, in grains, for a scale that reports late. That is "stopping early by the amount known to still fall" above, and the scale that is "on time but late".
- **Both PRs cut at the same place:** "give me the next trial's settings" and "here is the trial's result". Two authors who did not share code arrived at that seam between a tuner and what runs a throw.

**Handed on by ADR-003** (2026-11-15, its API review; read in `charge_mode.cpp`, not tried on a unit)
- **The cup's return is `weight >= 0` exactly.** A cup that comes back a hair lighter than it was zeroed with is never seen, and the charge waits for good. ADR-003's API refuses a zero while the unit waits for the cup, since there it would start a throw on whatever is on the pan; a tolerant return test here puts a drifted cup into `zeroing` by itself, where a zero that puts right a drift is taken.
- **The lifted cup is a mean below a fixed -10 in the scale's unit** (the cup's removal), so a cup under 10 g is never seen to go. ADR-003's API bounds a remote zero by the weight's distance from zero instead, for that reason.
- **The API reads the throw** (ADR-003's api_v1.md): its number, `in_throw` from `zeroing` to the verdict, the verdict judged a second after the cup is due off against the target as it then is, and the end reasons. A controller module that moves these keeps them, or the API's growth rules say how they change.

**Handed on by ADR-003's gate 2** (2026-11-15, measured on the Pico W at 1.20.179 with `rc_run.py soak --load` and `/api/v1/debug`'s `stream_make_ms`; the owner: "Close gate 2")
- **What a recording costs the unit to send.** A reply the size of a throw's samples, 600 rows of 16 numbers in JSON (82 KB), takes 3.1 s on an idle unit and 8 s during a charge. Pulled back to back, it slows the small web reads beside it to about a second (0.11 s without). The charge is untouched: no late step feed, the scale's 100 ms, every charge's time and weight as without it.
- **Half of that, during a charge, is formatting the numbers:** `%g` in the Pico SDK's own printf (`pico_printf`, wrapped over `snprintf`; newlib's is not in the image), 117 µs a number on an idle unit, in the server's task, which runs below the charge's tasks. The other half is sending, two segments in flight (`TCP_SND_BUF`, held there by lwIP's heap), the next chunk made only once the last is with lwIP.
- **Since gate 3,** numbers are written by our own code, not the SDK's printf, which could not write a float back as itself: 60 µs a number on an idle unit, and the throw-sized reply 2.6 s (measured 2026-11-15; during a charge not measured again).
- **So a recording's format is this ADR's lever.** `?format=` is reserved in api_v1.md: a binary format makes no text at all, and a number formatter of our own makes it faster. A second chunk buffer a connection would let the next chunk be made while the last is sent. The protocol needs no change for any of them. Measure the choice with `soak --load` and the recording's real size.

**Handed on by ADR-005** (2026-11-17, closed as a scoping ADR): the firmware is built at `-O0`, and ADR-005's optimised build changes timing. If that build comes after this ADR's tuning, the tuning's throws are shown again on it.

## Ideas

**The controller as a module.** Readings and times in; two speeds and the gate's position out. No FreeRTOS, no motors, no screen in it. Charge mode keeps the states, the buttons and the LEDs, and calls it.

**Tests on the PC.** The module against a simulated powder, in CI. Main's simulated scale and PR #113's powder model are two starts for it; one is kept. Main's would first have to stop holding the weight at the target, or it cannot show an overshoot.

**Every throw recorded.** Time, weight, both speeds and the three terms of each loop, per reading, in RAM for the last throw; downloadable, and drawn on the tuning page. How many throws are kept, and where, depends on the board (ADR-005).

**The faults fixed one at a time,** each against recordings of the same powder before and after: the I term scaled by time and limited, gains that do not depend on the scale's reading rate.

**Ideas to try, not promised:**
- a top-up when the throw ends under the target;
- the stop thresholds per profile;
- stopping early by the amount known to still fall.

**Test functions,** on the screen and the web: a test throw that is recorded, a motor at a set speed for a set time, the scale's readings and their rate.

**Automatic tuning,** designed here on its own, run through the same controller and judged by the same recordings. PR #113's tuner is then judged against that design ("Handed on by ADR-002").

## A gate: the servo gate tuned by eye (the owner, 2026-11-19)

The owner, during ADR-008's phone checks: "I want you to consider a servo gate tuning feature, perhaps offer that in initial commissioning as well as in the settings menu." Weighed the same day against ADR-008, the owner chose "A gate in ADR-009": ADR-008 was at its last checks, and the bench's unit has no gate fitted to prove it on. Here because this ADR holds how the charge uses the gate (it moves at the coarse stop, above), and tuning is the set-up half of the same part.

**What we want.** A person with a gate fitted sets it up by watching it, not by typing numbers: each shutter's closed and open ends found by moving the shutter a little at a time until it is right, then kept; the speeds tried by opening and closing it. Offered in the set-up when the unit has a gate (ADR-008's commissioning), and in Settings › Servo gate, on the screen and on the web alike (screen and web parity, ADR-010).

**What we know** (checked 2026-11-19 in `servo_gate.c`, `menu_nav.c` and the portal):
- The gate's settings are six numbers and a switch: Fitted; each shutter's closed and open ends as "a share of the PWM's period" (0.02 to 0.13, a servo's pulse at 50 Hz), which a person can only find by trying; the closing and opening speeds in full travels a second. Stored in its own EEPROM slot.
- The screen's Servo gate page already moves the gate to the end whose number is being stepped (`gate_show_end`), so its end is seen as it is set; the web's form does not: a number typed there applies at the gate's next move.
- The web's Servo gate part moves the gate now (Open, Close, how open), from the unit's own reading of it (ADR-008, gate 3). Both shutters move together, each between its own two ends.
- The set-up's steps are a list on the unit (`setup.c`: code, wifi, scale, motors, ready, done), drawn by both screens, so a step after Motors can be added without changing the others.
- The bench's unit has no gate fitted (AGENTS.md); a bare servo on its connector shows the pulse's ends turning, though not a shutter closing.

**Proposed, not designed.** In the build plan its mocks are taken with gate 4 and its build with gate 5. A design gate first, with mocks of both screens for the owner to settle (the skills `embedded-ui-design` and `web-ui-design`), then the build:
- a tuning flow per shutter: its end shown live while it is nudged, "Set closed" and "Set open", the two speeds tried, kept with one store at the end and nothing stored if left;
- the set-up's step "Servo gate", offered only when the build carries the gate and Fitted is on, and skipped with one tap;
- whether the ends stay numbers anywhere a person sees them, or only as the tuning's result.

**How the unit shows it works:** a gate fitted on the bench (or, before one is, a servo on its connector), tuned from the set-up and again from Settings on the screen and on the web; both shutters closing and opening at their ends; a charge with the gate, `charge_cycle` passing; the stored ends surviving a restart.

## Sign-off: what ADR-009 delivers

**Final** (the owner, 2026-11-22, 22:17 NZDT: "Final"). Settled with the owner 2026-11-22, about 22:00 (his hard gates and answers, under "Owner's direction"). It changes only on the owner's word. This list replaces "How the unit shows it works" as it was approved on 2026-11-21: its items are folded in below. **ADR-009 closes when every item is shown,** each by the proof named with it, on the units named. An agent who builds toward it reads this list first. A piece of work that serves none of it is out of scope; raise it with the owner, do not build it.

**The final controller SHALL:**

1. **Be one controller to the user.**
   - The screen, the web and the API show one controller. Its parts (the predictor, the auto-tuner, the calibration's rules, any speed law) run inside it, none of them visible or chosen.
   - The debug switches of the build go before the close: `settings/charge controller`, `settings/charge mode`, `controller=pid_learnt`, `ccon speed_law` and the like. What the bench still needs stays on the console only, never in the API or the screens.
   - *Shown by:* the API's index and schema, `menu_tour` and the web's pages carrying no such choice; the API frozen with it at the first user release.
2. **Change no parameter or setting without the owner's approval.** Every one added, kept with a new meaning, moved or removed is an entry in the parameter register (below), with its justification and the owner's approval and date.
   - *Shown by:* the register complete against the stored records and the settings tables, every entry approved.
3. **Keep the existing settings and their function** (the owner: "they are not legacy"), with their bugs fixed.
   - Each one's meaning in the one controller is a register entry: the fine stop threshold as the user's tolerance; the coarse stop threshold, the back-off, the precharge, the gate at the coarse stop, the decimal places.
   - A bug fix that changes what a user's setting does (the D term taken per millisecond, for one) is an entry of its own.
   - *Shown by:* each existing setting's function shown on the unit; `settings_roundtrip` and `api_check` pass.
4. **Keep the user's PID values where they are, and correct them two ways:**
   - **the calibration,** one routine on the user's word, which measures the unit and derives every number in one go (the PID values, the stop, the hidden ones), stored on the user's yes;
   - **the auto-tuner,** which corrects throw by throw the measured numbers the throws show (the stop, the grains a turn, the lag). The PID values are derived from those by the calibration's own rule, never learnt as free numbers.
   - A powder never calibrated runs from the user's PID values, guarded by the controller's frame (the forecast, the speed envelope, the settled verdict). They are its starting point, not a promise of its results: no bar is set for a charge before a calibration (the owner, 2026-11-23; item 10).
   - **The auto-tuner has a switch the user can turn off on demand, and it is on by default** (the owner). It is a setting of the register (its first row). Where it sits, once for the unit or for each powder, is decided at gate D, and so is what "off" keeps.
   - *Shown by:*
     - a powder with no calibration throwing on the unit;
     - a calibration changing its PID values on yes;
     - the auto-tuner's corrections bounded and recorded;
     - with the switch off, a session that corrects nothing and carries nothing;
     - a new or reset unit with the switch on.
5. **Keep a unit's own data apart from the powder profile.**
   - Calibration and learnt data that belong to one OpenTrickler are nested under Settings (an advanced part), never in the powder profile, with no words telling the user what is or is not portable: the structure keeps them apart.
   - They export and import through that advanced part and the backup, not exposed to the average user. The backup holds everything, so an update or a factory reset loses nothing. An import never applies another unit's own data to this one.
   - *Shown by:* backup, factory reset and restore giving every calibration back; another unit's backup imported leaving this unit's own data as it was; the screens' and web's mocks approved by the owner.
6. **Derive every number it uses from a rule of the controller or from this unit's calibration and learning,** never from one unit's throws.
   - During development nobody tunes the bench unit's numbers except by running the calibration itself. A value found by hand is recorded in the yardstick (below) as what the calibration and auto-tuner must reach by themselves. If they cannot, the calibration is mended, generically.
   - *Shown by:* an inventory of every numeric constant in the controller, each a rule with its reason or read from the calibration; the yardstick reached by the calibration with no hand settings; and **the proof on a unit it was not developed on**: the Pico W at .168, the owner's own trickler with its own tubes, calibrated from nothing, throwing to the results bar with no hand settings.
7. **Let a user's AI agent help tune that unit,** through the public API, on the user's agreement:
   - read the unit's calibration, learnt data, throw recordings, verdicts and events;
   - run the calibration;
   - change the calibration and learnt data directly. Each write is checked against its schema and bounds, backed up first, logged as the agent's, revertable, and stored on the user's yes.
   - The safety rules (item 8) are never within its reach.
   - Never done during development (item 6).
   - *Shown by:* an `api_check` covering these actions; a write outside its bounds refused; a write reverted; the event log naming the agent.
8. **Be safe:**
   - no turning without a fresh reading;
   - every fault stops both tubes before anything else: a scale lost or unsteady, a driver fault, a limit passed, **a cup lifted mid-throw** (the owner's sign-off condition of 2026-11-22), **no powder flowing** (an empty hopper, a jammed tube, a gate that did not open);
   - a throw past its limits ends short, never over.
   - *Shown by:* `robust_staged.py` with every case, `cup_lifted` and `no_powder` among them, on the Pico 2 W and the Pico W; `pulses`.
9. **Lose no feature a user has:** the servo gate (its waits by its own travel, issue #135), the precharge (counted in the forecast), the back-off (its spill counted), the LEDs and the verdict, series of charges, the API's throw and its events.
   - *Shown by:* each on the unit; the gate's on the day one is fitted, with a servo on its connector before.
10. **Migrate without losing anything, and say what changed** (reworded at the owner's word, 2026-11-23: the profiles are kept, not trusted):
    - an update from 2.0.3 keeps every profile and setting; the user's PID values stay as the starting point;
    - the user is told that the tuning made for the old controller will likely no longer hold, and is prompted to calibrate;
    - a charge before a calibration is allowed, with no bar set for its results;
    - the calibration is offered, never forced.
    - *Shown by:* the update on both units from 2.0.3, the settings compared before and after (`rc_run.py settings --compare`); the notice and the prompt seen on the screen and the web (their mocks approved at gates 4 and 5); a charge thrown before a calibration ending safely.
11. **Meet the results bar,** at the user's tolerance, on two units and two powders:
    - no worse than today's loop with the user's own settings on the same unit;
    - within the owner's over budget of one in fifteen where the powder allows it;
    - where a powder cannot (clumps), the calibration's verdict tells the user so, in words.
    - *Shown by:* ABBA series against today's loop, written first; the gate-2 figures of the Pico 2 W with AR2209 as the floor already met.
12. **Have its calibration as a job on the unit:**
    - the cup step kept;
    - bounded in powder and time, stoppable;
    - its verdict in words;
    - stored on the user's yes, run again whenever wanted;
    - measuring what the controller needs, generically: each tube's lag at every flow down to the creep, the grains a turn at each speed, the stop's spread against the approach time, the reverse's spill.
    - *Shown by:* gate 3's pass, and the yardstick reached.
13. **Learn within bounds, stably.**
    - A session resting on its aim leaves the next one's stop where it was (the carry's fault of 2026-11-22).
    - No session moves a stored number past its bound.
    - The simulated scale teaches nothing.
    - *Shown by:* simulation cases and a test on the unit for each.
14. **Pass the proofs,** on the Pico 2 W and the Pico W: `menu_tour`, `reach`, `api_check`, `settings_roundtrip`, `charge_cycle`, `pulses`, the controller's throw-for-throw check against its reference in CI, and memory and timing on the Pico W.
15. **Declare every stored layout change in this ADR,** each with its migration, as AGENTS.md asks.
16. **Have its design overview drafted at the end of ADR-009,** with the right level of information, and never a development note (the owner).
    - **Its reader:** a person or an agent who needs to know how the controller works and why, without reading the code. A contributor, an advanced user, or a user's AI agent helping with a unit.
    - **What it holds:**
      - what the controller does for the user, and what the user gives it;
      - its parts by their role (the predictor, the calibration, the auto-tuner, the safety rules) and how they work together through a throw and a session;
      - the rules it keeps and why, in words and symbols;
      - what a user and an agent can see and change, and where;
      - what the controller will never do.
    - **What it never holds:**
      - development history, experiments, results, figures or numbers of any one unit;
      - debug switches, console commands, internal constants' values;
      - the state of gates or of work in progress.
    - **The first attempt, deleted:** a "live document" of 2026-11-22 night held all of these and misled. Its lesson stands here: the overview describes the design, not a branch.
    - **Its outline is approved by the owner before it is written.** It changes only when the design changes, on the owner's word.
    - *Shown by:* the owner's approval of the outline, then of the overview.

### The yardstick: values found by hand on the bench unit

Recorded at the owner's word ("this is a good indication of what the auto tuner and calibration should achieve"). They are the Pico 2 W's, with the A&D FX-300i and the air-exposed AR2209 (`tools/charge_sim/recordings/`, `progress.md`).
- **They are a target, never a constant:** the calibration on the unit, and the auto-tuner after it, must reach them by themselves (item 6). They do not port to any other unit.

| What was set by hand | The calibration's value | Set to | What it gave | Where |
|---|---|---|---|---|
| Every lag (scale and powder) | as the PC measured it through the console | 0.055 s earlier | the coarse tube on time; the median 18.0 to 13.7 s | progress.md, 2026-11-22, series C (`--lag-shift -0.055`) |
| The coarse tube's grains a turn | 9.60 | 10.17, from the throws | with the fine's below: 80% within 0.02 gr, 11.7 s (series D) | progress.md, 12:12 |
| The fine tube's grains a turn at the finish | 0.797 | 0.715, from the throws (later throws 0.63 to 0.71) | the same | the same |
| The fine tube's top | 0.5 rev/s (the default) | 1.0 rev/s (`ccon fine_top_rps=1.0`) | every glide series since 11-21; fast at 10 gr 90% within 0.04, 8.2 s | progress.md, 14:57 |
| The fine figure's learning | on | off (`learn_fine_prior=0`) | it read through the fine tube's unmeasured lag at 1 rev/s | progress.md, 16:23 to 16:37 |
| The stop | placed by the PC's rehearsals | placed from series A's own throws (0.034 gr) | series B | progress.md, series B |
| The owner's own PID values on his trickler (.168) | none | fine kp 0.8, kd 15, 0.08 to 2 rev/s; coarse kp 0.05, kd 0.3, 0.1 to 8 rev/s; coarse stop 5 gr | today's loop on his rig, about one over in 10 to 15; on the bench unit, his fine values in the controller's frame with the coarse tube's from the calibration, 17 of 20 within 0.04 gr, 10.5 s | `recordings/settings_picow_168_20261122`, `research/pid_learning_2026-11-22.md` |

### The parameter register

Every parameter or setting the work adds, keeps with a new meaning, moves or removes. **Nothing in it is approved until the owner's approval and date are in its row** (item 2). Drafted 2026-11-23 from `numbers.md`, whose rows hold each number's default, origin and evidence; the last column but one names them. A setting `numbers.md` marks "unchanged" has no row here.

**★** marks a row that changes what a setting a user has does today.

| # | Parameter or setting | What ADR-009 does with it: its life cycle | Stored; the powder's or this unit's | Why | `numbers.md` | Approved |
|---|---|---|---|---|---|---|
| 1 | **The auto-tuner's switch** (new) | The user turns it off on demand; an agent on the user's yes. On by default. **Off freezes:** nothing more is learnt, carried or written to the profile, and the numbers in use are kept. Going back to the calibration's own numbers is a separate action a powder, "forget what was learnt" | A setting of the unit, under Settings (advanced); this unit's | The owner: "so user can disable it on demand" | E12 | The switch, on by default, one for the unit: the owner, 2026-11-22. What off keeps: waiting |
| 2 | **What the auto-tuner corrects** | The stop: learnt within a charge, carried between charges. The grains a turn: once gate 3 passes the fine figure's learning. From those, by the calibration's own rule, the profile's kp and creep, written at a charge's end. **The lag has no learner of its own:** its error at the stop is what the stop's learning corrects, and its value at speed is the calibration's to measure. A lag learner would be a new mechanism | The powder's record on the unit; the profile's kp and fine min speed | Sign-off item 4 names "the stop, the grains a turn, the lag". Nothing built corrects a lag throw by throw, and ADR-009 designs no new algorithm. This row is how the agent reads the item, for the owner to confirm | C10 to C15, E12, B3, B8 | Waiting |
| 3 | ★ **Fine stop threshold** (kept) | The user's tolerance in the one controller, for the stop and the verdict: band = threshold − half a digit. The debug `mode` and its two bands go | As today; this unit's | One tolerance, set once, by the setting a user already has | A7, A18, E14, D20 | Waiting |
| 4 | ★ **Coarse stop threshold** (kept; the one existing setting the calibration does not write) | The coarse stop of a powder with no calibration. A calibrated powder stops by the coarse room made from its own measurements, shown beside the setting. **Why the calibration does not write it,** as it writes the other settings: it is one number for the unit and the measured room is one a powder, so written from one powder it is wrong for the next, and too little room is an over. **The other way:** the calibration writes it, which is right for a user with one powder and wrong after a change of powder until that powder is calibrated too | As today; this unit's. The room is made from the powder's record | The setting is one for the unit, the measured room one a powder. Handed on to ADR-014, which settles where the thresholds belong | A6, D9 | Waiting |
| 5 | ★ **The profile's speeds; the motors' max and least speeds** (kept) | The limits of each tube in every law. The calibration measures within them, and no tube runs faster than the fastest speed measured. The fine min speed is the creep: the calibration writes it on yes, the auto-tuner while on, never under the motor's least. `fine_top_rps`, `coarse_top_rps` and `coarse_floor_rps` go | As today; the profile's, the motors' | Today the new loops ignore them, and three hand-set constants stand in their place, one of them a limit of the simulation | A2, A4, A5, A13, A14, C3 to C5, F7 | Waiting |
| 6 | ★ **The profile's kp, each tube** (kept) | The PID law's gain. The calibration writes it on the user's yes, and sets ki and kd to 0 with it. The auto-tuner, while on, writes the kp it derives at a charge's end | As today; the powder profile | Sign-off item 4. The offer's fine factor is made from each unit's measured trade (gate 3), never a constant | A1, A3, D19 | Waiting |
| 7 | **The profile's ki and kd** (kept, not mended) | As today's loop takes them: the I term adds each reading's error, the D term is per millisecond. The frame adds one thing: the I term stops counting while its tube is held at a limit. **Not mended in ADR-009:** a mend changes what every stored Ki and Kd does, and the calibration's own gains use neither | As today | Sign-off item 3 asks for an entry where a fix changes what a setting does. Here the recommendation is no fix: the users' numbers keep their meaning | A1, A3 | Waiting |
| 8 | ★ **The back-off, the precharge, the gate at the coarse stop** (kept) | Made by the frame. The back-off's spill and the precharge's powder are counted in the forecast. The precharge stays a time at a speed. **Issue #135, with no option:** every gate move is waited for by its own travel, and 500 ms more after a close; the tubes start when the gate is open | As today; this unit's | Sign-off item 9. Today the new loops make none of them. The 500 ms is today's figure and the issue's, a rule until a gate is fitted and it is measured | A10 to A12, A17, E10, E11 | Waiting |
| 9 | **The new parameters: what a calibration measures** (new; closes "Open, to be discussed") | **These and the switch of row 1 are all the new parameters ADR-009 asks for.** No existing setting can hold them: a scale's lag, what a tube pours a turn, how widely a stop lands. **The unit's record:** the scale's digit, lag and rest. **A record for each powder on this unit** (the calibration slots, grown): each tube's grains a turn; its stops at each speed (powder unseen, spread, quiet time, lag); the small moves; the stop learnt and carried. **Each one's life:** a default that is a general assumption or none at all (the controller then runs from the user's existing settings); measured by the calibration; corrected by the auto-tuner within its bound; then changeable by the user under Settings (advanced) and by the user's agent on the user's yes, each write checked against its bounds. **The powder profile** stays as it is today, shared as people share it today. Nothing else is stored: no extension of the profile, no figures against a standard tube, no lot | The unit's two kinds of record under Settings (advanced), in the backup; another unit's never applied | Sign-off items 4, 5 and 12. With the tolerance the user's and no law to choose, nothing is left for the draft's extension to hold. A profile shared is a starting point; the auto-tuner takes it from there | B1 to B12, G1, G4, G7 | Waiting |
| 10 | **The method's fixed numbers** (not parameters) | The shares and counts by which the calibration and the frame turn a unit's measurements into its numbers: 3 spreads, 0.6 of a lag, 15% of an error, a digit, two reading intervals. Each is in the unit's own terms, has a general reason in its row, and is held on a unit it was not developed on: one that needs another value there becomes a measurement. Changed only by an ADR. Removed as doing nothing: the 0.3 s least lag, and the glide times' four limits. Tried at nothing in N.3, as the bench's own: the 0.4 s of stillness, the rest's 0.3 s, the coarse room's 0.2 gr | Not stored | They are how a measurement becomes a number, not a number of any unit. Making each a setting would add some thirty parameters and let a unit contradict its own measurements. The sensitivity study supports them, on stand-ins made from the bench's records; gate 7 is their proof | C7 to C10, C12, C16, D1 to D4, D6 to D10, D12, D14 to D16, D20, D22, D23, E6, E19, E20, F1 to F6 | That they stay fixed and are not made parameters: the owner, 2026-11-23, 17:52 |
| 11 | **The safety bounds** (fixed, out of reach) | Never reachable by the calibration, the learning or an agent. None is a weight in grains or a value of the bench's scale: the far-over stop is 100 times the user's tolerance (3 at the default) | Not stored | Sign-off item 8. A gram scale reads 3 as 46 gr, and a digit is 0.02 gr only on an A&D | C11, C14, C17 to C20, E1 to E5, E11, E12, E17, E24, G9 | Waiting |
| 12 | ★ **Two checks at a charge's start** (new) | A charge is refused, in words, for a tolerance under a digit, and for a profile whose top speed is 0 for a tube the throw needs | None | Today such a profile creeps at the least speed, and a tolerance of 0 is taken. Fail early | A20, A21 | Waiting |
| 13 | ★ **The cup's tests** (mended) | A cup lifted mid-throw stops both tubes. The cup's return is judged within the zero's mean margin, not at exactly zero. Its removal loses the fixed 10 | None | Sign-off item 8; the faults ADR-003 and ADR-012 handed on | E7, E8, E19, E20 | Waiting |
| 14 | ★ **The verdict's moment** (changed) | Given at the frame's settled reading, not a second later. api_v1.md's "a second after" changes with it | None | The second is today's loop's way to a settled reading; the frame already has one | E9 | Waiting |
| 15 | **The fine figure's learning** (off until gate 3) | Off in the build. Gate 3's study on its own measurements says whether it is part of the auto-tuner, and with one figure or two | The powder's record | It read through a lag nothing had measured | C13, C15, D13, D17 | Waiting |
| 16 | **Before any calibration: the new parameters' defaults** | General assumptions, none of them the bench's. The digit: the smallest step the unit's own readings show. The figure: what the throw itself shows, the weight risen for the turns made. The scale's lag and rest: assumed on the slow side, so an unknown scale errs short, their values found on every stand-in scale (F.4). **The cost:** a user who never calibrates a quick scale throws slower than that scale could. A unit calibrated for any powder uses its own scale's numbers for the next | The unit's record, once measured | No bar is set for a charge before a calibration (item 10), and an over is worse than a second. The A&D's 0.9 s and 2.5 s and the 2.0 gr a turn were the bench's: on the slower stand-in scales they put the first throws over 15 to 61 points more often | G1, G2, G4, B1 | Waiting |
| 17 | **`controller`, `mode`** (removed) | Out of the API and the screens. Today's loop and the glide's pour stay on the console only, never stored, for the comparisons the gates ask; today's loop's code goes at gate 6 | The charge settings' filler, read as nothing | Sign-off item 1 | A18, C21, C22 | Waiting |
| 18 | **The experiments** (removed) | The jog finish; the controller `pid_learnt` and its constants (its learnt figure is folded in, row 16); `ctune` (it becomes the calibration's offer on the screens); `ccon`'s writes; the PC's lag shift; the 21 K record | None | Built for evaluation only | C6, D5, D11, G3, G5 to G8, G10, F8 | Waiting |

## Open, to be discussed

- **A powder profile portable and shareable** (the owner, 2026-11-22: "any storage decision should be temporary, and later we will discuss how to make the per powder profile portable and sharable (noted although the powder profile is not 100% portable, but a good profile is a starting point when combined with auto learning feature"). Until it is discussed, every stored form gate 2 has made is temporary: the calibration's slot of each powder profile, the throws' first errors carried into it at a charge's end, the mode in the charge settings' filler. **Closes** when the owner and the agent have settled the stored and shared form, before gate 3's calibration job writes a calibration of its own.
  - **Drafted** 2026-11-22, 19:45, with the owner's direction of that night (split by physics; the powder's part shared; the scale's and the printed parts' part on the unit, not configurable, backed up and restored; the glide kept beside a PID): [controller_design_draft.md](controller_design_draft.md). It covers one frame with two speed laws, the four records, each parameter's life cycle, the faults, tuning by hand with the auto-tuner, the timings, the servo gate (issue #135), the migration and the stored form, and ends with the owner's eight decisions. Not approved.
  - **Proposed to close it** (2026-11-23): the register's row 9, the stored form. The powder profile stays as it is and is shared as it is today; what belongs to one unit is in two kinds of record of its own. The draft's extension of the profile, its figures against a standard tube and its lot are not built: nothing in the sign-off needs them. It closes on the owner's approval of that row.

## Not in this ADR

- Ladder loading and other new modes.
- Profiles from an online library.

## To settle before approval

Settled since: what "better" is (the two bands, 2026-11-21), the order (ADR-008 closed first), the rig the proof is made on (the owner, 2026-11-21, 17:29). Left for the owner, and taken at the close of the build plan:
- **ADR-012's check against the firmware before it** ("Handed on by ADR-012"): proposed retired. The bench trial ran on ADR-012's motors, and today's loop puts 85% of AR2209's throws over on either firmware, so the comparison would say nothing of the motors; gate 6's side by side is against today's loop on the same firmware.
- **PR #113** (ADR-002 left its end here): its tuner tunes the two PID loops, which stay only as the fallback until gate 6, so nothing of it is taken. Proposed: closed with thanks and a note naming what this ADR built, at the owner's word, when this ADR's PR is opened.
