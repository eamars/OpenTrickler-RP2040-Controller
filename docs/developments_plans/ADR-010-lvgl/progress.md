# ADR-010: progress

## Gate 1: what LVGL and TLS take (2026-11-17)

Run on the Pico 2 W through the probe. The Pico W's figures come from its builds and from the Pico 2 W's run-time figures. The owner: "Based on Pico 2W stats you SHOULD be able to compute the Pico W usage right? Performance is lesser a concern, the space does." The memory a library takes does not change with the chip: both are 32-bit, mbedTLS holds its numbers in 32-bit words on both, and the code and its configuration are the same.

The code is on branch `adr010_gate1`, not yet merged:
- **The measure build,** `-DADR010_MEASURE=ON` (`measure/`). It runs beside today's firmware and is driven from the console:
  - `lvgl` draws a settings page into a frame of its own, never sent to the panel;
  - `tls get <url>` fetches one file over HTTPS, in a task of its own, with every mbedTLS allocation counted.
  Nothing of it is built otherwise.
- **The local OTA server,** `tools/ota_server.py`. It serves a folder of releases in GitHub's layout; its main host redirects a download to an asset host, and the test certificate chains are of GitHub's two kinds.

### What was built

- **LVGL 9.6.0** (2026-09-16), a submodule, trimmed by `measure/lv_conf.h` to a 128 × 64 one-bit screen.
  - **On:** the label, button, list, bar and image widgets, the mono theme, flex layout, and the 8-pixel `unscii_8` font.
  - **Off, for the feature table:**
    - drawing in any colour format but one-bit (I1 and A8), and the complex drawing (rounded corners, shadows, masks);
    - the default and simple themes, the grid layout, and the Montserrat fonts;
    - observers, object IDs and property names, argument checks and the log;
    - the widgets animimg, arc, arclabel, buttonmatrix, calendar, canvas, chart, checkbox, dropdown, imagebutton, keyboard, led, line, menu, msgbox, roller, scale, slider, span, spinbox, spinner, switch, textarea, table, tabview, tileview and win.
  - **Its own heap** is 12 KB, static.
