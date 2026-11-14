# ADR-001: groundwork, so an agent can build, flash, drive and check a unit

Status: **Open**. Approved by the owner 2026-11-13 with the seven decisions below. Steps 1 and 2 built 2026-11-13 ([progress](progress.md)), CI run included. The bench was declared ready 2026-11-14. **Revised 2026-11-14 for the bench as it was built (F5), and the revision approved by the owner the same day:** the unit is a Pico W reached by a debug probe only, so steps 3 to 5 use the probe where they used the unit's USB port. Decisions 1, 3, 4 and 7 are changed and decision 8 is new. **Steps 3, 4 and 5 built 2026-11-14**, step 3 except the owner's check by hand ([progress](progress.md)). Step 5 uses main's simulated scale (F2, the owner's word 2026-11-14). Left: step 6, the owner's check and real throw, then closing.

## What we want

An agent takes a change from source to a verdict on a real unit with no hands: build it, flash it, drive it, read it back, PASS or FAIL. Every later ADR is proven with what this one builds.

Nothing a user sees changes. *(Revised: the console was to show on the unit's USB port; it now runs through the debug probe, which a user does not have.)*

## Why here

First. Today an agent cannot build on this PC, cannot read a log, and has no way to show that a change works on a unit.

## Owner's direction (2026-11-13)

- AI agents are to take over the development of this firmware.
- Planning first. No contact with the hardware until all of it is connected.
- The bench will have a Pico 2 W controller, a real scale, the motors and the trickler.
- Agents may run the motors, once there is a working setup.
- USB always recovers a unit.
- The USB serial debug feature of the Open Weapon Mount Computer firmware may be asked for here. This ADR builds it.

**The bench as built (2026-11-14):**
- A Pico W (RP2040), not a Pico 2 W, with a real scale (A&D FX-300i). Its USB port is not connected. A debug probe is connected to its SWD port, and nothing else of the probe is wired.
- The unit is on the owner's Wi-Fi.
- "Hopper is empty, and the unit is fully set up." The owner said the bench is ready, SWD sessions and turning motors included.

## What we know (checked 2026-11-13)

This section was first written against a `main` that was 11 commits behind the remote's (F1 below). It is corrected to today's main; the corrected lines say so.

**Building**
- This PC has no Pico toolchain. `configure_env.ps1` expects one under `%USERPROFILE%\.pico-sdk` at fixed versions: Arm GNU toolchain 14.2.Rel1, CMake 3.31.5, Ninja 1.12.1, picotool 2.1.1, pioasm 2.1.1. The README installs them through the VS Code Pico extension.
- The submodules are checked out: pico-sdk 2.1.1, FreeRTOS-Kernel, u8g2, Trinamic-library. TinyUSB is inside pico-sdk.
- `build_pico_2w.ps1` builds into `build/`, `build_pico_w.ps1` into `build_w/`. Sources are found by a glob, so a new file needs a reconfigure.
- CI builds both boards (Debug) on every push and PR, and nothing else.

**The console**
- `stdio_init_all()` is commented out in `app.c`, and no `pico_enable_stdio_*` line is in the build. 109 `printf` calls print nowhere.
- The SDK's default console is UART0 on GP0 and GP1, which is the scale's port. So the console must be USB, with the UART console off. *(Revised 2026-11-14: or RTT through a probe, which the bench has; see "The bench" below.)*
- The SDK's USB console restarts the chip into the USB loader when its port is opened at 1200 baud (`PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE`). `reset_usb_boot()` does the same from code.
- The USB loader is in the chip's ROM. Its drive carries `INFO_UF2.TXT`, which names the board, and a `.uf2` copied to it is flashed and started.

**What a failure looks like today**
- No watchdog runs. `watchdog_reboot()` is used only to restart on purpose. A hang stays hung.
- A stack overflow or a failed allocation ends in `assert()`, which prints the failed line and then stops at a breakpoint (`_exit()` in the SDK). With no debugger attached the core locks up, and USB with it. A hard fault ends at a breakpoint too.
- The step pulses are made by PIO, which repeats the last period on its own until it is given a new one (`stepper.pio`). So a unit that stops this way during a charge keeps its motors turning until its power is cut. Read from the code; not tried on a unit.
- The SDK has an option, `PICO_ENTER_USB_BOOT_ON_EXIT`, that enters the USB loader in place of that breakpoint. Entering the loader resets the chip's peripherals, PIO among them.
- If a motor driver does not answer at start-up, `handle_motor_init_error()` loops on the error screen before the scheduler starts. No task ever runs, so the menu, Wi-Fi and a console task would all be absent. The bench therefore needs its TMC2209 drivers powered.

**Driving the unit**
- Input is a queue of `ButtonEncoderEvent_t` (`encoder_event_queue`). The REST handler `/rest/button_control` already puts events into it.
- The menu is 22 MUI forms (ids 1, 10 to 13, 20, 30 to 41, 51, 53, 60, 61). `mui_GotoForm()` returns 0 for an id that does not exist, and `mui_GetCurrentFormId()` gives the form on screen. The `mui_t` lives inside `menu_task`, so only that task may touch it.
- The screen is u8g2's buffer: 128 × 64, 1,024 bytes, in 8 rows of 128 bytes, each byte a column of 8 pixels (layout described in `display.c`). `display.c` has a mutex for it that nothing takes, so a copy can catch a frame half drawn.
- The web starts a charge by setting `charge_mode_config.target_charge_weight` and `exit_state`, then sending `OVERRIDE_FROM_REST` to the menu.
- *(Corrected.)* In the menu the reset button goes up one level, by a table of each form's parent in `menu.c` (PR #110). In a mode it still leaves the mode.

**The scale**
- A scale driver is one task and a zero function (`scale_handle_t`). The task writes `scale_config.current_scale_measurement` and gives `scale_measurement_ready`.
- `scale_init()` starts the stored driver's task once and keeps no handle to it. Changing the driver later swaps the handle only; the first task keeps running.
- Charge mode decides "cup removed" when the mean of five readings is below about −10, and "cup returned" at the first reading of 0 or more.
- The motors' commanded speed is kept as `prev_velocity` in each `motor_config_t`. There is no getter.
- *(Corrected.)* **Main already has a simulated scale** (`simulated_scale.c`, PR #107, merged 2026-07-14):
  - it is scale driver 9, chosen like any other from the screen menu and the web page, and stored like any other;
  - ten readings a second. While the motors turn, the weight grows by 5 gr/s for each rev/s of the coarse trickler and 1 gr/s for each of the fine one, read straight from `prev_velocity`, with ±0.05 gr of noise;
  - the weight is held at the target once it is within 0.1 gr of it, so it never overshoots;
  - three seconds after reaching the target it "removes the cup" by itself (−150 gr), and three seconds later puts it back (0), so charge mode runs cycle after cycle with nobody there;
  - like every driver, its task is started only if it is the stored driver at start-up.

**The bench** (checked 2026-11-14: the code, this PC's devices, and the unit's web API, read only)
- **The probe** shows on this PC as USB 2E8A:000C, "CMSIS-DAP v2", which is Raspberry Pi's debugprobe firmware. Its serial port (COM3) is there, but nothing is wired to it.
- **No pin is free for a console.** The board uses all 26 of the Pico W's header pins (GP0 to 22, GP26 to 28; `targets/raspberrypi_pico_w_config.h`). The one the firmware does not set up, GP16 (the display's SPI input), can only be UART0's TX, and UART0 is the scale's.
- **The SDK has a console that needs no pin.** SDK 2.1.1 has `pico_stdio_rtt`: SEGGER's RTT, a pair of buffers in RAM that the probe reads and writes while the chip runs. It takes input as well as output. Its buffers are 1 KB out and 16 bytes in unless set otherwise. With no probe reading, output that does not fit is dropped and nothing waits.
- **This PC has no openocd.** Raspberry Pi's build for Windows, `openocd-0.12.0+dev-x64-win.zip` (8.3 MB), is in the `raspberrypi/pico-sdk-tools` release `v2.1.1-3` that decision 4 already fetches from. The repository's `openocd_start.bat` and `program.bat` already name the RP2040 and a CMSIS-DAP probe.
- **The unit's firmware** is 1.19.7-dirty, commit 3940423, a Debug build. That is older than v1.20 and than this branch.
- **The unit's settings:** scale driver A&D FX-i (0) at 19,200 baud; profile 1 (`AR2209,gr`) selected; coarse stop threshold 3.0, where the default is 5. A copy of every setting the web API gives (Wi-Fi left out) is in `logs/unit_backup/` on this PC.
- **Flashing this branch resets the unit's charge-mode settings for good.** PR #108 added three fields to the stored charge-mode struct. `load_config()` (`common.c:155-173`) checks a CRC over the struct's new size, which fails, so it uses the defaults and writes them to the EEPROM at once. Going back to 1.19.7 would do the same again. The coarse stop threshold goes back to 5, and the coarse back-off is on.
- **On main, the web page cannot save the charge-mode settings.** The form sends 17 parameters and lwIP takes 16, so "save to EEPROM" is dropped (PR #108). Since PR #112 the query is also cut at 127 characters, which loses the last fields. So after a flash the settings are set back from the unit's menu, not from the web page. (Audit H1 and H2, checked by a second reading 2026-11-14.)
- **The step pulses and a halted core.** The step pulses come from PIO, which runs on whether or not the processor is halted. Whether resetting the chip through the probe stops them is not known; the RP2040 has more than one kind of reset. It is measured in step 5, once a command can turn a motor.
- **In the SDK, `_exit()` calls `reset_usb_boot()`** when `PICO_ENTER_USB_BOOT_ON_EXIT` is set (`pico_clib_interface/newlib_interface.c`). A failed `assert()` ends there.

**The reference** (Open Weapon Mount Computer, `main/tools/`, `tools/remote_control/`)
- Two layers: `remote_control` executes one command line and writes one reply line, and knows no transport; `rc_serial` is a task that reads lines from the console.
- A reply is one line, `@ok <command> ...` or `@err <reason>`. Lines a command sends before its reply also start with `@` (a snap is `@snap <w> <h> <format>`, then `@sd <base64>` lines). Everything else on the console is log.
- The host runner `rc_run.py` has `send` and `scenario`, keeps the session in a log file, counts failure lines, prints `VERDICT PASS` or `VERDICT FAIL` and exits 0 or 1.

## Decisions in this ADR

Approving the ADR approves these.

1. **The console is always on, over RTT through the debug probe, with no setting.** *(Revised 2026-11-14; it was USB. The bench's unit has no USB cable, and no pin is free for a serial port.)* It needs a probe on the unit, so a user never sees it, and a setting would add a stored field, a menu entry and a web field for nothing. The USB console is not built: nothing here could prove it. It comes with a bench that has a USB cable.
2. **The simulated scale can be switched on only from the console, is never stored, and is gone after a restart.** Nobody at the unit can end up trickling real powder against a made-up weight. While it is on, the scale's name reads `SIMULATED` wherever the unit shows it. *(Revised 2026-11-14, F2: the console switches main's simulated scale on for one run, never stores it, and a restart brings the stored driver back; its name reads "Simulated Scale". Main's menu and web page can still choose and store it, which a later ADR may guard.)*
3. **Flashing is through the debug probe, with openocd.** *(Revised 2026-11-14; it was by copying a `.uf2` to the USB loader's drive.)* The probe writes the flash whatever the firmware is doing, so it is also the recovery path, and it covers a hard fault, which the USB path did not. A user still flashes by `.uf2`; nothing changes for them.
4. **These files are downloaded to this PC**, into `%USERPROFILE%\.pico-sdk`, in the layout the VS Code extension makes, so `configure_env.ps1` stays as it is:
   - Arm GNU toolchain 14.2.Rel1 for `arm-none-eabi`, Windows x64 zip, from Arm's download page;
   - `cmake-3.31.5-windows-x86_64.zip` from Kitware's CMake releases on GitHub;
   - `ninja-win.zip`, v1.12.1, from ninja-build's releases on GitHub;
   - `picotool-2.1.1-x64-win.zip` and `pico-sdk-tools-2.1.1-x64-win.zip` from `raspberrypi/pico-sdk-tools`, release `v2.1.1-3`;
   - *(added 2026-11-14)* `openocd-0.12.0+dev-x64-win.zip` (8.3 MB) from the same release, into `openocd\0.12.0+dev`. `setup_toolchain.ps1` fetches it like the others.
5. **The menu's baseline snaps are tracked** in `tests/menu_baseline/<board>/`, so a later ADR can compare against them. Session logs are not tracked.
6. **Hopper empty.** An agent's run assumes there is no powder in the trickler. The owner says when there is.
7. **A failed `assert()` enters the USB loader** (`PICO_ENTER_USB_BOOT_ON_EXIT`). Today it stops at a breakpoint. Either way the unit is dead until its power is cycled. This way the step pulses should stop, because entering the loader resets the chip's peripherals; step 5 measures it. *(Revised 2026-11-14: the recovery is now the probe, which flashes a unit in the loader as well as one stopped by a hard fault.)*
8. **The bench's unit is the Pico W** *(new 2026-11-14)*. Steps 3 to 5 are proven on it, and the menu's baseline is `tests/menu_baseline/pico_w/`. The Pico 2 W's runs and baseline come when one is on a bench.

## What building found

| # | Found | Cause | Now |
|---|---|---|---|
| F1 | The ADRs were drafted against old code. Main had gained 11 commits since 2026-04-28: a simulated scale (#107), the coarse trickler's back-off (#108), the back button (#110), a REST query fix (#112), toolchain scripts for macOS | The agent read the local `main` and never fetched. The PR and issue lists came from GitHub and were current; the code was not | Main merged into this branch; both boards rebuilt and measured again; the facts in ADR-001, 002, 003, 005, 006, 009 and 010 corrected; "fetch first" is the first rule in `AGENTS.md` |
| F2 | Step 5 plans a simulated scale, and main already has one | F1 | **Decided by the owner 2026-11-14:** main's simulated scale, switched on per run from the console and never stored ("Yes, use main's"). Step 5 revised. The guard question below is left for a later ADR |
| F3 | `tools/setup_toolchain.ps1` sat apart from main's `setup_toolchain.sh` | F1 | Moved to the root, beside `configure_env.ps1` and the macOS script |
| F4 | CI failed on main for every PR | `ubuntu-latest` became Ubuntu 26.04, whose compiler cannot build SDK 2.1.1's `pioasm` | The runner pinned to `ubuntu-24.04` (PR #114, merged). The pin's exit is ADR-011 |
| F5 | The bench differs from the plan: a Pico W with no USB cable, reached by a debug probe on SWD | The plan was written before the bench was built | Steps 3 to 5 revised to the probe; decisions 1, 3, 4, 7 changed, 8 added. **Waits for the owner's approval** |
| F6 | Flashing this branch resets the bench unit's charge-mode settings (its coarse stop threshold of 3 goes back to 5) | The unit runs 1.19.7; PR #108 changed the stored struct | Every setting copied to `logs/unit_backup/` before the first flash. Seen at the first flash: "CRC32 mismatch at address 1400", defaults written. The owner's throw in step 6 is made after setting them back |
| F7 | The first image flashed stopped at start-up with the motor error (`Unable to add program, err: -20`, PIO version mismatch), so no task ran. **CI's Pico W image on main has the same fault** | Both boards' builds wrote the PIO headers into the shared `src/generated/`. pioasm marks `stepper.pio` as PIO version 1 for the RP2350, and the Pico W build, built second, reused that header (CI's log: the Pico W build generates no `stepper.pio.h`) | PIO headers now go to each board's build folder; `src/generated/` is put on the include path by name, which it had only been as a side effect. Fixed on this branch, and it goes to main with this ADR (the owner, 2026-11-14: "Keep it here") |
| F8 | openocd's `reset run` left core 0 locked up in a double fault, its vector table read as 0xFFFFFFFF, and core 1 in the boot ROM | Not known. openocd's RP2040 reset with this debugprobe (firmware 2.0.1, which openocd calls old) | `rc_run.py reset` makes the watchdog reset the whole chip, as `watchdog_reboot()` does. It brought the locked-up unit back, and passes |

**F2, the proposal: use main's simulated scale, and build no second one.**
- `sim on` chooses it for this run only: the stored driver's task is suspended, the simulated driver's task is started, and the driver is set to Simulated in RAM. `sim off` undoes it. Nothing is saved by the command.
- No `sim cup` commands: main's simulated scale removes and returns the cup by itself. `charge_cycle` waits for the states to come in order, and for a second charge to start on its own.
- No `src/sim_scale.c`, and no getter for the motors' speed is needed for it.
- What it cannot show: overshoot. Its weight is held at the target. `charge_cycle` proves that a charge runs from zero to cup return; it never claimed more.

**F2, the question that comes with it.** Main's simulated scale can be chosen by anyone from the menu or the web page, and it stays chosen after a restart. A unit left on it, with powder in the hopper, trickles to a made-up weight that always lands on the target, and "removes the cup" by itself. Decision 2 was written to rule that out, and main does not. Whether it needs a guard (not stored, a plain warning on the charge screen, or the motors held off while it is chosen) is the owner's call, and would be a change a user sees.

## What we will do

Each step is its own commit and is proven before the next starts. What was measured goes into `progress.md`.

### Step 1: the process (no hardware)

- `AGENTS.md` at the root, in the reference project's shape:
  - build and flash;
  - where code goes (the flat `src/` of today, and that ADR-006 changes it);
  - how a change is proven on a unit (filled in as steps 3 to 5 land);
  - the bench rules: no contact until the owner says the bench is ready; hopper empty; the motors may turn;
  - decisions and ADRs (approval first, one open at a time, closed before its PR);
  - do not: edit `src/generated/`, change `library/`, change a stored struct's layout without saying so in the ADR (it resets that module's settings on every unit).
- `requirements.txt` with `pyserial`, for a venv at the repo root (`venv/`, as the `python-venv` skill says).
- `.gitignore`: `logs/` and `venv/`.

Done when: a new agent, given only `AGENTS.md`, can say how to build and where a change is proven.

### Step 2: build here, and know the size (no hardware)

- `setup_toolchain.ps1` (at the root; F3) fetches the files in decision 4 and unpacks them. It prints each file and its source before it downloads, and does nothing if the folder is already there.
- Both boards build from a clean clone with `build_pico_2w.ps1` and `build_pico_w.ps1`.
- `tools/size_report.py <map file>` prints the image's flash by part (radio firmware, lwIP, pages, runtime, SDK, application, u8g2, FreeRTOS) and the RAM taken at link time. CI runs it for both boards and puts the tables in the job summary.

Done when: both boards build on this PC; the report's total is within 2% of `app.bin`; the CI run of the branch shows both tables.

### Step 3: the console, and flashing with no hands

*(Revised 2026-11-14 for the probe, F5.)*

- Fetch openocd (decision 4). First contact: openocd reads the chip's id through the probe, with the unit left running.
- Build: `pico_enable_stdio_rtt(app 1)`, `pico_enable_stdio_uart(app 0)`, `pico_enable_stdio_usb(app 0)`, `PICO_ENTER_USB_BOOT_ON_EXIT=1`. The RTT buffers are set to 2 KB out and 256 bytes in, so a whole command line and a snap's lines fit. `stdio_init_all()` is the first call in `main()`.
- `src/remote_control.c`, `.h`: `remote_control_execute(line, reply, reply_size, write, ctx)`, as in the reference. No transport in it.
- `src/rc_console.c`, `.h`: one task that reads the console a character at a time, gathers a line (128 characters at most), executes it and prints the reply with one `printf`. It knows nothing of RTT, so a USB console later needs no change here.
- Commands in this step:

| Command | Reply |
|---|---|
| `help` | `@ok help` and the command names |
| `state` | `@ok state charge=<0..4> weight=<gr or nan> target=<gr> scale=<driver> ver=<version> vcs=<hash> build=<type> board=<pico_w or pico2_w> id=<unit id> up=<seconds> profile="<name>"` |
| `stats` | `@ok stats heap_free=<bytes> heap_min=<bytes> stack_min=<task>:<words>` |
| `reboot` | `@ok reboot`, then the unit restarts |

- `tools/remote_control/rc_run.py`:
  - starts openocd for the probe and the RP2040 with its RTT server on a local port, talks to the console through that port, and stops openocd when it ends;
  - `send "<line>"`: one command, its reply, exit 0 for `@ok`;
  - `flash`: openocd writes the build's `app.elf` (`build_w\` for the Pico W), verifies it and resets the chip; the runner waits for the console and passes when `state` reports the `ver` and `vcs` in the build's `src/generated/version.c`;
  - `reset`: resets the whole chip through the probe, by its watchdog (F8);
  - keeps every session in `logs/rc/<time>_<name>.log`;
  - counts as failures: a line with `assertion` or `PANIC`, no reply within 5 s, `up=` going backwards without a `reboot`, `reset` or `flash` asked for, and the processor found halted (it reports where);
  - ends with `VERDICT PASS` or `VERDICT FAIL <reasons>` and exits 0 or 1.
- The first flash is through the probe as well. No button is pressed at any point.

Done when, on the Pico W:
- `flash` passes five times in a row with nobody touching the unit;
- a test build that asserts ten seconds after start-up puts itself into the USB loader, and `flash` brings the unit back;
- a test build that makes a hard fault ten seconds after start-up is brought back by `flash`;
- `state` and `stats` answer, and the log shows the firmware's `printf` lines;
- with the probe's USB cable out, the unit starts and the owner charges by hand as before;
- `progress.md` has what the console costs in flash and RAM, from the size report.

### Step 4: press, form, snap, and the menu tour

- `press <cw|ccw|enc|rst> [n]` puts the event into `encoder_event_queue`, n times. Reply `@ok press`.
- `form` replies `@ok form=<id>`, with −1 while a mode owns the screen. `form <id>` jumps there: one new event value, handled in `menu_task`, calls `mui_GotoForm()`; an id that does not exist gives `@err no such form`. `menu_task` publishes the current form id after each draw. `state` gains `form=`.
- `snap` copies the u8g2 buffer and sends `@snap 128 64 u8g2`, four `@sd <base64>` lines, then `@ok snap`. The runner turns it into a PNG under `logs/rc/snaps/`. It snaps twice and keeps the picture only when both are equal, because a single copy can catch a frame half drawn.
- `scenario menu_tour`: for ids 1 to 99, `form <id>` and, where the form exists, a snap named `form_<id>.png`. It never presses anything, so no setting changes and nothing starts. `--save <folder>` keeps the set; `--compare <folder>` fails on a form that is missing or differs. Forms that show live values (the version, the Wi-Fi address) are listed in the runner and reported, not judged.

Done when: `menu_tour` finds all 22 forms; run twice, the second run compares equal to the first; the set is committed as `tests/menu_baseline/pico_w/` *(revised: was `pico2_w`, decision 8)*.

### Step 5: motors, the simulated scale, and a charge cycle

*(Revised 2026-11-14 for F2: the owner chose main's simulated scale.)* With the hopper empty a real scale never reaches a target, so the charge cycle needs a simulated scale.

- `motor <coarse|fine> <rev/s>` enables the driver and calls `motor_set_speed()`; 0 stops it and disables the driver. It is refused while a mode has the screen. `motors.c` gets `motor_get_velocity()`, the speed last reached; `state` gains `coarse=` and `fine=`.
- `sim on|off`: main's simulated scale stands in for the stored driver for this run.
  - `scale_init()` keeps the handle of the driver task it starts.
  - `sim on` suspends that task, sets the driver to Simulated in RAM, and starts the simulated driver's task. `sim off` deletes it, sets the stored driver back, drops what the UART received meanwhile, and resumes the stored task.
  - Nothing is stored. A save from the menu or the web while it is on would store it.
  - Replies `@ok sim=<0|1> scale="<name>"`. `state` gains `sim=`.
- `charge <gr>` starts charge mode the way the web does; `charge stop` sends the reset press. `charge_mode.cpp` gets `charge_mode_start_remote()` and `charge_mode_stop_remote()`, which the REST handler and the console both call.
- `charge_cycle [--target 30]`:
  1. `sim on`, `charge <target>`;
  2. polls `state` four times a second and waits for the states to come in the order 1, 2, 3, 4, 1 within 120 s. The simulated scale holds the target, then "removes the cup" and puts it back by itself, so the second 1 is the next charge starting;
  3. `charge stop`, then waits for `charge=0` with both motors at 0, and finally `sim off`.

  It passes when the states come in that order and in time, the weight once charging ends is no more than the fine stop threshold under the target, both motors are commanded to 0 at the end, and no failure was counted. The time to target and the overshoot are printed, not judged: they belong to the made-up powder, and main's simulated scale never overshoots.
- *(Added 2026-11-14.)* **Do the step pulses stop?** `pulses` commands the coarse trickler to 1 rev/s, then tries three things: (a) `reset` through the probe; (b) the assert test build from step 3; (c) the hard-fault test build, which with the probe attached halts at a breakpoint.
  - After each, the runner samples the STEP pin's GPIO status through the probe, 200 times. A pulsing pin reads high almost always, a stopped one never. (The plan said PIO0's control register, but the LEDs' state machines may share PIO0.)
  - (a) and (b) pass when the pulses stop.
  - (c) is expected to keep the motor turning, since the processor halts and PIO does not. It is measured so ADR-012 starts from a fact.

Done when: `charge_cycle` passes five times in a row on the Pico W, with the motors turning; after a `reboot`, `state` shows `sim=0` and the stored scale driver; the step pulses stop on a probe reset and on an assert.

### Step 6: the owner's check, and closing

- The unit's settings are set back from `logs/unit_backup/`, from the unit's menu, where this branch reset them (F6). Then the owner makes one real throw with the real scale and powder, to see that nothing changed.
- `AGENTS.md` gets what was learned on the bench.
- The ADR is closed (Built, here, in `progress.md` and in the registry) in its branch before its PR is opened.

## How the unit shows it works

On the Pico W, from a clean clone, with nobody touching the unit: built, flashed through the probe, `menu_tour` equal to its baseline, `charge_cycle` passed, each with a verdict and a log. Then one real throw by the owner.

## Files

- New: `AGENTS.md`, `requirements.txt`, `setup_toolchain.ps1`, `tools/size_report.py`, `tools/remote_control/rc_run.py`, `src/remote_control.c` and `.h`, `src/rc_console.c` and `.h`, `tests/menu_baseline/pico_w/`, `docs/developments_plans/ADR-001-agent_groundwork/progress.md`.
- Changed: `CMakeLists.txt` (console over RTT, its buffer sizes), `setup_toolchain.ps1` (openocd), `src/app.c` (console start, the console task), `src/menu.c` and `src/mini_12864_module.h` (the form id, the jump), `src/scale.c` and `.h` (the driver task's handle, the switch to the simulated scale), `src/motors.c` and `.h` (the getter), `src/charge_mode.cpp` and `.h` (one start function), `.github/workflows/cmake.yml` (the size report), `.gitignore`.
- No stored struct changes.

## Not in this ADR

- The remote control over Wi-Fi. The reference added it in a later ADR of its own.
- A watchdog, and stopping the motors whenever the firmware stops. What is known is under "What a failure looks like today". It changes what a user sees, so it is the owner's call and an ADR of its own: ADR-012 carries it.
- A unit that starts without its motor drivers. Today it stops at the error screen; that stays. ADR-012 asks the question.
- The debug probe's old scripts (`openocd_start.bat`, `program.bat`), which name the RP2040 only. `rc_run.py` calls openocd itself and does not use them. ADR-011 asks whether they are made right or removed.
- The USB console (decision 1), and flashing by `.uf2` with no hands. They come with a bench that has a USB cable.
- The Pico 2 W. Its baseline and its runs come when a Pico 2 W is on a bench.
- Fixing what the bench turns up. It is written into `progress.md` and brought to the owner.
