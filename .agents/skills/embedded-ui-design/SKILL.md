---
name: embedded-ui-design
description: Design rules for this firmware's screen, the mini 12864 (128 x 64 pixels, one bit, a passive LCD) driven by a rotary knob with a click and a reset button. Drawn from pixel-display practice (the Flipper Zero, a 128 x 64 design case study, small-screen layout guides, display legibility standards) and the owner's reviews. Use before proposing, building or reviewing any screen, page, dialog, job screen, icon or wording on the unit; when choosing fonts, sizes or spacing; when a screen is "too dense", "too wide", "cramped" or "hard to read"; and before adding animation. Mock-ups come first, with tools/ui_mock.
---

# Embedded UI design (128 x 64, one bit, knob and button)

**The device:**
- **The panel:** the mini 12864, 128 x 64 pixels, one bit, a UC1701 controller, a passive (STN) LCD with an RGB backlight. The glass is negative: a lit pixel shows light on a dark ground. The owner can invert the screen (`invert_display`), and the agent's hello is always shown inverted, dark on a lit glass.
  - Its active area is not measured here. A typical 2.4 inch 12864 is about 55 x 27.5 mm, about 0.43 mm a pixel; measure the unit's glass before relying on a figure in mm.
- **How it is read:** at a bench, from about 50 cm. A charge's weight is glanced at from further away.
- **Input:** a knob that turns (next, previous), its click (Enter, taken as it goes down), and the reset button (Back). Back is also a click on the title's "<". No long press, no double click, and no hint on screen teaching the controls: nothing hidden (the owner, 2026-11-17). The one exception is a job whose turn acts (rule 5): there the knob means something new, and its line says so.
- **Drawing:** LVGL 9 into the screen's one frame, only what changed, and the panel sent the pages that changed (`src/menu_view_12864.c`, `src/menu.c`, `src/display.c`); the navigator (`src/menu_nav.c`) fills a screen model in words and knows no pixels (ADR-010).

## Rules

