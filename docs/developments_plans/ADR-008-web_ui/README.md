# ADR-008: a web UI that needs no internet

Status: **Draft** 2026-11-13. Not approved. Captures what is wrong with the pages today and ideas for the new ones; the look is shown to the owner as a mock before anything is built.

## What we want

- The whole web UI comes from the unit. It works from a phone joined to the unit's own access point, with no internet.
- It shows what this board carries and nothing else.
- It is the place for what the small screen cannot show: charts, profiles side by side, an update.

## Owner's direction (2026-11-13)

- Implement a better web UI.
- One implementation: the same pages on every board.

## Why here

Second, after ADR-010 (the owner, 2026-11-17: the current Pico platform first, in the order 010, 008, 009, 014, then 006 and 007 together). The screen comes first, so the unit's poll and the update's job are built in ADR-010, and this ADR's page shows the same offer the screen does. It stands on ADR-003's server, on that poll, and on ADR-005's report of what a board carries. It is proven on the Pico W and the Pico 2 W; ADR-007's board takes the same pages later.

## What we know (checked 2026-11-13)

- `src/html/web_portal.html` is one page of 1,787 lines: markup, styles and script together.
- It loads Tailwind and DaisyUI from CDNs and its icon from GitHub. With no internet the page has no styling.
- `wizard.html` (252 lines) is self-contained. It is what `/` serves while the unit is its own access point.
- `display_mirror.html` draws the screen from the raw u8g2 buffer at `/display_buffer`. `plot_weight.html` is in the folder and is not served.
- Pages become C strings through `scripts/html2header.py`, unminified and uncompressed: 90 KB of the image.
- The page polls with `fetch`; nothing is pushed.
- `tests/web_test.py` is a small stand-in server (Flask) that serves the pages and answers a few endpoints with made-up values.
- PR #113 adds a second, separate page (928 lines) for powder analysis; PR #111 adds an update page inside the portal; PR #101 adds about 950 lines to the portal.

**Handed on by ADR-004** (2026-11-16): the update's offer on the web page. ADR-004 built what it rests on and left the offer to this ADR and ADR-010. Until one of them ships it, an update is reached through the REST API only, for debugging, and is not for users (the owner: "not production ready until the UI is released"). `rc_run.py update` plays the offer's part on the bench.
- **What is there** (built and shown on the Pico 2 W; the Pico W built, not yet run):
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
- **What follows, and is recommended here:** the unit polls the source, not the browser. The screen needs the unit to poll anyway, so the web page shows the same offer the screen does. That is one implementation, not two, and the page then needs no access to GitHub of its own. The poll, the download into the free slot and the stored setting for the source are firmware. They move to ADR-010, now built first (2026-11-17), with the update's channel, stable or beta (ADR-010's `menu_design.md`). This ADR builds the page's offer, the page where the source and the channel are set, and the upload by hand.
- **What the page has to do,** so a person needs to know none of it (the owner's "too prosumer" list: where firmware comes from, which file is theirs, that the power must stay on, the wrong board):
  - **the source:** GitHub's latest release by default, or another manifest's address (a mirror on the owner's network, as the Open Weapon Mount Computer's bench host `ota_host.py` is). It is a stored setting, in a module's own slot beside the channel, built with ADR-010's poll; the page sets both;
  - **the offer:** the unit's poll found a newer release; the page shows it with its note. A critical one is marked as such, with no pop-up;
  - on a yes: the unit fetches the file `takes` names into the free slot as a job, then installs;
  - **the upload by hand:** the file chosen on the phone or the PC, sent with `PUT /api/v1/update/image` as `rc_run.py update` sends it. The page names the file this unit takes, as the manifest's entry for `takes` names it (its name ends in `<takes>_update.bin`: the Pico W's differs by its free slot). A wrong file is refused by the unit with its code, and the page says why in plain words;
  - say "keep the power on" and show how far it has got (`received` against the file's size);
  - follow the unit through its restart, which takes the page's connection with it. Wait for `boot` in `GET /api/v1` to change, then for `trial` to be false and `last` to be set, as `rc_run.py update` does;
  - say in plain words how it ended. `back` means the unit stayed on the version it had, nothing lost;
  - no downgrade: the page offers only a newer release (the owner, ADR-004).
- **To find out first:**
  - **TLS on the unit,** to fetch from GitHub over HTTPS. A release's files are at a GitHub address that redirects to another host, so the unit's client must follow a redirect. What TLS costs in flash and RAM on each board is measured in ADR-010's first gate, beside LVGL, now that the poll is built there; the Pico W's slot has 216.7 KB left at 1.20.218. A mirror on the owner's network can serve plain HTTP and needs no TLS.
  - On the unit's own access point there is no internet: no poll there, but the upload by hand works.
  - A page served over plain HTTP gets no WebCrypto (browsers give it only to HTTPS and localhost). The page need not check a file's SHA-256: the unit checks the image's own seal and refuses one that is not whole.

**Handed on by ADR-005** (2026-11-17, closed as a scoping ADR):
- **The feature table and the unit's report** of what it carries (ADR-005's rule 3) are built here, as their first reader: build options with a default per board, one generated header, one answer from the unit. The page shows only what is there.
- **The pages' size is this ADR's.** Today's 101 KB in the Pico W image are our own pages, unminified and uncompressed (19 KB gzipped). Tailwind and DaisyUI are not in the image; the browser fetches them from CDNs, so a page that works offline adds them, trimmed to what it uses.
- **What this ADR adds** (the pages) is measured on two builds, the shipping one and a size-optimised one (MinSizeRel), and decided on the second. If it fits only optimised, ADR-005's optimised build is made first. HTTPS for the unit's own fetch is measured in ADR-010.

