# ADR-002: progress

What was done and measured. The plan is in the [README](README.md). Logs are kept on this PC under `logs/rc/`, which git does not track.

## Where each step stands

| Step | State | Waits for |
|---|---|---|
| 1. The D-term fix | Done 2026-11-15: shown and fixed on the Pico 2 W (commit adc5f56). Not repeated on the Pico W, by the owner | Nothing |
| 2. The notes and comments | Drafted in [notes.md](notes.md), none posted | The owner reading each |
| 3. What each later ADR starts from | Done 2026-11-15 | Nothing |
| 4. The closes | Not started | The owner's word for each |
| 5. The branches | Not started | The owner's word |
| ADR-014, the charge profile (settled at approval) | Drafted 2026-11-15 | Nothing |

## Step 3: what each later ADR starts from

2026-11-15. A "Handed on by ADR-002" in each:
- **ADR-009:** the design first and without the PRs; #113 open and ending there; its three parts with no hardware in them, by file; what is not taken and why; #101's tuning as its header gives it; and that both PRs cut between a tuner and what runs a throw at the same place.
- **ADR-004:** #111 open and ending there; why nothing of it is merged before; its conflicts with main; #44 and #28 closed for it. Its "To settle" asks how #111 ends, no longer for this ADR's verdict.
- **ADR-006:** #113 is still open when files are moved, and that this costs nothing that is kept.

#101 was read for this in its header only (`src/ai_tuning.h` on the PR's branch), and #113 in its file list, its includes and its charge loop. Each hand-on says so.

## ADR-014

2026-11-15. A Draft that holds the owner's idea in the owner's words, what the code says today (the target weight is stored nowhere: five digits that start at zero at each power-up), and why #109's way does not serve it. It is in the index after ADR-006.

## Step 1: the D-term fix

2026-11-15, on the bench's Pico 2 W, the hopper empty, the A&D connected. The Pico W was not done: the owner, asked for the swap, said "We don't need to repeat on Pico W for application layer changes." Its image builds; it was not flashed. That rule is now in `AGENTS.md`, under "Proving a change on a unit".

**Before anything was changed** (image 1.20.140 at 945dfe6, whose `src/` is main's)
- The unit's settings copied: `logs/adr002/pico2w/settings_before/`. Its selected profile: coarse Kp 0.025, Kd 0.25, limits 0.1 to 5 rev/s; fine Kp 2, Kd 10, limits 0.1 to 3. Coarse stop threshold 5, no precharge. Both motors: 5 rev/s at most, 50 rev/s2.
- So for a 30 gr target the coarse trickler's P term asks for 0.625 rev/s (0.025 x 25), and the fine one's for more than its maximum.

**How it was measured.** `logs/adr002/throw_start.py` (this PC only; it drives `rc_run.py`'s session): a charge of 30 is started, `state` is read as fast as the console answers through the throw's start, and the charge is stopped 0.6 s into the throw. The coarse trickler's first speed is its highest in the 0.15 s after the motors are first seen turning. Twenty throws a run.

**The fault, on the firmware as it was**

| Scale | Throws | Coarse first speed, rev/s | Over P's 0.625 | Over twice P's |
|---|---|---|---|---|
| The A&D, the pan empty (41 ms between readings) | 20 | 0.794 to 1.875 | 20 | 5 |
| Simulated, 25 readings a second | 20 | 0.799 to 1.106 | 20 | 0 |
| Simulated, 10 readings a second | 2 | 0.709, twice | 2 | 0 |

- **What the fault is on the unit:** on a throw's first reading the last error is still 0, so the whole error (25 gr) counts as its change, over the few milliseconds since the loop began. The coarse trickler's D term adds 0.25 x 25 / that time: 1.25 rev/s more at 5 ms, 0.08 more at 74 ms. It lasts until the second reading, 41 ms with the A&D.
- **The division by 0 was not seen,** in 42 throws: the tricklers were never given their maximum. ADR-002's README expected 5 rev/s from the code alone; the unit gave at most 1.875. For the time to be 0 ms a reading must already be waiting when the loop begins. Here the first one came 5 to 36 ms into it with the A&D.
- **Why the simulated scale at 10 a second shows least:** it is in step with the wait for zero, so its first reading comes 74 ms into the throw every time. That is the rate `charge_cycle` uses, and why no proof before this one showed the fault.
- The fine trickler's first speed was its maximum, 3 rev/s, in every throw: its P term alone asks for more.

**What was built** (commit adc5f56, image 1.20.146)
- PR #113's fix in `charge_mode.cpp`: `error_derivative()` gives 0 on a throw's first reading and when no time has passed. The commit carries `Co-authored-by: Daniel Horbach`. Its comment is rewritten for what the unit showed.
- Both boards build.

**After the fix,** on the Pico 2 W

| Proof | Result |
|---|---|
| `flash` | Passed: 1.20.146 at adc5f56 |
| 20 throws, the A&D | Coarse first speed 0.625 rev/s in all 20: what P asks |
| 20 throws, simulated at 25 a second | 0.622 to 0.625 rev/s in all 20 |
| `menu_tour --compare tests/menu_baseline/pico2_w` | `VERDICT PASS` (the version form differs, not judged) |
| `charge_cycle` | `VERDICT PASS`, 14.5 s from charging to the cup prompt, as before |

**How much it mattered.** With the A&D the coarse trickler ran at up to three times its P term's speed for 41 ms at each throw's start: about 0.05 of a turn more, where it has 25 gr to throw. A real fault, in every throw, and a small one.
