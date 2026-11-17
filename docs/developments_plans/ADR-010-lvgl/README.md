# ADR-010: the screen's menu designed again, on LVGL

Status: **Draft** 2026-11-13, rewritten 2026-11-17 with the menu's design ([menu_design.md](menu_design.md)) and the owner's answers to it. Not approved. Gate 1, the measurement, was run before approval, 2026-11-17: the Pico W fits, its flash with LVGL and mbedTLS at -Os inside the shipping build, its RAM with trims. Gates 2 and 4 were built the same night at the owner's word, ahead of approval, which the owner gives on PR #132, and proven on both units. Gate 3 (the mock) is dropped by the owner, since gate 4 was built ahead of it. Gate 5 (the update at the screen) was built and proven on the Pico W the next day; the owner skipped the Pico 2 W's run, the lower-risk target ([progress.md](progress.md)). Gate 4 was opened again: it had left u8g2 in as the panel's driver and frame buffer, and the owner's review sent it back to remove u8g2 entirely. Built and proven on the Pico W the same day. Left: the owner's look at both panels, the Pico 2 W's run, the owner's use by hand, and the approval.

## What we want

- One UI stack on every board, which can also serve a larger screen later.
- A menu made for the knob and a button: only what a person needs at the bench is on it, and the setting up is on the web.
- Services that neither draw nor read buttons. The screen is a client of the same commands and status as the API.
- The update offered at the screen, from the unit's own poll.

## Owner's direction

2026-11-13:
- Migrate the UI library from u8g2 to LVGL.
- One implementation: no board keeps u8g2 while the others move.
- Issue #100 (the owner's): u8g2 is hard to use for the menu system; LVGL's menu widget could carry it; another OpenTrickler build supports a TFT35 screen.

2026-11-17:
- Design the menu's logic before any code:
  - what is nested under what;
  - what is left out of the screen and reached only from the web;
  - how a person with only the rotary encoder and a button drives it;
  - the update's poll and its offer.
- Drop what the menu does only to get round MUI, such as the Wireless menu's extra OK.
- A split like MVC, so a 3.5" TFT, an AMOLED like the Open Weapon Mount Computer's, or a higher-resolution TFT can come later.
- The design's answers, each in [menu_design.md](menu_design.md):
  - **the warnings before charge and cleanup stay.** They are safety warnings: the person acknowledges that the cup is on the scale;
  - **a change at the screen is stored when it is confirmed;**
  - **storing the target is ADR-014's;**
  - **LVGL and TLS are measured first,** and the owner is told if the Pico W does not fit. Trimming LVGL to fit is acceptable, and what is trimmed goes into the tiered feature table, to be decided when a larger screen comes;
  - **the simulated scale is out of the UI,** as a debug feature;
  - **the screen echoes the unit's state,** as today, and perhaps shows an icon for the remote control;
  - **perhaps a pre-release channel** for the update, as phones have a beta channel;
  - **the rework is agreed:** the new menu is built directly. A better layout is welcome wherever it makes better use of the screen;
  - **perhaps an interactive mockup** after the measurement, as a tool for designing the UI without updating the unit;
  - **the web page's missing acknowledgement** is documented in ADR-008;
  - **the QR code goes to a later ADR** on commissioning (ADR-015): "I'd like to design the commissioning process properly, not ad-hoc".