- **mbedTLS 3.6.6** (the SDK's), as a TLS 1.2 client (`measure/measure_mbedtls_config.h`):
  - ECDHE with ECDSA or RSA, and AES-GCM;
  - the curves P-256 and P-384, and RSA up to 4096 bits;
  - roots given as PEM;
  - a 16 KB input record and a 2 KB output one;
  - the NIST curves' fast reduction and the assembly multiply;
  - no clock, so certificates' dates are not checked.
- **The roots trusted:** the local server's two test roots, and GitHub's two:
  - USERTrust ECC, for `github.com`'s ECDSA chain (Sectigo);
  - ISRG Root X1, for its asset host `release-assets.githubusercontent.com`, whose RSA chain is Let's Encrypt's (YR1).

  Both chains were read 2026-11-17.
- **Optimisation:** LVGL and mbedTLS are compiled at -Os in every build, as `sha256.c` is already optimised for speed (ADR-004): this is third-party code nobody steps through. Our own code stays at the Debug build's -O0.

### Flash (`tools/size_report.py`, which now names LVGL and mbedTLS)

| Board, build | Today | With LVGL and TLS | LVGL | mbedTLS | Slot |
|---|---|---|---|---|---|
| Pico W, Debug (shipping) | 799.4 KB | **971.2 KB** | 86.6 KB | 69.8 KB | 1,016 KB |
| Pico W, Debug, the libraries at -O0 | 799.4 KB | 1,083.7 KB: over by 70,600 bytes | 189.4 KB | 79.8 KB (at -O2) | 1,016 KB |
| Pico W, MinSizeRel | 582.5 KB | 748.3 KB | 86.6 KB | 66.6 KB | 1,016 KB |
| Pico 2 W, Debug (shipping) | 775.5 KB | 939.8 KB | 83.2 KB | 65.8 KB | 1,792 KB |
| Pico 2 W, MinSizeRel | 580.0 KB | 739.6 KB | 83.2 KB | 62.6 KB | 1,792 KB |

- The mbedTLS column includes the 2.5 to 3.8 KB the image's SHA-256 already takes.
- The measure code and the roots as PEM add another 9 to 12 KB.
- **On the Pico W, u8g2 and MUI take 31.6 KB.** They go when LVGL replaces them, which leaves about 76 KB of the slot.

### RAM at link time

| Board, build | Today | With LVGL and TLS |
|---|---|---|
| Pico W, Debug | 223.9 KB | 243.8 KB of 256 KB |
| Pico W, MinSizeRel | 221.9 KB | 241.7 KB of 256 KB |
| Pico 2 W, Debug | 224.5 KB | 244.3 KB of 512 KB |

The added 19.9 KB is:
- LVGL's heap (12 KB) and its frame (1 KB);
- mbedTLS's contexts;
- the measure's own buffers.

On the Pico W, 12.2 KB is left for the C library's heap.

### At run time (measured on the Pico 2 W; the same on the Pico W)

- **The FreeRTOS heap** (112 KB) has 60,352 bytes free at rest on the Pico 2 W at 2.0.2. The Pico W had 59,216 free at rest at 1.20.194, its last reading, and 55,896 at its lowest in a soak with charges and two browsers.
- **LVGL,** a settings page of seven rows. It renders (![the frame](gate1_lvgl_frame.png); the colours in the picture are its palette, inverted).
  - **Its heap** held 7,636 bytes at most.
  - **Its task** has a 6 KB stack and used 4.0 KB of it at -Os. It took 6,480 bytes of the FreeRTOS heap.
  - **Speed:** a full frame in 7.1 ms at -Os and 36 ms at -O0; LVGL starts in 49 ms at -Os and 261 ms at -O0.
- **TLS,** one fetch through the main host's redirect to the asset host, with LVGL running:
  - **mbedTLS's peak** was 38,788 bytes with the local test chains and 40,740 with GitHub's. All of it was given back after each fetch.
  - **The fetch's task** has an 8 KB stack and used 2.2 KB of it.
  - **The FreeRTOS heap's low mark** fell from 53,872 bytes free to 5,376 with the local chains, and to **3,120 with GitHub's**. A fetch therefore takes 48.5 to 50.8 KB of it.
  - **Handshakes (the ECDSA chain, then the RSA chain):**
    - 148.4 s and 31.3 s with mbedTLS at -O0;
    - 19.8 s and 3.9 s at -Os without the NIST reduction and assembly multiply;
    - **3.3 s and 0.9 s** at -Os with them;
    - 7.6 s and 2.0 s in the Debug build with mbedTLS optimised;
    - 4.7 s and 1.1 s against GitHub itself, whose ECDSA chain is one certificate longer.
  - **The 795 KB update image** took 9.2 s over HTTPS at -Os, 15.7 s in the Debug build, and 0.96 s over plain HTTP.
- **GitHub was fetched once:** v2.0.2's manifest, 1,885 bytes, two hops. Every other fetch went to the local server (the owner's rule).

### The Pico W, computed

- **Flash fits** in the shipping Debug build, with the third-party libraries at -Os: 44.8 KB left, about 76 KB once u8g2 and MUI are gone. ADR-005's whole-image optimised build is not needed for this.
- **RAM does not fit as measured; it does with three trims.**
  - **As measured:** 59.2 KB free at rest, minus LVGL's task (6.5 KB) and a fetch from GitHub (50.8 KB), leaves about 2 KB at the fetch's peak with the unit at rest. Under a soak's load it is about 1.4 KB short, and the update job's 4 KB flash sector, held while it writes the free slot, comes on top.
  - **The trims,** each sized from a measured figure:
    - the fetch's task stack from 8 KB to 4 KB (2.2 KB used): 4 KB back;
    - LVGL's task in place of the menu's, whose 4 KB stack goes, with a 5 KB stack of its own (4.0 KB used): 5.5 KB back against the measure build;
    - only the roots the source needs, here GitHub's two: about 3.3 KB back. A plain-HTTP fetch, which still parses the four roots and seeds the random numbers, peaked at 8,392 bytes; the test roots are 1,839 of the four roots' 3,889 bytes of DER.
  - **With them,** about 14 KB is left at the fetch's peak at rest (1.9 + 4 + 5.5 + 3.3), and about 7 KB under a soak's load with the update's sector held. That margin is thin and computed, not measured: the poll's gate (5) measures it on the Pico W.
