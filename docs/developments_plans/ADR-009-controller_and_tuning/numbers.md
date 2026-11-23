# ADR-009: every number the charge uses, and its life cycle

Status: **Every row decided; waiting for the owner's approval** (2026-11-23, 17:48 NZDT). This page is gate N of the README. The rows were checked against the code on 2026-11-23 (below, "The audit"), and each fixed number and default against the test for a general-purpose controller the same evening ("The rows against the test"). Each row's life cycle is decided from the record, and none is left as a question. What changes a parameter or a setting is gathered into the README's parameter register, which the owner approves (sign-off item 2). No gate after N is built until then.

**What is listed:** every number the charge reads, from the zero to the verdict, in any loop on the branch `adr009_controller`. It also lists every number the calibration uses to measure. A number is listed whether it is a user's setting, a measurement, a constant or a literal inside a rule. Only identities (0 and 1 as nothing and all) and loop counters are left out. Where a number appears in several places, each place is named.

**Each row gives:**
- **Default:** the value a unit has before anyone sets, measures or learns it.
- **Who modifies:** who may change it, and through what (the screen, the web, the API, the console).
- **Who calibrates:** what measures or corrects it (the calibration, the auto-tuner, nothing).
- **Who uses:** which code reads it, and in which loop: today's PID loop (*legacy*), the staged controller's glide (*glide*), the PID law in the staged frame (*PID law*, `ccon speed_law=1`), the PID on a learnt forecast (*pid_learnt*).
- **Origin:** where its value came from, with the evidence found. "None found" means no measurement or study backs it.
- **Decided:** its life cycle in the final controller, by the classes below, and in brackets the gate of the README's build plan that makes it so.

**What the controller is for,** and the test every row is held to: it is a general-purpose powder trickling controller, tuned on each user's own device. So a number is one of three things:
- **an existing setting,** kept as it is, or written by the calibration and the auto-tuner;
- **a new parameter,** only with the owner's approval and its justification in the register. Its default is a general assumption, the calibration measures it on the unit, and afterwards the user and the user's agent can change it;
- **a fixed number of the method,** which must pass the test below.

What was measured on the bench unit (its scale, its tubes, its powders) shows what a calibration must measure. It is never a rule, a default or a constant in the firmware.

**The life-cycle classes:**

| Class | Default | Who modifies | Who calibrates | Stored |
|---|---|---|---|---|
| **User** (an existing setting) | today's default | the user (screen, web, API); the user's agent on the user's yes | only where the row says: the calibration on the user's yes, the auto-tuner while it is on | its record as today, in the backup |
| **Measured** (a new parameter) | a general assumption written in its row, on the side that errs short; never a value from the bench | the user, under Settings (advanced); the user's agent on the user's yes; each write checked against its bounds | the calibration measures it; the auto-tuner corrects it while on, within its bound | the unit's own records (gate R), in the backup, never in the powder profile |
| **Rule** (a fixed number of the method) | the value in the code | nobody: changed only by an ADR, with its reason | nothing | not stored; in the parameter register and the design overview with its reason |
| **Safety** | as Rule | as Rule, and never reachable by the calibration, the learning or an agent | nothing | as Rule |
| **Remove** | | | | debug switches, experiments and duplicates; gone by the gate named |
| **Not control** | | | | a counting step, a buffer, a clock, a test-only or bench-only value: listed so the list is complete |

**The test for a fixed number** (Rule or Safety). It passes only if all three hold:
1. **It is in the unit's own terms:** a count, a share of something the unit measured or of a setting the user has, or a number of the scale's digits or reading intervals. A weight in grains, a speed in rev/s or a time in seconds that another scale, tube or powder would change does not pass.
2. **Its reason is a general argument,** written in its row. "It worked on the bench" is not one. A study on the simulation's stand-in scales supports a reason; it does not replace it, because the stand-ins are made from the bench's own records.
3. **It holds on a unit it was not developed on** (gate 7). A fixed number that needs another value there is not a rule: it becomes Measured. It is never tuned to the second unit.

A time in seconds passes only as a bound on what a person waits for, or a limit a fault is judged by, the same on any unit (120 s for a throw, a second without a reading).

**How each class lives in the firmware** (the rule for every gate after N):
- **User:** the settings tables (`settings.h`), as today. Read at a charge's start.
- **Measured:** the unit's records. Each field is described once with its bounds, as a setting is, so the API, the schema and the backup follow. Read at a charge's start, never during a throw. Written between charges only: by the calibration on the user's yes, by the auto-tuner at a charge's end, and by the user or the user's agent through the same checked write.
- **Rule and Safety:** named constants in one header, each with its row here and its reason in a line. No number is written inside a formula. Nothing writes them on a unit: the console's `ccon` lists them, and its writes go at gate F.5. The PC's twin sets them for a study.
- **A number without a row fails CI** (gate N.2).

## The rows against the test (2026-11-23, evening)

Every Rule, Safety and default was read again against the test above. **Nine fail it.** Each was a value taken from the bench unit, or a conversion made with its scale's digit. Three of them were in this page's own decisions of the same afternoon.

| Row | What failed | What it becomes |
|---|---|---|
| G1 | The lag (0.9 s) and the rest (2.5 s) of a unit never calibrated: the bench's A&D's, kept as "the default" | A general assumption on the slow side, so an unknown scale errs short. Its value is found on every stand-in scale (F.4), never taken from the A&D |
| G2 | The starting figure, 2.0 gr a turn: raised from 1.0 "after this rig's dry throws", and a weight in grains | Not a number from a rig. The figure starts from what the throw itself shows, the weight risen for the turns made; until it has shown, the frame forecasts nothing, as today's loop does. If F.4's study finds that is not enough, a start value is a new parameter and goes to the owner |
| C20 | The far-over stop written as "150 digits": 3 gr turned into digits with the A&D's digit | In the user's own terms: 100 times the tolerance (3 at the default 0.03), in the scale's unit whatever that is |
| D9 | The coarse room's extra 0.2 gr, written as "10 digits" the same way | Tried at nothing (N.3). Without effect it goes; with one, it is made from a spread the unit measured |
| C16 | Stillness for at least 0.4 s, chosen on the bench's scale | Tried at nothing (N.3). Without effect it goes, and the two reading intervals of D15 stand alone |
| D10 | The rest's extra 0.3 s (and D13's 0.3 s, with C13) | Tried at nothing (N.3). Without effect it goes; with one, it is counted in reading intervals |
| C1 | A small move at 0.2 rev/s, "as the bench's watched nudges were". The calibration measures its moves at that same 0.2, so "measured" only repeated it | The calibration makes its moves at a speed from the unit's own measured ladder; the top-ups use that speed (3) |
| F5 | The rehearsals' 1 gr | Goes with the mend already decided: a rehearsal is a throw's last stretch, its size the coarse room's |
| F6 | The prime at 1 rev/s | A speed inside the user's own range (A2, A5, A13, A14) |

**What passes, and why it is not a list of settings:**
- **The shares and counts of the method** (3 spreads, 0.6 of a lag, 15% of an error, a digit, two intervals): each multiplies something the unit measured. The measurement is the unit's; the share is how the calibration turns it into the controller's number. They are fixed, approved as one row of the register, and held to point 3 of the test.
- **The safety bounds:** fixed, and out of reach of the calibration, the learning and an agent by sign-off item 8.
- **The numbers the rules make** (B10): shown wherever this unit's data is read, with what they are made from. A user or an agent changes the measurements, and the made numbers follow. They are not set by themselves: an agent setting a glide time against the lag it is made from would leave a unit that contradicts itself.

## The audit: the rows against the code (2026-11-23)

