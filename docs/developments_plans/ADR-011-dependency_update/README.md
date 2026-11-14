# ADR-011: the dependencies brought up to date

Status: **Open** 2026-11-14. Written in full and approved by the owner the same day. Steps 1 and 2 are built and proven on the Pico W; left: steps 3 and 4, then the Pico 2 W. What is built and measured is in [progress.md](progress.md).

## What we want

- The firmware built on current releases of what it depends on, so the ADRs after it start from a current base.
- CI that does not break when GitHub moves its runner image.
- One compiler for CI, for a PC and for a release.

## Owner's direction (2026-11-14)

- A dependency update, with a cleanup, is inserted after ADR-001, before the changes of future PRs are merged.
- The process keeps a dependency update and a functional change apart. So the cleanup of the motor drivers, which the owner named in the same breath, is ADR-012.
- This ADR goes before ADR-002: "I would like to uprev the dependency per ADR-011 first."
- The proof is on the Pico W first. A Pico 2 W is put on the bench when it is asked for; the bench holds one board at a time.
- The 2023 debug scripts are removed.
- Dependabot watches the GitHub actions, monthly.
- This ADR closes only when the Pico 2 W has passed too (at approval: "Approve, Pico 2 W too").
- The soak is 30 minutes at most, of charges and web traffic together, not an hour of one blind read: "one hour blind soak is not that useful, perhaps consider more aggressive interaction, charge cycles and internet traffic. I noticed in the past the internet traffic is the most fragile path, for which ADR-003 should fix, but before that, I think maximum 30min soak should be acceptable."
- macOS is parked: "We don't need the Mac script. The development is primarily on Windows and integration building on Linux. We can park the mac support unless I started development on mac again."
- That soak is this ADR's, not a rule for every change: "the soak test must be case by case basis, and only focus on area that needs focus. Today is a special case where we need the baseline and the tests are for dependency updates."

## Why here

- **After ADR-001.** An update is proven with ADR-001's tools: the size report, `menu_tour`, `charge_cycle`. Without them the only check is that it builds.
- **Before ADR-002.** A PR that is merged or rebuilt lands on the base it will live on. Of the open PRs, four touch the build files this ADR changes: #111 (`CMakeLists.txt`, `configure_env.ps1`), #113 (`src/CMakeLists.txt`), and the two old ones, #44 and #28.
- **Before ADR-003 and ADR-004.** The HTTP server is rewritten and the update over Wi-Fi is built once, on the lwIP and the SDK they will ship with.
- **It is the exit of the CI pin** (ADR-001, F4).

## What we know (checked 2026-11-14)

**What is pinned, and the newest**

| What | Pinned at | Newest |
|---|---|---|
| Pico SDK | 2.1.1 plus one commit, 2025-02-19 | 2.3.1, 2026-09-04 |
| picotool and `pioasm`, ready-made (`raspberrypi/pico-sdk-tools`) | 2.1.1, release `v2.1.1-3` | 2.3.1, release `v2.3.1-0`, for Windows, Linux and macOS |
| OpenOCD, ready-made (the same releases) | 0.12.0+dev from `v2.1.1-3` | 0.12.0+dev from `v2.3.1-0` |
| FreeRTOS kernel | Raspberry Pi's fork, `4f7299d`, 2024-08-08 | Upstream's V11.3.3, 2026-10-30 |
| Arm compiler | Arm's 14.2.Rel1 on a PC and on a Mac; Ubuntu 24.04's 13.2.1 in CI | Arm's 15.2.Rel1, for Windows, Linux and macOS. There is no 15.3 |
| CMake, Ninja | 3.31.5, 1.12.1 | 4.3.4 and 1.13.2 are what Raspberry Pi pairs with SDK 2.3.1 |
| u8g2 | 2.36.5 plus five commits, 2025-02-22 | Tag 2.37.1 |
| Trinamic-library | `d7debfa`, 2025-02-16 | Five commits newer, the last 2026-04-23 |
| GitHub actions | `checkout@v4`, `upload-artifact@v4` | Not checked; Dependabot will say |

- Raspberry Pi's VS Code extension names a set of tools for each SDK. For 2.3.1: Arm 15.2.Rel1, CMake 4.3.4, Ninja 1.13.2, picotool 2.3.1. For 2.1.1 it names 14.2.Rel1, 3.31.5, 1.12.1 and 2.1.1, which is the set this repository uses today. So the newest set is one Raspberry Pi puts together.

**The SDK**
- The SDK carries lwIP, the radio's driver and firmware, TinyUSB and Mbed TLS as submodules of its own. Moving the SDK moves all of them.
- From the release notes:
  - 2.2.0: lwIP to 2.2.1, the radio's driver to 1.1.0, and GCC 15 supported.
  - 2.3.0: the radio's driver again; the USB reset is taken out of `pico_stdio_usb` into a library of its own. This firmware has USB stdio switched off (`CMakeLists.txt`; its console is over RTT) and flashes through the probe, so the move does not reach it. The first draft of this ADR said otherwise.
  - 2.3.1: fixes for FreeRTOS on two cores at start-up, and for the radio being shut down and started again. `__time_critical_func` is put back as 2.1.1 had it. Build folders are to be deleted and made again after the update.
  - 2.3.1 also found rare races in the FreeRTOS RP2040 and RP2350 ports, still to be fixed in the ports. Until then the SDK works around them, by default (`LOCK_INTERNAL_SPIN_UNLOCK_WITH_NOTIFY_WAKES_ALL`).
