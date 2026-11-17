# ADR-010: the screen's menu, designed

Draft for the owner, 2026-11-17. Not approved. The owner answered its decisions and its list of what may have been missed the same day (see the end); the gates are in the [README](README.md). This covers the logic only: what is nested under what, what the screen offers and what is left to the web, how the knob and the buttons drive it, the update's offer, and the split that lets another display come later. No pixels: the look is designed once this is settled.

## What the owner asked (2026-11-17)

- The menu's order: what is nested under what, and what is left out of the screen and reached only from the web.
- Only sensible options on the mini 12864, for a person with a rotary encoder and a button.
- The update's poll and its offer.
- Drop what the menu does only to get round u8g2's MUI, such as the Wireless menu's extra OK.
- A split like MVC (similar, not identical), so a 3.5" TFT, an AMOLED like the Open Weapon Mount Computer's, or a larger TFT can come later.

## What is there today

22 forms in `mui_menu.c`, their parents kept by hand in `menu.c`:

```
1 Main ─┬ 10 Select profile ─ 11/12 Weight (2 or 3 dp) ─ 13 Warning ─ charge mode
        ├ 20 Warning ─ cleanup mode
        ├ 40 Wireless ─ 41 "Press OK to view" ─ Wi-Fi info (a mode)
        └ 30 Settings ─┬ 31 Scale ─┬ 53 Driver, baud rate, OK
                       │           └ 51 Warning ─ calibration (the A&D's CAL)
                       ├ 32 Profile manager ─ 33 list, 34 gains (1/2), 38 speeds (2/2)
                       ├ 37 EEPROM ─┬ 60 Save
                       │            └ 61 Erase
                       ├ 39 Servo gate: Disable, Close, Open, OK
                       ├ 35 Reboot
                       └ 36 Version
```

What MUI forced (read in the code):
- **W1.** A screen that changes while it is shown cannot be a form. The Wi-Fi info and every mode draw from their own task, with the menu's task blocked meanwhile. Hence form 41's extra OK, and the API's `view` activity, which refuses a start while such a screen is open.
- **W2.** There is no number field. The weight is five one-digit fields, in two forms (2 or 3 decimals), with a button (`B1`) to choose between them.
- **W3.** A form has no parent. Every form carries Back and Next buttons or a "<-Return" row, and forms are reached by number.
- **W4.** A list cannot show a live value: the scale's make is chosen with no reading to show whether it works.
- **W5.** The modes read the buttons themselves (`button_wait_for_input` in charge, cleanup, update, calibration and the Wi-Fi info). A mode is the menu's task stuck in a loop, so a start from the API waits for the menu to get back to its forms.
- **W6.** A setting stays live until "Save to EEPROM" (form 60), which the person has to remember. A profile picked at Start is lost at restart unless saved.

Also found while reading:
- the target lives in RAM (`charge_weight_digits`) and is 0 after every start. Storing it is ADR-014's: the weight last used is kept with the charge profile;
- the scale's list offers "Simulated (no scale)" as a stored choice. On a unit with powder in it, that choice pours by a made-up weight.

## Principles

