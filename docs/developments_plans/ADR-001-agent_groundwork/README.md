# ADR-001: groundwork, so an agent can build, flash, drive and check a unit

Status: **Decided** 2026-11-13, approved by the owner with the seven decisions below. Steps 1 and 2 need no hardware; steps 3 to 6 need the owner's word that the bench is ready.

## What we want

An agent takes a change from source to a verdict on a real unit with no hands: build it, flash it, drive it, read it back, PASS or FAIL. Every later ADR is proven with what this one builds.

Nothing a user sees changes, except that the unit's USB port now shows a serial console.

## Why here

First. Today an agent cannot build on this PC, cannot read a log, and has no way to show that a change works on a unit.

## Owner's direction (2026-11-13)

- AI agents are to take over the development of this firmware.
- Planning first. No contact with the hardware until all of it is connected.
- The bench will have a Pico 2 W controller, a real scale, the motors and the trickler.
- Agents may run the motors, once there is a working setup.
- USB always recovers a unit.
- The USB serial debug feature of the Open Weapon Mount Computer firmware may be asked for here. This ADR builds it.

## What we know (checked 2026-11-13)

**Building**
- This PC has no Pico toolchain. `configure_env.ps1` expects one under `%USERPROFILE%\.pico-sdk` at fixed versions: Arm GNU toolchain 14.2.Rel1, CMake 3.31.5, Ninja 1.12.1, picotool 2.1.1, pioasm 2.1.1. The README installs them through the VS Code Pico extension.
- The submodules are checked out: pico-sdk 2.1.1, FreeRTOS-Kernel, u8g2, Trinamic-library. TinyUSB is inside pico-sdk.
- `build_pico_2w.ps1` builds into `build/`, `build_pico_w.ps1` into `build_w/`. Sources are found by a glob, so a new file needs a reconfigure.
- CI builds both boards (Debug) on every push and PR, and nothing else.

**The console**
- `stdio_init_all()` is commented out in `app.c`, and no `pico_enable_stdio_*` line is in the build. 109 `printf` calls print nowhere.
- The SDK's default console is UART0 on GP0 and GP1, which is the scale's port. So the console must be USB, with the UART console off.
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

**The scale**
- A scale driver is one task and a zero function (`scale_handle_t`). The task writes `scale_config.current_scale_measurement` and gives `scale_measurement_ready`.
- `scale_init()` starts the stored driver's task once and keeps no handle to it. Changing the driver later swaps the handle only; the first task keeps running.
- Charge mode decides "cup removed" when the mean of five readings is below about −10, and "cup returned" at the first reading of 0 or more.
- The motors' commanded speed is kept as `prev_velocity` in each `motor_config_t`. There is no getter.

**The reference** (Open Weapon Mount Computer, `main/tools/`, `tools/remote_control/`)
- Two layers: `remote_control` executes one command line and writes one reply line, and knows no transport; `rc_serial` is a task that reads lines from the console.
- A reply is one line, `@ok <command> ...` or `@err <reason>`. Lines a command sends before its reply also start with `@` (a snap is `@snap <w> <h> <format>`, then `@sd <base64>` lines). Everything else on the console is log.
- The host runner `rc_run.py` has `send` and `scenario`, keeps the session in a log file, counts failure lines, prints `VERDICT PASS` or `VERDICT FAIL` and exits 0 or 1.

## Decisions in this ADR

Approving the ADR approves these.

1. **The console is always on over USB, with no setting.** It needs a cable in the unit, and a setting would add a stored field, a menu entry and a web field for nothing.
2. **The simulated scale can be switched on only from the console, is never stored, and is gone after a restart.** Nobody at the unit can end up trickling real powder against a made-up weight. While it is on, the scale's name reads `SIMULATED` wherever the unit shows it.
3. **Flashing is by copying a `.uf2` to the USB loader's drive.** It needs no driver and no extra tool, and it is the same path a user has, so it is also the recovery path.
4. **These files are downloaded to this PC**, into `%USERPROFILE%\.pico-sdk`, in the layout the VS Code extension makes, so `configure_env.ps1` stays as it is:
   - Arm GNU toolchain 14.2.Rel1 for `arm-none-eabi`, Windows x64 zip, from Arm's download page;
   - `cmake-3.31.5-windows-x86_64.zip` from Kitware's CMake releases on GitHub;
   - `ninja-win.zip`, v1.12.1, from ninja-build's releases on GitHub;
   - `picotool-2.1.1-x64-win.zip` and `pico-sdk-tools-2.1.1-x64-win.zip` from `raspberrypi/pico-sdk-tools`, release `v2.1.1-3`.
