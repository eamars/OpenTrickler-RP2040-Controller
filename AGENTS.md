# Working on this repository

Firmware for the OpenTrickler controller: a Raspberry Pi Pico W or Pico 2 W on the motor expansion board, with a mini 12864 display and rotary encoder, two TMC2209 stepper drivers, two servos, LEDs, an EEPROM and a scale on a serial port (see [README](README.md)). The trickler dispenses gunpowder. This page is for anyone, person or agent, about to change the firmware.

## Before anything: fetch

Run `git fetch` and compare your `main` with `origin/main` before you read the code as current. A checkout can be months behind. The first plans for this repository were drafted against a `main` that was 11 commits old, and had to be corrected.

## Build, flash

- **Clone** with the submodules: `git submodule update --init --recursive`. `library/` holds the Pico SDK, FreeRTOS, u8g2 and the Trinamic library.
- **Toolchain:**
  - Windows: `setup_toolchain.ps1` fetches the versions `configure_env.ps1` names into `%USERPROFILE%\.pico-sdk`. The VS Code Pico extension makes the same folder (README). `-List` prints what it would fetch.
  - macOS: `setup_toolchain.sh` fetches Arm's compiler into `.toolchain/` in the repository, and `configure_env.sh` sets the environment.
- **Build:**
  - `.\build_pico_2w.ps1` builds the Pico 2 W image, `build\app.uf2`;
  - `.\build_pico_w.ps1` builds the Pico W image, `build_w\app.uf2`.
  - Both run CMake's configure each time, so a new or removed source file is picked up (sources are found by a glob).
  - `src/generated/` is made by the build (version, pages) and is not tracked. PIO headers go to each board's build folder (`build\src\`, `build_w\src\`): pioasm writes a different one for each chip, and a shared one broke the Pico W image (ADR-001, F7).
- **Size:** `python tools/size_report.py build/app.elf.map --bin build/app.bin` prints the image's flash by part and the RAM taken at link time. CI prints the same for both boards on every build, in the job summary and in the log (`gh run view <id> --log`).
- **CI and a PC build different images.** CI runs on a pinned image, `ubuntu-24.04`, with Ubuntu's ARM compiler (13.2.1); a PC set up as above has Arm's 14.2.Rel1. CI's image is about 40 KB smaller, all in the C and C++ runtime. Compare sizes only between builds from the same place. The pin is there because SDK 2.1.1's `pioasm` does not compile on the next image's compiler; do not move it without a build that passes. ADR-011 is the planned update that removes the pin.
- **Flash:**
  - **With a debug probe on the unit's SWD port** (the bench): `venv\Scripts\python tools\remote_control\rc_run.py flash`. It writes `build_w\app.elf` through the probe, verifies it, and passes when the unit's console reports the build's version. This is also how the bench's unit is recovered, from an assert or a hard fault too.
  - **By hand**, as a user does: hold BOOTSEL, plug in USB, copy `app.uf2` to the drive ([manual](manuals/firmware_update_via_usb.md)).
- **Host tools:** Python from the venv at the repo root (`venv/`, built from `requirements.txt`). Never a second venv.

## Where code goes

`src/` is one flat folder of module pairs (`feature.c` and `feature.h`). ADR-006 will change this layout; until then:

