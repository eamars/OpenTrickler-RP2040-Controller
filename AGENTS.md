# Working on this repository

Firmware for the OpenTrickler controller: a Raspberry Pi Pico W or Pico 2 W on the motor expansion board, with a mini 12864 display and rotary encoder, two TMC2209 stepper drivers, two servos, LEDs, an EEPROM and a scale on a serial port (see [README](README.md)). The trickler dispenses gunpowder. This page is for anyone, person or agent, about to change the firmware.

## Before anything: fetch

Run `git fetch` and compare your `main` with `origin/main` before you read the code as current. A checkout can be months behind. The first plans for this repository were drafted against a `main` that was 11 commits old, and had to be corrected.

## Build, flash

- **Clone** with the submodules: `git submodule update --init --recursive`. `library/` holds the Pico SDK, FreeRTOS and u8g2.
- **Toolchain:** every version is written once, in `toolchain.env`: the compiler, CMake, Ninja, `pioasm` and picotool, OpenOCD. The scripts below and CI read it; change a version there and nowhere else.
  - Windows: `setup_toolchain.ps1` fetches them into `%USERPROFILE%\.pico-sdk`, and `configure_env.ps1` puts them on the path. The VS Code Pico extension makes the same folder (README). `-List` prints what it would fetch.
  - Linux (what CI runs): `setup_toolchain.sh` fetches the same set into `.toolchain/` in the repository, and `source ./configure_env.sh` puts it on the path.
  - macOS is parked (the owner, 2026-11-14: development is on Windows, integration builds on Linux). Its scripts as last used are in git: `git show 2ad9b0a:setup_toolchain.sh` and `configure_env.sh`. They come back when the owner works on a Mac again.
- **Build:**
  - `.\build_pico_2w.ps1` builds the Pico 2 W image, `build\app.uf2`;
  - `.\build_pico_w.ps1` builds the Pico W image, `build_w\app.uf2`.
  - Both run CMake's configure each time, so a new or removed source file is picked up (sources are found by a glob).
  - `src/generated/` is made by the build (version, pages) and is not tracked. PIO headers go to each board's build folder (`build\src\`, `build_w\src\`): pioasm writes a different one for each chip, and a shared one broke the Pico W image (ADR-001, F7).