5. **The menu's baseline snaps are tracked** in `tests/menu_baseline/<board>/`, so a later ADR can compare against them. Session logs are not tracked.
6. **Hopper empty.** An agent's run assumes there is no powder in the trickler. The owner says when there is.
7. **A failed `assert()` enters the USB loader** (`PICO_ENTER_USB_BOOT_ON_EXIT`). Today it stops at a breakpoint. Either way the unit is dead until its power is cycled. This way an agent can flash it back with no hands, and the step pulses stop. A hard fault is not covered and still needs the BOOTSEL button.

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

- `tools/setup_toolchain.ps1` fetches the files in decision 4 and unpacks them. It prints each file and its source before it downloads, and does nothing if the folder is already there.
- Both boards build from a clean clone with `build_pico_2w.ps1` and `build_pico_w.ps1`.
- `tools/size_report.py <map file>` prints the image's flash by part (radio firmware, lwIP, pages, runtime, SDK, application, u8g2, FreeRTOS) and the RAM taken at link time. CI runs it for both boards and puts the tables in the job summary.

Done when: both boards build on this PC; the report's total is within 2% of `app.bin`; the CI run of the branch shows both tables.

### Step 3: the console, and flashing with no hands

From here the owner's word that the bench is ready is needed.

- Build: `pico_enable_stdio_usb(app 1)`, `pico_enable_stdio_uart(app 0)`, `PICO_ENTER_USB_BOOT_ON_EXIT=1`. `stdio_init_all()` is the first call in `main()`.
- `src/remote_control.c`, `.h`: `remote_control_execute(line, reply, reply_size, write, ctx)`, as in the reference. No transport in it.
- `src/rc_serial.c`, `.h`: one task that reads the console a character at a time, gathers a line (128 characters at most), executes it and prints the reply with one `printf`.
- Commands in this step:

| Command | Reply |
|---|---|
| `help` | `@ok help` and the command names |
| `state` | `@ok state charge=<0..4> weight=<gr or nan> target=<gr> scale=<driver> ver=<version> vcs=<hash> build=<type> board=<pico_w or pico2_w> id=<unit id> up=<seconds> profile="<name>"` |
| `stats` | `@ok stats heap_free=<bytes> heap_min=<bytes> stack_min=<task>:<words>` |
| `reboot` | `@ok reboot`, then the unit restarts |
| `bootsel` | `@ok bootsel`, then the unit is in the USB loader |

