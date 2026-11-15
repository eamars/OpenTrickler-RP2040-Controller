# API v1: the table, for approval

ADR-003's API, written for gate 2's approval (2026-11-15) and checked against every ADR, the menu and the TODOs in the code ("What it must grow into", below). Once it is approved, this is the contract: v1 only grows. Once it is built, the unit's own description (`GET /api/v1/schema`) says what it carries, from the same tables as the firmware, and this file is kept to it: the scripted client checks the two against each other.

Each field below comes from today's stored structs, read in the code on 2026-11-15. Where today's code has no limit, the limit here is a **proposal** (marked *p*), for the owner to look at.

## The rules

- **Paths** start with `/api/v1`. Names are `snake_case` and say what a thing is; no codes.
- **A read is a GET, and it changes nothing.** Not a selection, not an event, not a light.
- **A write is a PATCH** with a JSON object of the fields to change: one field or many. The whole request is checked first. If any field is refused, nothing in the request is written. The reply is the whole resource as it is now.
- **An action is a POST** to a path named for it (`/charge/start`), with a JSON body (`{}` when it takes nothing). The reply is the state it led to: 202 when the action goes on after the reply (a charge, a restart), 200 when it is done.
- **A write or an action must be sent as `application/json`**, or as `application/octet-stream` where a path takes bytes (ADR-004's image). Anything else is refused with 415. The unit sends no CORS headers and answers `OPTIONS` with 405. A browser asks a server's leave before it sends either type from another site, and the unit never gives it: so a page from another site cannot make a browser write to the unit.
- **Settings are live when written and stored when saved.** A PATCH changes the setting at once (or at the next start, where the schema says so). `POST .../save` stores it. Every settings resource carries `"saved": true|false`, so what a restart would lose is visible.
- **Numbers** are JSON numbers. A value that is not there, such as a weight with no scale reading, is `null`.
- **Choices** are strings or numbers from a list given in the schema: `"driver": "and_fxi"`, `"baud": 9600`, `"rotation": 180`. No indexes.
- **Weights** are in the scale's own unit, grains or grams as the scale is set. The firmware does not know which.
- **Colours** are `"#rrggbb"`.
- **Ids are opaque numbers.** Powder profiles have the ids 0 to 7 today, but a client reads the list and does not count on that.
- **Unknown fields in a request are refused** (400 `unknown_field`), so a typo is caught rather than ignored.

### Growing within v1

What may change within v1, and what a client does about it:
- **New paths and new fields** are added. A client ignores a field it does not know.
- **A choice may gain values:** a new scale driver, a new charge state (ADR-009's top-up), a new end reason, a new event kind. A client treats a value it does not know as "something else", and does not fail.
- **A field never goes, and never changes its meaning or its unit.** A setting that moves keeps its old place as the default. For example, if the stop thresholds become per profile (ADR-009, ADR-014), `settings/charge` keeps them, and a profile's own field, `null` when not set, uses that default.
- **A part a board does not carry** (ADR-005) is absent: its paths answer 404, and the index and the schema do not list it. A client learns what is there from `GET /api/v1`, not from this file.
- **Long work is a job,** in one shape: a resource with a `state`, `POST .../start` and `POST .../stop`, refusals with 409, and its events in the unit's one event log. A charge and a cleanup are jobs now; tuning (ADR-009), an update (ADR-004), the scale's calibration (the menu's) and a motor's test run (ADR-009) will be.
- **A password,** if its own ADR adds one, comes as a header (`Authorization`). Paths and bodies stay as they are; a client adds the header.
- **A change that breaks any of this** is `/api/v2`, served beside v1 for a time.

### Status and errors

| Status | When |
|---|---|
| 200 | A read, a write, an action done |
| 202 | An action under way: a charge started or stopped, a restart |
| 400 | Not JSON, an unknown field, a wrong type, a value outside its limits, a missing confirmation |
| 404 | No such path, no such id, or a part this board does not carry |
| 405 | The path exists, the method does not; `Allow` lists the methods |
| 409 | Refused at this moment: a throw or a cleanup is running, a driver is at fault, the gate is disabled |
| 411 | A body with no `Content-Length` (a chunked one included) |
| 413 | The request is larger than the unit takes (headers 1 KB, a JSON body 2 KB) |
| 415 | A write not sent as `application/json` (or `application/octet-stream` where bytes are taken) |
| 500 | The unit failed to do it, such as a save the EEPROM did not take |
| 501 | This board or this scale cannot do it: the Steinberg, Creedmoor and Sartorius drivers cannot zero their scale (TODOs in their code) |

Every error has the same body:

```json
{"error": {"code": "out_of_range", "field": "fine_stop_threshold", "message": "12 is above the most, 10", "min": 0, "max": 10}}
```

The codes are `bad_json`, `length_required`, `unknown_field`, `wrong_type`, `out_of_range`, `not_a_choice`, `read_only`, `confirm_missing`, `not_found`, `method_not_allowed`, `busy` (with `"state"`), `driver_fault` (with `"fault"`), `gate_disabled`, `not_supported`, `too_large`, `wrong_media_type` and `failed`. `field`, `min`, `max`, `choices`, `state` and `fault` are given when they apply. More codes may come (the rule above).

## What the unit is

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1` | | `api` ("1"); `firmware` {`version`, `vcs`, `build`}; `board` (pico_w, pico2_w; esp32_s3_pico with ADR-007); `unit_id`; `hostname`; `boot`, a number new at each start; `uptime_s`; `features`, what this board carries (ADR-005); `resources`: each path with its methods and a line on what it is |
| `GET /api/v1/schema` | | Every resource's fields **and every action's body**. For each field: `name`, `label` (a few words for a form), `group` (a heading), `type` (number, integer, boolean, string, colour, choice), `unit`, `min`, `max`, `step`, `choices`, `access` (read, read_write, write_only), `applies` (now, at_restart), `refused_during` (throw, throw_or_cleanup, or none), and a line on what it is. ADR-008's forms and ADR-010's settings screen are drawn from it |
| `GET /api/v1/system` | | `firmware`, `board`, `unit_id`, `uptime_s`; `heap_free` (FreeRTOS, bytes); `net`, what the console's `net` gives |
| `GET /api/v1/network` | | `mode` (station, access_point), `ssid`, `address`, `hostname` (the mDNS name), `rssi`: the menu's Wi-Fi info, and how a person or an agent finds which unit is which |
| `POST /api/v1/system/save` | `{}` | 200 when every settings resource and the powder profiles were stored; 500 naming the one that failed |
| `POST /api/v1/system/restart` | `{}` | 202; the unit restarts once the reply is out |
| `POST /api/v1/system/erase` | `{"confirm": "erase"}` | 202. The stored settings are erased, then the unit restarts. 400 `confirm_missing` without the confirmation; 409 during a throw or a cleanup |

## The event log

One log for the whole unit, which any number of clients read, each from where it left off. It replaces the charge state's events, which were cleared by whoever read them first.

| Method and path | Reply |
|---|---|
| `GET /api/v1/events?after=N` | `boot` (as in the index: when it changes, the unit restarted and the numbers began again); `events`, each with `seq`, `at_ms` (the unit's clock), `source` and `kind`, and what that kind carries; `next`, the number to ask after next time. The newest 64 are kept *p* |

The kinds now:

| Source | Kind | Carries |
|---|---|---|
| charge | started | `target`, `powder_profile` |
| charge | paused | the pause's number in this throw (ADR-013: no reading for 3 of the scale's intervals) |
| charge | resumed | `weight`, the settled weight it went on from |
| charge | finished | `result` (good, under, over), `weight`, `elapsed_s` |
| charge | ended | `reason` (stopped, scale_no_reading, scale_unsteady, scale_unreliable, driver_fault), `fault` when a driver's |
| cleanup | started, stopped | |
| motors | fault | `motor`, `fault` |
| system | started | `firmware`, at each start |

Later ADRs add sources and kinds (the rule above). Waiting for the next event in place of polling (`?wait=s`, or a push channel) may come with ADR-008 or ADR-009.

## Charge

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/charge` | | `state` (idle, zeroing, charging, waiting_for_cup_removal, waiting_for_cup_return); `paused` (true while a throw waits for readings); `next` {`target`, `powder_profile` {`id`, `name`}}, what the next throw uses; `weight` (null with no reading); `elapsed_s` (this throw, or the last); `last` {`result` (good, under, over, none), `reason` (as `ended`'s, or finished), `weight`}; `event_seq`, the newest event's number |
| `PATCH /api/v1/charge` | `{"target": 30.0}`, `{"powder_profile": 3}`, or both | 200. What the next throw uses. `target` 0.001 to 500 *p*. The powder profile chosen is stored, as the menu's choice is today; the target is not (ADR-014 decides). 409 `busy` during a throw |
| `POST /api/v1/charge/start` | `{}`, or `{"target": 30.0}` and `"powder_profile"` to set them first | 202 with the charge. 409 `busy` unless idle; 409 `driver_fault` |
| `POST /api/v1/charge/stop` | `{}` | 202 with the charge; 409 `busy` when idle |

ADR-014 adds a charge profile: `next.charge_profile`, and `"charge_profile"` in the PATCH and in start. A target or a powder profile given beside it overrides it for that charge.

## Cleanup

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/cleanup` | | `state` (idle, running), `speed_rps` |
| `POST /api/v1/cleanup/start` | `{}` | 202. 409 `busy` while charge mode runs; 409 `driver_fault` |
| `POST /api/v1/cleanup/stop` | `{}` | 202; 409 `busy` when idle |
| `PATCH /api/v1/cleanup` | `{"speed_rps": 2.0}` | 200. Both tricklers turn at that speed. Limits: ± the lower of the two motors' `max_speed_rps`. 409 `busy` when idle |

## The scale, the gate, the motors, the screen

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/scale` | | `weight` (null with no reading), `driver`, `simulated`, `usual_interval_ms` (the scale's rhythm, ADR-013), `bad_frames`, `actions`: what this make of scale can be asked to do (zero now; its calibration, its display, its fast report later, by make) |
| `POST /api/v1/scale/zero` | `{}` | 200. 501 `not_supported` when the make cannot; 409 `busy` during a throw |
| `GET /api/v1/gate` | | `state` (open, closed, disabled), `ratio` (0 open to 1 closed, null when disabled) |
| `PATCH /api/v1/gate` | `{"ratio": 0.5}` or `{"state": "open"}` | 200. 409 `gate_disabled` when the gate is off in its settings; 409 `busy` during a throw |
| `GET /api/v1/motors` | | For `coarse` and `fine`: `speed_rps` (now, ramps included), `driver` (ok, absent, short, hot, reset, supply) |
| `GET /api/v1/display` | | `width`, `height`, `format` (`u8g2_tiles`: one bit a pixel, in u8g2's tiles), `frame` (the screen's bytes, base64). When LVGL draws the screen (ADR-010), or a larger screen comes, `format` says so |

## Powder profiles

What ADR-014 calls a powder profile: how a powder flows, and the controller's gains for it. Named so now, so that ADR-014's charge profiles have their own place beside them.

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/powder_profiles` | | `saved`, and `profiles`: for each its `id` and `name`. Which one the next throw uses is `GET /api/v1/charge`'s `next` |
| `GET /api/v1/powder_profiles/{id}` | | Its fields, below; 404 for an id not in the list |
| `PATCH /api/v1/powder_profiles/{id}` | Fields, below | 200; 409 `busy` during a throw when it is the one in use |
| `POST /api/v1/powder_profiles/save` | `{}` | 200: all of them, and the choice, stored |

| Field | Type | Unit | Limits | Today |
|---|---|---|---|---|
| `name` | string | | 0 to 15 bytes | `p2`; 16 were stored with no ending |
| `coarse_kp`, `coarse_ki`, `coarse_kd` | number | | 0 or more *p* | `p3` to `p5` |
| `coarse_min_speed_rps`, `coarse_max_speed_rps` | number | rev/s | 0 to the coarse motor's `max_speed_rps`, min not above max *p* | `p6`, `p7` (`..._flow_speed_rps`) |
| `fine_kp`, `fine_ki`, `fine_kd` | number | | 0 or more *p* | `p8` to `p10` |
| `fine_min_speed_rps`, `fine_max_speed_rps` | number | rev/s | as coarse, with the fine motor *p* | `p11`, `p12` |
| `rev`, `compatibility` | integer | | read only | `p0`, `p1` |

Eight slots today, so no POST to make one and no DELETE. Those may come (ADR-014, or ADR-009's tuning saving a new one) without changing the paths.

## Settings

Each is `GET`, `PATCH`, and `POST .../save` (`{}`), with `saved` in its reply. "When" is when a change takes effect; "Refused" is when a write to it is refused with 409.

**All of them at once:** `GET /api/v1/settings` gives every settings resource and the powder profiles in one document, with the firmware's version. `PATCH /api/v1/settings` takes such a document, or part of it, checked whole. This is the portal's export and import, `rc_run.py settings`, and the copy taken before a flash (AGENTS.md).

### `/api/v1/settings/charge` (refused during a throw)

| Field | Type | Unit | Limits | When | Today |
|---|---|---|---|---|---|
| `coarse_stop_threshold` | number | weight | 0 to 100 *p* | now | `c5` |
| `fine_stop_threshold` | number | weight | 0 to 10 *p* | now | `c6` |
| `zero_sd_margin` | number | weight | 0 to 1 *p* | now | `c7` (`set_point_sd_margin`) |
| `zero_mean_margin` | number | weight | 0 to 1 *p* | now | `c8` (`set_point_mean_margin`) |
| `decimal_places` | choice | | 2, 3 | now | `c9` (an index) |
| `coarse_stop_gate_ratio` | number or null | | 0 to 1; null: the gate is not moved | now | `c13` (-1 for not moved) |
| `coarse_stop_backoff_enable` | boolean | | | now | `c14` |
| `coarse_stop_backoff_turns` | choice | turns | 0.125, 0.25, 0.5, 1 | now | `c15` (any number) |
| `coarse_stop_backoff_speed_rps` | number | rev/s | 0 to the coarse motor's `max_speed_rps`; 0 is its minimum speed *p* | now | `c16` |
| `precharge_enable` | boolean | | | now | `c10` |
| `precharge_time_ms` | integer | ms | 0 to 5,000 *p* (RST is not read while it runs) | now | `c11` |
| `precharge_speed_rps` | number | rev/s | 0 to the coarse motor's `max_speed_rps` *p* | now | `c12` |
| `colour_good`, `colour_under`, `colour_over`, `colour_not_ready` | colour | | | now | `c1` to `c4` (`neopixel_..._colour`) |

### `/api/v1/settings/motors/coarse` and `/api/v1/settings/motors/fine` (refused during a throw or a cleanup)

The two are stored as one block, so saving either saves both. Today's checks (`motor_persistent_config_is_valid`) stay.

| Field | Type | Unit | Limits | When | Today |
|---|---|---|---|---|---|
| `full_steps_per_rotation` | integer | steps | 20 to 2,000 | now | `m1` |
| `current_ma` | integer | mA RMS | 10 to 2,000 | now (the driver is set up again) | `m2` (wrapped at 65,536 before its check) |
| `microsteps` | choice | | 1, 2, 4, ... 256 | now (set up again) | `m3` |
| `max_speed_rps` | integer | rev/s | 1 to 100 | now | `m4` |
| `r_sense` | integer | mΩ | 50 to 1,000 | now (set up again) | `m5` |
| `acceleration` | number | rev/s² | 0.1 to 10,000 | now | `m0` (`angular_acceleration`) |
| `min_speed_rps` | number | rev/s | 0 to `max_speed_rps` | now | `m6` |
| `gear_ratio` | number | | 0.01 to 100 | now | `m7` |
| `invert_direction` | boolean | | | now | `m9` |
| `invert_enable` | boolean | | | when the motor is next enabled | `m8` |

### `/api/v1/settings/scale` (refused during a throw)

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `driver` | choice | and_fxi, steinberg_sbs, gng_jjb, ussolid_jfdbs, jm_science, creedmoor, radwag_ps_r2, sartorius, generic, simulated | at restart (the reader task is made once; ADR-006) | `s0` (an index; an unknown one was stored) |
| `baud` | choice | 4800, 9600, 19200 | now | `s1` (an index; an unknown one set 0 baud) |
| `format` | choice | 8n1, 7n1 | now | `s2` (an index) |

### `/api/v1/settings/gate` (refused during a throw)

| Field | Type | Unit | Limits | When | Today |
|---|---|---|---|---|---|
| `enable` | boolean | | | now | `c0` |
| `shutter0_close_duty`, `shutter0_open_duty`, `shutter1_close_duty`, `shutter1_open_duty` | number | of the PWM's full scale | 0 to 1 *p* | the next move | `c1` to `c4` (outside 0 to 1 overflowed) |
| `close_speed`, `open_speed` | number | full travels a second | 0.01 to 100 *p* | the next move | `c5`, `c6` (`..._pct_s`: named percent, used as full travels) |

### `/api/v1/settings/leds`

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `backlight_colour`, `led1_colour`, `led2_colour` | colour | | now | `bl`, `l1`, `l2` |
| `chain_count` | integer | 1 to 16 | now | `l3` (no check) |
| `colour_order` | choice | rgb, grb | now | `l5` (no check) |
| `rgbw` | boolean | | at restart | `l4` |

### `/api/v1/settings/display`

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `invert_encoder` | boolean | | now | `b0` |
| `rotation` | choice | 0, 90, 180, 270 | at restart | `b1` (an index) |

### `/api/v1/settings/wifi`

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `enable` | boolean | | at restart | `w4` |
| `ssid` | string | 1 to 31 bytes | at restart | `w0` |
| `key` | string, write only | 0 to 63 bytes; never in a reply | at restart | `w1` |
| `auth` | choice | open, wpa_tkip, wpa2_aes, wpa2_mixed | at restart | `w2` (an index) |
| `timeout_ms` | integer | 1,000 to 120,000 *p* | at restart | `w3` (cut to 16 bits: 70,000 became 4,464) |

## What it must grow into

Read 2026-11-15 in every ADR, the menu's forms (`mui_menu.c`, `menu.c`) and the TODOs in `src/`. Each later need has its place now, so v1 grows without a break:

| Need | From | Its place |
|---|---|---|
| A charge profile: a name, a weight, the powder profile it uses | ADR-014 | `/api/v1/charge_profiles`; `next.charge_profile` and `"charge_profile"` in the charge's PATCH and start. Powder profiles are named apart now so it has room |
| The target weight kept, with the charge profile | ADR-014, PR #109's need | `next.target` is there now; whether it is stored is ADR-014's |
| Stop thresholds per profile | ADR-009, ADR-014 | A profile's own field, `null` for "the default in `settings/charge`", which stays |
| Every throw recorded, downloaded, drawn | ADR-009, ADR-008 | `/api/v1/throws` (a list with each throw's summary) and `/api/v1/throws/{id}` (its samples, written as they are read out, chunked) |
| Test functions: a recorded test throw, a motor at a speed for a time, the scale's readings and rate | ADR-009 | Jobs: `POST /api/v1/motors/{coarse|fine}/run`; a test throw is a charge whose start asks for a recording; the scale's rate is in `GET /api/v1/scale` now |
| Automatic tuning | ADR-009 | A job: `/api/v1/tuning`, with its trials' events in the log and its result saved to a powder profile |
| A press to skip the current charge (issue #75); ladder loading (issue #106) | ADR-009's list | Actions on the charge job, or a series of charge profiles; added, not changed |
| An update from the browser: upload, check, install; refused during a throw | ADR-004 | A job: `/api/v1/update` (the running and the staged version), its image `PUT` as `application/octet-stream` in pieces, `POST .../install` |
| What a board carries | ADR-005 | `features` in the index; absent parts answer 404 and are not listed |
| A unit's version in its image | ADR-004 | `firmware` in the index |
| Settings forms and screens drawn from the description | ADR-008, ADR-010 | `label`, `group`, `step` in the schema now |
| A colour or larger screen, or LVGL's frame | ADR-010 | `format` in `GET /api/v1/display` |
| Another chip | ADR-007 | `board`, `features` |
| The scale's own calibration (the A&D's CAL), its display, its fast report (TODOs in `and_scale.c`, `gng_scale.c`) | the menu, the scale drivers | `actions` in `GET /api/v1/scale`, and `POST /api/v1/scale/{action}` for each make that can |
| Zero on scales whose drivers cannot yet (Steinberg, Creedmoor, Sartorius: TODOs) | the scale drivers | 501 `not_supported` now; `actions` lists zero once a driver can |
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
2. **The names `powder_profiles` and, with ADR-014, `charge_profiles`,** in your words for the two. What the next throw uses sits in the charge (`next`), not in either list, so a charge profile can take it over.
3. **The limits marked *p*.** The code has none for them today; these are first figures.
4. **What is refused during a throw.** A throw is the states `zeroing` and `charging`. Between throws (waiting for the cup) and while idle, settings are written. So a profile's gains can be changed between two throws, as ADR-009's tuning will need.