1. **One family, Helvetica, in three sizes, and nothing else.**
   - Text: `helvR08`, for everything that is not a title or a figure.
   - Titles and emphasis: `helvB08`, its bold at the same size.
   - The figure: `helvB18`, for the one number a job screen is about. Its unit sits beside it, in the text face.
   - Hierarchy comes from size, weight and position, never from another face.
   - **Why:** the owner, 2026-11-17, chose "All Helvetica" from the whole design drawn in each candidate, after
     the first menu's mix of Helvetica with ProFont figures and bold hints was "a bit hard to read". One family
     answers that; `helvB08` is also the Flipper Zero's title face (its `canvas.c`), on the same kind of screen.
   - **What was tried and passed over, and why** (the drafts are in ADR-010's progress):
     - Lucida Sans: "a bit too wide on every screen" (the owner);
     - haxrcorp4089, the Flipper's text face: its "v" differs from its "u" by one pixel;
     - Nokia Small: slim and clean, but the owner found helvR08 and glasstown_nbp read better;
     - glasstown_nbp: taller and narrower, the clearest I, l and 1, but no bold (a third family for titles) and an
       unusual "M";
     - monospace faces (6x10, t0): as wide as Lucida.
2. **Check a face glyph by glyph before choosing it.**
   - Print the letters that are confused: `venv\Scripts\python tools\ui_mock\panel.py glyphs <font> "uvwIl10O"`.
   - A face whose u and v, or 0 and O, differ by a pixel is out.
   - **helvR08's known limit:** I and l are the same bar. Show strings where that matters (keys, ids) only where their context makes it clear.
3. **Sizes.**
   - Text capitals 8 px. At about 0.43 mm a pixel that is 3.4 mm, about 24 arcminutes at 50 cm. The display standards make 16 arcminutes the minimum and 20 the optimum (MIL-PRF-87268D, as summarised by Activu).
   - The figure's capitals 19 px, about 8 mm: legible at about 1.7 m by the same minimum.
   - Nothing smaller than the text face, ever.
4. **The grid.**
   - **The title band:** its baseline at 9, a rule at 11. Content starts at 13.
   - **Menus and lists:** rows of 13 px, four a screen.
   - **Dense pages:** lines of the text's height and 1 px: 11 px, four a screen. At 8 px capitals a dense page holds
     no more lines than a list; it is dense in that it has no focus and its lines run on, scrolled.
   - **Softkeys:** the bottom row, 53 to 63.
   - **Spacing:** no text nearer than 2 px to another, and 1 to 3 px from the panel's sides.
   - **Why:** the owner, 2026-11-17: "there should be enough clearance between two lines of text so the UI looks a bit clean".
5. **Seven kinds of screen, no others** (`menu_nav.h`; the seventh, the code, the owner allowed on 2026-11-18: "Allow it"). A new kind needs the owner's word.
   - **Pages** are lists: a title, then rows of label, value and ">" when the row opens something. The focused row is in reverse.
     - A number is set in its row: a click, a step a detent (the value in reverse), a click.
     - A choice opens its own page: every option, the current one with a check at its right (U+E002), the focus opening on it; a click picks and goes back. Never turned through blind in its row, and never marked by bold or a "*" alone: both too faint to see at a glance (the owner, 2026-11-17).
     - A row that only shows a value has no focus; every row that acts is reached by the turn.
   - **Notes** are read-only and dense: lines of 11 px, each a key at the left and its value at the right (a pair too wide for a line takes two, the value right on the second; the owner, 2026-11-17: otherwise the page looks unorganized), no focus on a line, a scroll bar when they run on, ending in their action, reached only by scrolling to it (Wi-Fi, the update's offer). Turned up past the top, the title is focused, which is Back, as on a list. A list's rows that are only read (Firmware's facts) take no focus instead.
   - **Dialogs** are a title, a message of one or two centred lines, and two softkeys (the cup's acknowledgement, a factory reset).
   - **Jobs:**
     - the title is the status ("Charging", "Lift the cup");
     - one figure, centred, its unit small beside it;
     - one centred line under it: the target while charging, the verdict after a throw;
     - two softkeys.
   - **The editor** is the target's alone: its digits as a figure, one selected, a click to set it; Cancel and Set.
   - **A job whose turn acts** (the cup's fill, ADR-009): the knob does the job's own motion instead of moving between the softkeys (each detent a quarter turn of the coarse tube from now, extending a pour under way and never adding to it, so input faster than the tube is dropped and the tube stops with the knob; nothing the other way), the click is its right softkey, drawn focused, and Back its left. Its context line says what the knob does ("Turn the knob to pour"): the one place a screen names a control, since the knob means something new there. **Why:** the owner, 2026-11-25: "tie the rotary button to the motion of the two tubes. So the rotary button will not control the speed, but position ... on the screen you should show what the user should do"; the mock-up with both exceptions approved as drawn the same evening.
   - **The control** is a job of rows, the cleanup's: its title Back, a line to read, rows set in place, two softkeys.
   - **The code** is a QR code a phone's camera reads (the set-up's, which joins the hotspot; ADR-008): the code at the left in a lit square of 64 x 64, dark modules on a light ground as a phone reads a code, however the panel is shown (an inverted panel has it drawn the other way round in the frame, ADR-008), its quiet zone 3 px; no title band; beside it a few lines saying what to do, and Back, the one softkey, at the right, where the code leaves room. Drawn at 2 px a module, a code holds version 3 (29 modules) at the most. Each code is checked by a reader on the drawn screen inside a dark bezel, and by a phone at the unit.
6. **Softkeys: one rule everywhere.**
   - The left softkey is what Back does (Cancel, Stop), the right the screen's action; on the code's screen, its one softkey, Back, stands at the right, the code taking the left. Both are reached by the knob: the focused one is drawn in reverse and a click does it (the owner, 2026-11-17: every element on screen is clickable). A dialog that destroys opens on Cancel.
   - They replace every private convention ("RST: stop", "[Zero]", "<Target>"). A screen whose click does nothing shows no right softkey.
   - **Gestures:** the turn, the click and the reset button only; no hold (rule of Input above). A design with more gestures is a different design (the 128 x 64 case study: three press variations at most).
7. **Remove before you shrink.**
   - A job screen leads with one figure, read from 50 cm, with its status and its softkeys. Nothing it showed or did moves to the web for want of room: every setting and action is on both the screen and the web, and only measured data or the hardware leaves one off (the owner, 2026-11-17; ADR-010's survey in `menu_design.md`). An earlier wording here ("Anything else moves to the web") cost the cleanup its weight, flow and gate.
   - **Why:** small-screen guidance says to relocate rather than shrink (Applied-EE), and the owner, 2026-11-17: "the information density is a bit on the higher side".
   - The timer, the powder's name and the throw's number left the charge screen this way in gate 4; the timer and the powder came back in gate 6 (the owner: no removal without a reason on record).
8. **The important thing in the middle.**
   - A job's figure and its context line are centred; the owner approved the charge screens laid out so ("you did a good job").
   - With the cup off the pan, the charge keeps the throw's weight and verdict, not the bare pan's reading.
9. **Every text has a width.**
   - A label and its value must fit in one 128 px row with 3 px between them, or the page is a card.
   - Never let two texts overlap: lay out so they cannot, and clip the left text as a safety net, the way the title clips for its marks.
   - Long values (names, addresses, notes) go in cards and wrap at a word; an ellipsis goes only at a word.
   - **Why:** gate 4's screens overlapped three times ("Check for update" and its value, the verdict over the status, "[Stop the motors]" over the gate line).
10. **Read before you act.**
    - Where an action has consequences (an update, an erase), it comes after what must be read.
    - The update's Install sits at the end of its notes and warnings, so it is reached only by scrolling through them; Back (Later) works throughout.
    - Erasing asks in a dialog whose message says it cannot be undone.
    - **Why:** the owner, 2026-11-17: Install "to the bottom of the page so the user will have to read the note and warnings properly".
11. **A scroll bar on everything that runs on.**
    - At the right edge: a 1 px track and a 3 px thumb, as long as the share shown. A dotted track was drawn first;
      dotted lines need LVGL's complex drawing, which is off to save flash, so the unit draws the track solid.
    - Content keeps 2 px from it, and a page never shows half a row.
    - **Why:** the owner accepted dense pages "with the scroll bar".
12. **Read-only pages have no focus on their lines.** A row in reverse means a click does something; their title, focused above the top, is Back as on every page.
13. **Icons on the pixel grid.**
    - At most 8 px tall in the title band, drawn pixel by pixel, never scaled.
    - **Remote:** a phone, while the unit is driven from elsewhere (the owner, 2026-11-18: icons, not text, for such marks; the phone over a monitor, a pointer and a link). SIM stays a word: only a developer sees it, and no icon at 8 px says "this weight is not real" as plainly.
    - **Wi-Fi:** full signal bars when joined (no strength: "most clear solution on the tiny screen", the owner, 2026-11-17), a hotspot as the unit's own access point.
    - They are glyphs of the text font in Unicode's private use (U+E000 and on; `scripts/bdf2lvgl.py`, ICONS): LVGL draws nothing below U+0020.
    - A passive or secondary fill is a checkerboard: one bit has no grey (the 128 x 64 case study).
14. **Words.**
    - What the person does is an imperative ("Lift the cup", "Return the cup"); what the unit does is a state ("Charging", "Zeroing"). A job's title shares the band with its time and marks: the shorter word wins.
    - Labels are short. Abbreviations only where standard (gr, g, rev/s, %, IP).
    - Budget characters, not pixels, for values that vary (the case study wrote "MAX" where "100%" did not fit).
15. **Safety stays visible.**
    - The simulated scale's mark (SIM) whenever it stands in.
    - The remote mark while the unit is controlled from elsewhere.
    - The cup's acknowledgement before every charge and cleanup: the owner ruled it a safety step, never to be cut as clutter (2026-11-17, ADR-010's menu design).
16. **Animation only where it means something, and only once measured.**
    - **Where it can earn its place:**
      - a list easing to its new row in two or three frames;
      - a slow mark while something waits (a poll).
    - **None while a charge pours,** whose figure must stay still and current.
    - **Why it must be slow:** the panel is a passive LCD. Such panels typically take on the order of 100 ms or more to settle a pixel, longer in the cold, so fast motion smears (general knowledge of STN panels, not measured on this one). Keep to a few frames a second and under about 300 ms a move.
    - **What is known of the cost** (the Pico W, ADR-010, gate 4; `screen`'s reply gives them): the whole screen draws in 38 to 43 ms, a charge's figure in 7 to 9 ms; sending the panel all 8 pages takes up to 9 ms, the 3 or 4 a figure touches 4 to 6 ms. Times on the wall clock, with the motors' and the network's tasks above the menu's. LVGL draws only what changed, so a small animated area costs a fraction of a frame.
    - **Set a style only when it changes.** LVGL redraws an object whenever one of its styles is set, even to the value it had, and the view is drawn at every refresh: a style set every time is a redraw every time (`view_reverse()` in `menu_view_12864.c`). Text goes through `view_text()`, which compares first.
    - **Before adopting any animation, measure:**
      - the Pico W's frame time;
      - the menu task's share of the CPU;
      - the motors' late feeds (`motor` on the console) while it runs;
      - the panel's smear, by eye on the unit.
    - **Why:** the owner, 2026-11-17: "open to animations if the refresh rate is acceptable and load is not affecting the controller performance".

## This project's design language

- **An overlay over a page is the screen's last child, opaque and as large as the screen.** LVGL then draws from it
  up and skips the page under it. On LVGL's top layer the page is drawn as well: the hidden hello's first overlay ran
  the heap out there and the core faulted, since LVGL's checks are off and a failed allocation is written through
  (ADR-010, 2026-11-17). An animated figure is one label of tiles, a pose a different set of tiles
  (`scripts/bdf2lvgl.py`, `:art`), rather than an object for each moving part; check the heap's free piece before
  making it, and let it go when it is short.
- **LVGL's heap is the view's budget** (`LV_MEM_SIZE`, 14 KB, `src/lv_conf.h`, and a 3 KB pool from the FreeRTOS heap, `src/menu.c`: 17 KB). It holds every object of the view,
  a frame's draw tasks while it draws, and a glyph's working buffer (384 bytes for an 18 px figure). When it runs
  short, LVGL silently skips the glyphs it cannot buffer: the redesigned view in 12 KB drew its figures as a lone "."
  and lost a title's first letter (2026-11-17). After adding objects to the view, read `screen`'s
  `lvgl_biggest_free_min` on every kind of screen; keep it above about 1.5 KB (2,656 bytes at 14 KB).
- **Fonts are BDF files in `fonts/`,** with their notice: the design's three, and helvR14 for the mock-up sheet's
  captions. The firmware's generator (`scripts/bdf2lvgl.py`) and `tools/ui_mock/panel.py` read the same files, so a
  mock-up is what the unit draws. A face to try is added there as its BDF file (u8g2's `tools/font/bdf` holds many;
  the firmware no longer depends on u8g2, ADR-010).
- **Faces:**
  - `helvR08`, text, caps 8;
  - `helvB08`, titles and emphasis, caps 8;
  - `helvB18`, figures, caps 19.
- **Trying another face:** `tools/ui_mock/menu_12864.py --text <font> --bold <font> --figure <font>` draws the whole
  design in it, every baseline and the dense line worked out from its height and the text wrapped by its widths.
- **The design as drawn:** `tools/ui_mock/menu_12864.py`, its constants the grid of rule 4, its sheet in `docs/developments_plans/ADR-010-lvgl/mockups/`.
- **Marks in the title, from the right:** Wi-Fi, then the remote mark, then SIM.

## Process

- **Design first, on paper the unit could draw.**
  - Change `tools/ui_mock/menu_12864.py` (or add a screen to it), draw the sheet, and look at it yourself against these rules before the owner sees it.
  - Mock-ups go to the owner before code: "The mock WAS supposed to be designed ahead of the deployment" (2026-11-17, after gate 4 was built ahead of its mock).
  - The owner decides by seeing: send the sheet, not a description.
- **Build to the mock-up,** then prove it on the unit:
  - `rc_run.py menu_tour --compare tests/menu_baseline/<board>` for the pages;
  - `rc_run.py reach`: every element of every screen (the "<" title, each row that acts, a notes' button, the softkeys, the target's digits) reached by turning alone. A new kind of screen tells the console what it has (`@screen ... back`, `@row ... info`) so the test needs no change;
  - snaps of each job screen through its states: `rc_run.py send` with `snap` lines, decoded from the session's log; a charge with the simulated scale, the hopper empty.
  - A change meant to alter a screen saves its baseline again in the same commit.
- **Look at every snap yourself:** overlaps, clipping and crowding show only in the picture.

## Relation to the other project skills

- `c-style` for the code.
- The LVGL mechanics live in `src/menu_view_12864.c` and ADR-010's `menu_design.md` (the tree, the keys, what leaves the screen). This skill decides how a screen looks and reads; `menu_design.md` decides what is on the menu and where. If they disagree on looks, this skill wins.

## Sources

- **The Flipper Zero's fonts** (helvB08, haxrcorp4089, profont11, profont22): its firmware, `applications/services/gui/canvas.c`, https://github.com/flipperdevices/flipperzero-firmware
- **The 128 x 64 case study** (hierarchy by size and position, three press variations, character budgets, a checkerboard fill, pixel-grid work checked on hardware): Marcin Dominik Jabłoński, "Designing for a 128x64 pixel display", UX Collective, 2018, https://uxdesign.cc/designing-for-a-128x64-pixel-display-3f3361aca7a0
- **Small-screen layout** (about four lines of medium text on 128 x 64, a 1 px rule under the status band, one primary value, relocate rather than shrink): Applied-EE, https://applied-ee.github.io/embedded/docs/screens-displays/ui-layout-patterns/small-screen-layout/
- **Character size by visual angle** (16 arcminutes minimum, 20 optimal, after MIL-PRF-87268D): Activu, "Optimal Font Size", https://activu.com/uploads/Optimal-Font-Size.pdf
- **The same guidance from a second source** (30 arcminutes for titles, 20 for critical items, 16 for static ones, after MIL-D-87213A): FHWA, https://www.fhwa.dot.gov/publications/research/safety/98057/ch03.cfm
- **The form of this skill:** the Open Weapon Mount Computer's `embedded-ui-design` skill.
