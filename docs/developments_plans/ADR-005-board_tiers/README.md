# ADR-005: what each board carries: a smaller image, a feature table, budgets

Status: **Draft** 2026-11-13. Not approved. Rule 1 below is the owner's; the rest is a proposal.

## What we want

- An image no larger than it needs to be.
- One place that says which optional modules a board carries, and a unit that reports it.
- A build that fails when a board no longer has room, in the PR that causes it.

## Owner's direction (2026-11-13)

- Boards are supported in tiers: some features only where there is room.
- The implementation is consistent: a feature is present on a board or absent from it, never built a second way.

## Why here

After ADR-004 the application's room on each board is a known number, so "does it fit" can be answered and a tier means something.

## What we know (checked 2026-11-13)

**The image**, from `tools/size_report.py` on the Pico W build (v1.20's code; the Pico 2 W image is 663 KB with the same shape):

| Part | Pico W |
|---|---|
| The radio: cyw43's driver and its firmware (the firmware's file, `cyw43_ll.c`, is 232 KB of it) | 252 KB |
| lwIP | 105 KB |
| Web pages, stored uncompressed | 90 KB |
| C and C++ runtime, 37 KB of it a C++ name demangler (`cp-demangle.o`) | 72 KB |
| Pico SDK | 53 KB |
| The whole application | 52 KB |
| u8g2 and MUI | 32 KB |
| FreeRTOS | 28 KB |
| The HTTP server and the Trinamic library | 6 KB |
| Total | 689 KB |

- The application is 7.5% of the image. Pico W is not short of flash because of features.
- The firmware is built at `-O0`. The root `CMakeLists.txt` appends `-O0 -DDEBUG` to the Debug flags; 342 of the 354 compile commands in `build/` carry it. CI builds Debug only, and the releases are those builds.
- `scripts/html2header.py` is run with `--no-minify`, and the pages are sent uncompressed.
- RAM on Pico W: 194 KB of 256 KB is taken at link time, 128 KB of it the FreeRTOS heap (`configTOTAL_HEAP_SIZE`). How full that heap runs is not known; ADR-001's `stats` command will say.
- The stack-overflow and failed-allocation hooks end in `assert()`. A build with `NDEBUG` compiles `assert()` away, so in such a build both hooks would do nothing.

**The boards**

| | Pico W | Pico 2 W | ESP32-S3-Pico |
|---|---|---|---|
| Flash | 2 MB | 4 MB | 16 MB |
| RAM | 264 KB | 520 KB | 512 KB and 2 MB PSRAM |
| Image today | 689 KB | 663 KB | none yet |
| Room for the application beside OTA (PR #111's layout) | about 960 KB | about 1,980 KB | set in ADR-007 |

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
3. **The demangler gone.** It is pulled in by the C++ runtime's handler for an uncaught exception, which this firmware never needs.
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

## To settle before approval

- Rules 2 to 5.
- Which modules are optional from the start.
- The budgets' numbers, once the optimised build is measured.
