# ADR-008: a web UI that needs no internet

Status: **Built** 2026-11-19, closed on its branch before its one PR, #134: every gate built and proven on the Pico W and the Pico 2 W (progress.md, "Closed"). PR #111 is closed with its note when #134 is merged. Was **Open** from 2026-11-18. Gate 1 (the mock) settled by the owner 2026-11-19; gate 2 (the API review) approved 2026-11-19 and built and proven the same day; gate 3 (the pages) started overnight on the owner's word, its plan for the owner's review; gate 4 (commissioning) approved 2026-11-19 with the iPhone moved to before its step 4 ("Approve, start step 1"). The whole ADR goes by one PR, #134 (the owner, 2026-11-19: "I'd merge ADR-008 as one PR, as more will change when you are implementing the actual code to the device"). Drafted 2026-11-13, revised 2026-11-18 (ADR-015, commissioning, folded in; the requirements aligned with the owner), approved by the owner the same day: "Approve, but don't start yet." Revised again that day with the agent's review of the plan against the code, and started at the owner's word, the Pico W given for gate 1's probes ("Pico W is connected so you can use the real device").

## What we want

- The whole web UI for the basics comes from the unit: the charge, the purge, the powders, every setting, the update, the set-up and the mirror. They work from a phone joined to the unit's own access point, with no internet. An advanced page (ADR-009's tuning, say) may load a library from the internet (the owner, 2026-11-18; "Resources from the internet", below).
- It shows what this board carries and nothing else.
- It is the place for what the small screen cannot show: profiles side by side, an update, and later the charts of a throw (ADR-009).
- Every client echoes the unit's state, and an agent finds and learns every call from the unit itself.
- A new unit, or one just reset, is taken from its first start to its first charge by one designed process, not by steps found here and there (commissioning, below).

## Owner's direction

- 2026-11-13: implement a better web UI.
- 2026-11-13: one implementation: the same pages on every board.
- 2026-11-17, on the QR code proposed in ADR-010's menu design: "I'm not sure about the QR code. Perhaps that's a feature for future ADR. I'd like to design the commissioning process properly, not ad-hoc." It became ADR-015.
- 2026-11-18: ADR-015 "may be folded into ADR-008 and sharing the same code base and capture requirements together".
- 2026-11-18, on what is hard: a person "needs to 1. Join the soft AP. 2. Go to a specific URL. which is the hardest part for the average user if they are not proficient with browser."
- 2026-11-18, deciding: "I'd take the existing captive portal route and do it together with the main web portal." ESP's SoftAP provisioning app was weighed and not taken (see "Commissioning").
- 2026-11-18, aligning the requirements before approval, on the agent's points:
  - the budget is set from the slot, not a round number; and "Currently I don't think the web is done in the efficient way, so I'd take your advice and start with mockup first";
  - Tailwind and DaisyUI stay, trimmed: "Trim is needed. You can install whichever the dev tool needed";
  - "Reserve the actual tuning with powder to ADR-009. No functional change at the powder throw level";
  - the cup is asked on the page and the API stays as it is; and "The API also needs a review, for example, how does the agent knows the existance of /hello";
  - on how the unit tells the page what changed: "How about alternative? Is there a better solution for backend to frontend comm?", then "I'd explore the SSE, and the life cycle of SSE must be managed";
  - with the screen and the web at once: "I'd prefer echoing the state, not taking over one from another";
  - for agents: a declaration on the root GET of every call an agent may make, not rendered on the page, "And each API should have enough information to self declare the usage? For example, a GET to hello should declare the usage instead of an error".