- **Speed is not measured on the Pico W.** Its Cortex-M0+ at 133 MHz multiplies slower than the M33, so its handshakes will take several times the Pico 2 W's. A background poll at start can wait that long.

### Decided

- **The owner, 2026-11-17:** LVGL and mbedTLS are compiled at -Os in the shipping build ("Yes, libraries at -Os"), in place of ADR-005's whole-image optimised build.
- **Gate 1's code** goes in with gates 2 and 4 by one PR, #132 (the owner, 2026-11-17: "I'd like to see one PR"): the measure build, the local OTA server, the size report's new parts, and `rc_run.py send`'s `pause`.

### Found along the way

- **`sscanf` costs 30 KB.** newlib's brings its wide-character tables (`jp2uc`, `categories`). The measure code first used it to read an HTTP status line; firmware code should not use it.
- **mbedTLS at -O0 is unusable,** and its assembly multiply does not compile there ("impossible constraints"). Hence the -Os.
- **The local server stalled.** It did each TLS handshake inside `accept()`, on its one accepting thread. A unit flashed mid-handshake held that thread, and every later connection waited. Each handshake now runs in its request's own thread, with a timeout.
- **LVGL's mono theme pads for large screens:** one row fits in 64 pixels. The 12864's styles are our own (gate 3).
- **The console's line is shorter than a long URL,** so the measure fetches by short paths, which the server accepts under any owner and repository. The poll takes its addresses from a setting, not from the console.
- **After gate 1 the Pico 2 W went back on main's firmware,** 2.0.2-17 (the code of v2.0.2; the commits since are documents). Its settings were unchanged against the copy taken before (`logs/adr010/pico2w/before_gate1/`, `rc_run.py settings --compare`). The Pico W was not on the probe and still runs 2.0.2.

## The owner's word overnight (2026-11-17)

"Can you prioritize on everything you could do without my involvement first, then ask for my approval tomorrow? I'd
like you to focus on the functional build if no technical blockers." So gates 2 and 4 were built that night, ahead of
this ADR's approval, which the owner gives on the PR. Gate 3 (the mock, which the owner settles by eye) and gate 5 (the
poll, below) were left.

Also that night: "The calibration should be removed as no scale supports that." Its mode (`and_scale_calibrate.c`),
the A&D driver's CAL key, the menu's form 51 and its row, and the `scale_calibration` activity are gone, with their
place in the API's schema and in api_v1.md.

## Gate 2: the modes in a job task (2026-11-17)

- **Built:**
  - `jobs.c`: the charge, a cleanup and an update run in a job task of their own, not in the menu's task.
  - None reads a button: a press is the menu's, which turns it into the job's command, the entry points the API
    calls, by button. Back stops a job; a click zeroes the scale while the charge waits for zero or for the cup's
    return, sets the cleanup's speed to 0, or acknowledges a fault; the knob sets the cleanup's speed.
  - A start refused for a motor driver is shown by the menu, not by the job.
- **Found and fixed on the unit:** `rc_run.py update --expect refused` failed first. The update's end called
  `menu_wake()` to wake its mode, which had waited on the button queue; in the job task nothing woke it, and the API
  read `ending` where it expected the job over. The job task now has a wake of its own (`jobs_wake()`). No other mode
  was woken that way: the charge and the cleanup look for a stop every 100 to 200 ms, as before.
