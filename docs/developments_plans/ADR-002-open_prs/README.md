# ADR-002: the open PRs, a decision for each

Status: **Built** 2026-11-15. Approved by the owner that day as written ("Approve as written"), after it was rewritten to the owner's policy of that day; drafted 2026-11-13. What was done and measured is in [progress.md](progress.md). The approved notes are posted right after this ADR's PR is merged: see "At the merge".

## What we want

No PR left open without a decision, and nothing in main that the roadmap would not have built.

## Why here

The decisions cost reading and talking, and they say what ADR-004 and ADR-009 start from.

It follows ADR-011 and ADR-012, so what is taken lands on the updated dependencies and the cleaned motor code.

## Owner's direction

2026-11-13 and 11-14:
- The dependency update and the code cleanup, the motor drivers named, come before any change from a PR.
- Not every PR is merged as it stands. Many of them do not make sense, or are not organised the right way. Each is discussed with the owner.
- Auto tare is dropped.

2026-11-15, on the strategy below as first proposed:
- **The policy:** "we take good stuff, merge, but discard the part that doesn't fit the road map." "We don't need to merge all PRs as is, and I'd stick to our roadmap, merge as they seems fit, but I'd keep your judgement independently from the influence of PRs."
- **The order of the ADRs stays.** ADR-009 is not brought forward for PR #113.
- **#109:** the owner's own idea was a charge profile beside the powder profile: "two caliber can share the same powder, but charge as different weight." It was not built, for its complexity. So the owner is "not sure if I want to merge #109, or at least the way it was built."
- **#113:** a first contribution, to be encouraged. But it overlaps ADR-009 a lot, and much of its work is in u8g2 screens that ADR-010 replaces.
- **#111:** "we should take serious look into the OTA feature." The owner's first word was to merge it and discard much of it later; see "#111" below for why this ADR proposes otherwise.
- **The owner's own PRs and the tuning port** (#44, #46, #101) are replaced by the roadmap.

An earlier direction, "community PRs land before the restructure (ADR-006)", was given when merging as-is was the plan. Under the policy above a PR ends in the ADR of its subject, and #113's is after ADR-006.

## What we know

