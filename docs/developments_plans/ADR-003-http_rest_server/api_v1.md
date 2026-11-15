# API v1: the table, for approval

ADR-003's API, written for gate 2's approval (2026-11-15). Once it is approved, this is the contract: v1 only grows. Once it is built, the unit's own description (`GET /api/v1/schema`) says what it carries, from the same tables as the firmware, and this file is kept to it: the scripted client checks the two against each other.

Each field below comes from today's stored struct, read in the code on 2026-11-15. Where today's code has no limit, the limit here is a **proposal** (marked *p*), for the owner to look at.

## The rules

- **Paths** start with `/api/v1`. Names are `snake_case` and say what a thing is; no codes.
- **A read is a GET, and it changes nothing.** Not a selection, not an event, not a light.
- **A write is a PATCH** with a JSON object of the fields to change: one field or many. The whole request is checked first. If any field is refused, nothing in the request is written. The reply is the whole resource as it is now.
- **An action is a POST** to a path named for it (`/charge/start`), with a JSON body (`{}` when it takes nothing). The reply is the state it led to: 202 when the action goes on after the reply (a charge, a restart), 200 when it is done.
- **A write or an action must be sent as `Content-Type: application/json`**, or it is refused with 415. The unit sends no CORS headers and answers `OPTIONS` with 405. So a page from another site cannot make a browser send one.
- **Settings are live when written and stored when saved.** A PATCH changes the setting at once (or at the next start, where the schema says so). `POST .../save` stores it. Every settings resource carries `"saved": true|false`, so what a restart would lose is visible.
- **Numbers** are JSON numbers. A value that is not there, such as a weight with no scale reading, is `null`.
- **Choices** are strings or numbers from a list given in the schema: `"driver": "and_fxi"`, `"baud": 9600`, `"rotation": 180`. No indexes.
- **Weights** are in the scale's own unit, grains or grams as the scale is set. The firmware does not know which.
- **Colours** are `"#rrggbb"`.
- **Unknown fields are refused** (400 `unknown_field`), so a typo is caught rather than ignored.

### Status and errors

| Status | When |
|---|---|
| 200 | A read, a write, an action done |
| 202 | An action under way: a charge started or stopped, a restart |
| 400 | Not JSON, an unknown field, a wrong type, a value outside its limits, a missing confirmation |
| 404 | No such path, or no such profile |
| 405 | The path exists, the method does not; `Allow` lists the methods |
| 409 | Refused at this moment: a throw or a cleanup is running, a driver is at fault, the gate is disabled |
| 411 | A body with no `Content-Length` (a chunked one included) |
| 413 | The request is larger than the unit takes (headers 1 KB, a JSON body 2 KB) |
| 415 | A write not sent as `application/json` |
| 500 | The unit failed to do it, such as a save the EEPROM did not take |

Every error has the same body:

```json
{"error": {"code": "out_of_range", "field": "fine_stop_threshold", "message": "12 is above the most, 10", "min": 0, "max": 10}}
```

The codes are `bad_json`, `length_required`, `unknown_field`, `wrong_type`, `out_of_range`, `not_a_choice`, `read_only`, `confirm_missing`, `not_found`, `method_not_allowed`, `busy` (with `"state"`), `driver_fault` (with `"fault"`), `gate_disabled`, `too_large`, `wrong_media_type` and `failed`. `field`, `min`, `max`, `choices`, `state` and `fault` are given when they apply.

