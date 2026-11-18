# ADR-008: the features of today's pages

Every feature a person can see or do on the pages the unit serves today, read line by line on 2026-11-18. It is the owner's rule "no feature is silently dropped" made into a list: each row is placed in the new pages or left out by the owner's decision, with its reason. The "Proposed" column is the agent's proposal; the owner settles it by reading. "Every page" marks a behaviour that belongs to the whole portal, not to one page. Where a row is unsure, it says so.

The pages read, whole: `src/html/web_portal.html`, `src/html/wizard.html`, `src/html/display_mirror.html`, and the routes in `src/rest_endpoints.c`. Line numbers are of the files as of commit `b79acc4`.

## How the pages are served

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 1 | `/` gives the portal on a network, the wizard while the unit is its own access point | `rest_endpoints.c:57-59`, `:90`; chosen at start, `wireless.c:600` | `GET /` | Set-up | Commissioning opens the set-up by itself on the access point (README, "Commissioning") |
| 2 | When the station joins after a start as the access point, `/` turns back to the portal | `wireless.c:342` | `GET /` | Set-up | Part of the set-up's end: what the page says when the access point stops is commissioning's design |
| 3 | `/mobile` gives the same portal | `rest_endpoints.c:91` | `GET /mobile` | Every page | Kept as another address of the one portal, so old bookmarks work; never a second page (README, "Phone first") |
| 4 | `/wizard` gives the wizard on any network | `rest_endpoints.c:92` | `GET /wizard` | Set-up | The set-up reached by an address too, from a network as well as the access point |
| 5 | `/display_mirror` gives the mirror | `rest_endpoints.c:93` | `GET /display_mirror` | Mirror | Kept; linked from the portal (today nothing links to it, row 104) |
| 6 | Each page is sent with an `ETag` of the firmware's version and VCS hash, and `Cache-Control: no-cache`; the server answers 304 to a matching tag | `rest_endpoints.c:40`, `:53`, `:87`; `http_server.c:434-435`, `:614-618` | every page GET | Every page | Already built: the README's "pages cached by version" is there for today's pages. The new pages keep it, gzipped |

## The portal: its frame

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 7 | The page's title, "OpenTrickler Web Controller" | `web_portal.html:6` | none | Every page | A name in the browser's tab |
| 8 | The bottom bar: Trickler, Settings, Purge, each with an icon; the one open is marked | `web_portal.html:817-836`, `:1270-1338` | none | Every page | The new pages' own navigation: charge, profiles, settings, update, set-up, mirror |
| 9 | The page opens on Trickler | `web_portal.html:1956-1961` | see row 10 | Charge | The charge's page is the first |
| 10 | Opening the page, or tapping Trickler, stops any cleanup that runs, also one a person started at the unit | `web_portal.html:1283`, `:1757-1767` | `POST /api/v1/cleanup/stop` | Left out (proposed) | Breaks "every client echoes the unit": a page that opens stops a job it did not start. `cleanup/stop` stops whoever's cleanup (`api_v1_commands.c:453-458`) |
| 11 | Tapping Purge stops any charge that runs, with no question, then starts a cleanup | `web_portal.html:1299-1318` | `POST /api/v1/charge/stop`, then `POST /api/v1/cleanup/start` | Left out (proposed) | The same: a tab is not a stop. The cleanup is started by its own button, with the cup asked (row 30) |
| 12 | Tapping Settings stops the charge's poll, and leaves a running cleanup running: its speed is still sent again every 2 s, its slider hidden | `web_portal.html:1319-1334`; the resend is not cleared there | none | Left out (proposed) | Looks broken: a motor turns with no control on screen. With the event stream every page shows the job that runs and its stop |
| 13 | No light or dark switch; DaisyUI's styles and the icons' `dark:` classes follow the device's setting | `web_portal.html:8-9`, `:819-834` | none | Every page | The README asks the page to follow the device. Thought so (DaisyUI 4's `full.min.css` and the Tailwind CDN's default), not checked in a browser |
| 14 | Nothing kept in the browser: no local storage, no cookie | whole file (no `localStorage`, `sessionStorage` or `cookie`) | none | Every page | Today a fact. The keep-awake switch would be the first thing kept (README, "Keep the phone awake") |
| 15 | Phones: the viewport set for a phone, the bottom bar, dialogs from the bottom on a small screen (`modal-bottom sm:modal-middle`); no script of its own for phones | `web_portal.html:11`, `:817`, `:840`, `:859`, `:880` | none | Every page | One layout that grows with the width (README, "Phone first") |

