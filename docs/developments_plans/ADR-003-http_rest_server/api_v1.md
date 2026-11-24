# API v1: the table, for approval

ADR-003's API, written for gate 2's approval (2026-11-15). It was checked against every ADR, the menu and the TODOs in the code ("What it must grow into", below). It was then reviewed by a second model at the owner's word, and revised from that review ("The review", at the end). Once approved, this is the contract: v1 only grows. Once built, the unit's own description (`GET /api/v1/schema`) says what it carries, from the same tables as the firmware, and this file is kept to it: the scripted client checks the two against each other.

Each field below comes from today's stored structs, read in the code on 2026-11-15. Where today's code has no limit, the limit here is a **proposal** (marked *p*), for the owner to look at.

**Renamed in ADR-008's gate 2** (2026-11-19, before the first release for users): `cleanup` is `purge` (its paths, `activity`, its events, its state's kind, `no_purge_runs`), and `system/erase` is `system/factory_reset`, its confirmation `factory_reset`, its reply `resetting`. The screen and the pages use the same words (ADR-008, "The words"). The review's record below keeps the words of its time.

## Words

- **The charge** is the job, from `charge/start` to its stop: charge mode, a loop of throws. **A throw** is one cup, numbered from 1 at each start of the unit. A throw lasts from the start of `zeroing` until its verdict is given: for the controller at once, at its settled reading (ADR-009, gate F.5); for today's loop, which only the console pours now, a second after. While it lasts, `in_throw` is true.
- **A job** is long work: the charge and the purge now; tuning, an update, a motor's test run later. **The unit runs one job at a time.** `GET /api/v1/system` says which (`activity`).

## The rules