- `tools/remote_control/rc_run.py`:
  - finds the unit's port by the SDK console's USB id (vendor 2E8A, product 000A on the RP2040 and 0009 on the RP2350), or takes `--port`;
  - `send "<line>"`: one command, its reply, exit 0 for `@ok`;
  - `flash <pico_w|pico2_w>`: sends `bootsel` (if nothing answers, opens the port at 1200 baud; if the loader's drive is already there, uses it); waits for a drive whose `INFO_UF2.TXT` names the right board; copies the build's `app.uf2`; waits for the console; passes when `state` reports the `ver` and `vcs` in the build's `src/generated/version.c`;
  - keeps every session in `logs/rc/<time>_<name>.log`;
  - counts as failures: a line with `assertion` or `PANIC`, no reply within 5 s, and `up=` going backwards without a `reboot` or `flash` asked for;
  - ends with `VERDICT PASS` or `VERDICT FAIL <reasons>` and exits 0 or 1.
- The first flash of a unit that has no console yet is by the BOOTSEL button, by the owner.

Done when, on the Pico 2 W controller:
- `flash` passes five times in a row with nobody touching the unit;
- `flash` passes through the 1200 baud path alone, with the `bootsel` command left out;
- a test build that asserts ten seconds after start-up puts itself into the USB loader, and `flash` brings the unit back;
- `state` and `stats` answer, and the log shows the firmware's `printf` lines;
- with the USB cable out, the unit starts and the owner charges by hand as before;
- `progress.md` has what the console costs in flash and RAM, from the size report.

### Step 4: press, form, snap, and the menu tour

- `press <cw|ccw|enc|rst> [n]` puts the event into `encoder_event_queue`, n times. Reply `@ok press`.
- `form` replies `@ok form=<id>`, with −1 while a mode owns the screen. `form <id>` jumps there: one new event value, handled in `menu_task`, calls `mui_GotoForm()`; an id that does not exist gives `@err no such form`. `menu_task` publishes the current form id after each draw. `state` gains `form=`.
- `snap` copies the u8g2 buffer and sends `@snap 128 64 u8g2`, four `@sd <base64>` lines, then `@ok snap`. The runner turns it into a PNG under `logs/rc/snaps/`. It snaps twice and keeps the picture only when both are equal, because a single copy can catch a frame half drawn.
- `scenario menu_tour`: for ids 1 to 99, `form <id>` and, where the form exists, a snap named `form_<id>.png`. It never presses anything, so no setting changes and nothing starts. `--save <folder>` keeps the set; `--compare <folder>` fails on a form that is missing or differs. Forms that show live values (the version, the Wi-Fi address) are listed in the runner and reported, not judged.

Done when: `menu_tour` finds all 22 forms; run twice, the second run compares equal to the first; the set is committed as `tests/menu_baseline/pico2_w/`.

### Step 5: motors, the simulated scale, and a charge cycle

- `motor <coarse|fine> <rev/s>` calls `motor_set_speed()`; 0 stops it. `motors.c` gets `motor_get_velocity()`, which returns the commanded speed.
- `src/sim_scale.c`: the simulated scale.
  - `scale_init()` keeps the handle of the driver task it starts. `sim on` suspends that task and starts the simulated one; `sim off` deletes the simulated one and resumes it. Nothing is stored.
  - Ten readings a second. Each one adds `coarse speed × A + fine speed × B` grains for the time passed, follows that through a short lag, and rounds to 0.02 gr. A, B and the lag are constants in the file, set on the unit so a 30 gr charge with the default profile takes between 10 and 30 s; the values go into `progress.md`. They stand for no real powder.
  - `sim cup off` reads −50 and empties the pan; `sim cup on` reads 0. The driver's zero function empties the pan.
  - Replies: `@ok sim=<0|1> cup=<0|1>`. `state` gains `sim=`.
- `charge <gr>` starts charge mode the way the web does; `charge stop` sends the reset press. `charge_mode.cpp` gets one function for this that the REST handler and the console both call.
- `scenario charge_cycle [--target 30]`:
  1. `sim on`, `sim cup on`, `charge <target>`;
  2. waits for `charge=1` (waiting for zero), then `charge=2` (charging), then `charge=3` (remove cup), 120 s at most;
  3. `sim cup off`, waits for `charge=4`; `sim cup on`, waits for `charge=1`;
  4. `charge stop`, waits for `charge=0`; `sim off`.

  It passes when the states came in that order and in time, the last weight is no more than the fine stop threshold under the target, both motors are commanded to 0 at the end, and no failure was counted. The time to target and the overshoot are printed, not judged: they belong to the made-up powder.

Done when: `charge_cycle` passes five times in a row on the Pico 2 W controller, with the motors turning; after a `reboot`, `state` shows `sim=0` and the stored scale driver.

### Step 6: the owner's check, and closing

- The owner makes one real throw with the real scale and powder, to see that nothing changed.
- `AGENTS.md` gets what was learned on the bench.
- The ADR is closed (Built, here, in `progress.md` and in the registry) in its branch before its PR is opened.

## How the unit shows it works

On the Pico 2 W controller, from a clean clone, with nobody touching the unit: built, flashed over USB, `menu_tour` equal to its baseline, `charge_cycle` passed, each with a verdict and a log. Then one real throw by the owner.

## Files

- New: `AGENTS.md`, `requirements.txt`, `tools/setup_toolchain.ps1`, `tools/size_report.py`, `tools/remote_control/rc_run.py`, `src/remote_control.c` and `.h`, `src/rc_serial.c` and `.h`, `src/sim_scale.c` and `.h`, `tests/menu_baseline/pico2_w/`, `docs/developments_plans/ADR-001-agent_groundwork/progress.md`.
- Changed: `CMakeLists.txt` (console over USB), `src/app.c` (console start, the console task), `src/menu.c` and `src/mini_12864_module.h` (the form id, the jump), `src/scale.c` and `.h` (the driver task's handle, the switch to the simulated scale), `src/motors.c` and `.h` (the getter), `src/charge_mode.cpp` and `.h` (one start function), `.github/workflows/cmake.yml` (the size report), `.gitignore`.
- No stored struct changes.

## Not in this ADR

- The remote control over Wi-Fi. The reference added it in a later ADR of its own.
- A watchdog, and stopping the motors whenever the firmware stops. What is known is under "What a failure looks like today". It changes what a user sees, so it is the owner's call and an ADR of its own.
- A unit that starts without its motor drivers. Today it stops at the error screen; that stays.
- The debug probe's scripts (`openocd_start.bat`, `program.bat`), which name the RP2040 only.
- The Pico W. Its baseline and its runs come when a Pico W is on the bench.
- Fixing what the bench turns up. It is written into `progress.md` and brought to the owner.
