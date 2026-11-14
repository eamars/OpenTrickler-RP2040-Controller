# ADR-001 progress

What was built and measured, step by step. The plan is in the [README](README.md).

## Step 1: the process

Built 2026-11-13.

- `AGENTS.md`, `requirements.txt`, and `venv/` and `logs/` in `.gitignore`.
- **The check:** a fresh agent was given `AGENTS.md` and nothing else, and asked five questions: how to get a Pico 2 W image from a clone, what proves a change, whether it may flash a unit now, what adding a field to a settings struct does, and whether it may merge a contributor's PR. All five answers were right.
- It named two gaps, both fixed in the file:
  - the file did not say where `app.uf2` ends up (`build\app.uf2`, `build_w\app.uf2`);
  - it was not clear that flashing with no hands is still under the bench rule (it is: the rules hold for every way of reaching a unit).

## Step 2: build here, and know the size

Built 2026-11-13, CI run included.

**Was a toolchain already on this PC?** No. Searched: `PATH`, `.pico-sdk` under every user profile, Program Files, ProgramData, AppData and the VS Code folders. The VS Code Pico extension (0.23.0) is installed but had fetched nothing. The old build folders' caches named `C:\Users\Ran Bao\.pico-sdk`, a profile that is not on this PC.

**`setup_toolchain.ps1`** (written as `tools/setup_toolchain.ps1`; moved to the root later, see "The branch was cut from an old main")

| Part | Fetched | Reports |
|---|---|---|
| Arm GNU toolchain | 298 MB | `arm-none-eabi-gcc` 14.2.1 20241119 (14.2.Rel1) |
| CMake | 44 MB | 3.31.5 |
| Ninja | 0.3 MB | 1.12.1 |
| picotool | 1.5 MB | v2.1.1 |
| pioasm (pico-sdk-tools) | 0.8 MB | present with its CMake files |

- 1.4 GB on disk in `%USERPROFILE%\.pico-sdk`. `configure_env.ps1` is unchanged.
- A second run fetched nothing and left nothing in the temp folder.
- **The first run failed** before it installed anything. The script assumed the Arm archive holds one top folder; it holds its files directly. The cause was an assumption written down per archive, so the fix was to stop assuming: the script now looks for the file it needs inside what it unpacked. It also removes its temp folder when it fails, which the first version did not (about 1.3 GB was left behind, and removed by hand).

**The old build folders.** `build/` and `build_w/` held the v1.20 build from another PC, and their CMake caches named that PC's compiler. They were moved aside, not deleted: `build_v1.20_reference/` and `build_w_v1.20_reference/` (both ignored by git). `build/.placeholder`, which is tracked, stayed.

**The builds,** from empty build folders, with the repository's own scripts:

| | `build_pico_2w.ps1` | `build_pico_w.ps1` |
|---|---|---|
| Result | 368 of 368 steps | 368 of 368 steps |
| Time | 8 s | 6 s |
| Version in the image | 1.20.14-dirty at 7c70131 (step 1's commit; step 2's files were not yet committed) | the same |
| Warnings | 4 | 4 |

- All the warnings are in u8g2's MUI (`library/u8g2/csrc/mui.c`, `mui_u8g2.c`): an unused function and an unused variable, each printed twice. None is from `src/`.

**The sizes** (`tools/size_report.py`). These two tables are of v1.20's code, which is what the branch held at the time; today's main is measured further down.

| | Pico 2 W | Pico W |
|---|---|---|
| Flash | 663.4 KB of 4,096 | 688.6 KB of 2,048 |
| RAM at link time | 195.4 KB of 512 | 194.8 KB of 256 |
| The report's total against `app.bin` | +0.3% | +0.3% |