**Handed on by ADR-010** (2026-11-17, from its menu's design, `ADR-010-lvgl/menu_design.md`, and the owner's answers to it):
- **A start from the page asks nothing about the cup.** `web_portal.html` starts a charge or a cleanup with no acknowledgement (read 2026-11-17). Before charge and before cleanup, the screen has the person acknowledge that the cup is on the scale. The owner: "Those are safety warnings, and not technical challenges. This ensures the users are acknowledged that they DID put the cup on the scale, rather than dropping powder to scale directly." The page's start asks the same. Whether the API carries the acknowledgement too, as a field in the start's body that a client must state, is settled here.
- **The simulated scale is not offered on the page,** as it is not on the screen (the owner: a debug feature). The schema still lists it among the scale's makes for the API and the console. The page's form leaves it out.
- **The update's channel and source:** the page shows and sets both. They are stored together, as ADR-010 builds them. Beta offers pre-releases; leaving beta downgrades nothing.
- **The feature table** takes the list of what ADR-010 trimmed from LVGL to fit the Pico W. Which board carries what is decided when a larger screen comes (the owner).
- **The mirror** follows the screen's view: its `format` is ADR-010's.
- **The settings pages are drawn from the schema, as the screen's are from the tables** (the owner, 2026-11-17, settling ADR-010's survey: "Table driven, but this needed to be built right, and consider the future web refactor to be done in the similar way"). Every setting is on both the screen and the web (ADR-010's `menu_design.md`, "Every setting on the screen and on the web"). What: the page's settings forms are made at run time from `GET /api/v1/schema`, a resource a form and a field a control by its type (boolean, choice, integer, number, string, colour), with its label, unit, limits, step, group and when it applies; no field is written into the page by hand, as today's `web_portal.html` does (`name="..."`). How: the schema is made from the same C tables (`settings.h`) the screen reads, so one description has two renderers. What ADR-010 adds to the tables for the screen (a value's decimals, a short label) goes into the schema too, and the page uses it (an input's step and its shown precision). The actions a settings page offers beside its fields (the gate's Open and Close, the scale's Zero) are listed with their resource in the schema, so both draw them from one list.

## Ideas

- **A mock first.** The pages, clickable in a browser against the stand-in server, before anything goes on a unit. The owner decides by using it. An agent can open the same mock in its own browser and check it.
- **Phone first.** Most use is a phone held beside the trickler.
- **Pages:** charge (target, live weight, start and stop), profiles, tuning with a chart of the throw (ADR-009), settings, update (ADR-004), and the set-up wizard.
- **Built from sources.** Separate files for markup, styles and script, joined and gzipped by a build step into what the unit serves. Whether that step may need Node, or stays in Python like `html2header.py`, is the owner's call.
- **No framework at run time, or a very small one.** The page is a handful of forms and a chart. A proposal with sizes comes with the mock.
- **Charts** by a small library or drawn by hand on a canvas; chosen by size.
- **A budget:** the whole UI under about 100 KB gzipped (a proposal; today's pages are 90 KB uncompressed).
- **Settings forms drawn from the settings' own description:** ADR-003 brought it (the schema). Settled by the owner, 2026-11-17: see "Handed on by ADR-010" above.
- **One language.** English.
- **The screen mirror** stays as a small page, fed by whatever the screen is then.

## How the unit shows it works

- With the internet unplugged, every page works from a phone on the unit's access point.
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
