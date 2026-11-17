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
  panel, the input and memory are a driver's and memory's change, proven on both). Done the next morning, below,
  but for the owner's own use by hand.

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

## Gates 2 and 4 on the Pico W (2026-11-17)

The owner put the Pico W on the probe the next morning. The Pico 2 W stays on the owner's Wi-Fi at 192.168.2.166,
running gate 4's build, with the A&D on it.

- **Its settings were copied first** (`logs/adr010/picow/before_gate4/`, at 2.0.2), and were unchanged after.
- **Proven,** at 7d83ab9 (2.0.2-26):
  - `flash`;
  - `menu_tour --save tests/menu_baseline/pico_w`: its 10 pages, the Pico W's first baseline. Against the Pico 2
    W's baseline, the one judged page that differs is Display: this unit stores its rotation as 180 ("Upside
    down"), the other as 0;
  - `charge_cycle`;
  - `api_check`, 24 of 24 actions.
- **The tour raced the Wi-Fi's join.** Run at once after a flash, about 22 s after the start, every page lacked the
  `W` mark and differed from the baseline. The tour now waits for the marks to settle: no `R`, and `W` or `AP`
  shown. Run straight after a restart, it waited about 5 s and passed.
- **Memory:**
  - the FreeRTOS heap at rest: 53,784 bytes free, against 59,216 at 2.0.2, which is the job task's 4 KB and the
    menu task's 2 KB more. Its lowest was 52,616, after the tour and a charge;
  - LVGL's heap held 8,424 bytes at most, of 12 KB;
  - flash 862.0 KB of the 1,016 KB slot; RAM at link 241.0 KB of 256.

## Gate 5: an HTTPS fetch on the Pico W, measured (2026-11-17)

Gate 1 computed the Pico W from the Pico 2 W. With the Pico W on the probe, the measure build (gate 4's code with
the TLS client, `-DADR010_MEASURE=ON`) was run on it against the local OTA server, and gate 4's build was put back
after. Its image is 944.9 KB of the slot, and RAM at link is 247.3 KB of 256.

- **The FreeRTOS heap** had 53,776 bytes free at rest. A fetch through the main host's redirect to the asset host
  (the ECDSA chain, then the RSA chain) took its low mark to 5,280: 48,496 bytes. That is with the fetch in its
  own task, on an 8 KB stack of which it used 2.4 KB, and with the four roots parsed.
- **mbedTLS's peak** was 38,788 bytes, as on the Pico 2 W. The body of the 820 KB update image added nothing.
- **Handshakes:** 6.1 to 6.3 s for the ECDSA chain and 2.2 to 2.3 s for the RSA chain. The Pico 2 W's same Debug
  build took 7.6 s and 2.0 s. The Cortex-M0+ is no slower here, so gate 1's worry, handshakes several times the
  Pico 2 W's, does not hold.
- **The update image** (820 KB) took 20.3 s over HTTPS and 5.6 s over plain HTTP.
- **What it leaves for the poll,** computed from these figures:
  - GitHub's chains take 1,952 bytes more at mbedTLS's peak (gate 1), so a fetch from GitHub would leave about
    3.3 KB at rest, as measured.
  - The fetch done in the job task, with no task of its own, gives back the 8 KB stack, less what the job task's
    stack must grow by (the fetch used 2.4 KB).
  - Only GitHub's two roots give back about 3.3 KB more.
  - That leaves about 12 to 15 KB at the fetch's peak at rest. Take off the web's load (3.3 KB in a soak at 2.0.2)
    and the update's 4 KB flash sector while it writes, and 5 to 7 KB are left.
  - This is what gate 1 computed, but its base is now measured on the Pico W. Gate 5 measures its own build.

## Gate 5, the poll: what it waited on

- ~~**The Pico W's FreeRTOS heap with gate 4.**~~ Measured 2026-11-17, above.
- ~~**The stored channel and source.**~~ Built: below.
- ~~**The owner's answers.**~~ The beta channel as proposed, and gate 5 to go ahead ("Yes continue with gate 5",
  2026-11-17). Gate 3 is dropped (README). The ADR's approval is still the owner's, on PR #132.

## Gate 5: the update at the screen (2026-11-17)