- 2026-11-18, after approval, on the agent's review of the plan against the code (its gaps, fixes and ideas, each written in below):
  - the feature table in the API review's gate: "Agree, and I need to make sure no feature is silently dropped";
  - the tools: "Install any development tool you need. I don't have strong preference of which framework you should use. I happened to use tailwind and daisy UI because that was the previous agent recommended, but if you see a better fit, or a better UI design for the application then I'm happy to review your proposal";
  - the radio probed in gate 1: "Pico W is connected so you can use the real device. This computer has WiFi module that I'm not using so you could use this workstation as the client to test";
  - "I'd take your fixes, please log in the ADR and relevant ADR";
  - the ideas (DHCP's captive-portal option, the pages cached by version, the phone kept awake): "Yes I'd like your ideas. However the last one may needs its own toggle switch and default to ON".
- 2026-11-18, after gate 1's radio probe (progress.md): "I'm not around the Pico W so let's defer the iPhone test to later stage. And I don't have Android phone so let's assume they are working the same way." Read here as: the iPhone is seen at the unit before commissioning's design is settled, at the start of gate 4 at the latest; Android is not tested and is taken to work as the iPhone and the Windows client do (below, "To find out first").
- 2026-11-18, on the System Control's Erase found starting at Yes (progress.md, the inventory), after first saying to fix it at once: "Actually I'd like to leave it to gate 3, as no release will be done mid ADR, so there is no point to fix anything that may subject to be thrown away, Just record what you found." Read here as: today's pages are not fixed while this ADR is open; what is found wrong in them is recorded in `features.md` and made right by the new pages in gate 3, which is checked against it.
- 2026-11-18, after a brief read of `features.md`: "There are a lot of information and I may miss one or two. Therefore my take is
  1. Record every feature supported today.
  2. For each feature, make your proposal.
  3. I'm open to recommendations of reconstruct, or reorganize the UI/UX, as I'm not confident about the current design.
  4. Trickling related features should be preserved in a way that follows "good UI design" principle.
  5. Settings MUST be preserved, but the way is presented to the user or interaction can be changed.
  6. The Web shall use the same terms compare to the display, for example, cleanup vs purge. I prefer to use purge everywhere."
  
  And: "I want you to use the web mockup to demonstrate your proposal. The later mockup should be used to create the final web interface design. Perhaps the same page but with mocked backend is preferred. Self contained is the goal for basic feature (like the trickling and configuration). Some resources MAY be downloaded from the interface for advanced features, like auto tuning etc. I'm open to discussion on this. For example I don't expect you to serve plotly from the pico."
- 2026-11-18, on the agent's answer that the API keep `cleanup` as v1 is stable: "No software released since v1 is proposed so this is a good chance to change everything for those doesn't fit." Read here as: every release since API v1 is a candidate, a pre-release, so v1 is frozen only from the first release for users. Until then a name in the API, the events or the console that does not fit the words a person reads is changed to fit, `cleanup` to `purge` first ("The words", below). ADR-003, which made v1, is told.
- 2026-11-18, on the charge's page drawn to the skill `web-ui-design`: "Perhaps the first tab should be called Trickler (or dispenser)? As both charge and purge are on the same pages now. Also, if possible, I don't want the page to scroll on smaller screen devices. We may be able to zoom in the elements if the size doesn't fit, but scrolling a control page seems inconvinent. That was my reason to separate the charge and purge onto two pages." Read here as: the first tab is the Trickler, a switch at its top between Charge and Purge, each a view that fits the screen with no scrolling, its elements scaled to the room there is (the skill's rule 14); lists (powders, settings) may scroll. "Trickler" names the page; the motors are "Motors" ("The words").

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
- **The flash left:** the Pico W's image is 983.4 KB of its 1,016 KB slot, 32.6 KB left; its RAM at link 253.0 KB of 256 (`size_report.py`, main at 616131f, 2026-11-18; ADR-010's last figures were 978.1 KB and 252.5 KB). The pages take 101.0 KB of it. A static table of 1.3 KB more RAM no longer links (progress.md, the probe's scan). What the pages add or take away is measured against these.
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
- **PR #111 stays open until this ADR's offer works** (the owner, 2026-11-17): the whole OTA flow shown end to end, from the offer on the page to the unit confirmed on the new version. Then it is closed with its note, on the owner's word. Shown on the Pico W 2026-11-19 (progress.md, "Gate 4's proof on the Pico W"); the owner then: "I want ADR-008 to be merged in one PR". So #111 is closed with its note when #134 is merged, not before.
- **What follows, and is recommended here:** the unit polls the source, not the browser. The screen needs the unit to poll anyway, so the web page shows the same offer the screen does. That is one implementation, not two, and the page then needs no access to GitHub of its own. The poll, the download into the free slot and the stored setting for the source are firmware. They moved to ADR-010 (2026-11-17), with the update's channel, stable or beta (ADR-010's `menu_design.md`), and were built there (gate 5): the stored `settings/update` (its channel and source), `POST /api/v1/update/check`, and `poll` and `found` in `GET /api/v1/update`, a release found also an event (api_v1.md). This ADR builds the page's offer on them, the page where the source and the channel are set, and the upload by hand.
- **What the page has to do,** so a person needs to know none of it (the owner's "too prosumer" list: where firmware comes from, which file is theirs, that the power must stay on, the wrong board):
  - **the source:** GitHub's latest release by default, or another manifest's address (a mirror). It is a stored setting, in a module's own slot beside the channel, built with ADR-010's poll; the page sets both. A bench host is not set here: the poll asks `opentrickler-update.local` first, as the Open Weapon Mount Computer's units ask its `ota_host.py` (ADR-010, gate 6, step 9), and an offer from it says so, which the page shows too (`found.bench`);
  - **the offer:** the unit's poll found a newer release; the page shows it with its note, in the Unit page's first card, Update. A critical one is marked as such, with no pop-up. From every page, a dot on the Unit's destination says an offer waits (red for a critical one), as the screen's main menu has its row (gate 1's mock, 2026-11-18);
  - on a yes: the unit fetches the file `takes` names into the free slot as a job, then installs;
  - **the upload by hand:** the file chosen on the phone or the PC, sent with `PUT /api/v1/update/image` as `rc_run.py update` sends it. The page names the file this unit takes, as the manifest's entry for `takes` names it (its name ends in `<takes>_update.bin`: the Pico W's differs by its free slot). A wrong file is refused by the unit with its code, and the page says why in plain words;
  - say "keep the power on" and show how far it has got (`received` against the file's size);
  - follow the unit through its restart, which takes the page's connection with it. Wait for `boot` in `GET /api/v1` to change, then for `trial` to be false and `last` to be set, as `rc_run.py update` does;
  - say in plain words how it ended. `back` means the unit stayed on the version it had; "nothing lost" holds only once a trial writes nothing a going back needs ("The update's life cycle, read whole", gap 1);
  - no downgrade: the page offers only a newer release (the owner, ADR-004).
- **Found out since:**
  - **TLS on the unit** was built and measured in ADR-010 (`https_client.c`, GitHub's two roots, redirects followed); its progress has what it costs on each board. A mirror on the owner's network can serve plain HTTP and needs no TLS.
  - On the unit's own access point there is no internet: no poll there (`POST /api/v1/update/check` answers 409 `no_network`), but the upload by hand works.
  - A page served over plain HTTP gets no WebCrypto (browsers give it only to HTTPS and localhost). The page need not check a file's SHA-256: the unit checks the image's own seal and refuses one that is not whole.

**The update's life cycle, read whole** (gate 1, 2026-11-18; the owner: "I'd like to challenge you the life cycle of the firmware upgrade including its failure modes, Also, does the firmware have the capability of reporting its upgrade status during upgrade?"). Read in the code (`update.c`, `update_slots_rp2040.c`, `update_slots_rp2350.c`, `bootloader/bootloader.c`, `wireless.c`, `common.c`, `https_client.c`), not tried on a unit.
- **What the unit reports:** during the job, all of it: `GET /api/v1/update`'s `state`, `received` against `expected` (the manifest's size for a download), `download`, and the events started, received, installing and stopped with a reason and a code. During the restart, nothing: about 8 s off the network. After it, `boot` is new, `trial` is true until the image confirms itself, and `last` says confirmed or back with the version. `last` and the event log are in RAM: a later restart forgets how the update ended.
- **Each stage and how it fails:**

| Stage | Fails by | The unit says | |
|---|---|---|---|
| The offer (the poll at each start, or Check now) | no network, the poll's error | `poll.state` and `error`; 409 `no_network` on the hotspot | sound |
| The start | another job, a view open at the unit, a poll, the image on trial | 409 with why | sound |
| The download | DNS, TLS, the HTTP status, a length not the manifest's, a read stalled 15 s, the Wi-Fi gone, Stop | the job ends: `last` refused with its code and a line, or stopped | sound |
| The check (the manifest's SHA-256, the board, the slot, the seal) | another image | `last` refused with its code | sound; `state` stays receiving at 100% while it checks |
| The install and its restart | the restart refused | stopped, `failed` | sound |
| A power cut while downloading, or between the install and the restart | a free slot half written, or the trial's request lost | the old image runs; `last` empty | safe, not said |
| The trial (the watchdog at 8 s, fed for 60 s at most) | a crash or a hang | the old image about 75 s on, `last` back | sound |
| The trial | not on its network within 60 s | the same back | why not said (gap 4) |
| A power cut during the trial | the note is in scratch registers, which a power cut clears | the old image; `last` empty | the unit does not know it tried (gap 2) |
| Confirmed | a stored layout that changed | that module's settings reset, silently | not said (gap 1) |

- **The gaps, the worst first.** Each is built in gate 2 (the API review, item 9), where the update's page needs it; ADR-004, whose area this is, is closed.
  1. **A trial writes into the EEPROM before it is confirmed.** On a stored struct whose layout changed, `load_config` writes that module's defaults at the trial's start (a CRC that does not match, `common.c`). If the trial goes back, the old image cannot read the record either and writes its own defaults: the settings are lost both ways. If the layout changed is the Wi-Fi's, the trial starts with Wi-Fi off, takes its own hotspot as its network (`wireless_server_listening`) and confirms: the unit is updated and gone from the person's network. The invariant broken: a trial changes nothing the image it may go back to relies on. What: on trial, defaults are kept in RAM and written only at the confirmation; a trial whose Wi-Fi record did not read never confirms; once confirmed, the unit reports which modules' settings the update reset (in `last`, and an event), and the page says so. Proof: a test image with one struct's size changed (an `RC_TEST_` build option, as ADR-004's test images) tried and left, the stored settings copied before and after (`rc_run.py settings`) and unchanged; tried and kept, the module named as reset. No release has needed it yet (ADR-010 changed no stored layout, only added a slot); it is built before the first release that changes a stored layout, whatever the gate.
  2. **A power cut during the trial is not seen.** The note of what was tried is in the watchdog's (RP2040) or the power manager's (RP2350) scratch registers, which a power cut clears. What: the note written into the EEPROM before the install, beside the update's settings, and cleared at the confirmation; a start that finds it and is not running its version is `back`, with the power cut as its reason. Proof: `rc_run.py update --expect power-trial`, which exists, then expects `last` back.
  3. **How the last update ended is forgotten at the next restart.** What: `last` kept in the same EEPROM record as gap 2's note, until the next update.
  4. **A back does not say why.** A crash, no network in 60 s and a confirmation that failed are one `back`; the screen says "did not start last time", wrong when only the router was away. What: the trial writes how far it got into the note as it goes (started, its network joined, confirming), which a watchdog restart keeps; `last` gains `reason`: did_not_start, no_network, not_confirmed, power_cut. The screen's line follows it.
  5. **The trial's 60 s is fixed; the join's timeout may be set up to 120 s** (`settings/wifi`'s `timeout_ms`). A slow join leaves a good image. What: the trial fed for the join's timeout and a margin, the RP2040's watchdog still fed each second.
  - Smaller: no stage for the download's check (`state` receiving at 100% while the SHA-256 and the seal run): a `checking` state; and nothing says how long the restart's silence may last (about 10 s, about 75 s for a back): the install's reply and `GET /api/v1/update` while installing give `back_within_s`, the trial's limit and a start, so the page waits that long before it says the unit is not back.
- **What the page then shows:** every stage, the restart included: "Restarting on 2.1.0", "Starting 2.1.0" while on trial, then how it ended in words, with why for a back and the settings an update reset. Until gaps 2 and 4 are built, a page that finds the old version and `last` empty says only "2.1.0 was not kept", from what it asked itself.

**Handed on by ADR-005** (2026-11-17, closed as a scoping ADR):
- **The feature table and the unit's report** of what it carries (ADR-005's rule 3) are built here, as their first reader: build options with a default per board, one generated header, one answer from the unit. The page shows only what is there.
- **The pages' size is this ADR's.** Our own pages take 102.7 KB of the Pico W image, unminified and uncompressed (18.6 KB gzipped; 2026-11-18). Tailwind and DaisyUI are not in the image; the browser fetches them from CDNs, so a page that works offline adds them, trimmed to what it uses.
- **What this ADR adds** (the pages) is measured on two builds, the shipping one and a size-optimised one (MinSizeRel), and decided on the second. If it fits only optimised, ADR-005's optimised build is made first. HTTPS for the unit's own fetch is measured in ADR-010.
- **Read 2026-11-18:** the build has no feature options today, only test ones (`HTTPS_TEST_CA`, `RC_TEST_*` in `CMakeLists.txt`): both boards carry the same set, and `app.c` starts every module, the servo gate and the LEDs included. What a unit has fitted is another question: the bench's Pico W has no servo gate, and the firmware cannot tell. A module that can be left out of a build is this ADR's; telling what is fitted is not (ADR-006's, if ever).
- **No feature silently dropped** (the owner, 2026-11-18). Read here as two rules:
  - **On the unit:** a feature left out of a build is reported by name as absent in the unit's report, never just missing; the build prints the board's list as it configures. The page shows what the report lists, and a page's part with no feature behind it, or a feature with no part, fails the test.
  - **On the pages:** every feature today's portal and wizard offer is listed in an inventory in gate 1 (`features.md` in this folder, as ADR-010's `menu_design.md` placed every row of the old menu), and each is placed in the new pages or left out by the owner's decision, with its reason. Nothing goes by a page rewritten.
- **When:** the table, the build options and the unit's report are built in gate 2, the API review, as the report is an addition to the index. Its first options are what a build can really leave out today: the servo gate and the LEDs. It is proven by one build with an option off: the report names it absent and the page drops its part.

**Handed on by ADR-010** (2026-11-17, from its menu's design, `ADR-010-lvgl/menu_design.md`, and the owner's answers to it):
- **A start from the page asks nothing about the cup.** `web_portal.html` starts a charge or a cleanup with no acknowledgement (read 2026-11-17). Before charge and before cleanup, the screen has the person acknowledge that the cup is on the scale. The owner: "Those are safety warnings, and not technical challenges. This ensures the users are acknowledged that they DID put the cup on the scale, rather than dropping powder to scale directly." The page's start asks the same. Whether the API carries the acknowledgement too, as a field in the start's body that a client must state, is settled here. Settled 2026-11-18: the page asks, and the API stays as it is. API v1 is stable, and a field every start must carry would break the clients it has (`rc_run.py`, and the Open Weapon Mount Computer when it moves to v1); the index's guide already tells an agent to start and zero only when a person has said an empty cup is on the pan.
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
  - its key is `opentrickler`, written in the code and the same on every unit (until gate 4, step 1: each unit's own, made from its Pico, shown on its screen);
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
- **A factory reset** (`POST /api/v1/system/erase`, or the menu's Erase) erases the stored settings. The unit then comes back as its own access point, with a new id and so a new host name (until gate 4, step 1: the id is the Pico's, and a reset leaves it).
- **The unit's id** came from `rnd()` (`eeprom.c`) until gate 4, step 1 read it from the Pico (`unit_id.c`), stored nowhere (progress.md has its life cycle). `rnd()` gave ids with few random bits: the bench's is `9CE00000`, and the host name shows four characters of it. Two units can end up with the same name (found in ADR-003).

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
  - **The key is only as secret as what it is made from** (the agent's review, 2026-11-18). If the host name and the key both come from the unique id, the host name, announced by mDNS to the whole network, gives away part of what makes the key. So the key is a hash of the id (SHA-256, already in the image) of which no part is sent anywhere: the host name and the access point's name come from other bits of the id, and the API gives neither the id nor the key (the Wi-Fi key is already never shown). Anyone who reads the chip by its debug port can make the key; being at the unit is the password either way.
- **The unit's id and name from the chip's unique id** (`pico_get_unique_board_id`) in place of `rnd()`: the same across a factory reset, and never two units with one name. Every unit already out gets a new host name once, with the update that brings it.
- **The set-up's address in DHCP** (the agent's idea, taken 2026-11-18): the unit's DHCP server gives the phone the set-up's address as the captive portal's (option 114, RFC 8910), so a phone that reads it opens the set-up without guessing from its own check. Newer iPhones and Android phones are said to read it (thought so, not checked); the others still have the answer to their check. A few lines in `dhcpserver.c`, tried on the phones in gate 1.
- **A QR code to join the access point,** the phones' own Wi-Fi format (`WIFI:T:WPA;S:<name>;P:<key>;;`, about 45 characters): QR version 3, 29 modules, 58 of the panel's 64 rows at 2 pixels a module. A phone's camera joins the network from it with no app. On the screen's Wi-Fi page. The owner's QR question (2026-11-17) is answered here.

### The set-up as drawn (gate 1's mock, 2026-11-18)

The owner: "Keep commissioning in the ADR, Set up on the page. Then continue with the design." Drawn in the mock (`src/web/setup.js`), for the owner to settle by using it; built in gate 4, after the iPhone is seen.
- **When it opens:** by itself only on a unit never set up, on its own hotspot, when the page is opened at its root (a phone's sign-in sheet opens it so). Finishing or skipping is remembered by the unit, so it never opens by itself again; a factory reset forgets it. From the Unit page, "Set up this unit" opens it any time. No pop-up at any other start.
- **Its steps,** "Set up this unit", "1 of 4", one column of a phone's width with no navigation, each step skippable:
  1. **Wi-Fi:** the networks the unit hears, strongest first, each with its signal in words and a lock where it needs a key; "Another network" to type one. Chosen, its key under it, the cursor in it. The join is tried while the hotspot stays up, and its end said on the page: "Joined Smith home. Its address there: opentrickler-9CE0.local (192.168.1.57)", or "Not joined: the key is wrong. Try again", the key ready to type again; nothing restarts. "Skip: use its own hotspot".
  2. **Scale:** its make, and its weight now, read live: a weight that moves shows the scale talks. Nothing read for about 3 s: "Nothing from the scale yet. Check its cable, and that the scale sends at the speed set here", with the speed. Each choice stored at once: a set-up has no "not stored".
  3. **Motors:** each driver found or not, in words ("Driver not found: check its cable and the board's power"); nothing to set.
  4. **Ready:** what was set (Wi-Fi, scale, motors). Joined to a network: "Finish: go to your Wi-Fi": the unit leaves its hotspot, and the page says where to find it there (`http://opentrickler-9CE0.local`, or its address) and to try a first charge with the empty cup on. On its own hotspot: "Start using it", to the Trickler.
- **Its API, proposed** (played by the stand-in, `propose_setup` in `tests/web_test.py`, which refuses to start once the unit has it; built in gate 4):
  - `GET /api/v1/wifi/networks`: `scanning`, and `networks` (each `ssid`, `rssi`, `auth`), merged from the scans by name, strongest first (gate 1's probe: a scan hears part of the list). `POST /api/v1/wifi/scan` asks a new scan. The table from the heap (the probe's static one did not link).
  - `POST /api/v1/wifi/join` (`ssid`, `key`, `auth`): the station joins while the hotspot stays up; `GET /api/v1/network` gains `station` (`state`: idle, joining, joined, failed; `ssid`, `address`, `reason`: wrong_key, not_found, timeout). Joined, the Wi-Fi is stored; refused, nothing is.
  - `set_up` in `GET /api/v1/system`: stored (a field in the Wi-Fi's slot or its own; a stored layout, said here as the rule asks), false after a factory reset. `POST /api/v1/setup/done` sets it; joined, the hotspot then stops. The hotspot's life: up from a start with no network joined, kept while the set-up runs, stopped at its end once joined, or at the next start.
- **On the screen,** drawn with the screen's skill (`tools/ui_mock/setup_12864.py`, its sheet `mockups/setup_12864.png`), for the owner to settle by seeing:
  - **The main menu of a unit never set up** leads with "Set up this unit >", as an update found leads with its row: an offer, no pop-up; gone once the unit is set up. The title's mark is the hotspot's.
  - **The code that joins the hotspot,** opened by that row: the code at the left in a lit square of 64 x 64, its quiet zone 3 px; beside it "Scan this with your phone's camera." and Back, the screen's one softkey, at the right (the code takes the left, where Back stands elsewhere). A seventh kind of screen, allowed by the owner (2026-11-18: "Allow it"), and written into the screen's skill (rule 5, "The code").
  - **A phone joined** (the hotspot gave it an address while the code is shown): the same screen becomes "A phone joined. Go on with it there, or open 192.168.4.1", Back and "Show the code". The address is the way in when a phone opens no sign-in sheet.
  - **The Wi-Fi page on the hotspot:** Hotspot and its name, Key, IP; its action "Join by camera", the code again. Wi-Fi's off and on stays where a network is set.
  - **The key:** eight characters from an alphabet with no i, l, o, 0 or 1, which helvR08 draws alike or nearly (31 characters, about 40 bits; WPA2 takes 8 at the least), made from a SHA-256 of the chip's id of which nothing else is sent ("The key is only as secret as what it is made from", above). Typed as shown, all small letters.
  - **The code:** the phones' own Wi-Fi format, version 3 at level L, 29 modules a side at 2 px, dark modules on a lit ground as a phone reads a code (on the negative glass, the ground lit). A quiet zone of 1.5 modules where the standard asks 4: OpenCV's reader decoded the drawn screen, and the same inside a dark bezel (2026-11-18); a phone's camera at the unit decides, with the iPhone, before gate 4 builds it.
  - **In the firmware:** LVGL's own code widget (`LV_USE_QRCODE`, Nayuki's encoder, in `library/lvgl/src/libs/qrcode`), off today. Its flash is measured in gate 4 against the Pico W's 32.6 KB left.
- **Settled as drawn** (the owner, 2026-11-19, with the mock); changed at gate 4's start only where the iPhone differs.
- **Not known before the iPhone:** whether the sign-in sheet opens by itself, and whether the address link on the last step opens in the phone's browser once it is back on its Wi-Fi.

### To find out first

On a real iPhone, before commissioning's design is settled: deferred by the owner (2026-11-18) to when the owner is at the unit, and at the start of gate 4 at the latest, on the probe image (`-DRC_TEST_AP_PROBE=ON`). No Android phone is at hand: Android is taken to work as the iPhone does (the owner), and is not tested. The mock goes on meanwhile; the set-up's pages are drawn as the Windows client and today's code show, and changed if the iPhone differs. What is listed here is what phones are known to do, not yet seen with this unit:
- whether joining the access point opens the sign-in sheet by itself, or only a notice to tap;
- whether the set-up works inside that sheet, a reduced browser on iOS that closes when the network changes;
- whether a phone leaves the access point, or sends its traffic over mobile data, because it has no internet;
- what the unit must answer to each phone's check (the address each phone asks to learn whether it is signed in).

On the unit, with the Pico W on the probe and this workstation's own Wi-Fi as the client (the owner, 2026-11-18), because a mock cannot show what the radio does (the agent's review):
- **The access point while the station joins.** The CYW43 runs its access point and its station on one channel. A station that joins a router on another channel very likely moves the access point with it, and a phone on it drops off: the very thing "the access point kept up while the station joins" is meant to avoid. Seen with a test image that keeps the access point up after the join, the router on another channel than the access point's, the client's link and pings logged through the join.
- **A scan with the access point up:** whether the unit can list the networks while a client is on its access point, and what the client sees meanwhile.
- **Today's captive parts** as a client sees them: the access point's DNS answering every name, the 302 to the unit, and what a request to port 443 gets (refused at once, or left to time out, which keeps a phone's check waiting).
- **The phones** then on the same image, for the list above.

### The set-up, designed again (gate 4's design gate, 2026-11-19, for the owner to settle)

The set-up as drawn in gate 1 failed on the owner's iPhone at the unit (progress.md, step 4). The owner: "Key is confusing. It should named password. Selecting SSID and password on the same screen doesn't make sense ... The wrong password warning doesn't disappear until it is touched. I stuck at Joining <SSID> view", and then: "the mini display doesn't change neither, and still stuck at the setup page, and because both screens are showing during commissioning, for an average user, it is legitimately stuck." What happened: the unit joined in 3.3 s and stored the network; the hotspot moved from channel 3 to the router's channel 1 with the join, the iPhone did not follow, the page took its failed reads for "still joining", and the 12864 stayed on "A phone joined". "To find out first" named this risk before gate 1; gate 1 saw only a Windows client follow the move, and the phone was never on the hotspot while the unit joined. The owner then asked for the set-up to be designed properly ("Yes, design it"). This design replaces "The set-up as drawn" where they differ; the code, the hotspot's own key and name, and the steps' order stay.

**What it holds to:**
- **Two screens, one process** (the owner). Through the set-up the phone and the 12864 are two views of one set-up, and both change at every stage. The set-up's stage is kept by the unit, not by the page: the page and the screen each draw it, and a phone that comes back after losing the hotspot finds the set-up where the unit is.
- **The unit's screen is where the outcome is always shown.** The phone can lose the hotspot (a channel move, its sheet closed, the hotspot stopped at the end); the screen cannot.
- **Every wait ends** in words: a stage that waits says what it waits for, and after its time says what happened or what to do.
- **One thing at a time on the phone:** a network chosen, then its password, each a view of its own.
- **A person's words:** "password", never "key", on the screen, the page and the API (below); "Wi-Fi" and "network" for the person's, "the unit's hotspot" for its own.
- **A message stands beside what it is about** (under the password, on the network's line), and goes when the person changes it; the page's bar at the top is for what has no place of its own.

**The radio: the hotspot on the network's channel.** The CYW43 keeps its hotspot and its station on one channel, so a join to a router on another channel moves the hotspot, and a phone may not follow (it did not here). Read in the driver: the hotspot's channel is set before it starts (`cyw43_wifi_ap_set_channel`, 3 by default), and a join can name the access point to join (`cyw43_arch_wifi_connect_bssid_timeout_ms`). So:
- A unit never set up scans at its start, before its hotspot (some 1.3 s), and starts the hotspot on the channel of the strongest network it hears, which is most often the person's own.
- A join takes, of the chosen network's access points, one on the hotspot's channel when the scan heard one (a mesh, as the owner's, has its nodes on 1, 6 and 11), else the strongest.
- The hotspot then moves only for a network heard on no access point on its channel; that case is the fallback below, not the rule.
- To be seen on the unit first, as the build gate's first step: the hotspot started on a chosen channel, a join of an access point on it, the phone kept on the hotspot through the join (the iPhone at the unit, with the owner).

**The stages, each on both screens.** The step names are the page's; the unit keeps the step (`GET /api/v1/setup`) and says each change as an event, which the page and the menu follow.

| Stage | The phone | The 12864 | How it ends, and the way back |
|---|---|---|---|
| Never set up, started | (nothing yet) | The main menu leads with "Set up this unit"; the title's hotspot mark | The row opens the code |
| The code | The camera's notice: join `opentrickler-XXXX` | The code, "Scan this with your phone's camera.", Back | A phone gets an address: the next stage. Back: the main menu, the row still there |
| A phone joined | The sign-in sheet opens by itself on the set-up's first view | "Set up: Wi-Fi" / "A phone joined." / "Choose your Wi-Fi" / "on your phone."; Back, Show the code | The page's choice. Another phone may scan "Show the code" |
| Choosing a network | **View 1, Your Wi-Fi:** the networks heard, strongest first, signal in words, a lock where a password is needed; "Another network"; "Look again"; "Not in the list? The unit hears 2.4 GHz networks only."; "Skip: keep the unit's own hotspot" | As above | A network tapped: view 2 |
| Its password | **View 2, Password for CMCC:** the password field, "Show password", Join; "Choose another network" | "Set up: Wi-Fi" / "Type the password of" / "CMCC" / "on your phone."; Back, Show the code | Join |
| Joining | **View 3:** "Joining CMCC", its seconds counting ("8 s of up to 30"), "The unit's screen shows it too." | "Joining" / "CMCC" / "8 s of 30", counting each second: a still screen reads as stuck; Back, which leaves the set-up on the phone | The unit's join ends, or the phone loses the unit (below) |
| Joined | **View 4:** "Joined CMCC." / "On your Wi-Fi the unit will be opentrickler-XXXX.local (192.168.2.168)." / Next | "Joined" / "CMCC" / "Continue on your phone."; Back | Next: the scale |
| Wrong password | View 2 again, the field kept and focused, under it: "Wrong password. Check it and try again." It goes as the person types | "Wrong password for" / "CMCC" / "Retype it on your phone." | Join again; or "Choose another network" |
| Not joined, no answer | View 2 again, under the field: "CMCC did not answer: move the unit nearer its router, or check it is a 2.4 GHz network." | "No answer from" / "CMCC" / "Move it nearer the router." (the phone gives the 2.4 GHz hint) | The same |
| The phone lost the unit (a read unanswered for 5 s at any stage) | **Lost view:** "Lost the unit's Wi-Fi. Your phone may have left it." / "The unit's screen shows how things stand." / "Join opentrickler-XXXX again in your phone's Wi-Fi settings; this page carries on by itself." The page keeps asking, every 2 s | Whatever the unit's stage is: its screen goes on as if nothing happened | The page hears the unit: it reads the stage and goes on from there. The sheet, if iOS closed it, opens again when the phone joins the hotspot, at the unit's stage |
| The scale | Its make, its live weight; nothing in 3 s: what to check (as drawn) | "Set up: Scale" / the weight as a job's figure / "The scale sends its weight", or "--" and "Nothing from the scale" | Next |
| The motors | Each driver found or not (as drawn) | "Set up: Motors" / "Coarse driver: found" / "Fine driver: found" / "Continue on your phone." | Next |
| Ready | What was set; "Finish" | "Set up: Ready" / "Ready." / "Finish on your phone." | Finish |
| Done, joined | **Before** the hotspot stops, the page says what comes: "The unit is leaving its hotspot for CMCC. Your phone goes back to your Wi-Fi; open the unit there at opentrickler-XXXX.local. The unit's screen shows the address too." Then the sheet may close | "Set up: Done" (the station's mark) / "Open on CMCC:" / "opentrickler-XXXX.local" / "or 192.168.2.168"; OK, which goes to the main menu (no set-up row) | The hotspot stops 2 s after the reply; the address stays on the screen until OK |
| Done, its own hotspot (Wi-Fi skipped) | "The unit stays its own hotspot. Open http://192.168.4.1 when your phone is on it." | "Set up: Done" / "On its own hotspot." / "Open 192.168.4.1" / "from a phone on it."; OK | The hotspot stays |
| The unit restarted mid-way | Lost view, then the unit's stage: from the start, unless the network was joined | A network joined and stored: the unit starts on it, set up (a network named); not joined: never set up, as at the start | As those stages |

**Where the 12864 follows the set-up.** Following a process the person runs on the phone is not a pop-up at the start: once a phone has joined (the code shown, or its row), the screen shows the set-up's stage as it changes, unless a job holds the screen. Back leaves it for the main menu, and the set-up goes on on the phone; the main menu's row then opens the set-up's stage, with "Show the code" there. Every set-up screen is a kind the skill has: a dialog (a title, up to three lines, softkeys) or a job's figure (the scale's weight).

**The API it takes** (the build gate's, here for the owner to see the cost):
- `GET /api/v1/setup`: `set_up`, `step` (code, wifi, scale, motors, ready, done), `phone` (a phone joined the hotspot), `hotspot` (its name, which the lost view names); `PATCH /api/v1/setup` `{"step": ...}` as the page goes on; `POST /api/v1/setup/done` as built. An event `setup` `step` at each change, and the join's end as an event `wifi` `joined` or `not_joined` with its reason, so a page on the stream hears it, and one that comes back reads the step.
- **"password" in the API too,** before the first user release (the API's names are renamed until then, the owner's rule): the join's `key` becomes `password`, and `settings/wifi`'s `key` with it; the screen's "Key" on the Wi-Fi page is "Password". The stored layout does not change.

**Drawn:** the 12864's stages, `tools/ui_mock/setup_12864.py` (`mockups/setup_12864.png`), every line measured against the panel's width, a network's name on a line of its own (a long one shown); the phone's, on the stand-in (`tests/web_test.py --hotspot`).

**Still not in it:** Android (no phone at hand, the owner); a hidden network beyond typing its name ("Another network").

## Every client echoes the unit

The owner, 2026-11-18: "echoing the state, not taking over one from another". Read here as: every client (the screen, each browser, each agent) shows the unit's own state as it is; a change made from one is shown by all; no client holds the unit or shuts another out.
- **The refusals that stay** are about the unit, not about a client: a write to the charge's or the motors' settings during a throw (409 `busy`, ADR-003), one job at a time.
- **Kept** (the owner, 2026-11-18): ADR-004 refuses an update while a person has a screen open at the unit, so a unit never restarts under a hand at the knob. It favours the screen over the web, as a safety guard, not a take-over.
- **What it needs:** a setting changed is not an event today (the log has the charge, the throw, the cleanup, the motors, the system and the update: `events.h`). So a setting changed at the screen never reaches an open page. A new kind of event, a setting changed, with its resource and who wrote it (`screen`, `web`, `console`, `api`); a page then reads that resource again.

## The unit to the page: an event stream

The pages poll today: the charge's state every 500 ms (What we know). ADR-003 measured what load costs the Pico W's web. The owner asked for a better way and chose to explore server-sent events, with their life cycle managed (2026-11-18).

### The ways weighed

| Way | How | On this unit | Verdict |
|---|---|---|---|
| Polling (today) | The page asks every 500 ms | A whole request and reply each time, each held in lwIP's heap until acknowledged | The load ADR-003 measured |
| Held reads | `GET /api/v1/events?after=N&wait=30`: the unit answers when something happens | Built (ADR-003), a parked request | Kept for agents; a request for each event |
| **Server-sent events** | One GET kept open; the unit writes each event as a line; the browser's `EventSource` reconnects by itself and says the last event it saw | The server sends a reply in pieces, and parks a request, but not both on one reply: a stream today must keep making progress or is cut as stalled (`http_server.h`). A stream that waits for events is new server work | **Chosen to explore** |
| WebSocket | A handshake, then frames both ways | A new protocol: its handshake's hash, framing, masking, pings. Both ways are not needed: writes stay REST | Not taken |

Why the stream: the event log is already numbered and already tells a client of a gap (`first`) and of a restart (`boot`; api_v1.md, "The event log"), which is what a stream's reconnect needs. One connection a browser in place of a request every 500 ms. Plain text over HTTP, so an agent reads it with `curl -N` too. And every client reading one stream is what echoing needs.

### Its life cycle

| Stage | What happens |
|---|---|
| **Open** | The page asks `GET /api/v1/events/stream` (built 2026-11-19 at a path of its own, not by `Accept` on `/api/v1/events`: no content negotiation in the server, and an agent reads it with `curl -N`). Over the cap of streams, 503 with `Retry-After`; a browser's `EventSource` does not try again after an error status, so the page does, after that time. |
| **Start** | First `unit` (the `boot`, and `resumed`: whether it goes on from the client's last id), then `state`: the job, the charge as its GET gives it (the live weight included) and the purge. Each settings resource's `saved` is not in it: the page reads the settings it shows when it starts, as now, and the `setting` events (`changed`, `stored`) keep them current. |
| **Running** | Each event with the id `<boot>-<seq>`. During a charge the live weight comes on the same stream, no faster than the scale's own rate, as a state message with no id: it is not an event and is not kept in the log (below). |
| **Kept alive** | The state again after 15 s with nothing sent, so each end knows the other is there. Not a comment, as first built: a browser's `EventSource` never shows its page a comment, so a page could not tell a quiet unit from one gone (found in gate 3, 2026-11-19). |
| **Reconnect** | By the browser itself, with the last id it saw. The same `boot` and the event still kept: the stream goes on from there, nothing missed. An id no longer kept, or another `boot` (a restart, an update): a new snapshot, then on. The rule held reads follow today. |
| **Closed by the page** | When its tab is hidden (the phone asleep, another app), the page closes its stream, and opens it again when shown. Its slot is free before it goes stale. |
| **Closed by the unit** | A client that stops reading: what was sent waits in its connection, and the server's stall limit (10 s with bytes not taken) closes it, so a dead connection never holds lwIP's heap for long (measured: closed about 14 s after the client stopped, the heap back to what it was). |
| **The network changes** | A restart the unit means (an update's, the API's restart and factory reset, the screen's restart) closes every connection first, its streams too, so the client hears at once (2026-11-19: the stream closed 0.5 s after the restart was asked; before, it was left silent). `wifi leave`, a power cut, the access point stopped when the station joins: the unit sends no close, and the client hears nothing (measured: the old stream silent, not closed). The page knows by the state that should come again within 15 s: nothing for some 20 s, it opens the stream again, and the reconnect above takes over. The unit frees the old slot by the stall limit once its bytes find no client. |
| **Seen** | `net` gives the streams open, opened and refused since the start (`live`, `live_opened`, `live_refused`); `GET /api/v1/debug` the same with the server's own counts, the stalled ones among `aborted_stalled`. |

- **A cap:** at most 3 streams of the server's 6 slots (a proposal, measured in the soak), so a request always finds a slot.
- **What is kept in the log, and what only passes on the stream** (the agent's review, 2026-11-18). The log keeps the newest 64 events (`EVENTS_KEPT`, `events.c`). The scale's readings at its own rate would push every other event out within seconds of a charge, and an agent's held reads would meet a gap every time. So:
  - **the live weight** is a state message on the stream, with no id: a client that reconnects reads the state again in its snapshot and has missed nothing it needs;
  - **the mirror's frame** (about 1 KB for each change) stays on its own poll, as today: on the stream it would take most of the 2 KB a stream may leave unacknowledged;
  - **a setting changed** is one event for each resource written, not one for each field, so an import of every resource is a handful of events, not a hundred.
- **Agents** keep the held reads; the stream is open to them too.
- **Built** in the API review's gate (below), with the server's change.

## An API an agent reads by itself (the API review)

The owner, 2026-11-18: the API needs a review; an agent should find every call it may make from the root, and each call should say how it is used, a GET of hello its usage rather than an error.

**What is there** (read 2026-11-18): `GET /api/v1` gives a guide in plain words (how a setting is written, one job at a time, the cup) and every resource with its methods and a line on it; `GET /api/v1/schema` gives every action with its fields, when it is refused and a line on it, the states, the events and the errors. What is missing:
1. `/` is the portal's page and says nothing to an agent.
2. `POST /api/v1/agent/hello` is in neither the index nor the schema, on purpose (`api_v1_commands.c`: "Hidden").
3. A GET of an action answers 405 `method_not_allowed`, "the path does not take that method"; a path not there, 404 `not_found`. Neither says where to read.
4. Nothing checks that every route is in the index.

**What we do** (the agent's proposal, approved with this ADR):
1. **The root points an agent to the index,** with nothing shown on the page: `<link rel="service-doc" href="/api/v1">` and `<link rel="service-desc" href="/api/v1/schema">` in the portal's head, the standard link relations for "this site's API is described here" (RFC 8631). Nothing more at the root: the index stays the one place, with no second document to keep in step. Whether agents read these links is not known; the next three work either way.
2. **A GET of every action gives its usage,** 200: its method, its body's fields with their types and limits, what it answers, when it is refused, and an example. It comes from the table the schema is made from: one description, three readers (the schema, the index, the action's own GET). A GET still changes nothing. A GET of `/api/v1/agent/hello` says to POST `{"name": "<who you are>"}` there, what the screen then shows, and that a charge refuses it.
3. **Every error under `/api/v1` says where to read:** a `see` field with the path that describes it; a 404 the index; a 405, with its `Allow`, the action's own GET.
4. **Every route is in the index,** or in a short list of exceptions named in the code (the pages, the mirror); `rc_run.py api_check` compares the server's routes with the index and fails on one in neither.
5. **Hello is listed,** under a kind of its own, `agent`: an agent finds it, and it stays the owner's fun. AGENTS.md's "Say hello", which says it is in neither the index nor the schema, is brought up to date in the same gate.
6. **The unit's report of what it carries** (ADR-005's feature table; "No feature silently dropped", above): each feature with present or absent, in the index.
7. **A revert beside each save** (the owner, 2026-11-18, asking for a reset beside Store): `POST <the save's path, /revert in place of /save>` reads the slot's record back from the EEPROM and applies it as a write does, so what was changed and not stored is undone; the schema gives it as each resource's `revert`, beside `save`. Like a save it is the slot's: the two motors together, the eight powders together. The mock's stand-in has it now (`propose_revert` in `tests/web_test.py`, refusing to start once the recording has it); the page offers "Undo changes" only where the schema lists it.
8. **`saved` for each resource, not each record** (the owner, 2026-11-18: a setting not stored is tagged, a powder "won't, and I think that's the gap"). Today `saved` is the record's CRC against the one last stored (`config_is_saved`, `common.c`): the eight powders are one record and the two motors another, so a change to one powder makes all eight not stored, and the unit cannot say which. Each resource's part of its record gets a CRC of its own, kept when the record is read and when it is stored (a powder's `profile_t`, a motor's settings), and `saved` compares that part alone; a value written back as stored is stored again. Store and revert still act on the whole record, which changes nothing for a part that was not changed. RAM: one entry of `config_stored` for each part, ten more. The stand-in does it now (its `write`, against what it holds as stored); AGENTS.md's line on `saved` is brought up to date in the same gate.
9. **The update's life cycle** ("The update's life cycle, read whole", above): its five gaps and the two smaller, in the firmware's update and its settings' loading, with `last`'s new fields (`reason`, the modules reset) and `back_within_s` in api_v1.md. Gap 1 goes first, and before any release that changes a stored layout.

**Where:** all of it adds to API v1 and breaks nothing. It is ADR-003's area, and ADR-003 is closed; it is built here, as the API review's gate, with the event stream and the event for a setting changed.

## The words: one set for the screen, the web and the API

The owner, 2026-11-18: the web uses the screen's words, "purge" everywhere; and every name that does not fit may change before the first release for users. Read here as one list of words, in this ADR, that the screen, the web, the API, the events and the console all use; a word changed is changed in all of them at once.

Today's words, read 2026-11-18 (the screen's from `menu_nav.c`, the web's from `features.md`, the API's from api_v1.md). The column "Proposed" is the agent's, settled with the mock:

| The thing | Today's screen | Today's web | Today's API | Proposed, everywhere |
|---|---|---|---|---|
| The page of the two jobs | (Charge and Cleanup, rows of the main menu) | Trickler (Trickler and Purge, two tabs) | (none) | **Trickler** (the owner), its switch Charge, Purge |
| A throw to a target | Charge | Trickler | `charge` | Charge |
| Running the tricklers to empty them | Cleanup | Purge | `cleanup` | **Purge** (the owner); the API `purge` |
| A powder's stored tuning | Powder | Profile, Powder Profile | `powder_profiles` | Powder (ADR-014's charge profile comes beside it, so "profile" alone would mean two things); the API as it is |
| The scale's tare | Zero | Scale Zero | `scale/zero` | Zero |
| The shutters | Servo gate | Servo Gate, Gate Control | `gate` | Servo gate |
| The two motors | Motors, Coarse and Fine | Coarse Trickler Motor, Fine Trickler Motor | `motors/coarse`, `motors/fine` | Motors: Coarse, Fine; never "tricklers", which names the page |
| The lights | Backlight (on Display) | Neopixel LED | `leds` | Lights: to settle with the mock |
| The 12864 | Display | Mini 12864 Module | `display` | Display |
| The network joined | Wi-Fi | Wireless | `wifi` | Wi-Fi |
| The unit's own network | Hotspot | (none; "access point" in these documents) | (none) | Hotspot |
| Bringing a unit into service, from its first start to its first charge | (the Wi-Fi page) | Setup Wizard, Configure WiFi | (none) | **Set up** wherever a person reads it: the page, the screen, the API ("Set up this unit"). "Commissioning" only in this ADR, the name of the design work and of the process with its parts no page shows (the owner, 2026-11-18: "Keep commissioning in the ADR, Set up on the page") |
| Starting again | Restart | Reboot | `system/restart` | Restart |
| Every setting erased | Factory reset | Erase EEPROM | `system/erase` | Factory reset; the API `system/factory_reset` |
| Writing settings to the EEPROM | (each row stores itself) | Save to EEPROM, Apply | `save` | To settle with the mock: one word, and whether a person sees "stored" at all |
| New firmware | Update, Firmware | (none today) | `update` | Update |

- **The settings' own words** (the schema's labels, groups and lines, from the C tables the screen reads too) follow the same list, and say each thing once (`web-ui-design`, rule 16): a group named as its resource goes, a field named as its resource takes its own name (the scale's "Make", the gate's "Fitted", the Wi-Fi's "Join a network"), the resources are "Coarse motor", "Fine motor", "Powder" and "Lights", and "trickler" leaves the motors and the powders. The mock shows them before the unit says them: `PROPOSED_WORDS` in `tests/web_test.py` puts them into the recorded schema. Gate 2 writes them into the C tables (the screen's rows checked against its baseline: it shows the same labels, flat) and records again; the stand-in then refuses to start until the list is emptied. Labels kept as they are because the screen shows them flat with no group: "Coarse Kp" and the like, "PWM3 LEDs" and the like.
- **The API's renames are built in gate 2,** the API review, with what reads them: `rc_run.py`, `unit_api.py`, `tests/web_test.py` and its recording, api_v1.md, AGENTS.md. A rename is a change of name, not of what it does: `api_check` and `settings_roundtrip` pass on the new names as on the old.
- **The screen's words are changed in gate 3,** with the pages ("Cleanup" to "Purge" on the main menu, its job and its cup), and its menu baseline saved again. The screen is ADR-010's, closed; the change is made here because the words are this ADR's.
- **Names in the source** (`cleanup_mode.cpp`, its functions) follow when they are touched: ADR-006 lays the source out again.

## Resources from the internet

The owner, 2026-11-18: the basics self-contained; an advanced feature may download what it needs; "I don't expect you to serve plotly from the pico" (Plotly is about 3.5 MB; the Pico W's slot has 32.6 KB left). Read here as one rule:
- **The basics never need the internet** (What we want): nothing they show or do is loaded from outside the unit.
- **An advanced page may load a library from the internet,** at an exact version from a CDN, with its integrity hash (the browser refuses a file that differs). With no internet, always so on the unit's hotspot, the page says plainly that this feature needs the internet, and the rest of the portal works on.
- **It cannot be kept for offline use:** a browser keeps a page's files for use offline (a service worker) only for a page served over HTTPS or from localhost, and the unit serves plain HTTP. An advanced page works while the phone or the computer has the internet.
- **Whether ADR-009 needs such a library** is ADR-009's to decide: a throw's chart is a few lines on a canvas, and a chart of our own may fit on the unit. No page of this ADR loads one.

## The pages

Settled (the owner, 2026-11-18):
- **A mock first.** The pages, clickable in a browser against the stand-in server, before anything goes on a unit. The owner decides by using it. An agent can open the same mock in its own browser and check it.
- **The mock is the final pages, with a stand-in backend** (the owner, 2026-11-18: "the same page but with mocked backend is preferred"). The pages are written once, as the real sources, and run unchanged against the stand-in and later the unit. The stand-in is `tests/web_test.py` grown into a simulated unit: a charge that goes through its steps, a purge, the event stream, an update; every write checked against the recorded schema, as today, and the recording made again from the unit whenever the API grows, so the two do not drift.
- **The mock shows the proposal for every feature** (the owner, 2026-11-18: record every feature, a proposal for each; the UI may be reorganised; the trickling kept by good design; every setting kept, its presentation free). In the mock only, each feature carries the number of its row in `features.md`; a check lists every row neither shown nor marked left out. So "nothing silently dropped" is checked by the script, and the owner reads only the short list proposed to be left out.
- **The styles and the tools: the agent's proposal, in gate 1** (the owner, 2026-11-18: no strong preference; Tailwind and DaisyUI were a previous agent's recommendation; "if you see a better fit, or a better UI design for the application then I'm happy to review your proposal"). Whatever is chosen is trimmed at build time to what the pages use and served by the unit. Its tools are development tools, installed as needed (the owner), each version written in `toolchain.env` and fetched by both setup scripts and CI like every other. The proposal weighs, by the mock's real gzipped size and how it reads at the bench: Tailwind with DaisyUI by Tailwind's standalone command-line tool (one program, no Node; DaisyUI 5 is said to work with it as a plain plugin file, thought so, not checked), Tailwind alone with our own few components, and plain CSS of our own with no tool. It comes with a page drawn each way, for the owner to choose by using it.
- **The tools come in gate 1, not gate 3** (the agent's review, 2026-11-18): the budget is settled against the mock's real size, which includes the trimmed styles, so the build that trims them is there first. The mock is the pages' first version, in the real layout of sources (nothing is thrown away), and `tests/web_test.py` serves what the build makes from them, not `src/html` as it stands, with stand-ins for what the recording lacks (the stream, a list of networks, an update under way).
- **The pages cached by version:** already built, by ADR-003. Proposed by the agent as new on 2026-11-18 and found built the same day (`features.md`, row 6): each page goes with an `ETag` of the firmware's version and commit and `Cache-Control: no-cache`, and a read with that tag is answered `304` with no body (seen on the Pico W: the portal's 93,789 bytes, then 0). The new pages keep it, gzipped. An update is a new version and so a new `ETag`: an old page is never shown.
- **The budget is set from the slot.** The Pico W's slot has 32.6 KB left (2026-11-18; 38 KB when this was agreed), and our pages take 102.7 KB of the image (18.6 KB gzipped). Served gzipped, the whole UI aims at about 50 KB, handing the rest back to the ADRs after this one (ADR-009, ADR-014); the figure is checked against the mock's real size.
- **No tuning here.** Tuning with powder, a chart of a throw and PR #113's powder analysis are ADR-009's. This ADR changes nothing at the level of a powder throw.
- **Settings forms drawn from the schema** (the owner, 2026-11-17: see "Handed on by ADR-010").
- **One language,** English.
- **Phone first, and the same page scales to a tablet and a computer's browser** (the owner, 2026-11-18: "the UI element should be made scalable on phone, tablet and computer browser, just consider the average use case for blokes working in the garage"). Read here as:
  - one layout that grows with the width, never a second page for phones (today's `/mobile` serves the same portal);
  - made for a person at a bench in a garage: a phone propped beside the trickler and read at arm's length, a glance between other jobs, hands that may be dirty or gloved, light from bright to dim. So the live weight and the target are the largest things on the charge's page; every control is a large target, with room between targets; nothing depends on hovering or a fine pointer; contrast holds in any light, and the page follows the device's light or dark setting;
  - the mock is checked at three widths, a phone (375 px), a tablet (768) and a computer (1280), in an agent's browser and by the owner on their own devices.
  - Known limit, thought so and not checked: a browser keeps the phone's screen awake (the Screen Wake Lock API) only for a page served over HTTPS, so over the unit's plain HTTP the phone may dim during a charge. Seen in the mock on real phones.
  - **Keep the phone awake** (the agent's idea, taken 2026-11-18, with "its own toggle switch and default to ON", the owner): where the Wake Lock API is not given, a small muted video looping on the charge's page keeps the screen on, as pages commonly do. It has a switch of its own, "Keep this screen awake", on by default. Proposed: the switch is kept by the browser, not stored on the unit, as it is the phone's screen and not the unit's (two phones may want it differently), so it is not one of the unit's settings and not on the 12864. Tried on real phones in the mock; what it costs a phone's battery is read there too.

Agreed with the owner (2026-11-18), the agent's proposals:
- **Pages:** charge (target, live weight, start and stop), profiles, settings, update (ADR-004), the set-up (commissioning, above), and the screen's mirror, all one portal.
  - **The cleanup has no page in this list** (found by `features.md`, row 30: today it is the portal's Purge). Proposed: on the charge's page as its other job, started by its own button with the cup asked, as the screen asks. Settled with the mock.
- **Built from sources:** separate files for markup, styles and script, joined, trimmed and gzipped by the build into what the unit serves, with `Content-Encoding: gzip`.
- **No framework at run time, or a very small one.** The pages are a handful of forms. A proposal with sizes comes with the mock.

## Gates (agreed with the owner, 2026-11-18)

1. **The mock** (a design gate). The pages clickable against `tests/web_test.py`; the look, the proposals above, commissioning's, and the budget against the mock's real size settled by the owner using it. In it, in this order:
   - the radio and today's captive parts probed on the Pico W from this workstation's Wi-Fi (done 2026-11-18, progress.md); the iPhone deferred to when the owner is at the unit, at the start of gate 4 at the latest ("To find out first");
   - the inventory of today's pages' features (`features.md`), each placed or left out by the owner's decision;
   - the styles' proposal and its tools in the build, the stand-in serving what the build makes;
   - the mock itself, at the three widths.
2. **The API review** (designed, then built). The event stream and its life cycle, the event for a setting changed, the API that describes itself, the feature table and the unit's report of what it carries, the API's names and the settings' own words made the words' ("The words"; `PROPOSED_WORDS` emptied), the revert beside each save (`propose_revert` taken out), `saved` for each resource, the update's life cycle made whole (its gaps 1 to 5), and the index's slow making looked into (progress.md); the server's change; soaked on the Pico W against polling.
3. **The pages built.** Every row `features.md` marks broken made right by the new pages, first the system's Save, Restart and Erase as three actions of their own, each with its own confirmation and none chosen in advance (rows 70 and 74). Gzip and the `ETag`, the forms from the schema, the update's page, every client echoing the unit, each feature's part shown only when the unit reports it. The screen's words made the same ("The words"), its menu baseline saved again.
4. **Commissioning built.** First the iPhone on the probe image, if not seen before (the owner at the unit). Then the set-up in the portal, opened by the captive portal and by DHCP's option 114; what the mock settled of the networks' list, the join, the key, the id and the QR code.

Then the proof on both boards; PR #111 is closed once the update is shown end to end from the page.

### Gate 2's plan (the agent's proposal, approved by the owner 2026-11-19)

What gate 2 builds is already said above, item by item ("Every client echoes the unit", "The unit to the page: an event stream", "An API an agent reads by itself", "The words", "Handed on by ADR-005"). This is the order, how each step is proven, and the few choices left. One branch, a step at a time, each step proven on the Pico W before the next starts; no PR of its own: the ADR's one PR, #134, waits for the owner. The bench is asked for before the first step that needs it.

1. **The words first,** so every later step is written on the final names.
   - The API's renames: `cleanup` to `purge` (its paths, its events, the `job` value, the schema), `system/erase` to `system/factory_reset`. The settings' own words into the C tables, `PROPOSED_WORDS` emptied and the unit recorded again.
   - Every reader in the same step: `rc_run.py`, `unit_api.py`, `tests/web_test.py` and its recording, api_v1.md, AGENTS.md, and today's portal's few fetches. Today's portal is replaced in gate 3; its paths are renamed here only so main's Purge keeps working until then (a rename, not a fix: "no fixes to code being replaced" holds).
   - **Kept in the API** (approved with the plan): `leds`, `save` and `saved`, `powder_profiles`. The rule is one word for one thing so a reader maps the screen to the API; "save" for Store and "leds" for Lights are read as the same thing at once, "cleanup" for Purge is not. Renaming them costs every reader and buys an agent nothing.
   - Proof: `api_check` and `settings_roundtrip` pass on the new names; `menu_tour --compare` unchanged (the screen shows its labels flat, as before).
2. **A setting changed is an event:** `setting`, with the resource's path and `by` (button, api, console, unit: `activity_by_t`, as a job's start says it), one for each resource written, a save and a revert included. Proof: a write from the console and one from the API each seen once in the log, with its `by`.
3. **The event stream** (its life cycle, above), the riskiest step, so it comes before the smaller ones.
   - In the server: a third kind of reply beside a stream and a parked request, a stream that waits: chunked, written when its `ready` says there is more, a comment line after 15 s quiet. The stall rule then counts only while bytes wait to go: a client that reads nothing fills its connection's send buffer (two segments, about 2.9 KB) and is closed after the stall time, so a dead stream never holds lwIP's heap for long.
   - Slots: 6, parked requests 2 at most (today's `HTTP_SERVER_PARK_MAX`), streams 3 at most, so one slot is always left for a request. Measured in the soak, changed if it says so.
   - The snapshot first, then each event with its id, and the live weight during a charge as a state message with no id. The pages still poll until gate 3 moves them onto it; the stand-in plays the stream for the mock.
   - Proof: the soak with two browsers on streams against the same with polling (`net`, the web's misses), and each stage of the life cycle once with its expected end (a client that stops reading, `wifi leave`, a restart, more than 64 events missed, the cap exceeded). A tab hidden and shown is the page's, checked in gate 3.
4. **`saved` for each resource, and the revert beside each save** (items 7 and 8). The parts' CRCs are in RAM: no stored layout changes. Proof: one powder changed, only it not saved; reverted, the value stored back and `saved` true; `settings_roundtrip` passes.
5. **The API that describes itself, and the unit's report of what it carries** (items 1 to 6). The report's first build options: the servo gate and the LEDs, on by default on both boards. The index's slow making (1.5 s for 7.4 KB) looked into first, as it is read first by every agent. Proof: `api_check` with its new check (every route in the index or a named exception, a GET of every action giving its usage); one build with the LEDs off, its report naming them absent.
6. **The update's life cycle** (item 9): gaps 1 to 5, `checking`, `back_within_s`. Gap 2's and 3's note is a record in an EEPROM slot of its own, so no stored struct of today's changes. Proof: as each gap says (a test image with a struct's size changed, tried and left, the settings unchanged; `update --expect power-trial`, then `last` back with its reason).

Then the proof on the Pico W: `menu_tour --compare`, `charge_cycle`, `api_check`, `settings_roundtrip`, the soak. The Pico 2 W for the server's change and the update's slots, at its next turn on the probe (AGENTS.md: the network is proven on the Pico W first; the RP2350's slots on its own).

### Gate 3's plan (the agent's proposal, 2026-11-19, for the owner's review)

Gate 3 builds the pages gate 1's mock settled onto the unit, on the API gate 2 made. Its order, each step proven before the next, on the branch of the ADR's one PR:

1. **The unit serves the new pages.** `scripts/web_build.py` runs in the firmware's build and makes `index.html.gz` into a header, served gzipped (`Content-Encoding: gzip`) with its `ETag` at `/`, `/mobile` and `/display_mirror` (the last opens the Unit page, where the mirror is: `features.md` rows 3, 5 and 6). Today's portal and mirror pages go. **The wizard stays** at `/` on the unit's own hotspot and at `/wizard` until gate 4 builds the set-up: taking it now would leave a unit just reset with no way onto a network. The flash measured: today's pages are some 103 KB of the image, the new one some 33 KB gzipped.
2. **The pages follow the unit:** the Trickler, the Unit page and the job's strip read the event stream in place of their polls, with its life cycle as the README's table has it (a tab hidden closes it and shown opens it again; nothing for 20 s, it opens again; a 503 tried again after its `Retry-After`); a settings form shown reads its resource again on its `setting changed` event, and its "Not stored" from `stored`. The mirror keeps its own poll. The stand-in's charge stops moving on with every read, so a stream's reads do not speed it.
3. **What the unit carries:** each page's part shown only when the index's `features` reports it (the servo gate's Settings part and its Open and Close).
4. **The screen's words** (README, "The words"): Cleanup becomes Purge on the main menu, its cup's question and its job; the menu's baseline saved again with `--save`, its pictures for the owner to see.
5. **The rows `features.md` marks broken** made right by the new pages, each checked on the unit (the system's Save, Restart and Factory reset as three actions of their own, each with its confirmation and none chosen in advance: rows 70 and 74; the gate's ratio read from the unit: row 37).

**How it is proven:** the pages driven in a browser against the Pico W at a phone's, a tablet's and a computer's width, each screenshot beside the mock's; `features_check.py --final` with only gate 4's rows left; `api_check`, `charge_cycle`, a soak with the pages' own reads (the stream and the mirror), and `menu_tour --compare` on the new baseline. The Pico 2 W at its next turn on the probe.

**Left for the owner:** the pages on the owner's own phone (carried from gate 1), and the new menu baseline's pictures. The phone done 2026-11-19 ("All fine", progress.md); the pictures are in #134 for the owner's review.

### Gate 4's plan (the agent's proposal, approved by the owner 2026-11-19)

Gate 4 builds the set-up gate 1 settled ("The set-up as drawn"), on the ADR's one branch, each step proven on the Pico W before the next. The gate says the iPhone comes first. Of what it shows, only the sign-in sheet (does it open by itself, does the set-up work inside it, does the last step's link open) changes anything built here, and only the pages' flow, step 4. So the steps that do not hang on it come first, and the iPhone, on the probe image, is seen before step 4, at the owner's time. A change of the gate's order, approved with the plan.

1. **The unit's id and its hotspot's key from the chip.** Each made from a SHA-256 of the chip's unique id with a word of its own (`name`, `key`), so neither gives away the other: the host name and the hotspot's name from the first, the eight-character key from the second. The README says "other bits of the id" for the name; two hashes keep the same rule more simply. The id is read from the Pico wherever it is wanted and stored nowhere, so it follows the Pico, not the board; the EEPROM's metadata record, which held only the id, goes (the owner, 2026-11-19: "Pico ID, and don't store to the board"; progress.md). Every unit already out gets a new host name once, with the update that brings it (said in its release notes). Proof: the same id after a restart; the key on the screen joins the hotspot from this PC's Wi-Fi; the API gives neither the id's source nor the key.
2. **The code on the screen** (ADR-010's menu, the screen's skill): LVGL's code widget (`LV_USE_QRCODE`) on, its flash measured against the Pico W's room; the main menu's "Set up this unit" row on a unit never set up, the code's screen, "A phone joined", and the Wi-Fi page's "Join by camera"; `menu_tour` and `reach` with the new screens, the baseline saved again. Proof: the drawn code decoded from `snap`'s picture, as gate 1's mock was; the iPhone's camera at the unit when the owner is there.
3. **The networks and the join, with the hotspot kept up:** `GET /api/v1/wifi/networks` (the scans merged by name, strongest first, the table from the heap), `POST /api/v1/wifi/scan`, `POST /api/v1/wifi/join` (stored only once joined), `station` in `GET /api/v1/network` (idle, joining, joined, failed, and why). The hotspot's life as drawn: up from a start with no network joined, kept while the set-up runs, stopped at its end once joined, or at the next start. The probe image's console commands go into the normal build as these. Proof: from this PC on the hotspot, a scan, a wrong key refused with the hotspot still up and nothing stored, the right key joined, the client's link kept through the join (gate 1 saw under 1 s of pings missed); `api_check` calls each.
4. **The set-up in the portal; the wizard goes.** `set_up` stored (said here before it is built: a field in the Wi-Fi's slot's filler, or a slot of its own, whichever the struct's size allows; checked with gdb as AGENTS.md asks), false after a factory reset; `POST /api/v1/setup/done`; `/` on the hotspot serves the portal, which opens the set-up on a unit never set up; DHCP's option 114 gives the set-up's address; `wizard.html`, its routes and `rest_root_is_wizard` removed. The pages as `setup.js` draws them, on the unit's API in place of the stand-in's `propose_setup`. Proof: a unit factory-reset (its settings copied first, put back after), set up from this PC's Wi-Fi end to end, and a wrong key typed again with no restart; `features_check.py --final` with no row left to come.

**How the gate is proven,** beside each step's: the iPhone at the unit on the built image, for the README's "How the unit shows it works" (a person sets up a unit just reset with no address typed); then `menu_tour --compare`, `charge_cycle`, `api_check`, `settings_roundtrip`, `web_robust` and a soak with a client on the hotspot. The Pico 2 W at its next turn on the probe, for the whole ADR's proof on both boards.

## How the unit shows it works

- With the internet unplugged, every page works from a phone on the unit's access point.
- A person not at ease with a browser commissions a unit just reset, from its first start, with no help: on an iPhone that has never joined it, joining its access point opens the set-up, and nobody types an address. Android is taken to work the same (the owner, 2026-11-18: no Android phone at hand), and is checked only if one comes to hand.
- Screenshots of each page from the unit match the mock.
- On a build without an optional module, the unit's report names it absent, and its page and its links are absent.
- Every feature in `features.md` is in the new pages or left out by the owner's decision.
- A charge started and watched from the phone, with `charge_cycle`'s checks.
- The same on the Pico 2 W. The ESP32-S3-Pico takes the same pages and is proven with them in ADR-007, which comes after this ADR (the agent's review, 2026-11-18: it was in this list by mistake).
- The size report shows the pages inside the budget.
- A setting changed at the screen shows on an open page, and one changed on a page shows at the screen, with no reload.
- An update tried and left leaves every stored setting as it was, a struct's layout changed or not; a power cut during a trial is reported as `back` with its reason; each way an update ends is said on the page in words ("The update's life cycle, read whole").
- The event stream, on the Pico W: a soak with two browsers on streams against the same with polling (`net`, the web's misses), and each stage of its life cycle once, each with its expected end: a client that stops reading (`web_robust`'s), `wifi leave`, a restart, more than 64 events missed, the cap exceeded, a tab hidden and shown again.
- `api_check` passes with its new check: every route in the index or a named exception, and a GET of every action giving its usage.

## Not in this ADR

- A phone app.
- A password (see ADR-003).
- Languages other than English.
- Tuning with powder, and any change at the level of a powder throw (ADR-009).
- WebSocket.

## To settle before approval

Nothing is left to settle before approval. Settled with the owner, 2026-11-18: the budget's rule (from the slot), the styles trimmed and served by the unit (which styles and tools, reopened after approval as the agent's proposal in gate 1), the cup asked on the page only, the event stream to explore, the API review in this ADR, echoing with ADR-004's refusal kept, phone first scaling to a tablet and a computer, the pages as proposed, and the gates.

Settled in gate 1, the mock, by the owner using it (2026-11-19: "I have reviewed the mock up ... happy to go ahead to the next stage"; progress.md says what was carried to gate 4): the look and the styles' tools; what `features.md` leaves out; whether the phone's keep-awake switch is kept by the browser; and commissioning, what it covers and each proposal above (the list of networks, the access point kept up while the station joins, the key and the id from the chip, the QR code), after the phones' captive portals are seen.