- **Size:** `python tools/size_report.py build/app.elf.map --bin build/app.bin` prints the image's flash by part and the RAM taken at link time. CI prints the same for both boards on every build, in the job summary and in the log (`gh run view <id> --log`).
- **CI builds with what a PC builds with.** It runs on `ubuntu-latest` but uses none of the runner's compiler or tools: `setup_toolchain.sh` fetches the set `toolchain.env` names, kept in GitHub's cache until a version in the file changes (ADR-011). A new runner image does not change the build. CI's image is about 1.3 KB larger than a PC's, and that is all paths: each assert stores its source file's full path, and CI's checkout path is 35 characters longer than `C:/workspace/OpenTrickler-RP2040-Controller` (37 paths in the Pico W image, 34 in the Pico 2 W's; measured 2026-11-14).
- **Flash:**
  - **With a debug probe on the unit's SWD port** (the bench): `venv\Scripts\python tools\remote_control\rc_run.py flash`. It writes `build_w\app.elf` through the probe, verifies it, and passes when the unit's console reports the build's version. This is also how the bench's unit is recovered, from an assert or a hard fault too.
  - **By hand**, as a user does: hold BOOTSEL, plug in USB, copy `app.uf2` to the drive ([manual](manuals/firmware_update_via_usb.md)).
- **Host tools:** Python from the venv at the repo root (`venv/`, built from `requirements.txt`). Never a second venv.
- **The unit's API from Python:** `tools/remote_control/unit_api.py` works from what the unit says of itself (`GET /api/v1` and its schema), checks a write against the schema before sending it, and follows the event log. `python tools/remote_control/unit_api.py <address>` prints what the unit is, its paths and its actions.
- **Page work without a unit:** `venv\Scripts\python tests\web_test.py` serves the pages from `src/html` and a recording of a unit's replies (`tests/web_test_data.json`) at http://127.0.0.1:8000/, checking each write with the client's code. `--record <address>` records a unit again when the API grows; the owner's Wi-Fi name and the unit's address are replaced before it is written.

## Where code goes

`src/` is one flat folder of module pairs (`feature.c` and `feature.h`). ADR-006 will change this layout; until then:

| Files | What they are |
|---|---|
| `app.c` | Start-up order |
| `menu.c`, `mui_menu.c` | The menu task and the menu's forms |
| `charge_mode.cpp`, `cleanup_mode.cpp` | The two modes. Each takes the screen from the menu and draws for itself |
| `scale.c`, `*_scale.c`, `and_scale_calibrate.c` | The scale: one driver per make, each a task that reads the serial port. `simulated_scale.c` is a driver with no scale behind it: its weight follows the motors' speeds |
| `motors.c` | The two steppers: one task that ramps both speeds and feeds the step pulses every millisecond, the stored settings, and the task that reads the drivers once a second |
| `step_pulses.c`, `stepper.pio` | The step pulses by PIO (ADR-012): the PIO makes only the steps it is fed, so a firmware that stops feeding is a motor that stops; a second state machine counts the steps on the pin |
| `fault_handler.c` | A hard fault stops the motors before the core halts |
| `tmc_uart.c`, `tmc2209.c` | The stepper drivers, our own on Klipper's model (ADR-012): the one-wire frames with every write checked, and the TMC2209's registers as a table. No pins and no RTOS in them |
| `servo_gate.c`, `neopixel_led.c`, `ws2812.pio` | The servo gate and the LEDs |
| `profile.c` | The powder profiles |
| `mini_12864_module.cpp`, `display.c`, `gpio_irq_handler.h` | The display, the encoder and the buttons |
| `eeprom.c`, `cat24c256_eeprom.c`, `common.c` | Settings storage |
| `wireless.c`, `access_point_mode.c`, `dhcpserver.c`, `dnsserver.c` | Wi-Fi, as a station or as its own access point |
| `http_server.c`, `http_json.c` | Our own HTTP server (ADR-003): one task on lwIP's sockets, `select` over six slots, routes, keep-alive, chunked and parked replies. A handler sees no socket |
| `api_v1.c`, `api_v1_commands.c`, `rest_endpoints.c`, `html/` | The API ([api_v1.md](docs/developments_plans/ADR-003-http_rest_server/api_v1.md)): the index, the screen and the debug routes; the commands, the unit's reads and the schema's commands; the pages. The old `/rest/` paths are gone (ADR-003, gate 4). `scripts/html2header.py` turns the pages into C strings |
| `events.c` | The unit's event log: the newest 64, numbered, read from where a client left off, held with `?wait` |
| `settings.c`, `activity.c` | Each setting described once (ADR-003, gate 3): one code for every module's table (the GET, the PATCH checked whole, the save, `saved`, the export, the schema, the console's `setting`), and the unit's job and throw, marked under the lock a settings write holds |
| `system_control.c`, `freertos_hooks.c`, `FreeRTOSConfig.h`, `lwipopts.h` | Restart, version, RTOS and network settings |
| `targets/` | The board's pin map |

