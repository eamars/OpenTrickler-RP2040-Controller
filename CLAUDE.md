# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for the OpenTrickler powder trickler controller, targeting Raspberry Pi Pico W (RP2040) and Pico 2W (RP2350) boards. Built on the Pico SDK with FreeRTOS, u8g2/MUI for the mini 12864 display, lwIP/CYW43 for WiFi + REST + web portal, TMC2209 stepper drivers via the Trinamic library, and an on-board I2C EEPROM for persisted config.

## Build commands

All builds require PowerShell and the Raspberry Pi Pico VSCode extension's toolchain (installed under `~/.pico-sdk`). Submodules under `library/` (pico-sdk, FreeRTOS-Kernel, u8g2, Trinamic-library) must be initialized first: `git submodule update --init --recursive`.

Load toolchain env vars into the current PowerShell session before any CMake/Ninja command:

```powershell
.\configure_env.ps1
```

Configure + build for a target board (from the same session, first build only needs to configure once):

```powershell
# Pico 2W (RP2350)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico2_w
cmake --build build --config Debug

# Pico W (RP2040)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico_w
```

Convenience scripts that do configure_env + configure + build in one shot:
- `.\build_pico_2w.ps1` — builds to `build/app.uf2` (pico2_w)
- `.\build_pico_w.ps1` — builds to `build_w/app.uf2` (pico_w)

Output firmware is `app.uf2` in the corresponding build directory; flash by holding BOOTSEL and copying the file to the Pico's USB mass-storage device.

Launch VSCode with the toolchain env pre-loaded (required for the CMake Tools extension to find the compiler): `.\run_vscode.ps1`. The default board in `.vscode/settings.json` is `pico2_w` — change `PICO_BOARD` there to switch.

### Debugging (Linux/WSL side, via OpenOCD + gdb)
- `openocd_start.sh` / `openocd_start.bat` — start OpenOCD for a CMSIS-DAP probe against rp2040.cfg
- `debug.sh` — attach `gdb-multiarch` (TUI) to `build/app.elf` using `scripts/debug.gdb`
- `program.sh` / `program.bat` — flash `build/app.elf` via gdb using `scripts/program.gdb`

### Testing
There is no on-device automated test suite. `tests/web_test.py` is a Flask app that serves the generated `web_portal.html` / `wizard.html` and stubs every `/rest/*` endpoint with fake JSON, so the web UI can be developed/iterated in a browser without real hardware:

```
pip install flask
python tests/web_test.py
```

## Architecture

### Build-time code generation (`src/CMakeLists.txt`)
Three things are generated into `src/generated/` before the app target builds, and are checked in as build artifacts, not hand-edited:
- `ws2812.pio.h`, `stepper.pio.h` — from `.pio` sources via `pico_generate_pio_header`
- `web_portal.html.h`, `wizard.html.h`, `display_mirror.html.h` — from `src/html/*.html` via `scripts/html2header.py`, embedding the web UI as C string constants that `rest_endpoints.c` serves directly
- `version.c` / `version.h` — from `scripts/gen_version.py`, embeds git rev + build type

If you edit `src/html/*.html`, `*.pio` files, or need a fresh version stamp, a rebuild regenerates these automatically — don't hand-edit files under `src/generated/`.

### Startup and task model (`src/app.c`)
`main()` runs hardware/module init in a fixed order (EEPROM → neopixel → display → wireless → motors → scale UART → charge mode → profile data → servo) before starting a single `menu_task` and handing off to `vTaskStartScheduler()`. Other tasks (display render, scale read loop, wireless/HTTP) are created by their owning modules during that init sequence, not from `main()` directly. Module init order matters because later modules assume earlier hardware (e.g. EEPROM) is already available.

### Module pattern
Each hardware/feature area is a `feature.c`/`feature.h` (or `.cpp`/`.h` when C++ helpers are needed) pair: `motors`, `scale` (+ per-brand drivers: `and_scale`, `steinberg_scale`, `gng_scale`, `ussolid_scale`, `jm_science_scale`, `creedmoor_scale`, `radwag_scale`, `sartorius_scale`, `generic_scale`), `charge_mode`, `cleanup_mode`, `servo_gate`, `neopixel_led`, `display` / `mini_12864_module` / `menu` / `mui_menu`, `wireless` / `access_point_mode` / `dhcpserver` / `dnsserver`, `eeprom`, `profile`, `system_control`. `scale.h` defines a small vtable-style `scale_handle_t` (`read_loop_task`, `force_zero`) that each brand-specific scale driver implements, selected at runtime via `scale_driver_t`.

### Persistent config (EEPROM)
`eeprom.h`/`eeprom.c` own the I2C EEPROM and a fixed address map (`EEPROM_*_BASE_ADDR` constants, one 1K-ish region per module: metadata, scale, wireless, motor, charge mode, app config, neopixel, mini12864, profile, servo gate). Each owning module keeps a live in-RAM config struct (`eeprom_*_data_t` / `eeprom_*_metadata_t`), a `const default_*` initializer, `*_config_init()`/`*_config_save()` functions, and registers its save handler via `eeprom_register_handler(...)` so `eeprom_save_all()` can persist everything at once. Preserving EEPROM layout/revision compatibility across firmware versions matters — see the `c-style` skill (Rule 5) before touching any `eeprom_*` struct.

### REST + web portal (`http_rest.*`, `rest_endpoints.*`, `rest_app_control.*`)
The web UI (served from the generated HTML headers above) talks to the firmware over a compact REST API implemented on top of lwIP's fs/CGI hooks. Handlers have the signature `bool http_rest_*(struct fs_file *file, int num_params, char *params[], char *values[])`, write a static JSON buffer into `file->data`, and set `file->len`/`file->index`/`file->flags` (`FS_FILE_FLAGS_HEADER_INCLUDED`, optionally `+HEADER_PERSISTENT` for static content). Parameter/response keys are short compact codes per module (e.g. `w0`=SSID, `m1`/`m2`=motor params, `c13`=charge param, `ee`=persist-to-EEPROM flag) — `tests/web_test.py` is the best quick reference for the current JSON shapes each endpoint returns. Handlers are wired up in `rest_endpoints_init()` via `rest_register_handler(path, handler)`.

### Hardware target config (`targets/`)
Board pin/peripheral assignments (UART instances, GPIO numbers, PIO/SPI/I2C instances) live in `targets/raspberrypi_pico_w_config.h`, pulled in through `src/configuration.h`. `PICO_BOARD_HEADER_DIRS` points CMake at this directory. Feature modules should consume pins via these macros, not hard-code them.

### Networking
`wireless.c` (AP or station mode, mDNS), `access_point_mode.c`, `dhcpserver.c`, `dnsserver.c` implement the on-device network stack on top of lwIP/CYW43, running under FreeRTOS. lwIP calls made outside the network task must be bracketed with `cyw43_arch_lwip_begin()`/`cyw43_arch_lwip_end()` — follow existing call sites in `wireless.c` for the locking pattern.

## Project skills

This repo ships Claude Code skills under `.claude/skills/` (mirrored in `.agents/skills/`) that encode conventions in more depth than this file — they load automatically for matching work:
- `c-style` — C/C++ conventions for this firmware (naming, module shape, EEPROM pattern, FreeRTOS/PIO/REST/display patterns). Read this before any non-trivial `src/**/*.c|h|cpp` change.
- `py-style` — Python conventions, applies to `scripts/*.py` and `tests/web_test.py`.
- `development-plan-writing` — required structure for any development/migration/refactor plan document.
