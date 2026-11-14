# Handover, 2026-11-14

Written at the end of a session that stopped early. It says where the work stands and what comes next. Delete this file once its to-do list is done or has moved into an ADR; it is not part of the registry.

## Where things stand

- **Branch:** `adr001_agent_groundwork`, pushed. Main's CI is green with the runner pinned (PR #114).
- **ADR-001:** open. Steps 1 and 2 are built. The bench is ready (2026-11-14); the revision for the probe bench (F5) was approved; steps 3, 4 and 5 are built, step 3 except the owner's check by hand. Step 6 (the owner's check, the settings set back, a real throw, closing) is left.
- **New today, both drafts, not approved:** ADR-011 (the dependencies brought up to date) and ADR-012 (the motor drivers cleaned up).
- **Order of the work:** 001, 011, 012, 002, 003, 004, 005, 006, 007, 008, 009, 010. An ADR's number is its name and does not change.
- **Hardware:** on 2026-11-14 the unit's settings were copied to `logs/unit_backup/`, then it was flashed with this branch through the probe (step 3). Its charge-mode settings are at their defaults now (F6).

## Conclusions of today

1. **The `pioasm` fault is CI's only.** This PC uses the ready-made `pioasm.exe`; CI compiles it with the runner's own compiler. Pico SDK 2.2.0 and later carry the fix.
2. **The cleanup is two ADRs, not one.** The process keeps a dependency update apart from a functional change.
3. **"After ADR-001" is right.** Both ADRs need ADR-001's tools for their proof, and the open PRs barely touch the files they change.
4. **The motor code has 14 faults read in the code** (ADR-012). The four that matter most:
   - the speed queue is made for 12-byte items and used with 4-byte floats;
   - a ramp is a loop that never yields, and the charge loop waits behind it;
   - about one restart in 32 should end at a driver error if the drivers stay powered;
   - the stored motor current may only be true with the driver module's trimmer at full scale.
5. **The owner wants the cleanup to cover more than the motors.** Which modules is not settled.

## The audit of the other modules

An audit of the rest of `src/` was run to find, for every other module, the kind of faults ADR-012 lists for the motors. It finished. Every file was read whole, except `http_rest.c`, which was read as a diff against lwIP's own server.

- Its report is in `logs/audit_2026-11-14_unverified.md`. That folder is not tracked by git, so the report exists only on this PC.
- **Nothing in it has been checked by a second reading.** Do not cite a finding from it before reading the line it points to.
- What it ranks first, all unchecked:
  - a scale frame that does not parse is published as NaN, and one NaN puts both tricklers at maximum speed for the rest of the throw;
  - a scale that goes silent during a charge leaves the motors at their last speed;
  - the web page's cleanup speed is applied outside cleanup mode;
  - on main since PR #112, a long web form is cut at 127 characters, so settings are saved wrong or not at all.
- Its verdicts: the scale drivers and the modes each need a cleanup of their own; storage, the servo gate, Wi-Fi and the system files need a few fixes; the screens can wait for ADR-010.
- `charge_mode.cpp` is the one file every live PR touches.

## To do next, in order

1. ~~`git fetch`, and compare with `origin/main`.~~ Done 2026-11-14: the branch holds all of origin/main; the local `main` was moved up to it.
2. ~~Ask the owner whether the bench is ready.~~ Done 2026-11-14: ready, SWD and motors included, hopper empty. The bench is a Pico W reached by a debug probe on SWD only, with an A&D FX-300i; see ADR-001's "The bench". ADR-001 is revised for it (F5), approved the same day.
3. ~~Check the audit's findings against the code, its top ten first.~~ Done 2026-11-14 for the top ten plus S2, W1, W3: `logs/audit_2026-11-14_verified.md` (this PC only). 14 confirmed, M5 only partly (ordinary readings fit; the overruns need a lifted tared container, huge readings or a REST target). H2's cause is PR #108, not #112. The audit's other findings are still unchecked.
4. Propose the cleanup's scope to the owner: which modules, in which ADR, each with a fixed list of faults so it has an end. Say what it costs the waiting PRs: #109 and #113 merge cleanly today, and a cleanup that touches their files ends that.
5. Write the agreed scope into the ADRs and the registry.

## Waiting on the owner

- **The cleanup's scope**, once step 4 above is ready.
- **ADR numbers:** kept fixed, with the table in the order of work. He can ask for renumbering; it is cheapest now.
- **ADR-012, fault 8:** the motor current's reference. Units as they are today, or the stored mA made true.
- **ADR-012, fault 7:** whether its one-line fix goes in early, since ADR-001's bench restarts a unit without cutting power.
- **ADR-011:** SDK 2.3.1 or 2.2.0; whether CI moves to Arm's compiler; the 2023 debug scripts.
- **ADR-001:**
  - the owner's check by hand for step 3 (the probe's USB out, the unit starts, a charge by hand);
  - whether steps 1 and 2 go to main now.
- **Earlier questions still open:**
  - whether anything but the built-in pages calls the REST API;
  - the tier rules 2 to 5 (ADR-005);
  - the PR verdicts (ADR-002), with closing PR #46 and issue #45.
