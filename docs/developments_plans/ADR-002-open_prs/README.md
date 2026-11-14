# ADR-002: the open PRs, a verdict for each

Status: **Draft** 2026-11-13. Not approved. The reviews need no bench; anything merged needs ADR-001's bench.

## What we want

No PR left open without a decision, and nothing merged that the owner has not looked at with the agent.

## Why here

The verdicts cost reading and talking, not a bench, and they decide what ADR-004 and ADR-009 contain. A PR that is merged as it stands goes in here, while it still merges.

It follows ADR-011 and ADR-012, so what is merged lands on the updated dependencies and the cleaned motor code. Of the open PRs, #111, #113, #44 and #28 touch the build files ADR-011 changes; #101 (one line of `stepper.pio`) and #28 touch the motor files.

## Owner's direction (2026-11-13)

- The dependency update and the code cleanup, the motor drivers named, come before the changes of PRs are merged (2026-11-14).
- Not every PR is merged as it stands. Many of them do not make sense, or are not organised the right way. Each is discussed with the owner at merging.
- Community PRs land before the restructure (ADR-006). For a PR that is rebuilt and not merged, see "To settle".
- Auto tare is dropped.

## What we know (checked 2026-11-13)

None of the PRs has been read in full. This is from each one's description, file list and comments.

| PR | What | Author, date | State | First look |
|---|---|---|---|---|
| #109 | The last charge weight is remembered per profile | NemesisXB, 2026-04 | Merges cleanly; 287 lines in 7 files | Changes the stored profile struct (`profile.c`, `profile.h`), so saved profiles must be shown to survive |
| #46 | Auto tare | owner, 2024-04 | Conflicts; 33 lines | Dropped by the owner. Issue #45 asked for it |
| #28 | OTA with the picowota bootloader | dirtbit, 2023-10 | Conflicts; 32 files | The bootloader carries its own Wi-Fi stack (about 400 KB) and a serial flashing tool. The owner asked in 2023 for the web interface in place of a command-line tool |
| #44 | A bootloader with two banks | owner, 2024-04 | Conflicts | Linker scripts for two banks and a third boot stage; two years behind main |
| #113 | Powder analysis: automatic tuning of a profile from the screen and a web page | DevDaniel1408, 2026-10 | Merges cleanly; 8,374 lines in 64 files | One 2,326-line file beside charge mode, a 750-line tuner, its own 928-line page. Brings tests that run on a PC, a simulated powder, manuals in English and German, and a fix for the D term dividing by 0. Its author says: thresholds are global and not per profile, fine Kd is not tuned, only stick powders were measured, only an A&D scale was tried |
| #101 | "AI tuning", a port from Jump73's fork | opened by the owner as a comparison, 2026-03 | Conflicts; 5,650 lines in 36 files | Tuning (880 lines and 606 of REST) mixed with an Ohaus scale driver, a G&G scale change, error pages, flash storage, a display config and a 1,307-line icon header. A commenter asks that it not be called AI |
| #111 | OTA: a small bootloader, a staging area, upload from the web | tomanyusers, 2026-07 | Conflicts; 1,719 lines in 24 files | Works on its author's Pico W. Its Pico 2 W build fails in CI. Adds a `CLAUDE.md`. The owner asked on the PR whether Pico W has the room and whether dropping it is acceptable |

- No CI run is shown for #109 and #113. Until 2026-11-13 a run would have failed whatever the PR held: CI's runner had moved to an image whose compiler cannot build the SDK's tools (fixed by PR #114).
- Merged since April, and missed by this ADR's first draft, which read an old checkout: #107 (a simulated scale), #108 (the coarse trickler's back-off) and #110 (the back button), all by #109's author, and #112 (REST values that contain `&`).
- Six branches on the remote hold commits that are not on main: `add_auto_tare`, `add_bootloader`, `codex/sub-pr-86`, `fix_pio_issue`, `pcb_design`, `pr_94_review`.

## What we will do

1. **One page per PR**, in this folder (`pr_109.md` and so on), after reading it whole:
   - what it does, in plain words;
   - what is sound and worth keeping;
   - what is in the wrong place or built the wrong way, and why;
   - what it costs in flash and RAM (ADR-001's size report on its branch);
   - whether it changes a stored struct;
   - a proposed verdict.
2. **The owner decides**, one PR at a time, in conversation. The verdict is one of:
   - *merge*: as it stands;
   - *merge after changes*: named changes first, by its author or by us on top of its branch;
   - *rebuild*: the idea is kept and built in a later ADR, with its author credited in the commits;
   - *close*.
3. **Order of the reviews:** #109 first (small), then #111, then #113 and #101 side by side.
4. **Then act on each verdict:**
   - close #46 and issue #45;
   - close what is superseded or turned down, with a note of thanks that says where the idea goes. The owner sees each note before it is posted;
   - merge what is to be merged, proven on the unit;
   - write each rebuilt idea into the ADR that will build it.

## How the unit shows it works

For anything merged: `charge_cycle` and `menu_tour` pass as before, and settings saved by v1.20 read back after the update.

## Not in this ADR

- Rebuilding anything. A rebuild belongs to the ADR of its subject: tuning in ADR-009, OTA in ADR-004.

## To settle before approval

- **Where a rebuilt PR lands.** "Before the restructure" was chosen when merging as-is was the plan. For a PR that is rebuilt, the proposal is the ADR where its subject lives, even when that is after ADR-006.
- **The six branches** that are not on main: delete them once their PRs are decided, or keep them.
