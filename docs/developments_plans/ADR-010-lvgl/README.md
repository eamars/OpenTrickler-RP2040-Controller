# ADR-010: LVGL in place of u8g2

Status: **Draft** 2026-11-13. Not approved. Captures what the screen code is today and ideas for the move. The sizes for LVGL are estimates until its first step measures them.

## What we want

- One UI stack on every board, which can also serve a larger screen later.
- Menus that are easy to add to.
- Services that do not draw.

## Owner's direction (2026-11-13)

- Migrate the UI library from u8g2 to LVGL.
- One implementation: no board keeps u8g2 while the others move.
- Issue #100 (the owner's): u8g2 is hard to use for the menu system; LVGL's menu widget could carry it; another OpenTrickler build supports a TFT35 screen.

## Why here

Last of the large changes: the screens then sit on a split application (ADR-006), run on both chips (ADR-007), and ADR-001's `menu_tour` has a baseline to compare against.

## What we know (checked 2026-11-13)

- The screen is a mini 12864: 128 × 64, one bit a pixel, a UC1701 controller on SPI at 4 MHz. Input is a rotary encoder, its button and a reset button. A setting rotates the display.
- The menu is u8g2's MUI: 22 forms described as one string in `mui_menu.c`, with a list of field functions.
- u8g2 or MUI is called from 9 files. Besides the menu, these draw for themselves: the charge screen and the cleanup screen (each its own task, redrawing every 20 ms), the Wi-Fi information, scale calibration, and the motor error screen.
- A mode takes the screen by the menu leaving its form and calling a function that blocks until the mode ends (`exit_state`, in `menu.c`).
- In the menu the reset button goes up one level (PR #110). MUI has no notion of a form's parent, so `menu.c` keeps a table of them by hand, beside the forms' own description in `mui_menu.c`.
- The web mirror serves u8g2's raw buffer.
- u8g2, MUI and their fonts are 32 KB of the Pico W image (`tools/size_report.py`).
- RAM on Pico W: 194 KB of 256 KB is taken at link time.
- The reference project runs LVGL 9.6 on its ESP32-S3, with skills for LVGL layout, widgets and embedded UI design in its `.agents/skills`.

**Handed on by ADR-012** (2026-11-15). Found by the audit of 2026-11-14, which read every module outside the motors for the kind of faults ADR-012 lists for them. Not read a second time. Nothing here was tried on a unit unless it says so.
- The buttons' interrupts are set up before the queue they write to: a press in the first moments of a start faults.
- A form names a field `SU` that is not defined: a unit label is never drawn.
- The display's lock functions are never called, and the web's mirror is served from the buffer being drawn: torn frames.
- The A&D's "CAL" sequence is offered for every make of scale, and its screen's strings are globals.
- "Wifi Info" opened before the radio is up reads a network interface that is not there yet.

**Handed on by ADR-004** (2026-11-16): the update's offer at the unit's screen. ADR-004 built the update and left its offer to this ADR and ADR-008. Until one of them ships it, an update is reached through the REST API only, for debugging, and is not for users. What the web page needs, the contract and the times measured are in ADR-008's hand-on; this is the screen's part.
- **What is there:**
  - The update's mode, `update_mode_menu()` in `update.c`. It holds the screen while the job runs, so nobody starts a charge at the unit meanwhile, and draws for itself with u8g2: "Firmware update"; "Waiting for the image", "Receiving n%", "Ready: <version>" or "Installing, restarting"; "Keep the power on."; "Press RST to stop." The reset button stops the job.
  - The job is started from the API, so `menu_tour` does not reach this screen.
  - `update_status()` gives everything the screen shows; the update's events say how it ended.
- **What the screens have to do:**
  - the update's screen rebuilt, read from `update_status()` rather than drawn by `update.c` (services stop drawing, below);
  - the offer: a newer release shown with its note, a critical one at once, as the Open Weapon Mount Computer's OTA mode does; install on a press;
  - once, at the start after an update, how it ended in plain words: confirmed, or back on the version it had;
  - the version running somewhere a person can find it (today the API gives it).
- **To find out first:**
  - **The screen alone needs the unit to fetch a release itself,** over HTTPS from GitHub, with TLS on the unit. On the Pico W that competes with LVGL for room: its image is 806.5 KB of its 1,016 KB slot, 209.5 KB left (the `.bin` with ADR-004's review fixes, 2026-11-16), against LVGL's estimate of 120 to 200 KB. Measure both before deciding the Pico W's screen offer. Without them, the screen can only show an offer a browser found (ADR-008).
- **An idea for the owner** (ADR-008's hand-on says why): a start from the web waits for a press of the encoder before anything is written. The update's screen is where that yes is asked.

## Ideas

- **Measure first.** One LVGL screen on each board, beside u8g2, only to read flash and RAM from the size report and `stats`. Estimate: 120 to 200 KB of flash and 30 to 50 KB of RAM. If Pico W cannot hold it, the owner decides Pico W's future there, by ADR-005's rule 5.
- **Services stop drawing.** Charge, cleanup, Wi-Fi, calibration and the motor check publish their state; screens read it and draw. This is the reference project's rule between services and UI. It comes before LVGL and is proven with u8g2 still in place.
- **LVGL on the same panel.** One bit a pixel, the whole frame sent as today. The encoder as LVGL's encoder input, so focus and editing come from LVGL.
- **Screens one at a time,** each against `menu_tour`'s snap of its form. The aim is that nothing is lost, not that pixels match: the owner uses each one.
- **A settings screen drawn from the settings' own description,** if ADR-003 brings one.
- **u8g2 and MUI removed** at the end, with their submodule.
- **The mirror** fed from LVGL's frame.
- **The reference project's three UI skills** copied here and fitted to this screen.

## How the unit shows it works

- `menu_tour` finds a screen for every form the baseline has.
- `charge_cycle` passes, and the charge screen shows the weight no later than it does today.
- The owner uses every screen by hand, with the encoder.
- The size report and `stats` show flash and RAM inside each board's budget.
- On each supported board.

## Not in this ADR

- A colour or touch screen. It is the reason to move, and it is a later ADR.
- New screens or a new menu order. First the same, then better.

## To settle before approval

- Pico W, once LVGL is measured on it.
- Whether "services stop drawing" is a step here or an ADR of its own.
