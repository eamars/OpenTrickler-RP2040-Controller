# OpenTrickler Powder Analysis – developer notes

For people who maintain or extend the powder analysis. The user manual is [powder_analysis.md](powder_analysis.md); the history of the design decisions and all measurement series are in [docs/powder_analysis_concept.md](../docs/powder_analysis_concept.md) (German).

## Files and responsibilities

| File | Responsibility |
|---|---|
| `src/powder_analysis_config.h` | Every tunable value: start-value matrix, flow speeds, time goals, search limits, verification rules, scale resolution, buffer sizes. Nothing else is hard-coded. |
| `src/powder_analysis_tuner.h/.cpp` | The tuning logic, free of hardware and RTOS: which values to try next, how to evaluate a throw, when a phase ends, the result incl. the fall statistics. Compiles on the host, covered by the unit tests. |
| `src/powder_analysis.h/.cpp` | The module on the device: state machine (one state per display screen), throw loop, pan handling, display screens, saving, REST handlers. Owns the status, the tuner and the recording. |
| `src/html/powder_analysis.html` | The web page, self-contained (no external resources, works in access-point mode). Polls the status every 400 ms; charts and CSV files are made in the browser. |
| `tests/powder_analysis/` | Host unit tests of the tuner against a simulated powder. |
| `tools/powder_analysis_preview/` | Renders every display screen and web scenario with dummy data; also produces the manual's images. |
| `docs/powder_analysis_concept.md` | Concept, decisions and measurement series (section 10 is the measurement log, append-only). |
| `docs/*.csv` | Protocols of the measurement series on real hardware. |

### Changes to existing files

Only what is needed to plug the module in, plus one bug fix:

| File | Change |
|---|---|
| `src/app.h` | `APP_STATE_ENTER_POWDER_ANALYSIS = 11` |
| `src/mui_menu.c` | Main menu entry "Powder Analysis" (after Cleanup) and its start form 70 |
| `src/menu.c` | Dispatch to `powder_analysis_menu()`, parent form for "Back" |
| `src/rest_endpoints.c` | The page `/powder_analysis` and the three REST endpoints |
| `src/CMakeLists.txt` | `html2header` rule for `powder_analysis.html` |
| `src/html/web_portal.html` | "Open Powder Analysis" button; the portal opens the tab named in the URL hash (`/#trickler`, `/#settings`, `/#purge`) so the page's bottom navigation can link back |
| `src/charge_mode.cpp` | Bug fix: the D term divided by a 0 ms interval on the first sample of a throw (`inf`/`NaN` → both tricklers at maximum speed until the next sample). See `docs/upstream_issue_charge_mode_d_term.md`. |

## State machine

`powder_analysis_menu()` runs in the menu task like charge mode and cleanup mode. `powder_analysis_state_t` has one state per screen (D1..D16 of the concept) plus the test throw states; the display render task draws the screen for the current state and the REST status reports it as `st`.

```
IDLE -> SETUP_WEIGHT -> SETUP_SHAPE -> SETUP_GRAIN -> PREPARE -> FLOODING
     -> WAIT_PAN_REMOVAL -> WAIT_PAN_RETURN -> WAIT_ZERO -> TRIAL -+
            ^                                                      |
            +------------------------------------------------------+  (until the tuner is done)
     -> RESULT_COARSE -> RESULT_FINE -> RESULT_THRESHOLD -> SAVE_SLOT -> SAVE_CONFIRM -> ACTIVATE -> EXIT
PAUSED (from the WAIT_* states), ABORT_CONFIRM (from anywhere), TEST_THROW / TEST_DONE (web test throw)
```

Input handling: every screen handler translates the two keys (encoder button, reset button) and the rotary knob into a `powder_analysis_command_t`, the same command type the web page posts. So a web command does exactly what the corresponding display button does. Web commands arrive through a FreeRTOS queue and wake a blocked `button_wait_for_input()` with `OVERRIDE_FROM_REST`, the pattern cleanup mode uses. Starting from the web while the device is idle goes through `exit_state = APP_STATE_ENTER_POWDER_ANALYSIS`, again like cleanup mode.

## Tuner phases and rules

The caller drives the tuner in a loop (`get_trial` → run the throw → `report_result`) until `is_done`. All numbers below are the config values in `powder_analysis_config.h`.