Gate N closes only when "every number the code uses has its row, checked against the code". The page as written on 2026-11-22 had not been checked. On 2026-11-23 a second agent read the charge's files line by line against it, without this page's conclusions (`charge_control`, `charge_calibration`, `charge_mode`, `charge_pid_learnt`, `profile`, and the parts of `motors`, `scale`, `servo_gate` and `jobs` a charge calls).
- **Found:** about 215 numbers. About 125 had a row that matched the code.
- **Missing:** 34 entries, about 75 numbers. About 35 of those are the settings' own limits and 30 the other profiles' defaults. They are added below (A19 to A21, B11, B12, C23, D22 to D25, E16 to E25, G8 to G10, and section H's).
- **Wrong in some attribute:** 14 rows and two of the findings. Each is corrected below; the corrections that change a decision are findings 5, 6 and 10 to 14.
- **Not tried on a unit:** everything here is read in the code.

## Findings that cut across the list

1. **Three of the frame's constants duplicate the user's speed settings, and one is a limit of the simulation.**
   - **`fine_top_rps` 0.5** comes from `staged.py`'s `FINE_TOP_RPS`. Its comment there: above 0.5 rev/s the simulated tube pours in gaps the bench never showed.
   - It also caps the calibration's ladder. So no calibration measures above 0.5 rev/s.
   - The bench ran 1.0 by hand, where nothing was measured, and the fine figure's learning read through that unmeasured lag (progress.md, 11-22, 16:37).
   - **`coarse_top_rps` 2.0 and `coarse_floor_rps` 0.3** are the ends of the same ladder.
   - Meanwhile the glide ignores the profile's speeds and the motors' least speeds altogether.
2. **The tolerance is set twice:** by the user's fine stop threshold (today's loop), and by the debug `mode`, 0.02 or 0.04 gr (the staged loops). The code already holds the conversion between them: the staged verdict's threshold is the band plus half a digit (`charge_mode.cpp:701`, `:850`).
3. **The scale's interval has two sources:** the scale's rhythm measured live (`scale_get_usual_interval_ms`, used for the pauses) and the calibration record's `interval` (used by the rules).
4. **The bench unit's calibration record holds a hand correction:** every lag shifted 0.055 s earlier (`series.py --lag-shift`). The PC tool's own timing made it necessary. A calibration on the unit removes the cause.
5. **The motor's least speed says one thing and does another, and differently in each loop** (corrected by the audit).
   - Its comment says a profile can override it (`motors.c`). Today's loop takes the higher of the two for both tubes (`charge_mode.cpp:933`), so a profile can only raise it.
   - The PID law takes the higher of the two for the coarse tube only (`:576`); its fine floor is the calibration's creep.
   - The glide reads neither the motor's nor the profile's least speed: its fine floor is the creep and its coarse floor is C5.
   - `motor_set_target` holds a speed to the motor's max only. So nothing stops a creep under the motor's least: the bench's creep ran at 0.058 rev/s against its stored 0.1 (the firmware's default fine least is 0.01).
