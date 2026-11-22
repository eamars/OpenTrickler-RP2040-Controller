# ADR-009: one controller, every throw recorded, automatic tuning on top

Status: **Open** 2026-11-21. Its sign-off, what it delivers, is final since 2026-11-22, 22:17 ("Sign-off: what ADR-009 delivers"); the remaining gates toward it are proposed, not approved. Earlier: 2026-11-21: the plan under "What we do" approved by the owner ("Approved. You can start with gate 0, 1, and 2."). Drafted 2026-11-13. Research made for it from 2026-11-19 to 2026-11-21 is in `research/` and `progress.md`. The plan to build the controller and its calibration is under "What we do" (written 2026-11-21, 20:58, with the owner's four answers), approved the same evening; gate 0 done, gates 1 and 2 being built. On 2026-11-22 night the PID law was built into the staged controller's frame and thrown against the glide, at the owner's question. A revision of the remaining gates for the final firmware is proposed under "Proposed for approval" below; it is not approved.

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
- **The controller and its calibration go into the firmware next, tuned hands on, on the bench's rig** (the owner, 2026-11-21, 17:29, after the bench trial of the two proposals with AR2209): "BM2 is low risk, so the next won't run the BM2 bench test. Instead, I'd expect the next agent to start to implement the controller and calibration in the firmware, and perform basic tune, against the calibration on the same rig based on proposal today. The next agent will be hands on, based on your research and simulation today." So no BM2 trial comes first, and for this stage a tune on the unit is his word, where the research's bench sessions only captured data. `research/HANDOVER.md` has what the trial found and where the next agent starts. The plan and its gates are under "What we do".
- **Four answers for the build** (the owner, 2026-11-21, just before 20:58, to the implementing agent's questions):
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
- **The facility, not this unit** (the owner, the same night): "I want you to make sure you are building the facility, not sliding into the tuning against this particular unit. Any data captured today should be used for future calibration." The agent agreed, and named where it had slid (below, "Proposed for approval").
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
   - adding the law per powder, "set" and "in use", the offer, keeping and forgetting what was learnt, a new lot, the backup and restore.
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
   - A powder never calibrated runs from the user's PID values, guarded by the controller's frame (the forecast, the speed envelope, the settled verdict), from its first charge.
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
10. **Migrate transparently:**
    - an update from 2.0.3 keeps every profile and setting;
    - the first charge after it works with no calibration;
    - the calibration is offered, never forced.
    - *Shown by:* the update on both units from 2.0.3, the settings compared before and after (`rc_run.py settings --compare`), and a charge thrown at once.
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

Every parameter or setting the work adds, keeps with a new meaning, moves or removes, one line each. **Nothing in it is approved until the owner's approval and date are in its row** (item 2). The design draft's records (`controller_design_draft.md`, sections 6 and 7) are its first candidates. So are the existing settings' meanings in the one controller (item 3), and the night's experimental stores (the 21 K record, `ccon`'s constants).

| Parameter | Where it is stored | Who sets it | The powder's or this unit's | Default, bounds | Why | Its life cycle | Approved |
|---|---|---|---|---|---|---|---|
| The auto-tuner's switch | To be decided at gate D: a setting of the unit, or of each powder | The user, on demand; an agent on the user's yes | To be decided at gate D | On | The owner: "so user can disable it on demand" | Recommended (for gate D): off freezes the auto-tuner, nothing more learnt or carried, the numbers in use kept; going back to the calibration's own numbers is a separate "forget what was learnt" | The switch and its default on: the owner, 2026-11-22. Where it sits and what off keeps: not yet |

## Open, to be discussed

- **A powder profile portable and shareable** (the owner, 2026-11-22: "any storage decision should be temporary, and later we will discuss how to make the per powder profile portable and sharable (noted although the powder profile is not 100% portable, but a good profile is a starting point when combined with auto learning feature"). Until it is discussed, every stored form gate 2 has made is temporary: the calibration's slot of each powder profile, the throws' first errors carried into it at a charge's end, the mode in the charge settings' filler. **Closes** when the owner and the agent have settled the stored and shared form, before gate 3's calibration job writes a calibration of its own.
  - **Drafted** 2026-11-22, 19:45, with the owner's direction of that night (split by physics; the powder's part shared; the scale's and the printed parts' part on the unit, not configurable, backed up and restored; the glide kept beside a PID): [controller_design_draft.md](controller_design_draft.md). It covers one frame with two speed laws, the four records, each parameter's life cycle, the faults, tuning by hand with the auto-tuner, the timings, the servo gate (issue #135), the migration and the stored form, and ends with the owner's eight decisions. Not approved.

## Not in this ADR

- Ladder loading and other new modes.
- Profiles from an online library.

## To settle before approval

Settled since: what "better" is (the two bands, 2026-11-21), the order (ADR-008 closed first), the rig the proof is made on (the owner, 2026-11-21, 17:29). Left for the owner, with the plan under "What we do":
- **ADR-012's check against the firmware before it** ("Handed on by ADR-012"): proposed retired. The bench trial ran on ADR-012's motors, and today's loop puts 85% of AR2209's throws over on either firmware, so the comparison would say nothing of the motors; gate 6's side by side is against today's loop on the same firmware.
- **PR #113** (ADR-002 left its end here): its tuner tunes the two PID loops, which stay only as the fallback until gate 6, so nothing of it is taken. Proposed: closed with thanks and a note naming what this ADR built, at the owner's word, when this ADR's PR is opened.