- **Built:**
  - **The client,** `https_client.c`: gate 1's measure grown into the unit's own GET over HTTPS or HTTP. It runs in
    its caller's task, follows redirects, takes a chunked body, hands the body over in pieces, and stops when its
    caller says. Every mbedTLS allocation is still counted. The measure build and `measure/` are gone: mbedTLS's TLS
    client, at -Os, is in every build.
  - **The roots:** GitHub's two (`src/github_roots.pem`) in every build, made into a header by `CMakeLists.txt`. The
    local server's test roots are only in a build made with `-DHTTPS_TEST_CA=ON`; such a build can also name its
    default source (`-DUPDATE_TEST_SOURCE`), so a bench unit's first start never polls GitHub.
  - **The stored channel and source,** `update_poll.c`: one struct, `eeprom_update_config_t`, in a new slot of its
    own at 11 KB of the EEPROM. **No other module's stored layout changed.** `settings/update` comes from its table.
  - **The poll:** once each start when the station joins, and from the Firmware page's Update row or `POST
    /api/v1/update/check`. It runs in a task of its own, at priority 2 with a 5 KB stack, deleted at its end.
    - Stable reads `releases/latest/download/manifest.json`; a 404 there is no stable release yet, so up to date.
    - Beta reads GitHub's list of releases, then that tag's manifest: api.github.com for github.com, `/api/v3` on
      any other host.
    - The JSON is read as it comes, never held whole. A release no newer than the image running is up to date.
  - **The download** (`update.c`): the job fetches the image the poll found itself, in the job task, into the free
    slot as an upload goes. It is checked against the manifest's SHA-256, then as an upload is, then installed with
    no second word. Back stops it until it installs.
  - **The API** (api_v1.md), within v1:
    - `GET /api/v1/update` gains `expected`, `download`, `poll` and `found`;
    - `POST /api/v1/update/check`;
    - `POST /api/v1/update/start` takes `download`;
    - the code `no_network`, the event `found`, and `settings/update`.
  - **The screens:**
    - the main menu's first row, "Update available" or "Important update" for a critical release, while one is
      found;
    - the offer: the version, the note's first two rows, "9.0.1 did not start" when the last try went back, "The
      last try failed" when it failed, Install, Later;
    - the Firmware page's Update row and Channel (while beta);
    - the update's screen: Connecting, Downloading n%, the version large.
  - **The test tools:**
    - the local OTA server lists releases as GitHub's API does, in its shape and sent chunked, and marks pre-releases;
    - `api_check` calls the new actions;
    - the console has `update check`, `update poll` and `stats tasks`.
- **Where it departs from menu_design.md:**
  - **"Check for update" is "Update"**, with what it is or does as its value: "Check now", "Checking...", "Up to
    date", "9.0.0 found", "Failed", and "needs a network" off a station. The label and "Up to date" overlapped on the
    12864.
  - **No "Checking" on the update's screen:** the check of the image downloaded takes under a second, and the screen
    goes straight to installing.
- **The Wi-Fi mark is an icon** (the owner, 2026-11-17, from renders of both): signal bars, always full, with no
  strength ("It is most clear solution on the tiny screen"), when joined as a station; a hotspot as the unit's own
  access point. Both are glyphs of the regular font at U+E000 and U+E001 (`scripts/bdf2lvgl.py`): LVGL draws nothing
  below U+0020. The console's `marks=W` and `AP` are unchanged. Every page's title changed, so the Pico W's baseline
  was saved again.
- **Proven on the Pico W,** through the local OTA server, with v9.0.0 (stable, confirms itself) and v9.0.1 (a
  pre-release that never confirms itself), both kept in `logs/adr010/gate5_releases/`:
  - **stable over HTTP:** the start's poll found 9.0.0, offered on the main menu, the offer page and the Firmware
    page;
  - **beta over HTTPS:** the list then the manifest found 9.0.1, "Important update" (critical). Install from the
    screen by the keys: Connecting, Downloading n%, installing, a trial that never confirmed, and back on slot A.
    `last` back, the event log back then found, the offer "9.0.1 did not start", Firmware "went back" and Channel
    "beta";
  - **stable over HTTP through the API** (`start {"download": true}`): downloaded in about 14 s, confirmed 9.0.0 on
    slot B;
  - **stable over HTTPS through the API,** with two clients on the pages throughout: confirmed. No web read missed
    (the portal's slowest 4.75 s), and no lwIP allocation failed;
  - **the shipping build, with no test roots,** refused the test server's chain (`-0x2700`, "not trusted");
  - **beta against a list in GitHub's own shape** (nested authors and assets, escapes, a character outside ASCII,
    4,785 bytes sent in 100-byte chunks): the right tag, then its manifest;
  - **then the shipping build:**
    - `menu_tour` against its baseline, the offer page added;
    - `charge_cycle`;
    - `api_check`, 26 of 26 actions;
    - the settings unchanged, `settings/update` new;
    - the icons on the unit, as a station and as its own access point, with Wi-Fi turned off for one start and on
      again, the settings the same after.
- **Figures** (the Pico W, Debug):
  - **flash:** 959.4 KB of the 1,016 KB slot (861.3 KB at gate 4); mbedTLS 69.8 KB. The Pico 2 W's is 930.7 KB of
    1,792;
  - **RAM at link:** 247.7 KB of 256 (241.0 at gate 4): the client's buffers and mbedTLS's contexts. That leaves
    about 8 KB for the C library's heap, whose users are small (the settings' reads, the charge's ring buffers);
  - **the FreeRTOS heap:** 53,592 bytes free at rest (53,784 at gate 4);
  - **a poll over HTTPS:** 8,168 bytes free at its low mark, with the poll's task and the test build's four roots,
    against the local chains. With GitHub's chains (1,952 bytes more) and only its two roots (about 3.3 KB less),
    about 9.5 KB. Beta's list then manifest takes no more: each fetch ends before the next;
  - **a download over HTTPS:** 985 KB in 33 s (handshakes 6.8 s and 2.3 s), about 14 s over HTTP. With two clients
    on the pages, the heap's low mark stayed at the poll's 8,168;
  - **mbedTLS's peak:** 38,788 bytes, as in gate 1. The handshakes took 6.1 to 6.8 s (ECDSA) and 2.1 to 2.3 s (RSA);
  - **stacks** (`stats tasks`, measured with 8 KB each first):
    - the poll used 3,352 bytes, over HTTP and HTTPS alike, so its task has 5 KB;
    - the job task used 2,952 for a download with its check, 984 for a charge, so it stays at 4 KB, as gate 1's trim
      planned.
- **The Pico 2 W's run is skipped** (the owner, 2026-11-17: "If we have verified the function on Pico W then Pico 2W
  is lower risk target. I'd propose to skip."). The Pico W is the tighter target in RAM and speed, and gate 5 adds no
  code of the RP2350's own: the download writes the free slot through the same functions as ADR-004's uploads, which
  were proven on both chips. The Pico 2 W's image is built (930.7 KB) and has not run gate 5. Its menu baseline,
  from before the Wi-Fi icon and the offer page, was removed; it is saved again at its next run on the probe.
- **Left:**
  - **The owner's use by hand of the screens.**
  - **GitHub itself:** the shipping roots against GitHub's real chains were measured in gate 1, one fetch. The poll
    against GitHub's API and releases is not run, by the owner's rule; its first contact is a user's.

## The screens designed again (2026-11-17)

After using gate 4's menu and seeing a throw and a cleanup snapped state by state, the owner found the screens too
dense, the mix of font sizes hard to read, and lines too close: "let's start with some mock up assuming you are the
designer of the embedded UI". Nothing in the firmware has changed yet: this is the design, to be built on the
owner's word.

- **Faults found on the unit first** (a throw from the keys and a cleanup, each snapped):
  - the last throw's verdict was drawn over the state ("Remove the cup" and "30.00 good");
  - the cleanup's "[Stop the motors]" was drawn over its gate line;
  - with the cup off, the figure was the bare pan's reading (-150.00);
  - no unit on the charge's figures;
  - the cleanup said "Turn to run" while it ran.
- **Five drafts, each drawn with u8g2's own fonts at 128 x 64** (`tools/ui_mock/`):
  - **Lucida Sans** came first. The owner: "a bit too wide on every screen".
  - **haxrcorp4089**, the Flipper Zero's text face, came next and was dropped by the glyph check: its "v" differs
    from its "u" by one pixel.
  - **Nokia Small** (bold for titles, plain for text, caps 7) with Logisoso 22 figures was the fifth draft. The owner
    found helvR08 and glasstown_nbp read better, in the faces' comparison.
  - **The design is all Helvetica** (the owner, 2026-11-17: "All Helvetica", chosen from the whole design drawn in
    each of those two): helvR08 for text, helvB08 for titles, helvB18 for figures. One family in three sizes, and
    all three among u8g2's BDF files, which the firmware's font generator already reads. At 8 px capitals a dense
    page holds four lines of 11 px, as a list does, and scrolls a little more.
  - **The owner's points, each written in:**
    - dense pages with a scroll bar are accepted;
    - Install sits at the end of the update's notes;
    - the charge's centred figure and line are kept.
- **The sheet:** `docs/developments_plans/ADR-010-lvgl/mockups/menu_12864.png`, drawn by
  `venv\Scripts\python tools\ui_mock\menu_12864.py`.
- **The guideline:** `.agents/skills/embedded-ui-design`, written from pixel-display practice:
  - the Flipper Zero's fonts, read from its firmware;
  - a 128 x 64 case study;
  - small-screen layout guides;
  - legibility by visual angle.

  Its form follows the Open Weapon Mount Computer's skill. Agents follow it for every screen from now on.
- **Animation:** the owner is open to it if the refresh and the load allow. The skill limits it to motion with a
  meaning, slow for a passive LCD, never while a charge pours. Before any is adopted, the Pico W's frame time, the
  menu task's share of the CPU, the motors' late feeds and the panel's smear are measured.
- **Left, on the owner's word to build:**
  - the view is rebuilt to the design, in faces the font generator already reads;
  - the navigator gains dialogs and notes, and softkeys in place of the job screens' action marks;
  - the menu baseline is saved again.

## The screens built to the design (2026-11-17)

On the owner's word ("Yes, build it"), the menu was rebuilt to the design in all Helvetica.

- **Built:**
  - **The faces:** helvR08 for text, helvB08 for titles, helvB18 for figures, from u8g2's BDF files by the
    existing generator; ProFont 22 is gone.
  - **The screen model** (`menu_nav.h`) has the design's six kinds (list, card, dialog, notes, edit, job), two
    softkeys, and a figure with its unit and one line under it. The job screens' old action marks are gone.
  - **Pages say their kind in the table:**
    - the cups' acknowledgements and the factory reset are dialogs (Cancel, Start or Erase);
    - Wi-Fi is a card;
    - Firmware and the update's offer are notes, ending in their button. The offer's Install is reached only by
      scrolling through its notes and warnings; a click before it turns a page.
    - Messages are dialogs with OK.
    - Focus passes over rows that do nothing.
  - **Notes and cards are wrapped by the navigator** with the view's measure of text
    (`menu_nav_set_text_width`), so it counts their lines and still knows no pixels.
  - **The charge:**
    - its status is the title;
    - its figure is in the scale's unit ("gr" for grains; none from the simulated scale, which reports none);
    - the line under the figure is "of 30.00", or after a throw its verdict ("Good", "Over by 0.04");
    - the throw's weight stays while the cup is away;
    - the softkeys are Stop and Zero, Change or OK.
  - **The cleanup:** "Cleanup" or "Cleanup: running", the speed in rev/s, Stop and Pause.
  - **The update:** "Connecting", "Downloading 9.0.0", "Checking", "Installing", the percentage, "Keep the power
    on".
  - **The view** (`menu_view_12864.c`) is rebuilt to the grid, every text placed by its baseline as the mock-up tool
    places it.
  - **The console's `screen`** prints the new model. Its reply gives LVGL's heap: the most used and the least free in
    one piece.
- **Where it departs from menu_design.md:**
  - **Between throws, the knob no longer cycles hidden actions.** The right softkey reads Change and opens a page with
    Target and Powder, which closes when the next throw begins, as the editor did.
  - **Who started a job is shown only by the remote mark (R),** no longer in words: the title is the status.
  - **The scroll bar is a 1 px track and a 3 px thumb.** The mock-up's dotted track needs LVGL's complex drawing,
    which is off; the mock-up and the skill now draw it as the unit does.
- **Found on the unit and fixed: LVGL's heap was too small for the new view.**
  - The figures drew as a lone "." and two titles lost their first letter.
  - LVGL's log, on for one build, said why: no memory for a glyph's working buffer (384 bytes for an 18 px digit).
    The heap had 620 bytes free in one piece at rest.
  - The new view's 33 objects (30 before), with a frame's draw tasks, filled 12 KB.
  - LV_MEM_SIZE is 14 KB now: 2,656 bytes free in one piece at worst through every screen and a whole throw.
  - The skill records the budget and how to read it.
- **Proven on the Pico W,** from the knob, each screen snapped and set beside the mock-up:
  - every page;
  - a throw: the target set digit by digit, the cup's dialog, waiting for zero, charging, "Lift the cup" with
    "Good" and Change, the Change page and back, the next throw's zero, a stop by holding the knob;
  - a cleanup idle, running and paused;
  - a driver's fault as a dialog;
  - an update found on the local OTA server: the main menu's row, the notes scrolled to Install, the download.
- **Figures (the Pico W):**
  - flash 966.6 KB of the 1,016 KB slot (959.4 before);
  - RAM at link 250.2 KB of 256 KB, 252.4 KB with the C library's 2 KB minimum heap, which leaves that heap about
    5.6 KB. (First written here as "258.5 KB of 256 KB": the linker's 258,468 bytes, read as KB.)
- **Seen in passing, not this work's:** after the console cleared a simulated driver fault (`tmc coarse fault
  none`), the main title still named the fault 30 s later. ADR-012's fault display; noted for its area.
- **Seen in passing, open, for the network's area:** the SDK's network task (`async_context_t`, lwIP and the
  Wi-Fi driver; 4,096 words, `lwipopts.h`) had 540 bytes of its stack never touched once, in the boot after
  `api_check`'s restart and its burst of web reads.
  - Otherwise: 3,696 bytes before Wi-Fi joins, and 1,956 after the join and a failed poll (1,924 that morning).
  - FreeRTOS's overflow check would halt the unit, motors stopped, before memory is corrupted. The margin still
    wants measuring.
  - **Its exit:** read `stats tasks` through a `soak --load` of 10 minutes with several restarts. If the task's
    stack left falls under 1 KB, raise `ASYNC_CONTEXT_DEFAULT_FREERTOS_TASK_STACK_SIZE` by 512 words (2 KB of the
    FreeRTOS heap) and measure again.

## Gate 4 sent back: u8g2 left in (2026-11-17)

The owner's review of the plan found u8g2 still in the firmware, as the panel's driver and frame buffer, beside
LVGL. That was against this ADR's own gate 4 ("all of u8g2 if LVGL is chosen, and the mirror fed from the new view")
and the owner's direction of 2026-11-13 (one implementation). Gate 4's write-up above did not list it among its
departures, and AGENTS.md and the skill `embedded-ui-design` then described it as the design ("u8g2 its driver and
frame buffer only"). The owner: u8g2 goes entirely, fonts included as a reason; the web's frame keeps no old format.
What it still does, what replaces each piece and how the unit shows it are in the README (gate 4, and "u8g2 after
gate 4"). Written, not yet built.

- **One buffer, not two** (the owner's question). The panel holds the frame it shows, so the frame in RAM is
  already the only one drawn into. A second buffer would gain about 2 ms of a frame, with a DMA send, for 1 KB of a
  Pico W at 98% of its RAM.
- **Found while writing it:**
  - the skill `embedded-ui-design` says a small animated area costs a fraction of a frame, "since LVGL redraws only
    what changed", and menu_design.md that the view "redraws what changed". In the render mode gate 4 chose, any
    change redraws the whole screen (`lv_refr.c`). Both become true with direct mode, which gate 4 now builds;
  - the Pico W's menu baseline for the screens in all Helvetica was saved at 12:00 (`menu_tour --save`, at
    2.0.2-32, VERDICT PASS) but not committed with them. It is committed now, so the compare above has its baseline;
  - the RAM figure of the screens' rebuild above read the linker's bytes as KB, and seemed over the limit. Corrected:
    250.2 KB of 256 at link, from a build of the same code.

## Gate 4 finished: u8g2 removed (2026-11-17)

On the owner's word ("Build it. And you can test on Pico W. I can test Pico 2W later today").

- **Built,** as the README's gate 4 says:
  - `display.c`: our own driver for the UC1701, on the SDK's SPI, sending the set-up u8g2's driver sent, byte for
    byte. It sends the pages of a frame it is given; it holds no frame.
  - `menu.c`: LVGL's frame is the only one, drawn in direct mode. A draw's areas mark the panel's pages they touch,
    sent with the draw's last area; the whole frame is sent again once a second. LVGL draws under the frame's lock,
    made before any task runs (`menu_init()`), and the mirror and `snap` copy the frame under it
    (`menu_frame_copy()`).
  - The frame's format is `mono_rows` (LVGL's rows, the body of a PBM image): `GET /api/v1/display`, the mirror's
    page, `snap` and `rc_run.py`. The web test's recording was converted to it. No old format is kept.
  - `screen`'s reply gives `frames`, `draw_us`, `draw_max_us`, `send_us` and `send_max_us`.
  - The fonts: `fonts/` holds helvR08, helvB08, helvB18 (the firmware's) and helvR14 (the mock-up sheet's captions),
    copied unchanged, with their notice in `fonts/README.md`. The mock-up tool reads only them; its reader of u8g2's
    catalogue is gone. The sheet it draws is byte for byte the one committed.
  - The submodule, its CMake lines, its part in the size report and five includes are gone; AGENTS.md, the skills
    `c-style` and `embedded-ui-design`, and api_v1.md say so.
- **Found on the unit, and fixed: the view redrew itself at every refresh.** The first build drew a frame about four
  times a second on an idle main menu, 23 to 26 ms each. LVGL redraws an object whenever one of its styles is set,
  even to the value it had, and the view set every row's highlight at every refresh (`view_reverse()`). Now it sets
  one only when it changes: an idle page draws once and then nothing. In the render mode gate 4 had first chosen, the
  whole screen was drawn each time; that cost was there before, unseen, since nothing measured it.
- **Figures (the Pico W, Debug):**
  - flash 960.9 KB of the 1,016 KB slot (965.3 KB before); RAM at link 249.1 KB of 256 (250.2 before): u8g2's
    1,024-byte buffer and its 148-byte handle gone;
  - the whole screen drawn: 38 to 43 ms (a start, a page opened);
  - a charge's figure drawn: 7.3 to 8.5 ms, about 8 times a second while it pours;
  - sent: all 8 pages up to 9.2 ms, a figure's 3.6 to 6.0 ms. These are wall-clock times, with the motors', the
    scale's and the network's tasks above the menu's;
  - so while a charge pours the screen takes about 12 ms in each 100 ms, where redrawing the whole screen would take
    about 40;
  - no late feed of the step pulses through a charge (`motor`: `underruns=0` on both);
  - the menu task's stack: 2,424 bytes never touched, of 6 KB; the FreeRTOS heap at rest 53,592 bytes free, as at
    gate 5.
- **Proven on the Pico W** (2.0.2-35, this change before its commit):
  - its settings copied first (`logs/adr010/picow/before_u8g2_removal/`), unchanged after;
  - `flash`;
  - `menu_tour --compare` against its baseline: every judged page the same picture, so LVGL's frame and the new
    snap are what the old path showed;
  - `charge_cycle`;
  - `api_check`, 26 of 26 actions;
  - the mirror's page in a browser on the unit, showing the main menu; `GET /api/v1/display` names `mono_rows`, and
    the frame is 1,024 bytes.
- **Left:**
  - **the owner's look at both panels:** a snap and the mirror read the frame in RAM, never the glass, so only an eye
    sees a wrong set-up or a flip 4 columns off. The Pico W stores 180 degrees, the Pico 2 W 0;
  - **the Pico 2 W's run** (the owner, later the same day): its baseline saved first on the build with u8g2, then
    `flash`, `menu_tour --compare` and `charge_cycle`. Its image is built: 931.7 KB of 1,792.

## A hidden hello for agents (2026-11-17)

The owner, for fun, with no plan wanted: "a special screen overlay event when an agent is tapped into the system
over the rest interface ... Claude chan is watching ya!", the agent naming itself ("whoever the agent can declare who
they are"), and "a flashing of the full screen overlay on the first connection".

- **Built:**
  - `POST /api/v1/agent/hello` with `{"name": ...}` (printable ASCII, 1 to 16 characters; Claude-chan by default).
    A POST, not the GET first thought of: api_v1.md's first rule is that a read "changes nothing. Not a selection,
    not an event, not a light". In neither the index nor the schema; AGENTS.md tells agents ("Say hello"). 409 while
    a job runs; the job's screen is never covered.
  - The figure: `fonts/claude_chan.txt`, four poses of 48 x 48 (ahead, left, right, blink), made by the font
    generator into 36 tiles of 16 x 16 (`:art`). The overlay is four objects: the dark ground, the figure (one label
    of nine tiles, a pose another nine), the name and the words. Ten steps of 200 ms: it peeks up from the bottom
    edge, looks left and right, blinks and goes down, "<name> / is watching / ya!" beside it.
  - The flash: the panel's own inversion (`0xA7`), twice, before the figure, when no agent has said hello for 10
    minutes. Two flashes in 0.8 s, under the three a second kept for flashing screens. The frame is untouched, so a
    snap and the mirror do not show it: only an eye does.
  - A press during a hello ends it and does nothing else; a job or `screen <id>` ends it too.
- **Found on the unit, and fixed: the first overlay faulted the core.** On LVGL's top layer, with eight objects
  (a box for each pupil and lid), the page under it was drawn as well; LVGL's heap ran out, a draw task's allocation
  failed and was written through (`lv_draw_finalize_task_creation`, read from the stacked frame through the probe),
  since LVGL's checks are off. Now the overlay is the screen's last child, opaque and full-size, so LVGL skips the
  page under it, and has four objects; it is made only with 2,048 bytes free in one piece, and let go otherwise.
- **Figures (the Pico W):** a hello over each of the 10 pages a person can open: 2,604 to 2,680 bytes of LVGL's
  heap in one piece before, about 1,950 with the overlay made, 1,868 at the least through all ten. Flash 966.1 KB
  (960.9 before); the Pico 2 W's 936.8 KB.
- **Proven on the Pico W:** the frames read back through the mirror's endpoint during a hello (the peek, the looks,
  the blink, the menu back after 2 s); a named one ("Codex") without the flash; a name empty, too long or under
  another field refused; a hello during a charge refused, the charge's screen kept; a turn of the knob during a hello
  ended it and left the menu's focus where it was; then `menu_tour --compare`, `charge_cycle` and `api_check` (26 of
  26). Left: the flash, by eye.

## The screen inverted, as a setting (2026-11-17)

The owner's look at the panel: the mini 12864's glass is negative, a lit pixel light on a dark ground, the opposite
of what the mock-ups, the snaps and the skill drew. The owner: the setting first, then the tools, then the hello
bright whatever the setting. A unit starts with the glass's own look (the owner's choice); the hello keeps its flash.

- **Built:**
  - `invert_display` in `settings/display` ("Invert the screen", applies now), its row Invert screen on the Display
    page, its field in the portal. The panel's own command (`0xA7`): the frame is untouched.
  - The menu's task alone sends the panel's inversion, when it changes: the setting, or during a hello its flash
    (lit, dark, twice) and then lit for the figure. `menu_frame_copy()` applies it, so the mirror and `snap` show the
    panel as a person sees it; the frame's `1` is a lit pixel.
  - The mirror's page, `rc_run.py`'s PNG, the mock-up sheet (`panel.py`) and the skill draw a lit pixel light on a
    dark glass. The Pico W's baseline pictures were flipped once by a script that reads each with the old meaning
    and writes it with `rc_run.py`'s own `write_png`, so the bytes are what a snap now writes.
- **Found: the stored struct did not reset.** It is 8 bytes before and after (the enums are one byte); the flag
  landed in its filler and read the stored byte, non-zero on the Pico W, so the screen started inverted. Stored off
  on the Pico W. Left as it is by the owner; handed on to ADR-006, and AGENTS.md's rule corrected.
- **Figures (the Pico W):** flash 968.4 KB of 1,016 KB (991,608 bytes; first written here as "991.6 of 1,016 KB", the bytes read as KB); the Pico 2 W builds.
- **Proven on the Pico W:** the settings copied before the flash (`logs/adr010/picow/before_invert/`); `menu_tour
  --compare` against the flipped baseline: every page the same but the Display page, its new row (saved again); the
  setting on, a snap of the main menu inverted; a hello with the setting off and on, the frame read through the
  mirror's endpoint every 0.2 s: off, the flash (74, 26, 74, 26% lit), the figure lit (84 to 96%), the menu back at
  26%; on, the figure lit, the menu back inverted at 74% (no flash: a second hello within 10 minutes);
  `charge_cycle`. Left: the owner's look at the glass, both settings.

## Back by the knob on every page (2026-11-17)

The owner, at the unit: on Firmware the knob could not scroll up to leave, only the reset button did, "breaking the
convention of the interaction flow" (menu_design.md: "above the first row is the title, which works as Back").
Every page was turned on the Pico W, pressing nothing, to count them:

- **Wi-Fi (a card), Firmware and Update (notes):** their title showed "< " but the knob only scrolled the lines,
  so the title was never focused. Fixed: turned up past the top, the title is focused (in reverse), a click there
  is Back, a turn down goes back to the lines. A notes' button is not shown reached while the title has the focus.
- **Every list (main, Charge, Settings, Scale, Display, a choice's list):** turning up past the title kept
  counting (4 turns up from the first row: focus -3), so as many turns down did nothing, and each read outside the
  page's rows in `row_acts()`. Fixed: the focus stops at the title.
- **Left as designed:** the three dialogs (Factory reset, the cup before a charge and a cleanup), the target's
  editor and a running job, whose Back is the left softkey (Cancel, Stop): a turn that cancels a factory reset or the
  cup's check by accident is worse than the reset button. The skill's lines on cards and read-only pages say the
  title is Back there too.
- **Proven on the Pico W:** Settings 4 turns up and 1 down, focus 0; Wi-Fi, Firmware and Update, a turn up focuses
  the title and a click goes back (Update's job left idle, nothing installed); Firmware scrolled down and up to its
  title and down again ([firmware_title_back.png](firmware_title_back.png)); `menu_tour --compare` and
  `charge_cycle` pass.
- **Seen on the way:** the Pico W's Invert screen was found on, stored, between 19:37 and 19:41, when it had been
  stored off; nothing the agent ran in that time writes it (the web test's recording was run again and left it
  off). The owner had set it by hand at the unit ("Yes, that was me"): no fault. Stored off again for the proof.

## A click acts as the knob goes down; no long press (2026-11-17)

The owner found Back slow at the unit, then ruled out the long press: "for any click event short click should be
made default", Back "either a short click on '<', or reset button. Any hidden feature is a NOT a good UI design. And
do not try to educate the user." Measured first: after a Back the next page is drawn and sent in 35 to 60 ms (six
pages), so the slowness was the input: a click acted at its release, and the hold took 600 ms.

- **Built:**
  - The knob's button keeps its state in the interrupt: an edge changes it only when its level differs and 20 ms
    have passed since the last change. A press is sent as it goes down (`BUTTON_ENCODER_PRESSED`); the release is
    not sent. The old way read the level at the first edge and dropped every edge for 20 ms after it, so a bounce
    read at that edge lost the click.
  - The hold, its timer and its pin check are gone from the menu, and `press hold` from the console. A dialog, the
    number editor and a running job are left by the reset button (menu_design.md's principle 2 rewritten; the skill
    too). No hint was ever built, so none goes.
  - `knob`, the knob's tap: every edge of its button (taken or dropped as a bounce) and every turn sent, with its
    time, and any event the menu's queue had no room for. Kept in the console, like `scale tap`.
- **Measured (the owner's 24 presses on the Wi-Fi page, slow and quick):** each press taken once, each release once;
  no turn, no event lost. The presses did not bounce at all; the releases bounced up to about 5 ms (16 of 24 had
  edges dropped, all within 5 ms of the release), a quarter of the 20 ms kept. The knob was held 111 to 840 ms, about
  190 ms in the middle: what every click used to wait before it acted.
- **Proven on the Pico W:** `menu_tour --compare` (the screen not inverted: the owner keeps Invert screen on, so it
  was turned off for the run, not stored, and put back), `charge_cycle`; a click on Settings' title goes back, the
  reset button leaves the charge's dialog.

## A card's keys at the left, their values at the right (2026-11-17)

The owner: "For key value type page, like the Wi-Fi, the key should be aligned to left and value aligned to the
right. Otherwise the page will look unorganized."

- **Built:** a card's or notes' row with a key and a value is one line, the key at the left and the value at the
  right, the key clipped short of it; a pair too wide for a line takes two, the value right on the second. A row
  with only one of them (the update's notes, its headings) is wrapped at the left as before. Wi-Fi's first line is
  now a key ("Joined", "Joining") and the network's name, and the unit's name has its key ("Name"). The console's
  `@line` gives the value (`value="..."`); the mock-up draws pairs the same way.
- **Proven on the Pico W:** Wi-Fi reads Joined / IP / Name with the name under it / Signal, Firmware Version /
  Update / Board / Unit / Build, each value at the right; `menu_tour --compare` (the screen not inverted for the
  run) and `charge_cycle` pass. The Wi-Fi and Firmware pages saved again as the baseline.

## Every element on screen reached by the knob (2026-11-17)

The owner: some Cancel buttons were "not selectable, for example from charge, and cleanup menu. I need you to scan
through the pages and ensure every UI element is actually clickable." The scan (the code, with the turns on the unit
from the earlier one): every list row, title and notes' button was reached; the left softkey was not, on seven
screens: the three dialogs (the cup before a charge and a cleanup, the factory reset), the target's editor, and a
running charge, cleanup and update. Only the reset button worked them.

- **Built, as the owner chose:**
  - Softkeys are focused like rows: the knob moves between them, the focused one is drawn in reverse, a click does
    it. A dialog opens on its action; the factory reset on Cancel. A message's OK is focused.
  - A running charge focuses each new action as it appears (Zero, Change, OK); with none, while it pours, Stop.
  - A cleanup starts paused, on Run (1 rev/s). Running, the turn sets the speed, through 0 into reverse, and a click
    pauses it, keeping the speed for Run; paused, the knob moves between Stop and Run. Its hints ("Turn the knob for
    speed", "... to run") went, by the owner's rule against teaching the controls.
  - The update's one softkey, Stop, is focused and a click stops it, until it installs.
  - The editor as the old menu's (MUI's "mud" fields): the turn moves the focus over the digits, then Cancel and Set;
    a click on a digit starts setting it (drawn in reverse, the turn its value), a second click ends that. Back ends
    a digit's setting, or leaves the value unchanged.
  - The console's `@softkeys` gives `focus=left|right`, the editor's `@figure` `editing`; the mock-up draws the
    focused softkey.
- **Proven on the Pico W, each by the knob from the console:** the factory reset opens on Cancel, a turn reaches
  Erase and back, Cancel leaves it with nothing erased; the charge's cup check opens on Start, Cancel reached and
  done; the target set to 30.00 through the editor; a charge started, Zero focused, Stop reached, and while pouring
  Stop focused and a click stopped it; a cleanup Run at 1, turned to 2 and through 0 to -1, paused, Stop and Run
  reached, Run resumed at -1, paused, stopped by Stop. The update's Stop was not driven (no local OTA server running).
  `menu_tour --compare`: the three dialogs differ, as meant, and are saved again; `charge_cycle` passes.

## The hello played only its flash (2026-11-17)

The owner: the hello "no longer show the animation, just flashing the screen. My original intention is except for
the charge mode, every call to hello should trigger the animation, regardless of the location."

- **Found on the Pico W:** the console said "Hello: let go, LVGL's heap has 2020 bytes in one piece". The overlay
  was made at each hello and asked for 2,048 bytes in one piece; the menu's growth since (the softkeys' boxes, the
  digit's box) had taken that away, unnoticed. The flash played, the figure never did, and the API still answered
  `"shown":true`.
- **Fixed:**
  - The overlay's four objects are made once at the start, the screen's last child, hidden until a hello. A hello
    no longer depends on LVGL's heap at that moment, and "shown" is true.
  - It plays everywhere but over a charge: over a cleanup and an update too (409 only while a charge runs). Over a
    job the reset button still stops it, ending the hello; other presses only end the hello, as before.
  - **LVGL's heap gets a second pool, 3 KB from the FreeRTOS heap.** With the overlay always there, the 14 KB were
    at most 11.1 KB used with 736 bytes free in one piece at the least, where a draw takes up to 384 bytes and an
    allocation LVGL cannot make faults the core. The image's RAM is full (251.6 of 256 KB at link), so the pool
    comes from the FreeRTOS heap, sized against what an HTTPS poll leaves it at its low mark (8,168 bytes with the
    local chains, about 9.5 KB with GitHub's, gate 5): about 5 and 6.5 KB with this pool. 6 KB was tried first (7 KB
    in one piece for LVGL, 2 to 3.5 KB left to a fetch). Measured again with an HTTPS poll in the survey's proof.
- **Proven on the Pico W:** a hello on the main menu (the flash, then about 2,100 pixels changing for 2 s), over a
  running cleanup (shown; the reset button during a hello stopped the cleanup), and over a charge refused ("a charge
  has the screen"); `menu_tour --compare` and `charge_cycle` pass. LVGL's heap at most 11.4 of 17 KB, 3.9 KB in one
  piece at the least; the FreeRTOS heap 39.9 KB at its least (no fetch in that run).

## Gate 6, step 1: the description grows (2026-11-17)

- **Built:**
  - The settings' description (`settings.h`) gains, for a client with little room: a field's `short_label`, its
    `edit_step` (what a detent moves a number by), a boolean's `on_label` and `off_label`, and a choice's
    `short_label`. The schema publishes them (api_v1.md), for ADR-008's pages too. Set where the screen needs them:
    the stop thresholds ("Fine stop", 0.01; "Coarse stop", 0.1), the decimals, the scale's make, baud rates and
    frame formats ("8N1"), the knob's and the screen's inversion, a motor's direction ("Reversed", "Normal"), the
    shutters' ends (0.001), the channel ("Beta").
  - No flag for "on the screen": the menu's tree places each setting, and a second record of it could drift.
  - **Charge view** (Weight or Chart), the display's new setting, in its struct's last byte of filler.
- **Found: that byte held 255 on the Pico W** (an erased EEPROM byte, which is also why Invert screen started on),
  so Charge view was none of its choices and the API gave it as `null`. **Fixed for every choice:** at the start, a
  stored choice that is none of its choices is replaced by its first and stored, and the console says so ("Settings:
  /api/v1/settings/display charge_view held 255, none of its choices: Weight put in its place"). Booleans and
  numbers in filler are left to ADR-006.
- **Proven on the Pico W:** the schema's new keys; the mend at the first start and none at the next, `saved` true;
  `api_check`, `settings_roundtrip`, `menu_tour --compare`, `charge_cycle` pass. The web test's recording made again.