## The portal: Trickler (the charge)

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 16 | The current weight, large; "---" with no reading; to three decimals at most, whatever the charge's `decimal_places` | `web_portal.html:33-34`, `:1052-1059`, `:1125-1130` | `GET /api/v1/charge` (`weight`) | Charge | The largest thing on the charge's page (README). Shown at the unit's decimals, as the screen does |
| 17 | A bar of the weight against the target | `web_portal.html:37`, `:1056-1058`, `:1126` | `GET /api/v1/charge` (`weight`, `target`) | Charge | How far the throw has got, at a glance |
| 18 | The profile in use, named under the weight | `web_portal.html:35`, `:1133-1134` | `GET /api/v1/charge` (`powder_profile_name`) | Charge | Which powder the next throw is for |
| 19 | Tapping the profile's name: a dialog lists the profiles, the one in use chosen; Confirm sets it. No Cancel button (Escape closes it) | `web_portal.html:35`, `:880-894`, `:1075-1115` | `GET /api/v1/powder_profiles`, `GET /api/v1/charge`, `PATCH /api/v1/charge` (`powder_profile`) | Charge | A visible choice of profile on the charge's page, not one hidden behind a tap on its name |
| 20 | The charge's time, "-.-- s" when there is none | `web_portal.html:43-44`, `:1137-1140` | `GET /api/v1/charge` (`elapsed_s`) | Charge | The throw's time |
| 21 | The charge's steps: Wait, Charging, Remove Cup, Return Cup, lit up to the state (zeroing lights Wait; idle, `starting` and any state it does not name light none) | `web_portal.html:59-64`, `:1035-1049` | `GET /api/v1/charge` (`state`) | Charge, as the status word | Tells the person what the unit waits for. Kept as the status, in the screen's words ("Zeroing", "Charging", "Lift the cup", "Return the cup"), every state named; a step track beside it was dropped: the same news twice (the owner, 2026-11-18) |
| 22 | The target typed (a number, step 0.001) | `web_portal.html:67` | none | Charge | The target, as large as the weight (README) |
| 23 | Start: an empty target shows "Invalid charge weight"; otherwise the charge starts at that target | `web_portal.html:71`, `:896-904`, `:994-1016` | `POST /api/v1/charge/start` (`target`) | Charge | Kept, with the new question first: an empty cup on the scale (README, "Handed on by ADR-010"). The target checked against the schema's limits, said on the page, not in a dialog |
| 24 | Stop | `web_portal.html:72`, `:1018-1029` | `POST /api/v1/charge/stop` | Charge | Kept |
| 25 | Start is disabled while a charge runs, Stop while none does | `web_portal.html:1061-1073`, `:1142` | `GET /api/v1/charge` (`state`) | Charge | The page shows what the unit does |
| 26 | Over Throw! and Under Throw! dialogs, once a throw, never for one before the page opened; nothing for a good throw | `web_portal.html:906-924`, `:1144-1153` | `GET /api/v1/charge` (`last.throw`, `last.result`) | Charge | The verdict of each throw, shown on the page beside the weight, not as a pop-up the person must close with dirty hands |
| 27 | A refused start, stop or profile shows the unit's own words in a dialog ("Not started", "Not stopped", "Not changed") | `web_portal.html:1008-1010`, `:1021-1023`, `:1107-1109`, `:980-988` | the error of each call | Every page | The unit's words kept; shown on the page, beside the control |
| 28 | The charge read every 500 ms on Trickler and Purge, at once after each command; stopped on Settings | `web_portal.html:1118-1165`, `:1341-1344`, `:1329` | `GET /api/v1/charge` | Charge | Replaced by the event stream (README, "The unit to the page", gate 2) |
| 29 | A read that fails shows "---" and no step lit; nothing says the unit is not reached (the console only) | `web_portal.html:1155-1159` | none | Every page | Proposed: the page says when the unit is not reached, and again when it is back (the stream's reconnect) |

## The portal: Purge (the cleanup)

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 30 | The cleanup starts when Purge is tapped, once a charge is idle (asked every 200 ms, 10 s at most); a refusal shows "Cleanup not started" | `web_portal.html:1299-1318`, `:1741-1768` | `GET /api/v1/charge`, `POST /api/v1/cleanup/start` | Charge | **No page for it in the agreed list** (README, "The pages": charge, profiles, settings, update, set-up, mirror). Proposed: on the charge's page as the other job, started by its own button with the cup asked (the screen asks before a cleanup too). The owner to place it |
| 31 | Trickler speed, a slider from -5 to 5 rev/s in steps of 0.1, sent 20 ms after it stops moving | `web_portal.html:81-82`, `:1726-1732` | `PATCH /api/v1/cleanup` (`speed_rps`, `time_s`: 5) | Charge | Kept with the cleanup. A slider is a fine pointer: large steps or buttons for gloved hands (README) |
| 32 | While the speed is not 0, it is sent again every 2 s, only to a cleanup that runs and was started from the web; a refusal or a failed send stops the resend and shows 0 | `web_portal.html:1687-1724` | `GET /api/v1/cleanup` (`state`, `by`), `PATCH /api/v1/cleanup` | Charge | Safety: a page that goes, or a phone that leaves, leaves no motor turning (each speed lasts 5 s) |
| 33 | Stop: the speed set to 0. The cleanup itself goes on at 0 until the person leaves Purge | `web_portal.html:83`, `:1734-1737` | `PATCH /api/v1/cleanup` (`speed_rps`: 0) | Charge | Kept, and a stop that ends the cleanup beside it (`POST /api/v1/cleanup/stop`) |

## The portal: Scale Control and Gate Control (on Trickler and Purge)

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 34 | Scale Zero; "Scale Zero Successful" on success, the unit's words ("Not zeroed") when refused | `web_portal.html:89-91`, `:944-952`, `:1168-1181` | `POST /api/v1/scale/zero` | Charge | A zero is done at the bench, before a charge. Also listed with settings/scale as its action (README, "Handed on by ADR-010") |
| 35 | Gate Open and Gate Close buttons | `web_portal.html:95`, `:101`, `:113`, `:1184-1186` | `PATCH /api/v1/gate` (`ratio` 0 or 1) | Settings (settings/gate) | The gate's actions, listed with its resource in the schema (README). Shown only when the unit reports a gate (ADR-005, gate 2) |
| 36 | The gate's ratio, a slider from 0.000 (open) to 1.000 (closed) and a number shown beside it, sent 60 ms after the slider stops | `web_portal.html:104-111`, `:117-131`, `:1196-1257` | `PATCH /api/v1/gate` (`ratio`) | Settings (settings/gate) | As row 35. The number is read-only |
| 37 | The ratio shows 0.500 when the page opens, never the gate's own: the page never reads the gate | `web_portal.html:109`, `:122`; no `GET /api/v1/gate` in the file | none | Settings (settings/gate) | Looks broken. The new page shows the gate's state and ratio as the unit gives them |
| 38 | Code for typing the ratio (`onServoGateRatioNumberInput`, `servoGateApplyRatioFromUi`, `debouncedServoGateRatioSendFromNumber`) that nothing calls: the number is read-only | `web_portal.html:1232-1247`, `:1259-1268` | none | Left out (proposed) | Dead code |

## The portal: Settings, how they work

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 39 | A side drawer with ten links (Scale, Profiles, Charge Mode, Wireless, Coarse Motor, Fine Motor, Neopixel LED, Mini 12864 Module, Servo Gate, System Control), the open one's name at the top; Settings opens on Scale | `web_portal.html:141-152`, `:794-808`, `:1332`, `:1347-1369` | none | Settings | The settings' resources, drawn from the schema; profiles and system get pages of their own |
| 40 | Each form filled from its resource when opened; a Wi-Fi key is never filled | `web_portal.html:1371-1378`, `:1431-1453` | `GET` of the form's resource | Settings | Kept |
| 41 | The schema read once, for each field's type (a number sent as a number, a choice of numbers as a number, a boolean as a boolean) and each resource's save path; an empty field is not sent | `web_portal.html:1407-1476` | `GET /api/v1/schema` | Settings | The new forms are drawn whole from the schema, labels, limits and choices too (README) |
| 42 | Apply opens "Save <section> Settings: Do you also want to save settings to EEPROM?" with Save to EEPROM, Apply, Cancel | `web_portal.html:859-878`, `:1602-1634` | see row 43 | Settings | Proposed: no dialog at each Apply. A change is live; the resource's `saved` shows it is not stored, and a Store button stores it. The owner to settle |
| 43 | Apply writes the form; Save to EEPROM writes it, then stores it, then reads it again; the form is filled from the reply | `web_portal.html:1496-1513` | `PATCH <resource>`, `POST <resource's save path>`, `GET <resource>` | Settings | Kept |
| 44 | "Settings Applied" on success; on a refusal "Not applied" with the field and the unit's words | `web_portal.html:926-934`, `:1506-1512`, `:1479-1494` | the error's `field` and `message` | Settings | Kept, said beside the field the unit refused, not in a dialog |
| 45 | The old way of writing a form, by GET with its fields as a query and `ee` to store, and the read of a form by its `action` | `web_portal.html:1379-1394`, `:1557-1599` | the old `/rest/` paths, gone (ADR-003, gate 4) | Left out (proposed) | Dead code: every form has a resource or is the system form |

## The portal: Settings, by resource

Each resource's form maps its fields one to one with nothing special, unless a row says so. Field names as the API's.

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 46 | Scale (3): `driver`, `baud` (4800, 9600, 19200), `format` (8n1, 7n1) | `web_portal.html:156-193` | `GET`/`PATCH /api/v1/settings/scale`, `POST .../scale/save` | Settings (settings/scale) | Kept, from the schema |
| 47 | The scale's makes offered: A&D FX-i, Steinberg SBS, G&G JJB, US Solid JFDBS, JM Science, Creedmoor, Radwag PS R2, Sartorius, Generic | `web_portal.html:160-169` | as row 46 | Settings (settings/scale) | Kept, from the schema's choices |
| 48 | The make "Simulated Scale Driver" offered | `web_portal.html:170` | as row 46 | Left out (decided) | The owner, ADR-010: a debug feature, not on the screen or the page; the schema still lists it for the API and the console |
| 49 | Charge Mode (16): `colour_good`, `colour_under`, `colour_over`, `colour_not_ready`, `coarse_stop_threshold`, `fine_stop_threshold`, `steady_sd_margin`, `zero_mean_margin`, `decimal_places` (2, 3), `precharge_enable`, `precharge_time_ms`, `precharge_speed_rps`, `coarse_stop_gate_ratio`, `coarse_stop_backoff_enable`, `coarse_stop_backoff_turns`, `coarse_stop_backoff_speed_rps` | `web_portal.html:292-395` | `GET`/`PATCH /api/v1/settings/charge`, `POST .../charge/save` | Settings (settings/charge) | Kept, from the schema. The four colours belong to the LEDs: shown only when the unit reports them (ADR-005, gate 2) |
| 50 | The back-off's turns offered as 1/8, 1/4, 1/2 and 1; the schema takes 0.125 to 2 in steps of 0.125 | `web_portal.html:380-385`; `charge_mode_settings.c:56-59` | as row 49 | Settings (settings/charge) | Drawn from the schema, the whole range is offered |
| 51 | "Mid-stage Servo Gate Position": a number 0 to 1. The schema also takes null (the gate not moved), which the page cannot write: an empty field is not sent | `web_portal.html:363-366`, `:1460`; `charge_mode_settings.c:49-52` | as row 49 | Settings (settings/charge) | The new form offers "not moved" as well. Only with a gate (ADR-005) |
| 52 | "Enable Pre-Charge (requires servo gate)" says in its label that it needs the gate | `web_portal.html:347` | as row 49 | Settings (settings/charge) | The schema's `about` says it; shown only with a gate |
| 53 | Wireless (5): `ssid`, `key`, `auth` (open, wpa_tkip, wpa2_aes, wpa2_mixed), `timeout_ms`, `enable` | `web_portal.html:398-438` | `GET`/`PATCH /api/v1/settings/wifi`, `POST .../wifi/save` | Settings (settings/wifi) | Kept, from the schema; the set-up uses the same resource |
| 54 | The Wi-Fi key: marked "Write Only", a password field, never filled, left as it is when empty | `web_portal.html:405-411`, `:1434` | as row 53 | Settings (settings/wifi) | Kept: the unit never gives the key |
| 55 | Coarse Trickler Motor (10): `acceleration_rps2`, `full_steps_per_rotation` (200, 400), `current_ma`, `microsteps` (16 to 256), `max_speed_rps`, `min_speed_rps`, `r_sense_mohm`, `gear_ratio`, `invert_enable`, `invert_direction` | `web_portal.html:441-510` | `GET`/`PATCH /api/v1/settings/motors/coarse`, `POST /api/v1/settings/motors/save` | Settings (settings/motors/coarse) | Kept, from the schema. The page offers microsteps 16 to 256; the schema 1 to 256 (`motors.c:377-383`); full steps 200 or 400, the schema an integer range (`motors.c:389-392`) |
| 56 | Fine Trickler Motor (10): the same ten fields as row 55 | `web_portal.html:513-582` | `GET`/`PATCH /api/v1/settings/motors/fine`, `POST /api/v1/settings/motors/save` | Settings (settings/motors/fine) | As row 55 |
| 57 | Neopixel LED (6): `backlight_colour`, `led1_colour`, `led2_colour`, `pwm3_chain_count` (1 to 16), `pwm3_rgbw`, `pwm3_colour_order` (rgb, grb) | `web_portal.html:585-649` | `GET`/`PATCH /api/v1/settings/leds`, `POST .../leds/save` | Settings (settings/leds) | Kept, from the schema; shown only when the unit reports its LEDs (ADR-005) |
| 58 | A note: "The default Neopxiel LED colour can be overridden by Charge Mode Functions." | `web_portal.html:587-590` | none | Settings (settings/leds) | Said by the schema's `applies` for each colour (`neopixel_led.c:140`) |
| 59 | Mini 12864 Module (3): `invert_encoder`, `invert_display`, `rotation` (0, 180) | `web_portal.html:652-680` | `GET`/`PATCH /api/v1/settings/display`, `POST .../display/save` | Settings (settings/display) | Kept, from the schema. The schema's fourth field, `charge_view`, is not on today's page (see "New") |
| 60 | Servo Gate (7): `enable`, `shutter0_close_duty`, `shutter0_open_duty`, `shutter1_close_duty`, `shutter1_open_duty`, `close_speed_per_s`, `open_speed_per_s` | `web_portal.html:682-724` | `GET`/`PATCH /api/v1/settings/gate`, `POST .../gate/save` | Settings (settings/gate) | Kept, from the schema, with the gate's actions (rows 35, 36); shown only with a gate (ADR-005) |

## The portal: Profiles (in Settings)

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 61 | A link "Powder Profile Tuning Guide" to the guide on GitHub | `web_portal.html:199-213` | none (`https://github.com/eamars/OpenTrickler/blob/main/Manual/tuning_guide.md`) | Profiles | A link out costs nothing offline and opens only with the internet; said so beside it. Unsure: it may be ADR-009's, with tuning |
| 62 | Select Profile, #0 to #7, each with its name; choosing one fills the form | `web_portal.html:215-225`, `:1397-1400`, `:1441-1452` | `GET /api/v1/powder_profiles`, `GET /api/v1/powder_profiles/{id}` | Profiles | The profiles side by side (README, "What we want"), not one at a time |
| 63 | A profile (11): `name` (15 characters), `coarse_kp`, `coarse_ki`, `coarse_kd`, `coarse_min_speed_rps`, `coarse_max_speed_rps`, `fine_kp`, `fine_ki`, `fine_kd`, `fine_min_speed_rps`, `fine_max_speed_rps` | `web_portal.html:232-287` | `PATCH /api/v1/powder_profiles/{id}`, `POST /api/v1/powder_profiles/save` | Profiles | Kept, from the schema. Writing gains is a setting; tuning them with powder is ADR-009's |
| 64 | Export Profile: the profile chosen downloaded as `<name>.json`, with the firmware and unit id, in the import's format, so it imports back into the same profile | `web_portal.html:227`, `:1770-1791` | `GET /api/v1`, `GET /api/v1/powder_profiles/{id}` | Profiles | Kept: a profile shared or kept |
| 65 | No import of one profile on this page: a profile's file is imported by System Control's Import (row 71), into the profile it came from | `web_portal.html:1868-1902` | as row 71 | Profiles | Proposed: an import beside the export, into the profile chosen. The owner to settle |

## The portal: System Control (in Settings)

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 66 | The unit's facts, read-only: Unique ID, Firmware Version, VCS Hash, Build Type | `web_portal.html:729-747`, `:1516-1523` | `GET /api/v1` (`unit_id`, `firmware.version`, `.vcs`, `.build`) | System | Kept. The board, the network and the uptime (`GET /api/v1/system`, `/network`) could join them: not on today's page |
| 67 | Save All Changes to EEPROM (Yes/No) | `web_portal.html:751-757`, `:1529-1531` | `POST /api/v1/settings/save` | System | Kept as one button: store everything not stored |
| 68 | Reboot (Yes/No) | `web_portal.html:760-766`, `:1535-1537` | `POST /api/v1/system/restart` | System | Kept as a button, the page following the unit through its restart |
| 69 | Erase EEPROM (Yes/No), marked Warning; when Yes, the reboot is not asked | `web_portal.html:768-777`, `:1532-1534` | `POST /api/v1/system/erase` (`confirm`: "erase") | System | The factory reset kept, with its own confirmation saying what is lost; the unit comes back as its access point with a new name (README, "Commissioning") |
| 70 | Apply shows "Pending Actions", a list of what is chosen ("You cannot undo this action!" for the erase), then Apply or Cancel. **All three choices start at Yes**: nothing selects No, and the form is never filled for them | `web_portal.html:779`, `:840-856`, `:1636-1675`, `:1525-1546` | as rows 67-69 | System | Looks broken: an Apply with no change saves, then erases. Proposed: three buttons, each its own confirmation, nothing chosen in advance |
| 71 | Export Config: every setting and profile downloaded as `opentrickler_<unit id>_settings.json`, with no Wi-Fi key | `web_portal.html:786`, `:1793-1797` | `GET /api/v1/settings` | System | Kept: the backup |
| 72 | Import Config: a `.json` chosen; each resource written with the fields this unit's schema has and may write; a field refused is left out and named, the rest written again; "Restored, but for these" lists what was left out; a file that does not read shows "Failed to Restore Settings" | `web_portal.html:787`, `:1864-1953`, `:936-942` | `GET /api/v1/schema`, `PATCH` of each resource in the file | System | Kept: the backup restored, on this unit or another |
| 73 | Import also reads an export from before API v1 (the old `/rest/` paths and their codes, profiles by `?pf=N`), turned into v1's fields | `web_portal.html:1799-1862`, `:1923` | as row 72 | System | Proposed kept: a person's backup from an older firmware still restores. About 60 lines; the owner may drop it |
| 74 | After an import, the page sets Save and Reboot to Yes and opens Pending Actions. Erase stays at its default, Yes (row 70), so the list asks to erase what was just imported | `web_portal.html:1929-1936` | as rows 67-69 | System | Looks broken. Proposed: after an import, the page offers "Store and restart", nothing else |

## The portal: dialogs and code that nothing uses

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 75 | "Failed to Zero Scale" dialog: never shown, a refused zero uses the unit's words (row 34) | `web_portal.html:954-962` | none | Left out (proposed) | Dead |
| 76 | `enterChargeMode(true)`, a start with no target: never called (Start uses row 23) | `web_portal.html:1018-1019` | `POST /api/v1/charge/start` | Left out (proposed) | Dead branch |
| 77 | The sidebar's list has a broken attribute, `id=drawerSlide"` | `web_portal.html:796` | none | Left out (proposed) | A typo; nothing reads the id |

## The wizard (`wizard.html`)

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 78 | The page's title "OpenTrickler Setup Wizard" and its heading "Configure WiFi" | `wizard.html:5`, `:125` | none | Set-up | The set-up's first step |
| 79 | Its own styles in the page, no internet; the font Raleway is named but never loaded, so the browser's own is used | `wizard.html:8-121` | none | Set-up | One look with the portal, from the same sources (README, "One code base") |
| 80 | The form filled from the unit when it opens (the key never) | `wizard.html:217-232` | `GET /api/v1/settings/wifi` | Set-up | Kept |
| 81 | Wi-Fi (5): `ssid` (31), `key` (63, a password field, sent only when typed), `auth` (open, wpa_tkip, wpa2_aes, wpa2_mixed), `timeout_ms` (30000 in the page until filled), `enable` | `wizard.html:128-161`, `:190-199` | as row 82 | Set-up | Kept, from the schema; a list of networks scanned in place of a name typed is proposed (README, "Proposed, not settled") |
| 82 | Complete & Reboot: the Wi-Fi written, stored, then the unit restarted, each once the last is answered; "Stored. The unit restarts and joins the network." | `wizard.html:163-166`, `:201-215` | `PATCH /api/v1/settings/wifi`, `POST /api/v1/settings/wifi/save`, `POST /api/v1/system/restart` | Set-up | Proposed: the join tried and its result told with the access point kept up, no restart (README, "Proposed, not settled"); settled with the mock |
| 83 | A refusal shown in a line under the button, in the unit's words, with the step ("Not taken", "Not stored", "Stored, but the unit did not restart") | `wizard.html:166`, `:176-188`, `:209-211` | the error of each call | Set-up | Kept |
| 84 | A wrong key is found only after the restart: the unit comes back as its access point and the person starts again | `wizard.html:201-215` (nothing after the restart) | none | Set-up | Commissioning's hard part (README, "What we know") |
| 85 | No link to the portal, and nothing else set up | whole file | none | Set-up | Proposed: the set-up goes on to the scale, the drivers and a first charge, each skippable (README, "Proposed, not settled") |

## The mirror (`display_mirror.html`)

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 86 | The screen drawn four times its size (512 by 256), lit pixels light on a dark ground as the 12864's glass | `display_mirror.html:12`, `:20-28`, `:36-45` | none | Mirror | Kept |
| 87 | The frame read once a second | `display_mirror.html:31-35`, `:52-53` | `GET /api/v1/display/frame` | Mirror | Kept on its own poll, not on the event stream (README, "Its life cycle") |
| 88 | The size (128 by 64) and the format (`mono_rows`) written in the page, not read from the unit | `display_mirror.html:27-28`, `:36-39` | none (`GET /api/v1/display` not read) | Mirror | Proposed: read from `GET /api/v1/display`, so another screen is drawn right (README: the mirror follows the screen's view) |
| 89 | A failed read leaves the last frame, and says so only in the console | `display_mirror.html:47-49` | none | Mirror | As row 29: the page says the unit is not reached |
| 90 | No title in the page but "Display Mirror", no controls, no link back to the portal | `display_mirror.html:4` | none | Mirror | Proposed: a page of the portal, with its navigation |

## Not served

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 91 | `plot_weight.html`: a chart of the scale's weight. Not served by any route; it reads `/rest/scale_weight`, gone since ADR-003's gate 4, every 500 ms, and loads Plotly and jQuery from CDNs | `plot_weight.html:5-6`, `:33-35`, `:62` | `GET /rest/scale_weight` (gone) | Left out: ADR-009 | Charts of a throw are ADR-009's. The file can be deleted |

## Loaded from the internet

| # | Feature | Where | API it uses | Proposed | Why |
|---|---|---|---|---|---|
| 92 | DaisyUI 4.7.3's styles from jsDelivr | `web_portal.html:8` | `https://cdn.jsdelivr.net/npm/daisyui@4.7.3/dist/full.min.css` | Left out (decided) | The pages need no internet: styles trimmed at build time and served by the unit (README, "The pages") |
| 93 | Tailwind's script, unversioned, from its CDN; it builds the styles in the browser at run time | `web_portal.html:9` | `https://cdn.tailwindcss.com` | Left out (decided) | As row 92 |
| 94 | The page's icon from GitHub | `web_portal.html:12` | `https://raw.githubusercontent.com/eamars/OpenTrickler/main/Resources/favicon.ico` | Every page | An icon served by the unit, small, or none |
| 95 | The tuning guide's link to GitHub | `web_portal.html:212` | `https://github.com/eamars/OpenTrickler/blob/main/Manual/tuning_guide.md` | Profiles | Row 61 |
| 96 | Plotly and jQuery, in the page not served | `plot_weight.html:5-6` | `https://cdn.plot.ly/plotly-latest.min.js`, `https://code.jquery.com/jquery-3.6.0.min.js` | Left out: ADR-009 | Row 91 |

The wizard and the mirror load nothing from the internet. The `http://www.w3.org/2000/svg` in the portal's icons is a name, not a fetch.

## New, not in today's pages

Handed on to this ADR, and placed here so they are not taken for today's features:
- **The update's offer:** a release found by the unit's poll, its note, marked when critical; a yes fetches and installs it; how far it got; the unit followed through its restart; how it ended in plain words (ADR-004, ADR-010; `GET /api/v1/update`, `POST .../update/start`, `.../install`, `.../stop`). Update.
- **The upload by hand:** a file chosen and sent, the file this unit takes named, a wrong one explained (`PUT /api/v1/update/image`). Update.
- **The update's source and channel,** stored together (`settings/update`: `channel`, `source`), and a poll asked now (`POST /api/v1/update/check`); an offer from the bench host said so (`found.bench`). Update.
- **The charge's view on the screen,** `charge_view` (weight or chart) in `settings/display` (`mini_12864_settings.c:42-45`): in the schema, not on today's page. Settings (settings/display), drawn from the schema with the rest.
- **The cup asked** before a charge or a cleanup starts from the page (ADR-010, settled 2026-11-18). Charge.
- **Keep this screen awake,** a switch, on by default (README, "Keep the phone awake"). Charge.
- **The set-up beyond the Wi-Fi:** a list of networks, the join told without a restart, the scale and its live weight, the drivers found, a first charge offered (README, "Proposed, not settled"). Set-up.
- **A setting changed elsewhere** shown on an open page with no reload (README, "Every client echoes the unit"). Every page.
- **Only what the unit carries** shown, by its report (ADR-005, gate 2). Every page.

## Counts

| Part | Rows |
|---|---|
| How the pages are served | 6 |
| The portal | 71 (frame 9, charge 14, cleanup 4, scale and gate 5, settings' working 7, by resource 15, profiles 5, system 9, unused 3) |
| The wizard | 8 |
| The mirror | 5 |
| Not served | 1 |
| Loaded from the internet | 5 |
| **All** | **96** |

Proposed: Charge 18, Profiles 6, Settings 23, Set-up 11, Mirror 6, System 9, Every page 10, Update 0 (the update is all new), and left out 13:
- decided by the owner already, 3: the simulated scale (48) and the two CDNs (92, 93);
- ADR-009's, 2: `plot_weight.html` and its CDNs (91, 96);
- the agent's proposal, 8: the stops a tab or a page's opening makes (10, 11), the cleanup left running under Settings (12), and dead code (38, 45, 75, 76, 77).

**The fields.** `web_portal.html` has 96 `name="..."`, as the README says, but 17 are not fields: the viewport's `<meta>` (line 11), the ten settings sections' names (156, 196, 292, 398, 441, 513, 585, 652, 682, 727) and six in the script (1388, 1427, 1433, 1441, 1521, 1589). That leaves **79 fields**:
- 71 settings fields in nine resources: scale 3, powder profile 11, charge 16, Wi-Fi 5, coarse motor 10, fine motor 10, LEDs 6, display 3, gate 7;
- the profile's selector, `pf`;
- System Control's seven, `s0` to `s6` (four facts, three actions).

The schema has 74 fields in the same resources and `settings/update`: the page lacks three, `charge_view` and the update's `channel` and `source` (above, "New"). `wizard.html` has 7 `name="..."`: the viewport's `<meta>`, five Wi-Fi fields, one in the script.