- **Proven on the Pico 2 W** (the screens still the old ones):
  - `menu_tour` against its baseline: 20 forms the same, form 31 without the calibration's row (saved);
  - `charge_cycle`;
  - a cleanup started over the API: two turns gave 2 rev/s, a click 0, Back stopped it (`by` button);
  - a charge stopped by Back (`by` button); a driver's fault (`tmc coarse fault short`) acknowledged by a click;
  - an update refused, and one stopped by Back;
  - `api_check`, 24 of 24 actions.

## Gate 4: the new menu on LVGL (2026-11-17)

- **Built,** as AGENTS.md's code map now lists it:
  - `menu_nav.c`: the tree of menu_design.md as one table, what is open (pages, a choice's list, the digit editor, a
    message, a job's screen), and four keys. It fills a screen model in words; it knows no pixels.
  - `menu_view_12864.c`: that model drawn with LVGL labels in the menu's own fonts, made at build time from u8g2's BDF
    sources (`scripts/bdf2lvgl.py`: Helvetica 8 regular and bold, ProFont 22).
  - `menu.c`: LVGL and the panel (u8g2 is its driver and frame buffer only), the keys, the jobs' starts and ends. A job
    takes the screen from any page, whoever started it.
  - The knob's button read on both edges (20 ms debounce): a click is Enter, a hold of 600 ms is Back, as the reset
    button is. Its queue is made before its interrupts (ADR-012's audit).
  - The frame buffer's lock taken by the flush, the mirror and the snap: whole frames (ADR-012's audit).
  - The marks on every screen: `W` or `AP`, `R` (a job started from elsewhere, or a write to the API in the last
    30 s, `http_server_wrote_within()`), `SIM`.
  - The console's `screen` (`list`, `<id>`) in place of `form`, and `press hold`; `menu_tour` walks the pages by id.
  - A setting written from the screen goes through the API's own checks and is stored when confirmed
    (`settings_write_and_save()`).
  - MUI and `mui_menu.c` are gone; the modes, the update and the Wi-Fi no longer draw.
- **Where it departs from menu_design.md,** each small:
  - Firmware shows Version (with "trial"), Last update (once there was one), Board, Unit and Build; Check for update,
    Channel and the main menu's Update row come with gate 5.
  - The Wi-Fi page: Wi-Fi (joined, joining, own access point), Network, Key (on its own access point), Address, Name,
    Signal (as a station).
  - The charge's screen: its state, the weight large, "Target x  t s", the powder; what a click does at the bottom
    right, `[Zero]` while it waits for zero, `<Zero>`, `<Target>`, `<Powder>` between throws (the knob moves among
    them), `[OK]` on a fault. The last throw's verdict is in the screen model (`screen`) but not drawn on the 12864.
  - The cleanup's screen: "Turn to run", the speed large, the weight and the gate, `[Stop the motors]`. The flow figure
    it showed is dropped: it was the difference of two readings 20 ms apart, not a flow.
  - Rotation: 0 and 180 degrees, by the panel's flip; 90 and 270, if stored, are shown as 0 and 180, said on the
    console.
  - The layouts are a first draft: gate 3's mock is where the owner settles them.
- **Figures:**
  - Pico W: 861.3 KB of its 1,016 KB slot at Debug (799.4 KB before); LVGL 73.8 KB, u8g2 4.2 KB (31.6 KB with MUI and
    its fonts). RAM at link 241.0 KB of 256 (223.9 KB before).
  - Pico 2 W: 857.6 KB at Debug (794.9 KB before).
  - LVGL's heap held 8,116 bytes at most, of 12 KB.
  - The FreeRTOS heap at rest on the Pico 2 W: 53,776 bytes free, against 60,352 at 2.0.2: the job task's 4 KB stack,
    and the menu task's 2 KB more (at 4 KB it had 142 words left with LVGL).
- **Proven on the Pico 2 W:**
  - every page opened and read (`screen <id>`), their pictures the new baseline (`tests/menu_baseline/pico2_w/`,
    `page_<id>.png`), and `menu_tour --compare` against it;
  - a charge by the keys: Start with no target opened the editor (000.00), the target set to 20.00 digit by digit,
    Start, the cup acknowledged, its screen while charging and between throws, stopped by a hold of the knob (`by`
    button), the screen back on the Charge page;
  - a cleanup by the keys: the acknowledgement, two turns 2.0 rev/s, Back (`by` button);
  - an update started over the API took the screen from the main menu; Back stopped it (`by` button);
  - Decimals set to 3 from the screen, stored (`saved` true over the API), and set back to 2;
  - the remote mark after a write over the API;
  - `charge_cycle`; `api_check`, 24 of 24 actions.
- **Left for the owner's bench:** the Pico W on the probe for its run (the menu draws on both chips alike, but the
  panel, the input and memory are a driver's and memory's change, proven on both): `flash`, `menu_tour --save
  tests/menu_baseline/pico_w`, `charge_cycle`, `stats` for its heap at rest, and the owner's own use by hand.

