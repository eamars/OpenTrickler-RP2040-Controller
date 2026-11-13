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
- The web mirror serves u8g2's raw buffer.
- u8g2, MUI and their fonts are 32 KB of the Pico W image (`tools/size_report.py`).
- RAM on Pico W: 194 KB of 256 KB is taken at link time.
- The reference project runs LVGL 9.6 on its ESP32-S3, with skills for LVGL layout, widgets and embedded UI design in its `.agents/skills`.

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
