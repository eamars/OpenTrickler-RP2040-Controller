# ADR-006: the application apart from the platform

Status: **Draft** 2026-11-13, written again with the owner 2026-11-26 and 27. Not approved. Each module a folder of its own, built on its own with what it needs named; a board that picks its modules and their optimisation; the application apart from the chip; our code in C. Nothing a user sees changes, and nothing of ADR-007 is built here.

## What we want

- Each module in a folder of its own, by what it does, built on its own, with the modules it needs named and linked.
- A board that picks its modules, and each module's optimisation: a module left out of a board that runs out of RAM or flash.
- The application apart from the driver: no Pico SDK or cyw43 name outside the platform and the boards.
- Ready for ADR-007, with none of ADR-007 built here.
- Our code in C.
- The same behaviour as before, shown on the unit after every gate. Stored settings byte for byte as they are.

## Owner's direction

**2026-11-13**
- Abstract the application layer from the target, to support the ESP32 platform.
- One implementation above the chip.
- Some parts need a rewrite, not a move. The HTTP stack was one (ADR-003, built).

**2026-11-26**
- "I want to make each module to be built independently":
  - modules grouped under their folder;
  - modules built on their own, with their dependencies linked to each other;
  - each module can build for its target, and the target, as ADR-007 asks, picks the right target and dependencies.