- **Update tests use a local OTA server,** so they do not hammer GitHub's releases. A debug image goes onto a unit by the probe.
- **After gate 1: LVGL and mbedTLS are compiled at -Os in the shipping build,** in place of ADR-005's whole-image optimised build ("Yes, libraries at -Os"). Our own code stays at -O0. ADR-005's trigger therefore does not fire for this ADR: the Pico W's image is 971 of 1,016 KB.
- **The scale's calibration is removed:** "The calibration should be removed as no scale supports that".
- **Overnight, one PR** (2026-11-17): "prioritize on everything you could do without my involvement first, then ask for my approval tomorrow ... focus on the functional build if no technical blockers". So the gates are built before this ADR's approval, which the owner gives on the PR.
- **Gate 3 is dropped** (2026-11-17, the next morning): "The mock WAS supposed to be designed ahead of the deployment. Now given you made changes to the firmware directly then there is no point of designing the mock anymore." Gate 4 was built ahead of it overnight, which was a mistake: the mock was where the owner settles the layouts before they are built. The owner settles them on the unit instead.
- **Gate 5 goes ahead, with the beta channel as proposed** (2026-11-17: "Yes continue with gate 5").
- **u8g2 goes entirely, in gate 4** (2026-11-17, the owner's review of the plan): "this iteration of the work should remove the dependency of u8g2, including the frame buffer, screen driver, draw worker etc ... We may reuse the font from u8g2, but that should not become the reason to keep that dependency." Gate 4 as built kept u8g2 as the panel's driver and frame buffer, against its own line here ("all of u8g2 if LVGL is chosen").
- **The screen's frame over the web keeps no old format:** "There is no backward compatibility for screenbuffer distribution over the ethernet so you can change off one go."
- **One frame buffer, not two,** as recommended (below); the owner: "I believe the gate 4 will inherently optimize for performance and RAM utilization. Let's write first, and report if you noticed any discrepancy."
- **The screen's colours as the glass shows them** (2026-11-17, the owner's look at the panel). The mini 12864's glass is negative: a lit pixel shows light on a dark ground, the opposite of what the mock-ups, the snaps and the skill drew. The owner: "The software is not released, so it is the chance to add feature we need. Therefore I'd like to add the settings first, then fix the tool, then fix the AI hello display to bright mode permanently, as the dark mode just looks strange." A unit starts with the glass's own look, light on dark (the owner's choice of the two). The hello keeps its flash and then holds bright: the owner asked for an opinion, and the flash is what marks a new agent whichever look is set. Both units' glasses are the same (the owner).
- **No long press; a click acts as the knob goes down** (2026-11-17, the owner at the unit, finding Back slow): "This is not a touch screen, and for this application no long press is expected as the button is not reliable (therefore I have the debounce). So I think for any click event short click should be made default." And: "The 'back' feature should either a short click on '<', or reset button. Any hidden feature is a NOT a good UI design. And do not try to educate the user." menu_design.md's principle 2 (the knob alone does everything) gives way: a dialog, the number editor and a running job are left by the reset button.

## Why here

First of the work on the current Pico platform (the owner, 2026-11-17: 010, 008, 009, 014, then 006 and 007 together). The screen comes before the web UI, so the unit's poll and the update's job are built here, and ADR-008's page shows the same offer. ADR-001's `menu_tour` has a baseline to compare against.

## What we know (checked 2026-11-13 and 2026-11-17)