1. **The screen is for the bench; the web is for setting up.** A row belongs on the screen if a person needs it during a session, or if it fixes the unit without a phone (the scale's make, the knob's direction, a factory reset). Tuning, wiring and anything typed (names, the Wi-Fi key) are web only.
2. **The knob alone does everything.** The second button is only a shortcut.
3. **A row is shown only while it does something** (the Open Weapon Mount Computer's rule 13). The rule that decides is the same one that would refuse its command at the API.
4. **The screen is a client, as the web is.** It reads what the API reads and acts through the same entry points, `by` button. No rule exists only on the screen.
5. **A change is stored when it is confirmed.** There is no Save.
6. **Only two things ask first: what pours and what destroys.** Before charge or cleanup, the person acknowledges that the cup is on the scale, so powder never falls onto the bare pan (the owner: a safety warning, not a technical workaround). The factory reset asks too.
7. **Shallow.** A hub, and no row deeper than three levels.
8. **No pop-ups.** An update, a fault or a result waits in a row, or on the job's own screen.
9. **A job holds the screen while it runs, whoever started it.** When it ends, the person is back on the page they left.

## The knob and the buttons

The mini 12864 gives three inputs: the encoder's turn, its press, and the reset button (GP12). A TFT may have no reset button, and a touch screen has neither. The input driver turns them into four keys, and nothing above the driver sees a pin:

| Key | From | On a list | In a number editor | On a job's screen |
|---|---|---|---|---|
| Next, Prev | one detent clockwise or anticlockwise (Invert knob swaps them) | focus moves down or up and stops at the ends; above the first row is the title, which works as Back | the digit goes up or down | moves between the job's actions, or changes its one value (cleanup's speed) |
| Enter | a press released within 0.5 s | opens or does the focused row | next digit; on the last digit, stores the value | does the focused action, whose name is on the screen |
| Back | the reset button, or the knob held for 0.5 s | up one level, with focus on the row you came from | previous digit; on the first digit, leaves the value unchanged | stops the job (except while an update installs) |

- The driver reads both edges of each button, so a hold can be told from a click. Today it reads only the falling edge, with a 250 ms lockout. The debounce becomes about 20 ms.
- No double click and no hold-to-repeat. Nothing needs the reset button.
- Each page remembers its focus.
- A hint on screen names the board's own input ("RST: stop", or "Hold: stop").
- **Numbers:** one editor for every number, digit by digit as today. Its digits, decimals and unit come from the field's own description (`settings.h`). The target gets 2 or 3 decimals from Decimals.
- **Choices:** a list page with the current option marked. Enter picks it, stores it and goes back.
- **Toggles:** Enter flips the value and stores it.
- **Text:** never typed on the screen.

## The tree

```
Main menu                        title: OpenTrickler, or the motor driver that is not OK
├─ Update available ›            only while the poll knows of a newer release; "Important update" when critical
├─ Charge ›
│  ├─ Start                      → "Cup on the scale" → the charge screen; with no target set, the target editor first
│  ├─ Target        24.35 gr ›   number
│  └─ Powder        AR2208 ›     choice of the 8 powder profiles
├─ Cleanup                       → "Cup on the scale" → the cleanup screen
├─ Wi-Fi ›
│  ├─ Network       HomeNet      or "Joining HomeNet", "Not joined"
│  ├─ Address       192.168.2.168
│  ├─ Name          opentrickler-4993.local
│  ├─ Signal        -58 dBm      as a station only
│  └─ Network, Key               as its own access point only: opentrickler-4993, opentrickler
└─ Settings ›
   ├─ Scale ›
   │  ├─ Reading    24.35 gr     live; "no reading" while the scale sends nothing
   │  ├─ Zero
   │  ├─ Make       A&D FX-i ›   choice, every make but Simulated (a debug feature: the owner)
   │  ├─ Baud rate  19200 ›      choice
   │  ├─ Decimals   2 ›          choice (charge's decimal_places)
   │  └─ Restart to apply        only after a change that applies at restart (the make)
   ├─ Display ›
   │  ├─ Invert knob   Off       toggle
   │  └─ Rotation      0° ›      choice
   ├─ Firmware ›
   │  ├─ Version       2.0.2     "on trial" until the image confirms itself
   │  ├─ Check for update        Checking…, then Up to date or "2.1.0 found ›"; hidden with no internet
   │  ├─ Last update   2.0.2 confirmed    once there was one; or "went back to 2.0.1"
   │  ├─ Channel       beta      only while on beta; set on the web
   │  └─ Board, Unit id
   ├─ Restart                    "Restart to apply" while a setting waits for it
   └─ Factory reset ›            → Cancel, Erase
```

Four rows on the main menu (five with an update), as today. No row is more than three levels down.

