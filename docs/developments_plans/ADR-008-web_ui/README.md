# ADR-008: a web UI that needs no internet

Status: **Draft** 2026-11-13, revised 2026-11-18 (ADR-015, commissioning, folded in). Not approved. Captures what is wrong with the pages today and ideas for the new ones; the look is shown to the owner as a mock before anything is built.

## What we want

- The whole web UI comes from the unit. It works from a phone joined to the unit's own access point, with no internet.
- It shows what this board carries and nothing else.
- It is the place for what the small screen cannot show: charts, profiles side by side, an update.
- A new unit, or one just reset, is taken from its first start to its first charge by one designed process, not by steps found here and there (commissioning, below).

## Owner's direction

- 2026-11-13: implement a better web UI.
- 2026-11-13: one implementation: the same pages on every board.
- 2026-11-17, on the QR code proposed in ADR-010's menu design: "I'm not sure about the QR code. Perhaps that's a feature for future ADR. I'd like to design the commissioning process properly, not ad-hoc." It became ADR-015.
- 2026-11-18: ADR-015 "may be folded into ADR-008 and sharing the same code base and capture requirements together".
- 2026-11-18, on what is hard: a person "needs to 1. Join the soft AP. 2. Go to a specific URL. which is the hardest part for the average user if they are not proficient with browser."
- 2026-11-18, deciding: "I'd take the existing captive portal route and do it together with the main web portal." ESP's SoftAP provisioning app was weighed and not taken (see "Commissioning").

## Why here

Second, after ADR-010 (the owner, 2026-11-17: the current Pico platform first, in the order 010, 008, 009, 014, then 006 and 007 together). The screen comes first, so the unit's poll and the update's job are built in ADR-010, and this ADR's page shows the same offer the screen does. It stands on ADR-003's server, on that poll, and on ADR-005's report of what a board carries. It is proven on the Pico W and the Pico 2 W; ADR-007's board takes the same pages later.

Commissioning was ADR-015, proposed on 2026-11-17 to come between ADR-010 and ADR-008 so the set-up wizard would be built once. Its screen half was built by ADR-010 (the Wi-Fi page), and what is left is web: the owner folded it in here on 2026-11-18, so its requirements are captured with the portal's and built from the same code. ADR-015 is Archived.

## What we know (checked 2026-11-13, brought up to date 2026-11-18)

- **The pages** (`src/html/`), each one file of markup, styles and script together:
  - `web_portal.html`, 1,966 lines, served at `/` on a network and at `/mobile`;
  - `wizard.html`, 235 lines, self-contained, served at `/` while the unit is its own access point, and at `/wizard`;
  - `display_mirror.html`, 55 lines, at `/display_mirror`: it reads the screen's frame at `/api/v1/display/frame` once a second, in the `mono_rows` format of `GET /api/v1/display` (ADR-010);
  - `plot_weight.html` is in the folder and is not served; it loads Plotly and jQuery from CDNs. `css/` and `js/` are empty.
- **The portal needs the internet for its look.** It loads Tailwind and DaisyUI from CDNs and its icon from GitHub, and links to the tuning guide on GitHub. With no internet the page has no styling.
- **It is on API v1** (ADR-003): every read and write is a `fetch` to `/api/v1/...`, the refusals shown from the API's error. Its settings forms use the schema only in part: it reads `GET /api/v1/schema` for each field's type (to send a number as a number), each resource's save path, and which fields an import may write; the forms themselves, their fields, labels, limits and choices, are written in the page by hand (96 `name="..."`).
- **It polls; nothing is pushed.** The charge's state every 500 ms; the cleanup's speed sent again every 2 s while it runs. The event log's held reads (`/api/v1/events?wait`, ADR-003) are not used by any page.
- **In the image** (the Pico W's map, ADR-010's last build): 102,741 bytes, the portal 93,834, the wizard 7,055, the mirror 1,852. Gzipped they are 18.6 KB. `scripts/html2header.py` can minify (`minify_html`), but the build calls it with `--no-minify`, and `minify_html` is not in the venv.
- **The flash left:** the Pico W's image is 978.1 KB of its 1,016 KB slot, 38 KB left; its RAM at link 252.5 KB of 256 (ADR-010's last figures). What the pages add or take away is measured against these.
- **`tests/web_test.py`** serves the pages from `src/html` and a recording of a real unit's replies (`tests/web_test_data.json`) at http://127.0.0.1:8000/, on Python's own HTTP server, and checks each write against the recorded schema (ADR-003). `--record <address>` records a unit again when the API grows.
- **Open PRs with pages:** #113 adds a second, separate page for powder analysis (928 lines) and 17 lines to the portal; #111 adds an update section of 155 lines to the portal. #101, which added about 950 lines to the portal, was closed by ADR-002.