- The screen is a mini 12864: 128 × 64, one bit a pixel, a UC1701 controller on SPI at 4 MHz. Input is a rotary encoder, its button and a reset button. A setting rotates the display.
- The menu is u8g2's MUI: 22 forms described as one string in `mui_menu.c`, with a list of field functions. What MUI forced is listed in [menu_design.md](menu_design.md) (W1 to W6).
- u8g2 or MUI is called from 10 files (9 on 2026-11-13; ADR-004 added the update's screen). Besides the menu, these draw for themselves: the charge screen and the cleanup screen (each its own task, redrawing every 20 ms), the Wi-Fi information, scale calibration, the update and the motor error screen.
- A mode takes the screen by the menu leaving its form and calling a function that blocks until the mode ends (`exit_state`, in `menu.c`). The modes read the buttons themselves (`button_wait_for_input`).
- In the menu the reset button goes up one level (PR #110). MUI has no notion of a form's parent, so `menu.c` keeps a table of them by hand, beside the forms' own description in `mui_menu.c`.
- The buttons are read on their falling edge only, with a 250 ms lockout (`mini_12864_module.cpp`): a hold cannot be told from a click.
- The target weight lives in RAM and is 0 after every start (`charge_weight_digits`).
- The web mirror serves u8g2's raw buffer.
- u8g2, MUI and their fonts are 32 KB of the Pico W image (`tools/size_report.py`).
- RAM on the Pico W: 224 KB of 256 KB taken at link time (2026-11-17, on either build; 194 KB on 2026-11-13).
- The reference project runs LVGL 9.6 on its ESP32-S3, with skills for LVGL layout, widgets and embedded UI design in its `.agents/skills`. Its UI bridge holds the services' state for the views, and its settings are pages of rows (`settings_ui.h`).

**Handed on by ADR-012** (2026-11-15). Found by the audit of 2026-11-14, which read every module outside the motors for the kind of faults ADR-012 lists for them. Not read a second time. Nothing here was tried on a unit unless it says so.
- The buttons' interrupts are set up before the queue they write to: a press in the first moments of a start faults.
- A form names a field `SU` that is not defined: a unit label is never drawn.
- The display's lock functions are never called, and the web's mirror is served from the buffer being drawn: torn frames.
- ~~The A&D's "CAL" sequence is offered for every make of scale, and its screen's strings are globals.~~ The calibration is removed, its mode, its menu form and its activity with it: "The calibration should be removed as no scale supports that" (the owner, 2026-11-17).
- "Wifi Info" opened before the radio is up reads a network interface that is not there yet.

**Handed on by ADR-004** (2026-11-16): the update's offer at the unit's screen. ADR-004 built the update and left its offer to this ADR and ADR-008. Until one of them ships it, an update is reached through the REST API only, for debugging, and is not for users. What the web page needs, the contract and the times measured are in ADR-008's hand-on; this is the screen's part.
- **What is there:**
  - The update's mode, `update_mode_menu()` in `update.c`. It holds the screen while the job runs, so nobody starts a charge at the unit meanwhile, and draws for itself with u8g2: "Firmware update"; "Waiting for the image", "Receiving n%", "Ready: <version>" or "Installing, restarting"; "Keep the power on."; "Press RST to stop." The reset button stops the job.
  - The job is started from the API, so `menu_tour` does not reach this screen.
  - `update_status()` gives everything the screen shows; the update's events say how it ended.
- **The owner's direction** (2026-11-16):
  - The screen only polls the update source, which the web page sets (ADR-008); it sets no source and takes no file.
  - The update is offered from a new row in the main menu, as the Open Weapon Mount Computer does. Its row "Update available" sits at the top of its settings hub, shown only once a newer firmware is found, and opens the offer.
  - Never a pop-up at every start: it is annoying.
  - No authentication for now (ADR-008's hand-on).
- **What the screens have to do:**
  - **the row:** in the main menu, shown only while the unit's poll knows of a newer release, and opening the offer: its version and note, Install and Cancel. A critical release changes the row's text ("Important update", as the Open Weapon Mount Computer's offer is titled), still with no pop-up. That last part is recommended here: the Open Weapon Mount Computer opens a critical offer at once, and the owner's rule leaves that out;
  - **polling:** the unit polls its source once each start after Wi-Fi joins, quietly, as the Open Weapon Mount Computer does; the offer can also ask again. The poll is built here now (Handed on by ADR-008, below);
  - the update's screen rebuilt, read from `update_status()` rather than drawn by `update.c` (services stop drawing, below);
  - the version running, and how the last update ended (confirmed, or back on the version it had), where a person can find them, with no pop-up.
- **To find out first:**
  - **The poll needs the unit to fetch a release itself,** over HTTPS from GitHub, with TLS on the unit. On the Pico W that competes with LVGL for room: its image is 799.3 KB of its 1,016 KB slot, 216.7 KB left (the release 1.20.218, 2026-11-16), against LVGL's estimate of 120 to 200 KB. Measure both before deciding the Pico W's offer. A source on the owner's network served over plain HTTP needs no TLS.

**Handed on by ADR-005** (2026-11-17, closed as a scoping ADR):
- **LVGL is measured on two builds,** the shipping one and a size-optimised one (MinSizeRel), and the Pico W decided on the second. At `-O0` its size and speed would both be overstated. If it fits only optimised, ADR-005's optimised build is made first.
- **RAM is the Pico W's tighter limit:** 224 of 256 KB taken at link time on either build, 112 KB of it the FreeRTOS heap, whose high-water mark is not known. This ADR's first step measures it, beside LVGL's buffers.

**Handed on by ADR-008** (2026-11-17, with the new order): the screen now comes first, so the update's firmware moves here from ADR-008:
- the unit's poll of its source;
- the download into the free slot, as a job;
- HTTPS and its cost, measured here;
- the stored setting for the source, with the channel beside it (menu_design.md).

ADR-008 keeps the page's offer, the page where the source and the channel are set, and the upload by hand. PR #111 closes at ADR-008's end, once the whole flow is shown from the offer to the unit confirmed (the owner).

**u8g2 after gate 4** (read in the code, and from the Pico W's build at 7c41e28, 2026-11-17):
- **What it still does:**
  - it drives the panel: the UC1701's reset and set-up, its contrast and flip, and the sending of each frame (`mini_12864_module.cpp`);
  - it holds a second copy of the frame: its own buffer of 1,024 bytes and its handle of 148, beside LVGL's frame of 1,032 (`display.c`, `menu.c`);
  - the flush copies each LVGL frame into that buffer, pixel by pixel, in our code at -O0 (`menu_flush`);
  - the mirror and `snap` read that buffer, and the API names its format `u8g2_tiles`;
  - the fonts' BDF files are read from its submodule at build time, and the mock-up tool reads its fonts and its compiled catalogue.
- **It takes 4.2 KB of the Pico W's flash.** Five files still include its header and use nothing of it (`access_point_mode.c`, `app.c`, `charge_mode.cpp`, `cleanup_mode.cpp`, `update.c`).
- **The Pico W's image** is 965.3 KB of its 1,016 KB slot, and RAM at link time 250.2 KB of 256 (252.4 KB with the C library's 2 KB minimum heap).
- **LVGL has no driver for this panel.** Its display drivers are for colour TFTs (ST7735, ST7789, ST7796, ILI9341 and others); a TFT later uses one of them.
- **LVGL draws the whole screen for any change** in the render mode gate 4 chose (`LV_DISPLAY_RENDER_MODE_FULL`; `lv_inv_area()` in `lv_refr.c`). In its direct mode, with the same one screen-sized buffer, it draws only the areas that changed, and the buffer still holds the whole frame. For a one-bit display LVGL rounds each area out to whole bytes itself.
- **The panel keeps its own copy of the frame** in the controller's memory and shows it with no help. LVGL's second buffer gains only when a frame is sent by DMA while the next is drawn, and the send is about 2 ms.
- **Where a frame's time goes** (the Pico W's is not measured):
  - LVGL drawing the whole screen: 7.1 ms on the Pico 2 W at -Os (gate 1);
  - the copy into u8g2's buffer: about 3 ms on the Pico W, estimated for code at -O0;
  - the send, 1,048 bytes at 4 MHz: 2.1 ms, computed;
  - a passive LCD typically takes 100 ms or more to settle a pixel (not measured on this panel).

  The glass, not the chip, bounds how fast an animation can move.
- **The fonts are not u8g2's work.** helvR08, helvB08 and helvB18 are Adobe's and Digital's X11 Helvetica, which u8g2 carries with their permission notice: their copyright notices go in every copy, and they and the permission notice in the documentation that comes with it.

## What we do

The design is [menu_design.md](menu_design.md): the principles, the knob's four keys, the tree, the jobs' screens, the status marks, what leaves the screen and where it goes, the update with its channel, and the split. Built in gates, each approved and built on its own:

1. **Measure.** This gate runs before approval.
   - **The local OTA server first,** a host tool after the Open Weapon Mount Computer's `scripts/ota_host.py`. It serves `release\` (from `build_release.py`) in GitHub's layout (`releases/latest/download/`, `releases/download/<tag>/`) over HTTP and HTTPS, prints every request, and can serve a made-up newer manifest for tests. The unit reaches it through its source setting, not by a name it always tries first, so no test path is left in a user's unit. Every update test from here on uses it, never GitHub's releases (the owner).
   - **What is measured:** LVGL trimmed to what a 128 × 64 mono screen shows, with one list screen beside u8g2; and TLS, fetching one file over HTTPS from the local server with a certificate chain of the same kind as GitHub's.
   - **How:** on both boards, on the shipping build and on MinSizeRel. Flash comes from the size report; RAM from the link and from `stats`, which gives the FreeRTOS heap's high-water mark.
   - **One fetch from GitHub,** at the end, confirms the figure against GitHub's own chain.
   - **The report:** what was trimmed is listed for the feature table, and a report goes to the owner if the Pico W does not fit.
2. **Services stop drawing and reading buttons.**
   - The mode loops leave the menu's task for a job task of their own.
   - The screen sends the API's commands, `by` button, and reads the same status the API reads.
   - A job started anywhere takes the screen; the `view` activity goes.
   - The screens stay as they are, so `menu_tour` compares against today's baseline and `charge_cycle` passes.
3. ~~**The menu on the PC.**~~ Dropped by the owner, 2026-11-17: gate 4 was built ahead of it, so the layouts are settled on the unit.
   - The tree as a table, the navigator, and the 12864's view, built for the PC as well, fed by a recording of a unit. The owner settles the layouts there by using them, before the unit's screens change.
   - The way it runs (a browser page by Emscripten, or a window by SDL) is chosen after gate 1. The browser is recommended.
4. **The menu on the unit.**
   - The input driver's four keys, read on both edges.
   - The new tree and screens, and the status marks.
   - `screen` and `key` for the remote control, and `menu_tour` walking the tree from its table to a new baseline.
   - MUI removed: all of u8g2 if LVGL is chosen, and the mirror fed from the new view.
   - **u8g2 removed entirely** (the owner, 2026-11-17). Gate 4 was first built with u8g2 left in as the panel's driver and frame buffer; this finishes it:
     - **The panel's driver is our own,** on the SDK's SPI: the UC1701's reset, its set-up, contrast and flip, and the frame sent as 8 pages of 8 rows. Its commands are those u8g2's driver sends; a flipped panel starts 4 columns in, since the controller has 132. The frame's lock is made before any task runs (`menu_init()`), so no task can ask for it first.
     - **One frame, LVGL's,** in direct mode: LVGL draws only what changed into its one screen-sized buffer. No second buffer: the panel holds the frame it shows, and the send is too short for a DMA send beside the next frame's drawing to gain anything.
     - **The send** covers the 8-row bands a change touches, each turned into the panel's columns as it goes. The whole frame is sent again once a second, so a glitch on the panel's cable does not stay on the glass.
     - **The mirror and `snap` read LVGL's frame,** under a lock the menu holds while LVGL draws, so each is one whole frame. Its format is LVGL's: 64 rows of 16 bytes, the leftmost pixel the top bit, 1 a lit pixel (the layout of a PBM image's body). The API's `format`, the mirror's page and `rc_run.py`'s snap change with it, and no old format is kept (the owner).
     - **The fonts' BDF files are the repository's own,** in `fonts/` with their notice; `scripts/bdf2lvgl.py` reads them there. The mock-up tool reads the same folder. Its reader of u8g2's compiled catalogue goes: a face is tried by adding its BDF.
     - **Then u8g2 goes:** the submodule, its CMake lines, its part in the size report, the five includes, and every page that names it as the driver or the source of the fonts (AGENTS.md, the skills `c-style` and `embedded-ui-design`, api_v1.md).
     - **The frame's times are reported:** `screen`'s reply gives the last frame's drawing and sending times, and the longest of each since the start. An animation is later judged against them (the skill `embedded-ui-design`).
   - **The screen inverted, as a setting** (the owner's look at the panel, 2026-11-17):
     - `invert_display` in `settings/display` ("Invert the screen", applies now), and its row, Invert screen, on the Display page beside Invert knob. It is the panel's own command (`0xA7`), so the frame is untouched.
     - **The display's stored struct keeps its size:** the field lands in its filler (byte 6 of 8), so nothing resets and the flag starts as the byte stored there. On the Pico W it started on, and was stored off. Left as it is by the owner, who keeps this change to the inversion; handed on to ADR-006.
     - The menu's task alone sends the panel's inversion: the setting, or during a hello its flash and then bright, whatever the setting.
     - The mirror and `snap` show the panel as a person sees it: the frame with the inversion applied, a 1 a lit pixel. The mirror's page, `rc_run.py`'s pictures, the mock-up sheet and the skill draw a lit pixel light on a dark glass. The baselines' pictures are flipped to match, once, by a script.
5. **The update at the screen.**
   - The stored channel and source, and the poll at each start and on asking.
   - The download into the free slot as a job.
   - The offer's row and page, the update's screen, and its result on the Firmware page.

## How the unit shows it works

- `menu_tour` visits every page of the tree from its table, and its text and snaps become the new baseline. Each of the 22 old forms is accounted for in menu_design.md's table: moved, kept, or web only by decision.
- `charge_cycle` passes, and the charge screen shows the weight no later than it does today.
- A charge and a cleanup started over the API take the screen, which marks them as remote. A stop at the knob ends them.
- The owner uses every screen by hand with the knob, once with the reset button and once without it.
- An update found by the poll on the local OTA server, on each channel, is offered at the screen, installed and confirmed. One that does not start comes back, and the offer says so. The images under test go onto the unit by the probe; only the update itself goes over the air.
- The size report and `stats` show flash and RAM inside each board's budget.
- **With u8g2 gone:**
  - the size report has no u8g2, and the build reads nothing under `library/u8g2`;
  - `menu_tour --compare` passes against the baseline taken before, so LVGL's frame and the new snap are the same picture. The Pico 2 W has none since gate 5: its baseline is saved first, on the build with u8g2, and then compared;
  - the owner looks at the panel itself on both units, the Pico W stored at 180 degrees and the Pico 2 W at 0: a snap and the mirror read the frame in RAM, never the glass, so only an eye sees a wrong set-up or a flip 4 columns off;
  - the mirror's page shows the screen, in a browser on the unit;
  - `screen` gives the frame's times on the Pico W, for a page opened and for a charge's figure, and `motor` shows no late feed meanwhile;
  - `charge_cycle` passes.
- **With the screen's inversion:** `menu_tour --compare` passes with the setting off against the flipped baseline; with it on, a snap and the mirror show the page inverted and the panel shows it bright; a hello shows bright with either; the owner looks at the panel.
- On each supported board.

## Not in this ADR

- A colour or touch screen. It is the reason to move, and it is a later ADR; this one's split is made ready for it.
- Storing the target weight (ADR-014, with the charge profile).
- The web page's offer, its acknowledgement before a start, and the upload by hand (ADR-008).

## To settle before approval

- ~~The Pico W, from gate 1's figures.~~ Settled 2026-11-17: the libraries at -Os (the owner); the three RAM trims are built with the poll and measured there (gate 5).
- ~~The beta channel as proposed~~ (the owner, 2026-11-17: yes), and the remote mark's 30 s.