On the charge and cleanup rows, a motor driver that is not OK opens the fault page in place of the mode, as today (ADR-012, step 6). It comes before the acknowledgement, so nobody is asked about the cup for a start that cannot happen.

**The acknowledgement,** before charge and before cleanup: "Put the cup on the scale", with one action, "Cup is on: start". Back cancels. It is a step of the start, not a page of the tree: Back from the job's screen returns to the page the start came from, not to the acknowledgement.

**The jobs' screens** are not in the tree. A job puts its screen on top, whoever started it:
- **Charge:** the state, the weight, the target, the powder, the time, the throw's number and the last verdict.
  - Actions: Zero while the job waits for zero, under the lock the charge starts its pour under. Not while it waits for the cup's return: the charge takes a reading of 0 as the cup back, so a zero with the pan bare would start a throw on it, as the API's own zero is refused there (found in the review of gate 4). Target and Powder between throws, from the verdict to the next zeroing (the API refuses both during a throw); what they open closes when the next throw begins, so Back then reaches the charge.
  - Back stops the job and returns to the Charge page, with Start focused.
- **Cleanup:** the weight, the flow, the speed and the gate. Turning sets the speed, Enter sets it to 0, Back stops.
- **Update:** Downloading n%, then Checking, then Installing (keep the power on), and the version. Back stops it until it installs.
- **Fault page:** names the driver and says charge and cleanup cannot start. Enter or Back leaves it.
- **Later (ADR-009):** the cup's ask ("put an empty cup on the pan, and press") and test throws, as screens of the same kind.

When ADR-014 brings charge profiles, a "Charge: <name>" row slots in above Target and Powder on the Charge page. Nothing else moves.

## Status on every screen

The screen echoes the unit, as it does today (the owner, 2026-11-17). Whatever job runs is on the screen, whoever started it. Every page shows live values, so a target set from the web is the one on the Charge page.