## Gate 4: a second reading, and its fixes (2026-11-17)

A fresh reviewer read gates 2 to 4's diff for bugs (the code only). What it found, and what was done:
1. **A stop lost behind an editor.** Between throws the charge's Target or Powder opened an editor or a list over
   its screen; when the next throw began it stayed open, and the reset button stepped back through its digits while
   the charge poured (up to six presses). Seen on the unit before the fix: the reset button only left the editor.
   Fixed: what was opened over the charge closes when a throw begins (`menu_nav_refresh()`). On the unit: the editor
   opened at "Remove the cup", closed by itself as the next throw zeroed, and one press stopped the charge.
2. **A zero with the pan bare could start a throw.** Zero was offered, focused, while the charge waited for the cup's
   return, which it takes a reading of 0 as; the API refuses that zero for this reason. The old firmware zeroed there
   on any press too. Fixed: Zero is offered only while the charge waits for zero. A person whose cup reads below zero
   on its return stops the charge and starts again.
3. **The screen's zero could land as the pour starts.** Fixed: taken under the activity lock, the state read again,
   as the API's zero is.
4. **A lost release could read as a hold, a stray Back.** Fixed: the interrupt posts the pin's level on each edge and
   keeps no state; the menu drops repeats and reads the pin again before a hold counts. The console's `press hold`
   has an event of its own, with no pin to read.
5. **A stop asked for a start refused (a driver not OK) was kept,** and ended the next charge at its start. Fixed: it
   goes with its request (`activity_clear_start()`).
6. **The job task's end flags in the wrong order** could drop a start taken at once after a job. Fixed.
7. **The last throw's verdict and the stop's hint were not drawn.** Fixed: the verdict at the right of the state, and
   "RST: stop" where a click does nothing.
8. **The marks asked the radio for its signal ten times a second** (`wireless_get_info()`, under lwIP's lock). Fixed:
   `wireless_get_link()`, the same without the signal; only the Wi-Fi page's Signal row asks.
9. **A save from the screen stores the web's unsaved changes to the same resource.** Kept: menu_design.md, item 4,
   says so and accepts it.

After the fixes, on the Pico 2 W: the editor's test above, `charge_cycle`, `api_check` (24 of 24), `menu_tour
--compare` (the Charge page now reported, not judged: the target lives in RAM). Also: a page opened by `screen <id>`
has its parents focused on the row that leads to it, as the knob would have left them; `tests/web_test_data.json`
recorded again from the unit, its schema without the calibration.

## Gate 5, the poll: what it waits on

- **The Pico W's FreeRTOS heap with gate 4,** measured on the unit. Gate 1 computed about 14 KB left at an HTTPS
  fetch's peak, with three trims; gate 2's job task (4 KB) and the menu's larger stack (2 KB) now take from it. The
  job task would do the download itself, with no task of its own for TLS, and its stack's high-water mark decides
  its size.
- **The stored channel and source:** a struct in a module's own slot in the EEPROM, said in this ADR when built.