- What for: "not to prevent failure, but to add modularity, and prepare to support ADR-007". It should allow per-module compiler optimisation, and leaving some module out for some target, "for example when Pico W ran out of RAM or flash".
- "I accept code changes, but not functional changes."
- Divided by function, perhaps the way ESP-IDF handles its modules. A "perhaps": checked below (ESP-IDF's components, followed, with one level of folders by layer added).
- Platform or BSP: the owner asked for a recommendation (below: both, for two different things).
- "For ADR-006 I don't want to include ADR-007 changes, so only separating the application from the driver."
- A build for the PC: the owner liked it, with a caution: "underneath you need a board, either ESP32 or Pico to drive the stepper motors". So the PC build is for tests only (below).
- Plan ahead: nothing built now, the ideas logged. "The earlier the breakout happens the less pain to move later, so ADR-006 can happen before ADR-014."

**2026-11-27**, on the agent's questions
- A module a board leaves out: "the EEPROM should serve the space even if no module is linked."
- The rest of the firmware learns of an absent module from the parts the module hands over at start-up, the switch in one place: agreed.
- One optimisation list a board, for the bench's builds and the release's alike: "That's the issue with the space, not the how it is built."
- "Small bug fixes are acceptable, as long as they don't need extensive tests."
- Stored enums written as fixed-width types: acceptable, "but we need to make sure the existing Pico targets are unaffected."
- C++: "Can we remove C++, or replace with the C version?" The agent found it small (below); the owner took it.

## Why here

- **After ADR-009, before ADR-014** (the owner, 2026-11-26). ADR-007 comes after ADR-014.
- **Nothing moves while ADR-009 is open.** Its branch was 111 commits ahead of main on 2026-11-26; a move of every file under it would meet nearly every one of its commits.
- What this ADR was to wait for is done: the PRs decided (ADR-002), the REST handlers cut (ADR-003), the motor module split by role (ADR-012). PR #113 ends in ADR-009, which closes first.
- **ADR-005** (Decided) keeps its trigger: an image that no longer fits its slot. This ADR gives it the means: a board's list of modules and each module's optimisation.

## What we know

Checked 2026-11-27 on ADR-009's branch (`adr009_restart` at 1765df3), the code this ADR starts from.

### The build

- One program, `app`, from every file CMake finds in `src/` (a glob), and one include path for all of them: any file can include any header, so nothing in the build stops one module from reaching into another.
- LVGL is already a library of its own, at `-Os`. mbedTLS is at `-Os` and `sha256.c` at `-O2`, by flags on their files. Our own code is at `-O0` (ADR-010).
- The one part a build may leave out, the servo gate (`-DFEATURE_SERVO_GATE=OFF`), takes `servo_gate.c` off the list and is read in 15 lines of four files: `api_v1.c`, `api_v1_commands.c`, `menu_nav.c` and `servo_gate.h`.
- The bootloader is a second program, and already shares one source with the image, `picobin_image.c` (`targets/flash_layout.cmake`).
- The host tests (`tests/host_test.py`, `tests/step2_sim.py`, `tests/calibration_sim.py`) list their sources by hand and stand in for six headers (`tests/host/stubs`: `FreeRTOS.h`, `semphr.h`, `task.h`, `hardware/dma.h`, `hardware/pio.h`, `pico/time.h`).

### The code

- `src/` is one flat folder: 61 `.c`, 4 `.cpp`, 54 `.h` and 2 `.pio` files, 33,470 lines (12,750 on 2026-11-13).
- 40 files include the Pico SDK (`pico/`, `hardware/`, `boot/`); three include cyw43 (`wireless.c`, `access_point_mode.c`, `dhcpserver.c`). `common.h` brings `FreeRTOS.h` and `hardware/pio.h` into nearly every file.
- Four files reach past the SDK into registers: the servo's PWM slice (`servo_gate.c`), the step pulses' PIO (`step_pulses.c`), and the image slots (`update_slots_rp2040.c`, `update_slots_rp2350.c`). The two others of the 2026-11-13 list, the TMC's receiver and the ring oscillator, are no longer in the code.
- 14 `xTaskCreate` calls in 10 files.
- The scale drivers each read the serial port themselves (`uart_is_readable` in 12 files: the drivers, `scale.c` and `motors.c`).
- 20 files reach into another module's state through `extern`: `scale_config` in 16, `charge_mode_config` in 8, each scale driver's handle, `neopixel_led_config`.
- **Calls that go up**, from a module into one that should sit above it:
  - charge mode and the job task wake the menu (`menu_wake`); charge mode and cleanup ask which page it shows (`menu_get_current_form_id`);
  - the event log calls the update (`update_version_text`, `update_write_reset`);
  - the Wi-Fi starts the web's endpoints (`rest_endpoints_init`);
  - the settings' one description (`settings.c`) includes the web server's header;
  - the update includes the menu's, the display's and the web's headers;
  - the restart (`system_control.c`) closes the web's clients first (`http_server_close_all`).
- **Includes that nothing uses:** `eeprom.c` includes ten other modules' headers and calls into one, the restart after an erase (it already works by the save handlers the modules register); `scale.h` includes `app.h`, of which it uses nothing.
- **Already free of the chip:** `charge_controller`, `charge_tuner`, `calibration_method`, `charge_mode_layout`, `tmc2209` and `tmc_uart`, `http_json`, `FloatRingBuffer`, and `menu_view_12864` (LVGL only). The host tests build the first four, with `common.c`'s loader.

### C++

- Six files, 2,105 lines: `charge_mode.cpp` (1,218), `cleanup_mode.cpp` (250), `mini_12864_module.cpp` (302), `FloatRingBuffer.cpp` and `.h` (210), `gpio_irq_handler.h` (125).
- `cleanup_mode.cpp` uses nothing of C++. `charge_mode.cpp` uses only `FloatRingBuffer`: four buffers, of 10, 10, 5 and 5 readings, made inside its functions. `mini_12864_module.cpp` uses only the interrupt class.
- `FloatRingBuffer` is the one real class. Its constructor takes its array from the heap by a `malloc` it does not check, and its destructor frees it.
- `gpio_irq_handler.h` is a table of the pins' interrupt callbacks behind a singleton.
- The image carries none of C++'s runtime: the SDK builds C++ with no exceptions and no RTTI, and the linker drops `new` and `delete` (the Pico W's map, 2026-11-26).
- 42 headers carry `extern "C"` guards.

### The stored records