A corner of every page and every job's screen shows three marks. The remote mark is the owner's idea:
- **Wi-Fi:** joined, the unit's own access point, or none.
- **Remote:** the unit is being controlled from elsewhere. Either the running job was started by the web, the API or the console (each job already records this as `by`), or one of them wrote a setting or sent a command in the last 30 s (a proposal). Reads do not count: a page that only watches, such as the mirror or a charge page, controls nothing. The command layer records the time, since every remote write passes through it.
- **Simulated:** the simulated scale is standing in for the real one (the console's `sim on`). Its weight is made up, so a charge with powder in the hopper would pour by it. The person at the unit must see this for as long as it lasts.

When a job was not started from the knob, its screen's state line also says who started it ("Charging, from the web").

## What leaves the screen, and where it goes

| Today | New home | Why |
|---|---|---|
| 1 Main | Main menu | plus the update's row |
| 10 Select profile | Charge › Powder | one row that shows its value |
| 11, 12 Weight | Charge › Target | one editor (W2) |
| 13 Warning, charge | the start's acknowledgement, "Cup on the scale" | kept: a safety warning (the owner, D1) |
| 20 Warning, cleanup | the same acknowledgement | kept (D1) |
| 30 Settings | Settings | |
| 31 Scale, 53 Driver and baud rate | Settings › Scale | a live reading, no OK (W4) |
| 51 Calibration | removed, with its mode | no scale supports it (the owner, 2026-11-17) |
| 32, 33 Profile manager | Charge › Powder | it was a second way to pick the same thing |
| 34, 38 Profile details | web only | read-only gains mean little without the web's context; editing is the web's |
| 37 EEPROM, 60 Save | gone | stored when confirmed (W6, D2) |
| 61 Erase | Settings › Factory reset | |
| 39 Servo gate | web only (`PATCH /api/v1/gate`, the portal's gate) | for setting up and testing; cleanup opens the gate itself |
| 35 Reboot | Settings › Restart | |
| 36 Version | Settings › Firmware | plus the update's rows |
| 40 Wireless, 41 OK | Wi-Fi | a live page (W1) |

**Web only, and already on today's portal,** so nothing the screen gives up becomes unreachable between this ADR and ADR-008 (`web_portal.html` and `display_mirror.html`, read 2026-11-17):
- the charge's thresholds, margins, back-off, precharge and colours;
- the powder profiles' names, gains and speeds;
- both motors;
- the gate's duties, its speeds and its manual control;
- the LEDs;
- the scale's frame format;
- the Wi-Fi network, key, security and timeout;
- the mirror.

**Web only, and new:** the update's channel and source, stored with this ADR's poll and set through the API until ADR-008's page; and the upload by hand (ADR-008).

## The update at the screen

- **The poll:** once each start, quietly, after Wi-Fi joins as a station, and again from Firmware › Check for update. There is no poll on the unit's own access point, which has no internet; the row is hidden and Firmware says why.
- **Found:** "Update available" becomes the first row of the main menu, or "Important update" for a critical release. No pop-up.
- **The offer page:** the version, the first lines of its note (the full note is on the web), Install, and Later. If the last try went back, the page says so ("2.1.0 did not start; this unit is on 2.0.2").
- **Install** runs the update job:
  - downloading, n% (Back stops it, and the free slot is never booted);
  - checking;
  - installing, which cannot be stopped;
  - the restart.
  The unit then runs on trial until it joins its network. Firmware shows "on trial", then "confirmed" or "went back".
- **Shared with the web:** the same job and status serve the web's offer (ADR-008), so there is one implementation.
- **No downgrade:** the offer names only a newer version.

**The channel** follows the owner's idea of a beta channel, as phones have (2026-11-17). Proposed:
- **A stored setting, `channel`:** `stable` by default, or `beta`.
  - Stable reads `releases/latest/download/manifest.json`, which GitHub never resolves to a pre-release.
  - Beta reads the newest release, pre-releases included. The manifest has a fixed name and gives each file's full address (`scripts/release_manifest.py`), so the poll needs only the newest release's tag. How to find that tag cheaply is worked out with the poll: GitHub's list of releases carries every release's notes, while a list of one (`?per_page=1`) keeps the reply small.
- **Leaving beta downgrades nothing.** The unit keeps its version, and nothing is offered until a stable release is newer than it.
- **Where it is set:** on the web and through the API, beside the update's source. The screen sets no source (the owner, 2026-11-16). The Firmware page shows the channel only while it is beta, so a person can see why pre-releases are offered.
- **One stored struct for the channel and the source,** laid out once in a module's own slot. It is built with the poll here; ADR-008's page sets it. The source is GitHub by default, or another manifest's address, such as a mirror on the owner's network.
- **Until every ADR closes, every release is a candidate,** so stable finds nothing.
- **Tests never use GitHub's releases** (the owner, 2026-11-17). The bench points the unit's source at a local OTA server on the PC, which serves `release\` in GitHub's layout and made-up newer manifests (README, gate 1).

## The split (MVC, roughly)

```
 services: charge, cleanup, update, scale, Wi-Fi, motors, settings
     │ status, read as the API reads it          ▲ commands: the API's entry points, by button
     ▼                                            │
 menu tree (a table) ──► navigator ◄── keys: Next, Prev, Enter, Back
                         (pages, focus, the editor, the job on top)
                              │ what to show                ▲
                              ▼                             │
                    view, one per kind of display      input, one per board
                    (128×64 mono; TFT 320 to 480;      (encoder and buttons; touch)
                     AMOLED with touch)
```

- **Model.** The services and their status, read as the API reads them: the charge's status, `update_status()`, the scale's reading, the network, the drivers, the settings tables. Services neither draw nor read buttons; the mode loops leave the menu's task for a task of their own (W5).
- **Commands.** The entry points the API calls (`api_v1_commands.c`), with `by` button. The screen asks the same layer whether a command would be refused, and hides the row if so (principle 3).
- **The menu tree** is one table. Each page and row has:
  - an id (`settings/scale/make`);
  - a label;
  - a kind: page, action, choice, number, toggle or info;
  - what it reads and does. For a setting, that is its resource and field name, so the type, unit, limits and choices come from `settings.h`, described once;
  - when it is shown.
- **Navigator.** The page stack, each page's focus, the open editor, and the job on top. It knows keys, not pixels, and runs on a PC in a test.
- **View.** Draws the navigator's state: a list, an editor, or a job's screen. Only the view knows the display's size, its fonts and its toolkit. There is one view per kind of display, not per board.
- **The display's driver,** under the view: LVGL's frame sent to the panel. The 12864's is our own, for its UC1701, with no u8g2 (the owner, 2026-11-17); a TFT's comes from LVGL's drivers.
- **Input.** One per board, giving the four keys. On touch, a tap is focus and Enter on that row, and Back is a button in the header.
- **A new display** needs a new view and input. The tree, the navigator and the commands stay as they are. A TFT's charge screen with a plot is that view's choice, drawn from the same status.
- **The status is pulled.** The view samples it at its frame rate (the charge's weight is live anyway) and redraws what changed. No new events to keep.
- **LVGL draws; the navigator decides.** LVGL's encoder mode (edit or navigate, switched by its long press) and `lv_menu` would replace MUI's workarounds with LVGL's own, so neither is used.

## What may have been missed

1. **A job started from the web takes the screen**, from whatever page is open, and the page under it is kept. An editor open at that moment is cancelled, with nothing stored. The API's `view` activity then never happens; it stays in the enum, unreported, and v1 stays stable. The owner agrees, and adds that the screen shows who controls it (Status on every screen, above).
2. **"Services stop drawing" means the modes leave the menu's task.** Charge, cleanup, update and the Wi-Fi info stop calling `button_wait_for_input`; the screen sends commands instead. This is the largest piece of work. It touches `charge_mode.cpp`, the file every PR touches.
3. **A start from the web page asks nothing about the cup.** `web_portal.html` starts a charge or a cleanup with no acknowledgement (checked 2026-11-17). The safety warning holds on every way a person starts at the bench, so ADR-008's start asks it too. Whether the API should carry the acknowledgement as well (a field in the start's body that an agent has to state) is for ADR-008 to settle. Handed on to ADR-008 (the owner).
4. **Storing on confirm also stores the web's unsaved changes** to the same resource. That is acceptable, and is recorded here.
5. **The poll and pre-releases.** GitHub's "latest release" is never a pre-release, and every release stays a pre-release until every ADR closes. Until then the poll finds nothing. The end-to-end proof needs either a source on the owner's network (as the Open Weapon Mount Computer's `ota_host.py`) or a candidate channel. The owner's answer is a beta channel (The update at the screen, above).
6. **HTTPS to GitHub.**
   - The asset download redirects to another host.
   - The unit's CA roots expire or change. A unit whose roots fail can never update over Wi-Fi again, so keep more than one root; the USB full install stays the way back.
   - On the Pico W, TLS and LVGL want the same RAM: measure them together.
7. **The Scale page's make applies at restart,** so its reading shows a new make only after a restart. The simulated scale already hands over between drivers live; doing the same for the make would make the page tell at once.
8. **An agent drives the screen:**
   - stable page ids replace the form numbers;
   - a console command `screen` gives the page as text (its title, rows, values and focus), and `key next|prev|enter|back` drives it;
   - `menu_tour` walks the tree from its table and compares both the text and the snaps;
   - the new baseline replaces the old one, and the table above shows that nothing was dropped by accident.
9. **The mirror.** A frame copy suits a 128×64 (1 KB). A 480×320 colour frame is 300 KB, too much for the web's memory. On a large screen the web UI serves people and the screen's text serves agents.
10. **Future displays.**
    - An AMOLED burns in a static menu, so it dims after a time with no input and no job, and the first input afterwards only wakes it.
    - A screen with both touch and an encoder (the TFT35) drives one navigator from both.
11. **Rotation of 90° or 270° makes this panel 64 pixels wide,** and no row fits. Offer 0° and 180° on the 12864; each view gives its own choices.
12. **ADR-010's README says** "New screens or a new menu order: not in this ADR. First the same, then better." This design reverses that. Building the 22 forms in LVGL and then this tree would build the menu twice. The proof changes with it (item 8). The owner agrees with the rework, and wants a better layout proposed wherever one makes better use of the screen (Designing the look, below).
13. **The order of work agreed on 2026-11-17 (010 first) was not yet in the repository.** It is written in with this rework.

## Decided (the owner, 2026-11-17)

- **D1. The warnings before charge and cleanup stay.** "Those are safety warnings, and not technical challenges": the person acknowledges that the cup is on the scale, so powder is not dropped onto the scale itself. Each is one step of its start (the tree above). It was proposed for removal here, which was wrong.
- **D2. A change is stored when it is confirmed,** and Save to EEPROM goes.
- **D3. Storing the target is ADR-014's,** not this ADR's: the weight last used is kept with the charge profile (ADR-014's ideas; api_v1.md's "What it must grow into"). PR #109 was closed because it kept the weight inside the powder profile instead (ADR-002). Until ADR-014 lands, the target stays in RAM as today, and Start opens the target editor when none is set.
- **D4. Measure first.** LVGL (and TLS) are measured on both boards before anything is built on them, and the owner is told if the Pico W does not fit. Trimming LVGL to fit is acceptable, because most of its features cannot show on a 128×64 anyway. What is trimmed goes into the tiered feature table (ADR-005's, now ADR-008's), to be decided when a larger screen comes.
- **D6. The web's missing acknowledgement** is documented in ADR-008.
- **D7. The simulated scale is left out of the UI,** as a debug feature: out of the screen's list of makes and out of the web page's form (ADR-008). The API and the console keep it for the remote control.
- **D8. The screen echoes the unit's state,** as it does today, with marks for the remote control and the simulated scale (Status on every screen).
- **D9. ADR-010 is reworked:** the new menu is built directly, and better layouts are welcome where they make better use of the screen.

## Still open

- ~~**D5. The QR code** to open the page or to join the access point.~~ Moved to ADR-015 (the owner, 2026-11-17: "Perhaps that's a feature for future ADR. I'd like to design the commissioning process properly, not ad-hoc"). Until then the Wi-Fi page gives the address, the name and the access point's key as text, as today, and the screen offers no "more on the web" row: how a person finds the web is ADR-015's.
- **The beta channel** as proposed above, and the remote mark's 30 s.
- **How the look is mocked,** after the measurement (below).

## Designing the look: the menu on the PC

The owner, 2026-11-17: after the measurement, perhaps a better interactive mockup, as a tool for designing the UI without updating the unit.

Proposed: not a separate mock, but the menu's own code built for the PC as well.
- **What runs:** the tree, the navigator and the 12864's view, in the same C the unit runs. The services are replaced by a recording of a unit (as `tests/web_test.py` serves the pages from `tests/web_test_data.json`) or by a small simulation, such as a charge whose weight rises. The keyboard is the knob: the arrows, Enter and Esc.
- **Why the real code and not a drawing:** what the owner settles on the PC is what the unit draws, pixel for pixel, and nothing is built twice. The navigator's tests also run there in CI, and every page's picture can be made there on each PR, without a unit.
- **Two ways to run it,** chosen after the measurement:
  - in a browser, compiled with Emscripten. One toolchain covers Windows and Linux, and the owner, an agent's browser and anyone the owner sends a link to open it as a page;
  - as a window on the PC (SDL). That needs a host compiler on Windows, which `toolchain.env` does not name today.

  Recommended: the browser.
- **The library choice is not waiting on this.** Both LVGL and u8g2 run on a PC.
- **Layouts on the mock.** The owner asked for layouts that make better use of the screen; proposals are shown on the mock, side by side with today's. A starting point: today's title bar takes 13 of the 64 rows, and a list shows four rows.
