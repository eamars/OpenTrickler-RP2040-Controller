# ADR-009: every number the charge uses, and its life cycle

Status: **Research, with proposals, none approved** (2026-11-22, 22:46 NZDT). This page is gate N of the README ("Gate N, the cleanup"): no gate after it is built until every row below has an approved proposal.

**What is listed:** every number the charge reads, from the zero to the verdict, in any loop on the branch `adr009_controller`. It also lists every number the calibration uses to measure. A number is listed whether it is a user's setting, a measurement, a constant or a literal inside a rule. Only identities (0 and 1 as nothing and all) and loop counters are left out. Where a number appears in several places, each place is named.

**Each row gives:**
- **Default:** the value a unit has before anyone sets, measures or learns it.
- **Who modifies:** who may change it, and through what (the screen, the web, the API, the console).
- **Who calibrates:** what measures or corrects it (the calibration, the auto-tuner, nothing).
- **Who uses:** which code reads it, and in which loop: today's PID loop (*legacy*), the staged controller's glide (*glide*), the PID law in the staged frame (*PID law*, `ccon speed_law=1`), the PID on a learnt forecast (*pid_learnt*).
- **Origin:** where its value came from, with the evidence found. "None found" means no measurement or study backs it.
- **Proposed:** its life cycle in the final controller, by the classes below.

**The proposed life-cycle classes:**

| Class | Default | Who modifies | Who calibrates | Stored |
|---|---|---|---|---|
| **User** | today's default | the user (screen, web, API); the user's agent on the user's yes | only where the row says: the calibration on the user's yes, the auto-tuner while it is on | its record as today, in the backup |
| **Measured** | none is taken as true: before a calibration the row's fallback is used, chosen to err short, never over | nobody by hand; the user's agent on the user's yes, checked against its bounds | the calibration measures it; the auto-tuner corrects it while on, within its bound | the unit's own calibration record, in the backup, never in the powder profile |
| **Rule** | the value in the code | nobody: changed only by an ADR, with its evidence | nothing | not stored; in the parameter register and the design overview with its reason |
| **Safety** | as Rule | as Rule, and never reachable by the calibration, the learning or an agent | nothing | as Rule |
| **Remove** | | | | debug switches, experiments and duplicates; gone before ADR-009 closes |
| **Not control** | | | | a counting step, a buffer, a test-only value: listed so the list is complete |

A Rule row is kept as a rule only when the research shows it: either the simulation's results are insensitive to it across the recorded rigs (`tools/charge_sim/records/`), or it follows from physics or a safety argument written in its row. Otherwise it becomes Measured, or a decision for the owner.

## Findings that cut across the list

1. **Three of the frame's constants duplicate the user's speed settings, and one is a limit of the simulation.**
   - **`fine_top_rps` 0.5** comes from `staged.py`'s `FINE_TOP_RPS`. Its comment there: above 0.5 rev/s the simulated tube pours in gaps the bench never showed.
   - It also caps the calibration's ladder. So no calibration measures above 0.5 rev/s.
   - The bench ran 1.0 by hand, where nothing was measured, and the fine figure's learning read through that unmeasured lag (progress.md, 11-22, 16:37).
   - **`coarse_top_rps` 2.0 and `coarse_floor_rps` 0.3** are the ends of the same ladder.
   - Meanwhile the glide ignores the profile's speeds and the motors' least speeds altogether.
2. **The tolerance is set twice:** by the user's fine stop threshold (today's loop), and by the debug `mode`, 0.02 or 0.04 gr (the staged loops). The code already holds the conversion between them: the staged verdict's threshold is the band plus half a digit (`charge_mode.cpp:701`).
3. **The scale's interval has two sources:** the scale's rhythm measured live (`scale_get_usual_interval_ms`, used for the pauses) and the calibration record's `interval` (used by the rules).
4. **The bench unit's calibration record holds a hand correction:** every lag shifted 0.055 s earlier (`series.py --lag-shift`). The PC tool's own timing made it necessary. A calibration on the unit removes the cause.
5. **The motor's least speed says one thing and does another.** Its comment says a profile can override it (`motors.c`), but every loop takes the higher of the two, so a profile can only raise it. The glide's creep goes under it: 0.058 rev/s against the bench unit's stored 0.1.
6. **Four of the user's settings do nothing in the staged loops:** the back-off, the precharge, the gate at the coarse stop and the coarse stop threshold. Only today's loop and `pid_learnt` read them.
7. **The fine figure's learning is on by default** (`learn_fine_prior` 3, `learn_fine_split` 1), although the evidence says it cannot be trusted until the fine tube's lag at speed is measured (progress.md, 11-22, 17:17). Every night series ran with it switched off by hand.
8. **Literals sit inside the rules** (`charge_control_set_rules`, section D): limits such as "between 0.4 and 2.0 s" for which no study was found.
9. **Units:** the bands, the digit and several literals are in grains. The thresholds' unit is the scale's ("weight"). A scale set to grams has not been tried with the staged loops.

