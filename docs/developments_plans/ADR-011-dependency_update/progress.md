# ADR-011: progress

What was built and measured. The plan is in the [README](README.md).

Images, size reports, build logs and settings copies of every step are kept on this PC under `logs/adr011/<step>/`, which git does not track.

## Before the first step

Measured 2026-11-14, on this PC, at ac1d07e: main's firmware with this ADR's documents.

**Sizes** (Arm 14.2.Rel1, Debug, `tools/size_report.py`)

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 701.9 KB | 676.4 KB |
| RAM at link time | 200.6 KB | 201.1 KB |

- Both boards build from clean with 4 warnings, all inside u8g2.

**The unit**
- It runs 1.20.46 at f484e19, the code of main.
- Its settings, copied with `rc_run.py settings`: 11 paths, kept in `logs/adr011/step0/settings/`.

**The soak, first as planned: an hour of the mirror alone.** Stopped at 31 minutes, when the owner asked for charges and web traffic together in its place (2026-11-14).
- `heap_free` was 96,248 at all 31 readings, `heap_min` 95,080. No restart.
- 2 of about 1,800 reads of the mirror timed out (5 s), at 20:07:27 and 20:17:12; the read after each was right.

**The soak as the owner asked: charges and two browsers.** A trial of 2 minutes on this firmware, 4 charges, all passed, `heap_free` steady:
- 33 of 407 web reads missed (8%); one poller missed 3 in a row.
- The misses fall in every state of the charge, before the first charge too. They follow the load of the web, not the motors.
- So the web reads are judged against the same run before this ADR, not a fixed rule.

**The baseline soak:** 30 minutes on this firmware, 20:29 to 20:59, with no limit on the web reads so that it measures them. `VERDICT PASS`.
- 62 charges, every one passed, each ending at 30.000 for 30.
- `heap_free` 96,248 at all 63 readings. No restart, no assert.
- 433 of 6,655 web reads missed: 6.51%.

| Poller | Right | Missed | Longest run |
|---|---|---|---|
| Browser 1, mirror | 1,152 | 75 | 3 |
| Browser 1, charge state | 1,955 | 120 | 3 |
| Browser 2, mirror | 1,182 | 72 | 2 |
| Browser 2, charge state | 1,867 | 140 | 3 |
| Portal page | 51 | 10 | 2 |
| Settings, all 11 paths in one read | 15 | 16 | 4 |

- By kind: 333 timed out (5 s); 100 were charge states that came back as broken JSON.

**The limits every step of this ADR is held to,** from that run: at most 10% of the web reads missed and at most 6 in a row on one poller (`--max-miss-share 0.10 --max-miss-run 6`). 10% is about one and a half times the baseline's share; the 2-minute trial gave 8%, so a run's share moves by a point or two. 6 in a row is twice the longest run of the pollers that make one request a read (3); the settings poller's 4 above was a run of whole sweeps, see step 1. A step past either limit made the web worse and is looked into. Charges, restarts and the heap are judged as before, with no allowance.