## What the unit is

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1` | | `api` ("1"), `firmware` {`version`, `vcs`, `build`}, `board` ("pico_w", "pico2_w"), `unit_id`, `uptime_s`, `resources`: one entry for each path, with its methods and a line on what it is |
| `GET /api/v1/schema` | | Every resource's fields. For each field: `name`, `type` (number, integer, boolean, string, colour, choice), `unit`, `min`, `max`, `choices`, `access` (read, read_write, write_only), `applies` (now, at_restart), `refused_during` (throw, throw_or_cleanup, or none), and a line on what it is |
| `GET /api/v1/system` | | `firmware`, `board`, `unit_id`, `uptime_s`; `heap_free` (FreeRTOS, bytes); `net`, what the console's `net` gives |
| `POST /api/v1/system/save` | `{}` | 200 when every settings resource was stored; 500 naming the one that failed |
| `POST /api/v1/system/restart` | `{}` | 202; the unit restarts once the reply is out |
| `POST /api/v1/system/erase` | `{"confirm": "erase"}` | 202. The stored settings are erased, then the unit restarts. 400 `confirm_missing` without the confirmation; 409 during a throw or a cleanup |

## Charge

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/charge` | | `state` (idle, zeroing, charging, waiting_for_cup_removal, waiting_for_cup_return); `target`; `weight` (null with no reading); `profile` {`index`, `name`}; `elapsed_s` (of this throw, or the last); `last` {`result` (good, under, over, none), `weight`}; `event_seq`, the number of the newest event |
| `POST /api/v1/charge/start` | `{"target": 30.0}` | 202 with the charge. 409 `busy` unless idle; 409 `driver_fault`; 400 for a target outside 0.001 to 500 *p* |
| `POST /api/v1/charge/stop` | `{}` | 202 with the charge; 409 `busy` when idle |
| `GET /api/v1/charge/events?after=N` | | `events`: each with `seq`, `at_ms` (the unit's clock), `kind` (started, finished, stopped, fault), and for finished its `result` and `weight`, for fault its `fault`. Only events after N; the newest 32 are kept. `next` is the number to ask after next time. Any number of clients each read all of them |

## Cleanup

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/cleanup` | | `running`, `speed_rps` |
| `POST /api/v1/cleanup/start` | `{}` | 202. 409 `busy` while charge mode runs; 409 `driver_fault` |
| `POST /api/v1/cleanup/stop` | `{}` | 202; 409 `busy` when not running |
| `PATCH /api/v1/cleanup` | `{"speed_rps": 2.0}` | 200. Both tricklers turn at that speed. Limits: ± the lower of the two motors' `max_speed_rps`. 409 `busy` when not running |

## The scale, the gate, the motors, the screen

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/scale` | | `weight` (null with no reading), `driver`, `simulated`, `usual_interval_ms` (the scale's rhythm, ADR-013), `bad_frames` |
| `POST /api/v1/scale/zero` | `{}` | 200. 409 `busy` during a throw |
| `GET /api/v1/gate` | | `state` (open, closed, disabled), `ratio` (0 open to 1 closed, null when disabled) |
| `PATCH /api/v1/gate` | `{"ratio": 0.5}` or `{"state": "open"}` | 200. 409 `gate_disabled` when the gate is off in its settings; 409 `busy` during a throw |
| `GET /api/v1/motors` | | For `coarse` and `fine`: `speed_rps` (now, ramps included), `driver` (ok, absent, short, hot, reset, supply) |
| `GET /api/v1/display` | | The screen's 1,024 bytes as `application/octet-stream`, with a header: 128 by 64, one bit a pixel, in u8g2's tiles |

## Profiles

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/profiles` | | `selected`, `saved`, and `profiles`: for each its `index` and `name` |
| `PATCH /api/v1/profiles` | `{"selected": 3}` | 200. 0 to 7; 409 `busy` during a throw |
| `GET /api/v1/profiles/{index}` | | Its fields, below; 404 outside 0 to 7 |
| `PATCH /api/v1/profiles/{index}` | Fields, below | 200; 409 `busy` during a throw when it is the selected profile |
| `POST /api/v1/profiles/save` | `{}` | 200: all eight and the selection stored |

| Field | Type | Unit | Limits | Today |
|---|---|---|---|---|
| `name` | string | | 0 to 15 bytes | `p2`; 16 were stored with no ending |
| `coarse_kp`, `coarse_ki`, `coarse_kd` | number | | 0 or more *p* | `p3` to `p5` |
| `coarse_min_speed_rps`, `coarse_max_speed_rps` | number | rev/s | 0 to the coarse motor's `max_speed_rps`, min not above max *p* | `p6`, `p7` (`..._flow_speed_rps`) |
| `fine_kp`, `fine_ki`, `fine_kd` | number | | 0 or more *p* | `p8` to `p10` |
| `fine_min_speed_rps`, `fine_max_speed_rps` | number | rev/s | as coarse, with the fine motor *p* | `p11`, `p12` |
| `rev`, `compatibility` | integer | | read only | `p0`, `p1` |

## Settings

Each is `GET`, `PATCH`, and `POST .../save` (`{}`), with `saved` in its reply. "When" is when a change takes effect; "Refused" is when a write to it is refused with 409.

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

## Pages, not the API

`/` (the portal, or the set-up wizard while the unit is its own access point), `/wizard`, `/display_mirror` and `/mobile` stay where they are. They are pages, not the API, and are free to change in ADR-008.

## Gone, with nothing in their place

- `/rest/button_control`. No page calls it; the console keeps `press`. The API offers commands, not button presses.
- `profile_config`'s `pf` as a way to select: `PATCH /api/v1/profiles {"selected": n}`.
- `scale_action`, `system_control`, the two `*_state` paths, the `*_config` paths: each is in a table above.

## For the owner

1. **Live until saved**, as the portal's Apply and Save buttons do today, with `saved` in every reply. The other way: every write is stored at once, and nothing is lost at a restart. An agent then cannot try a value without keeping it, and every slider move is an EEPROM write.
2. **The limits marked *p*.** The code has none for them today; these are first figures.
3. **What is refused during a throw.** A throw is the states `zeroing` and `charging`. Between throws (waiting for the cup) and while idle, settings are written. So a profile's gains can be changed between two throws, as ADR-009's tuning will need.
