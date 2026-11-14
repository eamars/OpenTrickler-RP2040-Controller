# ADR-011: the dependencies brought up to date

Status: **Draft** 2026-11-14. Not approved. Captures what is pinned today, what is newer, and ideas for the update. Nothing here has been built against a newer version.

## What we want

- The firmware built on current releases of what it depends on, so the ADRs after it start from a current base.
- CI that does not break when GitHub moves its runner image.
- One compiler for CI, for a PC and for a release.

## Owner's direction (2026-11-14)

- A dependency update, with a cleanup, is inserted after ADR-001, before the changes of future PRs are merged.
- The process keeps a dependency update and a functional change apart. So the cleanup of the motor drivers, which the owner named in the same breath, is ADR-012.

## Why here

- **After ADR-001.** An update is proven with ADR-001's tools: the size report, `menu_tour`, `charge_cycle`. Without them the only check is that it builds.
- **Before ADR-002.** A PR that is merged or rebuilt lands on the base it will live on. Of the open PRs, four touch the build files this ADR changes: #111 (`CMakeLists.txt`, `configure_env.ps1`), #113 (`src/CMakeLists.txt`), and the two old ones, #44 and #28.
- **Before ADR-003 and ADR-004.** The HTTP server is rewritten and the update over Wi-Fi is built once, on the lwIP and the SDK they will ship with.
- **It is the exit of the CI pin** (ADR-001, F4).

## What we know (checked 2026-11-14)

**What is pinned, and what is newer**

| What | Pinned at | Newer |
|---|---|---|
| Pico SDK | 2.1.1 plus one commit, 2025-02-19 | 2.2.0 (2025-07-29), 2.3.0 (2026-07-03), 2.3.1 (2026-09-04) |
| picotool and the SDK tools (`pioasm`) | 2.1.1 | 2.3.1 |
| FreeRTOS kernel | Raspberry Pi's fork, commit `4f7299d`, 2024-08-08. It is that fork's last commit | Upstream's V11.3.3, 2026-10-30 |
| u8g2 | 2.36.5 plus five commits, 2025-02-22 | Tag 2.37.1; last commit 2026-09-14 |
| Trinamic-library | Commit `d7debfa`, 2025-02-16 | Five commits newer, the last 2026-04-23 |
| ARM compiler | Arm's 14.2.Rel1 on a PC and on a Mac; Ubuntu 24.04's 13.2.1 in CI | Not checked |
| CMake, Ninja | 3.31.5, 1.12.1 | Not checked; nothing asks for newer |
| GitHub actions | `checkout@v4`, `upload-artifact@v4` | Not checked |

**The SDK**
- The SDK carries lwIP, the radio's driver and firmware, TinyUSB and Mbed TLS as submodules of its own. Moving the SDK moves all of them.
- From the release notes:
  - 2.2.0: lwIP to 2.2.1, the radio's driver to 1.1.0, and "GCC 15 is now supported".
  - 2.3.0: the radio's driver again; the USB reset is taken out of `pico_stdio_usb` into a library of its own. ADR-001's flashing with no hands uses that reset.
  - 2.3.1: fixes for FreeRTOS on two cores at start-up, and for the radio being shut down and started again.
- So the places to look for a change in behaviour are Wi-Fi and the USB console.
- `http_rest.c` is a copy of lwIP's HTTP server with about 190 lines added (ADR-003). Whether it still builds and behaves against lwIP 2.2.1 is not known.

**`pioasm` and the CI pin**
- SDK 2.1.1's `tools/pioasm/pio_types.h` lacks `#include <cstdint>`. 2.2.0 and 2.3.1 have it (read in the source at each tag). That missing line is what GCC 15 rejects.
- Each SDK version has a tools release with `pioasm` and picotool ready-made for Windows, Linux and macOS (seen for 2.2.0 to 2.3.1).
- A PC uses the ready-made `pioasm`. CI has none, so the SDK compiles it with the runner's own compiler. That is why only CI broke.

**FreeRTOS**
- The firmware runs FreeRTOS on both cores.
- The build takes the port through the fork's import file, which picks the RP2040 or the RP2350 port by the board.
- Upstream now carries both RP2350 ports among its community ports.
- SDK 2.3.1's notes say rare race conditions were found that "will need to be fixed in the FreeRTOS RP2040/RP2350 ports". Where those fixes land, the fork or upstream, is not checked.

**The build**
- Versions are written in five places: `configure_env.ps1`, `setup_toolchain.ps1`, `setup_toolchain.sh`, `configure_env.sh`, and the CI workflow, which takes whatever Ubuntu ships.
- CI's image and a PC's differ by about 40 KB, all of it the compiler's runtime. Releases have been PC builds (ADR-005).
- The clean builds of 2026-11-13, with `-Wall`, gave 4 warnings, all inside u8g2. The firmware's own code gave none.
- `scripts/gen_version.py` asks git for the version from the build folder. A build folder outside the repository fails (tried 2026-11-14).
- `debug.sh`, `openocd_start.bat`, `openocd_start.sh`, `program.bat` and `program.sh` were last changed in 2023 and name the RP2040 only.
- `configure_env.ps1` names an OpenOCD folder that `setup_toolchain.ps1` does not fetch.
- Today's sizes, the baseline for this ADR, are in ADR-001's progress.

## Ideas

**One dependency a step,** each built, flashed and proven alone, so a regression names its cause:
1. The SDK with its tools, to the newest release. They are one set.
2. The FreeRTOS kernel, to the repository the SDK's own examples use at that release.
3. The compiler, the same on a PC, on a Mac and in CI.

**u8g2 left where it is.** ADR-010 removes it.

**The Trinamic library left to ADR-012,** which decides whether the library stays.

**Each version written once.** One file names the versions; the Windows script, the macOS script and CI read it.

**CI fetches the named compiler and the ready-made tools,** as a PC does. Then:
- CI's image is the release image, which ADR-004 needs;
- CI no longer depends on the runner's own compiler;
- the runner pin goes.

**Told when a dependency moves.** A scheduled check, GitHub's Dependabot or a small job of our own, that opens an issue when a submodule or an action has a newer release. The next update is then a decision, not a broken build. To weigh.

**Small cleanups of the build, nothing a user sees:**
- the version script reads git from the source folder;
- the 2023 debug scripts made right for both chips, or removed;
- OpenOCD fetched by the set-up script, or taken out of `configure_env.ps1`.

## How the unit shows it works

After each step, on each supported board:
- Both boards build from clean on a PC and in CI. At the end CI passes on GitHub's newest image, with no pin.
- The size report before and after, in `progress.md`. A part that grows has its reason written down.
- `menu_tour --compare` finds every form equal to the baseline. u8g2 is not touched, so the pixels must match.
- `charge_cycle` passes.
- Flashing with no hands still works, and so does the BOOTSEL button.
- Wi-Fi: the unit joins the owner's network and serves its pages; with no network it opens its own access point and the wizard works; the display mirror runs for an hour with the free memory in `stats` steady.
- A unit that had saved settings before the update still has them after. The stored format is the compiler's layout of each struct, so a compiler change is the step to watch.
- The owner's real throw.

## Not in this ADR

- Anything a user sees.
- The motor code (ADR-012), the HTTP server (ADR-003), an optimised build (ADR-005), LVGL (ADR-010), the ESP32 (ADR-007).

## To settle before approval

- The newest SDK, 2.3.1, or the oldest that removes the pin, 2.2.0. First view: the newest; one update, not two.
- Whether CI moves to Arm's compiler. First view: yes.
- Whether u8g2 moves.
- The 2023 debug scripts: made right, or removed.
- Which boards the bench has for the proof.
