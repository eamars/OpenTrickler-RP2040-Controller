# ADR-005: what each board carries: a smaller image, a feature table, budgets

Status: **Decided** 2026-11-17, closed as a scoping ADR at the owner's word ("Yes, close it"). Its work is done when its trigger fires (below, "Decided"): an image that no longer fits its slot, or an ADR's measurement that fits only an optimised build. Drafted 2026-11-13. Rule 1 below is the owner's.

## What we want

- An image no larger than it needs to be.
- One place that says which optional modules a board carries, and a unit that reports it.
- A build that fails when a board no longer has room, in the PR that causes it.

## Owner's direction (2026-11-13)

- Boards are supported in tiers: some features only where there is room.
- The implementation is consistent: a feature is present on a board or absent from it, never built a second way.
- 2026-11-15, given in ADR-012 (its step 6): **fail early.** "If the Wifi is missing then the WiFi menu should not be made visible (This is just an example)." Rule 3 below says it for what a board is built to carry; the same holds for hardware found missing when the unit starts. One line to keep: a function that is only not set up keeps its menu, since the menu is how it is set up. A radio that does not start is missing; a network not joined is not. Motor drivers, which every board carries, stay offered and refuse at the first press with the reason (ADR-012, step 6).

## Owner's direction (2026-11-17)

- ADR-005 stays a scoping work: closed by recording its decisions, its real work done afterwards, when RAM or flash runs tight.
- The trigger: a future ADR's image exceeding its partition.

## Why here

After ADR-004 the application's room on each board is a known number, so "does it fit" can be answered and a tier means something.

## What we know

### Measured 2026-11-17, on main after ADR-004 (d69acf1)

`tools/size_report.py` on the Pico W image, as built today (Debug, `-O0`) and the same code size-optimised (MinSizeRel, `-Os`, which also turns `assert()` off, so a `-Os` build that keeps it is a little larger):

| Part | Debug | MinSizeRel |
|---|---|---|
| The radio: cyw43's driver and its firmware (data) | 252.0 KB | 232.0 KB |
| The application | 169.3 KB | 120.9 KB |
| lwIP | 133.3 KB | 53.8 KB |
| Web pages (data) | 100.9 KB | 100.7 KB |
| Pico SDK | 62.2 KB | 25.8 KB |
| u8g2 and MUI | 31.6 KB | 19.5 KB |
| FreeRTOS | 31.5 KB | 11.1 KB |
| C and C++ runtime | 18.5 KB | 18.7 KB |
| **Total, of the slot's 1,016 KB** | **799.4 KB** (217 KB left) | **582.5 KB** (433 KB left) |

- RAM at link time: 223.9 KB of 256 KB on Debug, 221.9 KB on MinSizeRel; 112 KB of it is the FreeRTOS heap, whose high-water mark is not known. RAM, not flash, is the Pico W's tighter limit.
- The web pages are our own markup and script, unminified and uncompressed: `web_portal.html` is 95 KB, the four pages 107 KB, 19 KB gzipped. Tailwind and DaisyUI are not in the image: the browser fetches them from `cdn.tailwindcss.com` and `cdn.jsdelivr.net`, which is why the page has no styling offline (ADR-008).
- The C++ name demangler is no longer in the image (the runtime is 18.5 KB, not 72 KB): Idea 3 below is done.
- An image larger than its slot fails to link (ADR-004 cut the linker's region to the slot): `region FLASH overflowed`.

### Checked 2026-11-13, before ADR-004

**The image**, from `tools/size_report.py` on the Pico W build of today's main (the Pico 2 W image is 668 KB with the same shape):

| Part | Pico W |
|---|---|
| The radio: cyw43's driver and its firmware (the firmware's file, `cyw43_ll.c`, is 232 KB of it) | 252 KB |
| lwIP | 105 KB |
| Web pages, stored uncompressed | 91 KB |
| C and C++ runtime, 37 KB of it a C++ name demangler (`cp-demangle.o`) | 72 KB |
| The whole application | 54 KB |
| Pico SDK | 53 KB |
| u8g2 and MUI | 32 KB |
| FreeRTOS | 28 KB |
| The HTTP server and the Trinamic library | 6 KB |
| Total | 693 KB |

- The application is 7.7% of the image. Pico W is not short of flash because of features.
- The firmware is built at `-O0`. The root `CMakeLists.txt` appends `-O0 -DDEBUG` to the Debug flags; 342 of the 354 compile commands in the v1.20 build folder carry it. CI builds Debug only.
- The same code gives two images (ADR-001's progress, measured on v1.20's code): 689 KB for Pico W with Arm's 14.2.Rel1 compiler on a PC, 645 KB in CI with Ubuntu 24.04's 13.2.1. The 40 KB is in the C and C++ runtime; the demangler is in the first and not in the second.
- v1.20's release files have the size of the PC's build, not of CI's. Releases have been built on a PC.
- `scripts/html2header.py` is run with `--no-minify`, and the pages are sent uncompressed.
- RAM on Pico W: 194 KB of 256 KB is taken at link time, 128 KB of it the FreeRTOS heap (`configTOTAL_HEAP_SIZE`). How full that heap runs is not known; ADR-001's `stats` command will say.
- The stack-overflow and failed-allocation hooks end in `assert()`. A build with `NDEBUG` compiles `assert()` away, so in such a build both hooks would do nothing.

**The boards**