**Settings in the EEPROM:**
- Each module stores one struct in its own 1 KB slot with `load_config` and `save_config` (`common.h`), with a CRC32.
- The struct's layout is the stored format. Changing it resets that module's settings to their defaults on every unit.
- Never change a stored struct's layout without saying so in the ADR.
- **Each setting is described once,** in its module's table (`settings.h`; charge mode's and the display's in a C file beside their C++ one): name, type, unit, limits or choices, when it applies, a line on what it is, and where it lies in the struct. A new setting goes in the struct and the table, and the API, the export, the schema, the portal's form (by its field's name) and the console's `setting` follow. `saved` is the struct's CRC against what was last stored, so a write from the menu counts.

**Style:** the skills in `.agents/skills`:
- `c-style` for all C and C++;
- `py-style` and `python-venv` for host tools.

## Proving a change on a unit

Changes are proven on a real unit, not only built. The usual proof, after a build: `rc_run.py flash`, then `rc_run.py menu_tour --compare tests/menu_baseline/pico_w`, then `rc_run.py charge_cycle`. Each must end `VERDICT PASS`. A change meant to alter a screen updates the baseline in the same commit, with `--save`.

- **One unit is enough for a change in the application layer** (the owner, 2026-11-15: "We don't need to repeat on Pico W for application layer changes"). Read here as:
  - a change that touches nothing the two boards differ in (the modes, the control loop, the menu's logic, the profiles) is proven on whichever unit is on the probe;
  - a change to a driver, the PIO, timing against the chip, memory, the network or the build is proven on both.

- **The bench rules** hold for every way of reaching a unit, the tools below included:
  - No contact with the hardware (no flashing, no serial port, no debug probe, no call to a unit over the network) until the owner says the bench is ready. Ask again in every new session.
  - An agent's run assumes the hopper is empty. The owner says when there is powder in it.
  - The motors may turn in an agent's run.
- **The bench** (2026-11-14): a Pico W on the motor board with an A&D FX-300i scale, reached only by a Raspberry Pi debugprobe on SWD. Its USB port is not connected.
  - A second unit, a Pico 2 W on a controller board of its own, with its own EEPROM and so its own settings, is put on the probe when the owner is asked. One unit at a time. `rc_run.py --board pico2_w` drives it.
  - The A&D goes to whichever unit the owner connects it to. `state` tells: a number when a scale sends, `nan` (`inf` on the Pico W, whose float code treats NaN as infinity) when none does. Charges in proofs use the simulated scale either way.
  - Each unit is on the owner's Wi-Fi with an address of its own, given by DHCP: ask, or read it from the unit's screen.
- **The web has little memory.** A socket copies every byte it sends into lwIP's heap and holds it there until the client acknowledges it, and so does every TCP connection lwIP keeps, those the server has closed included. So the heap (40,960 bytes) is sized for lwIP's 12 connections with a send buffer of 2 segments each, and taken from the FreeRTOS heap (112 KB). On lwIP's own file server the Pico examples' 4,000 bytes ran out 7,404 times in a ten-minute soak and missed 8.9% of the web's reads (ADR-003, gate 1); on sockets, 16 KB ran out 340 times under a tuning session's load, 24 KB 54 times (gate 2). The most used moves with the Wi-Fi. A change to the server is soaked, with `--load` where it touches what a stream sends, and `net` says what it takes.
- **A GET changes nothing** (api_v1.md): read a unit over the web freely. A unit with a firmware from before ADR-003's gate 4 still has the old `/rest/` paths, which do change it (a GET of `/rest/profile_config?pf=N` selects profile N): read such a unit through the console, and copy its settings with an `rc_run.py` of its own time (below).
- **Flashing a unit can reset its settings.** When a stored struct's layout differs from the one on the unit, the defaults are written at once (`common.c`). Copy the unit's settings before the first flash of a newer or older version.
- **The console** (ADR-001, step 3) is stdio over RTT: the debug probe reads it from the chip's RAM, with no pin and no cable to the unit. Lines starting with `@` belong to the remote control; everything else is the firmware's log.
- **`tools/remote_control/rc_run.py`** (run with the venv's Python) starts openocd itself, keeps each session in `logs/rc/`, and ends with `VERDICT PASS` or `VERDICT FAIL <reasons>`, exiting 0 or 1:
  - `send "state" "stats"`: commands, one after another (`help` lists them);
  - `flash [--elf <path>]`: write, verify, start, check the version;
  - `reset`: a whole-chip reset by the watchdog (openocd's own `reset run` locked the RP2040 up, F8);
  - `watch <seconds>`: log the console for a time;
  - `snap`: the screen as a PNG under `logs/rc/snaps/`, as a person at the unit sees it;
  - `menu_tour [--save <folder>] [--compare <folder>]`: every menu form in turn, pressing nothing. The Pico W's baseline is `tests/menu_baseline/pico_w/`, the Pico 2 W's `tests/menu_baseline/pico2_w/`; compare against the unit's own to see what a change did to the menu. They differ in six forms, by the settings the two units store.
  - `charge_cycle [--target 30]`: a charge from zero to cup return and the start of the next one, with main's simulated scale switched on for the run (`sim on`), never stored;
  - `pulses`: whether the step pulses stop on a probe reset, an assert and a hard fault. All three are judged. It needs the two test images, built with `-DRC_TEST_FAULT=1` and `=2` into `build_w_fault1` and `build_w_fault2` (`build_2w_fault1` and `2` for the Pico 2 W, given with `--assert-elf` and `--hardfault-elf`);
  - `settings --host <address> [--save <folder>] [--compare <folder>]`: the stored settings copied over the web API (`GET /api/v1/settings`, which changes nothing) and compared with an earlier copy, a field at a time; the firmware's version is reported, not judged. Take one before every flash of another version. Copies made before ADR-003's gate 3 are of the old paths and do not compare: the Pico W's v1 copy is `logs/adr003/picow/before_gate3_v1/`. A unit still on such a firmware is copied with the `rc_run.py` of a commit before gate 3 (`git show <commit>:tools/remote_control/rc_run.py`);
  - `api_check --host <address>`: the API by the client (`unit_api.py`): every GET path the index lists read twice, each answer checked against the schema's tables and the two the same, the settings and the event log unchanged; then every action the schema lists called where it is safe, each answered as api_v1.md says, a charge and a cleanup started and stopped with their events (the simulated scale on for it), and last a restart, after which the boot is new. It fails on an action it did not call (ADR-003, gate 4);
  - `settings_roundtrip --host <address>`: every setting written back as read and read again, unchanged; each field given a value outside its limits, refused, the resource unchanged; a write to the charge's and the motors' settings during a throw refused with 409. Nothing on the unit changes (ADR-003, gate 3);
  - `soak <seconds> --host <address>`: charges back to back with the simulated scale while two browsers use the web pages (the mirror, the charge page's polling, the portal reloaded, the settings read). It fails on a failed charge, on memory that is not given back (the FreeRTOS heap read quiet before and after), on an allocation lwIP's heap or a pool failed during the run (`net`, read at its start and end), on a late feed of the step pulses (`motor`), and on web reads missed past `--max-miss-share` and `--max-miss-run`; it sums the misses by kind (timeout, reset, refused, wrong). It is not part of every proof: whether a change needs one, how long, and against what limits is decided for that change, for the area it touches. Its web figures compare only when nothing else uses the unit's web pages: a browser left open on the unit tripled the timeouts of ADR-011's step-3 soak. Each poller's reply times are given (median, 95%, longest). `--load` adds what a tuning session asks of the unit: a reply the size of a throw's samples (600 rows of 16 numbers, `/api/v1/debug/load`) pulled back to back, and a client reading the charge four times a second. `--upload` adds ADR-004's image: 700 KB sent back to back as a body taken in pieces (`PUT /api/v1/debug/upload`), each checked by its CRC-32. Two clients follow the event log with held reads through every soak, and it fails when either misses an event or they differ;
  - `web_robust --host <address>`: requests that are wrong or awkward (headers of 8 KB, a request line too long, another `Host`, a body longer than it says, a chunked body, a client that stops reading, writes that are not JSON or not the resource's...), each answered or closed as api_v1.md says, then seven clients at once, the parked requests and two stalls; the unit must go on answering after each (ADR-003, gates 2 and 3);
  - `scale_frames [--idle s] [--handovers n] [--reboots n] [--motors s]`: the scale's frames through the tap, summed up by phase: how many read as a number, the texts of those that did not, the time between frames (ADR-013);
  - `charge_fault <bad|quiet|cleanup|none> --host <address> --expect seen|not-seen`: a simulated charge with one fault put in at a weight, `state` read every 50 ms, and the charge's own `Charge: ...` lines checked with `--want` and `--want-none` (ADR-013).
- Console commands to drive the menu: `press <cw|ccw|enc|rst> [n]`, `form` and `form <id>`.
- Console command for the settings (ADR-003, gate 3): `setting` lists the resources; `setting <path>` gives each field as an `@setting name=value` line; `setting <path> name=value ...` writes first, through the same tables, checks and refusals as the API (`@err <code> <field> <message>`); `setting <path> save` stores it. The path as the API's, with or without `/api/v1/` (`setting settings/charge`, `setting powder_profiles/3`). A write is live, not stored, until saved; the Wi-Fi key is written but never shown.
- Console command for the network (ADR-003): `net`, lwIP's heap and the pools a web connection takes from (TCP connections, segments, pbufs, receive buffers), each in use, most ever, size and failed allocations. The heap is 40,960 bytes (ADR-003, gate 2); a change to the server is measured with it. `tcp_pcb` at 12 of 12 is no fault: closed connections wait out TIME_WAIT there and lwIP takes back the oldest for a new one, counting a failure only when none can be taken (`tcp_alloc`). `GET /api/v1/debug` gives the same over the web, with the server's own counts.
- Console commands for the motors and charging: `motor <coarse|fine> <rev/s>`, `motor` alone (the steps counted on each STEP pin, the steps fed, the feeds that came late, with the unit's clock: two replies give a speed), `sim on|off`, `charge <gr>` (a start through the same entry point as the API's, `by` console), `charge stop`.
- Console commands for the motor drivers (ADR-012): `tmc <coarse|fine>` (the driver's registers by field as `@tmc` lines, then its serial line's counts), `tmc <coarse|fine> setup <n>` (set up n times from the stored settings), and, never stored, `tmc <coarse|fine> lose` and `find` (the driver made to look absent, and found again) and `tmc <coarse|fine> fault <short|hot|reset|supply|none>` (the driver read to report a fault). `state` ends with `drivers=<coarse>,<fine>`: what each driver was last found to be.
- Console commands for the scale, never stored (ADR-013): `scale tap on|off` (the A&D driver's frames as `@sf` lines), `sim bad` (the simulated scale's next reading does not parse), `sim quiet <ms>` (it sends nothing for that long), `sim rate <per second>`, `sim weight <a number|off>` (it shows that weight, whatever its powder does). `stats` counts `bad_frames`.
- It counts as failures: a line with `assertion` or `PANIC`, no reply within 5 s, an `@err` reply, `up=` going backwards unasked, and a core found halted (named with its function).

## Decisions and ADRs

Work is planned and recorded as architecture decision records in [docs/developments_plans](docs/developments_plans/README.md). The process is in that README. The parts that trip people up:
- **Approval first:** an ADR is approved by the owner before work starts.
- **One at a time:** one ADR is open at a time; a functional change and a dependency update are never mixed in one.
- **Plain form:** plain words. What we want, the owner's direction, what we know (measured or checked in the code), what we do, how the unit shows it works. `progress.md` records what was built and measured.
- **Close before the PR:** an ADR is closed (Built, in its README, its progress and the index) in its branch before its PR is opened.
- **Stage gates, not small ADRs** (the owner, 2026-11-15: "Don't use small ADR. If you need staged release, you can have smaller stage gates within ADR-003"). A large ADR is built in gates inside it. A gate is approved and built on its own and may be merged by a PR of its own; the ADR is then Open until its last gate, which closes it before its PR.
- **PRs from others** are not merged as they stand. Each is discussed with the owner first (ADR-002).
- **An update that is not a feature may be merged without asking** (the owner, 2026-11-13: "for no feature update you can just merge the code"). Read here as: a change that leaves what the firmware does for a user as it was, such as CI, build scripts, host tools and documents. Open its PR, see its checks, merge it. A feature, or anything a user can see, waits for the owner.
- **Push only when it is needed** (the owner, 2026-11-15: GitHub has begun to block accounts that agents push from often, and the owner's must not become one). Read here as:
  - commit locally as often as the work needs; a local commit never reaches GitHub;
  - push a branch when its work is ready for its PR, or when the owner asks to see it on GitHub. Not after each commit, and not to keep work in progress safe;
  - once a PR is open, gather its fixes and push them together;
  - never push to try something in CI. CI builds with the toolchain a PC builds with, so a local build answers first.

## Do not

- **Do not edit `src/generated/`.** Change the source (a page, a `.pio` file, `scripts/gen_version.py`) and build.
- **Do not change anything under `library/`.** It is third-party code in submodules.
- **Do not change a stored struct's layout without saying so in the ADR** (see "Settings in the EEPROM").
- **Do not push often.** Push when a branch is ready for its PR, not as you go (see "Decisions and ADRs").
- **Do not merge a feature, close a PR or an issue of someone else's, or post a comment** without the owner's word. An update that is not a feature is the exception above.
