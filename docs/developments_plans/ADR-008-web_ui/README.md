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

It stands on ADR-003's server and on ADR-005's report of what a board carries. After ADR-007 it is built once and proven on both chips.

## What we know (checked 2026-11-13)

- `src/html/web_portal.html` is one page of 1,787 lines: markup, styles and script together.
- It loads Tailwind and DaisyUI from CDNs and its icon from GitHub. With no internet the page has no styling.
- `wizard.html` (252 lines) is self-contained. It is what `/` serves while the unit is its own access point.
- `display_mirror.html` draws the screen from the raw u8g2 buffer at `/display_buffer`. `plot_weight.html` is in the folder and is not served.
- Pages become C strings through `scripts/html2header.py`, unminified and uncompressed: 90 KB of the image.
- The page polls with `fetch`; nothing is pushed.
- `tests/web_test.py` is a small stand-in server (Flask) that serves the pages and answers a few endpoints with made-up values.
- PR #113 adds a second, separate page (928 lines) for powder analysis; PR #111 adds an update page inside the portal; PR #101 adds about 950 lines to the portal.

## Ideas

- **A mock first.** The pages, clickable in a browser against the stand-in server, before anything goes on a unit. The owner decides by using it. An agent can open the same mock in its own browser and check it.
- **Phone first.** Most use is a phone held beside the trickler.
- **Pages:** charge (target, live weight, start and stop), profiles, tuning with a chart of the throw (ADR-009), settings, update (ADR-004), and the set-up wizard.
- **Built from sources.** Separate files for markup, styles and script, joined and gzipped by a build step into what the unit serves. Whether that step may need Node, or stays in Python like `html2header.py`, is the owner's call.
- **No framework at run time, or a very small one.** The page is a handful of forms and a chart. A proposal with sizes comes with the mock.
- **Charts** by a small library or drawn by hand on a canvas; chosen by size.
- **A budget:** the whole UI under about 100 KB gzipped (a proposal; today's pages are 90 KB uncompressed).
- **Settings forms drawn from the settings' own description**, if ADR-003 brings one. A new setting then needs no page change.
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