- **Paths** start with `/api/v1`. Names are `snake_case` and say what a thing is; no codes. A quantity with a physical unit carries it in its name (`_rps`, `_rps2`, `_ms`, `_ma`, `_mohm`, `_per_s`); a weight and a ratio carry none.
- **A read is a GET, and it changes nothing.** Not a selection, not an event, not a light.
- **A write is a PATCH** with a JSON object of the fields to change, one or many. **A PATCH's fields are the GET's fields:** same name, same type. The whole request is checked first; if any field is refused, nothing in the request is written. The reply is the whole resource as it now is.
- **An action is a POST** to a path named for it (`/charge/start`), with a JSON body (`{}` when it takes nothing). The reply is the state it led to: 202 when the action goes on after the reply (a charge, a restart), 200 when it is done. **An action that is refused changes nothing**, fields it carries included.
- **A stop never fails.** Every job's stop, and `POST /api/v1/stop`, answer 202 when they stopped something and 200 with the state when there was nothing to stop. A client may repeat one after a lost reply. The charge and the purge take a stop; a view is a person's at the unit, so a stop then answers 200 `"stopped": false` with the `activity` (review C1, 2026-11-16). Since ADR-010 there is no view: a job takes the screen from any page.
- **Only the unit's own name is served.** An API request whose `Host` is not the unit's address or its host name (with or without `.local`) is refused with 403. A page asked for under another name is redirected to the unit's address, which the captive portal needs too. This stops a page that has pointed its own name at the unit (DNS rebinding).
- **A write or an action must be sent as `application/json`**, or as `application/octet-stream` where a path takes bytes (ADR-004's image). Anything else is refused with 415, a POST with no body and no `Content-Type` included. The unit sends no CORS headers and answers `OPTIONS` with 405. A browser asks a server's leave before it sends either type from another site, and the unit never gives it. With the `Host` rule, a page from another site cannot make a browser write to the unit.
- **Settings are live when written and stored when saved.** A PATCH changes the setting at once (or at the next start, where the schema says so). A save stores it. Every settings resource carries `"saved": true|false`. `saved` is worked out, not remembered: the struct in RAM against what was last stored, by their CRCs. So a change made at the menu or at the console counts too.
- **A powder's record is the exception** (`/api/v1/settings/powder_records/{id}`, ADR-009, gate R): its write takes effect at its save, not at once, since a charge reads only the stored record. One powder's write is held at a time: a write of another is refused 409, naming the one held and its save and revert; a job's own write of that powder drops it (event `setting` `dropped`); a restart loses it. Its saves, as its writes, are refused while a job runs.
- **Numbers** are JSON numbers. **A number a GET gives, PATCHed back, changes nothing:** a setting's float is written as the shortest decimal that reads back to the same float, nine significant digits at most (30.02, not 30.0200005). A recording's samples are written with six (below). A value that is not there, such as a weight with no scale reading, is `null`.
- **Choices** are strings or numbers from a list in the schema, each with a label: `"driver": "and_fxi"` ("A&D FX-i"), `"baud": 9600`. No indexes.
- **Weights** are in the scale's own unit: `GET /api/v1/scale` says which, or `null` when the scale cannot tell. A client does not assume.
- **Colours** are `"#rrggbb"`.
- **Lists** (added by ADR-009's gate R, as v1's growth rules allow): a field may be a list of rows, each row a flat object of the schema's `row` fields (`"fine_stops": [{"speed_rps": 0.05, ...}, ...]`). One level only: a row's values are as any field's, nothing nested in them. **A write replaces a whole list:** each row is given every one of its fields, the count is within `rows_min` and `rows_max`, and if any of it is refused the whole PATCH is refused, as for any field. There is no write of one row. A GET of such a resource is sent in chunks; a PATCH's body is still 1 KB at most, so a client writes a large resource in several PATCHes, each list whole in one (`tools/remote_control/unit_api.py`'s `bodies`).
- **A write the unit has no memory for** is answered 503 with `Retry-After`, and nothing is written (ADR-009, gate R: a write's copies of its resource are taken from the heap).
- **Ids:** a powder profile's is an opaque number (0 to 7 today, but a client reads the list and does not count on that); a motor's is its name, `coarse` or `fine`. Each is listed at its collection's path: `GET /api/v1/powder_profiles`, `GET /api/v1/settings/motors`.
- **What a request reads or writes is named in its path; what an action works on is a field of its body.** `PATCH /api/v1/settings/motors/fine` writes the fine motor's settings. `POST /api/v1/motor_run/start` takes `{"motor": "fine", ...}`: the run is the job, and the motor its parameter (ADR-009). A GET carries no body. An id is the same in a path, a body, a reply and an event (owner's review of PR #129, 2026-11-16).
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
- **Bytes are sent with PUT** (ADR-004's image), as `application/octet-stream`, and handed to their route in pieces as they come (gate 4): the server holds none of it whole, and the route says how much it takes (413 `too_large`).
- **A resource may gain a representation,** asked for with `?format=` (a link can carry it, a header cannot); JSON stays the default.
- **What is under `/api/v1/debug` is not part of v1.** It may change or go in any build.
- **A password,** if its own ADR adds one, comes as a header (`Authorization`). Paths and bodies stay as they are; a client adds the header.
- **A change that breaks any of this** is `/api/v2`, served beside v1 for a time.

### Status and errors

| Status | When |
|---|---|
| 200 | A read, a write, an action done; a stop with nothing to stop |
| 202 | An action under way: a charge started or stopped, a restart |
| 304 | The schema or a page, unchanged since the `ETag` the client gave |
| 400 | Not JSON, an unknown field, a wrong type, a value outside its limits, a missing confirmation, a start with no target |
| 403 | A `Host` that is not the unit's |
| 404 | No such path on this build, or no such id |
| 405 | The path exists, the method does not; `Allow` lists the methods. A GET of an action's path is not refused: it gives the action's usage (below) |
| 409 | Refused at this moment: another job runs, a throw is under way, no job runs that the command needs, a driver is at fault, the gate is disabled |
| 411 | A body with no `Content-Length` (a chunked one included) |
| 413 | A request larger than the unit takes: a JSON body over 1 KB; headers over 8 KB in all; one of the headers the unit reads, longer than its line. Other headers, a long `Cookie:` line among them, are passed over unread |
| 414 | A request line over 256 bytes |
| 415 | A write not sent as `application/json` (or `application/octet-stream` where bytes are taken) |
| 500 | The unit failed to do it, such as a save the EEPROM did not take |
| 501 | The path is known, but this make of scale cannot do it: the Steinberg, Creedmoor, Sartorius, US Solid, JM Science and generic drivers cannot zero their scale (TODOs in their code); `actions` in `GET /api/v1/scale` says which can |

**The API describes itself** (ADR-008, gate 2):
- **A GET of an action** (a path that takes only a POST, a PUT or a PATCH) gives 200 `{"usage": <the action as the schema gives it, with an "example" body>, "see": "/api/v1/schema"}`, its `Allow` still saying what it takes. A GET still changes nothing.
- **Every error under `/api/v1` says where to read,** in `see`: a path not there, `/api/v1`; a method a path does not take, the path itself (its GET); any other, `/api/v1/schema`.
- **The root points to the index:** each page's head carries `<link rel="service-doc" href="/api/v1">` and `<link rel="service-desc" href="/api/v1/schema">` (RFC 8631).
- **Every route is listed** in the index or the schema, or named as a page (`/`, `/mobile`, `/wizard`, `/display_mirror`) or a debug route: `rc_run.py api_check` reads the server's routes (`GET /api/v1/debug/routes`, not part of v1) and fails on one in none.
- **The hello for agents** (`POST /api/v1/agent/hello`) is listed, an action of the kind `agent`, `refused_while` `a_charge_runs`.

Every error has the same body (with `see` under `/api/v1`):

```json
{"error": {"code": "out_of_range", "field": "fine_stop_threshold", "message": "12 is above the most, 10", "min": 0, "max": 10}}
```

The codes are `bad_json`, `length_required`, `unknown_field`, `wrong_type`, `out_of_range`, `not_a_choice`, `read_only`, `confirm_missing`, `not_found`, `method_not_allowed`, `busy` (with `activity` and `state`), `not_running`, `no_target`, `weight_off_zero` (with `weight`), `driver_fault` (with `motor` and `fault`), `gate_disabled`, `not_supported`, `wrong_host`, `too_large`, `wrong_media_type` and `failed`; for an update's image (ADR-004) `not_an_image`, `wrong_chip`, `wrong_board`, `not_an_update`, `not_newer`, `wrong_slot`, `hash_mismatch` and `not_ready`; for the update's poll (ADR-010) `no_network`; for the set-up's join (ADR-008) `not_on_hotspot`. `field`, `min`, `max`, `choices`, `activity`, `state`, `motor` and `fault` are given when they apply. More codes may come; the schema lists them.

## What the unit is

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1` | | `api` ("1"); `firmware` {`version`, `vcs`, `build`}; `board` (pico_w, pico2_w; esp32_s3_pico with ADR-007); `unit_id` (eight hex digits made from the Pico's own unique id: the same across a factory reset, never stored; ADR-008, gate 4); `hostname` (`opentrickler-` and the id's first four); `boot`, a 32-bit random number new at each start (the SDK's `pico_rand`; 32 bits, since a browser's JSON keeps only 53); `uptime_s`; `features`, each part a build may leave out, `{name, present, about}`, present or not (ADR-008, gate 2; ADR-005's feature table): today `servo_gate`, which `-DFEATURE_SERVO_GATE=OFF` leaves out with its paths, its settings, its schema entries and its menu page; a client shows only what is present; `guide`, a page of plain words for a client new to the unit (the job cycle, what stop does, what the unit cannot know); `resources`: each path with its methods and a line on what it is |
| `GET /api/v1/schema` | | Every resource's fields **and every action's body**: `name`, `label`, `group`, `type` (number, integer, boolean, string, colour, choice), `unit`, `min`, `max`, `step`, `choices` (each `{value, label}`), `access` (read, read_write, write_only), `applies` (now, at_restart, or as the field says), `refused_while` (in_throw, a_job_runs, or never), and a line on what it is. Each settings resource names its save path. Also every state and what it means, every event kind and what it carries, and every error code. ADR-008's forms and ADR-010's settings screen are drawn from it. It changes only with the firmware |
| `GET /api/v1/system` | | `firmware`, `board`, `unit_id`, `uptime_s`; `activity` (none, charge, purge, view, update; more later); `restart_pending` (a setting that applies at restart was changed). `view` is never reported since ADR-010 (a job takes the screen from any page); it stays a name of v1. It was: a person has a screen open at the unit that is not a job, and a start is refused until it is closed there. A job in `fault` is still that job's `activity`: the mode holds the screen and the motors until it is acknowledged. A view is no job: it refuses a start, and nothing else (`refused_while: a_job_runs` is the charge, a cleanup and an update). The scale's calibration was removed in ADR-010 (the owner: no scale supports it) `set_up` (ADR-008, gate 4): the set-up finished or skipped, or a network named; false after a factory reset, when the portal opens the set-up by itself on the unit's own hotspot |
| `GET /api/v1/network` | | `mode` (station, access_point), `ssid`, `address`, `hostname` (the mDNS name), `rssi`: the menu's Wi-Fi info, and how a person or an agent finds which unit is which. `station` (ADR-008, gate 4): the station's last join, whichever asked it (the start's, the minute's retry, the set-up's): `state` (idle, joining, joined, failed), `ssid`, `address` on that network, `reason` when failed (wrong_password, timeout, failed), `joining_ms` while joining (since it began: the page and the screen count the same seconds). On its own hotspot with the station joined beside it, `mode` is access_point and `station` says where else it is |
| `GET /api/v1/setup` | | `set_up`; `step` (code, wifi, scale, motors, ready, done); `network` (chosen on the Wi-Fi step, or null); `phone` (a phone has an address from its hotspot); `hotspot` (its name while it is up, else null). Kept in RAM: a restart begins it again (ADR-008, gate 4) |
| `GET /api/v1/wifi/networks` | | `scanning`, and `networks`: each `ssid`, `rssi` (dBm), `auth` (open, wpa_tkip, wpa2_aes, wpa2_mixed: the radio tells WPA2 from WPA, not AES from TKIP), merged by name, strongest first. Hidden networks and WEP alone are left out (ADR-008, gate 4) |
| `POST /api/v1/stop` | `{}` | Stops whatever job runs, or a start asked and not yet begun. A stop: never refused (the rule above). `{"stopped": true\|false, "activity"}`: 202 when it stopped one, 200 when there was nothing |
| `POST /api/v1/wifi/scan` | `{}` | 202 with `/api/v1/wifi/networks`'s reply: two scans, merged, `scanning` until they end (some 1.5 s). 409 `busy` while a scan or a join runs |
| `POST /api/v1/wifi/join` | `{"ssid": "Smith home", "password": "...", "auth": "wpa2_mixed"}` | The set-up's join (ADR-008, gate 4): 202 with `/api/v1/network`'s reply, its `station` joining. The unit stays its own hotspot meanwhile, so the page that asked reads the end there: joined (the network stored, by `api`, and in effect: no restart pending) or failed with its `reason` (nothing stored; told within seconds for a wrong key). `auth` wpa2_mixed when left out; `password` 8 to 63 bytes with security. The join takes the network's access point on the hotspot's channel when the scan heard one, so the hotspot stays where the phone is (the hotspot starts on the strongest access point's channel); events `wifi` `joining`, then `joined` or `not_joined` with its reason. 409 `not_on_hotspot` when the unit is on its network, which a join would leave; 409 `busy` while a scan or a join runs; 400 `out_of_range`, `not_a_choice`, `wrong_type` |
| `PATCH /api/v1/setup` | `{"step": "wifi", "network": "Smith home"}` | 200 with `GET /api/v1/setup`'s reply: the set-up's step as the page goes on (wifi, scale, motors, ready), with the network chosen on the Wi-Fi step or null. The unit keeps it (ADR-008, gate 4: two screens, one process): its screen shows the same stage, on its own hotspot, and a page that lost the hotspot reads where the set-up is. An event `setup` `step`. 400 `not_a_choice` for code or done (done is the POST below) |
| `POST /api/v1/setup/done` | `{}` | 200 `{"set_up": true, "hotspot": ...}`: the set-up finished or skipped (ADR-008, gate 4), remembered, so the portal never opens it by itself again; a factory reset forgets it. Joined, the hotspot stops a moment after the reply (`hotspot` false) and the unit is on its network alone; not joined, the hotspot stays (`hotspot` true). `/wizard`, the old wizard's address, redirects to the portal's set-up (`/#setup`) |
| `POST /api/v1/system/restart` | `{}` | 202 `{"restarting": true}`; the unit restarts once the reply is out. 409 `busy` while a job runs: stop it first |
| `POST /api/v1/system/factory_reset` | `{"confirm": "factory_reset"}` | 202 `{"resetting": true}`. The stored settings are erased, then the unit restarts. **It then leaves the network and comes back as its own access point**, with the same id and host name: the id is read from the Pico (ADR-008, gate 4). 400 `confirm_missing` without the confirmation; 409 `busy` while a job runs |
| `GET /api/v1/debug` | | Not part of v1: today `heap_free` (FreeRTOS) and `net` (lwIP's heap and pools, as the console's `net`) |

## The event log

One log for the whole unit, which any number of clients read, each from where it left off. It replaces the charge state's events, which were cleared by whoever read them first.

| Method and path | Reply |
|---|---|
| `GET /api/v1/events?after=N` | `boot` (as in the index: when it changes, the unit restarted and the numbers began again); `now_ms`, the unit's clock now; `events`, each with `seq`, `at_ms` (the unit's 64-bit clock, in ms since the start), `source` and `kind`, and what that kind carries; `next`, the number to ask after next time. The newest 64 are kept *p*. `seq` goes up by one, so a client sees a gap: one that asks after a number no longer kept gets all that are, and `first`, the oldest kept. With `wait`, a read after a number above the newest (one from before a restart) is answered at once, with the new `boot` and `next` |

**The same log as it happens** (ADR-008, gate 2): `GET /api/v1/events/stream`, server-sent events (`text/event-stream`), as a browser's `EventSource` reads them, or `curl -N`:

| Message | When | `data` |
|---|---|---|
| `retry: 3000` | first | how soon a browser opens it again once it drops |
| `event: unit` | first, and again when events were missed | `boot`, `first`, `newest`, `resumed`: true when the stream goes on from the client's `Last-Event-ID` (`<boot>-<seq>`, which a browser sends by itself on its reconnect) or `?after=N`, nothing missed; false when it starts afresh (no id, another `boot`, or the event no longer kept), and the client then reads what it shows again |
| `event: state` | after the unit, then whenever it changes, no oftener than every 100 ms | `activity`, `charge` (as `GET /api/v1/charge`, its live weight included) and `purge` (as `GET /api/v1/purge`): the weight comes at the scale's own rate. Not an event: not numbered, not kept |
| `id: <boot>-<seq>` with no `event:` | each event as it is logged | the event, as `GET /api/v1/events` gives it |
| `event: state` again | after 15 s with nothing sent | the state, as above: the connection is alive. Not a comment: a browser's `EventSource` never shows its page one (found in ADR-008's gate 3) |

- **At most 3 at once,** so a request always finds one of the server's 6 slots: a fourth is answered 503 `busy` with `Retry-After: 5`. A browser's `EventSource` does not try again after an error status: a page does, after `Retry-After`.
- **A client that stops reading** is closed by the server's stall limit (10 s with its bytes not taken), so a dead connection never holds lwIP's heap for long.
- **A unit that restarts or leaves its network sends no close:** the client hears nothing. It knows by the comment that should come within 15 s: nothing for some 20 s, it opens the stream again (a page does this itself; an `EventSource` alone may wait much longer). Of the same `boot`, it goes on from its last id; after a restart, `resumed` is false.
- Held reads stay, for agents: they need no connection kept.

An event is a change, not a state: a fault that stands is one event when it appears and one when it clears, not one a second. The kinds now:

| Source | Kind | Carries |
|---|---|---|
| charge | started | `by` (button, api, console), `target`, `powder_profile` |
| charge | stopped | `by` (button, api, console, or the unit itself), `reason` (stopped, fault). A charge that ends on a fault is logged stopped at the fault, `by` the unit; its acknowledgment is no event |
| throw | started | `throw`, `target`, `powder_profile` |
| throw | paused | `throw`, `pause`: its number in the throw (ADR-013: no reading for 3 of the scale's intervals) |
| throw | resumed | `throw`, `weight`, the settled weight it went on from |
| throw | finished | `throw`, `result` (good, under, over), `weight`, `elapsed_s` |
| throw | ended | `throw`, `reason` (stopped, scale_no_reading, scale_unsteady, scale_unreliable, driver_fault; the staged controller's `cup_lifted`, a reading below the base by more than the scale's noise, `no_powder`, the powder turned out not shown: ADR-009, gate F.1; and `limit`, a bound of the throw reached (its time, the fine tube's turns, a reading far over), which is not a verdict: ADR-009), and for a driver's fault its `motor` and `fault` |
| purge | started | `by` |
| purge | stopped | `by`, `reason` (stopped, time, fault); at a fault, as the charge's |
| motors | fault, cleared | `motor`, `fault` |
| system | started | `firmware`, at each start |
| update | started | `by` |
| update | stopped | `by`, `reason` (stopped, refused, time), and for a refusal its `code` |
| update | received | `version`: the image is whole and checked, ready to install |
| update | installing | `version`: the unit restarts into a trial of it |
| update | confirmed | `version`: after the restart, the image confirmed itself once on its network |
| update | back | `version`: after the restart, the image tried was left, and the one before runs |
| update | found | `version`: the poll found a newer release, now offered (ADR-010, gate 5) |
| setting | changed | `by` (button for the menu, api, console), `path`: a settings resource or a powder written, and what it holds changed; one for each resource written, none for a write that changed nothing (ADR-008, gate 2) |
| setting | stored | `by`, `path`: the save's path; what was written stored where it was not, so each resource that save stores is `saved` again (ADR-008, gate 2) |
| setting | dropped | `by` (unit), `path`: a write not yet saved, dropped because the unit wrote that resource itself (a powder's record, at a charge's end: ADR-009, gate R) |

Later ADRs add sources and kinds (the rules above). **`?wait=s`** (1 to 30): the request is held until there is an event after N, or the time is out, and then answered whole, as any other. Built in gate 4 with the owner's leave ("For the owner", 7); without it, reserved, and the server built to take it.

## Charge

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/charge` | | `state` (idle, starting, zeroing, charging, waiting_for_cup_removal, waiting_for_cup_return, fault); `in_throw`; `throw`, the number of this throw or the last; `paused` (true while a throw waits for readings); `target` (null until set); `powder_profile` (its id) and `powder_profile_name`; `weight` (null with no reading); `elapsed_s` (this throw, or the last); `last` {`throw`, `result` (good, under, over, none), `reason` (finished, or as a throw's `ended`; or `refused`, a charge that refused itself as it began: its powder's record found damaged since the start was taken, or no memory for its controller: ADR-009), `weight`}; `event_seq`, the newest event's number |
| `PATCH /api/v1/charge` | `{"target": 30.0}`, `{"powder_profile": 3}`, or both | 200. What the next throw uses. One target for the unit: the menu's weight digits show it and set it. `target` from 0.001 to 999.99 at two decimal places, 99.999 at three: what the menu can show. The powder profile chosen is live until saved, with the profiles (`powder_profiles.saved`). 409 `busy` while `in_throw` |
| `POST /api/v1/charge/start` | `{}`, or `target` and `powder_profile` to set them first | 202 with the charge, in `starting` until the menu begins it. A target or a powder profile given is written only if the start is taken. 409 `busy` while another job runs or the menu is not at its forms (a person is in a mode at the unit); 409 `driver_fault` (the drivers looked at afresh first, as a start at the unit does: a fault that has passed does not refuse); 400 `no_target` when no target is set; 409 `not_calibrated` (the staged controller and the powder's record damaged: ADR-009, gate R; a powder with no record is thrown by the learnt source since gate F.4); 503 `busy` with its `field` `powder_profile` when the unit has no memory to read the powder's record and check the start (try again; ADR-009); 409 `settings_unusable` with its `field` (a fine stop threshold under the scale's digit, or a top speed of 0 for a tube the throw needs: ADR-009, gate F.2) |
| `POST /api/v1/charge/stop` | `{}` | A stop (the rule above). It also acknowledges `fault`, as a press at the unit does |

The charge in `fault` has ended on one: it waits, as the screen does, to be acknowledged by a stop or a press, and `last.reason` says why. It is not a throw: settings are written. A fault between throws ends no throw: `last.reason` says why the charge ended, and `last.result` stays the last throw's verdict.

ADR-014 adds a charge profile: `charge_profile` beside `target` and `powder_profile`, in the GET, the PATCH and start. A target or a powder profile given beside it overrides it for that charge.

## Purge

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/purge` | | `state` (idle, starting, running, fault), `by` (button, api, console; null with none), `speed_rps`, `time_left_s` (null when the speed was given with no time) |
| `POST /api/v1/purge/start` | `{}` | 202. The purge runs, at speed 0 until a PATCH gives one. 409 `busy` while another job runs; 409 `driver_fault` (looked at afresh, as the charge's) |
| `POST /api/v1/purge/stop` | `{}` | A stop; it acknowledges `fault` |
| `PATCH /api/v1/purge` | `{"speed_rps": 2.0}`, or `{"speed_rps": 2.0, "time_s": 30}` | 200. Both tricklers turn at that speed, within ± the lower of the two motors' `max_speed_rps` at that moment. With `time_s` (1 to 600 *p*), for at most that long: a new PATCH before then sets a new speed and a new time, and when the time runs out with none, the purge stops by itself (`purge stopped`, `by` the unit, `reason` time). A client with no person at the unit gives a short time and sends the speed again while it wants it, so a client that loses the network leaves no motor turning for longer than its last time. Without `time_s` it turns until stopped, as at the unit. 409 `not_running` when idle. A speed beyond the motors' limit is turned at the limit, not refused: the limit follows the motors' settings, so the schema cannot give it, and the reply's `speed_rps` is the speed taken. In `fault` no speed is taken (409 `not_running`): the motors stay stopped until the purge is acknowledged (review D1, C6) |

## The scale, the gate, the motors, the screen

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/scale` | | `weight` (null with no reading); `unit` (gn, g, or null when the make cannot tell: null for all but the A&D, whose frame carries it); `driver`, the one that runs; `simulated` (the console's stand-in is running in its place); `usual_interval_ms` (the scale's rhythm, ADR-013); `bad_frames`; `actions`, what this make can be asked to do (zero now; its display, its fast report later) |
| `POST /api/v1/scale/zero` | `{}` | 200: the scale was asked to zero, not that it did (some scales do not zero while unsteady); a client reads the weight after. When it is taken is in the table below. 501 `not_supported` when the make cannot |
| `GET /api/v1/gate` | | `state` (open, closed, partial, held, disabled), `ratio` (0 open to 1 closed; null when disabled). `held`: the menu's "Disable", which moves the servo no more and leaves it at its last duty, `ratio` the last one (`servo_gate.c`); charge mode moves it again when it needs it. `disabled`: off in its settings (`settings/gate.enable`), so charge mode does not use it |
| `PATCH /api/v1/gate` | `{"ratio": 0.5}` | 200. 409 `gate_disabled` when it is off in its settings; 409 `busy` while `in_throw`. Holding it as the menu's "Disable" does is not offered in v1: no page sends it, and a held gate reads back its last ratio |
| `GET /api/v1/motors` | | For `coarse` and `fine`: `speed_rps` (now, ramps included), `driver` (ok, absent, short, hot, reset, supply) |
| `GET /api/v1/display` | | `width`, `height`, `format` (`mono_rows`: 64 rows of 16 bytes, the leftmost pixel the top bit, 1 a lit pixel as the panel shows it, its inversion applied, laid out as the body of a PBM image; ADR-010, gate 4. The mini 12864's glass shows a lit pixel light on a dark ground). The frame's format is not kept across versions: a client reads `format` first (the owner, 2026-11-17). When a larger screen comes, `format` says so |
| `GET /api/v1/display/frame` | | The screen's bytes, as `application/octet-stream`, in the `format` above |

**A zero over the API.** The unit cannot tell what is on the pan, and a zero can start a throw. Two things must not be zeroed away: a cup that is off the pan (the throw then pours onto the pan) and powder still in the cup (the throw then pours a second charge on top, and the scale reads it as good). So over the API a zero in `zeroing` puts right only a drift. The encoder's press at the unit is not changed: the person there sees the pan.

| State | A zero over the API |
|---|---|
| No job, a purge, `fault` | Taken (the page's Scale Zero is on the purge page too, for the container) |
| `starting`, `zeroing` | Taken only while the weight is within 5 times `zero_mean_margin` of zero *p* (0.1 at the default 0.02): a cup that reads 0.05 is put right. Otherwise 409 `weight_off_zero`, with `weight`: "the scale reads 30.02; over the API a zero puts right only a drift; empty the cup or put it on, or zero at the unit". A remote start passes no "Put pan on the scale" form: with the cup off the scale reads about minus the cup, and with a charge left in it about plus the charge |
| `charging`, `waiting_for_cup_removal`, `waiting_for_cup_return` | 409 `busy`. While it waits for the cup's return the unit takes the cup to be away, and a zero there starts the next throw at once. |

The band is two-sided and in the scale's own unit, through the setting the user tunes for their scale: a fixed figure would be fifteen times wider in grams than in grains, wide enough at ±1 to zero away a pistol charge. It is a live condition on a command, not a limit in the schema. It is not the unit's test for a lifted cup either: that test is a fixed 10 in the scale's unit (`charge_mode.cpp`, the cup's removal), and misses a cup under 10 g. What no rule can close, and the guide says: a zero while idle and then a start, or a start on a scale zeroed with nothing on it, pours onto whatever is on the pan. Start, and zero, only when a person has said an empty cup is on the pan.

## Firmware updates (ADR-004)

Offered at the screen since ADR-010's gate 5, from the unit's own poll; the web page's offer comes with ADR-008. An update is a job: one at a time, with the job's shape, and while it runs no other job starts. It holds the screen at the unit, as a mode does, so nobody starts a charge there during it, and the reset button stops it. The image is written into the slot the unit does not run from; the slot it runs from is never written.

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/update` | | `state` (idle, starting, receiving, ready, installing, ending: the job has ended and the screen is being given back, a start taken again once it is idle); `running`: `version`, `block` (the version its metadata block carries, which an update must exceed, written as the version is), `slot` (a, b), `trial` (true while the image runs on trial, until it confirms itself); `takes`: the release manifest's name for the image this unit takes (`pico2_w`, `pico_w_a`, `pico_w_b`); `free_bytes`, the largest image; `received`, the bytes of the image taken so far (0 until a job takes some), and `expected`, its length once the upload or the download says it (0 before); `download`, true while the job fetches the image itself; `poll`: `state` (idle, checking, up_to_date, found, failed) and `error` (why it failed, or null); `found`: the release the poll found and offers, `version`, `block` (its image's), `note`, `importance` (normal, critical), `channel` and `bench` (the bench host's address it was found on, or null from the stored source: ADR-010, gate 6, step 9), or null; `last`: how the last update ended, `result` (confirmed, back, refused, stopped, time), `version`, and for a refusal its `code` and `message`, or null |
| `POST /api/v1/update/start` | `{}`, or `{"download": true}` | 202, `starting` until the menu begins it, then `receiving`. With `download` (ADR-010, gate 5) the job fetches the image of the release `found` names itself, checks it as an upload is and against its manifest's SHA-256, and installs it with no further word, as the offer's Install at the screen does; 409 `not_found` when nothing is found. 409 `busy` while another job runs, a poll runs, a person has a screen open at the unit, or the image running is on trial (until it is confirmed, the free slot holds the image the unit goes back to) |
| `POST /api/v1/update/check` | `{}` | 202: the unit polls its source now (`settings/update`), and `poll` and `found` say what came of it; a release found is also an event. It polls once each start by itself, when it joins its network as a station. 409 `busy` while a poll or the update runs; 409 `no_network` unless it is a station on its network (its own access point has no internet) |
| `PUT /api/v1/update/image` | the image (`app.bin`), `application/octet-stream` | Written into the free slot as it comes; its first block checked in its first 4 KB, the whole once it is in. 200 with the state `ready`. 409 `not_running` unless `receiving`; 409 `busy` while another upload of the image is open (one at a time); 413 `too_large` beyond the free slot; 400, nothing installed and the job ended, with `not_an_image`, `wrong_chip`, `wrong_board`, `not_an_update` (no try-before-you-buy mark), `not_newer`, `wrong_slot` (linked for the other slot, on the RP2040; told by the image's first 4 KB) or `hash_mismatch`. A body cut off leaves the job `receiving`, for the image again from its start |
| `POST /api/v1/update/install` | `{}` | 202: the unit restarts into a trial of the image. It comes back on it, confirmed once it is on its network, or on the image before; `last` and the event log say which. 409 `not_ready` unless `ready` |
| `POST /api/v1/update/stop` | `{}` | A stop: 202, or 200 with nothing to stop. The image written stays in the free slot, never booted. 409 `busy` while `installing`: the restart is under way |

- **Time limits** *p*: a job `receiving` that gets no bytes for 30 s, or `ready` and not installed within 60 s, ends by itself (`time`).
- **The update's life cycle made whole** (ADR-008, gate 2):
  - `checking` is a state between `receiving` and `ready`: all of the image in, its SHA-256, its board and its seal checked.
  - While `installing`, `GET /api/v1/update` (and the install's reply) gives `back_within_s`: the longest the restart's silence may last, the trial's longest and some 20 s, before a client says the unit is not back.
  - `last` gains `reason`, why a trial went back (`did_not_start`, `no_network`: not on its network within the Wi-Fi join's time and a margin, or its Wi-Fi settings did not read; `not_confirmed`; `power_cut`), and `reset`, the settings a kept update reset to their defaults, by name (`leds`, `charge`, ...): a stored layout that changed. Both are kept in the EEPROM with the trial's note, so a later restart does not forget how the last update ended. The events `back` (with `reason`) and `confirmed` (with `reset`) say the same.
  - **On trial, nothing is written to the EEPROM** but the trial's own note: each setting's write is held and written when the image confirms itself. Gone back, the image before reads its settings as it left them.
- **The image** is the release's update image for the unit's board and free slot (ADR-004): the release's manifest names each, and `takes` says which one this unit wants. A newer version only: going back is a full erase and a USB install.
- **The poll** (ADR-010, gate 5) reads the source's releases as GitHub lays them out. Stable reads `releases/latest/download/manifest.json`, which GitHub never resolves to a pre-release; none there is no stable release yet, `up_to_date`. Beta reads the newest release's tag from GitHub's list of releases (`api.github.com/repos/<owner>/<repo>/releases?per_page=1` for github.com, `/api/v3/repos/...` on any other host, as GitHub Enterprise has it), then that release's manifest. A release no newer than the image running is `up_to_date`. Over HTTPS the unit trusts GitHub's two roots only; it has no clock, so a certificate's dates are not checked.

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
- **A revert beside each save** (ADR-008, gate 2): `POST` the save's path with `/revert` in place of `/save` (`/api/v1/settings/charge/revert`, `/api/v1/powder_profiles/revert`, `/api/v1/settings/motors/revert`), `{}`: every resource stored there given back what the EEPROM holds, undoing what was written since. Each part is written as a write is: refused whole with 409 `busy` as a write to it would be (the motors' while a job runs, the powder in use during a throw), and a `setting changed` event for each resource that changed. 200 `{"reverted": true}`. The schema gives it as each resource's `revert`, and as an action.
- **A test stand-in is never stored.** The console's simulated scale runs in place of the stored driver and never shows in `settings/scale`, and no save stores it.
- **The shapes, as built in gate 3:**
  - `GET /api/v1/settings` is `{"api": "1", "firmware": {"version", "vcs", "build"}, "unit_id", "resources": {...}}`, each resource under its path (`"/api/v1/settings/charge"`, `"/api/v1/powder_profiles/0"`) as its own GET gives it. An import PATCHes each path with its fields, `saved` left out.
  - A save, and `POST /api/v1/settings/save`, answer 200 `{"saved": true}`, or 500 `failed`, or 409 `busy` while a job runs for a resource refused while one does (`refused_while` `a_job_runs` in its usage: the powder records, with a write held; ADR-009, gate R), and while the settings are being written; a save is taken under the activity lock, from every side; their body is `{}`, and a field in it is refused (400 `unknown_field`). `saved` sent in a PATCH is 400 `read_only`.
  - `GET /api/v1/schema` is `{"api", "firmware", "resources": [...], "actions": [...], "errors": [...]}`. A resource: `path`, `methods`, `save`, `label`, `about`, `ids` (for `{id}`), `refused_while`, `fields`. A field: `name`, `label`, `group`, `type`, `unit`, `min`, `max`, `step`, `min_length` and `max_length` (a string's bytes), `choices` (`{value, label}`, and `short_label` where the label is long), `nullable`, `access`, `applies`, `refused_while`, `about`, each given where it applies: `min`, `max` and `step` for numbers only. For a client with little room (ADR-010, gate 6: the unit's screen; ADR-008's pages may use them): `short_label` (where the label and a value do not fit 128 px), `edit_step` (what one detent, or an arrow, moves a number by), and a boolean's `on_label` and `off_label` ("Reversed", "Normal"). `refused_while` is `never`, `in_throw`, `a_job_runs`, or for the powder profiles `in_throw_when_in_use`.
  - **Gate 4's share of the schema:** an action is `method`, `path`, `fields` (its body's, in a field's shape, with `required` where one must be given; empty for `{}`), `refused_while` (as above, and `no_purge_runs` for the purge's speed; absent for the zero, whose rule is in its `about`) and `about`; the saves are actions too. `states` gives each kind (`activity`, `charge`, `purge`, `gate`, `driver`) as a list of `{value, about}`. `events` gives each kind as `{source, kind, carries, about}`: `carries` names what it may carry beside `seq`, `at_ms`, `source` and `kind`. A client works from these (`tools/remote_control/unit_api.py`).
  - `GET /api/v1/powder_profiles/{id}` gives `saved`, this powder's own (ADR-008, gate 2: the eight are one block in the EEPROM, and a powder is saved when it is as the EEPROM holds it, whatever the others are) and its fields; `GET /api/v1/powder_profiles` gives `saved`, the block's, and each profile's `id` and `name`.
  - `GET /api/v1/settings/motors/coarse` and `/fine` give `saved`, each motor's own as a powder's, and their fields; `GET /api/v1/settings/motors` gives `saved` and each motor's `id` and `label`.

### `/api/v1/settings/charge`, saved at `/api/v1/settings/charge/save` (refused while `in_throw`)

| Field | Type | Unit | Limits | When | Today |
|---|---|---|---|---|---|
| `coarse_stop_threshold` | number | weight | 0 to 100 *p* | now | `c5` |
| `fine_stop_threshold` | number | weight | 0 to 10 *p* | now | `c6` |
| `steady_sd_margin` | number | weight | 0 to 1 *p*. The steady test, in three places: the zero, the cup's removal, the resume after a pause | now | `c7` (`set_point_sd_margin`) |
| `zero_mean_margin` | number | weight | 0 to 1 *p* | now | `c8` (`set_point_mean_margin`) |
| `decimal_places` | choice | | 2, 3 | now. A change keeps the charge's target as a weight, and is refused (400 `out_of_range`) when the target cannot be shown at the new decimals (review D2) | `c9` (an index) |
| `coarse_stop_gate_ratio` | number or null | | 0 to 1; null: the gate is not moved | now | `c13` (-1 for not moved) |
| `coarse_stop_backoff_enable` | boolean | | | now | `c14` |
| `coarse_stop_backoff_turns` | number | turns | 0.125 to 2 *p*, step 0.125 | now | `c15` |
| `coarse_stop_backoff_speed_rps` | number | rev/s | 0 to 100; 0 is the coarse motor's minimum speed; used up to its `max_speed_rps` | now | `c16` |
| `precharge_enable` | boolean | | | now | `c10` |
| `precharge_time_ms` | integer | ms | 0 to 5,000 *p* (RST is not read while it runs) | now | `c11` |
| `precharge_speed_rps` | number | rev/s | 0 to 100; used up to the coarse motor's `max_speed_rps` | now | `c12` |
| `colour_good`, `colour_under`, `colour_over`, `colour_not_ready` | colour | | | now | `c1` to `c4` (`neopixel_..._colour`) |

### `/api/v1/settings/motors/coarse` and `/api/v1/settings/motors/fine`, both saved at `/api/v1/settings/motors/save` (refused while a job runs)

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/settings/motors` | | `saved`, and `motors`: for each its `id` (`coarse`, `fine`) and `label`. A board lists the motors it carries |
| `GET /api/v1/settings/motors/{motor}` | | Its fields, below; 404 for another name |
| `PATCH /api/v1/settings/motors/{motor}` | Fields, below | 200; 409 `busy` while a job runs |
| `POST /api/v1/settings/motors/save` | `{}` | 200: both stored |

The two are stored as one block, so they are saved together, and each one's `saved` is the block's. Between throws the motors are still enabled, and some of these set the driver up again, so they are refused while a charge or a purge runs at all, as the console's `tmc ... setup` is. Today's checks (`motor_persistent_config_is_valid`) stay.

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

### `/api/v1/settings/update`, saved at `/api/v1/settings/update/save` (ADR-010, gate 5)

| Field | Type | Limits | When | Today |
|---|---|---|---|---|
| `channel` | choice | stable, beta (pre-releases too) | now: what was found is forgotten | new |
| `source` | string | 8 to 127 bytes; an `http://` or `https://` address with a host | now: what was found is forgotten | new: a repository's address, its releases laid out under it as GitHub's are; `https://github.com/eamars/OpenTrickler-RP2040-Controller` by default |

Its own slot in the EEPROM (11 KB): no other module's stored layout changed.

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
| Every throw recorded, downloaded, drawn | ADR-009, ADR-008 | Recordings, below |
| Test functions: a recorded test throw, a motor at a speed for a time, the scale's readings and rate | ADR-009 | Test throws, motor runs and scale recordings, below |
| Automatic tuning, on the unit or by an agent off it | ADR-009 | Tuning, below: both run test throws, so both give the same recordings |
| An agent developing the firmware with no probe: the log, the simulated scale and its faults, presses, the drivers' registers | ADR-001's console, the owner | `/api/v1/debug`, below |
| A press to skip the current charge (issue #75); ladder loading (issue #106) | ADR-009's list | Actions on the charge job, or a series of charge profiles; added, not changed |
| An update from the browser: upload, check, install; refused while a job runs | ADR-004 | Built in ADR-004's gate 2: "Firmware updates", above |
| What a board carries | ADR-005 | `features` in the index; absent parts answer 404 and are not listed |
| A unit's version in its image | ADR-004 | `firmware` in the index |
| Settings forms and screens drawn from the description | ADR-008, ADR-010 | `label`, `group`, `step`, choice labels in the schema now |
| A colour or larger screen, or LVGL's frame | ADR-010 | `format` in `GET /api/v1/display`; the bytes apart at `/display/frame` |
| Another chip | ADR-007 | `board`, `features`; what is chip-only (lwIP's pools) is under `debug` |
| The scale's display, its fast report (TODOs in `and_scale.c`, `gng_scale.c`) | the scale drivers | `actions` in `GET /api/v1/scale`, and `POST /api/v1/scale/{action}` for each make that can |
| Zero on scales whose drivers cannot yet (Steinberg, Creedmoor, Sartorius, US Solid, JM Science, generic: TODOs) | the scale drivers | 501 `not_supported` now; `actions` lists zero once a driver can |
| The weight's unit for every make | the scale drivers | `unit`, null today but for the A&D |
| Which unit is which, among several | OWMC's ADR-013 (the owner's review) | `hostname` and `unit_id` in the index, `GET /api/v1/network`, and the mDNS announcement with the API's and the firmware's versions |
| The console and the API saying the same | ADR-001, the owner | One table of names for both (gate 3) |
| A password | its own ADR | A header, paths unchanged |
| Waiting for news in place of polling | ADR-008, ADR-009 | `?wait=` on the event log, or a push channel |

## Experiments and recordings, for ADR-009 (reserved)

Written 2026-11-15 at the owner's challenge ("does the API capable of supporting the ADR-009 and the future AI agent assisted development and tuning needs?"), with the owner's word to settle it with the reviewer so that ADR-009 needs "minimum changes later", and the owner's warning that "tuning will need some access to huge amount of data". Reviewed in a fourth round (E1 to E21, at the end). **ADR-003 builds none of this but what "What gate 2 and gate 4 build for it" names.** It fixes the paths and the shapes now, so that ADR-009 builds to them and only adds: columns, marks, summary fields, override fields, as the growth rules allow.

**One primitive for every experiment.** PR #113's tuner and PR #101's both cut at "give me the next trial's settings" and "here is the trial's result" (ADR-009, "Handed on by ADR-002"). So the API has one call that takes a trial's settings and one record that is its result: a **test throw** and its **recording**. An agent tuning off the unit, and ADR-009's tuner on it, use the same two, and their data are the same.

**The rules for experiments:**
- **Every stage ends by itself:** a throw, its zero, a motor run, a scale recording, a cup's ask. Nothing an experiment starts turns a motor without a bound in time, because nobody need be there.
- **What pours, pours into a declared cup,** and **a cup is declared at the unit, by a press**, never over the network: a person is there when a cup is put on or emptied, and the call that gets a stuck agent moving must not be the one that pours.
- **What a throw reads is one list of names.** A powder profile's field with the name of a field in `settings/charge` is the same parameter, and the profile's is used when it is not null. An override sets that parameter for one throw. The schema's entry for each start lists what may be overridden: the fields a throw reads, not a name or a colour.
- **Try a value with a test throw's overrides, not with a PATCH:** a PATCHed gain is live for a person's next real charge, and stays so after a lost agent until a restart. The guide says so.

### The data: summaries first, the client keeps

The figures, from the A&D at 21 readings a second: a throw is 400 to 650 rows, 24 KB as floats and 40 to 50 KB as JSON; live, about 1.7 KB a second. A session of 300 trials is about 14 MB over more than 3 hours. So the data is large in sum and small by the second. The hard parts are the unit's RAM, loss, and what sending costs the charge loop, and the cup bounds a session before any protocol does: at 10 gr a trial a 200 gr cup holds 20 trials, and a person empties it.

- **The unit is the source, not the store.** It keeps a short ring; the client keeps the data. The scripted client in `tools/` writes each recording to a folder as it reads it, in the same JSON that is ADR-009's test fixture on a PC, so "is it better" is answered from files and nothing depends on what a unit still holds. A board with more room (ADR-005's extended tier, ADR-007's PSRAM) keeps more, and `samples_kept` and 410 say what is there.
- **A rich summary is the answer to "huge".** A tuner decides from a few figures a throw (both PRs judge a throw by its overthrow and its times), and ADR-009's tuner on a Pico W cannot hold hundreds of throws' samples. So the summary is where ADR-009 spends its fields, and samples are pulled for the throws someone studies.
- **No silent loss.** Rows are numbered, so a gap is seen. **The samples of the last complete recording are not dropped before the next experiment starts**, so a client that reads each recording before it starts the next loses nothing. A hold kept by the caller (`"hold": true` on a start, released by `DELETE /api/v1/recordings/{id}`) may come with ADR-009 if it is needed; never a hold released by a read, since a read changes nothing.
- **An `id` is unique among the recordings the unit holds,** so `/recordings/{id}` names one. A unit that keeps none across a restart starts its ids again, so a client that keeps recordings keys them by `boot` and `id`; each recording carries the `boot` it was made in. A board that keeps recordings across a restart keeps their `boot` and goes on from the highest `id` it kept.

### Recordings

Every throw is recorded (ADR-009: "every throw recorded"), and so is each motor run and scale recording.

| Method and path | Reply |
|---|---|
| `GET /api/v1/recordings` | `recordings`, newest first: for each its `boot`, `id`, `kind` (throw, motor_run, scale), `job` (charge, test_throw, tuning, motor_run, scale_record), `throw` (for a throw), `tag`, `started_at_ms`, `duration_ms`, `rows`, `complete` (false while it is being made), `truncated` (the cap on rows cut it), `samples_kept`, and its `summary` |
| `GET /api/v1/recordings/{id}` | The same, and `settings`, the values in force: the firmware's version, `simulated` (made with the console's simulated scale; a tuner never takes one for powder), the weight's `unit`, the controller (`controller`: `pid` today), the powder profile's fields, the charge's settings, the scale's driver and usual interval, the motors' speed limits, and `overridden`, the names given as overrides; and `marks`, each with `t_ms`, `row` (the row it falls at), `kind` (coarse_stop, gate_moved, backoff_done, paused, resumed, fine_stop, verdict; more later) and what its kind carries |
| `GET /api/v1/recordings/{id}/samples?after=N` | `columns`, `rows` (arrays of numbers in the columns' order) after row N, `next`, `complete`. Readable while the recording is made, so a client follows it live. Chunked. `?wait=s` holds the request until there is a row after N or the time is out (below) |

- **A row is a moment,** in time order, with its own `t_ms`. Its column `reading` counts the scale's readings: a row whose `reading` is the row before's holds no new weight. Today there is one row a reading; ADR-009 may add rows between readings and nothing changes.
- **The columns v1 promises are what the hardware did:** `t_ms`, `reading`, `weight`, `coarse_rps`, `fine_rps` (the speeds commanded). ADR-009 adds columns: the turns counted on each STEP pin (`motor_get_step_stats`), the gate's ratio, the reading's own time.
- **The controller's columns are the controller's.** They start with `ctl_` (`ctl_coarse_p`, `ctl_coarse_i`, ..., today's PID terms), come and go with the controller that `settings.controller` names, and a client that does not know the controller passes over them. So a controller that is not three terms (ADR-009's stop by what still falls, a top-up, PR #101's learned allowance) changes no promise.
- **A value may be `null`.** `t_ms` and `reading` are whole numbers; the rest are written with six significant digits, the scale's own resolution (999.999).
- **A throw's span** is from the start of the pour (`t_ms` 0, as `elapsed_s`) to the verdict, the settling after the fine stop included: what falls after the stop is what PR #113 measures and ADR-009 lists. The zero's readings are not in it.
- **A throw's summary** now: `target`, `weight`, `error` (weight less target), `result`, `reason`, `elapsed_s`, `pauses`. ADR-009 adds its own: overshoot, the time to the coarse stop, what fell after the stop.
- **What is kept.** Summaries of the newest 32 *p*, with their snapshots, 6 to 8 KB; samples within a fixed budget of RAM, oldest dropped first (`samples_kept` false, and the samples' path answers 410). A recording is capped at a number of rows, so a stuck throw cannot take the budget. The Pico W's figures are ADR-005's and ADR-009's.
- **A recording is named where its throw is:** `recording` in the events `throw started` and `throw finished` and in the charge's `last`, when ADR-009 builds recordings.
- **Other forms by `?format=`,** JSON the default: a CSV by a link from a tuning page (a link cannot set a header, nor can most agents' fetch tools), a packed form only if a measured rate asks for it. By the figures above it does not.
- **Built by ADR-009's gate 2 (2026-11-23),** the three paths above, with these choices:
  - **Only the charge controller's throws are recorded.** A throw of today's PID loop has none: its events' `recording` is `null`, and so is the charge's `last.recording`.
  - **A recording's `id` is its throw's number,** so `id` and `throw` are the same; both start again at a restart.
  - **The columns:** v1's, then `ctl_forecast` (the reading and what is still on its way), `ctl_glide_coarse_rps` and `ctl_glide_fine_rps` (what the glide's stream law would ask, whichever law pours).
  - **A recording longer than its rows is thinned, not cut:** every other row dropped, then one row every second reading. `thinned` counts the halvings, and `truncated` stays false. A samples reply in progress when its recording is thinned ends with `complete` false, and is read again from the start (`?after=-1`).
  - **The summary** adds the controller's `first`, `moves`, `streams`, `coarse_turns`, `fine_turns` and `stop_room`.
  - **`settings`** is shared: a snapshot is taken again only when one of its settings changed, and is `null` only for a recording whose snapshot could not be kept. `marks` is `null` once a recording's samples are not kept.
  - **What is kept:** the newest summaries, and the samples of the throw being made and of the last complete one. On the Pico 2 W that is 32 summaries and 800 rows a throw, kept across charges. **The Pico W keeps them for its charge only:** 8 summaries and 121 rows a throw, from its heap, given back when the charge ends. Its RAM has no room for them between charges, when an update's poll needs nearly all of the heap. Read them within the charge.
  - **A charge never waits for its recording.** When the heap cannot give the samples, the throws go on: `samples_kept` is false and the samples' path answers 410 `gone`.
  - **Not built:** `?wait` on samples, and `?format`.

### The cup

The one thing an experiment pours into. Its own resource, since test throws, tuning and forward motor runs all pour into it.

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/cup` | | `state` (none, asked, declared), `capacity`, `poured` |
| `POST /api/v1/cup/ask` | `{"capacity": 200}` | 202. The screen says "Put an empty cup on the pan, and press". The press zeroes the scale and declares the cup, with `poured` 0. The ask ends by itself after a time *p*. 409 `busy` while a job runs. The console's command counts as at the unit, so a bench proof needs nobody there. ADR-009's tuner on the unit asks the same way |

- `capacity` is in the scale's unit; with `unit` null, the guide warns, as for a target.
- **Poured is counted, and a drop is a touched cup.** Before each throw's zero the weight must be what the last left. A drop beyond 5 times `zero_mean_margin` is a cup touched, emptied or taken: `cup_changed`, and a new cup is asked for. A rise is what fell late, or a scale that reported late: it is added to `poured`, and given in the reply and in that recording's summary. Only a rise beyond the last target is `cup_changed`.
- A test cup is not a charge: after five trials it holds five. The LEDs do not show a charge's "good", and the screen gives the cup's total (for ADR-009).

### Test throws

A job: one throw to a target, recorded, with **no cup cycle** (no removal, no return), with a trial's settings given in its start and not stored. Charge mode needs the cup lifted and put back between throws, which an agent cannot do; a test throw is how an agent, or the tuner, runs a series of trials.

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/test_throw` | | `state` (idle, zeroing, charging, settling, fault), `in_throw`, `throw`, `recording`, `last` (its summary) |
| `POST /api/v1/test_throw/start` | `{"target": 10.0}`, and any of `powder_profile`, `tag` (up to 31 bytes), `time_s` (60 by default, 300 at most *p*), and the overridable parameters by name, for this throw only | 202. The unit zeroes on top of what is in the cup, throws to the target with the overrides, records, gives the verdict, and is idle again, with `poured` grown. Refused: 409 `no_cup` without a declared cup; 409 `cup_full` when `poured` and the target would pass `capacity`; 409 `cup_changed` as above; 409 `busy`, `driver_fault` as a charge's start; 400 for an override outside its limits, refused whole. It ends by itself: `throw ended`, `reason: time` past `time_s`; `reason: not_zeroed` when its zero does not take within a bound *p* |
| `POST /api/v1/test_throw/stop` | `{}` | A stop |

- **The one place the unit zeroes on top of powder,** by design, and safe only as far as the cup is: declared at the unit, within its capacity, unchanged since the last throw.
- **Zeroing on top depends on the scale.** The A&D's driver sends `Z` (`and_scale.c`), the Radwag's `T`, a tare, "not Zero" (`radwag_scale.c`), since a true zero works only near the scale's own zero. On a make whose zero cannot take a cup's weight, `not_zeroed` says so at once; the cup and what is poured stay inside the scale's range, past which its frames are no reading (ADR-013).
- A test throw runs through the same controller and the same floor as a charge's throw (ADR-013's pause and stop, ADR-012's driver faults), takes the same lock, and is a throw in the event log (`throw started` with `job` and `tag`) and in the one series of throw numbers.
- One trickler alone cannot be had by overrides today: a profile's speed at 0 leaves that trickler at the motor's own least speed (`charge_mode.cpp`, the higher of the two minimums). PR #101's tuning runs the coarse trickler alone, then the fine; ADR-009 adds a parameter for it, so nobody counts on today's.

### Tuning

A job, ADR-009's to design: `/api/v1/tuning`, with `state`, `start` (a powder profile, a target, the tuner's own fields) and `stop`. Its trials are test throws into the declared cup: the same recordings, with `job` tuning and a tag a trial, so its data are the same as an agent's. Its result is written into the powder profile live and kept by the profile's save, as every setting is. It plans for a person every cupful.

### Motor runs and scale recordings

Jobs in the one shape:

| Method and path | Body | Reply |
|---|---|---|
| `GET /api/v1/motor_run` | | `state`, `motor`, `speed_rps`, `time_left_s`, `recording` |
| `POST /api/v1/motor_run/start` | `{"motor": "coarse", "speed_rps": 2.0, "time_s": 5}` | 202. That motor at that speed for that time (1 to 60 s *p*), recorded with the scale's weight (`t_ms`, `reading`, `weight`, `speed_rps`): a step of speed and the scale's answer give the powder's flow rate and the scale's lag (ADR-009's "slow" and "late" scales). A run forward pours: it needs the declared cup, ends when the weight gained would pass the capacity (`reason: cup_full`), and its gain is counted into `poured`. A run backwards needs no cup. 409 `busy` while another job runs |
| `POST /api/v1/motor_run/stop` | `{}` | A stop |
| `GET /api/v1/scale_record` | | `state`, `time_left_s`, `recording` |
| `POST /api/v1/scale_record/start` | `{"time_s": 10}` | 202. Moves nothing; the scale's readings for that time (`t_ms`, `reading`, `weight`, `interval_ms`), for its rate, its noise and its gaps (ADR-013's rhythm). 409 `busy` while another job runs |
| `POST /api/v1/scale_record/stop` | `{}` | A stop |

### Development: `/api/v1/debug` (reserved, not part of v1)

For an agent developing the firmware over the network, with no probe: the log, the simulated scale and its faults, presses for the menu, a driver's registers. Its paths are its own ADR's. Two rules are fixed now:
- **It is never turned on over the network,** only at the unit (the menu) or at the console. How long it stays on, across a flash included, is its ADR's: off at every restart would defeat an agent that flashes over Wi-Fi (ADR-004).
- **While it is on, the screen and `GET /api/v1/system` say so.**

Today's `GET /api/v1/debug`, which only reads figures (lwIP's pools, the FreeRTOS heap, the server's counts, the uploads whole and cut off), is always there: the soak reads it. So are gate 2's and gate 4's routes for the proofs: `debug/load` (a reply the size of a throw's samples), `debug/wait` (a parked request), and `PUT debug/upload` (bytes taken in pieces, answered with their count and CRC-32, and dropped). None of it is part of v1.

### What gate 2 and gate 4 build for it

So that ADR-009 changes nothing in the server:
- **Gate 2: a slot that can be parked.** A request may be held, with a deadline and a condition, until there is something to send; the server's task looks at parked slots at least every 50 ms, so no other task wakes it. At most two are parked; a third request with `wait` is answered at once. The stall rule becomes "a reply that makes no progress while the unit has something to send". Built and proven in gate 2 (README).
- **Gate 2: the server's task below the menu's**, which runs the charge loop (6), so at 5 or under, and the load proven: a reply the size of a throw's samples (600 rows of 16 numbers, from a route under `debug`) pulled back to back, and a client asking four times a second, through `charge_cycle` and a soak, with no late feed of the step pulses (`motor`), the scale's readings as regular as without it, each charge's time and weight as without it, and no allocation failed (`net`).
- **Gate 4: a body in pieces.** A route may take its body as it comes, a piece at a time (`http_server_add_upload`: begin, piece, end, and abort when the client goes): ADR-004's image is 700 KB, the slot's buffer 1.5 KB. Built and proven in gate 4: 45 uploads of 700 KB back to back during a soak's charges, each whole by its CRC-32 (progress).
- **Gate 4: `?wait=s` on the event log,** with the owner's leave (below): an agent waits for `throw finished`, and the portal's charge page for its next event, in place of asking twice a second. Every reply is a whole JSON document, so every client and every agent's tool can use it. ADR-009 then uses the same for a recording's samples.

### What ADR-009 then changes in the API

Only additions: recordings and their columns, marks and summary fields; `recording` in the throw's events and in `last`; `ctl_` columns for its controller; override fields as new parameters come (each also a field of its resource); tuning's own fields; the cup, test throws, motor runs and scale recordings as built here; new `activity` values (test_throw, tuning, motor_run, scale_record); new error codes (`no_cup`, `cup_full`, `cup_changed`); 410 in the status table; `?wait=s` on samples; other formats by `?format=` if measured. No path moves, no field changes its meaning.

**Found while checking** (`wireless.c`, `eeprom.c`): the unit's host name is `opentrickler-` and the first four characters of its id, copied into `char id[4]` with no ending and printed with `%s`. `snprintf` reads on past the four bytes, and only its limit of 18 keeps the name right. Two units match in those four characters about once in 65,536, if the id is random. The bench's Pico W has the id `9CE00000`, its last five characters 0, so how random `rnd()` makes it (`eeprom.c`) is to be read first. For gate 4, where the mDNS announcement changes: the id written with its ending, and enough of it to tell units apart. **Built in gate 4:** the ending (`opentrickler-9CE0`); the name keeps its four characters, as units are known by it today. **Read since:** `rnd()` reads the ring oscillator's random bit 32 times in a row with no wait, and the bench's `9CE00000` ends in five zeros, so the bit did not change between reads: the four characters carry fewer than 16 random bits. A new id from the SDK's `get_rand_32()` is the id's own area's change, not this ADR's (raised with the owner).

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
5. **A zero over the API** (the review's S3, argued out in three rounds): taken while idle, during a cleanup and in `fault`; in `zeroing` only to put right a drift, within 5 times `zero_mean_margin` *p* of zero; refused once a throw pours and while the unit waits for the cup (and in the scale's calibration, removed since in ADR-010). So no remote zero pours onto a bare pan, nor doubles a charge left in the cup. A zero while idle and then a start still can, and the guide says so.
6. **A cleanup's time is given with its speed** (`time_s` on the PATCH): a client that loses the network leaves the motors turning no longer than the time it last gave.
7. **A held request, `?wait=s`,** on the event log in gate 4. The README's "Not in this ADR" says "Polling stays". A held request is not a push channel: each reply is a whole document, and an agent's HTTP tool can use it, where a reply that never ends it cannot. It spares the portal's and an agent's polling now, and ADR-009 uses the same for a throw's samples. Gate 2 builds the server so it can, whatever the answer. The reviewer and the author recommend it. **Allowed by the owner 2026-11-15** ("Yes to both").
8. **The experiments' safety** ("Experiments and recordings"): a cup declared only by a press at the unit; every stage of an experiment ending by itself; a forward motor run pouring only into the declared cup. The one place the unit zeroes on top of powder is a test throw into that cup.

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
| S3 | Zero refused in `zeroing`, where it is used; taken while waiting for the cup, where it starts a throw | Round 1: refused while waiting for the cup too (the author; conceded). Round 2: the reviewer showed a zero in `zeroing` after a remote start, cup off, pours onto the pan; in `zeroing` only a drift is put right, by the weight's distance from zero (the author's, in place of the lifted-cup test, which misses a cup under 10 g); taken during a cleanup and in `fault`. Round 3: the reviewer showed a charge left in the cup is zeroed away and doubled, reading good, and that ±1 g holds a pistol charge: the band is 5 times `zero_mean_margin`, in the scale's unit; refused in the scale's calibration |
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
| M1 to M9 | Names, `rev`, the frame, limits, 404 against 501, growth rules, the gate's states, server details, two things said plainly | In the tables above; the author's rule for unit suffixes; a cleanup's time (the author's), put on the speed at the reviewer's word, since a start turns nothing |
| R1 to R7 (round 2) | 413 against a long cookie; zero refused during a cleanup; `activity` in `fault`; `busy` with nothing running; `released` that is not released; `boot` past 53 bits; 400's line | 413 and 414 as the README's server; zero taken in a cleanup and `fault`; a job in `fault` is its activity, logged stopped at the fault; `view`; `held`, read only (round 3: holding is not offered in v1); 32 bits; `no_target` |

**Agreed** by both, 2026-11-15, after three rounds: the reviewer's word at round 3, "With that in, I am satisfied".

**Round 4, the experiments and the data** (at the owner's challenge, on a4c27a5), each claim read in the code by the author and all taken:

| | Found | Now |
|---|---|---|
| E1 | The cup declared over the network: a stuck agent declares again and the cup overflows | Declared only by a press at the unit; `/api/v1/cup` its own resource |
| E2 | A motor run poured with no cup, and neither it nor the scale recording had the job's shape | `motor_run` and `scale_record` as jobs; a forward run pours only into the cup |
| E3 | "A row a reading" and the PID terms as columns, the two things ADR-009 will change | A row is a moment with `reading`; promised columns are the hardware's; `ctl_` columns are the controller's |
| E4 | A test throw with no time limit; a zero on top that depends on the make | `time_s`; `not_zeroed`; every stage ends by itself |
| E5 | The recording's span unsaid; the fall after the stop left out | From the pour to the verdict; commanded speeds; `null`; `truncated` |
| E6 | Two fields under one override name; the snapshot unclear; simulated recordings like real | One list of parameter names; the snapshot in force with `overridden`, `simulated`, `unit`; overrides, not PATCH, for trials |
| E7 | A late fall read as a touched cup | A drop is a touched cup; a rise is counted into `poured` |
| E8 | Nine digits print a float's noise | Settings by the shortest decimal that reads back; samples at six digits |
| E9 to E17 | A throw not naming its recording; `debug` off at every restart; a test cup that is not a charge; one trickler alone; Pico W figures; capacity's unit; marks' rows; histories across restarts; `debug`'s paths | In the section |
| E18 | Back-pressure released by a read: a read that changes what the unit does next | The last complete recording's samples are kept until the next experiment starts; any hold is the caller's, never a read's |
| E19 | A reply that never ends is no document for an agent's tool, and holds a slot a whole throw | A held request, `?wait=s`; a parkable slot in gate 2; on the event log in gate 4 with the owner's leave |
| E20 | Representations by `Accept`, which a link cannot set | `?format=` |
| E21 | The server "below lwIP's" may share the charge loop's priority | Below the menu's task (6), so 5 or under; the load proven in gate 2 |

Two findings crossed, E16 (histories that outlive a restart) and the data part (ids that start again at a restart). Round 5: an `id` is unique among the recordings the unit holds; a client keys what it keeps by `boot` and `id`; a board that keeps recordings across a restart goes on from the highest `id` it kept (the reviewer's words).

**Agreed** by both on experiments and data, 2026-11-15, after round 5: the reviewer's word, "With it in I am satisfied, and I do not need to read it again."