| Flash by part | Pico 2 W | Pico W |
|---|---|---|
| The radio: cyw43's driver and its firmware | 249.9 KB | 251.6 KB |
| lwIP | 93.4 KB | 105.4 KB |
| Web pages | 89.7 KB | 89.8 KB |
| C and C++ runtime | 68.9 KB | 71.6 KB |
| Pico SDK | 50.3 KB | 52.9 KB |
| Application | 49.2 KB | 51.6 KB |
| FreeRTOS | 29.5 KB | 28.4 KB |
| u8g2 and MUI | 27.9 KB | 31.5 KB |
| HTTP server | 3.5 KB | 4.0 KB |
| Trinamic | 1.1 KB | 1.6 KB |

- The image built here and v1.20's image from the other PC give the same table to 0.1 KB. That was read as "main has no code change since v1.20". It was the local `main` that had none; the remote's had 11 commits more (below).
- **A fault found in the report, and fixed.** Its first version told the parts apart by words anywhere in an object's path. On this PC the compiler lives in a folder named `.pico-sdk`, so the C runtime (70 KB) was counted as the Pico SDK; the same loose matching had put `mui_menu.c` under u8g2 and `freertos_hooks.c` under FreeRTOS. A part is now told by where its source sits in the repository, and anything from outside the tree is the compiler's runtime. The table no longer depends on where anything is installed.
- The numbers in the first drafts of ADR-005 and ADR-010 came from the loose version and were corrected (the application is 52 KB on Pico W, not 48; u8g2 and MUI 32 KB, not 35).

**The CI run.** The owner said to push the branch and start a run by hand (CI runs on pushes to `main`, on PRs, and when started by hand).

- **The first run failed, and not because of this branch.** It stopped in "Build Pico2 W Package", before the size report:
  - `ubuntu-latest` is now `ubuntu-26.04`, whose own compiler is GCC 15.2;
  - the Pico SDK builds its `pioasm` tool with the runner's compiler, and SDK 2.1.1's `pio_types.h` uses `uint8_t` without including `<cstdint>`, which GCC 15 no longer lets pass;
  - main's last green run was 2026-07-28, on the image before. So CI has been broken on main since the image moved, for any PR.
- **The fix:** the runner is pinned to `ubuntu-24.04`, the image the build last passed on, with a comment that says why and when to move it (when the SDK is updated, which is a dependency ADR of its own).
- **The second run passed** ([run 43410387723](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/runs/43410387723)), both boards, with both tables in the job summary and in the log:

| In CI | Pico 2 W | Pico W |
|---|---|---|
| Flash | 624.3 KB | 645.1 KB |
| RAM at link time | 195.5 KB | 195.1 KB |
| The report's total against `app.bin` | −0.1% | −0.1% |
| C and C++ runtime | 29.1 KB | 27.3 KB |

- The report works on a map made by Make on Linux as it does on one made by Ninja on Windows: same parts, object names ending `.o` there and `.obj` here.
- **The same code gives two images.** CI's are about 40 KB smaller than this PC's, all of it in the C and C++ runtime. The compilers differ: CI installs Ubuntu 24.04's `gcc-arm-none-eabi` (13.2.1), this PC has Arm's 14.2.Rel1, which is what `configure_env.ps1` names. The 37 KB name demangler is in the image from Arm's compiler and not in CI's.
- **The releases are not CI's builds.** v1.20's release files are 1,323 KB and 1,374 KB as `.uf2`, which is 661 KB and 687 KB of image: the size of a build with Arm's compiler on a PC, not of CI's (625 KB and 646 KB). ADR-005 said otherwise and is corrected. Which compiler is the reference for a release is for ADR-004's release job to settle.

## The branch was cut from an old main

Found 2026-11-13, while opening the PR for the CI pin.