**The PRs** (checked 2026-11-13 from each one's description, file list and comments; none has changed since, checked 2026-11-15)

| PR | What | Author, date | Size |
|---|---|---|---|
| #109 | The last charge weight is remembered per profile | NemesisXB, 2026-04 | 287 lines in 7 files, one commit |
| #113 | Powder analysis: automatic tuning of a profile from the screen and a web page | DevDaniel1408, 2026-10 | 8,374 lines in 64 files, two commits |
| #111 | OTA: a small bootloader, a staging area, upload from the web | tomanyusers, 2026-07 | 1,719 lines in 24 files |
| #101 | "AI tuning", a port from Jump73's fork | opened by the owner as a comparison, 2026-03 | 5,650 lines in 36 files |
| #44 | A bootloader with two banks | owner, 2024-04 | 1,588 lines in 14 files |
| #46 | Auto tare. Issue #45 (Memphis003) asked for it | owner, 2024-04 | 33 lines |
| #28 | OTA with the picowota bootloader | dirtbit, 2023-10 | 32 files, from its author's `main` |

- #113 has no comment from anyone. #111's last is its author's, 2026-07-14: it works on a Pico W, a Pico 2 W is ordered, and why CI's Pico 2 W build fails is not looked into. #109 has none.
- All three community PRs (#109, #111, #113) let maintainers push to their branches.
- Merged since April, by #109's author: #107 (a simulated scale), #108 (the coarse trickler's back-off) and #110 (the back button).
- Branches on the remote: five hold commits that are not on main (`add_auto_tare`, `add_bootloader`, `codex/sub-pr-86`, `fix_pio_issue`, `pr_94_review`). Seven hold nothing that is not on main.

**Trial merges into main at 1e50b28** (2026-11-15, `git merge-tree`)
- #109 and #113 merge without a conflict. That says the text merges, no more: neither merged tree was built.
- #111 conflicts in `.gitignore`, `configure_env.ps1`, `charge_mode.cpp` and `menu.c`. #101 conflicts in ten files, #46 in two, #44 in five. #28 was not tried.

**#109, read 2026-11-15** (its diff, but for the web page)
- It adds `last_charge_weight` to the powder profile (`profile_t`), raises the stored profiles' revision from 1 to 2, and carries a migration of the old layout.
- So a powder has one weight. Two calibres on one powder need two copies of the powder's profile, and what is tuned on one is not tuned on the other.
- The whole block of profiles is written to the EEPROM at each "Next" on the weight form, and at each weight set over the web.
- It also changes the charge state's JSON (`s1` becomes a string, and `nan` and `inf` lose their quotes) and makes that reply's static buffer 512 bytes from 160.
- Its migration tells the old revision from the stored revision field and a CRC, and allocates its buffer at start-up.

**#113, looked at 2026-11-15** (its file list, its includes and its charge loop; not read whole)
- **Its charge loop is a copy of charge mode's**, in `powder_analysis.cpp` (2,326 lines, with its screens and its REST paths). Its comment: "Copy of the control loop of charge_mode_wait_for_complete() from charge_mode.cpp, keep in sync."
- **The copy has none of what ADR-013 and ADR-012 put into charge mode's loop.** With no reading in 200 ms it waits again, the motors at their last speed, until its own limit of 30 s for a trial stops them; charge mode pauses after three of the scale's usual intervals. It never asks whether a motor driver reports a fault. What sits below the loop still holds for it: a frame that does not parse is dropped (`scale.c`), and a speed that is not a number is refused (`motors.c`).
- **Three parts have no hardware in them** (they include only the C library and each other), about 2,700 lines:
  - the tuner: `powder_analysis_tuner.cpp` and `.h`, `powder_analysis_config.h` (1,220 lines). It hands out the next trial's settings and takes the trial's result;
  - its tests, which run on a PC: `tests/powder_analysis/test_powder_analysis_tuner.cpp` (1,318 lines), with a Makefile;
  - a simulated powder for those tests (168 lines).
- **Nothing but the 2,326-line file calls the tuner.** Taken alone, it is code the firmware never runs.
- The rest is the web page (928 lines, on today's server), tools that draw the u8g2 screens on a PC, three manuals and their pictures.
- Its second commit is "Remove concept files created by the ai".
- Its author says: thresholds are global and not per profile, fine Kd is not tuned, only stick powders were measured, only an A&D scale was tried.

**The D-term fault that #113 fixes** (read in the code 2026-11-15; not yet seen on a unit)
- `charge_mode.cpp` divides each trickler's change of error by the milliseconds since the last reading (both derivative lines). #113's author reports that a reading is often already waiting on the loop's first pass, so that time is 0 ms.
- The result is infinite, or not a number. The line after it holds the speed between the profile's minimum and maximum, and that turns either into the maximum. So the motors' refusal of a speed that is not a number (ADR-012) never sees it.
- What that costs at a throw's start, with the default profile and a 30 gr target: the coarse trickler is asked for 5 rev/s where its P term asks for 0.625 (0.025 × 25 gr), until the next reading. The fine trickler's P term is at its maximum there anyway.
- #113's fix is 18 lines in `charge_mode.cpp` and stands alone: no D term on a throw's first reading, nor when no time has passed.
- **Seen on the unit since** (2026-11-15, [progress](progress.md)): the fault is there in every throw, but smaller than this reading of the code said. The time was never 0 ms; the first reading's D term gave the coarse trickler up to 1.875 rev/s, not its 5.

**#111 and the flash**
- It brings a new flash layout. A unit gets a new layout only by cable; every later change to the layout costs every user another update by cable.
- Its Pico 2 W build fails in CI. Its upload is built on the file server ADR-003 replaces. It adds a `CLAUDE.md`.
- The owner asked on the PR whether the Pico W has the room, and whether dropping it is acceptable.

**#101, #44, #28** (from their descriptions)
- #101 mixes tuning (880 lines and 606 of REST) with an Ohaus scale driver, a G&G scale change, error pages, flash storage, a display config and a 1,307-line icon header. A commenter asks that it not be called AI.
- #44 is linker scripts for two banks and a third boot stage, two years behind main.
- #28's bootloader carries its own Wi-Fi stack (about 400 KB) and a serial flashing tool. The owner asked in 2023 for the web interface in place of a command-line tool.

## The strategy

**The rule.** An ADR's design is settled on its own, not from what a PR happens to hold. Then a PR ends one of three ways:

1. **The design keeps most of it.** The ADR's branch starts from the PR's head, and our commits go on top. When the ADR is merged the author's commits are in main, and GitHub shows the PR as merged.
2. **The design keeps parts of it.** Those are taken in the ADR's branch, in commits that carry a `Co-authored-by:` line for the author. The PR is closed with a note that names what was kept.
3. **The design keeps nothing.** The PR is closed with a note of thanks that says why.

**Two tests for anything taken, at any time:**
- **It is alive in main:** the firmware calls it, or CI runs it. Code that nothing calls is not merged to be kept for later.
- **One loop moves the tricklers.** What ADR-013 and ADR-012 guard, they guard in charge mode's loop. A second loop is not merged.

**What goes into main now, in this ADR:** only a part that stands alone and fixes something users have today. There is one: #113's D-term fix.

**Applied**

| PR | Now, in this ADR | Then |
|---|---|---|
| #109 | Closed, with a note | The charge profile, the owner's idea, which no ADR holds yet (see "To settle") |
| #113 | Its D-term fix is taken, credited. The PR stays open, with a comment that says what happens to it and when | ADR-009 designs its controller first, then judges the tuner, its tests and the simulated powder against that design, and ends the PR by the rule. Its loop, screens and page are not taken: ADR-009, ADR-010 and ADR-008 build those |
| #111 | Nothing. The PR stays open, with a comment | ADR-004 settles its layout first, then ends the PR by the rule. ADR-004 is two ADRs away |
| #101 | Closed, with a note (by the owner, 2026-11-15) | Its tuning ideas are listed in ADR-009 |
| #44, #28 | Closed, with a note for #28 | ADR-004 |
| #46 | Closed (by the owner, 2026-11-15), with issue #45 | Dropped |

**#109.** Not merged. Its weight is in the wrong table for the owner's idea, and it would cost users a change of the stored profiles now and a second one when charge profiles come. Its author has three PRs in main; the note says where the idea goes.

**#113.** Its good part cannot be merged alive today: the tuner's only caller is the file with the second loop. And in main it would pull ADR-009's design toward it, which is the influence the owner wants kept out. So the tuner waits for the design it would serve. The PR stays open, since a closed PR reads as a refusal to a first contributor, and its closing condition is ADR-009's design.

**#111.** Not merged before ADR-004. Code in the tree can be discarded later; a layout on users' units cannot. "Merge now, discard later" would cost every user one update by cable for each time the layout moves. So the layout is decided once, in ADR-004, and the PR ends there by the rule.

**What it costs.** #113 stays open until ADR-009, which is seven ADRs away, and tuning reaches users no sooner than that. The other way was looked at: the tuner and its tests merged now, with a CI job to run them. Its author would see "Merged" today, ADR-009 would inherit an interface it did not choose, and taking a contributor's merged work out again says worse than waiting does.

## What we will do

1. **The D-term fix,** in a branch of this ADR:
   - shown first on the unit as it is today: a charge with the simulated scale, and the speed the coarse trickler is given at the throw's first reading. If the unit does not show the fault, the fix is not taken here and goes to ADR-009 as a finding;
   - then #113's 18 lines as one commit with a `Co-authored-by:` line for its author, and the same charge again;
   - then the usual proof, on one unit (see "How the unit shows it works").
2. **The notes and comments,** each shown to the owner before it is posted:
   - #113: a comment. What is taken now, what ADR-009 will judge, and why the loop, the screens and the page are not taken;
   - #111: a comment. That ADR-004 is where it is decided, and the owner's open question on the Pico W;
   - #109, #101, #28: a closing note of thanks that says where the idea goes;
   - #44 and #46 are the owner's own and need none. Issue #45 gets one line.
3. **What each later ADR starts from,** written into it under "Handed on by ADR-002":
   - ADR-009: #113's three parts with no hardware in them, by file, as candidates; what its author says is missing; #101's tuning ideas; and that #113 is open and ends there;
   - ADR-004: that #111 is open and ends there; that #44 and #28 are closed.
4. **Close** #109, #101, #44, #46, #28 and issue #45, on the owner's word for each.
5. **The branches,** on the owner's word: the seven with nothing of their own are deleted; `add_auto_tare` and `add_bootloader` go with their PRs; `codex/sub-pr-86`, `fix_pio_issue` and `pr_94_review` are looked at first.

No stored struct changes in this ADR.

## How the unit shows it works

For the D-term fix, on one unit (the owner, 2026-11-15, after the Pico 2 W passed: "We don't need to repeat on Pico W for application layer changes"):
- the coarse trickler's first speed in a throw is what its P term asks for, where before it was the maximum;
- `menu_tour --compare` against the unit's baseline and `charge_cycle` end `VERDICT PASS`.

Nothing else in this ADR reaches a unit.

## Not in this ADR

- Designing anything. A PR's subject is designed in its own ADR: tuning in ADR-009, OTA in ADR-004.
- Reading #113 and #111 whole. Their ADRs do that when their turn comes, against their own design.
- The charge profile itself.

## At the merge

The owner, 2026-11-15: one PR for the whole of this ADR, and the notes posted right after it is merged. Two of them say what is true only then: #113's, that its fix goes into main, and #109's, which points to ADR-014.

Right after the merge, from [notes.md](notes.md) as the owner approved it:
- the comments on #113 and #111, which stay open;
- #109 and #28 closed, each with its note;
- issue #45 closed with its line.

GitHub holds what was posted and when. Nothing more is written here for it.

## Settled at approval (2026-11-15)

- **The charge profile** gets a short Draft ADR of its own, ADR-014, that holds the idea as the owner gave it, so #109's note has something to point to. It sits after ADR-006, which changes how settings are stored, so that profiles are migrated once.
- **The D-term fix is taken here,** not left to ADR-009: it is part of a PR, users have the fault today, and ADR-009 is far.