- So the places to look for a change in behaviour are Wi-Fi, lwIP and the start of the second core.
- `http_rest.c` is a copy of lwIP's HTTP server with about 190 lines added (ADR-003). Whether it still builds and behaves against lwIP 2.2.1 is not known.

**`pioasm` and the CI pin**
- SDK 2.1.1's `tools/pioasm/pio_types.h` lacks `#include <cstdint>`. 2.2.0 and 2.3.1 have it (read in the source at each tag). That missing line is what GCC 15 rejects.
- Each SDK version has a tools release with `pioasm` and picotool ready-made for Windows, Linux and macOS.
- A PC uses the ready-made `pioasm`. CI has none, so the SDK compiles it with the runner's own compiler. That is why only CI broke.

**FreeRTOS**
- The firmware runs FreeRTOS on both cores, with core affinity, and with no MPU and no TrustZone (`FreeRTOSConfig.h`).
- Raspberry Pi's fork has one branch, and our pin is its head. Nothing has been pushed to the fork since 2024-11. Our pin was last moved on 2025-02-26, for the RP2350. So the kernel is as new as the fork; the fork stopped.
- Upstream carries the RP2040 port in the kernel itself (`portable/ThirdParty/GCC/RP2040`), last changed 2026-01-18. The two RP2350 ports are in upstream's community ports, a submodule of the kernel.
- Upstream's import file at V11.3.3 picks the RP2040 port or an RP2350 one by the board, as the fork's does. The copy in Raspberry Pi's own examples, at SDK 2.3.1, handles both the fork's layout and upstream's.
- V11.3.3 holds the community ports at their state of 2025-01-14. Three RP2350 commits came after it:
  - a fix of the path in the ports' `CMakeLists.txt`;
  - the saved context kept clear of the MPU settings beside it. With no MPU, it does not reach this firmware;
  - a variable renamed.

**The build**
- Versions are written in five places: `configure_env.ps1`, `setup_toolchain.ps1`, `setup_toolchain.sh`, `configure_env.sh`, and the CI workflow, which takes whatever Ubuntu ships.
- CI's image and a PC's differ by about 40 KB, all of it the compiler's runtime. Releases have been PC builds (ADR-005).
- The clean builds of 2026-11-13, with `-Wall`, gave 4 warnings, all inside u8g2. The firmware's own code gave none.
- `scripts/gen_version.py` asks git for the version from the build folder. A build folder outside the repository fails (tried 2026-11-14).
- `debug.sh`, `openocd_start.bat`, `openocd_start.sh`, `program.bat` and `program.sh` were last changed in 2023 and name the RP2040 only. `rc_run.py` does their work and does not use them.
- `setup_toolchain.ps1` fetches OpenOCD since ADR-001. `configure_env.ps1` and `rc_run.py` name its folder `0.12.0+dev`, the name in both tools releases.
- `rc_run.py` knows the Pico 2 W's openocd target and build folder, but not its whole-chip reset nor the addresses of its STEP pins. No Pico 2 W has been on the bench.
- The macOS scripts fetch the compiler only; the SDK builds `pioasm` there with the Mac's own compiler. There is no Mac here to run them on.
- Today's sizes, the baseline for this ADR, are in ADR-001's progress.

## What we will do

One step at a time. Each step is built, flashed and proven on the Pico W alone, and committed alone, so a regression names its cause. The images of every step, for both boards, are kept under `logs/adr011/`, so the Pico 2 W can later be taken through them in turn.