| Phase | Throws | Rule | Why |
|---|---|---|---|
| `COARSE_KP` | coarse only | Bracketing search for the smallest coarse Kp whose throw overthrows the coarse stop point **and** takes at most 10 s / 1.3 = 7.7 s. | Both conditions grow with Kp, so the search stays monotonic. The 1.3 (`COARSE_BRAKING_FACTOR`) leaves room for the Kd phase, which can only make the throw slower. |
| `COARSE_KD` | coarse only | Probing, not a search: start at 0.1, double up to 5 times, stop when the coarse time changes by less than 10 % or exceeds 10 s. Take the fastest Kd within 10 s; times within 0.2 s count as equal and the smaller Kd wins. If not even the start value was within 10 s, retry it up to 2 times before giving up (then the smallest Kd is used and the result is flagged). | Coarse times of the same values scatter by seconds on the device, more than Kd shifts them. A bracketing search chased that noise. |
| `FINE_KP` | full | Bracketing search for the smallest fine Kp whose full throw finishes within 13 s (`FINE_TARGET_TIME_S`). Overthrows do not matter here. | The fine trickler's speed at the stop is `max(Kp × fine threshold, minimum speed)`; the slower it runs, the less powder falls after the stop. The 2 s below the 15 s total goal absorb the scatter of the coarse time. |
| `VERIFY` | full | Rounds of 10 throws with fixed values. 9 hits (within the fine threshold and under 15 s) verify the profile. 2 overthrows in a round give the round up and apply a correction: fine Kp × 0.9 if `Kp × threshold` is above the minimum speed, otherwise the fine minimum speed × 0.75. At most 3 corrections, then the best round (most hits, on a tie the faster one) is the result. | Overthrows are kernels still falling after the stop; what falls depends on the speed at the stop, so that speed is lowered. |

Fine Kd is not searched: the D term works on the error change per millisecond and the scale changes its reading only every other sample, so with usual values it is below 1 % of the fine speed (measurement 9 in the concept). The matrix value is used as is.

The bracketing search (`powder_analysis_search_t`) doubles while nothing met the condition (max 6 times), halves while everything met it (max 5 times), then bisects until the interval is below 15 % or after 3 steps. Limits: 60 throws for the whole analysis (`MAX_TRIALS`, also the size of the protocol), 30 s per throw (`TRIAL_TIMEOUT_S`, the throw is aborted and reported as `TIMEOUT`).

### Fall after the stop and the threshold recommendation

Every full throw records the weight of the sample that triggered the fine stop; the fall is `settled weight − that weight`. Over the verification throws (or the fine Kp throws if the verification was never reached) the tuner computes median and maximum. The scale reports multiples of its resolution (0.02 gr) and charge mode stops when `target − weight < threshold`; with a threshold on an exact multiple, float rounding decides whether the throw stops one step early or at the target (23.1 − 23.08 = 0.0200005 is not < 0.02, 8.2 − 8.18 = 0.0199995 is). So the recommendation is `(n + 0.5) × resolution` with `n = round(median / resolution)`, at least 1, which stops reliably at `target − n × resolution`; `expected_final_offset_gr = median − n × resolution` is where throws land on average. `powder_analysis_is_exact_scale_step()` is the shared check behind the warnings on the display (D4) and the page.

### Configuration values

From `src/powder_analysis_config.h`:

| Value | Default | Meaning |
|---|---|---|
| `powder_analysis_coarse_kp_start[shape][grain]` | Ball 0.08 / 0.04 / 0.075, Stick 0.10 / 0.06 / 0.12, Flake and Disc 0.16 / 0.10 / 0.18 (fine / medium / coarse) | Coarse Kp the search starts from |
| `powder_analysis_fine_kp_start[shape][grain]` | Ball 3.0 / 2.4 / 2.5, Stick 4.0 / 3.0 / 3.0, Flake and Disc 5.0 / 3.6 / 4.0 | Fine Kp the search starts from |
| `powder_analysis_fine_kd_start[shape][grain]` | 4.0 / 8.0 / 4.0 for every shape | Fine Kd, used as is |
| `COARSE_KD_START` | 0.1 | First coarse Kd of the probing |
| `COARSE_MIN/MAX_FLOW_SPEED_RPS` | 0.1 / 5.0 | Coarse trickler speed limits (not searched, avoid stalls) |
| `FINE_MIN/MAX_FLOW_SPEED_RPS` | 0.06 / 5.0 | Fine trickler speed limits; the minimum is lowered by verification corrections |
| `KI` | 0 | Ki of both tricklers (tuning guide) |
| `FLOOD_COARSE/FINE_SPEED_RPS` | 1.0 / 2.0 | Motor speeds while flooding the tubes |
| `COARSE_TIME_GOAL_S` | 10 | The coarse phase must finish within this time |
| `COARSE_BRAKING_FACTOR` | 1.3 | Coarse Kp throws must stay within goal / factor |
| `COARSE_KD_MAX_DOUBLINGS` | 5 | Kd probing: 0.1 → 3.2 at most |
| `COARSE_KD_MIN_TIME_CHANGE` | 0.10 | Probing stops when the coarse time changed by less than this fraction |
| `COARSE_KD_TIME_TIE_S` | 0.2 | Coarse times this close count as equal (smaller Kd wins) |
| `COARSE_KD_MAX_RETRIES` | 2 | Retries of the start value when it was too slow |
| `COARSE_MAX_OVERSHOOT_GR` | 1.0 | Check only: overshoot over the coarse stop point above this flags the result |
| `TOTAL_TIME_GOAL_S` | 15 | A hit must finish within this time |
| `FINE_TARGET_TIME_S` | 13 | The fine Kp search's time condition |
| `TRIAL_TIMEOUT_S` | 30 | Safety timeout per throw |
| `STABLE_WEIGHT_TIMEOUT_S` | 10 | Waiting for a stable weight after a throw, then the average is used |
| `BISECTION_MIN_INTERVAL` | 0.15 | Bisection stops below this relative interval ... |
| `MAX_BISECTION_STEPS` | 3 | ... or after this many steps |
| `MAX_HALVING_STEPS` / `MAX_DOUBLING_STEPS` | 5 / 6 | Bounds of the first search step |
| `VERIFY_ROUND_THROWS` | 10 | Throws per verification round |
| `VERIFY_HITS_FOR_VERIFIED` | 9 | Hits that verify |
| `VERIFY_MAX_OVERTHROWS` | 2 | Overthrows that give a round up |
| `FINE_KP_CORRECTION_FACTOR` | 0.9 | Fine Kp correction (−10 %) |
| `FINE_MIN_SPEED_CORRECTION_FACTOR` | 0.75 | Fine minimum speed correction (−25 %) |
| `MAX_FINE_CORRECTIONS` | 3 | Corrections at most |
| `MAX_TRIALS` | 60 | Throws per analysis, size of the protocol in RAM |
| `MAX_TARGET_WEIGHT_GR` | 300 | Plausibility limit for web commands |
| `SCALE_RESOLUTION_GR` | 0.02 | Scale step, used by the threshold recommendation only |
| `TRACE_MAX_SAMPLES` | 300 | Recording buffer per throw (about 30 s at 10 Hz, 9.6 KB RAM) |

The display and the web page build their texts ("Kp -10%", "(1/3)", "Overshoot > 1.0 gr", "Slower than 10 s") from these values; the status JSON carries the correction rules (`fcm`, `kpc`, `msc`, `fmd`) for the page.

### Changing the start values from CSV protocols

The matrix comment in `powder_analysis_config.h` records which measurement series each column is based on. To refine a column: run the analysis with the powder, download the protocol CSV, look at the found `coarse_kp` (last `Coarse Kp` throw) and `fine_kp` (last `Fine Kp` throw or the verification throws) and move the start value towards them. A start value close to the result saves the doubling/halving throws; the search still finds the result from a start value that is off by a factor of 2 to 4, it just needs more throws. Append the series to section 10 of the concept so the next person knows where the numbers come from.

## The throw loop and charge mode

`run_throw()` in `powder_analysis.cpp` is a copy of the control loop of `charge_mode_wait_for_complete()` in `charge_mode.cpp`, and `coarse_trickler_backoff()`, `error_derivative()`, the stable-weight wait and the pan handling (`wait_for_pan_removal/return/zero`) are copies of their charge mode counterparts. **Keep them in sync** when charge mode changes. The deliberate differences are listed at the top of `run_throw()`:

- gains, Ki (always 0) and flow speeds come from the trial parameters and the analysis config, not from the selected profile;
- reset / abort asks for confirmation instead of leaving the mode;
- the servo gate is opened once at the start (no gate ratio at the coarse stop), no precharge, no 20 ms delay at the end, no title / elapsed-time display handling;
- a coarse-only throw ends after the coarse stop plus backoff; `stop_motors()` stops both motors and clears the backoff flag;
- safety timeout, coarse and total time are measured, the live elapsed time goes into the status;
- errors and derivatives are computed before the stop checks so the stop sample can be recorded and its weight reported (`stop_weight`, the base of the fall statistics).

## Dual-core access

Three tasks touch the module and with two cores they really run in parallel: the menu task runs `powder_analysis_menu()` and is the only writer of the status, the tuner and the recording; the display render task reads the status (and refreshes `current_weight`); the lwIP task answers REST requests. Rules (see the note at the top of `powder_analysis.cpp`):

- The two strings of the status (`prompt`, `profile_name`) are written inside `taskENTER_CRITICAL()`; `powder_analysis_get_status()` copies the whole struct inside the same critical section and the status handler builds its JSON from that copy. A torn number would be one poll with an old or new value; a torn string could be unterminated.
- The trial protocol and the recording are append-only: an entry is complete before its counter (`trial_count`, `trace.count`, `trace.id`) is incremented, and the readers take the counter first.

## REST API

All responses are JSON. Floats are plain numbers; the current weight `w` is the string `"nan"` or `"inf"` when the scale gives no number.

### `GET /powder_analysis`

The page itself.

### `GET /rest/powder_analysis_state`

Status, and optionally a command. Parameters (all optional, a command is posted only with `cmd`):

| Parameter | Meaning |
|---|---|
| `cmd` | `powder_analysis_command_type_t` as int: 1 START, 2 NEXT, 3 BACK, 4 PAUSE_RESUME, 5 FLOOD_DONE, 6 ABORT, 7 ABORT_CONFIRM, 8 ABORT_CANCEL, 9 SAVE, 10 DISCARD, 11 USE_NOW_YES, 12 USE_NOW_NO, 13 ZERO_SCALE, 14 TEST_THROW |
| `tw`, `sh`, `gs` | Target weight (gr), shape (0 Ball, 1 Stick, 2 Flake, 3 Disc), grain size (0 Fine, 1 Medium, 2 Coarse) for START |
| `pi`, `pn` | Profile slot and name (max 15 characters, no quotes or backslashes) for SAVE |
| `ckp`, `ckd`, `fkp`, `fkd`, `fms`, `tw` | Gains, fine minimum speed and target weight for TEST_THROW |

Response keys:

| Key | Meaning |
|---|---|
| `st`, `sn` | State (`powder_analysis_state_t` as int) and its name |
| `ph` | Phase name (`Coarse Kp`, `Coarse Kd`, `Fine Kp`, `Verify`, `Done`) |
| `pr` | Prompt for the user |
| `tm`, `fl`, `za` | Test throw mode, flooding motors running, scale can be zeroed |
| `tn`, `tt` | Number of the running / next trial and its throw type (0 coarse only, 1 full). **`tt` has a second meaning inside `lt`, `res` and the trials list: total time in seconds.** |
| `ckp`, `ckd`, `fkp`, `fkd` | Gains of the running / next throw |
| `w`, `el` | Current weight, elapsed seconds of the running (or last) throw |
| `tw`, `sh`, `gs` | Setup values |
| `cs`, `fs`, `sr` | Coarse / fine stop threshold of charge mode, scale resolution |
| `vr`, `vrh`, `vrt` | Verification round (0 before the verification), hits in it, throws per round |
| `fc`, `fcj`, `fck` | Corrections so far; the last throw gave its round up and a correction was applied; that correction lowered fine Kp (true) or the minimum speed (false) |
| `fcm`, `kpc`, `msc`, `fmd` | Config: max corrections, Kp correction percent, min speed correction percent, default fine minimum speed |
| `tc`, `tid` | Trials done so far (fetch the trials when it changes), id of the last recording (fetch the trace when it changes) |
| `lt` | Last trial `{n, w, tt, ev}` (number, weight, total time, evaluation name) or `null` |
| `res` | Result or `null`: `ckp, ckd, cmin, cmax, fkp, fkd, fmin, fmax` (profile values, Ki is always 0), `vh` hits of the best round, `fc` corrections, `ct, co, tt` coarse time, coarse overshoot, total time, `fn, fm, fx` fall count / median / max, `rft` recommended fine threshold, `efo` expected final offset, `rs` reason text, `ch` coarse hint text |
| `pi`, `pn` | Proposed save slot and name |
| `error` | Only when a command was rejected: `InvalidCommand`, `InvalidTargetWeight`, `InvalidShape`, `InvalidGrainSize`, `InvalidProfileIndex`, `InvalidProfileName`, `ProfileNameTooLong`, `InvalidGain`, `InvalidMinSpeed`, `AnalysisRunning`, `QueueFull` |

