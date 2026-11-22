# ADR-009 research: handover

Written 2026-11-20, 00:20 NZDT, at the owner's request, by the session that made the research, the simulation and the first bench session. For the agent who goes on. Read this first; it says what is known, what I think it means, and what to do next, in order.

ADR-009 was a Draft when this file began; it is **Open** since 2026-11-21 (the newest section below). The sections below the newest are the research that led to it, kept as they were written.

## 2026-11-22, 18:41: the legacy PID with learning, thrown against the glide

The owner asked whether today's PID loop, with the glide's learning, can do as well or better. This session built it with no calibration: the PID on a forecast it learns from its own throws. It threw that against the glide on the rig in ABBA blocks of 10 at 10 gr. **The glide won:**
- settled good 18 of 20 against the PID's 14;
- 2 overs against 5;
- 10.9 s against 13.7 s.

The cause: without a calibration the PID stops at its least speed (0.1 rev/s here) where the glide's calibration sets 0.058.

**A second run (19:10 to 19:22):** the PID's least speed was set from its learnt figure, the glide's rule with no calibration. **It matched the glide's accuracy** (16 of 20 settled good against 15, 4 overs against 5) **and was 4.5 s slower** (15.5 against 11.0 s). So the predictor and the finish give the accuracy. **Corrected at 19:40:** the 4.5 s is all in the bulk, from the coarse tube's settings (the owner's coarse kp 0.05 and 5 gr stop). The approach and the rest took the same time as the glide's (`bench/phase_times.py`). So "the lag at every speed gives the time" is not shown; the next test is a coarse tube set from the calibration.

**A third run (19:51 to 20:04), the owner's option 1:** the PID's coarse tube set from the calibration (kp 0.306, a stop 1.43 gr short, 0.3 to 2 rev/s, its powder in the forecast, the back-off off).
- **Accuracy beat the glide:** 19 of 20 settled good, 1 over, against 16 and 4.
- **Time failed:** 14.8 against 11.2 s.
- **The cause:** the coarse tube stops about 2.5 gr early. The fine tube's powder at 2 rev/s is forecast with the creep's learnt figure (1.48 to 2.06), and the A&D lags longer at a low flow. The simulation's scale does not, and missed it.
- **Next, proposed:** the PID law on the glide's own predictor (lags by flow), with a scale in the simulation that lags longer at a low flow.

**A fourth run (20:13 to 20:25), the owner's option 2:** the PID law in the staged frame (`ccon speed_law=1`, `charge_control.c`, checked throw for throw), the owner's fine gains, the coarse tube's from the calibration.
- **Within what 20 throws resolve, it does as well as the glide:** 17 of 20 settled good against 19, 3 overs against 1, and 10.5 s against 11.4 s, quicker.
- Two of its overs were each block's first throw, from the glide's carried stop: each law needs its own carried stop.
- **Options 1 and 2 are closed;** the next steps are the owner's choice (`../controller_design_draft.md`, decision 8).

The owner's design questions are drafted in `../controller_design_draft.md`, and the live description is `docs/charge_controller.md` (AGENTS.md: kept true in the same commit as any change to the controller).

The owner, after the first run, settled a direction:
- split the profile by physics;
- share the powder's part;
- keep the scale's and the printed parts' part on the unit, not configurable from its pages, but backed up and restored;
- keep the glide, with the predictor and the other non-PID parts (the reverse, the advanced ones) running beside a PID.

Not yet written into the ADR: the README's "Open, to be discussed" is where it goes, with the owner.

Everything is in `pid_learning_2026-11-22.md`; the figures are in `../progress.md` (18:09 to 18:41, and 18:50 to 19:23). The code stays on the branch as an experiment: `controller=pid_learnt`, `src/charge_pid_learnt.c`, `cpid`. Whether it is kept, as a fallback for an uncalibrated powder, is the owner's to decide. The section below (18:00) is unchanged and still the plan's starting point. The bench was left at `controller=staged`, running the build of this work (`2.0.3-169-dirty` at ac27847).

## Where it stands, 2026-11-22, 18:00: read this first

Written at the owner's request ("record what we learned in to the handover note, for the next agent to actually build the system based on our hands on experiment today") by the session that built gate 2 on the unit and threw it all day on the rig. ADR-009 is **Open** (approved 2026-11-21 evening, gates 0 to 2 to build first). Everything below is on the branch `adr009_controller`, local commits up to `b23b214` and later, **not pushed**. `../progress.md` holds every figure with its recording; `talk_2026-11-22.md` holds the day's design talk with the owner and the simulation runs made to answer it, **a record, not a reference**: nothing in it is a decision.

**Which controller ran on the rig today:** every rig throw was the new staged controller, in the firmware (`src/charge_control.c`), glide finish. The legacy PID loop ran only in the simulation's comparisons and once in `charge_cycle` on the simulated scale.

### What is built (gate 2 and the day's MVPs)

- **The controller** in the firmware's job task every millisecond, beside the PID loop (`settings/charge controller`, staged by default). Gate 1's check (`check_firmware_controller.py`) proves it against `staged.py` throw for throw; run it after any change to either. It has passed at 20 gr and at 5 gr with everything below on.
- **Two mends found on the rig:**
  - the band's edge is a quarter digit (a reading less a large base was off the digit grid in float);
  - the step feed rounds a segment up (it ran late at about 1,000 steps a second, the 0.02 gr band's creep; `motor`'s `fine_underruns` 0 at every check since the mend, each flash starting it afresh, over the day's 260 throws after it).
- **The simulated scale** pours each tube's own grains a turn every 2 ms, reads on a digit, never snaps to the target, lifts the cup when the charge asks, and has its own calibration, so `charge_cycle` runs the staged controller.
- **The modes, an MVP:** `settings/charge mode`, `accurate` (±0.02) or `fast` (±0.04), a byte in the charge settings' filler (0x1A). Not on the screens or the portal yet.
- **The fine tube's figure learnt over a session,** as one figure or split into a stream's and a finish's (`ccon learn_fine_prior` 3, `learn_fine_split` 1, both on by default). **Not to be trusted yet** (below).
- **The stop's learning carried into the calibration:** at a charge's end its throws' first errors, translated to the calibration's own stop, go into the profile's slot (the newest 16; at most 3 digits at once). The next charge places its stop from them. **A temporary form:** the README's "Open, to be discussed" keeps every stored form of gate 2 temporary until the profiles' portable, shareable form is settled with the owner.
- **The proofs on the Pico 2 W:**
  - pass: `pulses`, `menu_tour --compare`, `charge_cycle` (both loops);
  - `bench/robust_staged.py`: every case passes but one. A cup lifted mid-throw is poured on at full speed, by both loops. **The owner made that a condition of the ADR's sign-off, not the current focus** (README, "How the unit shows it works").
- **Not built of gate 2:** the API's recordings (`/api/v1/recordings`). Not begun: gate 3 (the calibration as a job on the unit), gates 4 to 7.

### What the day established (each in `../progress.md` under its time)

1. **The algorithm holds.** Where the fine tube stops from its creep, the throw rests −0.006 to +0.012 gr from the forecast on average, its scatter the powder's own (sd 0.036 to 0.045 gr). Verify any change by the same reading: `bench/verify_series.py` on a series' rows.
2. **A wrong calibration costs time, not accuracy**, because the closed loop puts the weight right. In the simulation and on the rig, lags 0.055 s long cost about 6 s a throw (series B 18 s; with them mended, D 11.7 s).
3. **The calibration must be measured the way the firmware throws:**
   - **lags in the firmware's own time:** a PC adds 0.055 s;
   - **each tube's grains a turn where it is used.** The coarse tube poured 3 to 9% more a turn in throws than in runs from rest. The fine tube pours 0.83 gr a turn over a whole 5 gr throw and 0.68 at its finish: one figure cannot hold both;
   - **the fine tube's lag at its top speed of 1 rev/s:** never measured (the stops reach 0.5 rev/s), and the learning of the fine figures reads through it, which is why they run to their limits;
   - **the stop's scatter from throws, not short streams from rest:** throws scatter by 0.046 gr where the calibration measured 0.028, and its tail (6% of stops land 4 digits or more above their aim) is what an over budget lives in;
   - **its rehearsals as a throw's last stretch:** the PC's rehearsals at 0.5 rev/s placed the firmware's stop late.
4. **Overs come from two places:** the stream's stop scatter, and small top-ups that release a clump. **A dry turn owes its powder:** after three or more nudges that showed nothing, the next release is a clump three times as often, and what comes out is what the dry turns owed (the 11-20 and 11-21 nudge recordings, both powders). The forecast forgets owed powder once its lag has passed. Counting it before a top-up takes overs away where top-ups are many (simulation; not built).
5. **The top-ups decide the time:** 8 to 12 s for a throw without one, 17 to 24 s with one. That is the lever for speed, not the waits.
6. **Accurate mode adds nothing with a clumpy powder:** fast-mode throws judged at ±0.02 were within it as often as accurate's (57% against 62%) with half the overs. The owner's direction: optimise the fast mode; budget one over in 15 for both modes; "don't over build if the root cause is clump". A BM2 series would say whether accurate ever pays.
7. **The rig drifts, and that is what learning is for.** Two series an hour apart with nothing changed differed by a second; at the same calibration and stop, first rests moved 1.5 digits in twelve minutes while the powder's grains a turn did not change. The owner (2026-11-22): "I'd expect the rig will drift, so that's the real shrine of the continuous learning works. I don't know exactly why, but this is what I observed during a long session on the bench before, perhaps the distribution of small powder from the kernel acting like lubrication that could drift the friction from continuous session." His observation and his guess at the cause; not measured.
8. **So never judge a change by medians across series.** Judge within a series: the first ten throws against the last ten, the figures' movement, a control run after.
9. **Learning, from a fresh calibration, pays:** 65% good and 14.0 s with every learning off, against 80% and 12.1 s on (the last ten 9 good, 1 over). Most of it is the stop's learning (15% of each first error), mending a stop the rehearsals had placed late. On a calibration already mended from the rig's own throws, learning showed nothing: the owner pointed out that bias.
10. **The carry-over works:** each charge carried its first errors into the slot, and the next charge's stop moved by them. The slot after one session held the stop the drifted rig needed next; a fresh calibration in the same place started 1.5 digits off and paid for it with 4 overs in its first ten.
11. **PID against staged** (`pid_and_staged_2026-11-22.md`): the PID loop is fast because it never waits, and over because it cannot see the powder in the air. Fed the forecast it stops being over and ends short, since it has no finish. What it offers the new controller is its form (plain gains, a floor, a top) and its going on without waiting. The firmware's PID derivative is per millisecond, so the field's Kd values do almost nothing.

### What to build next (for the owner to approve as the plan; nothing here is approved)

1. **The calibration as a job on the unit** (gate 3), measuring in the firmware's own time and in a throw's shape:
   - each tube's lags at every speed a throw uses, the fine tube's 1 rev/s among them;
   - each tube's grains a turn for its stream and for its finish;
   - the stop's scatter, with enough throw-shaped rehearsals to see a tail (40 or more);
   - the small moves' releases, with their dry runs.
   `bench/verify_series.py` on its own throws is its check. Settle its stored form with the owner first (README, "Open, to be discussed").
2. **The learning, kept as built:** the stop's within a session and carried across sessions. The fine figures' learning only after item 1 gives it the lags it reads through, and checked then by the same within-series test.
3. **The fast mode as the one to optimise.** Its time is in the top-ups; whether the owed-powder count belongs in it is a question for the simulation first.
4. **Gate 2's rest:** the API's recordings. Then gates 4 to 7 as the README has them, the Pico W's proofs (gate 7), the cup-lifted safety before sign-off.

### Defaults on the unit as left

- `controller=staged`, `mode=accurate` (stored), `learn_share=0.15`, `learn_fine_prior=3`, `learn_fine_split=1`, the carry on.
- The constants (`ccon`) are live, never stored: a restart puts the defaults back.
- Profile 0's slot holds the fresh calibration of 16:46 (its lags 0.055 s earlier) with the 17:46 session's first errors carried in.

### Tools and traps

- `bench/series.py`: `--mode`, `--lag-shift`, `--coarse-gr-turn`, `--fine-gr-turn` and `--keep-calibration`; it sets `ccon` and the mode live and puts the stored charge settings back at the end. `bench/trial.py --throws 0` makes a calibration and its rehearsals with no PC throws. `bench/robust_staged.py` puts faults in. `carry_stop.py` and `bench/learning_loop.py` are the day's simulation studies.
- **The simulation:**
  - its fine tube pours the same at every speed (`rigs.get_rig("<record>+fine_by_speed")` is an assumed variant, not a record);
  - its tube cannot be trusted above 0.5 rev/s, where the rig ran 1.0;
  - it scatters less at the stop than the rig;
  - so it ranks changes and predicts times well, and is kind to accuracy at the finish.
- **One console session at a time.** Never put `&` inside a background command: the task ends at once and the series runs on unseen.
- Inline heredocs in this shell collapse backslashes and break on apostrophes: write a script file, or use the editor.
- `build/app.elf` must be the image on the unit for the console to work; rebuild only to flash.
- A series stops when the cup would pass 600 gr; the owner empties the cup when asked, and sometimes before.

**The bench as left (17:53):** the Pico 2 W on the probe, idle, both motors off, no console open. It runs the build of `a1c6ae4` (the carry-over); later commits are documents and tools only. The cup holds about 300 gr; the hopper's level was not read (some 3,000 gr of AR2209 were thrown today, the cup emptied between).

## Where it stands, 2026-11-21, 17:29: the day before

**Both proposals have thrown on the real unit, with AR2209, and the trial is judged.** `../progress.md`, "the two proposals on the unit" (the three short runs, what they found, what was mended) and "the full trial with AR2209, judged". `proposals_2026-11-21.md` now opens with what the bench did to it. In short:

- **The frame holds on a unit; the ranking turned for this powder.** 25 throws a set at 10 gr: proposal 1 (stop short and jog) 56% good within 0.02 gr in 25 s, where its check asked 60%; proposal 2 (glide and stop) 68% in 13 s; both 84% within 0.04 gr. Checks A, C and E pass, B and D fail.
- **Why:** this powder gathers at the fine tube's lip and falls as a clump. The smallest move gave nothing 66 times in 100 and four to nine digits 14 times. The stream's stop is tight (first rests spread by about two digits). The simulation had the stop right and the moves too kind.
- **The ceiling with AR2209, from the trial's own moves and stops** (`tools/charge_sim/bench/best_finish.py`): about 70% within 0.02 gr and 90% within 0.04 gr, whatever the finish. The finish decides the time.
- **The controller changed** (`tools/charge_sim/staged.py`): the lag is measured over a whole run; both tubes keep clear of the coarse tube's unseen powder; the fine tube takes its stop on its own powder alone; a rest far short goes back to the stream; a move is a turn made to the unit's step count. The simulation's figures moved by two points at most (`fit/bands_after_bench.txt`).
- **The tool changed** (`tools/charge_sim/bench/trial.py`; `trial_guide.md` is as it now stands): it reads the unit's own step count each round, makes a short turn with nothing read in between, starts a held tube from switched off, and can measure a kept trial's turns again (`--remeasure`). **Start any trial with `--throws 2`:** three faults showed there before an hour was spent.
- **The recordings are kept** (`tools/charge_sim/recordings/`, the session `pico2w_ar2209_trial`): four folders, of which `20261121_162403` is the trial that is judged.

**The owner's direction for what comes next (2026-11-21, 17:29):** "BM2 is low risk, so the next won't run the BM2 bench test. Instead, I'd expect the next agent to start to implement the controller and calibration in the firmware, and perform basic tune, against the calibration on the same rig based on proposal today. The next agent will be hands on, based on your research and simulation today." It is in `../README.md` in his words. (Minutes before, asked whether to run a BM2 trial, he had answered "Not today"; this replaces it.)

### For the agent who implements it

**Read in this order:** this section; `proposals_2026-11-21.md` (its first section, then 2 and 3.2); `tools/charge_sim/staged.py`, which is the proposal as about 500 lines of code and the reference to port; `../progress.md` from "the two proposals on the unit" on.

**Before any code, three things of this repository's rules:**
1. **Fetch, and start from `origin/main`.** This branch is 77 commits behind it and 45 ahead, all of them documents and `tools/charge_sim` (nothing under `src/`). It is not pushed since ff87a43 this morning.
2. **The ADR is still written as a Draft.** The owner's word above is the direction; the ADR's own "what we do", its gates and "how the unit shows it works" are not written. Write them and put them to him before the first gate is built (AGENTS.md: approval first; stage gates inside the ADR).
3. **Ask again whether the bench is ready.** As left: the Pico 2 W on the probe, AR2209 in the hopper, 558.5 gr in the cup.

**What is settled, and what is not:**
- **Settled by the owner:** the controller and its calibration go into the firmware now; the rig is the same one (Pico 2 W, A&D FX-300i, the air-exposed AR2209); the tune is hands on, on that rig. That is new: through the research his rule was that the bench captures data and nothing is tuned on the unit ("I don't want you to enter the tuning mode", 2026-11-20). I read today's word as lifting it for this stage; if a step feels like the old tuning by trial, ask him.
- **Settled by the owner:** no BM2 trial first. The frame carries both finishes as two numbers, so BM2 is thrown by the firmware itself when he loads it.
- **Mine, not his:** which finish is the default. I would build the frame once and start with proposal 2's finish (the creep and a stop a digit short), since it is the one that held on this rig, with proposal 1's kept as the other setting of the same two numbers.

**The bar to meet on the rig** (the PC's trial of today, 25 throws of 10 gr a set; `tools/charge_sim/recordings/20261121_162403_trial_pico2w_ar2209`): proposal 2's finish 68% good within 0.02 gr and 84% within 0.04 gr, 13 s a throw, no under. The firmware should do no worse and should be the quicker: it acts every millisecond where the PC acted every 40 to 80 ms. About 70% and 90% is this powder's ceiling here (`bench/best_finish.py`), so a tune that chases more within 0.02 gr with AR2209 is chasing the powder.

**What the rig's calibration gave, to tune against** (that folder's `calibration.json` and each set's own in `throws_*.json`): coarse 9.60 and fine 0.797 gr a turn; the coarse tube's spread 6.1% a run; lags of 0.60, 0.55, 0.58 s (coarse at 0.3, 1, 2 rev/s) and 1.02 to 0.82 s (fine, from 1 to 20 digits a second); every watch 5.3 s; the three moves 0.024, 0.071 and 0.158 of a turn. Proposal 2 within 0.02 gr: floor 0.025 rev/s, stop at 1 to 1.5 digits of room, at its floor from 5.2 digits, glide 0.66 s, coarse margin 0.61 gr, rest 2.3 s.

**What the firmware has that the PC did not, and what it must add:**
- **The step count is its own,** and a decision every millisecond: the motor task already ticks at 1 ms. Today's charge loop runs once a scale reading (`charge_mode.cpp`); the controller's seam is `start`, a tick, a reading (`tools/charge_sim/README.md`, last paragraph).
- **A small move is a number of steps.** The PIO makes only the steps it is fed (`step_pulses.c`), so a move fed as a count is exact. There is no such call today.
- **A stopped tube is held at zero, never at a creeping speed.** The PC's stand-in for a hold made a start up to 0.7 s late, because a new step rate waits for two segments at the old one. From a stop it does not wait. Check it once on the unit before trusting a move's timing; it is ADR-012's code.
- **The calibration is a job** like a cleanup (`jobs.c`), offered from the powder's profile, with the cup step kept before it. It took 13 minutes and 173 gr on this rig.
- **Where its tables are stored is a stored layout's change** and is said in the ADR: a profile today holds the two PIDs' gains and speed limits (`profile.h`), and eight profiles share one record.
- **ADR-013's rule stays:** no fresh reading, no turning. And nothing of today's loop is mended on the way: it is the code this replaces.

**Weak points the trial left, to mend while building, none begun:**
1. **The rehearsals:** ten throws of 1 gr from rest spread twice as widely as real throws' stops. Rehearse as a throw ends.
2. **Which move to make:** the rule is not the best play of a powder's odds (56% where the best play of the same moves gives 66%). `bench/best_finish.py` is that play at the desk; the calibration could keep its table.
3. **The coarse tube's last powder at its floor:** five throws of 100 ended 6 to 26 digits short after the coarse tube alone, one within 0.12 gr of an over.
4. **A small charge is slow at its start:** the coarse tube leaves about 2 gr of a 10 gr charge to the fine tube, five seconds of it. That margin is three spreads of an unseen coarse stream; a larger charge loses less.
5. **Not tried at all:** the Pico W, a second scale, BM2, any charge but 10 gr on the unit and 20 gr in the simulation, the servo gate.

**The simulation stays the place to try a change before the unit,** where it can say: `python bench/trial.py --made-up pico2w_ar2209` runs the same controller code against it in a minute. Its small moves are kinder than the rig's and its coarse start shows 0.1 s late (`proposals_2026-11-21.md`, section 5's last list); the recordings to mend it with are kept.

**Gates I would propose to the owner, as a start for the ADR's plan and no more than that:**
1. The controller as a module with no hardware in it, built for the PC too and checked there against `staged.py` throw for throw; every throw recorded.
2. That module in the charge, its numbers given from today's measured calibration, on the rig: the bar above.
3. The calibration as a job on the unit, its numbers against today's.
4. The screens and the web for both (every setting on both), and what the calibration tells the user.
5. The Pico W; a second powder.

**Bench traps met today, besides those below:**
- A tube started from the console's hold speed starts up to 0.7 s late: the firmware puts a new step rate on the pin only when two segments at the old rate are made (`step_pulses.c`). Switch it off first. This is ADR-012's to know of.
- A lag measured at the moment of a stop holds the latency of whoever turns the tubes; a PC's is 0.07 s, which is 1.3 gr of a coarse stream.
- The owner may empty the cup between runs without saying so beforehand; the tool counts every throw from the weight the cup rests at, so it does no harm, and it is read off the first weight.

**The bench as left (17:15):** the Pico 2 W on the probe, idle, both motors off; AR2209 in the hopper and 558.5 gr of it in the cup; no console session open.

## Where it stood, 2026-11-21, 12:30

Another agent took over. Two things changed; the section below still holds otherwise.

- **The recordings are in the repository and the branch is on GitHub** (the owner: "push the record to the github for future analysis, by me or other people"). `tools/charge_sim/recordings/` holds every session's `runs.json`, an index and each session's unit, powder and scale; its README says what a trial holds. After a bench session: add it to `sessions.json`, run `bench/keep.py`. Every tool's `--logs` takes that folder, so the desk work no longer needs the main checkout's `logs/adr009`.
- **The lead that step 3 was to test first is tested** (`../progress.md`, "is what arrives after a stop the scale's or the powder's?", its pass written before the run; `bench/scale_or_powder.py`). The scale alone does not forecast BM2's stops from AR2209's to a digit (off by 2.07); the powder's two forecasts are worse. A start is short by what a stop leaves, which any delay gives: "owed at the next start" is not a finding of its own. **So step 3 stands as written:** what arrives after a stop is a measured table by speed, for each powder and tube, over a scale with the 0.55 to 0.6 s delay every curve shares. BM2's table is one number (0.9 s of its flow); AR2209's is uneven by speed.
- **One recording is added to the next batch:** the fine tube stopped from 0.7, 1.5 and 2 rev/s, fifteen stops each. It says how much of the table is the scale's, which decides how a record is carried to another scale. `protocol.py` does not have it yet; it goes in with the throws of step 1 below.
- **A trap in `measure.py`:** with no folders named it reads every stop under `--logs`, and the folder now holds two powders. Step 2 below (a session's figures from its batch) must take its folders from the session, as `scale_or_powder.py` does from `sessions.json`.
- **Next is step 2 below, unchanged.** No bench step.

### The same day, 13:20: steps 2 to 4 below are done; step 5 is next

`../progress.md`, "a record for each powder", has the pass written first and every figure. In short:

- **A record for each powder:** `tools/charge_sim/records/pico2w_ar2209.json` and `pico2w_bm2.json`, each built from its session by `build_rig.py --session <name>` (BM2's with `--scale-from pico2w_ar2209`: one scale weighed both). `bench_pico2w` is gone. `tools/charge_sim/README.md` has the commands; a record's report is `fit/report_<record>.txt`.
- **The simulation's tube changed.** The leftover dropped at a stop and owed to the next start is removed: the recordings are not that shape. A tube has a table `late_s`, how much later than the scale's delay its powder shows at each speed, which holds at a stop, while slowing and at a start. BM2's is one number (0.19 to 0.25 s from 0.2 to 1 rev/s).
- **The check** (the speed plans with no controller, and AR2209's throws; none used to build): **BM2 passes.** **AR2209 passes on its throws** (slow finish 0.82 over against the bench's 0.83; the old simulation gave 0.47) **and fails on its throw-shaped ramp** (6.6 digits after the stop against 4.5).
- **AR2209's failure is one entry, the stops from 0.2 rev/s,** and the lead is that it is the test's order: those stops were shuffled with faster ones, and the ten that followed a faster trial left 11.0 digits where the seven that did not left 7.7. Not changed by hand. Until it is settled, a figure from AR2209's record is quoted with this said: two digits harsh where a finish slows through 0.2 rev/s, and a throw's spread at rest double the bench's.
- **For the next batch, three things in `protocol.py`, none yet in it:** each speed's stops in a block of their own, the first not counted, in place of shuffled speeds; the fine tube's stops from 0.7, 1.5 and 2 rev/s; the throws of today's loop as its last step.
- **GRT** (the owner sent its pages for both powders; `sources/grt_two_powders.md`): the two densities and no kernel size. The bulk densities are 1% apart. Asked of the owner: the window's "Detail Picture" tab for each powder, for a kernel's length and width.
- **Next: step 5 below,** the owner's questions by simulation, on BM2's record first. No bench step.

### The same day, 13:30: step 5 begun; the first figures, and the owner's two tolerances

`../progress.md`, "the owner's two tolerances". **The owner: good is within 0.02 gr, and within 0.04 gr for loads that need less; "don't over do"; tell him when an input is lacking** (`../README.md`). In short:

- **Every controller is now judged at both bands** (`tools/charge_sim/bands.py`). Today's loop with a slow finish: BM2 79% good within 0.02 gr in 15 s (a forecast: no BM2 throw is recorded), AR2209 15% (the bench's 10 over of 12). `flow_dose` is worse than that with both powders: not the candidate to go on from.
- **What a finish by small moves could reach, from the recorded nudges alone** (`bench/topup_bound.py`): within 0.02 gr, 88 to 90% good with BM2 and 80% with AR2209, however it is controlled; within 0.04 gr, 99% and 92 to 94%. So one over in fifty within 0.02 gr is not a controller's to give with moves of that size: it asks for smaller releases (the owner's oscillation). And such a finish is slow: 10 to 20 s for three digits.
- **Where a controller can win:** AR2209, from 15% to near 80%, and time. **Next, if the owner says so** (asked 11-21, 13:30): a finish that stops the steady stream by counted turns short of the target, then corrects once or twice, designed on the two records and judged by `bands.py` against these two tables. Its design is put to the owner before it is built.

### The same day, 14:30: two proposals made, the owner to choose

**Read `proposals_2026-11-21.md` before anything else in this folder:** it is what the owner asked for and has not yet answered. `../progress.md`, "two proposals for the controller and its calibration", has how it was made.

- **The owner's direction** (`../README.md`): no scale is characterised by hand, since the firmware reads nine; the calibration must find each scale's lag, and the lag depends on how fast the weight changes. Two proposals, each with its control and its calibration in detail, for any build, powder and scale, ranked by confidence.
- **The two are one controller** (`tools/charge_sim/staged.py`): a forecast (the reading plus the powder turned out and not yet shown, each parcel with the lag of its flow), one law a tube, a verdict on a reading at rest, small moves for a throw that rests short, and one calibration that measures every number on the unit. They differ in the fine tube's slowest speed and in how far short it stops.
- **Proposal 1, stop short and jog, confidence high; proposal 2, glide and stop, confidence medium.** In the simulation, within 0.02 gr on the measured scale: proposal 1 94% good with BM2 and 74% with AR2209 (5% and 17% over) in 12.7 and 16.6 s; proposal 2 89% and 74% (11% and 24% over) in 11.9 and 13.0 s; today's slow finish 79% and 15%. On a scale twice as late today's loop puts every throw over and both proposals keep 60 to 90%, in half a minute.
- **What limits the trial** (the file's section 5): the simulated fine tube is lumpier than the bench's (held to 0.5 rev/s for it; its model of a release needs mending above that); the simulated scale shows a small move sooner than the bench's; nothing is measured on a second scale.
- **The bench trial was run the same afternoon** (the section at the top). As it stood at 14:50: prepared and not yet run (the owner: "Yes, prepare it"). `trial_guide.md` is its guide: the tool (`tools/charge_sim/bench/trial.py`, the PC as the controller through the console, no firmware change), what the owner sets up, the commands, how it stops, the forecast written first (`tools/charge_sim/fit/trial_forecast_<record>.json`) and the five checks it is judged on. **It waits for the owner's word that the bench is ready, and the powder's name;** it was tried only against a made-up unit. About an hour and ten minutes and two emptyings of the cup. No firmware is designed until the trial is read with the owner.

## Where it stood, 2026-11-21, 01:50

The owner's direction for the bench (11-20, evening): sessions capture data and verify written assumptions ("make assumption, and use the actual trickler to verify the assumption"); no tuning on the unit and no design decision while capturing; hybrid control is open, to be decided later. `../progress.md` has it in the owner's words.

- **Two powders are now recorded on the Pico 2 W by the same tests:** AR2209 (air-exposed; 11-20, 21:08 to 23:36) and ADI BM2 (11-20 23:50 to 11-21 01:49). The capture is one script, `bench/protocol.py` (twenty steps, about two hours and one cup-emptying), summed up by `bench/batch_look.py`. Figures: `bench_2026-11-20/evening_capture.txt`, `late_capture.txt`, `bench_2026-11-21/bm2_batch.txt`.
- **What the two powders say** (`../progress.md`, "ADI BM2 recorded in one batch"): the powder follows the tube's turns, not its speed, for both. A powder's grains a turn cannot be had from its density (BM2 is 28% under AR2209 in the fine tube, 16% under in the coarse). The delay from the tube to the reading differs by powder (BM2 0.2 s longer), so it is not all the scale's. What a stop leaves and what a reverse costs differ by powder. The once-a-turn wave and the scale's answer to a digit carried over.
- **Prime until steady, not by a count of turns,** and prime a tube just before its own steps: `protocol.py`'s top says how.
- **One console session at a time.** A recorder that loses its console leaves the motor turning: send `motor fine 0` and `motor coarse 0` after any session that ends badly. It happened twice on 11-21.
- **Fitting is not begun** (the owner: capture first). The order in the section below stands; the capture protocol it asks for is written.
- **Still to capture:** the second unit (the Pico W's build), fresh AR2209, AR2208; BM2's kernel mass (the owner was asked for the weight of 50 or 100 kernels).
- **The bench as left (02:35 on 11-21):** Pico 2 W on the probe, idle; the owner packed BM2 away after the kernel count, so the hopper is taken as empty and the tubes as unprimed. BM2's kernel is 0.0117 gr (the owner's count); AR2209's 0.021 gr stands on the owner's word ("0.021gr is about the weight per kernel. So I don't think it worth a weight").

### The night's talk with the owner is a record, not a reference

The owner asked for an interim conclusion and then put four ideas as questions (the fine tube before the coarse, a stream in place of bursts, feedforward with the lag, a reverse turn as retraction). The answers given are in `talk_2026-11-21.md`, kept at the owner's word "only as the record, and not to be directly cited as the reference". Do not build on it or quote it as settled: take figures from `../progress.md`, and the owner's direction from `../README.md`. One thing in it was settled and is in the README: the work turns to the fine tube and the scale.

### Tomorrow's order (the owner asked for it; nothing below is begun)

**Settled with the owner at 03:20 (the ADR README has his words): the work turns to the fine tube and the scale.** The coarse tube is kept in the record as four measured numbers (grains a turn; what arrives after its stop; how far a move is off; that nothing falls late) and is dropped from the fit's targets otherwise. In steps 2 to 5 below, read "the tube" as the fine tube. Step 5's questions are the owner's six of 11-20 (the README); the talk's ideas (a stream against small moves with waits, feedforward with the lag) are candidates to try there, not directions. `../progress.md`, "the two powders read together", holds a lead step 3 must test first: the lag to the reading follows the flow and may be all the scale's.

1. **No bench step.** The agent named 30 throws of today's loop with BM2 as a held-out check only after telling the owner the capture was complete, and the owner had packed BM2 away ("I thought you finished the BM2 today so I packed them up already"). They are not asked for again now. BM2's record is checked on its throw-shaped ramp (30 trials, open loop, in the batch); its closed-loop check comes later as a prediction: the simulation's figures for today's loop with BM2 written first, then the throws, when BM2 is next loaded. **The cause is the protocol's: a batch did not hold the throws.** Before the next powder's batch, `protocol.py` gets them as its last step (the unit's address, a profile and a target as options), so "complete" means the check is in it.
2. **Desk:** `measure.py` takes a batch (`batch.json`) and makes the figures file for a powder: the fixed moves, the stops to 1 rev/s, the ramps, the speed steps, the reverse lengths, the watched nudges.
3. **Desk:** the simulation's tube doses by turns: what a stop leaves by speed, what the next start owes, what a reverse costs, and the powder's own delay apart from the scale's. The scale's small-change answer from the watched nudges.
4. **Desk:** two records built (AR2209, BM2), each checked on its throw-shaped ramp and its throws. Pass: the slow finish's over-rate within the throws' own scatter, which the simulation has never met.
5. **Desk, only once 4 passes:** the owner's questions by simulation: other scales by delay and small-change answer, finishes other than PID (dose by turns, end short, add), speed against accuracy, each with its figures.
6. **Bench, later, the owner's choice of when:** the same batch on the second unit (the Pico W's build) with one of these two powders, which tells tube from powder; then fresh AR2209 and AR2208, AR2208 held out as a prediction.

## Where it stands, 2026-11-20, daytime

The owner read the night's notes and set the next stage: "I don't want you to enter the tuning mode. Your job is to build simulation to answer my above questions without touching the physical units later on." Calibration is to be three parts (scale, tube, powder), built into the OpenTrickler and working without an AI agent; the simulation is to be one other users can adopt. `../progress.md`, "the simulation rebuilt on measured records", has every figure. In short:

- **The simulation's input is now a record of measurements** (`tools/charge_sim/records/`), made from a session's raw folders by one command (`build_rig.py`), with every number's source. Nothing is set by hand, and the throws of today's loop are the check, not a target. The hand fit of 02:05, `fit_search.py`, `matrix.py` and `pr113_check.py` are gone. `tools/charge_sim/README.md` has the commands.
- **It passes as a pipeline:** 63 to 70 of 80 of its own figures, 13 to 14 of 18 on throws it never saw.
- **It is still kind at the slow finish, and the cause is now measured** (`../progress.md`, "a capture session, the same unit and powder", 21:08 to 22:31 on 11-20). A stop from 0.2 to 0.5 rev/s leaves 4 to 6 digits more than the scale's delay accounts for, and the next start owes them; the simulation had no stop above 0.1 rev/s. A tube taken through a real finish's speeds in open loop leaves the 4.5 digits the real throws left. A tube that slows down does not lag: the evening's "flow lag" and the afternoon's "the scale's small change" were both wrong. **The data to fit it is recorded; the fitting is not begun.**
- **Also recorded that night:** the fine tube backward (a reverse quarter turn spills about a kernel; a reverse costs up to a third of a turn of the forward flow after it), back and forth at four swings a second (no more even than steady), the coarse tube steady, speed steps with no stop, and 300 watched nudges (how the scale shows one to five digits). `bench/record.py` has a `ramp` test for all of the speed plans.
- **A finish by one stop on the reading cannot reach 1 over in 50 at any evenness of the powder** (`evenness.py`): the finish must end short and add what is missing. Oscillation alone is not the answer.
- **Today's loop holds only near the scale it was set on** (`scale_map.py`).
- **Not measured at all:** the fine tube turned backward, the coarse tube's evenness, a stop the reading triggers. They go into the capture campaign with a second unit and other powders (the owner offers the Pico W's build, fresh AR2209, AR2208, ADI BM2). The campaign's protocol is not yet written.
- **The order:** `measure.py` and the simulation's twin take the new tests; the record built again with what follows a stop at every speed, what the start after owes and what a reverse costs, the throw-shaped ramp and the throws as checks; the scale's small-change shape from the watched nudges; the capture protocol as one fixed script for the second unit and the other powders; the design on the records, predictions written before the verifying throws.
- **The bench as left (22:31 on 11-20):** Pico 2 W on the probe, idle, nothing stored or changed; 546 gr in the cup, which needs emptying before any run (the recorder stops at 600).
- **A trap in the tools:** worker processes that had lived through many jobs failed in five builds of five on 11-20 (lost, or an impossible arithmetic error). Cause not found. `workers.py` starts fresh processes for every batch and runs a failed job once more; use it for anything spread over cores.
- The sections below are the night's, kept as they were written. Where they name `rigs.py`'s parameters, `fit_search.py` or the rig `reference`, those no longer exist.

## Where things are

| What | Where |
|---|---|
| The branch | `adr009_research`, off local main, local only. Checked out at `.claude/worktrees/adr009-research` (made with plain `git worktree add`) |
| What was done, with every figure | `../progress.md` |
| What the research proposes (D1 to D7), the owner's directions | `../README.md`, "Owner's direction" and "Decisions proposed by the research" |
| The desk research and its sources | `README.md` beside this, `sources/` |
| The bench procedure | `bench_guide.md` |
| The bench's results, a line a run | `bench_2026-11-19/` |
| The bench's raw frames, logs and the settings copy | `logs/adr009/` and `logs/rc/` of the **main checkout** (not tracked) |
| The simulation | `tools/charge_sim/` (pure Python; its README lists the files) |
| The recorder and the analysis scripts | `tools/charge_sim/bench/` |
| PR #113's branch, fetched for its data | local branch `pr113` |

Run the bench tools from the main checkout's venv and against the main checkout's `rc_run.py` and `build/`, which match the unit's firmware: `record.py --repo C:\workspace\OpenTrickler-RP2040-Controller ...`.

## The owner's directions, in their words (2026-11-19)

- Today "around 1 in 10/15 overcharge". "The target speed is ~10 [s] but if you could reach 1/50 overcharge that's a bonus. I'd accept 15s per charge." The design target for AR2209 at 42.5 gr was "around 7-15 seconds depending on the cluster", and "my AR2209 tends to have more cluster than other powder".
- The fine tube is the low-flow one (22 teeth, generation 2). "You can fix the staled gear ratio in the settings": done.
- "Fine tuned lip is hard to print, so not prefered. I'd prefer using high frequency oscillation if cluster is the real limitation."
- "backward compatible should not be considered during the initial research. Later when designing 'what to keep', then we will evaluate the cost to transit the system over, as we have 600+ users around the world now."
- A second goal: automatic tuning for a community whose printed tubes differ, "in two modes": autonomous, and assisted by an external AI agent through the API.
- On the tube-flow research: "just focus on flow dynamics instead of the powder behavior".
- The bench's powder: "this batch of AR2209 has been exposed in the air for a long time so its property might look different than the fresh news ones. but anyway, this is starting point."
- PR #113 may be read "for data".

## Findings

Marked **[B]** measured on the bench (Pico 2 W, 2.0.3-84, A&D FX-300i, large tube, low-flow small tube, the owner's air-exposed AR2209), **[C]** read in the code, **[S]** simulation on an assumed plant, **[D]** desk research.

### The tubes

- **[B] Flow follows speed, in both tubes.** Fine: 0.71 to 0.81 gr a turn at every speed from 0.05 to 5 rev/s (3.8 gr/s at 5), and a steady 3.0 gr/s through 8 s at 3.5 rev/s. Coarse: 8.0 to 10.1 gr a turn from 0.1 to 3 rev/s (23.6 gr/s at 3). No inlet ceiling, no sag, no fall toward the top speed in these runs.
  - So the owner's "rotary speed doesn't correlate directly to the charge speed" **did not show in open loop**. Not looked for: the coarse tube above 3 rev/s, a hopper nearly empty, minutes at 5 rev/s, and the closed loop, where the scale's lag makes the reading trail the flow (below). That last one is my best guess at what the owner sees.
- **[B] A tube that has stood still or been handled gives nothing at first:** the fine tube's first two 2 s runs gave 0, then 0.10, 0.10, 0.26 gr. Prime before measuring.
- **[C] The firmware's speeds are trickler rev/s** (`motors.c` divides by the gear ratio). The Pico 2 W's fine ratio was a generation-1 leftover (2.105); the step counts showed 0.432 rev/s for 0.5 asked. Now 1.818182, stored, with the owner's word. Other users' units may hold the same leftover.
- **Measured from the STLs:** both tubes are rifled drums (5-start, about half a kernel deep), 32 and 22 teeth; the small one is fed through one wall window; centrifuging at 5.8 and 8.2 rev/s, above the 5 rev/s limit.

### The scale

- **[B] After a stop, 0.5 to 0.65 s of the flow is still to show**, in both tubes, from 0.4 to 24 gr/s; the reading is within a digit of its end by 0.8 to 1.0 s. I had assumed 0.25 s.
- **[B] One kernel is far slower to show than a grain:** after a small turn the reading first moved at a median of 1.24 s (a tenth by 0.7 s, nine tenths by 2.0 s) and was still by 1.55 s (nine tenths by 2.6 s).
  - **Not separated:** whether that is the scale's filter (slow for small changes, as balances with a two-speed filter are) or the kernel falling late after the tube is released. `motor <tube> 0` switches the driver off, so the tube is free to move back.
- **[B] The A&D's frames carry `US` while the weight moves and `ST` at rest**, 20.7 a second. No driver uses that flag today [C]; it is the natural "settled" signal.
- **The A&D's response setting is FAST** (the owner, 2026-11-20: "The A&D is at FAST settings"). So the lag measured is the scale at its quickest: there is no faster setting to gain from. Zero tracking (`trc`) is still not known.

### The powder at the lip

- **[B] A kernel is 0.020 to 0.025 gr**, one digit of the A&D in grains. The band is ±0.02 gr.
- **[B] This batch clumps.** With turns of 4° and 3 s pauses, 110 nudges: 84 released nothing, 15 one kernel, 2 two, 9 three or more, up to 8 (0.17 gr). Long runs of nothing come before the large falls.
- **[B] The earlier nudge runs with 1.2 to 1.3 s pauses under-waited the scale** and counted some kernels to the next nudge. Read only runs with 2.6 s or more.
- **[B] Shaking from the console (4 to 5 Hz) showed no clear gain.** Three kernels or more, of trials that released: plain nudge 15 of 39, small shake 3 of 18, wide shake 5 of 16. A shaken trial moved half the powder of a plain one.

### Today's loop [C]

- The fine tube turns at its least speed until a lagged reading is within the stop threshold. With the measured figures: 0.1 rev/s is 0.075 gr/s, the scale is 0.55 s or more behind, so about two kernels are unseen at the stop, before any clump.
- Its gains hold the scale's delay and the powder's flow together; I and D do no useful work; the verdict is one reading a second after the stop, earlier than this scale shows a kernel.
- **[S]** On slower scales the simulated loop overcharged nearly every throw.

### The simulation [S]

- It runs today's loop (ported line for line) against a candidate with no tuning (`flow_dose`: a flow in proportion to the weight still to go through a measured flow map, then nudges judged on settled readings).
- **Its plant is now known to be wrong in three places** (the fine tube's flow per turn and ceiling, the scale's lag, the clump sizes). **Do not quote its over-rates or times** until it is refitted. What it showed that should outlast the refit: the overcharge rate is set by clump size once the blind feed is removed; sharing the feed between the tubes three ways made no measurable difference; a candidate calibrated on each unit held its figure across differently printed tubes.
- Tried and dropped: counting powder pushed to the lip as delivered, with a reverse turn (no gain).

## What these sessions are for: the owner, 2026-11-20, 01:20. Read this first

"Your session, as well as previous session is to capture data so you can design the simulation. I don't want you to slip into the live debug or tuning session. The goal is to design the simulation with adjustable parameter so we don't need to develop the algorithm solely on the device, but based on the simulation and verify by the experiment."

Read here as:
- **A bench run measures a parameter of the simulation, or checks something the simulation predicted.** A run that tries a controller's setting on the unit to see what it does is tuning, and is not run.
- **The algorithm is developed in the simulation,** whose parameters are adjustable: a powder, a tube and a scale are each a set of numbers, so another rig is another set and not another round on a unit.
- **So the next work is the simulation refitted to what is captured** (A2 below), and the sections under this one are corrected by it: where they say the simulation waits ("A2 and A3 are not done first"), that was the agent's, said against this goal, and is withdrawn. What was wrong on 11-19 was a plant assumed before any data, not the simulation.
- **The real throws of 01:00 to 01:09 were the slip:** two settings of today's loop tried on the unit. They are kept for what they are worth to the simulation: with the five throws as stored, three operating points of today's loop that the refitted simulation must reproduce (`../progress.md`).
- **The order now:** (1) refit `tools/charge_sim` to the recordings, every parameter in one place, and say which recordings it reproduces and which it cannot; (2) capture on the bench only what that fit lacks; (3) a second parameter set from fresh powder, which also tells this batch from the tubes; (4) the finish and the feed designed in the simulation; (5) its prediction checked by throws. Oscillation is an idea for (4) and (5), not a build before them.

### Where it stands, 2026-11-20, 02:05 (the owner asleep; decisions left for the owner)

- **(1) is done as far as tonight's recordings carry it:** `tools/charge_sim` has a rig `bench_pico2w` fitted to the bench; `python fit_report.py` sets the simulation beside each of the bench's 90 figures, and 70 to 72 are within tolerance. `../progress.md`, "the simulation refitted", lists what it reproduces, what it does not and what recording would settle each. `tools/charge_sim/README.md` says how to run every part.
- **It reproduces today's loop at the three settings recorded** (1.3 gr, 0.2 gr and under 0.1 gr over; within 0.12 gr and a second), so it can be used to design the feed. **It is kind at the finish** (half the slow-finish throws over where the bench had 10 of 12), so no over-rate is to be quoted from it yet.
- **Next, with the owner:** the list "For the owner, tomorrow" in `../progress.md`: fresh powder as a second set of numbers; twenty turns for the rifling; the scale's own response; then the feed and the finish designed in the simulation and checked by throws.
- **For the agent who goes on:** before a bench run, name the parameter it measures or the prediction it checks. Do not start from the old `flow_dose.py`, `calibrate.py` and `matrix.py` as they stand: they were written against the plant assumed before the bench, and their figures in `README.md` beside this are void. They are the place to start the design in step (4), on the fitted rig.
- **The bench as left:** Pico 2 W on the probe, idle, its stored settings as before tonight (the live changes reverted, read back `saved=true`); about 382 gr in the cup, which holds 700 and more; the recorder's limit is 600. Powder is still in the hopper.

## Revised 2026-11-20 by the agent who took over: read this before the interim conclusion

An independent review of the raw recordings (`../progress.md`, "a second agent's review") changes conclusions 1 to 3 and the order of the actions below. In short:

- **Conclusion 1 does not hold as written.** Turning steadily at 0.05 to 0.2 rev/s, this powder leaves the fine tube a kernel at a time (variance over mean of the kernels a window 0.65 to 1.4; the nudges give 4.9). The clumps come from stopping and starting, and from a driver switched off between nudges. They are a fault of the nudged finish (D2), not a limit of powder and tube.
- **Conclusion 2 is narrower than written.** After a stop from a slow steady turn the reading is still within about 1 s. Only one kernel alone after a nudge takes 2 s.
- **Conclusion 3's first candidate is the one to test** (stop the steady feed by what is already on its way), and oscillation waits until a steady finish is shown to fail.
- **Actions, in the new order:** (1) the creep-stop test on the bench (`record.py creepstop`; pass and fail in `../progress.md`); (2) by its result, ten recorded throws of today's loop with its stop moved, the owner at the cup; (3) the ADR revised with the owner. A1's question on the A&D's response setting still stands. A2 and A3 (the simulation refitted, the finish redesigned in it) are not done first: the simulation is refitted when a design question needs it that the bench cannot answer cheaper. A5 (oscillation) is parked with its closing condition: opened only if the creep-stop test fails at 0.02 rev/s too.

### After the creep-stop test (2026-11-20, 00:26 to 00:50): the revision above is itself revised

The test failed at 0.05, 0.1 and 0.02 rev/s (`../progress.md`, with the table "Where that leaves the finish"). In short:

- **Bursts are in a steady turn too,** and grow as the tube slows: at 0.02 rev/s as large as the nudges'. Conclusion 1 below is nearer the truth than the revision above allowed. What stands of the revision: nudging is the worst case, the driver's switching does not matter, the scale is still within 1.5 s of a stop.
- **The bind:** even flow needs a speed at which three digits and more are in flight behind the scale; a speed slow enough for the scale gives bursts.
- **Oscillation's question is open again** (its closing condition was met: a fail at 0.02 rev/s). What it must show is narrower now: an even flow at 0.02 to 0.05 rev/s, judged by the variance over mean of the digits a second (`creepstop` gives it), not by nudges.
- **The actions now:** (1) ten real throws of today's loop, recorded (`record.py throws`, written, tried only against a made-up unit): the baseline never taken, and the check of the prediction that today's loop on this unit is over in about half its throws; (2) by its result, with the owner: the `wiggle` research build for an oscillation laid over a slow turn; (3) the ADR revised.
- **Action (1) is done** (01:00 to 01:09, `../progress.md`): 29 real throws. As stored, today's loop is 1.2 to 1.5 gr over on this unit (its fine gain is four times what the measurements allow). Set from the measurements, with a slow finish, it lands within 0.1 gr in 11 s, and within the tolerance about half the time: what lands after the stop scatters by 2.5 digits, as in the creep-stop test. **Next:** the owner's answer on where "1 in 10 to 15" comes from (unit, tubes, powder), then the `wiggle` build if this bench is the case to solve, then the ADR revised.
- **How to throw on the bench without a hand at the cup:** `record.py throws --host <address> --target 20 --profile 1 --count 12 --rezero [--cup-holds <gr>]`. The owner's word first that an empty cup is on the pan; 600 gr is the recorder's limit for a cup that holds 700.
- **A trap in `creepstop`:** it stops at moments the PC chooses, so it understates a real finish's overs where the flow is bursty: a real stop is triggered by a burst arriving.

## Interim conclusion

1. **For this powder, tube and scale, clumps at the fine tube's lip are the limit**, not the controller. About a third of the releases from a small turn carry three kernels or more, and three kernels is the width of the band. No stopping rule reaches 1 in 50 against that.
2. **The scale is too slow for a finish that waits for it.** Judging each nudge on a settled reading costs 2 to 3 s a nudge here. D2 as written (nudges on settled readings) does not fit a 7 to 15 s throw on this scale; it needs rethinking, not tuning.
3. **So the two things to change are upstream of the controller's finish:** fewer kernels per release (the owner's choice: oscillation, to be tested properly), and a finish that does not need the scale after every move. The candidates I see, none tried:
   - **count the turn, not the weight, for the last kernels:** the flow per turn is steady (0.75 gr a turn, measured), so stop the continuous feed by a prediction of what is already on its way, then make one measured correction after a single settle;
   - **read the kernel sooner:** the scale's `US`/`ST` flag, a faster response setting if the scale is not at FAST, or a slope over several readings in place of waiting for stillness;
   - **make releases smaller and steadier** so that a blind finish has less spread: oscillation is the owner's preference.
4. **What stands from the proposals:** one demand shared between the tubes (D1), the scale and the powder kept apart (D3), a profile of measurements with a calibration (D3, D6), the verdict on a settled weight (D5), the simulation kept and refitted (D7). The measured maps are as simple as a calibration could wish: one number a tube.
5. **What is open again:** D2's finish, and D4's place (oscillation moves from "if needed" to the first experiment).
6. **One batch, one unit, one evening.** The owner says this powder was long exposed to air and clumps more than others. Fresh powder and another print may be kinder. The conclusion is about where to look, not a law.

## Actions for the next agent, in order

**A1. Ask the owner two things before anything else.**
- The A&D's response setting (hold MODE: FAST, MID or SLOW) and `trc`. If it is not FAST, set it and repeat the single-kernel lag (A4) before believing conclusion 2.
- Whether to build the `wiggle` test command (A5): a debug image on the Pico 2 W, flashed by the probe.

**A2. Refit the simulation to the bench** (desk work, no unit). In `tools/charge_sim/`:
- `rigs.py`: coarse 9 gr a turn; fine 0.75 gr a turn with no inlet ceiling (`max_inflow_gr_s=math.inf`, the inlet keeping up); the held-powder dynamics can stay but should do nothing.
- `rigs.py`, AR2209 as measured: kernel 0.021 gr; releases of 1, 2, 3, 5, 7, 8 kernels in the proportions 15 : 2 : 4 : 2 : 2 : 1 (mean 2.4); long gaps before large releases.
- `scale.py`, the A&D: about 0.55 s behind a flow; a single kernel first showing at about 1.2 s. In the model that is a slower closed filter (about 1.2 s) and an open one of about 0.25 s; check with `scale_step.py`.
- `calibrate.py` and `flow_dose.py`: the settle time after a nudge must come from the nudge test (how long a single kernel takes to show), not from 2.5 times the flow's delay, or the candidate will judge too early.
- **Pass:** today's loop in the refitted simulation gives the owner's own figures (1 overcharge in 10 to 15, 7 to 15 s for 42.5 gr) within reason. If it cannot, a cause is still missing: say so, do not tune it in.
- Then rerun `matrix.py` and replace the table in `progress.md` and the figures in `README.md` beside this.

**A3. Redesign the finish in simulation** (conclusion 3) before more bench time: a predicted stop with one correction against nudge-and-wait, at the measured lag and clump sizes, judged on over-rate and time. Also ask the simulation how much smaller clumps must be for 1 in 50: that is the target for A5.

**A4. Bench, without powder or with little** (ask the owner that the bench is free; `bench_guide.md`):
- the scale at rest and with each motor turning, hopper empty (not done: the hopper was loaded);
- the chain test (guide, step 1), to separate the scale's lag from late-falling kernels. It needs a fine chain from the owner;
- nudges with the driver left on between them, if a way is found without a firmware change, to see whether releasing the tube causes late falls.

**A5. Oscillation, properly** (the owner's preferred answer to clumps): the `wiggle` command of the guide's step 7 in a research build, then plain nudges against 20 Hz and 50 Hz swings with an exact net turn, **at least 300 trials a kind** (a release comes in about one trial of four; the console test's 18 releases settled nothing). Judge by the share of releases of three kernels or more and by the reading's noise while it swings.

**A6. Ten recorded throws with today's loop**, the owner at the cup, AR2209 profile selected (the unit has "AR2208,gr" selected now). They are the simulation's whole-chain check and give the real throw time.

**A7. Then revise the ADR** with the owner: D2 and D4 as the bench leaves them, the "what to keep" step for the 600+ users, and the gates.

## Traps and facts worth knowing

- **Bench rules:** ask the owner before any contact, every session. The cup holds about 300 gr; the recorder refuses above 250. It held 1.6 gr at 00:13.
- **The scale task's stack:** `stats` showed `stack_min="Scale Task"` fall from 160 to 48 during about 20,000 tapped frames. I stopped using `scale tap on`. Use `record.py flick`'s way (weights by `state`, averaged) or fix the cause first; tell whoever owns ADR-008 or ADR-013. Not changed by me.
- **Step counts count both directions**, so a back-and-forth move's net turn is not known from them.
- **`motor <tube> 0` disables the driver.** A nudge from the console therefore ends with the tube released.
- **A nudge from the PC is at least about 45 ms**, so about 0.011 turn at 0.2 rev/s; smaller or faster moves need firmware.
- **Git Bash here-documents** break on apostrophes in this shell: write a script file instead.
- **numpy and matplotlib** were pip-installed into the repo's venv and never used; not in `requirements.txt`.
- **A `matrix.py` run lost a worker once** (`BrokenProcessPool`); not reproduced.
- **Do not push or merge** without the owner's word; main has unpushed commits that go with ADR-008's push.
- **A usage-policy safeguard stopped one research subagent** on the first try; the owner had it retried on another model with a narrower brief. Keep desk research on flow dynamics, not on the powder's own behaviour.