**Step 1: the SDK with its tools.**
- `library/pico-sdk` to the tag 2.3.1, with its own submodules.
- picotool and `pioasm` from the tools release `v2.3.1-0`. CMake 4.3.4 and Ninja 1.13.2, as Raspberry Pi pairs them with this SDK.
- The compiler stays, 14.2.Rel1 on a PC. CI stays on `ubuntu-24.04` with Ubuntu's compiler until step 3.
- The build folders are deleted once, as the SDK's notes ask.
- Whatever no longer builds is fixed in our code, never under `library/`. `http_rest.c` against lwIP 2.2.1 is the likely place.
- `rc_run.py` gets `settings`, a copy of the stored settings over the web compared with an earlier one, and `soak`: charges back to back with the simulated scale while two browsers use the web pages, as with the portal open on a phone and a PC (the mirror once a second, the charge page's polling every 500 ms, the portal reloaded, the settings read). Step 1 is the first change to Wi-Fi and lwIP that needs them.

**Step 2: FreeRTOS from upstream.**
- `library/FreeRTOS-Kernel` moves to `FreeRTOS/FreeRTOS-Kernel` at V11.3.3, with its community ports; `.gitmodules` names the new address.
- `CMakeLists.txt` takes upstream's import file.
- `FreeRTOSConfig.h` is brought to what V11.3.3 asks for, and nothing it sets changes in effect.
- The community ports stay where V11.3.3 holds them. If the Pico 2 W's build needs the later path fix, the RP2350 port is taken from the community ports' own repository, as a submodule of ours, and the reason is written down.

**Step 3: one compiler, its version written once.**
- Arm's 15.2.Rel1 on a PC and in CI.
- One file at the root, `toolchain.env`, names every version: the compiler, CMake, Ninja, the tools release and OpenOCD. `setup_toolchain.ps1`, `configure_env.ps1`, `setup_toolchain.sh`, `configure_env.sh`, `rc_run.py` and CI read it.
- OpenOCD stays at the build ADR-001 proved (from tools release `v2.1.1-3`). It is the instrument every proof here goes through, so it does not move in the same step as the compiler. Changed while building step 3; the first view was that it come from the same release as picotool and `pioasm`.
- CI fetches the compiler and the ready-made tools a PC uses, kept in GitHub's cache between runs. The runner's own compiler is no longer used, and the pin goes: `ubuntu-latest`.
- The stored settings are each struct as the compiler lays it out, so this is the step where they are watched.
- macOS is parked: `setup_toolchain.sh` and `configure_env.sh` are for Linux only, and `AGENTS.md` points to the Mac's scripts as last used, in git.

**Step 4: small cleanups, nothing a user sees.**
- `scripts/gen_version.py` asks git from the source folder, so a build folder outside the repository works.
- The five 2023 scripts are removed.
- `.github/dependabot.yml`: the GitHub actions, monthly.
- `AGENTS.md` brought up to date: the versions, the pin, and "CI and a PC build different images" replaced by what is true after step 3.

**Then the Pico 2 W, when the owner puts one on the bench.**
- `rc_run.py` learns the RP2350's whole-chip reset and its STEP pins.
- Main's image from before this ADR is flashed first. Its menu is saved as `tests/menu_baseline/pico2_w/`, and its settings are read.
- Then this ADR's image, with the same proofs as the Pico W. If one fails, the kept images of each step are flashed in turn to find the step.

## How the unit shows it works

After each step, on the Pico W:
- Both boards build from clean on this PC, with no new warning in our own code. CI passes on the branch.
- The size report before and after, in `progress.md`. A part that grows has its reason written down.
- `rc_run.py flash` passes: written, verified, and the console reports the build's version.
- `menu_tour --compare tests/menu_baseline/pico_w` finds every form equal. u8g2 is not touched, so the pixels must match.
- `charge_cycle` passes.
- `pulses`: the step pulses stop on a probe reset and on an assert, as before. After a hard fault they ran on before this ADR; what they do now is written down, not fixed here.
- Wi-Fi: the unit joins the owner's network and serves its pages. `soak` passes, 30 minutes: every charge passes, no restart or assert, `heap_free` ends no lower than its lowest of the first five minutes, and the web reads missed are no more than in the same run on the firmware before this ADR. Main's server already misses some under that load; the limits taken from that run are in `progress.md`, and making the server sound is ADR-003's.
- The settings read over the web API before the flash (each settings path, GET, no parameters, as ADR-001 did) are the same after it.

At the end:
- CI passes on GitHub's newest image, with no pin. Its size report matches this PC's part by part, and a difference has its reason written down.
- `picotool info` on each board's `.uf2` names the right board and the build's version.
- The owner's checks, each of which the owner may waive:
  - one real throw;
  - Wi-Fi switched off in the menu, the unit's own access point and its wizard seen from a phone, and Wi-Fi switched back on;
  - one flash by the BOOTSEL button, if the unit's USB is connected for it.
- The Pico 2 W, with the same proofs, once it is on the bench.

## Not in this ADR

- Anything a user sees.
- u8g2: ADR-010 removes it.
- The Trinamic library: ADR-012 decides whether it stays.
- The motor code (ADR-012), the HTTP server (ADR-003), an optimised build (ADR-005), the ESP32 (ADR-007).
- A watch on the SDK, FreeRTOS and the compiler. Their versions are in one file, and moving one is a decision.

## Files

- Changed: `.gitmodules`, the pins of `library/pico-sdk` and `library/FreeRTOS-Kernel`, `CMakeLists.txt`, `src/FreeRTOSConfig.h`, `setup_toolchain.ps1`, `configure_env.ps1`, `setup_toolchain.sh`, `configure_env.sh`, `.github/workflows/cmake.yml`, `scripts/gen_version.py`, `tools/remote_control/rc_run.py`, `AGENTS.md`. Perhaps `src/http_rest.c`, if lwIP 2.2.1 asks for it.
- New: `toolchain.env`, `.github/dependabot.yml`, `progress.md`.
- Removed: `debug.sh`, `openocd_start.bat`, `openocd_start.sh`, `program.bat`, `program.sh`.
- No stored struct changes. Step 3 is checked for it on the unit.

## Settled at approval

- **When this ADR closes:** after the Pico 2 W has passed too, put on the bench once, at the end. The SDK and FreeRTOS both change RP2350 code, and the Pico 2 W's image is the one the build names first. The cost is one swap of boards on the bench.