6. **Four of the user's settings do nothing in the staged loops** (both laws): the back-off, the precharge, the gate at the coarse stop and the coarse stop threshold. Today's loop reads all four. `pid_learnt` reads the coarse stop threshold and the back-off only.
7. **The fine figure's learning is on by default** (`learn_fine_prior` 3, `learn_fine_split` 1), although the evidence says it cannot be trusted until the fine tube's lag at speed is measured (progress.md, 11-22, 17:17). Every night series ran with it switched off by hand.
8. **Literals sit inside the rules** (`charge_control_set_rules`, section D): limits such as "between 0.4 and 2.0 s" for which no study was found.
9. **Units:** the bands, the digit and several literals are in grains. The thresholds' unit is the scale's ("weight"). The charge reads the scale's unit nowhere (`scale_get_unit` is called only by the API and the menu). A scale set to grams has not been tried with the staged loops.
10. **A second of waiting before every verdict, in every loop** (the audit). The 1000 ms before the verdict's event (`charge_mode.cpp:1135`, E9) was listed as today's loop's. The staged loops reach it too, after their own settled verdict, and so does every throw of a series. A user waits a second a throw for nothing.
11. **The Pico W keeps no throw's rows** (the audit). `CHARGE_RECORD_ROWS` is 800 on the RP2350 and 1 on the Pico W (`charge_mode.cpp:134`). "Every throw recorded" and an agent reading a throw's recording (sign-off items 7 and 14) do not hold on the Pico W until its RAM is found (gate 2's rest, gate 7).
12. **The bench's series can leak into a user's charge** (the audit; read in the code, not tried). The console sets a series before the start is taken (`remote_control.c:731`) and clears it only on "not calibrated". A start refused for a driver's fault or as busy leaves it set, and the next charge from the screen, the web or the API takes it: no zero, no cup lifted between throws.
13. **Two forecasts cut a long lag short without saying so** (the audit).
    - The frame keeps the powder on its way in 256 parcels of 0.01 s a tube, 2.56 s (`CHARGE_CONTROL_PARCELS_MAX`). Past it the parcels merge and the throw stops earlier.
    - `pid_learnt`'s window is 128 steps of 0.01 s, 1.28 s; a lag above it is cut silently (`charge_pid_learnt.c:84`).
    - The slowest stand-in scale's longest fine lag is 3.5 to 3.8 s (worked back from its glide time of 2.12 to 2.30 s, which is 0.6 of it; not measured for this). Erring early is the safe side, but a number that acts without a row is what gate N is for.
14. **The settings' limits take values the frame cannot use** (the audit): a fine stop threshold of 0, and a profile whose speeds are all 0. Profiles 4 to 7 are stored that way by default; today's loop then holds each tube at the motor's least speed.

## A. The user's settings the charge reads today

| # | Number (where) | Default | Who modifies | Who calibrates | Who uses | Origin | Decided |
|---|---|---|---|---|---|---|---|
| A1 | Coarse kp, ki, kd (`profile_t`, per powder, 8 profiles) | AR2209: 0.05, 0, 0.3 (`profile.c`); the others in A21 | user: screen, web, API; `ctune accept` (console, experiment) | nothing; the offer (`ctune`, experiment) makes a kp | legacy; PID law; pid_learnt. Not the glide | users' hand tuning | **User**, the PID law's gains. The calibration writes kp on the user's yes and sets ki and kd to 0 with it (the offer has no I or D). The auto-tuner, while on, writes the kp it derives at a charge's end. ki and kd keep today's meaning (register row 7) (F.4, 3) |
| A2 | Coarse min and max speed (`profile_t`) | AR2209: 0.1, 8 rev/s | user | nothing | legacy, PID law, pid_learnt (each held within the motor's limits). The glide uses C4 and C5 instead | users' hand tuning | **User:** the coarse tube's limits in every law. The calibration measures its ladder within them, and the frame never runs faster than the fastest speed measured (replaces C4, C5, F7) (F.2) |
| A3 | Fine kp, ki, kd (`profile_t`) | AR2209: 0.8, 0, 15 | as A1 | as A1 | legacy; PID law; pid_learnt | users' hand tuning | as A1 |
| A4 | Fine min speed (`profile_t`) | AR2209: 0.08 rev/s | user | nothing | legacy, pid_learnt (raised to the motor's least, A14). The glide and the PID law use the calibration's creep instead | users' hand tuning | **User**, as the creep. Before a calibration it is the floor. The calibration writes its creep into it on yes; the auto-tuner, while on, writes the creep it derives. Never under the motor's least speed (A14) (F.2, 3) |
| A5 | Fine max speed (`profile_t`) | AR2209: 2.0; profile 0: 5.0 rev/s | user | nothing | legacy, pid_learnt. The PID law takes the lowest of it, the motor's max (`charge_mode.cpp:583`) and, with the envelope on, C3. The glide uses C3 | users' hand tuning | **User**, as A2 for the fine tube (replaces C3) (F.2) |
| A6 | Coarse stop threshold (charge settings, one per unit) | 5 gr | user | nothing | legacy, pid_learnt. The glide and the PID law use the calibration's coarse room (D9) | the original firmware | **User**, kept: the coarse stop of a powder with no calibration. A calibrated powder stops by its measured coarse room, and the screens show which is in use (below, "Decided in the review", 1) (F.2, 4) |
| A7 | Fine stop threshold (charge settings) | 0.03 gr | user | nothing | legacy (stop and verdict). The staged loops take the band from A18's `mode` | the original firmware | **User**, the user's tolerance in every law: band = threshold − half a digit, the code's own conversion (finding 2). A18 and E14 go. Its least is in A20 (F.2) |
| A8 | Zero steadiness, SD (`set_point_sd_margin`) | 0.02 | user | nothing | every loop: the zero, the cup's removal (`:1230`), the steadiness after a pause, a series' start | the original firmware | **User**, unchanged. A scale's noise could be measured, but the setting works and is the user's |
| A9 | Zero mean (`set_point_mean_margin`) | 0.02 | user | nothing | every loop: the zero; the cup's removal test (E7) | the original firmware | **User**, unchanged; the cup's return is judged within it (E8) |
| A10 | Gate at the coarse stop (`coarse_stop_gate_ratio`) | 0 (−1 is off) | user | nothing | legacy only | PR history | **User.** The frame moves the gate at the coarse stop and waits its travel (issue #135, E10) (F.3) |
| A11 | Back-off: on, turns, speed | on, 0.25 turn, 1 rev/s | user | nothing | legacy, pid_learnt. Not the glide or the PID law | PR #108 | **User.** The frame makes it, and the forecast counts its spill as the calibration measured it at these turns and this speed (0.83 gr of AR2209 on the bench, 11-21). A back-off whose spill was not measured as it is now set is never forecast from another's: the coarse tube then stops by A6, and the screen says so (F.3, 3) |
| A12 | Precharge: on, time, speed | off, 1000 ms, 2 rev/s | user | nothing | legacy only | the original firmware | **User,** still a time at a speed. The frame makes it once the gate has closed (E10), and counts its powder, its turns by the coarse grains a turn, in the next throw's forecast (F.3) |
| A13 | Motor max speed (each motor) | 5 rev/s | user | nothing | every loop: the hard cap in `motor_set_target` | the original firmware: "before the stepper starts to lose step" | **User,** the hardware cap. The calibration's ladder never goes above it (F.2) |
| A14 | Motor least speed (each motor) | coarse 0.08, fine 0.01 rev/s | user | nothing | legacy and pid_learnt: the higher of it and the profile's. The PID law: the coarse tube only. The glide: neither tube (finding 5) | the original firmware | **User,** the hardware floor for every law and both tubes. The calibration's ladder starts at it. When it holds the creep up, the calibration's verdict says so in words. The comment is mended to what the code does (F.2) |
| A15 | Motor acceleration | 50 rev/s² | user | nothing | the motors' ramp, every loop. The forecast counts steps on the pin, so a ramp is counted | the original firmware | **User**, unchanged |
| A16 | Gear ratio, full steps, microsteps; the current and the sense resistor | 1.25 / 1.818, 200, 256; 400 mA, 110 mΩ | user | nothing | turns from steps, every loop; the driver's set-up | the hardware | **User**, unchanged (the current is ADR-012's) |
| A17 | Servo gate: fitted; each shutter's ends; the two speeds (`servo_gate.c:36`) | not fitted; shutter 0 closed 0.09, open 0.05; shutter 1 closed 0.05, open 0.09; open 5.0, close 3.0 travels a second | user | nothing | the gate's moves; legacy's partial close | the hardware | **User**, unchanged; E10's waits are made from them (F.3) |
| A18 | `controller` (staged, pid, pid_learnt), `mode` (accurate, fast): the charge settings' filler | staged, accurate | user (screen, web, API today) | nothing | read once at a charge's start (`:1333`): its loop and the staged band. pid_learnt reads the band at each throw | built for evaluation (gate 2 and the night's experiments) | **Remove** (sign-off item 1). The band comes from A7. Out of the API and the screens at F.5; its byte read as nothing and written as zero at R. Today's loop and the glide's pour stay reachable from the console only, never stored, for the comparisons the gates ask (C21) |
| A19 | The target (the weight form; the API; the console) | unset; 1 to 99999 steps of the decimal places (0.01 to 999.99, or 0.001 to 99.999); 0 refuses a start (`:1515`, `:1568`); the console takes up to 1000 | user | nothing | every loop | the original firmware | **User**, unchanged |
| A20 | The settings' own limits (`charge_mode_settings.c`, `profile.c`, `motors.c`, `servo_gate.c`) | coarse stop threshold 0 to 100; fine stop threshold 0 to 10; SD and mean margins 0 to 1; gate ratio 0 to 1; back-off 0.125 to 2 turns, 0 to 100 rev/s (0 is the least speed); precharge 0 to 5000 ms, 0 to 100 rev/s; gains at least 0; profile speeds 0 to 100 rev/s, least not above most; motor max 1 to 100, least 0 to 100 and not above max; acceleration 0.1 to 10000; gear 0.01 to 100; gate duty 0.02 to 0.13, speeds 0.01 to 100 | nobody: the tables | nothing | every write of a setting, from any side | the original firmware, ADR-003's tables | **User** limits, unchanged, with two checks at a charge's start, each refused in words (finding 14): a tolerance under a digit (the band would be under half a digit), and a profile whose top speed is 0 for a tube the throw needs. Both change what a unit does today: register row 12 (F.2) |
| A21 | The other profiles' defaults (`profile.c:16`) | profile 0, AR2208: coarse 0.025, 0, 0.3, 0.1 to 5; fine 2.0, 0, 10, 0.08 to 5. Profile 2, 8208XBR: coarse 0.05, 0, 0.3, 0.1 to 5; fine 2.0, 0, 12, 0.06 to 5. Profile 3, Benchmark2: coarse 0.06, 0, 0.3, 0.1 to 5; fine 0.8, 0, 15, 0.08 to 5. Profiles 4 to 7: every gain and speed 0 | user | as A1 | as A1 to A5 | the original firmware | **User**, unchanged (A20's check meets an empty profile) |

## B. The calibration record (one slot a powder profile, 13 to 20 K)

Measured today by the PC (`bench/trial.py` through the console) and written by `ccal`. Read by the glide and the PID law; pid_learnt reads none of it.

| # | Number | Default | Who modifies | Who calibrates | Who uses | Origin | Decided |
|---|---|---|---|---|---|---|---|
| B1 | The scale's digit | none (refused without a calibration) | `ccal` (console) | the PC tool, from readings | band edges, aim, rise, verdict (C, D) | measured | **Measured**, a property of the unit's scale, kept in the unit's record. Fallback on a unit never calibrated: the smallest step between two readings seen so far (a pour shows it within its first second; a steady zero does not) (R, F.4) |
| B2 | The scale's interval | none | `ccal` | the PC tool | the rules (D15) | measured; a second source exists live (finding 3) | **One source:** the scale's rhythm measured live. The record keeps it for the simulation only (F.4) |
| B3 | Grains a turn, coarse and fine | none | `ccal`; `series.py --coarse-gr-turn --fine-gr-turn` (hand, series D) | the PC tool, from runs at 1 rev/s from rest | the forecast, every rule | measured. Throws poured 3 to 9% more (coarse); the fine 0.83 a turn over a throw and 0.68 at its finish (progress.md, 11-22) | **Measured** where a throw uses it (gate 3's mend); corrected by the auto-tuner while on, within its bound, once gate 3 passes C13 (3) |
| B4 | Watch time | none | `ccal` | the PC tool: 1.5 × the slowest probe + 1 s, at least 3 s (F4) | the calibration's own waits | measured by rule | **Measured** (3) |
| B5 | Coarse share (the coarse figure's run-to-run spread) | none | `ccal` | the PC tool | the forecast's coarse margin (D21) | measured | **Measured** (3) |
| B6 | Stops, each tube (coarse 3, fine 6): speed, unseen powder, spread, quiet time, lag | none | `ccal`; `--lag-shift` (hand, −0.055 s on the bench record, finding 4) | the PC tool, at the ladder's speeds (F1, F2) | the forecast, the creep, the rest, the coarse room | measured, its lags corrected by hand | **Measured,** in the firmware's own time, across the user's speed range (A2, A5, A14). The hand shift goes with the PC tool (F.2 for the range, 3 for the job) |
| B7 | Small moves (3 sizes): turns, mean, most, wait | none | `ccal` | the PC tool (F3) | the top-ups read the turns, the mean and the wait (`charge_control.c:489`). No loop reads the most; it is stored and shown only | measured | **Measured.** The most is **Not control**: a measurement the calibration's verdict may cite (3) |
| B8 | First errors: the rehearsals', then carried from charges (newest 16) | none | `ccal`; the carry at a charge's end | the rehearsals (F5); the carry (E12) | where the stop is placed (`charge_control_place_stop`) | measured; rehearsals spread twice as widely as real throws' stops (progress.md, 11-22) | **Measured,** rehearsed as a throw's last stretch (3); the carry is the auto-tuner, while on |
| B9 | The band and the finish stored with it | 0.02 gr, glide | `ccal` | nothing | rules made for that band (re-made for another band at a charge's start) | chosen by the PC tool's run | **Remove** from the record: the band is A7's, the finish is the glide's alone (C6) (R) |
| B10 | Made by the rules, not stored: creep, stop room, aim, glide room, glide time, coarse room, coarse glide time, rest | made at each charge's start | nobody | made from B with C and D | the throw | rules | stay made by rule at a charge's start, from the measurements and the user's tolerance; each rule's literals are section D. They are shown where this unit's data is read; what a user or an agent changes is the measurements they are made from (4) |
| B11 | The record's sizes: 4 coarse stops, 8 fine stops, 3 moves, 16 first errors kept (`charge_control.h:41`); `charge_control_place_stop` takes at most 32 | | nobody | nothing | the record's layout | the build | **Not control.** A ladder cut by the user's range (F1, F2) stays within them; the unreached 32 becomes the kept 16 (R) |
| B12 | What a record must hold before a throw may use it: at least 1 coarse stop and 3 fine, exactly 3 moves; the digit, the interval, both grains a turn and the band above 0; speeds rising (`charge_calibration.c:74`) | | nobody | nothing | every charge's start | the build (fail early) | **Rule.** The band's part goes with B9 (R) |

## C. The frame's constants (`charge_control_default_constants`, `charge_control.c:40`)

The same on every unit, never stored, changed live only by the console's `ccon` until a restart. **None is calibrated.**

| # | Number | Default | Who modifies | Who calibrates | Who uses | Origin | Decided |
|---|---|---|---|---|---|---|---|
| C1 | `move_rps`, a small move's speed | 0.2 rev/s | `ccon` | nothing | the top-ups' small moves | `staged.py`: "as the bench's watched nudges were" | **Measured:** the speed the calibration made its moves at (B7), read from the record, so the moves are made as they were measured. The calibration does not take the bench's 0.2 either: it makes its moves at a speed from the unit's own measured ladder (3) |
| C2 | `parcel_s` | 0.01 s | `ccon` | nothing | the forecast's counting step | `staged.py` | **Not control** (a counting step), with C23 |
| C3 | `fine_top_rps` | 0.5 rev/s (1.0 by hand in every night series) | `ccon`; `series.py` | nothing | the glide's top; the PID law's envelope; the fine learning's window; the jog's floor cap (`:133`); the offer's fine max (`charge_calibration.c:750`) | the simulation's tube (finding 1) | **Remove.** The top is the lowest of A5, A13 and the fastest fine speed the calibration measured (F.2) |
| C4 | `coarse_top_rps` | 2.0 rev/s | `ccon` | nothing | the glide; the envelope; the offer's coarse max | the ladder's top; no reason found | **Remove,** as C3 with A2 (F.2) |
| C5 | `coarse_floor_rps` | 0.3 rev/s | `ccon` | nothing | the glide; the envelope; the offer's coarse min | one bench observation: "pours in lumps below it" (2026-11-20) | **Remove.** The floor is the higher of A2's and A14's least speeds. The calibration measures a stop there, and its spread is in the forecast's coarse margin (F.2) |
| C6 | `dribble_digits_s` | 8 digits/s | `ccon` | nothing | the jog finish's dribble only | the bench: the slowest even flow, AR2209 | **Remove** with the jog finish. The bench trial of 11-21 judged it (jog 56% against glide 68%), and one controller has one finish (F.5) |
| C7 | `coarse_spreads` | 3 | `ccon` | nothing | the forecast's margin for the coarse powder still unseen (D21) | `staged.py`; no study found | **Rule**, a trade of time at its present value (study, group 3; "Decided in the review", 4) |
| C8 | `creep_band_share` | 1.0 | `ccon` | nothing | the creep: the powder in the air at the stop as a share of the band | `staged.py`: half was tried, 1.5 to 2 s slower and no more good throws (progress.md, 2026-11-21) | **Rule.** Its reason: the powder still in the air at the stop is no more than the band, so a stop's own tail cannot carry it out of the band. Support: no effect beyond chance at half or twice (study, group 2) |
| C9 | `creep_most_s` | 6 s | `ccon` | nothing | the creep is never so slow that its stretch takes longer than this | `staged.py`; no study found | **Rule**: lowered, it costs overs on late scales (study, group 4) |
| C10 | `learn_share` | 0.15 | `ccon` | nothing | the stop's learning: the share of each first error taken in | the simulation; a mean-based learning tried and rejected on the rig (progress.md, 11-22) | **Rule**: no effect beyond chance at half or twice (study, group 2); the auto-tuner's own rate |
| C11 | `learn_most_digits` | 2 | `ccon` | nothing | the most a throw teaches | `staged.py` | **Safety** of the learning: a bound |
| C12 | `learn_dead_digits` | 0.25 | `ccon` | nothing | the least a throw teaches | `staged.py` | **Rule** (below a quarter digit is the reading's own rounding) |
| C13 | `learn_fine_prior` | 3 (on) | `ccon`; set to 0 by hand in the night series | nothing | the fine figure's learning: how many throws the calibration's figure counts for | 11-22 day; judged untrustworthy (finding 7) | **Off in the build until gate 3 has measured the fine tube's lag across the user's range.** Gate 3's study, its pass written first, then says whether it is part of the auto-tuner. Its switch is the auto-tuner's, never a constant ("Decided in the review", 2) (N.2 for the default, 3) |
| C14 | `learn_fine_most` | 0.25 (±25%) | `ccon` | nothing | the fine figure's bound | 11-22 | **Safety** of the learning: a bound |
| C15 | `learn_fine_split` | 1 (on) | `ccon` | nothing | one fine figure, or a stream's and a finish's | 11-22: no measurable gain on the rig (16:37) | **Remove** the switch. One figure or two is decided with C13, by the same study (3) |
| C16 | `still_least_s` | 0.4 s | `ccon` | nothing | at rest: no new high for this long, or two intervals if longer (D15) | `research/proposals_2026-11-21.md` | A time chosen on the bench's scale: it fails the test. Tried at nothing (N.3): without effect it goes, and D15's two intervals stand alone. At half and twice it had no effect beyond chance (study, group 2) |
| C17 | `most_moves` | 8 | `ccon` | nothing | tries a throw gets after its first rest | `staged.py` | **Safety:** a bound on a throw |
| C18 | `throw_most_s` | 120 s | `ccon` | nothing | a throw past it ends "limit" | `staged.py` | **Safety** |
| C19 | `fine_most_turns` | 15 turns | `ccon` | nothing | a throw past it ends "limit" | `staged.py` | **Safety.** In turns it means another weight on another tube, and a tube that pours little a turn would end a true throw. Its form is decided with the no-powder check, which asks the same question ("Decided in the review", 6) (F.1) |
| C20 | `over_most_gr` | 3 gr | `ccon` | nothing | a reading this far over stops everything | `staged.py` | **Safety,** in the user's own terms: 100 times the tolerance (A7), which is 3 at the default 0.03 and in the scale's unit whatever that is. Not grains (a gram scale reads 3 as 46 gr), and not a count of digits (a digit is the A&D's 0.02 gr only on an A&D) (F.1) |
| C21 | `speed_law` | 0 (glide) | `ccon` | nothing | which law pours | the night's experiment | **Remove** as a choice: the PID law pours (README). The glide's law stays in the code, checked throw for throw in CI, and runs in shadow in the recordings. Its pour stays on the console only, never stored, for the comparisons gates F, 3 and 7 ask (F.5) |
| C22 | `pid_envelope` | 1 (on) | `ccon` | nothing | the guard on the PID law | the night's experiment | **Remove** the switch; the envelope is always on (F.5) |
| C23 | `CHARGE_CONTROL_PARCELS_MAX` (`charge_control.h:48`) | 256 parcels a tube, 2.56 s at C2 | nobody | nothing | the powder on its way, glide and PID law; past it the parcels merge and the throw stops earlier | the build (finding 13) | **Not control,** on one condition: the window covers the longest lag in the record, and a lag beyond it is said, never cut silently. The slowest stand-in scale is its case (F.4) |

## D. Literals inside the rules (`charge_control.c`, `charge_calibration.c`, `charge_mode.cpp`)

None is a constant anyone can see or set; each is a number written into a formula.

| # | Rule (where) | Literal | Origin | Decided |
|---|---|---|---|---|
| D1 | The slowest fine stop's lag is taken as at least 0.3 s (`:124`) | 0.3 s | none found | **Remove**: no effect at half or twice in 40 cases (study, group 1) (F.5) |
| D2 | The creep is never under half the slowest stop's speed (`:126`) | 0.5 | none found: keeps the creep near what was measured | **Rule**, a trade of time (study, group 3) |
| D3 | The creep is never above the third fine stop's speed (`:127`, index 2) | the third stop | none found | **Rule** in words, "within the measured ladder"; the index goes: no effect at the stop before or after (study, group 1) (F.5) |
| D4 | The aim: a digit under the target, or half the band if more (`:129`) | 1 digit, ½ band | `proposals_2026-11-21.md`, section 2 | **Rule** |
| D5 | The jog's stop room: 2.5 spreads and a digit (`:134`) | 2.5 | jog only | **Remove** with the jog (C6) (F.5) |
| D6 | The glide room: the stop room plus twice the spread of a stop from four times the creep (`:141`) | 2, 4 | `proposals_2026-11-21.md`: the creep then lasts about 3 s | **Rule**: at half it costs overs, at twice time (study, groups 3 and 4) |
| D7 | The glide time: 0.6 × the longest lag of the fine stops from the third to the fastest, between 0.4 and 2.0 s (`:148`, `:151`) | 0.6, 0.4, 2.0, from the third stop | `proposals_2026-11-21.md`, "between set limits"; no study of the limits found | 0.6: **Rule** (study, group 4: at half, overs up to +11 points). 0.4 s: **Remove** (group 1). 2.0 s: **Remove**: it acts only on the slowest scale, where doubling it changed nothing and halving it cost overs (F.5) |
| D8 | The coarse glide time: 0.6 × the coarse lag at its floor, between 0.3 and 1.5 s (`:152`) | 0.6, 0.3, 1.5 | as D7 | 0.6: as D7. 0.3 s: **Remove** (group 1). 1.5 s: **Remove**: it never acts, and halving it cost overs (F.5) |
| D9 | The coarse room: 3 spreads of a coarse stop from its floor + the glide room + 0.2 gr (`:153`) | 3, 0.2 gr | `proposals_2026-11-21.md` | 3: **Rule**, a trade of time (study, group 3). 0.2 gr: a weight chosen on the bench's scale, so it fails the test (a gram scale reads it as 3 gr). Tried at nothing (N.3): without effect it goes; with one, it is made from a spread the unit measured (N.3, F.1) |
| D10 | The rest: the slowest quiet time of stops within twice the creep + 0.3 s (`:157`, `:161`) | 2, 0.3 s | `proposals_2026-11-21.md` | The 2: **Rule**. The 0.3 s: a time chosen on the bench's scale; tried at nothing (N.3): without effect it goes, with one it is counted in reading intervals. At half and twice neither had an effect beyond chance (study, group 2) |
| D11 | The jog's aim: two SDs of the rehearsals and a digit (`:192`) | 2 | jog only | **Remove** with the jog (F.5) |
| D12 | The coarse tube runs only if the target is more than the coarse room and half a coarse turn (`:226`, `:659`) | ½ turn | none found | **Rule**: at half no effect, at twice up to 15 s slower (study, group 3) |
| D13 | The fine learning's window opens after the coarse tube's quiet time + 0.3 s (`:518`) | 0.3 s | none found | with C13 (3) |
| D14 | A rise is half a digit (`:610`) | ½ digit | the digit grid | **Rule** |
| D15 | At rest: no new high for C16, or two intervals if longer (`:647`) | 2 intervals | `proposals_2026-11-21.md` | **Rule**: no effect beyond chance at half or twice (study, group 2) |
| D16 | The band's edge is a quarter digit past the band (`:32`, `:669`; also `charge_pid_learnt.c:8`) | ¼ digit | a float fault mended on 11-22 (10.02 over a 60 gr cup) | **Rule** (numerical tolerance), one definition, not two (N.2) |
| D17 | The fine learning ignores a stretch under 0.2 turns, a stream under 0.5 (`:33`, `:34`) | 0.2, 0.5 turn | `staged.py` | with C13 (3) |
| D18 | Two times within 0.1 ms are the same time (`:37`) | 1e-4 s | lockstep with the Python reference | **Not control** |
| D19 | The offer: coarse kp = 1 / (grains a turn × coarse glide time); fine kp = 0.6 / (grains a turn × glide time); ki, kd 0 (`charge_calibration.c:741`, `:746`) | 1, 0.6 | the coarse from the glide's own law; the fine 0.6 chosen in the simulation on this rig's two records; on the rig 2 to 3 s quicker with more overs (README) | **Rule** for the form. The fine factor is not a constant: gate 3 measures each unit's trade between approach time and stop spread and makes the factor from the user's tolerance and the over budget of one in fifteen (a Rule, the owner's figure). Until gate 3 no offer is made to a user (3) |
| D20 | The staged verdict's threshold: the band and half a digit (`charge_mode.cpp:701`, `:850`) | ½ digit | the firmware's verdict rule (over or under from 0.03 gr) | **Rule**; it is A7's conversion |
| D21 | The forecast's coarse margin: C7 × B5 × the coarse powder unseen (`charge_control.c:365`) | (C7) | | with C7 |
| D22 | The top-up: the largest move whose mean is at most half of the short and the band together (`:489`) | ½ | `proposals_2026-11-21.md`; added by the audit | **Rule** |
| D23 | The fine tube's stop is only a pause while a digit or more of coarse powder is still unseen (`:422`) | 1 digit | `staged.py`; added by the audit | **Rule** |
| D24 | A parcel is counted from the middle of its stretch (`:309`) | ½ | counting; added by the audit | **Not control** |
| D25 | A small move under half a step makes no step and is reported done; a move's speed is held to the motor's max, not its least (`motors.c:982`, `:987`) | ½ step | the motors' task (gate 2); added by the audit | **Not control** for the half step. A move's speed is C1's, within A13 and A14 as every speed (F.2) |

## E. The charge's own fixed numbers (`charge_mode.cpp`, `charge_mode.h`, `charge_calibration.h`)

| # | Number (where) | Value | Who uses | Origin | Decided |
|---|---|---|---|---|---|
| E1 | No reading for this many usual intervals: the tubes pause (`CHARGE_STALE_INTERVALS`) | 3 | every loop | ADR-013, approved | **Safety** (ADR-013) |
| E2 | This many, and at least 1 s: the scale is lost (`CHARGE_LOST_INTERVALS`, `_MIN_MS`) | 10, 1000 ms | every loop | ADR-013 | **Safety** (ADR-013) |
| E3 | The rhythm not yet known: a G&G's (`CHARGE_USUAL_INTERVAL_UNKNOWN_MS`) | 250 ms | every loop | ADR-013 | **Safety** (ADR-013), with E24 |
| E4 | A third pause in one throw ends it (`CHARGE_PAUSES_BORNE`) | 2 | every loop | ADR-013 | **Safety** (ADR-013) |
| E5 | Steady after a pause: 10 readings 300 ms apart, unsteady after twice that (`CHARGE_STEADY_*`) | 10, 300 ms | every loop | ADR-013 | **Safety** (ADR-013) |
| E6 | The zero: 10 readings, each waited for up to 300 ms (`:319`, `:376`; a comment says 200 ms) | 10, 300 ms | every loop | the original firmware | **Rule;** the comment is mended (N.2) |
| E7 | The cup lifted: the mean + 10 under the mean margin (`:1231`), over 5 readings 300 ms apart (E19) | 10 (the scale's unit) | every loop, after the verdict. No loop looks for a cup lifted during a throw | the original firmware; known faults: a cup under 10 is never seen to go (README, handed on by ADR-012 and ADR-003), and a cup lifted mid-throw is poured on (`robust_staged.py`, `cup_lifted`) | **Safety,** mended, both tests in one step: its form is decided there by its cases ("Decided in the review", 7) (F.1) |
| E8 | The cup back: the weight ≥ 0 exactly, on one reading (`:1283`) | 0 | every loop | known fault: a cup back a hair light is never seen (README, ADR-003) | **Rule,** mended: within the zero's mean margin (A9) (F.1) |
| E9 | A second's wait before the verdict's event (`:1135`) | 1000 ms | every loop, the staged ones after their own settled verdict; between a series' throws (finding 10) | the original firmware: today's loop reads its verdict there | **Remove** for the frame: its verdict is a settled reading, given at once. api_v1.md's "a second after" changes with it (register row 14). Today's loop keeps it until that loop goes (F.5) |
| E10 | The gate: opened at a throw's start without waiting (`:890`); closed at its end, waited for (about 0.33 s at the default speed); the precharge 500 ms after the close (`:1113`); 20 ms for other tasks (`:1122`) | 500 ms, 20 ms | every loop opens and closes; the precharge is legacy's | issue #135 asks for a delay | **Rule:** a gate's move is waited for by its own travel (A17's ends and speeds), and after a close 500 ms more to settle (today's figure and issue #135's; a Rule until a gate is fitted and it is measured). The tubes start when the gate is open. The 20 ms goes with today's loop (F.3) |
| E11 | The back-off's longest (`COARSE_STOP_BACKOFF_MAX_MS`; a copy as 15.0 s in `charge_pid_learnt.c:162`) | 15 s | legacy, pid_learnt | "safety cap" | **Safety,** one definition (F.3) |
| E12 | The carry: at least 5 throws at rest; of a charge's first errors (32 kept in RAM) the newest 16 are merged and kept; at most 3 digits at once, but a first carry into an empty list is taken whole (`CHARGE_CARRY_*`, `charge_calibration.c:270`) | 5, 32, 16, 3 digits | the glide and the PID law, at a charge's end | 11-22 (b23b214), a temporary form | **Safety** of the learning (bounds), part of the auto-tuner; its switch is the auto-tuner's. A first carry has no stored stop to bound it against, and that is said in its rule (R) |
| E13 | A series ends when the cup would pass this (`CHARGE_SERIES_CUP_MOST`) | 600 gr | a series, started from the console only | the bench's cup | **Not control:** the bench's tool, with E22. What pours for a user without a cup lifted, the calibration and a test throw, is bounded by the cup the user declares (api_v1.md, "The cup") ("Decided in the review", 3) (3) |
| E14 | The bands of the debug mode (`CHARGE_TRICKLE_*_BAND_GR`) | 0.02, 0.04 gr | the staged loops, pid_learnt (`:757`), `ctune` (`charge_calibration.c:771`) | evaluation | **Remove** (A7) (F.2) |
| E15 | A throw's recording, rows kept (`CHARGE_RECORD_ROWS`) | 800 on the RP2350, 1 on the Pico W (`:134`) | the record | RAM | **Not control,** but finding 11: the Pico W's rows are found by measurement, and a throw longer than its rows is thinned, not dropped (2R, 7) |
| E16 | The loops' clock: the staged and pid_learnt loops and the motors' task run every millisecond (`:684`, `:843`; `motors.c:552`) | 1 ms | the staged loops; every ramp | gate 2 | **Not control:** the controller takes the time as an input |
| E17 | A driver's fault: looked at every 100 ms in the staged loops (`:608`, `:786`), at each reading in today's; the drivers polled every second, a flag counted at its third read, absent after 3 missed polls (`motors.c:42`) | 100 ms; 1 s, 3, 3 | every loop | ADR-012 | **Safety** (ADR-012) |
| E18 | Today's loop waits up to 200 ms for a reading (`:982`) | 200 ms | legacy | the original firmware | **Remove** with today's loop (6) |
| E19 | The cup's removal: 5 readings, each waited up to 200 ms, 300 ms apart (`:1132`, `:1222`, `:1237`) | 5, 200 ms, 300 ms | every loop | the original firmware | **Rule,** with E7 (F.1) |
| E20 | The cup's return: one reading, waited up to 200 ms, looked for every 20 ms (`:1278`, `:1288`); a buffer of 5 at `:1261` is never used | 200 ms, 20 ms | every loop | the original firmware | **Rule,** with E8; the unused buffer goes (F.1) |
| E21 | A fault's acknowledgement is looked for every 50 ms (`:501`) | 50 ms | every loop | gate 2 | **Not control** |
| E22 | A series: 1 to 500 throws, from the console (`remote_control.c:713`) | 500 | the bench | gate 2 | **Not control** (the bench's). Its leak into the next charge (finding 12) is mended (N.2) |
| E23 | A recording's row holds a speed between 0 and 65 rev/s (`:536`) | 65 | the record | the row's packing | **Not control.** A profile allows 100 rev/s (A20): a speed above 65 is recorded wrongly, and the row's packing is settled with the recordings (2R) |
| E24 | The scale's usual interval: the median of the newest 16, unknown until 4 are kept (`scale.c:534`, `scale.h:94`) | 16, 4 | every loop's stale and lost limits; the frame's interval (B2) | ADR-013 | **Safety** (ADR-013) |
| E25 | The motors' task: a rate under 0.05 steps a second is a stop; a shortest step period caps the rate (`step_pulses.c:16`, `:187`) | 0.05 steps/s | every loop | ADR-012 | **Not control** (the motors') |

## F. The calibration procedure (`tools/charge_sim/staged.py`, `bench/trial.py`; the job gate 3 builds on the unit)

| # | Number | Value | Origin | Decided |
|---|---|---|---|---|
| F1 | The fine stops' flows, counts, run lengths (`FINE_RATES_DIGITS_S`) | 1, 2, 4, 8, 16, 32 digits a second; 8, 8, 4, 4, 4, 4 stops; 5 to 3 s | the research's design, in digits so the ladder follows the scale | **Rule** for the form. The ladder is cut by the user's range (A5, A13, A14) instead of F7, and reaches the user's fine max (F.2 in the reference and the PC's trial, 3 in the job) |
| F2 | The coarse stops (`COARSE_STOPS`) | 0.3, 1.0, 2.0 rev/s; 3, 2, 2 stops | the bench's coarse runs | **Rule** for the form; the speeds from A2, A13 and A14 instead of fixed (F.2, 3) |
| F3 | The small moves (`MOVES_DIGITS`) | 0.7, 2.5, 6 digits; 24, 12, 8 moves | the research's design | **Rule** |
| F4 | The probes: 4 moves watched 15 s; then every watch 1.5 × the slowest + 1 s, at least 3 s | 4, 15, 1.5, 1, 3 | the research's design | **Rule.** The 15 s is a general assumption, longer than the slowest response reported of a supported scale (about 5 s); after the probes every watch is made from what this unit's scale showed |
| F5 | The rehearsals (`REHEARSALS`, `REHEARSAL_GR`) | 10 of 1 gr | the research's design; spread twice the throws' (progress.md, 11-22) | **Rule,** mended: 40 or more, each a throw's last stretch. The 1 gr goes with it: a rehearsal's size is the coarse room's (3) |
| F6 | The prime: 1 coarse turn, 3 fine turns at 1 rev/s, until two runs pour the same | 1, 3 turns, 1 rev/s | the research's design | **Rule** for the turns; the speed is one inside the user's own range, not a fixed 1 rev/s; "the same" is written as a tolerance in digits with the job (3) |
| F7 | The ladder's ends (`FINE_TOP_RPS`, `COARSE_TOP_RPS`, `COARSE_FLOOR_RPS`) | 0.5, 2.0, 0.3 rev/s | the simulation's tube (finding 1) | **Remove** (A2, A5, A13, A14) (F.2) |
| F8 | The PC's lag shift (`--lag-shift`) | −0.055 s | a hand correction for the PC tool's timing (finding 4) | **Remove** with the PC tool: the job runs on the unit. Until then it is the PC tool's own correction, declared with every record it makes (3) |
| F9 | The PC trial's watch after a verdict (`WATCH_AFTER_S`) | 4 s | the PC trial | **Not control** (a PC tool's) |

## G. The experiments, to fold or remove (`charge_pid_learnt.c`, the 21 K record, `ctune`)

| # | Number | Value | Origin | Decided |
|---|---|---|---|---|
| G1 | `lag_s`, `rest_s` | 0.9 s, 2.5 s | the A&D's, by hand | **Measured** (B6), kept in the unit's record once any powder is calibrated on it, so a new powder on that unit starts from its own scale's numbers. On a unit never calibrated the default is a general assumption on the slow side, so an unknown scale errs short; its value is found on every stand-in scale, never taken from the A&D. No bar is set for a charge before a calibration (sign-off item 10); the fallback study says how far off the A&D's numbers were on other scales (F.4) |
| G2 | `gr_turn_prior` | 2.0 | raised from 1.0 after this rig's dry throws (`charge_pid_learnt.c:16`) | **Remove** as a number: it is the bench's, and a weight in grains. The figure starts from what the throw itself shows, the weight risen for the turns made, and until then the frame forecasts nothing, as today's loop does. If the study finds that is not enough, a start value is a new parameter for the owner ("Decided in the review", 5) (F.4) |
| G3 | `floor_least_rps`, `coarse_quiet_s`, `coarse_lag_s` | 0.02 rev/s, 1.0 s, 0.6 s | guesses | **Remove** with pid_learnt; A14 and B6 cover them (F.4) |
| G4 | `digit_gr` | 0.02 | the A&D's | **Measured** (B1) (F.4) |
| G5 | `learn_share`, `throw_most_s`, `over_most_gr`, `most_tops`, `BAND_EDGE_DIGITS` | 0.15, 120 s, 3 gr, 8, 0.25 | copies of C and D | **Remove** the copies; one definition each (F.4) |
| G6 | `floor_band_share`, `coarse_gr_turn`, `coarse_margin_share` | 0, 0, 0 | switches of the night's experiments | **Remove** (F.4) |
| G7 | The PID law's carried stops (EEPROM 21 K) | a list a profile | 11-22 night, a temporary form | **Measured** (B8), moved into the powder's record on the unit. One law pours, so one list. The 21 K slot is freed (R) |
| G8 | pid_learnt's own copies as literals: the aim (D4, `:52`), the still time (C16 and D15, `:54`), a rise (D14, `:309`), the back-off's cap (E11, `:162`) | as their rows | copies; added by the audit | **Remove** the copies (F.4) |
| G9 | pid_learnt's learning: a stretch under 0.01 turn teaches nothing (`LEARN_LEAST_TURNS`); a throw's figure is held between half and twice the learnt one (`:272`) | 0.01 turn; 0.5, 2 | the night's experiment; added by the audit | **Safety** of the learning (bounds), kept with the learnt source when it is folded in, one definition with C14's (F.4) |
| G10 | pid_learnt's window: 128 steps of 0.01 s; a lag above 1.28 s is cut silently (`charge_pid_learnt.h:28`) | 1.28 s | the build; added by the audit (finding 13) | **Remove** with pid_learnt: the learnt source uses the frame's parcels (C23) (F.4) |

## H. Test only: the simulated scale's calibration (`charge_calibration.c:109` to `:139`, `scale.h:128`, `simulated_scale.c`)

The speeds 0.3, 1, 2 and 0.025 to 0.5 rev/s; moves of 0.7, 2.5 and 6 digits; a lag of half an interval; a watch of 1 s; a coarse share of 0.01; a band of 0.02. Added by the audit: a coarse spread of a digit and a fine one of half a digit; a quiet time of the lag and one interval; a move's most its mean and a digit; a move's wait the lag and two intervals; the simulated scale's digit 0.02, its 10.0 and 0.8 gr a turn, its interval 100 ms and its rate of 1 to 50 readings a second. Used only with `sim on`, never with a real scale. **Not control.** They follow the frame when it changes, so `charge_cycle` still runs.

## Decided in the review of 2026-11-23

The page of 2026-11-22 ended with four decisions "for the owner" and three items "still to research". Each is settled here from the record. Where one changes what a setting does, it is a row of the README's register, and the owner's approval of that row is the only word asked.

1. **The coarse stop threshold (A6) is kept, and a calibrated powder uses its measured room.**
   - **Why both:** the setting is one for the unit and the measured room is one a powder. On the rig the user's coarse settings, the 5 gr stop against the measured 1.44 gr among them, cost 4.5 s a throw, all of it in the bulk (progress.md, 11-22, 19:40).
   - **Why the calibration does not write the setting,** as it writes the PID values: one number for the unit, written from one powder's calibration, is wrong for the next powder. Too little room is an over.
   - **Why the setting is not a least room:** at the default 5 gr every calibrated powder would lose those 4.5 s.
   - **So that nothing is hidden:** the screen and the web show the room in use beside the setting when the powder is calibrated (gate 4's mocks).
   - **Handed on to ADR-014,** which settles which profile the thresholds belong to: a threshold a powder would end the difference.
2. **The fine figure's learning (C13, C15, D13, D17) is off until gate 3 measures what it reads through.**
   - The record decides it: every night series ran with it off; on, it hit its −25% bound and read 0.93 gr a turn where the throws poured 0.83 (progress.md, 11-22, 17:17); two fine figures gave no measurable gain (16:37).
   - The cause is finding 1: the calibration measured nothing above 0.5 rev/s. With the ladder reaching the user's fine max, the cause goes.
   - It stays in ADR-009: sign-off item 4 names the grains a turn among what the auto-tuner corrects. Gate 3 runs the study on its own measurements and says on or not, and one figure or two.
3. **The series' 600 gr (E13) is the bench's, not a user's.**
   - A series is started from the console only (`charge <gr> series <n>`); no screen, page or API path reaches it. It stays a bench tool with its bound.
   - What a user runs into one cup, the calibration and a test throw, is bounded by the cup the user declares at the unit, with its capacity (api_v1.md, "The cup", reserved for ADR-009).
4. **The spread multipliers (C7, D9, D6) stay Rules at their present values; they are not made from an over budget.**
   - **What the unit measures is already in them:** each margin is the multiplier times a spread the calibration measured on that unit. The multiplier is the rule; the spread is the unit's.
   - **The study:** they trade time, with accuracy within the noise (group 3). Lowered, D6 costs overs.
   - **A confidence made from a budget would be false precision.** It needs the spread to be normal. The rig's overs are clumps, and its stops spread 0.046 gr where the calibration measured 0.028 (progress.md, 11-22).
   - **It would add a setting nobody asked for.** One over in fifteen is ADR-009's bar (sign-off item 11), not a user's setting. What a user trades speed against accuracy with is the tolerance (A7).
   - The one place the budget enters is the offer's fine factor (D19), from each unit's own measured trade.
5. **The starting figure (G2) is not kept as a number** (corrected the same evening, "The rows against the test": the afternoon's decision had kept the bench's 2.0 as a rule).
   - The fallback study showed it only makes up for a lag that is wrong, and its value was set after the bench's own dry throws.
   - The figure starts from what the throw itself shows. F.4's study on every stand-in scale, its pass written first, says whether that is enough.
   - Nothing waits for it: no bar is set for a charge before a calibration.
6. **The 15 fine turns (C19) are decided with the no-powder check, in gate F's first step.**
   - Both answer one question: the tube turns and the powder does not come.
   - The no-powder check is weight-true: the forecast holds powder that has not shown for longer than its lag.
   - What is left for C19 is a last bound on the motor's work. Its form is a multiple of the turns the forecast says the throw needs, so a tube that pours little a turn is not cut off.
   - **Its cases, written before the build:** `no_powder`, and a tube whose grains a turn are a third of the bench's.
7. **The cup tests (E7, E8) are mended in gate F's first step.**
   - **The return (E8)** is within the zero's mean margin (A9), not exactly zero.
   - **The cup lifted mid-throw** is a reading that falls below the throw's base by more than its noise: both tubes stop at once (the owner's sign-off condition).
   - **The removal after the verdict (E7)** loses its fixed 10.
   - **Their cases, written before the build:** `cup_lifted`, a cup lighter than 10 of the scale's unit, a scale set to grams, a scale that drifts a digit during a throw (no false stop).

## The research behind the rows

The research run for gate N on 2026-11-22 and 2026-11-23, kept as it was written, each study with its pass before its run. Where it says "proposed" or leaves a choice, the rows above and "Decided in the review" are the outcome.
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
  - **Run 2026-11-22, 23:24, to 2026-11-23, 01:58** (`tools/charge_sim/fit/study_20261122/numbers.txt`; 920 cases of 800 throws).
  - **By the pass's letter, four numbers are Rules:** C12, D1, D3 and D7's 0.4 s. The other 19 "move" somewhere.
  - **The pass was too strict to tell chance from a real effect, and that is my error.** Each number is judged in 40 cases on three measures, about 120 tests a number, each at two SDs. Chance alone moves about one test in twenty, so several "moves" a number were to be expected with no effect at all. The run shows it:
    - more margin sometimes gave more overs (D6 at twice: +5.0 points on one case);
    - the baseline's own second seed moved its median up to 2.8 s on the slowest scale.

    The letter's verdict stands as written. Below, the same results read by how often and how consistently a number moved.
  - **Each value tried, in 20 cases (both powders, five scales, both bands)**: the cases where good, over and median moved past their bars, and the mean moves.

    | Row | Number | Tried | Good moved | Over moved | Median moved | Mean: good, over, median | Group |
    |---|---|---|---|---|---|---|---|
    | C12 | `LEARN_DEAD_DIGITS` | 0.125, 0.5 | 0, 0 | 0, 0 | 0, 0 | 0, 0, 0 | 1 |
    | D1 | `SLOWEST_LAG_LEAST_S` | 0.15, 0.6 | 0, 0 | 0, 0 | 0, 0 | 0, 0, 0 | 1 |
    | D3 | `CREEP_TOP_STOP` | 1, 3 | 0, 0 | 0, 0 | 0, 0 | 0, 0, 0 | 1 |
    | D7 | `GLIDE_LEAST_S` | 0.2, 0.8 | 0, 1 | 0, 1 | 0, 0 | 0, 0, 0 | 1 |
    | D8 | `COARSE_GLIDE_LEAST_S` | 0.15, 0.6 | 0, 1 | 0, 1 | 0, 2 | 0, 0, 0 | 1 |
    | C10 | `LEARN_SHARE` | 0.075, 0.3 | 0, 1 | 1, 1 | 2, 1 | −0.3, +0.5, +0.2 s at most | 2 |
    | C11 | `LEARN_MOST_DIGITS` | 1, 4 | 0, 0 | 0, 0 | 2, 2 | −0.4, +0.3, +0.2 s at most | 2 |
    | C16 | `STILL_LEAST_S` | 0.2, 0.8 | 0, 0 | 0, 0 | 0, 1 | 0, +0.1, +0.1 s | 2 |
    | D10 | `REST_FLOOR_TIMES` | 1, 4 | 0, 0 | 0, 0 | 1, 1 | ±0.2, ±0.2 s | 2 |
    | D15 | `STILL_INTERVALS` | 1, 4 | 0, 1 | 0, 1 | 1, 1 | ±0.2, +0.3 s | 2 |
    | D6 | `GLIDE_FLOW_TIMES` | 2, 8 | 1, 1 | 1, 1 | 2, 0 | ±0.5, 0 | 2 |
    | C8 | `CREEP_BAND_SHARE` | 0.5, 2 | 3, 2 | 3, 2 | 1, 6 | +0.4 over, −0.3 s at twice | 2 |
    | C7 | `COARSE_SPREADS` | 1.5, 6 | 3, 1 | 2, 1 | 8, 9 | −0.3 s at half; +1.4 s at twice (up to +4.4) | 3 |
    | D9 | `COARSE_ROOM_SPREADS` | 1.5, 6 | 1, 0 | 1, 0 | 8, 16 | −0.4 s at half; +1.0 s at twice | 3 |
    | D9 | `COARSE_ROOM_EXTRA_GR` | 0.1, 0.4 | 1, 1 | 1, 1 | 2, 11 | +0.5 s at twice | 3 |
    | D2 | `CREEP_LEAST_SHARE` | 0.25, 1 | 1, 1 | 1, 2 | 0, 8 | −0.3 s at twice | 3 |
    | D6 | `GLIDE_SPREADS` | 1, 4 | 3, 0 | 3, 1 | 3, 9 | +1.0 over at half (up to +8.1) | 3, 4 |
    | D12 | `COARSE_LEAST_TURNS` | 0.25, 1 | 0, 0 | 0, 0 | 0, 6 | +3.3 s at twice (up to +14.8) | 3 |
    | D7 | `GLIDE_LAG_SHARE` | 0.3, 1.2 | 5, 1 | 5, 1 | 18, 10 | at half: −1.7, +2.0, −0.5 s (over up to +11.4) | 4 |
    | C9 | `CREEP_MOST_S` | 3, 12 | 4, 0 | 4, 0 | 7, 1 | at half: +0.8 over (up to +5.9), −0.6 s | 4 |
    | D7 | `GLIDE_MOST_S` | 1, 4 | 3, 0 | 4, 0 | 0, 0 | at half: +0.9 over (up to +5.6, the slowest scale) | 4 |
    | D8 | `COARSE_GLIDE_MOST_S` | 0.75, 3 | 1, 0 | 1, 0 | 4, 0 | at half: over up to +7.1 (the slowest scale) | 4 |
    | D10 | `REST_EXTRA_S` | 0.15, 0.6 | 0, 3 | 2, 2 | 0, 3 | ±0.3 over, +0.3 s at twice | 2 |

  - **The groups:**
    1. **No effect over a fourfold range.** A guard that does nothing is a number nobody needs: **Remove** D1 and D8's 0.3 s. D3's index becomes the words "within the measured ladder". D7's 0.4 s goes as well. C12 stays as a **Rule** (a quarter digit is the reading's own rounding).
    2. **Moves no more often than chance, with no consistent direction:** **Rule**, with this study as its evidence.
    3. **A trade of time, with accuracy within the noise on average:** the margins (C7, D9 both, D6's spreads), D2's creep floor, D12's half turn. Each sets how much room is left for the unknown, so how fast a throw goes. The simulation scatters less at the stop than the rig (`research/HANDOVER.md`), so "no cost in accuracy" is not shown for a unit.
       - **Proposed:** each kept as a **Rule** at its present value, named as the trade it is.
       - **Weighed, and not taken** ("Decided in the review", 4): the spread multipliers (C7, D9, D6) made from an over budget, as a confidence on a measured spread (3 spreads leaves about one throw in 740 beyond it, if the spread is normal). The rig's spreads are not normal, and the budget of one over in fifteen is ADR-009's bar, not a setting a user has.
       - None is moved to a value that tested quicker: that would be tuning against the simulation's rigs.
    4. **Lowered, it costs overs, on the slower scales above all:** D7's share and its 2.0 s top, C9, D8's 1.5 s top, D6's spreads at half. The present values sit on the safe side.
       - **Proposed: Rule,** with this evidence: lowering any of them costs overs on a late scale.
       - The tops (D7's 2.0 s, D8's 1.5 s) changed nothing when doubled, and the clamp check shows the 1.5 s never acts. **Proposed:** remove both, so a slow scale gets the glide time its lag asks for.
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
    - **How far the fallback is from a calibrated unit,** 10 gr, AR2209. The calibrated controller is the numbers study's baseline (the PID law in the frame, the same gains). Each cell is good / over / median:

      | Scale, band | Calibrated (simulation) | Not calibrated (simulation) |
      |---|---|---|
      | measured (the A&D), 0.04 | 88.6% / 10.9% / 9.6 s | 85.2% / 14.8% / 11.8 s |
      | measured, 0.02 | 74.2% / 24.1% / 10.8 s | 67.8% / 31.8% / 12.1 s |
      | holds, 0.04 | 83.5% / 16.0% / 20.5 s | 58.7% / 41.3% / 11.5 s |
      | holds, 0.02 | 66.4% / 30.6% / 22.1 s | 36.2% / 63.7% / 11.6 s |
      | slow, 0.02 | 65.2% / 31.4% / 20.6 s | 31.7% / 68.2% / 12.6 s |

      - **On the rig** (the A&D, 0.04, 20 throws a side, 2026-11-22): the calibrated glide threw 18 good, 2 over, 10.9 s; the fallback 14, 5, 13.7 s. With its least speed learnt it threw 16, 4, 15.5 s against the glide's 15, 5, 11.0 s.
      - **Today's loop** on the same rig puts about 85% of AR2209's throws over (README, "To settle before approval").
      - **Read:** on the scale its numbers came from, the fallback is a few points and about 2 s from a calibrated unit. On a scale later than the A&D it is twice as often over, quickly: it does not wait for a scale it does not know is late.
- **The fixed weights in grains** (D9's 0.2 gr, C19's turns, C20, E13), checked in the code 2026-11-22, 23:10:
  - The firmware reads a weight as a bare number in the scale's own unit. Only the A&D's frame carries the unit (`scale_get_unit`, `scale.c:148`), and the charge reads it nowhere.
  - So on a scale set to grams every literal written in grains is read as grams:
    - D9's 0.2 gr is 0.2 g, about 3 gr;
    - C20's far-over stop is 3 g, about 46 gr;
    - E13's cup limit is 600 g;
    - E14's bands are 0.02 and 0.04 g, about 0.3 and 0.6 gr.
  - The user's thresholds (A6 to A9) are in the scale's unit by design, so they hold.
  - **Decided:** no literal is a weight in grains, and none is the bench's weight turned into digits with the A&D's digit. A weight the frame holds is in the unit's own terms: a multiple of the user's tolerance (C20), or made from a spread the unit measured (D9, if N.3 finds it is needed at all). The bands go with the debug mode (E14); the tolerance is the user's (A7). E13 is the bench's.
- **What the auto-tuner writes into the profile** (A1, A3, A4): the PID values and the creep, while it is on. What "off" keeps is the register's first row.
- **One trade measured on each unit, not a rule:** the offer's fine factor (D19), by gate 3.

**Kept true by design:** a number added to the controller without a row here is an omission the next agent cannot see. So gate N's build step (N.2 of the README's plan) names every Rule and Safety number in one header, and adds a check to CI that fails on a number in the controller's files this page does not name.
