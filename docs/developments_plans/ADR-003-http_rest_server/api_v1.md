# API v1: the table, for approval

ADR-003's API, written for gate 2's approval (2026-11-15). It was checked against every ADR, the menu and the TODOs in the code ("What it must grow into", below). It was then reviewed by a second model at the owner's word, and revised from that review ("The review", at the end). Once approved, this is the contract: v1 only grows. Once built, the unit's own description (`GET /api/v1/schema`) says what it carries, from the same tables as the firmware, and this file is kept to it: the scripted client checks the two against each other.

Each field below comes from today's stored structs, read in the code on 2026-11-15. Where today's code has no limit, the limit here is a **proposal** (marked *p*), for the owner to look at.

## Words

- **The charge** is the job, from `charge/start` to its stop: charge mode, a loop of throws. **A throw** is one cup, numbered from 1 at each start of the unit. A throw lasts from the start of `zeroing` until its verdict is given, a second after the cup is due off. While it lasts, `in_throw` is true.
- **A job** is long work: the charge and the cleanup now; tuning, an update, a motor's test run, the scale's calibration later. **The unit runs one job at a time.** `GET /api/v1/system` says which (`activity`).

## The rules

- **Paths** start with `/api/v1`. Names are `snake_case` and say what a thing is; no codes. A quantity with a physical unit carries it in its name (`_rps`, `_rps2`, `_ms`, `_ma`, `_mohm`, `_per_s`); a weight and a ratio carry none.
- **A read is a GET, and it changes nothing.** Not a selection, not an event, not a light.
- **A write is a PATCH** with a JSON object of the fields to change, one or many. **A PATCH's fields are the GET's fields:** same name, same type. The whole request is checked first; if any field is refused, nothing in the request is written. The reply is the whole resource as it now is.
- **An action is a POST** to a path named for it (`/charge/start`), with a JSON body (`{}` when it takes nothing). The reply is the state it led to: 202 when the action goes on after the reply (a charge, a restart), 200 when it is done. **An action that is refused changes nothing**, fields it carries included.
- **A stop never fails.** Every job's stop, and `POST /api/v1/stop`, answer 202 when they stopped something and 200 with the state when there was nothing to stop. A client may repeat one after a lost reply.
- **Only the unit's own name is served.** An API request whose `Host` is not the unit's address or its host name (with or without `.local`) is refused with 403. A page asked for under another name is redirected to the unit's address, which the captive portal needs too. This stops a page that has pointed its own name at the unit (DNS rebinding).
- **A write or an action must be sent as `application/json`**, or as `application/octet-stream` where a path takes bytes (ADR-004's image). Anything else is refused with 415, a POST with no body and no `Content-Type` included. The unit sends no CORS headers and answers `OPTIONS` with 405. A browser asks a server's leave before it sends either type from another site, and the unit never gives it. With the `Host` rule, a page from another site cannot make a browser write to the unit.
- **Settings are live when written and stored when saved.** A PATCH changes the setting at once (or at the next start, where the schema says so). A save stores it. Every settings resource carries `"saved": true|false`. `saved` is worked out, not remembered: the struct in RAM against what was last stored, by their CRCs. So a change made at the menu or at the console counts too.
- **Numbers** are JSON numbers. **A number a GET gives, PATCHed back, changes nothing:** floats are written with nine significant digits. A value that is not there, such as a weight with no scale reading, is `null`.
- **Choices** are strings or numbers from a list in the schema, each with a label: `"driver": "and_fxi"` ("A&D FX-i"), `"baud": 9600`. No indexes.
- **Weights** are in the scale's own unit: `GET /api/v1/scale` says which, or `null` when the scale cannot tell. A client does not assume.
- **Colours** are `"#rrggbb"`.
- **Ids are opaque numbers.** Powder profiles have the ids 0 to 7 today, but a client reads the list and does not count on that.
- **Unknown fields in a request are refused** (400 `unknown_field`), so a typo is caught rather than ignored.
- **API replies carry `Cache-Control: no-store`.** The schema and the pages carry an `ETag`, the build's hash, and are answered 304 when unchanged.

### Growing within v1

What may change within v1, and what a client does about it:
- **New paths and new fields** are added. A client ignores a field it does not know. **A request only ever gains optional fields.**
- **A choice may gain values:** a new scale driver, a new state, a new end reason, a new event kind, a new `activity`. A client treats a value it does not know as "something else", and does not fail.
- **A field never goes, and never changes its meaning or its unit.** A setting that moves keeps its old place as the default. For example, if the stop thresholds become per profile (ADR-009, ADR-014), `settings/charge` keeps them, and a profile's own field, `null` when not set, uses that default.
- **Limits, defaults and steps are the schema's, not the contract's.** They may change from one build to another; the schema is the truth for its build.
- **A part a board does not carry** (ADR-005) is absent: its paths answer 404, and the index and the schema do not list it. A client learns what is there from `GET /api/v1`, not from this file.
- **Every job has one shape:** a resource with a `state`, `POST .../start` and `POST .../stop`, `fault` when it ended on one, refusals with 409, its events in the unit's one event log, and a place in `activity` and in `POST /api/v1/stop`.
- **Bytes are sent with PUT** (ADR-004's image), as `application/octet-stream`.
- **What is under `/api/v1/debug` is not part of v1.** It may change or go in any build.
- **A password,** if its own ADR adds one, comes as a header (`Authorization`). Paths and bodies stay as they are; a client adds the header.
- **A change that breaks any of this** is `/api/v2`, served beside v1 for a time.

### Status and errors

| Status | When |
|---|---|
| 200 | A read, a write, an action done; a stop with nothing to stop |
| 202 | An action under way: a charge started or stopped, a restart |
| 304 | The schema or a page, unchanged since the `ETag` the client gave |
| 400 | Not JSON, an unknown field, a wrong type, a value outside its limits, a missing confirmation |
| 403 | A `Host` that is not the unit's |
| 404 | No such path on this build, or no such id |
| 405 | The path exists, the method does not; `Allow` lists the methods |
| 409 | Refused at this moment: another job runs, a throw is under way, no job runs that the command needs, a driver is at fault, the gate is disabled |
| 411 | A body with no `Content-Length` (a chunked one included) |
| 413 | A request larger than the unit takes: a JSON body over 1 KB, a header line over 512 bytes, headers over 8 KB in all |
| 415 | A write not sent as `application/json` (or `application/octet-stream` where bytes are taken) |
| 500 | The unit failed to do it, such as a save the EEPROM did not take |
| 501 | The path is known, but this make of scale cannot do it: the Steinberg, Creedmoor and Sartorius drivers cannot zero their scale (TODOs in their code) |

Every error has the same body:

```json
{"error": {"code": "out_of_range", "field": "fine_stop_threshold", "message": "12 is above the most, 10", "min": 0, "max": 10}}
```

The codes are `bad_json`, `length_required`, `unknown_field`, `wrong_type`, `out_of_range`, `not_a_choice`, `read_only`, `confirm_missing`, `not_found`, `method_not_allowed`, `busy` (with `activity` and `state`), `not_running`, `no_target`, `driver_fault` (with `motor` and `fault`), `gate_disabled`, `not_supported`, `wrong_host`, `too_large`, `wrong_media_type` and `failed`. `field`, `min`, `max`, `choices`, `activity`, `state`, `motor` and `fault` are given when they apply. More codes may come; the schema lists them.

## What the unit is

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1` | | `api` ("1"); `firmware` {`version`, `vcs`, `build`}; `board` (pico_w, pico2_w; esp32_s3_pico with ADR-007); `unit_id`; `hostname`; `boot`, a 64-bit random number new at each start (the SDK's `pico_rand`); `uptime_s`; `features`, a list of names of the optional parts this board carries (ADR-005; empty today: every board carries every part); `guide`, a page of plain words for a client new to the unit (the job cycle, what stop does, what the unit cannot know); `resources`: each path with its methods and a line on what it is |
| `GET /api/v1/schema` | | Every resource's fields **and every action's body**: `name`, `label`, `group`, `type` (number, integer, boolean, string, colour, choice), `unit`, `min`, `max`, `step`, `choices` (each `{value, label}`), `access` (read, read_write, write_only), `applies` (now, at_restart, or as the field says), `refused_while` (in_throw, a_job_runs, or never), and a line on what it is. Each settings resource names its save path. Also every state and what it means, every event kind and what it carries, and every error code. ADR-008's forms and ADR-010's settings screen are drawn from it. It changes only with the firmware |
| `GET /api/v1/system` | | `firmware`, `board`, `unit_id`, `uptime_s`; `activity` (none, charge, cleanup, scale_calibration; more later); `restart_pending` (a setting that applies at restart was changed) |
| `GET /api/v1/network` | | `mode` (station, access_point), `ssid`, `address`, `hostname` (the mDNS name), `rssi`: the menu's Wi-Fi info, and how a person or an agent finds which unit is which |
| `POST /api/v1/stop` | `{}` | Stops whatever job runs. A stop: never refused (the rule above) |
| `POST /api/v1/system/restart` | `{}` | 202; the unit restarts once the reply is out. 409 `busy` while a job runs: stop it first |
| `POST /api/v1/system/erase` | `{"confirm": "erase"}` | 202. The stored settings are erased, then the unit restarts. **It then leaves the network and comes back as its own access point, with a new id and host name** (the id is kept in the EEPROM, and the name is made from it). 400 `confirm_missing` without the confirmation; 409 `busy` while a job runs |
| `GET /api/v1/debug` | | Not part of v1: today `heap_free` (FreeRTOS) and `net` (lwIP's heap and pools, as the console's `net`) |

## The event log

One log for the whole unit, which any number of clients read, each from where it left off. It replaces the charge state's events, which were cleared by whoever read them first.

| Method and path | Reply |
|---|---|
| `GET /api/v1/events?after=N` | `boot` (as in the index: when it changes, the unit restarted and the numbers began again); `now_ms`, the unit's clock now; `events`, each with `seq`, `at_ms` (the unit's 64-bit clock, in ms since the start), `source` and `kind`, and what that kind carries; `next`, the number to ask after next time. The newest 64 are kept *p*. `seq` goes up by one, so a client sees a gap: one that asks after a number no longer kept gets all that are, and `first`, the oldest kept |

An event is a change, not a state: a fault that stands is one event when it appears and one when it clears, not one a second. The kinds now:

| Source | Kind | Carries |
|---|---|---|
| charge | started | `by` (button, api, console), `target`, `powder_profile` |
| charge | stopped | `by` (button, api, console, or the unit itself), `reason` (stopped, fault) |
| throw | started | `throw`, `target`, `powder_profile` |
| throw | paused | `throw`, the pause's number in it (ADR-013: no reading for 3 of the scale's intervals) |
| throw | resumed | `throw`, `weight`, the settled weight it went on from |
| throw | finished | `throw`, `result` (good, under, over), `weight`, `elapsed_s` |
| throw | ended | `throw`, `reason` (stopped, scale_no_reading, scale_unsteady, scale_unreliable, driver_fault), and for a driver's fault its `motor` and `fault` |
| cleanup | started | `by`, `seconds` when given |
| cleanup | stopped | `by`, `reason` (stopped, time, fault) |
| motors | fault, cleared | `motor`, `fault` |
| system | started | `firmware`, at each start |

Later ADRs add sources and kinds (the rules above). Waiting for the next event in place of polling (`?wait=s`, or a push channel) may come with ADR-008 or ADR-009.

## Charge

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/charge` | | `state` (idle, zeroing, charging, waiting_for_cup_removal, waiting_for_cup_return, fault); `in_throw`; `throw`, the number of this throw or the last; `paused` (true while a throw waits for readings); `target` (null until set); `powder_profile` (its id) and `powder_profile_name`; `weight` (null with no reading); `elapsed_s` (this throw, or the last); `last` {`throw`, `result` (good, under, over, none), `reason` (finished, or as a throw's `ended`), `weight`}; `event_seq`, the newest event's number |
| `PATCH /api/v1/charge` | `{"target": 30.0}`, `{"powder_profile": 3}`, or both | 200. What the next throw uses. One target for the unit: the menu's weight digits show it and set it. `target` from 0.001 to 999.99 at two decimal places, 99.999 at three: what the menu can show. The powder profile chosen is live until saved, with the profiles (`powder_profiles.saved`). 409 `busy` while `in_throw` |
| `POST /api/v1/charge/start` | `{}`, or `target` and `powder_profile` to set them first | 202 with the charge. 409 `busy` while another job runs or the menu is not at its forms (a person is in a mode at the unit); 409 `driver_fault`; 400 `no_target` when no target is set |
| `POST /api/v1/charge/stop` | `{}` | A stop (the rule above). It also acknowledges `fault`, as a press at the unit does |

The charge in `fault` has ended on one: it waits, as the screen does, to be acknowledged by a stop or a press, and `last.reason` says why. It is not a throw: settings are written.

ADR-014 adds a charge profile: `charge_profile` beside `target` and `powder_profile`, in the GET, the PATCH and start. A target or a powder profile given beside it overrides it for that charge.

## Cleanup

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/cleanup` | | `state` (idle, running, fault), `speed_rps`, `seconds_left` (null when it runs until stopped) |
| `POST /api/v1/cleanup/start` | `{}`, or `{"seconds": 60}` | 202. With `seconds` (1 to 600 *p*), the cleanup stops by itself after that time, so a client that loses the network leaves no motor turning. Without it, it runs until stopped, as at the unit. 409 `busy` while another job runs; 409 `driver_fault` |
| `POST /api/v1/cleanup/stop` | `{}` | A stop; it acknowledges `fault` |
| `PATCH /api/v1/cleanup` | `{"speed_rps": 2.0}` | 200. Both tricklers turn at that speed, within ± the lower of the two motors' `max_speed_rps` at that moment. 409 `not_running` when idle |

## The scale, the gate, the motors, the screen

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/scale` | | `weight` (null with no reading); `unit` (gn, g, or null when the make cannot tell: null for all but the A&D, whose frame carries it); `driver`, the one that runs; `simulated` (the console's stand-in is running in its place); `usual_interval_ms` (the scale's rhythm, ADR-013); `bad_frames`; `actions`, what this make can be asked to do (zero now; its calibration, its display, its fast report later) |
| `POST /api/v1/scale/zero` | `{}` | 200. Taken when no job runs and in `zeroing` (a cup that reads 0.05 is put right, as the encoder's press does). Refused with 409 `busy` in `charging`, `waiting_for_cup_removal` and `waiting_for_cup_return`: the unit cannot tell whether a cup is on the pan, and a zero while it waits for the cup starts the next throw at once. 501 `not_supported` when the make cannot |
| `GET /api/v1/gate` | | `state` (open, closed, partial, released, disabled), `ratio` (0 open to 1 closed; null when released or disabled). `released`: not driven, the menu's "Disable"; `disabled`: off in its settings (`settings/gate.enable`), so charge mode does not use it |
| `PATCH /api/v1/gate` | `{"ratio": 0.5}`, or `{"ratio": null}` to release it | 200. 409 `gate_disabled` when it is off in its settings; 409 `busy` while `in_throw` |
| `GET /api/v1/motors` | | For `coarse` and `fine`: `speed_rps` (now, ramps included), `driver` (ok, absent, short, hot, reset, supply) |
| `GET /api/v1/display` | | `width`, `height`, `format` (`u8g2_tiles`: one bit a pixel, in u8g2's tiles). When LVGL draws the screen (ADR-010), or a larger screen comes, `format` says so |
| `GET /api/v1/display/frame` | | The screen's bytes, as `application/octet-stream`, in the `format` above |

## Powder profiles

What ADR-014 calls a powder profile: how a powder flows, and the controller's gains for it. Named so now, so that ADR-014's charge profiles have their own place beside them.

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/powder_profiles` | | `saved`, and `profiles`: for each its `id` and `name`. Which one the next throw uses is the charge's `powder_profile` |
| `GET /api/v1/powder_profiles/{id}` | | Its fields, below; 404 for an id not in the list |
| `PATCH /api/v1/powder_profiles/{id}` | Fields, below | 200; 409 `busy` while `in_throw`, when it is the one in use |
| `POST /api/v1/powder_profiles/save` | `{}` | 200: all of them, and which one is chosen, stored. They are one block in the EEPROM |

| Field | Type | Unit | Limits | Today |
|---|---|---|---|---|
| `name` | string | | 1 to 15 bytes, printable ASCII (the screen's font draws nothing else) | `p2`; 16 were stored with no ending |
| `coarse_kp`, `coarse_ki`, `coarse_kd` | number | | 0 or more *p* | `p3` to `p5` |
| `coarse_min_speed_rps`, `coarse_max_speed_rps` | number | rev/s | 0 to 100, min not above max; used up to the coarse motor's `max_speed_rps` | `p6`, `p7` (`..._flow_speed_rps`) |
| `fine_kp`, `fine_ki`, `fine_kd` | number | | 0 or more *p* | `p8` to `p10` |
| `fine_min_speed_rps`, `fine_max_speed_rps` | number | rev/s | as coarse, with the fine motor | `p11`, `p12` |

Eight slots today, so no POST to make one and no DELETE. Those may come (ADR-014, or ADR-009's tuning saving a new one) without changing the paths. Today's `rev` and `compatibility` (`p0`, `p1`) are used by nothing in the code and are not in v1.

## Settings

Each is `GET` and `PATCH`; the schema names its save path. "When" is when a change takes effect. "Refused" is when a write to it is refused with 409.

- **`GET /api/v1/settings`**: every settings resource and the powder profiles in one document, with the firmware's version: the portal's export, `rc_run.py settings`, and the copy taken before a flash (AGENTS.md). It holds no Wi-Fi key, so a copy cannot bring a wiped unit back onto the network by itself.
- **An import is the client's work:** a PATCH of each resource and each profile in turn, leaving out fields the schema marks read-only or does not list, and saying what it left out. A bulk write may come later, as a new method on this path.
- **`POST /api/v1/settings/save`** `{}`: every settings resource and the powder profiles stored. 500 names the one that failed.
- **A test stand-in is never stored.** The console's simulated scale runs in place of the stored driver and never shows in `settings/scale`, and no save stores it.

### `/api/v1/settings/charge`, saved at `/api/v1/settings/charge/save` (refused while `in_throw`)

| Field | Type | Unit | Limits | When | Today |
|---|---|---|---|---|---|
| `coarse_stop_threshold` | number | weight | 0 to 100 *p* | now | `c5` |
| `fine_stop_threshold` | number | weight | 0 to 10 *p* | now | `c6` |
| `steady_sd_margin` | number | weight | 0 to 1 *p*. The steady test, in three places: the zero, the cup's removal, the resume after a pause | now | `c7` (`set_point_sd_margin`) |
| `zero_mean_margin` | number | weight | 0 to 1 *p* | now | `c8` (`set_point_mean_margin`) |
| `decimal_places` | choice | | 2, 3 | now | `c9` (an index) |
| `coarse_stop_gate_ratio` | number or null | | 0 to 1; null: the gate is not moved | now | `c13` (-1 for not moved) |
| `coarse_stop_backoff_enable` | boolean | | | now | `c14` |
| `coarse_stop_backoff_turns` | number | turns | 0.125 to 2 *p*, step 0.125 | now | `c15` |
| `coarse_stop_backoff_speed_rps` | number | rev/s | 0 to 100; 0 is the coarse motor's minimum speed; used up to its `max_speed_rps` | now | `c16` |
| `precharge_enable` | boolean | | | now | `c10` |
| `precharge_time_ms` | integer | ms | 0 to 5,000 *p* (RST is not read while it runs) | now | `c11` |
| `precharge_speed_rps` | number | rev/s | 0 to 100; used up to the coarse motor's `max_speed_rps` | now | `c12` |
| `colour_good`, `colour_under`, `colour_over`, `colour_not_ready` | colour | | | now | `c1` to `c4` (`neopixel_..._colour`) |

### `/api/v1/settings/motors/coarse` and `/api/v1/settings/motors/fine`, both saved at `/api/v1/settings/motors/save` (refused while a job runs)

The two are stored as one block, so they are saved together. Between throws the motors are still enabled, and some of these set the driver up again, so they are refused while charge mode or cleanup runs at all, as the console's `tmc ... setup` is. Today's checks (`motor_persistent_config_is_valid`) stay.

| Field | Type | Unit | Limits | When | Today |
|---|---|---|---|---|---|
| `full_steps_per_rotation` | integer | steps | 20 to 2,000 | now | `m1` |
| `current_ma` | integer | mA RMS | 10 to 2,000 | now (the driver is set up again) | `m2` (wrapped at 65,536 before its check) |
| `microsteps` | choice | | 1, 2, 4, ... 256 | now (set up again) | `m3` |
| `max_speed_rps` | number | rev/s | 1 to 100, step 1 | now | `m4` |
| `r_sense_mohm` | integer | mΩ | 50 to 1,000 | now (set up again) | `m5` |
| `acceleration_rps2` | number | rev/s² | 0.1 to 10,000 | now | `m0` (`angular_acceleration`) |
| `min_speed_rps` | number | rev/s | 0 to `max_speed_rps` | now | `m6` |
| `gear_ratio` | number | | 0.01 to 100 | now | `m7` |
| `invert_direction` | boolean | | | now | `m9` |
| `invert_enable` | boolean | | | when the motor is next enabled | `m8` |

### `/api/v1/settings/scale`, saved at `/api/v1/settings/scale/save` (refused while `in_throw`)

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `driver` | choice | and_fxi, steinberg_sbs, gng_jjb, ussolid_jfdbs, jm_science, creedmoor, radwag_ps_r2, sartorius, generic, simulated | at restart (the reader task is made once; ADR-006). The stored choice: a test stand-in never shows here | `s0` (an index; an unknown one was stored) |
| `baud` | choice | 4800, 9600, 19200 | now | `s1` (an index; an unknown one set 0 baud) |
| `format` | choice | 8n1, 7n1 | now | `s2` (an index) |

### `/api/v1/settings/gate`, saved at `/api/v1/settings/gate/save` (refused while `in_throw`)

| Field | Type | Unit | Limits | When | Today |
|---|---|---|---|---|---|
| `enable` | boolean | | | now | `c0` |
| `shutter0_close_duty`, `shutter0_open_duty`, `shutter1_close_duty`, `shutter1_open_duty` | number | of the PWM's period | 0.02 to 0.13 *p*: a servo's pulse at 50 Hz (the defaults are 0.05 and 0.09) | the next move | `c1` to `c4` (outside 0 to 1 overflowed) |
| `close_speed_per_s`, `open_speed_per_s` | number | full travels a second | 0.01 to 100 *p* | the next move | `c5`, `c6` (`..._pct_s`: named percent, used as full travels) |

### `/api/v1/settings/leds`, saved at `/api/v1/settings/leds/save`

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `backlight_colour`, `led1_colour`, `led2_colour` | colour | | now when no mode has the LEDs; otherwise when it gives them back, so a verdict's colour is not painted over | `bl`, `l1`, `l2` |
| `pwm3_chain_count` | integer | 1 to 16 | now | `l3` (no check) |
| `pwm3_colour_order` | choice | rgb, grb | now | `l5` (no check) |
| `pwm3_rgbw` | boolean | | at restart | `l4` |

### `/api/v1/settings/display`, saved at `/api/v1/settings/display/save`

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `invert_encoder` | boolean | | now | `b0` |
| `rotation` | choice | 0, 180 (90 and 270 turn a 128 by 64 screen on its side) | at restart | `b1` (an index) |

### `/api/v1/settings/wifi`, saved at `/api/v1/settings/wifi/save`

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `enable` | boolean | | at restart | `w4` |
| `ssid` | string | 1 to 31 bytes | at restart | `w0` |
| `key` | string, write only | 0 to 63 bytes; never in a reply, nor in the export | at restart | `w1` |
| `auth` | choice | open, wpa_tkip, wpa2_aes, wpa2_mixed | at restart | `w2` (an index) |
| `timeout_ms` | integer | 1,000 to 120,000 *p* | at restart | `w3` (cut to 16 bits: 70,000 became 4,464) |

## What it must grow into

Read 2026-11-15 in every ADR, the menu's forms (`mui_menu.c`, `menu.c`) and the TODOs in `src/`. Each later need has its place now, so v1 grows without a break:

| Need | From | Its place |
|---|---|---|
| A charge profile: a name, a weight, the powder profile it uses | ADR-014 | `/api/v1/charge_profiles`; `charge_profile` beside `target` and `powder_profile` in the charge. Powder profiles are named apart now so it has room |
| The target weight kept, with the charge profile | ADR-014, PR #109's need | The charge's `target` is there now; whether it is stored is ADR-014's |
| Stop thresholds per profile | ADR-009, ADR-014 | A profile's own field, `null` for "the default in `settings/charge`", which stays |
| Every throw recorded, downloaded, drawn | ADR-009, ADR-008 | `/api/v1/throws` (a list with each throw's summary) and `/api/v1/throws/{throw}` (its samples, written as they are read out, chunked), by the throw's number the charge and its events carry now |
| Test functions: a recorded test throw, a motor at a speed for a time, the scale's readings and rate | ADR-009 | Jobs: `POST /api/v1/motors/{coarse|fine}/run`; a test throw is a charge whose start asks for a recording; the scale's rate is in `GET /api/v1/scale` now |
| Automatic tuning | ADR-009 | A job: `/api/v1/tuning`, with its trials' events in the log and its result saved to a powder profile |
| A press to skip the current charge (issue #75); ladder loading (issue #106) | ADR-009's list | Actions on the charge job, or a series of charge profiles; added, not changed |
| An update from the browser: upload, check, install; refused while a job runs | ADR-004 | A job: `/api/v1/update` (the running and the staged version), its image `PUT` as `application/octet-stream` in pieces, `POST .../install` |
| What a board carries | ADR-005 | `features` in the index; absent parts answer 404 and are not listed |
| A unit's version in its image | ADR-004 | `firmware` in the index |
| Settings forms and screens drawn from the description | ADR-008, ADR-010 | `label`, `group`, `step`, choice labels in the schema now |
| A colour or larger screen, or LVGL's frame | ADR-010 | `format` in `GET /api/v1/display`; the bytes apart at `/display/frame` |
| Another chip | ADR-007 | `board`, `features`; what is chip-only (lwIP's pools) is under `debug` |
| The scale's own calibration (the A&D's CAL), its display, its fast report (TODOs in `and_scale.c`, `gng_scale.c`) | the menu, the scale drivers | `actions` in `GET /api/v1/scale`, and `POST /api/v1/scale/{action}` for each make that can; the calibration a job |
| Zero on scales whose drivers cannot yet (Steinberg, Creedmoor, Sartorius: TODOs) | the scale drivers | 501 `not_supported` now; `actions` lists zero once a driver can |
| The weight's unit for every make | the scale drivers | `unit`, null today but for the A&D |
| Which unit is which, among several | OWMC's ADR-013 (the owner's review) | `hostname` and `unit_id` in the index, `GET /api/v1/network`, and the mDNS announcement with the API's and the firmware's versions |
| The console and the API saying the same | ADR-001, the owner | One table of names for both (gate 3) |
| A password | its own ADR | A header, paths unchanged |
| Waiting for news in place of polling | ADR-008, ADR-009 | `?wait=` on the event log, or a push channel |

**Found while checking** (`wireless.c`, `eeprom.c`): the unit's host name is `opentrickler-` and the first four characters of its id, copied into `char id[4]` with no ending and printed with `%s`. `snprintf` reads on past the four bytes, and only its limit of 18 keeps the name right. Two units match in those four characters about once in 65,536, if the id is random. The bench's Pico W has the id `9CE00000`, its last five characters 0, so how random `rnd()` makes it (`eeprom.c`) is to be read first. For gate 4, where the mDNS announcement changes: the id written with its ending, and enough of it to tell units apart.

## Pages, not the API

`/` (the portal, or the set-up wizard while the unit is its own access point), `/wizard`, `/display_mirror` and `/mobile` stay where they are. They are pages, not the API, and are free to change in ADR-008.

## Gone, with nothing in their place

- `/rest/button_control`. No page calls it; the console keeps `press`. The API offers commands, not button presses.
- `profile_config`'s `pf` as a way to select: `PATCH /api/v1/charge {"powder_profile": n}`.
- `scale_action`, `system_control`, the two `*_state` paths, the `*_config` paths: each is in a table above.

## For the owner

1. **Live until saved**, as the portal's Apply and Save buttons do today, with `saved` in every reply. The other way: every write is stored at once, and nothing is lost at a restart. An agent then cannot try a value without keeping it, and every slider move is an EEPROM write.
2. **The words.** `powder_profiles` and, with ADR-014, `charge_profiles`, in your words for the two. The **charge** is the job (charge mode), a **throw** is one cup, numbered.
3. **The limits marked *p*.** The code has none for them today; these are first figures.
4. **What is refused, and when.** While `in_throw` (from zeroing to the verdict): the charge's settings, the profile in use, the scale's and the gate's settings, the gate, a zero after `zeroing`. While any job runs: the motors' settings, another job, a restart, an erase. Between throws, a profile's gains can be changed, as ADR-009's tuning will need.
5. **A remote zero is refused while the unit waits for the cup's return** (the review's S3, argued below). A remote caller fixes a drifted cup by stop, start, zero.

## The review (2026-11-15)

A second model (Fable 5.2, a session of its own, read only) reviewed the table at d2364e6 against every ADR and the code, at the owner's word: "Once both of you are agreeing then I'm happy to move on." It found 4 blocking faults, 15 to fix and 9 minor; the author checked each claim in the code, and took 26 as proposed. Its report: `review_api_v1_2026-11-15.md`, kept with the session's notes, not in the repository.

| | Found | Now |
|---|---|---|
| B1 | A bulk import cannot be built on a flat-JSON, 2 KB server, and what a GET gives cannot be written back | Export only; the import is the client's, resource by resource; JSON bodies 1 KB |
| B2 | A charge that ended on a fault had no state; "charge" meant the loop and one throw; throws had no number; "a throw" ended a second before its verdict | `fault`; charge and throw apart; `throw`; `in_throw` until the verdict |
| B3 | A stop could be refused, and nothing said one job at a time | A stop never fails; `POST /api/v1/stop`; `activity`; `not_running` |
| B4 | The profile chosen was said to be stored as the menu's is: the menu's is not, and it shares a block with all profiles | Live until saved, like the rest |
| S1 | The charge read one shape and wrote another; the menu's digits are a second target | GET's fields are PATCH's; one target, the menu's digits read and write it |
| S2 | "Refused during a throw" was a check, then a write | One lock for what a throw reads, held by the throw (gate 3) |
| S3 | Zero refused in `zeroing`, where it is used; taken while waiting for the cup, where it starts a throw | Taken idle and in `zeroing`; refused after (the author refused it in `waiting_for_cup_return` too) |
| S4 | DNS rebinding passes the cross-site guard | `Host` checked, 403; page requests redirected; 415 for a POST with no type |
| S5 | A save during `sim on` stores the simulated scale | A stand-in is never stored |
| S6 | A colour write paints over a verdict | Colours apply when no mode has the LEDs |
| S7 | Limits that hang on another resource's value | Constants; the live limit at use |
| S8 | Three places to save; `saved` blind to the menu | One save path each, named in the schema; `saved` from CRCs |
| S9 | Events said no who, standing faults would flood, gaps unseen | `by`; changes, not states; `seq` by one; `now_ms`; 64-bit clock; `boot` from `pico_rand` |
| S10 | Choices with no labels; `system.net` as contract | Choices as value and label; ETag; states, kinds and codes in the schema; `guide`; `debug` outside v1 |
| S11 | Three decimals lose small gains | A number read and written back changes nothing |
| S12 | Motor settings written between throws, with the motors enabled | Refused while any job runs |
| S13 | A stalled reply, six idle connections, long headers, `select` and the heap, pools | Gate 2's server (README) |
| S14 | No weight unit, though the A&D's frame carries one | `unit` in the scale |
| S15 | Restart taken mid-throw; erase's effects unsaid | Restart refused while a job runs; erase says what follows |
| M1 to M9 | Names, `rev`, the frame, limits, 404 against 501, growth rules, the gate's states, server details, two things said plainly | In the tables above; the author's rule for unit suffixes; `seconds` on a cleanup (the author's) |
