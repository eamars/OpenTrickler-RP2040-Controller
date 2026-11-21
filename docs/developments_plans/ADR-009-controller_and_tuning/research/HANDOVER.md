# ADR-009 research: handover

Written 2026-11-20, 00:20 NZDT, at the owner's request, by the session that made the research, the simulation and the first bench session. For the agent who goes on. Read this first; it says what is known, what I think it means, and what to do next, in order.

ADR-009 is a **Draft, not approved**. Everything here is research for its design. Nothing is merged or pushed.

## Where it stands, 2026-11-21, 17:30: read this first

**Both proposals have thrown on the real unit, with AR2209, and the trial is judged.** `../progress.md`, "the two proposals on the unit" (the three short runs, what they found, what was mended) and "the full trial with AR2209, judged". `proposals_2026-11-21.md` now opens with what the bench did to it. In short:

- **The frame holds on a unit; the ranking turned for this powder.** 25 throws a set at 10 gr: proposal 1 (stop short and jog) 56% good within 0.02 gr in 25 s, where its check asked 60%; proposal 2 (glide and stop) 68% in 13 s; both 84% within 0.04 gr. Checks A, C and E pass, B and D fail.
- **Why:** this powder gathers at the fine tube's lip and falls as a clump. The smallest move gave nothing 66 times in 100 and four to nine digits 14 times. The stream's stop is tight (first rests spread by about two digits). The simulation had the stop right and the moves too kind.
- **The ceiling with AR2209, from the trial's own moves and stops** (`tools/charge_sim/bench/best_finish.py`): about 70% within 0.02 gr and 90% within 0.04 gr, whatever the finish. The finish decides the time.
- **The controller changed** (`tools/charge_sim/staged.py`): the lag is measured over a whole run; both tubes keep clear of the coarse tube's unseen powder; the fine tube takes its stop on its own powder alone; a rest far short goes back to the stream; a move is a turn made to the unit's step count. The simulation's figures moved by two points at most (`fit/bands_after_bench.txt`).
- **The tool changed** (`tools/charge_sim/bench/trial.py`; `trial_guide.md` is as it now stands): it reads the unit's own step count each round, makes a short turn with nothing read in between, starts a held tube from switched off, and can measure a kept trial's turns again (`--remeasure`). **Start any trial with `--throws 2`:** three faults showed there before an hour was spent.
- **The recordings are kept** (`tools/charge_sim/recordings/`, the session `pico2w_ar2209_trial`): four folders, of which `20261121_162403` is the trial that is judged.

**What is the owner's to say next** (put to him 17:30, not answered): whether to run the same trial with BM2 (an hour; its forecast is written, `fit/trial_forecast_pico2w_bm2.json`: proposal 1 94% and proposal 2 88% within 0.02 gr). It says whether the ranking turns for a fine powder too.

**What I would do after it, in order, none begun and none approved:**
1. **Mend AR2209's record from the trial** (`proposals_2026-11-21.md`, section 5's last list): its small moves, its first rise after a coarse start, its coarse tube's powder a turn at speed.
2. **Let the calibration choose the finish:** play the finish on its own measured moves and stops (`best_finish.py` is that play, at the desk) for the aim, the move to make from each shortfall, and the share of good throws to tell the user. In the simulation first.
3. **Rehearse as a throw ends,** not with a short stream from rest: the rehearsals' spread was twice the throws' and set proposal 1's aim too far short.
4. **The coarse tube's last powder at its floor:** five throws of 100 ended 6 to 26 digits short after the coarse tube alone, one within 0.12 gr of an over.

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