- Eight fields in five stored records are enums: the charge's `decimal_places`, the display's `display_rotation`, the LEDs' chain count and colour order, the scale's driver, baud rate and serial format, and the Wi-Fi's `auth`.
- Each is one byte with the Pico's compiler (gdb on the Pico 2 W's image, 2026-11-27). The Wi-Fi record is 108 bytes, the scale's 52.
- `cyw43_auth_t` is our own enum (`wireless.h`, values 0 to 3) with cyw43's name. The draft's "cyw43's own `cyw43_auth_t`" was wrong.
- ESP-IDF's compiler makes an enum four bytes, as far as is known (not checked). There, the Wi-Fi record would be 112 bytes, with every field after `auth` moved.
- `tests/release_records.py` checks each record's v1.21 fields by type and name, in order. A field written as `uint8_t` in place of its enum is, to it, a change of type. It does not check AGENTS.md's filler rule (an appended field never starts in the old struct's tail padding).

### How other builds do it (read, not built here)

- **ESP-IDF** (its documentation): its unit is the component. A component is a folder, named for it, with a `CMakeLists.txt` of one call: `idf_component_register(SRCS, INCLUDE_DIRS, PRIV_INCLUDE_DIRS, REQUIRES, PRIV_REQUIRES)`. `REQUIRES` are what its headers need, `PRIV_REQUIRES` what only its sources need. A chip's own code lies in subfolders of the low components (`hal`, `soc`), picked by the target. A component's options are in its Kconfig.
- **LVGL**'s own `CMakeLists.txt` builds it as an ESP-IDF component under ESP-IDF and as a plain CMake library otherwise (`library/lvgl/env_support/cmake/esp.cmake`): one folder, two build systems.
- **The Pico SDK**'s libraries are INTERFACE libraries: their sources compile in whatever links them. Each has a `_headers` twin with only its headers (`library/pico-sdk/src/CMakeLists.txt`), for a library built against the SDK and linked into the image later.

## What we do

### Modules

- A module is a folder named for it, with `include/` (what other modules may include) and its sources, and a build file of one call:
  `ot_module(SRCS ... INCLUDE_DIRS ... PRIV_INCLUDE_DIRS ... REQUIRES ... PRIV_REQUIRES ...)`.
  The arguments are those of ESP-IDF's `idf_component_register`, and no others.
- Under the Pico SDK the call makes a static library: `INCLUDE_DIRS` and `REQUIRES` public, the `PRIV_` ones private. A module can include only the headers of the modules it names: the compiler refuses the rest. That is what makes the layout hold, not the folders.
- In ADR-007 the call gets its ESP-IDF branch, which passes the arguments on, as LVGL does.
- A module's name is its folder's, and no two are the same (ESP-IDF's rule).
- Folders by layer, and in each a module by what it does. A module requires only modules of its own layer or below:

```
app/        start-up and wiring: the one place that knows the board's modules
ui/         the menu, the 12864's settings
web/        the HTTP server, the API, the pages
console/    the remote control's console
services/   charge, cleanup, calibration, jobs, scale, motors, profile, settings,
            storage, events, update, wireless, servo gate, LEDs
drivers/    the TMC2209, the UC1701, the CAT24C256, the scale protocols
common/     helpers with nothing above in them: JSON, the ring buffer
platform/   include/ (the interfaces), rp2/            (ADR-007: esp32s3/)
boards/     pico_w/, pico2_w/                          (ADR-007: esp32s3_pico/)
```

Every file's place, a first draft, is in [The layout](#the-layout-a-first-draft-2026-11-27) below.

### Boards and the platform

- Two words for two things, as Zephyr keeps them (a board, and its chip):
  - **A board** (`boards/<name>/`) is the controller board as the module in its socket sees it: its pins, its flash layout, its modules, each module's optimisation, the third-party code's settings (`FreeRTOSConfig.h`, `lwipopts.h`, `mbedtls_config.h`, `lv_conf.h`), and the SDK libraries the image links. Today `pico_w` and `pico2_w`; ADR-007 adds `esp32s3_pico`. The controller board is the same under each: only the module in its socket changes.
  - **The platform** (`platform/`) is the chip. `include/` holds the small interfaces (below); `rp2/` gives them on the Pico SDK for the RP2040 and the RP2350, their differences inside it (the slots, the PIO version) as today.
- "BSP" is not used for the chip: in ESP's world (its BSP components) it means a board.
- **The drivers for the controller board's parts** (the TMC2209, the UC1701, the CAT24C256, the scale protocols) sit above the platform, on its serial port, SPI and I2C: written once, for every module that goes in the socket.
- Only `platform/rp2` names the Pico SDK's libraries, and by their `_headers`. The board's file links the libraries themselves into the image, once; otherwise SDK code would compile into several libraries.
- **lwIP stays where the network code uses it.** ESP-IDF's network is lwIP too (its documentation; not built here), so the sockets are the same on both chips. Only the Pico's settings of it, `lwipopts.h`, go to the board. This changes the draft, which kept lwIP inside the platform.

**The interfaces**, each a header in `platform/include` (the draft's list, with the slots added by ADR-004):
- storage: read and write a block;
- a serial port: open, its format, read, write;
- step pulses: a rate for a pin, and stop;
- an LED strip: write colours;
- a servo: a pulse width;
- inputs: a pin's edges as events (the encoder, the buttons);
- the display's bus: write bytes, and its three control pins;
- the network: start as access point or station, its state and address;
- the image slots: write, check, try, confirm (ADR-004);
- the system: restart, enter the USB loader, the unit's id, a random number, the time, the hard fault's hook;
- a task: one call that takes a stack size in bytes, since the two FreeRTOS ports count it differently.

### What a board carries

- A board's file lists its modules. A module a build may leave out is also a CMake option, as the servo gate is today, printed as it configures (`Features: ...`) and reported in the index's `features`, as now.
- **A module hands its parts to the rest at start-up**, from the wiring in `app/`: its settings table, its API routes, its console commands, its EEPROM save handler (storage already works this way). The wiring is the one place that asks whether a module is in the build.
- **The menu keeps its one table** (ADR-010): a row names what it acts on, and a row whose module is absent is left out, its page's count taken from its list, as now.
- **The EEPROM keeps every module's space, linked or not** (the owner). The slot map stays one list (`eeprom.h`). An absent module's record is never read, reset or written, so a later build with the module reads it back as it was stored. A factory reset still clears the whole EEPROM, as today.
- **A stored choice that names an absent module never runs another in its place:** the unit says so at start, and what needs it refuses (fail early). Today no stored choice names a module a build can leave out; the rule is for the first that does. Why it matters: a scale driver read as another one once nearly ran the simulated scale while the motors poured real powder (AGENTS.md, v1.21's 9).
- **Each module's optimisation is in the board's file:** ours at `-O0` unless the board says otherwise, LVGL and mbedTLS at `-Os` and `sha256.c` at `-O2`, as ADR-010 decided. One list a board, for the bench's builds and the release's alike: what the bench proves is what users get. A module at `-Os` is harder to step through on that board; the owner: that is the cost of the space, not of the build.

### A build for the PC, for tests only

- **The PC is never a board** (the owner: underneath there is always an ESP32 or a Pico to drive the motors). The PC build has no product, and turns no motor.
- **What it builds:** the modules meant to be free of the chip, with stand-ins where a test needs one (an EEPROM in RAM, the RTOS's headers).
- **What it runs:** today's host tests (`host_test.py`, `step2_sim.py`, `calibration_sim.py`), their sources and include paths taken from the modules in place of their lists by hand.
- **What it gains this ADR:** a module meant to be free of the chip that is not fails to build on the PC. It is the check that the cut was made, before any ESP32 exists.
- **Its meter:** the stand-ins. Today six headers, three of them the SDK's only because `common.h` pulls them in. At this ADR's end only the RTOS's should be left.
- **What it is not:** a simulated unit (FreeRTOS's PC port, LVGL in a window, the PC's sockets). That is a third platform to keep alive; the motors' timing cannot be judged there; the bench and `tests/web_test.py` cover what it would show. The motors' side stays the Python simulation (`tools/charge_sim`), as now.
- **A later use**, once the scale's parsers are code with nothing of the chip in it: the frames the bench records (`scale_frames`'s tap) read by them on the PC.

### C only

- `cleanup_mode.cpp` renamed `.c`: it uses nothing of C++.
- **`FloatRingBuffer` in C:** a struct and its functions, the array given by the caller on its stack (40 bytes at most), alive as long as today's. No heap, nothing to free on each return path, and the unchecked `malloc` gone.
  - Checked once, on the PC: the same readings through the old class and the new functions, their sums, means and standard deviations compared. A check at the change, not a kept test: the charge judges a steady reading by them, and a slip there would be silent.
  - The job task's stack read with `stats tasks` before and after.
- `charge_mode.cpp` then renamed `.c`.
- `mini_12864_module.cpp` and `gpio_irq_handler.h` go with the platform's inputs interface, which the interrupt table becomes.
- The `extern "C"` guards stay: harmless, and they let C++ code call ours, as ESP-IDF's own headers do.
- After this, the build still has C++ switched on for the SDK's own C++ files. Whether the SDK builds with it off was not checked and does not matter: none of our code is C++.

### Stored records apart from the compiler

- The eight enum fields written as fixed-width types of their own width (`uint8_t` for each today), their values unchanged; `cyw43_auth_t` renamed as ours.
- **The Pico boards unaffected** (the owner): each record's size, and each field's offset (gdb's `ptype /o`), on both images before and after, the same; `release_records.py` and `host_test.py` pass.
- `release_records.py` takes such a change only when it is declared: a field whose type becomes one of the same width, its values the same.
- Its reason is ADR-007's: a record laid out the same by both compilers. It is here because on the Pico it changes no byte.

### The small fixes

The owner: "Small bug fixes are acceptable, as long as they don't need extensive tests." Each was read again in the code 2026-11-27 and is still there:
- **The radio's `exit(-1)`** (`wireless.c`): a radio that fails stops a core. The unit goes on without Wi-Fi instead, shown by a test build whose radio start fails.
- **A failed save reported as done:** `eeprom_save_all` ignores what each handler returns, and returns 37, the old menu's page number. It returns whether every save worked.
- **The pre-1.17 revision path** (`common.c`, `load_config_layout`): a record whose first two bytes equal the old revision number is taken with no CRC and no check of its size. It is below the v1.21 floor (the owner, 2026-11-26), so it goes, once `host_test.py` has shown v1.21's records still read without it.
- **The Generic scale's sign apart from its digits:** "- 12.34" reads as no number, so a lifted cup goes unseen. Its parser reads the sign; checked on the PC with frames.
- **The scale's reader task not swapped with its make:** it goes by design when the scale becomes one reader with a parser a make, on the platform's serial port. Each make's framing stays as it is today, byte for byte, compared old against new on the PC once.
- **The ring buffer's unchecked `malloc`**, with C only.

### Handed on to an ADR of its own

These change what a user sees, or need tests that are not small:
- **A CRC that fails writes the defaults at once** (`common.c`): one bad read wipes a module's settings. When to write, and how the unit says so, is a design.
- **An EEPROM that does not answer:** the start-up's results are ignored, the unit comes up blank, then calls through NULL; the I2C calls have no time limit. The fix is a screen that fails early, proven on a unit with its EEPROM taken off.
- **The other makes' frames taken at their line endings:** no scale of theirs is on the bench to prove it.
- **The filler rule checked by `release_records.py`:** it needs each record's layout from the compiler, at v1.21 and now.

Its exit: that ADR is drafted before this one closes.

Gone since they were handed on: the menu's one-byte writes into enums (`mui_menu.c`, removed by ADR-010), and the unit's id printed with no ending (`unit_id.c`, written again by ADR-008, makes it with its ending, by `snprintf`).

## Gates

Each proven on the unit before the next.

1. **Modules and boards.** The module call and the boards' files; every file moved into its module's folder; each module requiring what it includes today, loops and all (CMake takes a loop between static libraries); includes that nothing uses dropped; the tools that name `src/` paths following (the host tests, `release_records.py`, the build scripts, CI, AGENTS.md's table). No other line of code changes. The module graph (CMake's `--graphviz`) goes into progress.md, its loops shown. This gate is merged by a PR of its own as soon as it is proven: a move costs most while other branches are open.
2. **The PC build** from the modules, in place of the host tests' lists.
3. **C only**, but for the 12864's module and its interrupt class, which go with the inputs interface (gate 5).
4. **The loops cut**, one at a time: each call that goes up turned into a call down or a part handed over at start-up, and each `extern` of another module's state turned into that module's call.
5. **The platform's interfaces**, one at a time, in the order above. `common.h` without the SDK. At the end no `pico/`, `hardware/` or cyw43 include outside `platform/` and `boards/`.
6. **Stored records apart from the compiler.**
7. **What a board carries:** the parts handed to the rest from the wiring, the servo gate's switch first; each module's optimisation from the board's file; the EEPROM's space kept for an absent module.
8. **The small fixes**, each with its own check.

## How the unit shows it works

After every gate, on the units it needs (AGENTS.md: the build and the platform on both boards, the rest on the unit on the probe):
- `rc_run.py flash`, then `menu_tour --compare` equal to the unit's baseline, snap for snap, then `charge_cycle`;
- the size report within a few KB of before, on both boards;
- `release_records.py` and the host tests pass;
- a gate that touches a stored struct: its size and its fields' offsets the same as before, on both images.

At the end:
- the owner's real throw;
- no `pico/`, `hardware/` or cyw43 include outside `platform/` and `boards/`;
- no module requiring one in a layer above it (the module graph);
- a unit updated from the release before keeps its settings (`rc_run.py settings`, compared);
- the Pico W built with the servo gate left out by its board's list runs, and a build with it then reads its record back as stored;
- the PC build's stand-ins the RTOS's only.

## Not in this ADR

- ADR-007's work: the ESP-IDF build, the module call's ESP-IDF branch, `platform/esp32s3`, `boards/esp32s3_pico`.
- A simulated unit on the PC.
- The fixes handed on to an ADR of their own.
- Any change a user can see.
- The screens (ADR-010) and the controller (ADR-009).

## To settle before approval

- **The layout** below: every module's name and every file's place.
- **The stored enums against AGENTS.md's rule** that an old field is never retyped. By its letter, writing `decimal_places_t` as `uint8_t` is a retype; by its intent (the bytes and what they mean kept) it is not. The owner's word is needed before `release_records.py` is taught the difference.
- **lwIP outside the platform**, a change from the draft.
- **Gate 1 merged on its own**, as soon as it is proven.

## The layout, a first draft (2026-11-27)

Today's files by their module. A file split between a module and the platform is in both, its part named.

| Folder | Module | Today's files |
|---|---|---|
| `app/` | `app` | `app.c`, `app.h`; the version (`scripts/gen_version.py`) |
| `ui/` | `menu` | `menu.c`, `menu_nav.c`, `menu_view_12864.c` and their headers; the fonts (`fonts/`, `scripts/bdf2lvgl.py`) |
| `ui/` | `mini_12864` | `mini_12864_module.cpp`, `mini_12864_settings.c`, `button.c` |
| `web/` | `http_server` | `http_server.c` |
| `web/` | `api` | `api_v1.c`, `api_v1_commands.c`, `rest_endpoints.c`, `web/` (the portal, `scripts/web_build.py`), `html/plot_weight.html` (not served) |
| `console/` | `console` | `rc_console.c`, `remote_control.c` |
| `services/` | `charge` | `charge_mode.cpp`, `charge_mode_settings.c`, `charge_mode_layout.c` |
| `services/` | `charge_control` | `charge_controller.c`, `charge_tuner.c` (free of the chip) |
| `services/` | `cleanup` | `cleanup_mode.cpp` |
| `services/` | `calibration` | `calibration.c` |
| `services/` | `calibration_method` | `calibration_method.c` (free of the chip) |
| `services/` | `jobs` | `jobs.c` |
| `services/` | `activity` | `activity.c` |
| `services/` | `events` | `events.c` |
| `services/` | `settings` | `settings.c` |
| `services/` | `storage` | `eeprom.c`, `common.c` (the loader) |
| `services/` | `scale` | `scale.c`, `simulated_scale.c` |
| `services/` | `motors` | `motors.c` |
| `services/` | `servo_gate` | `servo_gate.c`, but for its PWM register |
| `services/` | `leds` | `neopixel_led.c` |
| `services/` | `profile` | `profile.c` |
| `services/` | `update` | `update.c`, `update_poll.c`, `https_client.c`, `github_roots.pem` |
| `services/` | `wireless` | `wireless.c` but for cyw43, `access_point_mode.c`, `dnsserver.c`, `setup.c`, `unit_id.c` (its hashes) |
| `drivers/` | `tmc2209` | `tmc2209.c`, `tmc_uart.c` |
| `drivers/` | `uc1701` | `display.c` |
| `drivers/` | `cat24c256` | `cat24c256_eeprom.c` |
| `drivers/` | `scale_protocols` | the ten `*_scale.c` of the makes, `generic_scale.h` |
| `common/` | `json` | `http_json.c` |
| `common/` | `ring` | `FloatRingBuffer.cpp` |
| `platform/` | `rp2` | `step_pulses.c`, `stepper.pio`, `ws2812.pio`, `gpio_irq_handler.h`, `update_slots.h` (to `include/`), `update_slots_rp2040.c`, `update_slots_rp2350.c`, `update_boot.h`, `picobin_image.c` (with the bootloader), `fault_handler.c`, `freertos_hooks.c`, `dhcpserver.c`; the restart and the USB loader of `system_control.c` (the web's clients closed first, by `app`); the cyw43 part of `wireless.c`, the servo's PWM register, the unit's unique id |
| `boards/` | `pico_w`, `pico2_w` | `targets/` (the pin map, the flash layout, the partition table), `configuration.h`, `FreeRTOSConfig.h`, `lwipopts.h`, `lwipopts_examples_common.h`, `mbedtls_config.h`, `lv_conf.h` |

The bootloader stays a program of its own, linking `platform/rp2`'s image reader.