**Handed on by ADR-004** (2026-11-16): the update's offer on the web page. ADR-004 built what it rests on and left the offer to this ADR and ADR-010. Until one of them ships it, an update is reached through the REST API only, for debugging, and is not for users (the owner: "not production ready until the UI is released"). `rc_run.py update` plays the offer's part on the bench.
- **What is there** (built and shown on both boards, ADR-004's progress):
  - `GET /api/v1/update`: the job's `state`; what the unit runs (`version`, `block`, `slot`, `trial`); `takes`, the manifest's name for the file this unit needs; `free_bytes`; `received`; and `last`, how the last update ended (confirmed, back, refused, stopped, time) with the version tried.
  - The job: `POST .../start`, `PUT .../image` (the release's file for `takes`, as `application/octet-stream`), `POST .../install`, `POST .../stop`. The update's events in the event log: started, stopped, received, installing, confirmed, back.
  - The refusals, each with its code (api_v1.md, "Firmware updates"): 409 `busy` while another job runs, a person has a screen open at the unit, the image running is still on trial, or another upload is open; 413 too large; 400 for an image that is not this unit's, not newer or not whole, the job then ended and nothing installed.
  - Every release has a `manifest.json`: the version, a note, an importance (normal or critical), and for each name `takes` gives its file, size, SHA-256 and block version (as `running.block` is written), so the unit knows whether it is newer before it fetches it.
  - The times measured: about 800 KB sent in 14 s; the unit back on the network about 8 s after the install and confirmed at once; an update that is not kept comes back on the old image after about 75 s.
- **The owner's direction** (2026-11-16):
  - The web page sets the update source: GitHub's releases by default.
  - The web page also takes a firmware file uploaded by hand, for those who want it ("prosumers").
  - The screen only polls the source the web set, and offers an update from a row in the main menu (ADR-010), never a pop-up at every start.
  - No authentication for now: the unit has no password either, and a PIN would make the screen mandatory, which some users may not want. Perhaps an optional feature later.
- **PR #111 stays open until this ADR's offer works** (the owner, 2026-11-17): the whole OTA flow shown end to end, from the offer on the page to the unit confirmed on the new version. Then it is closed with its note, on the owner's word.
- **What follows, and is recommended here:** the unit polls the source, not the browser. The screen needs the unit to poll anyway, so the web page shows the same offer the screen does. That is one implementation, not two, and the page then needs no access to GitHub of its own. The poll, the download into the free slot and the stored setting for the source are firmware. They moved to ADR-010 (2026-11-17), with the update's channel, stable or beta (ADR-010's `menu_design.md`), and were built there (gate 5): the stored `settings/update` (its channel and source), `POST /api/v1/update/check`, and `poll` and `found` in `GET /api/v1/update`, a release found also an event (api_v1.md). This ADR builds the page's offer on them, the page where the source and the channel are set, and the upload by hand.
- **What the page has to do,** so a person needs to know none of it (the owner's "too prosumer" list: where firmware comes from, which file is theirs, that the power must stay on, the wrong board):
  - **the source:** GitHub's latest release by default, or another manifest's address (a mirror). It is a stored setting, in a module's own slot beside the channel, built with ADR-010's poll; the page sets both. A bench host is not set here: the poll asks `opentrickler-update.local` first, as the Open Weapon Mount Computer's units ask its `ota_host.py` (ADR-010, gate 6, step 9), and an offer from it says so, which the page shows too (`found.bench`);
  - **the offer:** the unit's poll found a newer release; the page shows it with its note. A critical one is marked as such, with no pop-up;
  - on a yes: the unit fetches the file `takes` names into the free slot as a job, then installs;
  - **the upload by hand:** the file chosen on the phone or the PC, sent with `PUT /api/v1/update/image` as `rc_run.py update` sends it. The page names the file this unit takes, as the manifest's entry for `takes` names it (its name ends in `<takes>_update.bin`: the Pico W's differs by its free slot). A wrong file is refused by the unit with its code, and the page says why in plain words;
  - say "keep the power on" and show how far it has got (`received` against the file's size);
  - follow the unit through its restart, which takes the page's connection with it. Wait for `boot` in `GET /api/v1` to change, then for `trial` to be false and `last` to be set, as `rc_run.py update` does;
  - say in plain words how it ended. `back` means the unit stayed on the version it had, nothing lost;
  - no downgrade: the page offers only a newer release (the owner, ADR-004).
- **Found out since:**
  - **TLS on the unit** was built and measured in ADR-010 (`https_client.c`, GitHub's two roots, redirects followed); its progress has what it costs on each board. A mirror on the owner's network can serve plain HTTP and needs no TLS.
  - On the unit's own access point there is no internet: no poll there (`POST /api/v1/update/check` answers 409 `no_network`), but the upload by hand works.
  - A page served over plain HTTP gets no WebCrypto (browsers give it only to HTTPS and localhost). The page need not check a file's SHA-256: the unit checks the image's own seal and refuses one that is not whole.

**Handed on by ADR-005** (2026-11-17, closed as a scoping ADR):
- **The feature table and the unit's report** of what it carries (ADR-005's rule 3) are built here, as their first reader: build options with a default per board, one generated header, one answer from the unit. The page shows only what is there.
- **The pages' size is this ADR's.** Our own pages take 102.7 KB of the Pico W image, unminified and uncompressed (18.6 KB gzipped; 2026-11-18). Tailwind and DaisyUI are not in the image; the browser fetches them from CDNs, so a page that works offline adds them, trimmed to what it uses.
- **What this ADR adds** (the pages) is measured on two builds, the shipping one and a size-optimised one (MinSizeRel), and decided on the second. If it fits only optimised, ADR-005's optimised build is made first. HTTPS for the unit's own fetch is measured in ADR-010.

**Handed on by ADR-010** (2026-11-17, from its menu's design, `ADR-010-lvgl/menu_design.md`, and the owner's answers to it):
- **A start from the page asks nothing about the cup.** `web_portal.html` starts a charge or a cleanup with no acknowledgement (read 2026-11-17). Before charge and before cleanup, the screen has the person acknowledge that the cup is on the scale. The owner: "Those are safety warnings, and not technical challenges. This ensures the users are acknowledged that they DID put the cup on the scale, rather than dropping powder to scale directly." The page's start asks the same. Whether the API carries the acknowledgement too, as a field in the start's body that a client must state, is settled here.
- **The simulated scale is not offered on the page,** as it is not on the screen (the owner: a debug feature). The schema still lists it among the scale's makes for the API and the console. The page's form leaves it out.
- **The update's channel and source:** the page shows and sets both. They are stored together, as ADR-010 builds them. Beta offers pre-releases; leaving beta downgrades nothing.
- **The feature table** takes the list of what ADR-010 trimmed from LVGL to fit the Pico W. Which board carries what is decided when a larger screen comes (the owner).
- **The mirror** follows the screen's view: its `format` is ADR-010's.
- **The settings pages are drawn from the schema, as the screen's are from the tables** (the owner, 2026-11-17, settling ADR-010's survey: "Table driven, but this needed to be built right, and consider the future web refactor to be done in the similar way"). Every setting is on both the screen and the web (ADR-010's `menu_design.md`, "Every setting on the screen and on the web"). What: the page's settings forms are made at run time from `GET /api/v1/schema`, a resource a form and a field a control by its type (boolean, choice, integer, number, string, colour), with its label, unit, limits, step, group and when it applies; no field is written into the page by hand, as today's `web_portal.html` does (`name="..."`). How: the schema is made from the same C tables (`settings.h`) the screen reads, so one description has two renderers. What ADR-010 adds to the tables for the screen (a value's decimals, a short label) goes into the schema too, and the page uses it (an input's step and its shown precision). The actions a settings page offers beside its fields (the gate's Open and Close, the scale's Zero) are listed with their resource in the schema, so both draw them from one list.

## Commissioning (folded in from ADR-015)

From a unit's first start, or its first start after a reset, to its first charge, as one designed process. ADR-015 held the owner's direction from 2026-11-17; it was folded in here on 2026-11-18.

### The hard part, and the route

- **The hard part** (the owner, 2026-11-18): joining the unit's access point, then going to an address in a browser. A person not at ease with a browser stops there.
- **The route** (the owner, 2026-11-18): the captive portal, done together with the main web portal. Read here as:
  - **No address typed.** A phone that joins the unit's access point opens the set-up by itself, in the sign-in sheet a phone shows for a network that asks to be signed in to.
  - **One code base.** The set-up is part of the portal, built from the same sources and styles, its forms drawn from the schema like every other settings form. Today's `wizard.html` as a page apart goes.

### What we know

Read in the code 2026-11-17 (ADR-015), and 2026-11-18 where marked:
- **The first start.** With no Wi-Fi network set, or none joined within its join timeout (30 s by default), the unit becomes its own access point (`wireless.c`, `access_point_mode.c`):
  - its name is `opentrickler-` and four characters of the unit's id;
  - its key is `opentrickler`, written in the code and the same on every unit;
  - its address is 192.168.4.1, with the unit's own DHCP and DNS servers.
- **The captive portal's parts are there** (2026-11-18), never tried with a phone:
  - the access point's DNS server answers every name it is asked with the unit's own address (`dnsserver.c`);
  - the web server sends any page asked for under another name to `http://<the unit's address>/` with a 302 (`http_server.c`, `slot_admit`); an API path under another name is refused with 403, against DNS rebinding;
  - on the access point, `/` serves `wizard.html` (`rest_endpoints.c`).
- **The wizard today** (2026-11-18): the network's name typed (no list of networks), its key, its security, the join timeout and Wi-Fi on or off, then "Complete & Reboot": it writes `/api/v1/settings/wifi`, saves it and restarts the unit. A wrong key is found only after the restart: the unit tries for its join timeout, comes back as its access point, and the person joins it again and starts over. Nothing else is set up.
- **When the station joins,** after a start as the access point, the access point is stopped (`wireless_rejoin`, ADR-004): a phone still on it loses it then.
- **Defaults.** Every other setting starts at its default: the scale is the A&D (`scale.c`), and the motors, the charge and the display are at their defaults. A person finds them on the web portal or on the screen, where every setting a unit needs is (ADR-010's parity rule). Only the Wi-Fi's network, key and security are too hard by the knob (ADR-010's `menu_design.md`).
- **What the screen shows** (ADR-010's Wi-Fi page): the access point's name and key, or the network joined, and the address, as text. No QR code: ADR-010 left it to commissioning.
- **Finding the unit on a network.** mDNS, as `opentrickler-<id>.local`, with the services `_http._tcp` and `_opentrickler._tcp` (`wireless.c`). The Open Weapon Mount Computer finds units by the second.
- **A factory reset** (`POST /api/v1/system/erase`, or the menu's Erase) erases the stored settings. The unit then comes back as its own access point, with a new id and so a new host name.
- **The unit's id** comes from `rnd()` (`eeprom.c`), which gives ids with few random bits: the bench's is `9CE00000`, and the host name shows four characters of it. Two units can end up with the same name (found in ADR-003).

### ESP's SoftAP provisioning app, weighed and not taken (2026-11-18)

The Open Weapon Mount Computer is set up this way, and the owner asked whether this unit could be too.
- **How the OWMC does it:** Espressif's `network_provisioning` (1.4.0) over its access point, security 1 with a proof of possession of random digits (`main/services/wifi_provision.c`); its Wi-Fi page shows a QR code for the app with the access point's name and that proof (`main/ui/settings/wifi_page.c`); its access point is open.
- **What the protocol asks of a unit** (read in the component and in Espressif's `esp_prov` tool): five POST routes at 192.168.4.1 (`proto-ver` in JSON; `prov-session`, `prov-scan`, `prov-config`, `prov-ctrl` in protobuf), a session held by a cookie, a Curve25519 key exchange and AES-CTR. The app scans networks through the unit, sends the network and its key, and reads back whether the unit joined, so a wrong key is told while the phone is still on the access point.
- **This unit could:** it already runs the access point and the station at once (ADR-004's rejoin), and its server takes binary bodies. It would need Curve25519 and AES-CTR switched on in mbedTLS, and a protobuf coder of its own. Not measured.
- **Why not:** it takes away both hard steps (the app joins the access point from its QR code, and there is no address), but a person must install an app first; it ends at the Wi-Fi; its code (protobuf, a session, two ciphers) serves nothing else in a Pico W with about 38 KB of flash left (978.1 of 1,016 KB, ADR-010); OWMC's access point is open, and on this unit that would give anyone in range the whole API, the motors included; and security 1 is off by default from ESP-IDF 6.1. The owner chose the captive portal with the main portal.

### Proposed, not settled

The agent's proposals, 2026-11-18, for the owner to settle with the mock:
- **What it covers:** the Wi-Fi; the scale, its make chosen and a live weight seen (which shows the scale talks); the motor drivers found; then a first charge offered. Each step can be skipped, and a unit that never finishes still works: offered, never forced, and no pop-up at every start. Tuning stays ADR-009's.
- **A list of networks** to choose from, scanned by the unit, in place of a name typed.
- **The access point kept up while the station joins,** and the result told on the page: a wrong key is typed again, with no restart and no new join of the access point. Today's rejoin stops the access point when the station joins; when it stops, and what the page says before it does, is part of this design.
- **A key for each unit's access point,** made from the chip's own unique id and shown only on its screen: being at the unit is then the password, as "no password for now" asks. Today's shared key goes.
- **The unit's id and name from the chip's unique id** (`pico_get_unique_board_id`) in place of `rnd()`: the same across a factory reset, and never two units with one name. Every unit already out gets a new host name once, with the update that brings it.
- **A QR code to join the access point,** the phones' own Wi-Fi format (`WIFI:T:WPA;S:<name>;P:<key>;;`, about 45 characters): QR version 3, 29 modules, 58 of the panel's 64 rows at 2 pixels a module. A phone's camera joins the network from it with no app. On the screen's Wi-Fi page. The owner's QR question (2026-11-17) is answered here.

### To find out first

On real phones, at least one iPhone and one Android, before the design is settled. What is listed here is what phones are known to do, not yet seen with this unit:
- whether joining the access point opens the sign-in sheet by itself, or only a notice to tap;
- whether the set-up works inside that sheet, a reduced browser on iOS that closes when the network changes;
- whether a phone leaves the access point, or sends its traffic over mobile data, because it has no internet;
- what the unit must answer to each phone's check (the address each phone asks to learn whether it is signed in).

## Ideas

- **A mock first.** The pages, clickable in a browser against the stand-in server, before anything goes on a unit. The owner decides by using it. An agent can open the same mock in its own browser and check it.
- **Phone first.** Most use is a phone held beside the trickler.
- **Pages:** charge (target, live weight, start and stop), profiles, tuning with a chart of the throw (ADR-009), settings, update (ADR-004), and the set-up (commissioning, above), part of the same portal.
- **Built from sources.** Separate files for markup, styles and script, joined and gzipped by a build step into what the unit serves. Whether that step may need Node, or stays in Python like `html2header.py`, is the owner's call.
- **No framework at run time, or a very small one.** The page is a handful of forms and a chart. A proposal with sizes comes with the mock.
- **Charts** by a small library or drawn by hand on a canvas; chosen by size.
- **A budget:** the whole UI under about 100 KB gzipped (a proposal; today's pages are 102.7 KB uncompressed, 18.6 KB gzipped, and the Pico W's slot has 38 KB left).
- **Settings forms drawn from the settings' own description:** ADR-003 brought it (the schema). Settled by the owner, 2026-11-17: see "Handed on by ADR-010" above.
- **One language.** English.
- **The screen mirror** stays as a small page, fed by whatever the screen is then.

## How the unit shows it works

- With the internet unplugged, every page works from a phone on the unit's access point.
- A person not at ease with a browser commissions a unit just reset, from its first start, with no help: on an iPhone and on an Android phone that have never joined it, joining its access point opens the set-up, and nobody types an address.
- Screenshots of each page from the unit match the mock.
- On a board without an optional module, its page and its links are absent.
- A charge started and watched from the phone, with `charge_cycle`'s checks.
- The same on the Pico 2 W and the ESP32-S3-Pico.
- The size report shows the pages inside the budget.

## Not in this ADR

- A phone app.
- A password (see ADR-003).
- Languages other than English.

## To settle before approval

- The look, from the mock.
- Whether the build may need Node.
- The budget.
- Commissioning: what it covers, and each proposal above (the list of networks, the access point kept up while the station joins, the key and the id from the chip, the QR code), after the phones' captive portals are seen.