| | Pico W | Pico 2 W | ESP32-S3-Pico |
|---|---|---|---|
| Flash | 2 MB | 4 MB | 16 MB |
| RAM | 264 KB | 520 KB | 512 KB and 2 MB PSRAM |
| Image today | 693 KB | 668 KB | none yet |
| Room for the application beside OTA (PR #111's layout) | about 960 KB | about 1,980 KB | set in ADR-007 |

**Handed on by ADR-012** (2026-11-15). Found by the audit of 2026-11-14, which read every module outside the motors for the kind of faults ADR-012 lists for them. Not read a second time; the figures are from ADR-012's runs. Nothing here was tried on a unit unless it says so.
- **The tasks' stacks are unmeasured.** Tasks that format floats and draw have 1 KB (256 words), with FreeRTOS's weakest overflow check. The console's `stats` names the task with the least to spare: in ADR-012's runs on the Pico 2 W it was the charge screen's, with 64 to 90 words left, and the A&D scale's with 56 while its frame tap printed. A RAM budget has these margins in it.

**Handed on by ADR-004** (2026-11-16): what `-O0` costs, measured in one place. mbedTLS's SHA-256 checks an update image (826 KB) in 4.9 s on the Pico W at `-O0` and in 1.4 s at `-O2`, and the image is 6.4 KB smaller; on the Pico 2 W 3.2 s and 0.8 s. At 5 s it stalled the web server, whose task runs the check, and browsers' reads timed out. `sha256.c` alone is now compiled at `-O2` (the root `CMakeLists.txt`); once this ADR sets the build's level, that line goes.

## The rules

1. **One firmware, one screen UI, one web UI, one HTTP server.** A feature is present on a board or absent from it. It is never built a second way for a small board. The code that touches the chip (flash, pins, the radio) is the only code that differs between boards. *(The owner's, 2026-11-13.)*
2. **A tier is what a feature needs, not a board's name.**
   - *Core*: what it takes to trickle. Charge, cleanup, profiles, scales, the screen, the web UI, Wi-Fi. On every supported board.
   - *Standard*: optional modules that fit beside the core in 2 MB and 264 KB. Update by upload from the browser, automatic tuning. Expected on all three boards.
   - *Extended*: modules that need TLS, or much more RAM or flash. A check for updates on GitHub over HTTPS, long throw histories, Bluetooth on the ESP32. Where they fit.
3. **One table in the build** says which modules a board carries. The unit reports it, and the screen menu and the web UI show only what is there.
4. **CI prints flash and RAM for each board on every build and fails over the board's budget.**
5. **A board leaves a tier, or support, only by the owner's decision in an ADR.** The likely moment for Pico W is LVGL (ADR-010): it is core, so if it does not fit there, Pico W stays on the last u8g2 release.

What tiers cost: every release is proven on each supported board, and every optional module is one more build to keep working.

## Ideas

Each of the first three is proven on the unit alone, because each changes the image a user gets.

1. **A size-optimised build.** The optimiser changes timing. Step pulses come from PIO and the loop runs at the scale's pace, so the charge should not notice; `charge_cycle` and the owner's real throws say whether it does. The two hooks stop relying on `assert()` first.
2. **Pages gzipped** and sent with the header that says so.
3. **The demangler gone.** It is pulled in by the C++ runtime's handler for an uncaught exception, which this firmware never needs. It is there with Arm's compiler and not with Ubuntu's, so this depends on which compiler builds a release.
4. **The feature table.** Build options with a default per board, one generated header, and one answer from the unit that lists what it carries.
5. **Budgets.** Two numbers per board in the repo, flash and RAM at link time, checked by ADR-001's size report in CI.

Estimate, not measured: gzipped pages, no demangler and an optimised build should bring the Pico W image near 450 to 500 KB.

## How the unit shows it works

- `charge_cycle` and `menu_tour` pass on the optimised build, and the owner's real throws are no different from before.
- The size report shows the new numbers for both boards.
- A build without an optional module shows no trace of it on the screen or in the web UI, and the unit's report lists it as absent.
- A change that takes a board over its budget fails in CI.

## Not in this ADR

- Dropping any board.
- A newer SDK or compiler. A dependency update is its own ADR.

## Decided (2026-11-17)

On the agent's proposal, closed by the owner ("Yes, close it"):

- **The trigger.** An image that overflows its slot fails to link. That failure points here: a comment in `targets/flash_region.ld.in`, and a line in AGENTS.md ("Size"). The first remedy is the optimised build, before any feature is cut.
- **Measured right before then.** ADR-008 and ADR-010 measure what they add (the pages and HTTPS, LVGL) on two builds, the shipping one and a size-optimised one, and decide on the second. If what they add fits only optimised, this ADR's optimised build is made first. ADR-010 measures the heap's high-water mark too.
- **When it fires, the work:** the optimised build on both boards, the stack-overflow and failed-allocation hooks first made to stop the unit without `assert()`, proven by `charge_cycle`, `menu_tour` and the owner's real throws (the optimiser changes timing). `sha256.c`'s own `-O2` (the root `CMakeLists.txt`) then goes.
- **Handed on:**
  - the feature table and the unit's report of what it carries (rule 3) to ADR-008, their first reader;
  - the pages' size to ADR-008, which replaces the pages and their build step;
  - LVGL's cost and the heap's high-water mark to ADR-010;
  - HTTPS's cost to wherever the unit's own fetch is designed (ADR-008);
  - to ADR-009: tuning done at `-O0` is shown again on the optimised build, if that comes after.
- **The rules:** rule 1 is the owner's. Rules 2 and 5 stand as written, as principles. Rule 3's table is ADR-008's. Rule 4's budgets in CI wait for the trigger; until then the linker's slot limit is the budget.