Evaluation names: `UNDER`, `OVER`, `HIT`, `SLOW`, `OK`, `TIMEOUT`.

### `GET /rest/powder_analysis_trials`

`{"tc": <count>, "t": [{n, ph, kp, kd, w, ct, tt, ev, fms, fa}, ...]}` – number, phase name, gains of the trickler that phase tunes, final weight, coarse time, total time, evaluation name, fine minimum speed, fall after the stop in gr (`null` if not measured). The page turns this into the protocol CSV with the columns `number,phase,kp,kd,weight_gr,coarse_time_s,total_time_s,evaluation,fine_min_speed_rps,fall_after_stop_gr`.

### `GET /rest/powder_analysis_trace?from=<index>`

The recording of the last throw, 40 samples per response starting at `from`:

`{"id": <recording id>, "n": <samples recorded>, "p": {tw, ckp, ckd, fkp, fkd, fms, fw, k}, "s": [[t_s, weight, fine_p, fine_d, fine_speed, coarse_p, coarse_d, coarse_speed], ...]}`

`p` holds the values of the throw, `fw` the settled final weight (`null` while unknown) and `k` the kind (`"test"` or the phase name). `id` changes only once the settled weight is known, so the page fetches a recording exactly once and complete. The sample that ended the throw is recorded too, with the P and D terms of that moment and speeds 0. Throws longer than 300 samples are cut off.

The page's CSV of one recording has the columns `t_s,weight_gr,fine_p_rps,fine_d_rps,fine_speed_rps,coarse_p_rps,coarse_d_rps,coarse_speed_rps` after a first line `final_weight_gr,...`; "Download all" prefixes every row with `throw,coarse_kp,coarse_kd,fine_kp,fine_kd,fine_min_speed_rps,target_gr,final_weight_gr`.

## Building and flashing

Same as the rest of the firmware (see the [README](../README.md) for Windows). On macOS / Linux with the Pico SDK installed:

    source ./configure_env.sh
    cmake -B build_pico2_w -DPICO_BOARD=pico2_w        # or -DPICO_BOARD=pico_w
    cmake --build build_pico2_w

Flash `build_pico2_w/app.uf2` with the Pico in BOOTSEL mode. CMake collects the sources at configure time: run the configure step again after adding a file.

## Unit tests

    make -C tests/powder_analysis test

Host tests of the tuner (`tests/powder_analysis/test_powder_analysis_tuner.cpp`) against a simulated powder whose overshoot grows with Kp and whose times shrink with Kp and grow with Kd. They cover the bracketing search, every phase and its limits, the corrections, the fall statistics and a noisy powder with 20 seeds. A failing check prints its line; the run ends with `<n> checks, 0 failed`. Run them after every change of the tuner or the config.

## Preview tool

    make -C tools/powder_analysis_preview            # out/index.html with every screen and web scenario
    make -C tools/powder_analysis_preview doc-images # also the manual's images in resources/powder_analysis/

The display screens are rendered from the module's real drawing code (cut out of `powder_analysis.cpp` by `extract_sources.py` between its banner comments) with the u8g2 bitmap backend; the web scenarios are the real page with canned JSON from the real REST serializers. Add a screen or a scenario in `display_preview.cpp` / `json_gen.cpp` when you add one to the module. The images for the manual are defined in `doc_images.py` (markings in LCD pixel coordinates, screenshots with headless Chrome).

## Resources

The whole firmware with the module: FLASH about 752 KB (17.9 % of 4 MB on the Pico 2 W), RAM about 228 KB (43.4 %). The module itself costs about 70 KB flash and 27 KB RAM, most of the RAM being the trial protocol (60 entries) and the recording buffer (300 samples).