**Found on the way**
- **Every JSON handler of the web API answers from one static buffer.** It writes the reply into the buffer and gives lwIP a pointer to it; lwIP sends it later. A second request to the same path, from another browser, rewrites the buffer while the first answer is still being sent, and the first browser gets a mix: the 100 broken charge states above. 13 handlers in 9 files do it (`charge_mode.cpp`, `cleanup_mode.cpp`, `motors.c`, `scale.c`, `servo_gate.c`, `neopixel_led.c`, `system_control.c`, `wireless.c`, `mini_12864_module.cpp`). Two phones watching one charge see a wrong state now and then. For ADR-003.
- `/display_buffer` answers with the bare 1,024 bytes of the screen and no HTTP header, though its handler sets `FS_FILE_FLAGS_HEADER_INCLUDED`. A browser takes it; Python's HTTP client does not, so `soak` reads it over a plain socket. For ADR-003.
- Deleting `build\` for a clean build deletes `build/.placeholder`, a tracked file.

## Step 1: the SDK with its tools

Built 2026-11-14.

**What moved**
- The Pico SDK from 2.1.1 plus one commit to 2.3.1. With it: lwIP 2.2.1, cyw43-driver 1.1.1, TinyUSB 0.18.0, Mbed TLS 3.6.6.
- picotool and `pioasm` 2.3.1 (tools release `v2.3.1-0`), CMake 4.3.4, Ninja 1.13.2. The build's cache shows each one in use.
- The compiler did not move: 14.2.Rel1.

**The build**
- Both boards build from clean with no change to our code. `http_rest.c`, the copy of lwIP's server, builds against lwIP 2.2.1 as it is.
- The same 4 warnings inside u8g2. CMake 4 adds one note: u8g2's `cmake_minimum_required(VERSION 3.5)` will stop working in a later CMake. u8g2 is ADR-010's.

**Sizes**

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 701.9 to 647.9 KB (−54.0) | 676.4 to 626.4 KB (−50.0) |
| RAM at link time | 200.6 to 196.9 KB (−3.7) | 201.1 to 197.0 KB (−4.1) |

- All of the fall is the C and C++ runtime: 71.8 to 16.9 KB on Pico W, 69.0 to 19.2 KB on Pico 2 W. The other parts moved by less than 1 KB.
- **Why.** SDK 2.3.0 supplies the guards of C++ static initialisers (`__cxa_guard_acquire` and the rest) itself (SDK commit 6685d000). They came from libstdc++'s `guard.o` before, whose error path calls `std::terminate`, and that linked the verbose terminate handler: the name demangler (`cp-demangle.o`, 38 KB on Pico W), the exception and unwind code, and newlib's FILE writing. None of it is in the image now; the map names none of those objects.
- The firmware is built with `-fno-exceptions`, so nothing it does reached that code. A failed guard now ends in the SDK's `panic`.

**CI** ([run 43568615732](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/runs/43568615732), started by hand on the branch; still `ubuntu-24.04` with Ubuntu's compiler, 13.2.1): passed.

| | Pico W | Pico 2 W |
|---|---|---|
| Flash, CI | 648.9 KB | 627.1 KB |
| Flash, this PC | 647.9 KB | 626.4 KB |
| C and C++ runtime, CI | 16.5 KB | 18.7 KB |

- CI's image and this PC's are now within 1 KB. The 40 KB between them that ADR-001 measured was all in the C and C++ runtime, and it went with the code above. That CI's old image lacked exactly that code is likely, not checked: CI's old map was not read.

**On the unit** (the bench's Pico W, 2026-11-14, from 21:00)
- `settings` before the flash: 11 paths copied (`logs/adr011/step1/settings_before/`).
- `flash`: written and verified; the unit runs 1.20.59 at d9e5570 10.3 s after the start.
- `settings --compare`: every stored value as before; only the version and its hash differ, as they should. The first try, 6 s after the start, had one read time out; `settings` now tries each path up to three times, since how the web answers is soak's to judge. The second try passed.
- `menu_tour --compare tests/menu_baseline/pico_w`: all 22 forms equal, but for form 36, the version, which is not judged.
- `charge_cycle`: passed, 30.000 for 30, 16.7 s from charging to the cup prompt.
- `pulses`: the pulses stop on a reset through the probe and on an assert. After a hard fault they run on, as before this ADR.

**The soak, first run** (21:02 to 21:32): `VERDICT FAIL`, on "settings missed 7 in a row, more than 6". All else as good as the baseline or better:

| | Baseline | Step 1 |
|---|---|---|
| Charges | 62, all passed | 62, all passed |
| `heap_free` | 96,248 throughout | 97,320 at the start, 96,152 from the first charge on |
| Web reads missed | 433 of 6,655, 6.51% | 409 of 6,834, 5.98% |
| Longest run, pollers of one request a read | 3 | 3 |
| Settings sweeps missed | 16 of 31, longest run 4 | 15 of 31, longest run 7 |

- **The cause was the test.** The settings poller did its 11 requests as one read, missed when any of them failed. At about 6% a request, a sweep misses about half the time (1 − 0.94¹¹) on either firmware, as both runs show. A run of misses of a coin is 4 to 7 long in 31 tries by chance, and says nothing about a server that stopped.
- **Fixed in `rc_run.py`:** each request of a round is one read now, the settings' too, so a share and a run mean the same for every poller. A trial of 65 s after the fix: 362 reads, 5.8% missed, the settings poller 22 reads.
- The broken JSON came back as in the baseline: 116 of the misses, all from the charge state.
- The heap's one step down at the first charge is an allocation made once; it held at 96,152 to the end.

**The soak, second run** (21:35 to 22:03, with each request a read): `VERDICT FAIL`, on console replies lost to the probe's link, not on the unit.
- The web: 392 of 6,500 reads missed, 6.03%; longest run 2. Within the limits (10%, 6), and as the baseline.
- `heap_free` 96,152 at all 56 readings. 55 charges, every one passed, each 30.000 for 30.
- **At 22:02:39, during the 56th charge, the probe's link dropped.** openocd read the debug port's id again, then "Failed to read memory at 0x20001518" (the RTT block) and "rtt: Failed to read up-channel 0 description", and read RTT no more. Every console command after it got no reply within 5 s, and the run failed on those.
- **The unit was well throughout.** Its last state before the drop: charging, 29.8 of 30, `up=3637`. On a new connection a minute later the replies waiting in its buffer came through: it had carried out the run's `state`, `charge stop`, `sim off` and `stats`, `up` going 3637, 3647, 3657, 3709, never back. `heap_free` 96,152. The web was answered to the end of the run.
- **On this bench before:** RTT read failures were seen only across a reset of the chip, where they are expected. This is the first drop with the chip running: one in about 2.5 hours of soak with the motors turning, none in the baseline's hour. Whether the new firmware, the motors' noise on the SWD wires or chance caused it, one case cannot say. The drops are counted from here on, so a drop that comes back only on newer firmware shows.
- **Fixed in `rc_run.py`:** when a reply is late and openocd has reported an RTT read failure, RTT is started again once and the wait starts over, without sending the line again. A unit that truly stopped answering still fails. Tried on the bench by stopping RTT by hand: the late reply came, nothing was sent twice, `VERDICT PASS`.

**Step 1 passes,** judged on the second run's record rather than a third run: every measure of the unit is within its limits (charges, the heap, no restart, the web), and each failure in the verdict is a reply the unit gave and the probe did not carry. Steps 2 and 3 run the soak with the fixed tool.

## Step 2: FreeRTOS from upstream

Built 2026-11-14.

**What moved**
- `library/FreeRTOS-Kernel` from Raspberry Pi's fork (`4f7299d`, 2024-08-08, the head of its only branch) to `FreeRTOS/FreeRTOS-Kernel` at V11.3.3. The RP2040 port is in the kernel; the RP2350 ports come from its community ports, held at `bae4c7a` (2025-01-14).
- `.gitmodules` names the new address.

**The build**
- The Pico W built as it was. The Pico 2 W did not: "does not contain a 'rp2350-arm-s' port here: portable/ThirdParty/GCC/RP2350_ARM_NTZ".
- **Why.** Upstream's import file searches for the kernel in two passes, the fork's paths first, then the community ports'. On the first pass it found the kernel as the SDK's sibling and stopped there, with the first pass's RP2350 path, which only the fork had. It never tried the second.
- **Fixed in our `CMakeLists.txt`:** the kernel's location is given (`FREERTOS_KERNEL_PATH`), the case the import file is written for. It then tries both paths and takes the RP2350 port from the community ports. Nothing under `library/` changed.
- `FreeRTOSConfig.h` needed no change. The same 4 warnings inside u8g2, nothing else.

**Sizes**

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 647.9 to 649.6 KB (+1.7) | 626.4 to 628.0 KB (+1.6) |
| RAM at link time | 196.9 KB, unchanged | 197.0 KB, unchanged |

- FreeRTOS +1.5 KB, the SDK +0.2 KB. Two years of upstream: the walk over the task lists reworked (`uxTaskGetSystemState` split into `prvCallForEachTask` and others), the recursive mutex and the queue's delete grown, many functions a few bytes each.
- **Core 1 is reset before it starts** (`multicore_reset_core1`, 168 bytes, called by upstream's RP2040 port as the scheduler starts; upstream commit f0d79459d, "Fix SMP debugging issue on rp2040").

**CI** ([run 43579297767](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/runs/43579297767)): passed. Pico W 650.5 KB, Pico 2 W 628.7 KB.

**On the unit** (the bench's Pico W, from 22:10)
- `settings` before, `flash` (1.20.65 at 7913330, 10.3 s after the start), `settings --compare` (every stored value as before; one read needed a second try), `menu_tour --compare` (22 forms equal), `charge_cycle` (30.000 for 30): all passed.
- **The flash's verdict failed on "core not running: rp2040.core1 unknown".** The core ran: halted and read, it was in FreeRTOS's idle task. Right after a flash openocd answers "unknown" for core 1, even after examining it again, and "running" two seconds later. It follows the reset of core 1 above: each core of the RP2040 has its own debug port, and openocd needs a moment to see core 1 again. `rc_run.py` now asks again for up to 5 s and judges the first real answer; a core still unknown after that fails as before. The flash's verdict then passed.
- **`pulses`, first run: two starts with no console.** After the assert test image and then the hard-fault one were flashed, the unit printed its start up to the motors' set-up and then nothing; the console never answered. The normal image, flashed after, started at once. Not seen again in 14 more starts of the test images: 8 in a row of the assert image, and two more runs of `pulses`, both `VERDICT PASS` (pulses stop on a reset and on an assert, run on after a hard fault, as on step 1). Not explained. `rc_run.py` now halts both cores and logs where they are when the console does not come back after a restart, so a next one leaves evidence.
- **The soak** (22:24 to 22:55): `VERDICT PASS`. 62 charges, every one passed, each 30.000 for 30. `heap_free` 97,320 at the start and 96,152 from the first charge on, as on step 1. No restart (`up` from 14 to 1,827), no drop of the probe's link.

| | Baseline | Step 1, second run | Step 2 |
|---|---|---|---|
| Web reads missed | 6.51% | 6.03% | 7.02% (471 of 6,705) |
| Longest run on one poller | 3 | 2 | 4 |
| Broken JSON among the misses | 100 | 116 | 110 |

- The web stays where it was: the share moves by about a point from run to run, within the limits (10%, 6).

**Step 2 passes.**

## Step 3: one compiler, its version written once

Built 2026-11-14.

**What moved**
- The compiler from Arm's 14.2.Rel1 to 15.2.Rel1 (`arm-none-eabi-gcc` 15.2.1 20251203), on a PC and in CI.
- `toolchain.env` names every version. `setup_toolchain.ps1`, `configure_env.ps1`, `setup_toolchain.sh`, `configure_env.sh`, `rc_run.py` and CI read it; a version is written nowhere else.
- `setup_toolchain.sh` now works on Linux too, where it fetches the set a Windows PC uses: the compiler, CMake, Ninja, `pioasm` and picotool. CI runs it, keeps `.toolchain/` in GitHub's cache keyed on `toolchain.env`, and builds with Ninja as a PC does. The runner's own compiler is no longer used, and the pin went: `ubuntu-latest`.
- macOS was parked by the owner the same day: the shell scripts are for Linux only, and the Mac's as last used are in git at 2ad9b0a.
- `configure_env.sh` no longer sets `set -euo pipefail` in the shell that sources it.
- OpenOCD did not move: it is the instrument of every proof. Changed from the plan while building.

**The build**
- GCC 15 found one fault in our code: `cleanup_mode.h` tests `CLEANUP_MODE_H_` and then defines `CLENAUP_MODE_H_`, so its guard never held (`-Wheader-guard`, three times, once a file that includes it). The spelling is fixed. Every other header in `src/` was checked for the same slip; none has it.
- Then the same 4 warnings inside u8g2 and none of ours.

**Sizes**

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 649.6 to 649.3 KB (−0.3) | 628.0 to 627.5 KB (−0.5) |
| RAM at link time | 196.9 to 197.3 KB (+0.4) | 197.0 to 197.3 KB (+0.4) |

- The RAM is newlib's, which comes with the compiler: its locale table (`__global_locale`, 364 to 596 bytes) and its data for the program (`_impure_data`, 320 to 512), 424 of the 456 bytes on Pico W.

**CI** ([run 43583322068](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/runs/43583322068)): passed on its first run, on `ubuntu-26.04`, the image whose compiler could not build SDK 2.1.1's `pioasm`. It fetched the set from `toolchain.env` and built no `pioasm` of its own.

| | Pico W | Pico 2 W |
|---|---|---|
| Flash, CI | 650.6 KB | 628.7 KB |
| Flash, this PC | 649.3 KB | 627.5 KB |
| RAM, both | 197.3 KB | 197.3 KB |

- **The 1.3 KB between them is the paths.** An assert stores its source file's full path. CI's checkout path is 35 characters longer than this PC's, and the image holds 37 paths on Pico W and 34 on Pico 2 W: 1,295 and 1,190 bytes, as measured. The parts that differ are the ones with asserts: the radio's driver, the application, the SDK, FreeRTOS. So CI builds the image a PC builds, but for where the tree sits.

**On the unit** (the bench's Pico W, from 23:03)
- `settings` before; `flash` (1.20.68 at ae1daa8, 10.3 s after the start, `VERDICT PASS`); `settings --compare`: every stored value as before, the version and its hash aside. This was the step to watch for the stored layout, and nothing moved.
- `menu_tour --compare`: 22 forms equal. `charge_cycle`: 30.000 for 30.
- `pulses`: stop on a reset and on an assert, run on after a hard fault, as on steps 1 and 2. Every start had its console.
- **The soak** (23:06 to 23:36): `VERDICT PASS`, but at both limits of the web: 576 of 6,194 reads missed, 9.30%, and 6 in a row. 62 charges, every one passed; `heap_free` as on steps 1 and 2; no restart, no drop of the probe's link.
- **The misses rose partway through, not from the start.** Misses per 5 minutes: 50, 56, then 87, 122, 127, 121. The timeouts tripled from about 23:16; the broken JSON stayed as in the baseline (15 to 26 per 5 minutes), so no third poller of the charge state. The owner had a browser open on the unit then, and another agent read it once near the end.
- **A clean run of 5 minutes** (23:40, the browser closed, nothing else on the unit): 91 of 1,131 reads missed, 8.05%, longest run 3, 11 charges passed, `VERDICT PASS`. Per 5 minutes, 63 timeouts and 85 misses: within the baseline's (45 to 66, 62 to 84) and step 2's (51 to 72, 63 to 94), not the 95 to 112 timeouts of the browser's twenty minutes.
- So the rise was the other client, not the compiler. A soak's web figures compare only when nothing else uses the unit's web pages; `AGENTS.md` says so now.
- Stopping the diagnostic runs mid-charge left the simulated scale on, and `settings --compare` then showed `scale_config s0` as 9. With `sim off` every stored value was as after step 3's flash: the other agent's read changed nothing stored.

**Step 3 passes.**