- **What happened.** The local `main` was 11 commits behind the remote's, and the agent never ran `git fetch`. The survey, all ten ADRs and steps 1 and 2 were done against the code of 2026-04-28. The PR and issue lists came from GitHub and were current.
- **What main had gained:**
  - a simulated scale, as scale driver 9, in the screen menu and the web page (PR #107);
  - a short reverse of the coarse trickler when it stops, with three new stored settings (PR #108);
  - the reset button as "up one level" in the menu (PR #110);
  - a fix for REST values that contain `&` (PR #112);
  - `setup_toolchain.sh` and `configure_env.sh` for macOS, and `.toolchain/` in `.gitignore`.
- **What was done:**
  - main merged into this branch. One conflict, in `.gitignore`; both sides kept;
  - `setup_toolchain.ps1` moved from `tools/` to the root, beside the macOS script and `configure_env.ps1`;
  - both boards rebuilt and measured again (below);
  - the facts corrected in ADR-001, 002, 003, 005, 006, 009 and 010;
  - "fetch first" written as the first rule in `AGENTS.md`.
- **What it changes in this ADR.** Step 5 planned a simulated scale and main has one. That is F2 in the README, with a proposal, and it waits for the owner.

**Today's main, built on this PC:**

| | Pico 2 W | Pico W |
|---|---|---|
| Flash | 667.7 KB of 4,096 | 693.0 KB of 2,048 |
| RAM at link time | 195.5 KB of 512 | 195.0 KB of 256 |
| The report's total against `app.bin` | +0.3% | +0.3% |
| The radio: cyw43's driver and its firmware | 249.9 KB | 251.6 KB |
| lwIP | 93.4 KB | 105.4 KB |
| Web pages | 91.2 KB | 91.3 KB |
| C and C++ runtime | 69.0 KB | 71.7 KB |
| Application | 51.4 KB | 53.7 KB |
| Pico SDK | 50.3 KB | 52.9 KB |
| FreeRTOS | 29.5 KB | 28.4 KB |
| u8g2 and MUI | 27.9 KB | 31.6 KB |
| HTTP server | 4.0 KB | 4.8 KB |
| Trinamic | 1.1 KB | 1.6 KB |

- 4 KB more than v1.20 on each board: about 2 KB of application, 1.5 KB of pages, the rest the HTTP server.

**The CI pin on main.** The pin went to main on its own as PR #114 and was merged by the agent, under the owner's rule of the same day that an update that is not a feature may be merged without asking.

## Step 3: the console, and flashing with no hands

Built 2026-11-14 on the bench's Pico W, through the debug probe (the revision for the bench, F5, approved the same day). Left: the owner's check by hand.

**Before any contact**
- The unit's settings were read from its web API (GET only) and kept in `logs/unit_backup/`, because this branch resets its charge-mode settings (F6).
- The probe: Raspberry Pi debugprobe, firmware 2.0.1. openocd calls it old and uses a slower workaround for it.

**First contact.** openocd read the chip id through the probe with the unit left running: `0x20002927`, an RP2040 rev B2. The unit's web page kept answering.

**openocd.** `setup_toolchain.ps1` fetched `openocd-0.12.0+dev-x64-win.zip` (decision 4), and a second run fetched nothing.

**What was built**
- `CMakeLists.txt`: stdio over RTT, the UART and USB consoles off, `PICO_ENTER_USB_BOOT_ON_EXIT=1`, and RTT buffers of 2 KB out and 256 bytes in. `RC_TEST_FAULT=1` or `2` makes the test builds.
- `src/remote_control.c`: `help`, `state`, `stats`, `reboot`. The unit id and the profile name are stored without a terminator, so they are printed by their length.
- `src/rc_console.c`: the console task, polling every 20 ms (RTT has no interrupt). It prints `@hello remote control ready` when it starts.
- `stdio_init_all()` is now the first call in `main()`.
- `tools/remote_control/rc_run.py`: `send`, `flash`, `reset`, `watch`.
  - It starts openocd with no gdb or telnet port, drives it through its Tcl port, and finds the RTT control block at the address of `_SEGGER_RTT` in the ELF.
  - `reset` is a watchdog reset written through the probe (F8).

**F7, found at the first flash: the image did not start.** The boot log ended at `Unable to add program, err: -20` (PIO version mismatch), so the unit stopped at the motor error before any task ran.
- The cause: both boards wrote their PIO headers to the shared `src/generated/`. The Pico 2 W build had marked `stepper.pio` as PIO version 1, and the Pico W build reused that header.
- CI's log of main shows the same thing: the second build (Pico W) generates no `stepper.pio.h`. So CI's Pico W image has this fault.
- The fix: PIO headers go to each build folder. `build\src\stepper.pio.h` now says version 1, and `build_w\src\stepper.pio.h` says 0.

**Proof, on the Pico W** (logs in `logs/rc/`, this PC)

| Done when | Result |
|---|---|
| `flash` passes five times in a row, nobody touching the unit | Passed. 11.2 to 12.1 s from start to the console answering with the build's `ver` and `vcs`. Each flash is 10 s of writing and 1 s of verifying |
| An assert test build puts itself in the USB loader; `flash` brings it back | Passed. The assert's line reached the PC (`assertion "false" failed: ... rc_console.c, line 101`), and the runner counted it. Core 0 was then in the boot ROM (pc 0x20e0) and the web page was gone. `flash` brought the unit back |
| A hard-fault test build is brought back by `flash` | Passed. With the probe attached, core 1 halted at the breakpoint in `isr_hardfault` (`crt0.S:170`) and openocd halted core 0 with it. The runner named both places. `flash` brought the unit back |
| `state` and `stats` answer; the log shows the firmware's `printf` lines | Passed. Boot lines, the radio's firmware lines and `connect status` lines are logged. `state` reads the real scale: `weight=0.000`, `scale="AND FX-i Std"` |
| With the probe's USB cable out, the unit starts and the owner charges by hand | **Waits for the owner** |
| What the console costs | See below |

Also run:
- **`reboot`** passes: the next `state` shows `up=1`.
- **`reset`** passes three times in a row, and it brought back a unit left locked up by openocd's own `reset run` (F8).
- **An unknown command** gets `@err unknown command "bogus"`, which the runner counts as a failure.

**What the console costs** (`tools/size_report.py`, this PC's compiler)

| | Pico W | Pico 2 W |
|---|---|---|
| Flash, before → after | 693.0 → 697.9 KB (+4.9) | 667.7 → 672.5 KB (+4.8) |
| RAM at link time, before → after | 195.0 → 199.2 KB (+4.2) | 195.5 → 199.7 KB (+4.2) |

- **The flash:** about 1.8 KB of application, 1.9 KB of SDK (RTT and stdio), and 1.1 KB of FreeRTOS (`uxTaskGetSystemState`).
- **The RAM:** mostly the RTT buffers (2.3 KB), the task list `stats` reads (0.9 KB), and the reply and line buffers.
- **Measured on the unit:** `stats` gives a free heap of 97.5 KB, with the lowest stack margin in "Scale Task" (185 words).

**Seen, not chased**
- Every `program` prints `Execution of event reset-init failed: args[i] option value ('CX') is not valid` from openocd's RP2040 script. Flashing and verifying still pass.
- openocd prints read errors while the chip restarts (`Failed to select multidrop`, `Polling failed`). It examines the chip again and carries on.

## Step 4: press, form, snap, and the menu tour

Built 2026-11-14 on the bench's Pico W.

**What was built**
- `press <cw|ccw|enc|rst> [n]`: the event goes into `encoder_event_queue`, 50 ms apart, so the menu draws between presses.
- `form` and `form <id>`:
  - A new event, `OVERRIDE_GOTO_FORM`, is handled in `menu_task`, so only that task touches the `mui_t`.
  - `menu_task` answers after it has drawn the form, so a snap taken next shows it.
  - It publishes the form on screen after each draw, and −1 while a mode has the screen.
  - `state` gains `form=`.
- `snap`: one copy of the u8g2 buffer, sent as `@snap 128 64 u8g2 r=<rotation>` and four `@sd <base64>` lines.
  - The runner undoes u8g2's rotation. The bench's display is set to R2, 180°, so a picture is what a person at the unit sees.
  - A line waits up to 500 ms for room in the RTT buffer instead of being dropped.
- `rc_run.py`:
  - `snap` saves a PNG under `logs/rc/snaps/`;
  - `menu_tour [--save <folder>] [--compare <folder>]`;
  - a picture is kept only when two snaps in a row are equal;
  - the PNGs are written byte for byte the same for the same screen, so they compare by their files;
  - openocd reads the RTT buffers every 10 ms (its default is 100).
- **No `sscanf` in the firmware.** The first build used it to parse arguments, and it pulled about 39 KB of newlib's scanf into the image. It was replaced by `strtol`.

**Proof**

| Done when | Result |
|---|---|
| `menu_tour` finds all 22 forms | Passed: 1, 10 to 13, 20, 30 to 41, 51, 53, 60, 61, in about 6 s |
| Run twice, the second run compares equal to the first | Passed |
| The set committed as `tests/menu_baseline/pico_w/` | 22 PNGs, 11.6 KB |

- **`press` checked:**
  - `form 30`, then `press rst` gives `form=1`: the back button goes up one level.
  - `press cw 2`, then `press ccw 2` leaves the main menu as it was.
- **The error replies:** a bad button, a form id out of range and a form that does not exist each get their `@err` reply.
- **Looked at by eye:**
  - the main menu, the charge weight entry and the version page are upright and sharp;
  - the profile pages show the unit's stored profile (`AR2209,gr`: Kp 0.050 and 0.800, Kd 0.300 and 15.000).

**What the baseline depends on**
- **The unit's settings.** The profile pages show the selected profile, and the charge-weight form shows 2 decimal places (form 11; form 12 is the 3-place one). A unit with other settings differs there.
- **Form 36 (version info) changes with every build.** `menu_tour` reports it and does not judge it.

**What step 4 costs**

| | Pico W | Pico 2 W |
|---|---|---|
| Flash after step 3 → after step 4 | 697.9 → 699.8 KB (+1.9) | 672.5 → 674.4 KB (+1.9) |
| RAM at link time, step 3 → step 4 | 199.2 → 200.5 KB (+1.3) | 199.7 → 201.1 KB (+1.4) |

Most of the RAM is the snap's 1 KB copy of the screen.

## Step 5: motors, the simulated scale, and a charge cycle

Built 2026-11-14 on the bench's Pico W, with main's simulated scale (F2, the owner's word the same day). The coarse trickler turned during the runs; the hopper was empty.

**What was built**
- `motor <coarse|fine> <rev/s>`: it enables the driver and sets the speed; 0 stops the motor and disables the driver. It is refused while a mode has the screen.
- `motor_get_velocity()`. `state` gains `coarse=`, `fine=` and `sim=`.
- `sim on|off`:
  - `scale_init()` keeps the stored driver's task handle;
  - `sim on` suspends that task, sets Simulated in RAM, and starts main's simulated driver;
  - `sim off` reverses that, and drops the UART bytes that came in meanwhile.
- `charge <gr>` and `charge stop`. The REST handler now calls the same two functions, `charge_mode_start_remote()` and `charge_mode_stop_remote()`, with the same checks as before.
- `rc_run.py`:
  - `charge_cycle [--target 30] [--threshold 0.03]`;
  - `pulses`, which samples each STEP pin's GPIO status through the probe;
  - `flash` can take another ELF.

**Proof**

| Done when | Result |
|---|---|
| `charge_cycle` passes five times in a row, with the motors turning | Passed. Each run went 1 → 2 → 3 → 4 → 1 in about 28 s. The charge ended at 30.000 for a target of 30.000, and charging to the cup prompt took 16.6 to 17.0 s |
| After a `reboot`, `state` shows `sim=0` and the stored scale driver | Passed. `sim on`, then `reboot`, then `state` gives `sim=0 scale="AND FX-i Std"` |
| The step pulses stop on a probe reset and on an assert | Passed (below) |

**Do the step pulses stop?** (`pulses`; coarse trickler at 1 rev/s; the coarse STEP pin was high in 100% of 200 samples before each case)

| Case | Coarse STEP pin after | Verdict |
|---|---|---|
| (a) `reset` through the probe, sampled 0.5 s after | high in 0% | Stopped |
| (b) The assert test build: the chip enters the USB loader | high in 0%, 3 s after the assert | Stopped |
| (c) The hard-fault test build: the core halts at the breakpoint in `isr_hardfault` | high in 100%, 3 s after the fault | **Runs on**, as expected: the processor halts and PIO does not. A unit that hard-faults during a charge keeps its motor turning until power is cut. This is the fact ADR-012 starts from |

- **(a) does not tell the reset apart from the firmware's own start.** 0.5 s after the reset, the firmware has started again and set the motors to 0. Either way the pulses stop.
- **The check by eye was waived by the owner** (2026-11-14). The STEP pin itself was measured, which is the signal the driver steps on.

**Seen in the first run.** The runner first took the last weight polled while still charging (29.875) as the charge's weight, and failed the run. It now takes the weight once charging has ended, which is what the scale shows before the cup is lifted.

**What the console costs in all** (steps 3 to 5; this PC's compiler)

| | Pico W | Pico 2 W |
|---|---|---|
| Flash: main → this branch | 693.0 → 701.9 KB (+8.9) | 667.7 → 676.4 KB (+8.7) |
| RAM at link time: main → this branch | 195.0 → 200.6 KB (+5.6) | 195.5 → 201.1 KB (+5.6) |

## Step 6: the owner's check, and closing

Done 2026-11-14.

**The owner's check by hand.** The owner started a charge at the unit with no powder: the unit started and ran the process.
- Nothing was reading the console then: no openocd was running, and the last runner session had ended before it. So it also covers the step 3 check "the unit runs as before with no one at the console". With RTT, output nobody reads is dropped and nothing waits.
- The owner judged that unplugging the probe's USB cable adds nothing. A user's unit has no probe at all.

**The real throw** was waived by the owner, given the proof below ("I don't need to perform the actual throw if you can prove it works").

**The proof from a clean clone.**
- **The clone:** the branch was cloned, and the four submodules were copied in exactly as committed (each clean; every tracked file checked present).
- **The build:** it needs nothing outside the commits. It is not dirty: `1.20.46` at `f484e19`. The Pico W image is 717,608 bytes.
- **The run,** with the clone's own runner and nobody touching the unit:

| Run | Result |
|---|---|
| `flash` | PASS: the unit runs 1.20.46 at f484e19, 10.9 s after the start |
| `menu_tour --compare tests/menu_baseline/pico_w` | PASS: all 22 forms. Form 36 (version) differs, as it does for every build, and is reported, not judged |
| `charge_cycle` | PASS: 1 → 2 → 3 → 4 → 1 in 28.3 s, the charge ended at 30.000 for 30.000 |

**The unit's settings** are left as this branch made them. The charge-mode settings are at their defaults: a coarse stop threshold of 5 where it was 3, and the coarse back-off on. The owner did not ask for them to be set back, and the old values are in `logs/unit_backup/`.

**Lessons from the bench,** now in `AGENTS.md`:
- the usual proof of a change (flash, `menu_tour --compare`, `charge_cycle`);
- that reading the web API can change a unit (P2);
- that flashing can reset settings (F6).

**The settings, compared 2026-11-14** (read-only, against `logs/unit_backup/`):
- Only the charge-mode settings differ. The coarse stop threshold is 5 (it was 3), and the coarse back-off, new in PR #108, is on (`c14`, `c15`, `c16`). Every other module's settings are as they were.
- **The comparison itself changed the selected profile, in RAM.** Reading `profile_config?pf=N` selects profile N (audit P2, now seen on the unit), so the reads left profile 3 selected. Nothing was saved, and one more read of profile 1 put it back; `state` shows `AR2209,gr`. The backup on 2026-11-14 did the same on 1.19.7 until the flash restarted the unit.
- **The menu has no charge-mode settings,** so they are set back by one short web request (`c5`, `c14`, `ee=true`), which stays within lwIP's limits (audit H1, H2).
