# ADR-002: progress

What was done and measured. The plan is in the [README](README.md). Logs are kept on this PC under `logs/rc/`, which git does not track.

## Where each step stands

| Step | State | Waits for |
|---|---|---|
| 1. The D-term fix | Not started | The owner's word that the bench is ready |
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

## Step 1: how the fault will be shown

Not run yet. Worked out from the code on 2026-11-15, for the bench:
- `state` gives each trickler's speed now (`coarse=`, `fine=`), and `sim rate` sets the simulated scale's readings a second, down to 1.
- With the simulated scale slow, the time from a throw's first reading to its second is long enough to read the coarse trickler's speed several times. With the default profile and a 30 gr target its P term asks for 0.625 rev/s; the fault asks for the profile's maximum, 5 rev/s.
- So: `sim on`, a slow `sim rate`, `charge 30`, and `state` read through the start of the throw. Above 0.625 rev/s on today's firmware is the fault seen; at or under it after the fix is the fix seen.
