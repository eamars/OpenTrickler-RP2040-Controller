# ADR-009: one controller, every throw recorded, automatic tuning on top

Status: **Draft** 2026-11-13. Not approved. Research made for it on 2026-11-19 is in `research/` and `progress.md`; what it proposes is under "Decisions proposed by the research". Captures how the charge is controlled today, what is known to be wrong, and ideas. Nothing here has been tried on powder.

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

## A gate: the servo gate tuned by eye (the owner, 2026-11-19)

The owner, during ADR-008's phone checks: "I want you to consider a servo gate tuning feature, perhaps offer that in initial commissioning as well as in the settings menu." Weighed the same day against ADR-008, the owner chose "A gate in ADR-009": ADR-008 was at its last checks, and the bench's unit has no gate fitted to prove it on. Here because this ADR holds how the charge uses the gate (it moves at the coarse stop, above), and tuning is the set-up half of the same part.

**What we want.** A person with a gate fitted sets it up by watching it, not by typing numbers: each shutter's closed and open ends found by moving the shutter a little at a time until it is right, then kept; the speeds tried by opening and closing it. Offered in the set-up when the unit has a gate (ADR-008's commissioning), and in Settings › Servo gate, on the screen and on the web alike (screen and web parity, ADR-010).

**What we know** (checked 2026-11-19 in `servo_gate.c`, `menu_nav.c` and the portal):
- The gate's settings are six numbers and a switch: Fitted; each shutter's closed and open ends as "a share of the PWM's period" (0.02 to 0.13, a servo's pulse at 50 Hz), which a person can only find by trying; the closing and opening speeds in full travels a second. Stored in its own EEPROM slot.
- The screen's Servo gate page already moves the gate to the end whose number is being stepped (`gate_show_end`), so its end is seen as it is set; the web's form does not: a number typed there applies at the gate's next move.
- The web's Servo gate part moves the gate now (Open, Close, how open), from the unit's own reading of it (ADR-008, gate 3). Both shutters move together, each between its own two ends.
- The set-up's steps are a list on the unit (`setup.c`: code, wifi, scale, motors, ready, done), drawn by both screens, so a step after Motors can be added without changing the others.
- The bench's unit has no gate fitted (AGENTS.md); a bare servo on its connector shows the pulse's ends turning, though not a shutter closing.

**Proposed, not designed.** A design gate first, with mocks of both screens for the owner to settle (the skills `embedded-ui-design` and `web-ui-design`), then the build:
- a tuning flow per shutter: its end shown live while it is nudged, "Set closed" and "Set open", the two speeds tried, kept with one store at the end and nothing stored if left;
- the set-up's step "Servo gate", offered only when the build carries the gate and Fitted is on, and skipped with one tap;
- whether the ends stay numbers anywhere a person sees them, or only as the tuning's result.

**How the unit shows it works:** a gate fitted on the bench (or, before one is, a servo on its connector), tuned from the set-up and again from Settings on the screen and on the web; both shutters closing and opening at their ends; a charge with the gate, `charge_cycle` passing; the stored ends surviving a restart.

## How the unit shows it works

- The owner's recorded throws before and after, same powder, same target, at least ten each: time to target, overshoot and spread no worse, and better where a fault was fixed. The first set is also ADR-012's check: against the firmware before ADR-012, no worse ("Handed on by ADR-012").
- The PC tests in CI.
- `charge_cycle` passes.
- A tuning run completes, saves a profile, and ten charges with it land inside the threshold.
- On each supported board.

## Not in this ADR

- Ladder loading and other new modes.
- Profiles from an online library.

## To settle before approval

- What "better" is, as numbers the owner cares about: time to target, overshoot, spread.
- Whether the controller and its recordings move ahead of ADR-008.
- Which powders and which scale the proof is made with.