## A. The user's settings the charge reads today

| # | Number (where) | Default | Who modifies | Who calibrates | Who uses | Origin | Proposed |
|---|---|---|---|---|---|---|---|
| A1 | Coarse kp, ki, kd (`profile_t`, per powder, 8 profiles) | AR2209: 0.05, 0, 0.3 (`profile.c`) | user: screen, web, API; `ctune accept` (console, experiment) | nothing; the offer (`ctune`, experiment) makes a kp | legacy; PID law; pid_learnt. Not the glide | users' hand tuning | **User**, the PID seed. The calibration writes kp on yes; the auto-tuner writes its derived kp while on. ki and kd: the offer sets them to 0, which changes what a user's setting does: a register entry |
| A2 | Coarse min and max speed (`profile_t`) | AR2209: 0.1, 8 rev/s | user | nothing | legacy, PID law, pid_learnt (each held within the motor's limits). The glide uses C4 and C5 instead | users' hand tuning | **User:** the coarse tube's limits in every law. The calibration measures its ladder within them, and the frame never runs faster than the fastest speed measured (replaces C4, C5, F7) |
| A3 | Fine kp, ki, kd (`profile_t`) | AR2209: 0.8, 0, 15 | as A1 | as A1 | legacy; PID law; pid_learnt | users' hand tuning | as A1. The firmware's D is taken per millisecond, so a user's Kd does almost nothing (`research/pid_and_staged_2026-11-22.md`): fixing it changes behaviour, a register entry |
| A4 | Fine min speed (`profile_t`) | AR2209: 0.08 rev/s | user | nothing | legacy, pid_learnt (raised to the motor's least, A14). The glide and the PID law use the calibration's creep instead | users' hand tuning | **User**, as the creep. Before a calibration it is the floor. The calibration writes its creep (D-rules from the measured lag) into it on yes; the auto-tuner, while on, writes the creep it learns. Never under the motor's least speed (A14) |
| A5 | Fine max speed (`profile_t`) | AR2209: 2.0; profile 0: 5.0 rev/s | user | nothing | legacy, pid_learnt; the PID law takes the lower of it and C3; the glide uses C3 | users' hand tuning | **User**, as A2 for the fine tube (replaces C3) |
| A6 | Coarse stop threshold (charge settings, one per unit) | 5 gr | user | nothing | legacy, pid_learnt. The glide and the PID law use the calibration's coarse room (D9) | the original firmware | **User**, the coarse stop for a powder not calibrated. A calibrated powder uses its measured coarse room, per powder, in the calibration record. **For the owner:** the setting is one per unit and the room one per powder; the proposal keeps both |
| A7 | Fine stop threshold (charge settings) | 0.03 gr | user | nothing | legacy (stop and verdict). The staged loops take the band from A18's `mode` | the original firmware | **User**, the user's tolerance: band = threshold − half a digit, the code's own conversion (finding 2). A18 and E16 go |
| A8 | Zero steadiness, SD (`set_point_sd_margin`) | 0.02 | user | nothing | every loop: the zero, the steadiness after a pause, a series' start | the original firmware | **User**, unchanged. A scale's noise could be measured, but the setting works and is the user's |
| A9 | Zero mean (`set_point_mean_margin`) | 0.02 | user | nothing | every loop: the zero; the cup's removal test (E7) | the original firmware | **User**, unchanged; E7's literal is mended separately |
| A10 | Gate at the coarse stop (`coarse_stop_gate_ratio`) | 0 (−1 is off) | user | nothing | legacy only | PR history | **User.** The frame moves the gate at the coarse stop and waits its travel (issue #135, E10) |
| A11 | Back-off: on, turns, speed | on, 0.25 turn, 1 rev/s | user | nothing | legacy, pid_learnt. Not the glide or the PID law | PR #108 | **User.** The frame makes it, and the forecast counts its spill as the calibration measures it at these turns and this speed (0.83 gr of AR2209 on the bench, 11-21) |
| A12 | Precharge: on, time, speed | off, 1000 ms, 2 rev/s | user | nothing | legacy only | the original firmware | **User.** The frame makes it once the gate has closed (its travel, E10), and counts its powder in the next throw's forecast |
| A13 | Motor max speed (each motor) | 5 rev/s | user | nothing | every loop: the hard cap in `motor_set_target` | the original firmware: "before the stepper starts to lose step" | **User,** the hardware cap. The calibration's ladder never goes above it |
| A14 | Motor least speed (each motor) | coarse 0.08, fine 0.01 rev/s | user | nothing | legacy, PID law (coarse), pid_learnt: the higher of it and the profile's. The glide's creep ignores it (finding 5) | the original firmware | **User,** the hardware floor for every law. The calibration's ladder starts at it. When it holds the creep up, the calibration's verdict says so in words |
| A15 | Motor acceleration | 50 rev/s² | user | nothing | the motors' ramp, every loop. The forecast counts steps on the pin, so a ramp is counted | the original firmware | **User**, unchanged |
| A16 | Gear ratio, full steps, microsteps | 1.25 / 1.818, 200, 256 | user | nothing | turns from steps, every loop | the hardware | **User**, unchanged |
| A17 | Servo gate: each shutter's ends, the two speeds | `servo_gate.c` | user | nothing | the gate's moves; legacy's partial close | the hardware | **User**, unchanged; E10's waits are made from them |
| A18 | `controller` (staged, pid, pid_learnt), `mode` (accurate, fast): the charge settings' filler | staged, accurate | user (screen, web, API today) | nothing | every throw's choice of loop and band | built for evaluation (gate 2 and the night's experiments) | **Remove** (sign-off item 1). The band comes from A7 |

## B. The calibration record (one slot a powder profile, 13 to 20 K)

Measured today by the PC (`bench/trial.py` through the console) and written by `ccal`. Read by the glide and the PID law; pid_learnt reads none of it.

| # | Number | Default | Who modifies | Who calibrates | Who uses | Origin | Proposed |
|---|---|---|---|---|---|---|---|
| B1 | The scale's digit | none (refused without a calibration) | `ccal` (console) | the PC tool, from readings | band edges, aim, rise, verdict (C, D) | measured | **Measured**, a property of the unit's scale. Fallback before a calibration: the readings' own step, seen at the zero |
| B2 | The scale's interval | none | `ccal` | the PC tool | the rules (D15) | measured; a second source exists live (finding 3) | **One source:** the scale's rhythm measured live. The record keeps it for the simulation only |
| B3 | Grains a turn, coarse and fine | none | `ccal`; `series.py --coarse-gr-turn --fine-gr-turn` (hand, series D) | the PC tool, from runs at 1 rev/s from rest | the forecast, every rule | measured. Throws poured 3 to 9% more (coarse); the fine 0.83 a turn over a throw and 0.68 at its finish (progress.md, 11-22) | **Measured** where a throw uses it (gate 3's mend); corrected by the auto-tuner while on, within its bound (C13) |
| B4 | Watch time | none | `ccal` | the PC tool: 1.5 × the slowest probe + 1 s, at least 3 s (F4) | the calibration's own waits | measured by rule | **Measured** |
| B5 | Coarse share (the coarse figure's run-to-run spread) | none | `ccal` | the PC tool | the forecast's coarse margin (D21) | measured | **Measured** |
| B6 | Stops, each tube (coarse 3, fine 6): speed, unseen powder, spread, quiet time, lag | none | `ccal`; `--lag-shift` (hand, −0.055 s on the bench record, finding 4) | the PC tool, at the ladder's speeds (F1, F2) | the forecast, the creep, the rest, the coarse room | measured, its lags corrected by hand | **Measured,** in the firmware's own time (gate 3), across the user's speed range (A2, A5, A14). The hand shift goes with the PC tool |
| B7 | Small moves (3 sizes): turns, mean, most, wait | none | `ccal` | the PC tool (F3) | the top-ups (`charge_control.c:489`) | measured | **Measured** |
| B8 | First errors: the rehearsals', then carried from charges (newest 16) | none | `ccal`; the carry at a charge's end | the rehearsals (F5); the carry (E13) | where the stop is placed (`charge_control_place_stop`) | measured; rehearsals spread twice as widely as real throws' stops (progress.md, 11-22) | **Measured,** rehearsed as a throw's last stretch (gate 3); the carry is the auto-tuner, while on |
| B9 | The band and the finish stored with it | 0.02 gr, glide | `ccal` | nothing | rules made for that band (re-made for another band at a charge's start) | chosen by the PC tool's run | **Remove** from the record: the band is A7's, the finish is the glide alone (C6) |
| B10 | Made by the rules, not stored: creep, stop room, aim, glide room, glide time, coarse room, coarse glide time, rest | made at each charge's start | nobody | made from B with C and D | the throw | rules | stay made by rule; each rule's literals are section D |

## C. The frame's constants (`charge_control_default_constants`, `charge_control.c:40`)

The same on every unit, never stored, changed live only by the console's `ccon` until a restart. **None is calibrated.**

| # | Number | Default | Who modifies | Who calibrates | Who uses | Origin | Proposed |
|---|---|---|---|---|---|---|---|
| C1 | `move_rps`, a small move's speed | 0.2 rev/s | `ccon` | nothing | the top-ups' small moves | `staged.py`: "as the bench's watched nudges were" | **Measured:** the speed the calibration made its moves at (B7), read from the record, so the moves are made as they were measured |
| C2 | `parcel_s` | 0.01 s | `ccon` | nothing | the forecast's counting step | `staged.py` | **Not control** (a counting step). It stays, listed |
| C3 | `fine_top_rps` | 0.5 rev/s (1.0 by hand in every night series) | `ccon`; `series.py` | nothing | the glide's top; the PID law's envelope; the fine learning's window | the simulation's tube (finding 1) | **Remove.** The top is the lower of A5 and the fastest fine speed the calibration measured |
| C4 | `coarse_top_rps` | 2.0 rev/s | `ccon` | nothing | the glide; the envelope; the offer's coarse max | the ladder's top; no reason found | **Remove,** as C3 with A2 |
| C5 | `coarse_floor_rps` | 0.3 rev/s | `ccon` | nothing | the glide; the envelope; the offer's coarse min | one bench observation: "pours in lumps below it" (2026-11-20) | **Remove.** The floor is A2's least speed. The calibration measures a stop there, and its spread is in the forecast's coarse margin |
| C6 | `dribble_digits_s` | 8 digits/s | `ccon` | nothing | the jog finish's dribble only | the bench: the slowest even flow, AR2209 | **Remove** with the jog finish. The bench trial of 11-21 judged it (jog 56% against glide 68%), and one controller has one finish |
| C7 | `coarse_spreads` | 3 | `ccon` | nothing | the forecast's margin for the coarse powder still unseen (D21) | `staged.py`; no study found | **Rule** if the research below shows it is insensitive; it is a confidence of three spreads, a trade of time against overs |
| C8 | `creep_band_share` | 1.0 | `ccon` | nothing | the creep: the powder in the air at the stop as a share of the band | `staged.py`: half was tried, 1.5 to 2 s slower and no more good throws (progress.md, 2026-11-21) | **Rule**, with that evidence |
| C9 | `creep_most_s` | 6 s | `ccon` | nothing | the creep is never so slow that its stretch takes longer than this | `staged.py`; no study found | **Rule** if insensitive; otherwise a trade for the owner |
| C10 | `learn_share` | 0.15 | `ccon` | nothing | the stop's learning: the share of each first error taken in | the simulation; a mean-based learning tried and rejected on the rig (progress.md, 11-22) | **Rule**, with that evidence: the auto-tuner's own rate |
| C11 | `learn_most_digits` | 2 | `ccon` | nothing | the most a throw teaches | `staged.py` | **Safety** of the learning: a bound |
| C12 | `learn_dead_digits` | 0.25 | `ccon` | nothing | the least a throw teaches | `staged.py` | **Rule** (below a quarter digit is the reading's own rounding) |
| C13 | `learn_fine_prior` | 3 (on) | `ccon`; set to 0 by hand in the night series | nothing | the fine figure's learning: how many throws the calibration's figure counts for | 11-22 day; judged untrustworthy (finding 7) | **For the owner:** off until gate 3 measures the fine tube's lag at speed; then on, a rule with that evidence. Its switch is the auto-tuner's switch, not a constant |
| C14 | `learn_fine_most` | 0.25 (±25%) | `ccon` | nothing | the fine figure's bound | 11-22 | **Safety** of the learning: a bound |
| C15 | `learn_fine_split` | 1 (on) | `ccon` | nothing | one fine figure, or a stream's and a finish's | 11-22: no measurable gain on the rig (16:37) | **Remove** the switch. Whether the split stays is decided with C13, from gate 3's measurement |
| C16 | `still_least_s` | 0.4 s | `ccon` | nothing | at rest: no new high for this long, or two intervals if longer (D15) | `research/proposals_2026-11-21.md` | **Measured:** the scale's own quiet time (B6) and interval give it. The 0.4 s goes |
| C17 | `most_moves` | 8 | `ccon` | nothing | tries a throw gets after its first rest | `staged.py` | **Safety:** a bound on a throw |
| C18 | `throw_most_s` | 120 s | `ccon` | nothing | a throw past it ends "limit" | `staged.py` | **Safety** |
| C19 | `fine_most_turns` | 15 turns | `ccon` | nothing | a throw past it ends "limit" | `staged.py` | **Safety.** It is in turns, so it means a different weight on another tube: research whether it should be in grains, from B3 |
| C20 | `over_most_gr` | 3 gr | `ccon` | nothing | a reading this far over stops everything | `staged.py` | **Safety** |
| C21 | `speed_law` | 0 (glide) | `ccon` | nothing | which law pours | the night's experiment | **Remove.** The PID law pours (README, "Where ADR-009 stands") |
| C22 | `pid_envelope` | 1 (on) | `ccon` | nothing | the guard on the PID law | the night's experiment | **Remove** the switch; the envelope is always on |

## D. Literals inside the rules (`charge_control.c`, `charge_calibration.c`, `charge_mode.cpp`)

None is a constant anyone can see or set; each is a number written into a formula.

| # | Rule (where) | Literal | Origin | Proposed |
|---|---|---|---|---|
| D1 | The slowest fine stop's lag is taken as at least 0.3 s (`:124`) | 0.3 s | none found | research: what a lag under 0.3 s would do; derive it from the interval if it guards the scale's resolution |
| D2 | The creep is never under half the slowest stop's speed (`:126`) | 0.5 | none found: keeps the creep near what was measured | **Rule** ("never far outside what was measured"), with the study |
| D3 | The creep is never above the third fine stop's speed (`:125`, index 2) | the third stop | none found | **Rule** in words ("within the measured ladder") once the ladder is the user's range (B6); the index goes |
| D4 | The aim: a digit under the target, or half the band if more (`:129`) | 1 digit, ½ band | `proposals_2026-11-21.md`, section 2 | **Rule** |
| D5 | The jog's stop room: 2.5 spreads and a digit (`:134`) | 2.5 | jog only | **Remove** with the jog (C6) |
| D6 | The glide room: the stop room plus twice the spread of a stop from four times the creep (`:141`) | 2, 4 | `proposals_2026-11-21.md`: the creep then lasts about 3 s | **Rule** if insensitive |
| D7 | The glide time: 0.6 × the longest lag of the middle stops, between 0.4 and 2.0 s (`:148`, `:151`) | 0.6, 0.4, 2.0, from the third stop | `proposals_2026-11-21.md`, "between set limits"; no study of the limits found | 0.6: **Rule** if insensitive. The limits: research; drop them if they never bind on the records |
| D8 | The coarse glide time: 0.6 × the coarse lag at its floor, between 0.3 and 1.5 s (`:152`) | 0.6, 0.3, 1.5 | as D7 | as D7 |
| D9 | The coarse room: 3 spreads of a coarse stop from its floor + the glide room + 0.2 gr (`:153`) | 3, 0.2 gr | `proposals_2026-11-21.md` | 3: as C7. **0.2 gr is in grains, a fixed weight on any scale:** research; express it in digits or from a measurement |
| D10 | The rest: the slowest quiet time of stops within twice the creep + 0.3 s (`:157`, `:161`) | 2, 0.3 s | `proposals_2026-11-21.md` | research: the 0.3 s may be an interval or two (B2) |
| D11 | The jog's aim: two SDs of the rehearsals and a digit (`:192`) | 2 | jog only | **Remove** with the jog |
| D12 | The coarse tube runs only if the target is more than the coarse room and half a coarse turn (`:226`, `:659`) | ½ turn | none found | **Rule** if insensitive |
| D13 | The fine learning's window opens after the coarse tube's quiet time + 0.3 s (`:518`) | 0.3 s | none found | with C13 |
| D14 | A rise is half a digit (`:610`) | ½ digit | the digit grid | **Rule** |
| D15 | At rest: no new high for C16, or two intervals if longer (`:647`) | 2 intervals | `proposals_2026-11-21.md` | **Rule** (with C16 measured) |
| D16 | The band's edge is a quarter digit past the band (`:32`; also `charge_pid_learnt.c:8`) | ¼ digit | a float fault mended on 11-22 (10.02 over a 60 gr cup) | **Rule** (numerical tolerance), one definition, not two |
| D17 | The fine learning ignores a stretch under 0.2 turns, a stream under 0.5 (`:33`, `:34`) | 0.2, 0.5 turn | `staged.py` | with C13 |
| D18 | Two times within 0.1 ms are the same time (`:37`) | 1e-4 s | lockstep with the Python reference | **Not control** |
| D19 | The offer: coarse kp = 1 / (grains a turn × coarse glide time); fine kp = 0.6 / (grains a turn × glide time); ki, kd 0 (`charge_calibration.c:741`, `:746`) | 1, 0.6 | the coarse from the glide's own law; the fine 0.6 chosen in the simulation on this rig's two records; on the rig 2 to 3 s quicker with more overs (README) | **Rule** for the form. The 0.6 is not a rule yet: gate 3 measures the trade between speed and spread on each unit and chooses it from the user's band and over budget. It is the open item of the record |
| D20 | The staged verdict's threshold: the band and half a digit (`charge_mode.cpp:701`, `:850`) | ½ digit | the firmware's verdict rule (over or under from 0.03 gr) | **Rule**; it is A7's conversion |
| D21 | The forecast's coarse margin: C7 × B5 × the coarse powder unseen (`charge_control.c:365`) | (C7) | | with C7 |

## E. The charge's own fixed numbers (`charge_mode.cpp`, `charge_mode.h`, `charge_calibration.h`)

| # | Number (where) | Value | Who uses | Origin | Proposed |
|---|---|---|---|---|---|
| E1 | No reading for this many usual intervals: the tubes pause (`CHARGE_STALE_INTERVALS`) | 3 | every loop | ADR-013, approved | **Safety** (ADR-013) |
| E2 | This many, and at least 1 s: the scale is lost (`CHARGE_LOST_INTERVALS`, `_MIN_MS`) | 10, 1000 ms | every loop | ADR-013 | **Safety** (ADR-013) |
| E3 | The rhythm not yet known: a G&G's (`CHARGE_USUAL_INTERVAL_UNKNOWN_MS`) | 250 ms | every loop | ADR-013 | **Safety** (ADR-013) |
| E4 | A third pause in one throw ends it (`CHARGE_PAUSES_BORNE`) | 2 | every loop | ADR-013 | **Safety** (ADR-013) |
| E5 | Steady after a pause: 10 readings 300 ms apart, unsteady after twice that (`CHARGE_STEADY_*`) | 10, 300 ms | every loop | ADR-013 | **Safety** (ADR-013) |
| E6 | The zero: 10 readings, each waited for up to 300 ms (`:319`, `:376`; a comment says 200 ms) | 10, 300 ms | every loop | the original firmware | **Rule;** the comment is mended |
| E7 | The cup lifted: the mean + 10 under the mean margin (`:1231`) | 10 (the scale's unit) | every loop | the original firmware; known fault: a cup under 10 is never seen to go (README, handed on by ADR-012 and ADR-003) | **Rule,** mended: research a test from the zero's own weight, not a fixed 10 |
| E8 | The cup back: the weight ≥ 0 exactly (`:1283`) | 0 | every loop | known fault: a cup back a hair light is never seen (README, ADR-003) | **Rule,** mended: within the zero margin (A9) |
| E9 | Today's verdict, 1 s after the stop (`:1135`) | 1000 ms | legacy | the original firmware | **Remove** with today's loop (sign-off item 11's comparison first); the frame's verdict is a settled reading |
| E10 | The precharge starts 500 ms after the gate's close (`:1113`); 20 ms for other tasks (`:1122`) | 500 ms, 20 ms | legacy | issue #135 asks for a delay | **Rule:** each wait is the gate's own travel (A17) and a settle, measured with a gate fitted. The 20 ms goes with today's loop |
| E11 | The back-off's longest (`COARSE_STOP_BACKOFF_MAX_MS`) | 15 s | legacy | "safety cap" | **Safety** |
| E12 | The carry: at least 5 throws at rest, at most 32 a charge, the newest 16 kept, at most 3 digits at once (`CHARGE_CARRY_*`, `CHARGE_CALIBRATION_FIRST_ERRORS_MAX`) | 5, 32, 16, 3 digits | the glide and the PID law, at a charge's end | 11-22 (b23b214), a temporary form | **Safety** of the learning (bounds), part of the auto-tuner; its switch is the auto-tuner's |
| E13 | A series ends when the cup would pass this (`CHARGE_SERIES_CUP_MOST`) | 600 gr | a series | the bench's cup | **For the owner:** a bound of the bench's cup, applied to every user's. Research the cup's own capacity as a setting, or drop the limit |
| E14 | The bands of the debug mode (`CHARGE_TRICKLE_*_BAND_GR`) | 0.02, 0.04 gr | the staged loops | evaluation | **Remove** (A7) |
| E15 | A throw's recording, rows kept (`CHARGE_RECORD_ROWS`) | 800 | the record | RAM | **Not control** (its RAM is gate 7's measurement) |

## F. The calibration procedure (`tools/charge_sim/staged.py`, `bench/trial.py`; the job gate 3 builds on the unit)

| # | Number | Value | Origin | Proposed |
|---|---|---|---|---|
| F1 | The fine stops' flows, counts, run lengths (`FINE_RATES_DIGITS_S`) | 1, 2, 4, 8, 16, 32 digits a second; 8, 8, 4, 4, 4, 4 stops; 5 to 3 s | the research's design, in digits so the ladder follows the scale | **Rule** for the form. The ladder is cut by the user's range (A5, A14) instead of F7, and reaches the user's fine max |
| F2 | The coarse stops (`COARSE_STOPS`) | 0.3, 1.0, 2.0 rev/s; 3, 2, 2 stops | the bench's coarse runs | **Rule** for the form; the speeds from A2 and A14 instead of fixed |
| F3 | The small moves (`MOVES_DIGITS`) | 0.7, 2.5, 6 digits; 24, 12, 8 moves | the research's design | **Rule** |
| F4 | The probes: 4 moves watched 15 s; then every watch 1.5 × the slowest + 1 s, at least 3 s | 4, 15, 1.5, 1, 3 | the research's design | **Rule** |
| F5 | The rehearsals (`REHEARSALS`, `REHEARSAL_GR`) | 10 of 1 gr | the research's design; spread twice the throws' (progress.md, 11-22) | gate 3's mend: 40 or more, as a throw's last stretch |
| F6 | The prime: 1 coarse turn, 3 fine turns at 1 rev/s, until two runs pour the same | 1, 3 turns, 1 rev/s | the research's design | **Rule**; the "same" needs its tolerance written |
| F7 | The ladder's ends (`FINE_TOP_RPS`, `COARSE_TOP_RPS`, `COARSE_FLOOR_RPS`) | 0.5, 2.0, 0.3 rev/s | the simulation's tube (finding 1) | **Remove** (A2, A5, A14) |
| F8 | The PC's lag shift (`--lag-shift`) | −0.055 s | a hand correction for the PC tool's timing (finding 4) | **Remove** with the PC tool: the job runs on the unit |
| F9 | The PC trial's watch after a verdict (`WATCH_AFTER_S`) | 4 s | the PC trial | **Not control** (a PC tool's) |

## G. The experiments, to fold or remove (`charge_pid_learnt.c`, the 21 K record, `ctune`)

| # | Number | Value | Origin | Proposed |
|---|---|---|---|---|
| G1 | `lag_s`, `rest_s` | 0.9 s, 2.5 s | the A&D's, by hand | **Measured** (B6) when its predictor is folded into the frame as the fallback for a powder not calibrated. A unit never calibrated needs a cautious fallback: research what errs short on every scale in `scales_like.py` |
| G2 | `gr_turn_prior` | 2.0 | raised from 1.0 after this rig's dry throws | as G1: the fallback must err short; research it across the records, not this rig |
| G3 | `floor_least_rps`, `coarse_quiet_s`, `coarse_lag_s` | 0.02 rev/s, 1.0 s, 0.6 s | guesses | **Remove** with pid_learnt; A14 and B6 cover them |
| G4 | `digit_gr` | 0.02 | the A&D's | **Measured** (B1) |
| G5 | `learn_share`, `throw_most_s`, `over_most_gr`, `most_tops`, `LEARN_LEAST_TURNS`, `BAND_EDGE_DIGITS` | 0.15, 120 s, 3 gr, 8, 0.01, 0.25 | copies of C and D | **Remove** the copies; one definition each |
| G6 | `floor_band_share`, `coarse_gr_turn`, `coarse_margin_share` | 0, 0, 0 | switches of the night's experiments | **Remove** |
| G7 | The PID law's carried stops (EEPROM 21 K) | a list a profile | 11-22 night, a temporary form | **Measured** (B8), moved into the calibration record (gate R). One law pours, so one list |

## H. Test only: the simulated scale's calibration (`charge_calibration.c:109` to `:139`)

The speeds 0.3, 1, 2 and 0.025 to 0.5 rev/s; moves of 0.7, 2.5 and 6 digits; a lag of half an interval; a watch of 1 s; a coarse share of 0.01; a band of 0.02. Used only with `sim on`, never with a real scale. **Not control.** They follow the frame when it changes, so `charge_cycle` still runs.

## The research for each row

The proposals above come from the code, its comments and the record. What remains to find out, by row:
- **The sensitivity of every Rule candidate** (C7, C9, C12, D1, D2, D6 to D10, D12): each at half and at twice its value, in the simulation, on every record in `tools/charge_sim/records/` and the scales of `scales_like.py`, against the glide's yield and median.
  - **Pass for a Rule:** the yield moves by less than the simulation's own noise of 800 throws, and the median by less than half a second.
  - A literal that fails becomes Measured, or a trade for the owner, with its figures.
- **The study as run** (`tools/charge_sim/numbers_study.py`, written 2026-11-22, 23:00, before its run):
  - **The controller** is the one ADR-009 delivers: the PID law in the frame, the envelope on, the owner's fine gains with the coarse tube's from the calibration (option 2's form). It is calibrated afresh with each number changed, so the calibration's rules see the change too.
  - **The cases:** both records, the five scales of `scales_like.py`, both bands (0.02 and 0.04 gr), 10 gr, 800 throws a case.
  - **The numbers:** C7 to C12, C16, D1 to D3, D6 to D10, D12, D15. Each is tried at half and twice its value; D3, an index, at the stop before and the stop after.
  - **The noise:** two SDs of the difference between two independent runs of 800, 2 × √(2p(1 − p)/800), from the baseline's share p. The baseline is run on a second seed as well, to show it.
  - **One amendment before the run:** a 200-throw timing check showed the baseline's median moving 1.5 s between two seeds. So where the baseline's two seeds differ by more than 0.5 s in median, that case's median bar is their difference.
  - **The literals named first:** in `staged.py`, as module constants. The firmware's lockstep check passes on both laws, so the refactor changed nothing.
- **The limits, checked 2026-11-22, 23:05** (D7, D8): 0.6 × the lag against each limit, on the three distinct calibrations in the recordings, and on the simulation's calibrations of both records on the five scales (three seeds each):
  - **D7's 2.0 s acts** on the slowest stand-in scale with AR2209 (2.12 to 2.30 s, so held at 2.0); nowhere else. Its 0.4 s never acts: the least was 0.53 s, on the bench.
  - **D8's 0.3 s comes within 0.02 s of acting on the bench** (0.320 to 0.359 s); its 1.5 s never acts (the most was 1.07 s).
  - So two of the four limits matter: the fine time's top on a slow scale, and the coarse time's floor on the bench's own. The study above says whether moving them changes a result.
- **The fallback before any calibration** (G1, G2): what errs short on every recorded scale and powder. The study, `tools/charge_sim/fallback_study.py` (written 2026-11-22, 23:20, before its run):
  - **The controller:** the PID on a learnt forecast (the firmware's `charge_pid_learnt.c`) with the owner's .168 gains, as the rig threw it.
  - **The numbers tried:** its lag 0.9 s, rest 2.5 s and prior 2.0 gr a turn, each as built, at half and at twice.
  - **The cases:** both records, the five scales, both bands, 10 gr, 3,200 throws a case in sessions of 25 from a fresh figure.
  - **The pass:** the numbers hold on any scale when, as built, on every stand-in scale the first three throws of a session are over no more often than on the measured scale, within two SDs of the difference. Where they are over more often, the numbers are this scale's and must be measured on each unit (a check of the scale without powder, or the calibration).
  - **Run 2026-11-22, 23:26, to 2026-11-23, 00:10** (`fit/study_20261122/fallback.txt`). **Fail: G1's numbers are the A&D's.** The first three throws' overs against the measured scale's:

    | Scale | AR2209, 0.02 | AR2209, 0.04 | BM2, 0.02 | BM2, 0.04 |
    |---|---|---|---|---|
    | late_small | +4.2 (ok) | +1.8 (ok) | +3.9 (more, noise 3.5) | +0.5 (ok) |
    | holds | +29.9 | +18.0 | +15.1 | +2.3 (more, noise 2.1) |
    | slow | +41.1 | +32.8 | +31.5 | +19.5 |
    | slower | +59.1 | +60.9 | +56.8 | +49.5 |

    - **What mends it is the scale's own numbers.** On the slow scale (twice as late), a lag and a rest matched to it cut the overs:
      - BM2 at 0.04: lag ×2 took the first three's overs from 21.6% to 2%; rest ×2 took the whole session's from 33.6% to 15.6%.
      - AR2209 at 0.04: lag ×2 took the first three's overs from 42% to 34%; rest ×2 took the session's from 49.5% to 36.8%.

      So lag and rest are properties of the scale. **G1 is Measured:** the calibration measures them on each unit, its scale part with a fixed weight (the owner's three-part calibration, 2026-11-20) or with its first stops.
    - **G2, the prior,** moves with them: ×2 cut the first three's overs on the slow scale (AR2209 42% to 22%, BM2 21.6% to 4%) and cost about a second on the measured one. It makes up for a wrong lag. It cannot be judged apart from G1: it is studied again once G1 is measured, as a Rule for the cautious start.
    - **Even on its own scale** the fallback over-poured AR2209 at 0.02: 31.8% of all throws, and 23% of the first three. The rig of 11-22 at 0.04 saw no over from a fresh start (`research/pid_learning_2026-11-22.md`); the simulation's 0.04 gives 9.6%. Sign-off item 10 (the first charge after an update works with no calibration) rests on this fallback, so its accuracy is part of gate F's pass.
- **The fixed weights in grains** (D9's 0.2 gr, C19's turns, C20, E13), checked in the code 2026-11-22, 23:10:
  - The firmware reads a weight as a bare number in the scale's own unit. Only the A&D's frame carries the unit (`scale_get_unit`, `scale.c:128`), and the charge reads it nowhere.
  - So on a scale set to grams every literal written in grains is read as grams:
    - D9's 0.2 gr is 0.2 g, about 3 gr;
    - C20's far-over stop is 3 g, about 46 gr;
    - E13's cup limit is 600 g;
    - E14's bands are 0.02 and 0.04 g, about 0.3 and 0.6 gr.
  - The user's thresholds (A6 to A9) are in the scale's unit by design, so they hold.
  - **Proposed:** no literal is a weight. A margin is in digits (B1) or in the user's tolerance (A7). A safety limit is in the target's own terms (for example, a share of the target over). Research each one's form with its row.
- **What the auto-tuner writes into the profile** (A1, A3, A4): the PID values and the creep, while it is on. What "off" keeps is the register's first row.
- **One open trade, not a rule:** the offer's fine factor (D19), decided by gate 3's measurement on each unit.

**Kept true by design:** a number added to the controller without a row here is an omission the next agent cannot see. Recommended: a check in CI that lists the numeric literals in the controller's files and fails on one this page does not name. An idea for gate F, not built.