| Files | What they are |
|---|---|
| `app.c` | Start-up order |
| `menu.c`, `mui_menu.c` | The menu task and the menu's forms |
| `charge_mode.cpp`, `cleanup_mode.cpp` | The two modes. Each takes the screen from the menu and draws for itself |
| `scale.c`, `*_scale.c`, `and_scale_calibrate.c` | The scale: one driver per make, each a task that reads the serial port. `simulated_scale.c` is a driver with no scale behind it: its weight follows the motors' speeds |
| `motors.c`, `stepper.pio` | The two steppers: TMC2209 set-up over its UART, step pulses by PIO |
| `servo_gate.c`, `neopixel_led.c`, `ws2812.pio` | The servo gate and the LEDs |
| `profile.c` | The powder profiles |
| `mini_12864_module.cpp`, `display.c`, `gpio_irq_handler.h` | The display, the encoder and the buttons |
| `eeprom.c`, `cat24c256_eeprom.c`, `common.c` | Settings storage |
| `wireless.c`, `access_point_mode.c`, `dhcpserver.c`, `dnsserver.c` | Wi-Fi, as a station or as its own access point |
| `http_rest.c`, `rest_endpoints.c`, `html/` | The HTTP server (a copy of lwIP's), the paths it serves, and the pages. `scripts/html2header.py` turns the pages into C strings |
| `system_control.c`, `freertos_hooks.c`, `FreeRTOSConfig.h`, `lwipopts.h` | Restart, version, RTOS and network settings |
| `targets/` | The board's pin map |

**Settings in the EEPROM:**
- Each module stores one struct in its own 1 KB slot with `load_config` and `save_config` (`common.h`), with a CRC32.
- The struct's layout is the stored format. Changing it resets that module's settings to their defaults on every unit.
- Never change a stored struct's layout without saying so in the ADR.

**Style:** the skills in `.agents/skills`:
- `c-style` for all C and C++;
- `py-style` and `python-venv` for host tools.

## Proving a change on a unit

Changes are proven on a real unit, not only built.

- **The bench rules** hold for every way of reaching a unit, the tools below included:
  - No contact with the hardware (no flashing, no serial port, no debug probe, no call to a unit over the network) until the owner says the bench is ready. Ask again in every new session.
  - An agent's run assumes the hopper is empty. The owner says when there is powder in it.
  - The motors may turn in an agent's run.
- **The bench** (2026-11-14): a Pico W on the motor board with an A&D FX-300i scale, reached only by a Raspberry Pi debugprobe on SWD. Its USB port is not connected.
- **The console** (ADR-001, step 3) is stdio over RTT: the debug probe reads it from the chip's RAM, with no pin and no cable to the unit. Lines starting with `@` belong to the remote control; everything else is the firmware's log.
- **`tools/remote_control/rc_run.py`** (run with the venv's Python) starts openocd itself, keeps each session in `logs/rc/`, and ends with `VERDICT PASS` or `VERDICT FAIL <reasons>`, exiting 0 or 1:
  - `send "state" "stats"`: commands, one after another (`help` lists them);
  - `flash [--elf <path>]`: write, verify, start, check the version;
  - `reset`: a whole-chip reset by the watchdog (openocd's own `reset run` locked the RP2040 up, F8);
  - `watch <seconds>`: log the console for a time;
  - `snap`: the screen as a PNG under `logs/rc/snaps/`, as a person at the unit sees it;
  - `menu_tour [--save <folder>] [--compare <folder>]`: every menu form in turn, pressing nothing. The Pico W's baseline is `tests/menu_baseline/pico_w/`; compare against it to see what a change did to the menu.
- Console commands to drive the menu: `press <cw|ccw|enc|rst> [n]`, `form` and `form <id>`.
- It counts as failures: a line with `assertion` or `PANIC`, no reply within 5 s, an `@err` reply, `up=` going backwards unasked, and a core found halted (named with its function).
- Still to come in ADR-001: `charge_cycle` with a simulated scale (step 5).

## Decisions and ADRs

Work is planned and recorded as architecture decision records in [docs/developments_plans](docs/developments_plans/README.md). The process is in that README. The parts that trip people up:
- **Approval first:** an ADR is approved by the owner before work starts.
- **One at a time:** one ADR is open at a time; a functional change and a dependency update are never mixed in one.
- **Plain form:** plain words. What we want, the owner's direction, what we know (measured or checked in the code), what we do, how the unit shows it works. `progress.md` records what was built and measured.
- **Close before the PR:** an ADR is closed (Built, in its README, its progress and the index) in its branch before its PR is opened.
- **PRs from others** are not merged as they stand. Each is discussed with the owner first (ADR-002).
- **An update that is not a feature may be merged without asking** (the owner, 2026-11-13: "for no feature update you can just merge the code"). Read here as: a change that leaves what the firmware does for a user as it was, such as CI, build scripts, host tools and documents. Open its PR, see its checks, merge it. A feature, or anything a user can see, waits for the owner.

## Do not

- **Do not edit `src/generated/`.** Change the source (a page, a `.pio` file, `scripts/gen_version.py`) and build.
- **Do not change anything under `library/`.** It is third-party code in submodules.
- **Do not change a stored struct's layout without saying so in the ADR** (see "Settings in the EEPROM").
- **Do not merge a feature, close a PR or an issue of someone else's, or post a comment** without the owner's word. An update that is not a feature is the exception above.
