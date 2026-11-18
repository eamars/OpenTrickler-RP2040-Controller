# Handover, 2026-11-18

First written at the end of a session that stopped early; brought up to date 2026-11-14 when ADR-001 closed, 2026-11-15 when ADR-011 and ADR-013 closed, 2026-11-16 when ADR-003 closed, and again that day when it was merged and ADR-004 was written for approval, that evening when the Pico W ran ADR-004's gates, when ADR-004 closed, 2026-11-17 when it was merged, and later that day when the owner's new order of work and ADR-010's menu design were written in, and 2026-11-18 when ADR-010 closed. It says where the work stands and what comes next. Delete this file once its to-do list is done or has moved into an ADR; it is not part of the registry.

## Where things stand

- **ADR-001:** Built and closed 2026-11-14, merged to main by PR #115. The owner waived the real throw and the check by eye (see its progress.md).
- **ADR-011:** Built and closed 2026-11-15: SDK 2.3.1, FreeRTOS V11.3.3 from upstream, Arm's 15.2.Rel1, every version in `toolchain.env`, CI on `ubuntu-latest` with no pin. Proven on the Pico W and the Pico 2 W. The owner waived the end checks by hand and accepted the Pico 2 W's web soak as it stood; ADR-003 carries that finding. Merged by PR #119.
- **ADR-013:** Built and closed 2026-11-15: one door for a reading (a bad frame is dropped and counted), a missing reading pauses the tricklers and a lost scale ends the charge, by the scale's own rhythm, and the web's cleanup speed only in cleanup mode. Proven on both units. A bad frame did opposite wrong things on the two chips before it: full speed on the RP2350, a short throw on the RP2040. Reviewed and merged by the owner, PR #124.
- **ADR-012:** Built and closed 2026-11-15, in its branch `adr012_motor_drivers`, pushed for its PR: our own TMC2209 driver on Klipper's model, step pulses that stop when they are not fed, one motor task, the drivers read while running, the audit's small fixes, and a driver's fault shown before a mode is entered (step 6, the owner's "fail early"). Proven on both units. Handed on: the Pico W's web cost from step 5 (20 values a request, 32 bytes more of lwIP's heap a connection) and its measured fix to ADR-003, accepted by the owner; the real throws to ADR-009, its first recorded throws (the owner). Its fault 7 very likely caused ADR-011's two silent starts; the old firmware stopped at its start once more on 2026-11-15.
- **ADR-002:** Built and closed 2026-11-15, in its branch `adr002_open_prs`, in one PR. A PR ends in the ADR of its subject, after that ADR's own design. PR #113's D-term fix is in, shown on the Pico 2 W first; #113 and #111 stay open for ADR-009 and ADR-004; the others are closed; the charge profile, the owner's idea, is Draft ADR-014. Its approved notes are posted right after its PR is merged (its README, "At the merge").
- **ADR-004:** Built and closed 2026-11-16, reviewed and merged by the owner the same day, PR #130: two slots on both chips, the update through the API, releases on GitHub as pre-releases (v2.0.0 to v2.0.2). Its progress.md has every figure, and its "Closed" what the owner waived or changed.
- **ADR-003:** Built and closed 2026-11-16. Gate 1 merged by #128; gates 2 to 4 reviewed and merged by the owner the same day, PR #129, with the review's one change: `GET /api/v1/settings/motors`, and the rule that what a request reads or writes is named in its path. Our own server on lwIP's sockets in place of the copy of lwIP's file server, and API v1, stable from now on, which the unit describes itself (`GET /api/v1`, `/api/v1/schema`): each setting described once, one entry point for each command, one event log, uploads in pieces, the pages on v1, the old `/rest/` paths gone. Proven on both units, reviewed by a second model, its fixes proven on the Pico 2 W. The client `tools/remote_control/unit_api.py`, `rc_run.py api_check`, and the stand-in for page work `tests/web_test.py` came with it. Its progress.md has every figure.
- **ADR-010:** Built and closed 2026-11-18, merged by #132 and #133: the menu on LVGL designed again (u8g2 gone), the modes in a job task, every setting a unit needs without the web on the screen too, the update's poll, offer and download at the screen, and the bench host asked first (`opentrickler-update.local`, announced by `tools/ota_server.py`). Proven on the Pico W. Two checks not done, each with its occasion in AGENTS.md: the Pico 2 W's run on the probe (its menu baseline) and the servo seen at a shutter's end once a gate is fitted. Its progress.md has every figure.
- **Releases:** v2.0.3, the fourth candidate (ADR-010's screen, the update from GitHub), drafted by CI from its tag 2026-11-18 for the owner to publish as a pre-release and try over the air from GitHub. A unit offered it must be on the beta channel. No OTA server may run meanwhile: it is the bench host and takes over every unit's poll.
- **CI:** the build workflow is the only check. It fetches its toolchain from `toolchain.env` and does not depend on the runner's compiler.
- **Order of the work, as the registry lists it:** 001, 011, 013, 012, 002, 003, 004, 005, then 010, 008, 009, 014, and 006 and 007 together (the owner, 2026-11-17: the current Pico platform before the platform's expansion). ADR-015, commissioning, was added the same day, and folded into ADR-008 on 2026-11-18. An ADR's number is its name and does not change.
- **Hardware:** two units, one on the probe at a time (see `AGENTS.md`, "The bench"):
  - the Pico 2 W is off the probe. It runs 2.0.2-64 (ADR-010 before its review's fixes) in slot B, updated over Wi-Fi from this PC 2026-11-18 and confirmed; its update source GitHub's, its channel stable. It is on a controller board and settings of its own; its web API at 192.168.2.166, by DHCP, also `opentrickler-4993.local`. Its settings before that update: `logs/settings/pico2w_before_2.0.2-64.json`. Copies of its settings: `logs/adr003/pico2w/before_adr003/` (the old paths, at 1.20.146), `after_flash_v1/` (API v1), `logs/adr004/pico2w/before_review/` and `before_o2/` (at 1.20.212);
  - the Pico W is on the probe. It runs ADR-010's last build (step 9), flashed by the probe; its bootloader at the start of flash; its update source this PC's server (`http://192.168.2.13:8080/a/b`), its channel beta, Invert screen on; its web API at 192.168.2.168, by DHCP (it was .162 before). Copies of its settings: `logs/adr003/picow/before_gate1/`, `before_gate3_v1/`, `before_close/`, and `logs/adr004/picow/before_adr004/` (at 1.20.194, before the new layout).
- **macOS is parked**, by the owner: development on Windows, integration builds on Linux.

## Conclusions of today

1. **The `pioasm` fault is CI's only.** This PC uses the ready-made `pioasm.exe`; CI compiles it with the runner's own compiler. Pico SDK 2.2.0 and later carry the fix.
2. **The cleanup is two ADRs, not one.** The process keeps a dependency update apart from a functional change.
3. **"After ADR-001" is right.** Both ADRs need ADR-001's tools for their proof, and the open PRs barely touch the files they change.
4. **The motor code has 14 faults read in the code** (ADR-012). The four that matter most:
   - the speed queue is made for 12-byte items and used with 4-byte floats;
   - a ramp is a loop that never yields, and the charge loop waits behind it;
   - about one restart in 37 should end at a driver error if the drivers stay powered;
   - the stored motor current may only be true with the driver module's trimmer at full scale.
5. **The owner wants the cleanup to cover more than the motors.** Which modules is not settled.

## The audit of the other modules

An audit of the rest of `src/` was run to find, for every other module, the kind of faults ADR-012 lists for the motors. It finished. Every file was read whole, except `http_rest.c`, which was read as a diff against lwIP's own server.

- Its report is in `logs/audit_2026-11-14_unverified.md`. That folder is not tracked by git, so the report exists only on this PC.
- **Its top ten, plus S2, W1 and W3, were checked by a second reading** on 2026-11-14: `logs/audit_2026-11-14_verified.md`, also this PC only. The rest is still unchecked: read the line a finding points to before citing it.
- What it ranks first (confirmed):
  - a scale frame that does not parse is published as NaN, and one NaN puts both tricklers at maximum speed for the rest of the throw;
  - a scale that goes silent during a charge leaves the motors at their last speed;
  - the web page's cleanup speed is applied outside cleanup mode;
  - on main since PR #112, a long web form is cut at 127 characters, so settings are saved wrong or not at all.
- Its verdicts: the scale drivers and the modes each need a cleanup of their own; storage, the servo gate, Wi-Fi and the system files need a few fixes; the screens can wait for ADR-010.
- **For the scale drivers' cleanup, seen on the bench in ADR-013** (2026-11-15): the A&D driver takes 17 bytes as a frame, not what ends in its CR LF. A frame stitched across a hand-over from the simulated scale was seen (`SST,+00000.00 GN\r`, 1 in 30). ADR-013's door drops one that does not parse; one that parses as a wrong number it cannot see. Take a frame only at its line ending, and look at the other drivers for the same. See ADR-013's progress.
- **For the modes' cleanup, `FloatRingBuffer`** (read in the code 2026-11-15, owner's question in ADR-013's review). Its faults are latent: no caller meets them today.
  - `getSum()`, `getMean()` and `getSd()` run over every slot, not the readings it holds, so they read memory `malloc` never set until the buffer is full. The three that use one wait for a full buffer first; a fourth, in the cup-return wait (`charge_mode.cpp`), is made and never used.
  - `last()` gives the slot to be written next, not the newest reading. Nothing calls it.
  - Its lock is a plain bool and locks nothing; its `malloc` is not checked.
  - It is C++, and it has no median. So the scale's rhythm in `scale.c` (ADR-013) keeps its own window of 16 intervals and takes their median: a mean would be moved by the gaps it is there to find. Make one ring buffer in C, its sums over what it holds, with a median, and move the rhythm onto it.
- `charge_mode.cpp` is the one file every live PR touches.

## To do next, in order

1. ~~`git fetch`, and compare with `origin/main`.~~ Done 2026-11-14: the branch holds all of origin/main; the local `main` was moved up to it.
2. ~~Ask the owner whether the bench is ready.~~ Done 2026-11-14: ready, SWD and motors included, hopper empty. The bench is a Pico W reached by a debug probe on SWD only, with an A&D FX-300i; see ADR-001's "The bench". ADR-001 is revised for it (F5), approved the same day.
3. ~~Check the audit's findings against the code, its top ten first.~~ Done 2026-11-14 for the top ten plus S2, W1, W3: `logs/audit_2026-11-14_verified.md` (this PC only). 14 confirmed, M5 only partly (ordinary readings fit; the overruns need a lifted tared container, huge readings or a REST target). H2's cause is PR #108, not #112. The audit's other findings are still unchecked.
4. ~~Propose the cleanup's scope to the owner.~~ Done 2026-11-15: the owner gave the scope to the agent, with no ADR of its own for a small cleanup. It is in ADR-012: six small fixes as its step 5, and every other finding of the audit given to an ADR that exists ("Where the rest of the audit goes"). `charge_mode.cpp` is left alone but for two hooks, and #109 and #113 are test-merged on this PC before ADR-012 closes.
5. ~~Before ADR-012 closes, write each handed-on finding into the ADR named for it.~~ Done 2026-11-15: ADR-003, 004, 005, 006, 009 and 010 each have a "Handed on by ADR-012".

## Waiting on the owner

- **PR #111 stays open** (the owner, 2026-11-17; the screen's half built by ADR-010, the web's is ADR-008's) until the web and screen updates are in place and the whole OTA flow is shown end to end, from the offer to the unit confirmed (ADR-008, ADR-010). Then it is closed with thanks and a note naming what was taken, its RP2040 jump (credited in the bootloader's commit 91b9514), on the owner's word.
- **Found in ADR-003 and handed on**, each to decide where it goes:
  - ~~a tuning session's load and an upload at once overrun the Pico W's network~~: ADR-004 makes the update a job, so the two never run together;
  - the gate's move queue lets a write replace a move a mode waits for (`servo_gate.c`);
  - the EEPROM's erase runs outside its mutex, and the API's erase runs it beside the server;
  - `rnd()` (`eeprom.c`) gives ids with few random bits: the bench's is `9CE00000`, and the host name shows four characters of it.
- **ADR-005:** Decided and closed as a scoping ADR 2026-11-17 (the owner): its optimised build waits for its trigger, an image that no longer fits its slot; its feature table went to ADR-008. Measured that day: the Pico W image 799 KB, 583 KB at `-Os`; RAM 224 of 256 KB at link time.
- **The open ADR:** 008, the web UI, approved 2026-11-18 with its requirements aligned, revised the same day with the agent's review of the plan against the code (the feature table in gate 2 with "no feature silently dropped", the styles' tools in gate 1 and open to a proposal, the radio probed on the Pico W from the workstation's Wi-Fi, the weight off the event log, the access point's key, DHCP's option 114, the pages cached by version, the phone kept awake behind a switch on by default), and Open. Gate 1, the mock, settled by the owner 2026-11-19 (every left-out row decided; the phones' checks carried to gate 4's start, progress.md); gate 2, the API review, approved, built and proven on the Pico W 2026-11-19; gate 3, the pages, started overnight on the owner's word (steps 1 to 5 built and proven on the Pico W; its plan and what is left in progress.md). The whole ADR goes by one PR, #134, a draft until the ADR closes (the owner). Commissioning (ADR-015) is folded into it (the owner, 2026-11-18): the captive portal route with the main portal, its proposals and what to find out on real phones in its "Commissioning". Its "What we know" was brought up to date the same day. ADR-010 left ADR-008 the page's offer and its upload by hand, the page that sets the update's channel and source, and its settings pages drawn from the schema (ADR-008's "Handed on").

- **ADR numbers:** kept fixed, with the table in the order of work. The owner can ask for renumbering; it is cheapest now.
- ~~**ADR-012, fault 7:** whether its one-line fix goes in early.~~ ADR-012 is next and its step 1 comes first: the new transport checks every write, and the fault goes with the old code.
- **Earlier questions still open:**
  - whether anything but the built-in pages calls the REST API;
  - the tier rules 2 to 5 (ADR-005);
  - ~~the PR verdicts (ADR-002), with closing PR #46 and issue #45.~~ ADR-002 was approved 2026-11-15: what happens to each PR is in it, and each close and each note waits for the owner's word.
