# ADR-006: the application apart from the platform

Status: **Open** 2026-11-27. Gates 1 to 3 approved by the owner, built and proven on the Pico W the same day: the build's mechanics, every file in its module's folder, and every module built on its own with what it needs named. Gates 1 and 2 are merged by #139, gate 3 by #140. Gate 4, the PC build, approved and built the same day: the host tests are built from the modules' own build files, with no unit needed; merged by #141. Gate 5, our own code in C, approved and built the same day and proven on the Pico W: the two modes and the ring buffer are C; merged by #142. Gate 6, the loops cut, approved and built the same day and proven on the Pico W: no module requires one of a layer above it, and none require each other; merged by #143. Gate 7, the platform's interfaces, approved and built the same day and proven on the Pico W: no file outside the platform and the boards includes the Pico SDK, and only the platform's RP side requires it; merged by #144. Gate 8, stored records apart from the compiler, approved. The gates after 8 are not approved. The Pico 2 W's runs wait, by the owner's word, until the whole ADR is proven on the Pico W.

## What we want

- Each module in a folder of its own, by what it does, built on its own, with the modules it needs named and linked.
- A board that picks its modules, and each module's optimisation: a module left out of a board that runs out of RAM or flash.
- The application apart from the driver: no Pico SDK or cyw43 name outside the platform and the boards.
- **A base build, and room for others.** Everything the firmware has today is the base build, on both boards. A later build opts a part in or out (a larger screen with its own UI, another kind of dispenser, the ESP32) by adding modules and a list, editing no shared module.
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

**2026-11-27, later**, on a second agent's review of this draft
- The review's points written in, with its steps in detail: "Yes revise, and include details of your steps."
- "All current features are considered as base features for the base build. Future changes may allow the opt in, or opt out of certain features, that ADR-006 should design for that, which includes but not limited to":
  1. "Larger LCD screen with alternative UI. Will likely exclude Pico W or even Pico 2W."
  2. "Supporting different trickler controller, like the future thrower/per kernel basis trickler (not rotary tube design). This would be different firmware build, for a soft ware switch to change the mode depending on the available FLASH space." Read here as: a build of its own where both do not fit, and one build with both and a stored switch where they do. To confirm (below, "To settle").
  3. "ESP32 support. which means discarding bsp and platform for Pico, and only include ESP IDF library for ESP32, but sharing the same application layer code."
  4. "More??" The agent's candidates are under [More lines](#more-lines-the-agents-candidates), for the owner to strike or add to.

**2026-11-27, the approval**
- Gate 1 approved, alone: "Approve." Gate 2 waits for the layout and for the Pico 2 W's runs ADR-009 handed on; the later gates for what is under "To settle".
- "I don't expect other agents to work in parallel so you don't need an isolated worktree."

**2026-11-27, gate 2**
- Approved, with the layout: "I'd like to work on gate 2 as approved."
- **The Pico W first, for the whole ADR:** "Pico 2W will be tested later. I want the full ADR to be tested on the most risky target Pico W before moving on to Pico 2W." Read here as: every gate is proven on the Pico W on the probe; both boards are still built and compared on the PC at every gate, which needs no unit; the Pico 2 W's runs on the probe are gathered after the last gate. It changes AGENTS.md's "a change to the build is proven on both" for this ADR's gates, not its end.
- **The layers lie under `src/`** (asked before the move, since a second move is what gate 2 is there to avoid): `src/` stays the firmware's one source root, and the repository's root keeps its folders. The draft read "in place of `src/`".
- The Pico W's menu baseline for `settings/display` saved again: "Yes please update."

**2026-11-27, the merge and gate 3**
- PR #139, gates 1 and 2, merged at the owner's word once its checks had passed: "Cool. Merge then."
- Gate 3 approved: "And continue with the gate."
- Asked why not a CMake file in each module's folder, built as a library, its dependencies declared there: that is gate 3. `ot_module()` is our own function in plain CMake, not CMake's and not an extension; its arguments are ESP-IDF's so one file serves both platforms. The owner: "Cool."

**2026-11-27, gate 3's merge**
- PR #140 merged once its checks had passed: "Merge when green."

**2026-11-27, gate 4**
- Approved: "Gate 4 approved. please continue."
- PR #141 merged once its check had passed: "Merge when green."

**2026-11-27, gate 5**
- Approved: "Approve gate 5."
- PR #142 reviewed and merged by the owner: "I reviewed and merged."

**2026-11-27, gate 6**
- Approved: "Approve gate 6."
- **The UI hands its parts down** (asked before cut 2, since the ADR had `ui/` above `web/` and `console/` and also had the web's mirror and the console's `snap` ask the view): `ui/` stays on top; the console's screen commands are the UI's and are registered with the console at start-up; the API is handed the frame and the hello. The other reading, `ui/` put below the web and the console so that they may name the menu, was not taken.
- PR #143 merged by the owner: "PR merged, continue with the next gate."

**2026-11-27, gate 7**
- Approved, in the same words: "continue with the next gate."
- **The interfaces are buses** (asked before cut 1, since the gate's list had storage as blocks, the display's bus with its pins inside and the inputs as events, while the drivers' line has the drivers above the platform on its serial port, SPI and I2C, and the motors' enable and direction pins fitted nothing on the list): GPIO (outputs, inputs, edges), I2C, SPI and a serial port, beside the step pulses, the image slots, the system, a task, an LED strip, a servo and the network. The CAT24C256, the UC1701, the TMC2209 and the scale's makes stay drivers above them, written once for every chip. The other reading, an interface per use, was not taken.
- PR #144 merged by the owner.

**2026-11-27, gate 8**
- Approved: "Gate 7 of ADR-006 is merged. I want you to check the progress, and continue to the next gate."

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

### The review on main (2026-11-27, main at 4719984, ADR-009 merged)

Read in the code and the build files. Nothing was built for it: what it says of the linker and of CMake is to be shown by gate 1.

- **The figures above hold on main:** 61 `.c`, 4 `.cpp`, 54 `.h`, 2 `.pio`, 33,650 lines; 40 files on the Pico SDK; 14 `xTaskCreate` in 10 files.
- **Third-party code is at `-Os` more widely than written above** (ADR-009, 2026-11-27): lwIP, the cyw43 driver and the Pico SDK's own code too, FreeRTOS's kernel left at the build's level. It is done by a property on their source files that names the target `app` and holds only for targets made in the root folder (`CMakeLists.txt`, the lines on `THIRD_PARTY_OS_SOURCES`). A module made in a folder of its own is neither.
- **A static library drops the hard fault's handler.** `isr_hardfault` is the only function in `fault_handler.c`, and nothing of ours calls it: it takes the place of the SDK's own, which `crt0.S` defines weak. A linker takes a file out of a static library only for a name still undefined, and this one is defined. So `fault_handler` as a static library is left out with no error, and a hard fault no longer stops the motors. The usual proof (flash, `menu_tour`, `charge_cycle`) does not see it; `pulses` does. The same holds for anything linked only because its file is on the link line.
- **The SDK's `_headers` twins do not carry all a module needs.**
  - Each SDK library's own mark, `LIB_<NAME>=1`, is put on the full library and not on its twin (`pico_add_impl_library`), and 11 of the SDK's headers read such marks. A module built on the twins can read a header differently from the image.
  - FreeRTOS has no twin: `FreeRTOS-Kernel-Core` carries the kernel's sources and its include path together (`library/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/library.cmake`). A module that links it compiles a kernel of its own.
  - The image's own definitions (`RC_TEST_*`, `FEATURE_SERVO_GATE`, read in nine of our files) are private to `app` today.
- **Two of the draft's module names are, or were, ESP-IDF's own.** `console` is one of ESP-IDF's components (its `components/` folder, read 2026-11-27 on its main branch), and a project's component of the same name takes its place. `json` is not there today; an earlier ESP-IDF had one, from memory, not checked.
- **The step pulses' seam is wider than "a rate and stop".** `step_pulses.h` (ADR-012): a rate for a channel; a feed every millisecond, with no feed no steps; the steps counted on the pin, the feeds that came late; a stop by register writes alone for the fault's handler. Each is a rule ADR-012 proved on the unit.
- **The serial port is read with a time limit** by the motor drivers' one-wire line (`uart_is_readable_within_us`, `motors.c`), not only as bytes come.
- **mDNS is lwIP's own responder** (`mdns_resp_*` in `wireless.c`, on cyw43's network interface). ADR-007 names ESP-IDF's in its place.
- **Who names the screen today:** `display.h` is included by 11 files and `mini_12864_module.h` by 13, among them `wireless.c`, `access_point_mode.c`, `update.c`, `charge_mode.cpp` and `cleanup_mode.cpp`. `menu.h` is included by the charge, the cleanup, the calibration, the jobs and the update. Each of the four jobs refuses its start while the menu shows no page (`menu_get_current_form_id() < 0`) and wakes the menu (`menu_wake`).
- **Who names the tubes today:** `motors.h` is included by 12 files, among them `api_v1_commands.c`, `menu_nav.c`, `remote_control.c`, `rest_endpoints.c`, `simulated_scale.c`, `fault_handler.c` and `eeprom.c`; `charge_mode.h` by 13.
- **The unit's `features`** is one name written by hand (`api_v1.c`, the servo gate's).
- **A release's files are named by their board** (`scripts/build_release.py`: `pico_w`, `pico2_w` and their update images). Nothing in an image or the manifest says which kind of build it is, there being one a board.

## What we do

### Modules

- A module is a folder named for it, with `include/` (what other modules may include) and its sources, and a build file of one call:
  `ot_module(SRCS ... INCLUDE_DIRS ... PRIV_INCLUDE_DIRS ... REQUIRES ... PRIV_REQUIRES ...)`.
  The arguments are those of ESP-IDF's `idf_component_register`, and no others.
- Under the Pico SDK the call makes a static library: `INCLUDE_DIRS` and `REQUIRES` public, the `PRIV_` ones private. A module can include only the headers of the modules it names: the compiler refuses the rest. That is what makes the layout hold, not the folders.
- **Every module is linked whole** (the linker's whole-archive, which CMake 3.24 and later give by `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`; the build asks for 3.25), and what nothing uses is dropped by section as today. A module's files then take part in the link exactly as they do now, on the link line: the hard fault's handler stays (the review, above). ESP-IDF has the same switch for a component (`WHOLE_ARCHIVE`).
- **Third-party code goes by ESP-IDF's names:** a module that needs the RTOS, the network, TLS or the screen's library requires `freertos`, `lwip`, `mbedtls` or `lvgl`. Under the Pico SDK those are targets of ours that carry the include paths of the real libraries and of all they need, and no source; the image links the real libraries once, from the board's file, where the third-party `-Os` rule also goes. Under ESP-IDF the same names are its own components, so a module's `REQUIRES` line is the same in both. How FreeRTOS's headers are spelt there (`freertos/FreeRTOS.h`) is ADR-007's, not checked.
- **The Pico SDK itself is `rp2_sdk`**, a fifth such target. A module that still includes the SDK requires it, so the modules that require `rp2_sdk` are gate 7's list, and at its end only `platform/rp2` does.
- **A module compiles with the image's definitions and options,** all of them, whatever it requires: the SDK's and the RTOS's settings are one view for the whole firmware, as when it was one program, and the test builds' and the features' (`RC_TEST_*`, `FEATURE_*`) reach every module from one place. Only a module's include paths follow what it requires. Shown in gate 1: each tried module's definitions the image's, 84 of 84 on the Pico W and 82 of 82 on the Pico 2 W.
- **Nothing a module sees is chosen on the image alone** (found in gate 1). The SDK's libraries answer by who asks: `pico_enable_stdio_rtt(app 1)` gave the image the RTT console's headers and left every other target, a module among them, with the UART's. So the console is chosen once for every target (`PICO_STDIO_RTT`, before the SDK starts), and the same holds for any other choice the SDK lets a target make for itself.
- In ADR-007 the call gets its ESP-IDF branch, which passes the arguments on, as LVGL does.
- A module's name is its folder's, and no two are the same (ESP-IDF's rule). **None is a name ESP-IDF uses:** the remote control's module is `remote_control`, not `console`, and the JSON helper's is `http_json`, its file's name. The layer folder `console/` is not a component and keeps its name. ADR-007 checks the rest against the ESP-IDF version it picks; against today's list none other is taken.
- **A static library can be had this way** (gate 1, on both boards' builds): the fallback the draft named, each module's files compiled in the image itself, is not needed.
- Folders by layer, and in each a module by what it does. A module requires only modules of its own layer or below:

```
app/        start-up and wiring: the one place that knows the board's modules
ui/         the menu; a view and its panel; the knob and the buttons
web/        the HTTP server, the API, the pages
console/    the remote control's console
services/   charge, cleanup, calibration, jobs, scale, motors, profile, settings,
            storage, events, update, wireless, servo gate, LEDs
drivers/    the TMC2209, the UC1701, the CAT24C256, the scale protocols
common/     helpers with nothing above in them: JSON, the ring buffer
platform/   include/ (the interfaces), rp2/            (ADR-007: esp32s3/)
boards/     pico_w/, pico2_w/                          (ADR-007: esp32s3_pico/)
```

The layers lie under `src/` (the owner, 2026-11-27): `src/app/`, `src/ui/` and so on. Every file's place is in [The layout](#the-layout-as-gate-2-built-it) below.

### Boards and the platform

- Two words for two things, as Zephyr keeps them (a board, and its chip):
  - **A board** (`boards/<name>/`) is one set of hardware a build is made for: the module in the socket, the controller board and the screen on it. It holds its pins, its flash layout, its modules, each module's optimisation, the third-party code's settings (`FreeRTOSConfig.h`, `lwipopts.h`, `mbedtls_config.h`), and the SDK libraries the image links. Today `pico_w` and `pico2_w`; ADR-007 adds `esp32s3_pico`. Today the controller board and its screen are the same under each, and only the module in the socket changes. A later board with another screen is a folder of its own that includes the same pin map: boards share by including a file, and there is no table of boards by parts.
  - **The platform** (`platform/`) is the chip. `include/` holds the small interfaces (below); `rp2/` gives them on the Pico SDK for the RP2040 and the RP2350, their differences inside it (the slots, the PIO version) as today.
- "BSP" is not used for the chip: in ESP's world (its BSP components) it means a board.
- **The drivers for the controller board's parts** (the TMC2209, the UC1701, the CAT24C256, the scale protocols) sit above the platform, on its serial port, SPI and I2C: written once, for every module that goes in the socket.
- Only `platform/rp2` names the Pico SDK's libraries, and by their `_headers`. The board's file links the libraries themselves into the image, once; otherwise SDK code would compile into several libraries.
- **lwIP stays where the network code uses it.** ESP-IDF's network is lwIP too (its documentation; not built here), so the sockets are the same on both chips. Only the Pico's settings of it, `lwipopts.h`, go to the board. This changes the draft, which kept lwIP inside the platform. What is not the sockets goes behind the network's interface: the radio, the access point's DHCP server, and the unit's name on the network (mDNS, lwIP's own responder today).

**The interfaces**, each a header in `platform/include` (the draft's list, with the slots added by ADR-004):
- storage: read and write a block;
- a serial port: open, its format, read, read within a time, write;
- step pulses: `step_pulses.h` as it is today, with its rules (a rate for a channel; fed every millisecond, and with no feed no steps; the steps counted on the pin and the late feeds; the fault's stop);
- an LED strip: write colours;
- a servo: a pulse width;
- inputs: a pin's edges as events (the encoder, the buttons);
- the display's bus: write bytes, and its three control pins;
- the network: start as access point or station, its state and address, its scan, the unit's name announced;
- the image slots: write, check, try, confirm (ADR-004), `update_slots.h` as it is today;
- the system: restart, enter the USB loader, the unit's id, a random number, the time, the hard fault's hook;
- a task: one call that takes a stack size in bytes, since the two FreeRTOS ports count it differently.

**How an interface is made:**
- It is today's header lifted, where one exists, not a new one drawn up. Each rule the code keeps today is written in the header, and none is dropped without the owner's word.
- **Checked on paper against ESP-IDF** before it is fixed: one row in progress.md for each call, ours beside the ESP-IDF call that would give it (its documentation read, nothing built). A seam with one platform behind it is a guess (ADR-007's words); the row is the cheapest thing that makes it less of one.
- No interface is made for hardware no board has.

### What a board carries

- A board's file lists its modules. A module a build may leave out is also a CMake option, as the servo gate is today, printed as it configures (`Features: ...`) and reported in the index's `features`, as now.
- **A module hands its parts to the rest at start-up**, from the wiring in `app/`: its settings table, its API routes, its console commands, its EEPROM save handler (storage already works this way). The wiring is the one place that asks whether a module is in the build.
- **The menu keeps its one table** (ADR-010): a row names what it acts on, and a row whose module is absent is left out, its page's count taken from its list, as now.
- **The EEPROM keeps every module's space, linked or not** (the owner). The slot map stays one list (`eeprom.h`). An absent module's record is never read, reset or written, so a later build with the module reads it back as it was stored. A factory reset still clears the whole EEPROM, as today.
- **A stored choice that names an absent module never runs another in its place:** the unit says so at start, and what needs it refuses (fail early). Today no stored choice names a module a build can leave out; the rule is for the first that does. Why it matters: a scale driver read as another one once nearly ran the simulated scale while the motors poured real powder (AGENTS.md, v1.21's 9).
- **Each module's optimisation is in the board's file:** ours at `-O0` unless the board says otherwise; third-party code at `-Os` but FreeRTOS's kernel, and `sha256.c` at `-O2`, as ADR-010 and ADR-009 decided. One list a board, for the bench's builds and the release's alike: what the bench proves is what users get. A module at `-Os` is harder to step through on that board; the owner: that is the cost of the space, not of the build.
- **The unit's `features` is made from the board's list**, not written by hand: every part a build may leave out, by name, present or absent (ADR-008: no feature silently dropped).

### The base build, and what a later build changes

The owner, 2026-11-27: everything the firmware has today is the base build; later builds opt parts in or out, and this ADR designs for it.

- **The base build** is what `boards/pico_w` and `boards/pico2_w` list: every module of today. It is what the bench proves and what a release ships. In ADR-005's words it is the core and the standard tier together, with the update's fetch over HTTPS.
- **This ADR builds no other build.** It makes the cuts along the lines below, so a later build is new modules and a list. Where a line needs an interface that only its second side can shape, the interface is left to that side's ADR, and this ADR writes down what names the first side today.
- **An option exists only with a build that uses it,** in CI, and a unit to prove it on (ADR-005: every optional module is one more build to keep working). No switch is added for a build nobody makes.

**Three kinds of pick** in a board's list:

| Kind | What it is | Today | Wrong pick |
|---|---|---|---|
| In or out | a module the build carries or leaves out | the servo gate | a module that needs an absent one fails at configure, by its `REQUIRES` |
| One of | a place only one module can fill | the chip's platform; the screen's view | none, or two, fails at configure |
| Several, one chosen | modules linked together, a stored setting picks one at start | the scale's make | a stored choice whose module is absent: the unit says so at start, and what needs it refuses (above) |

A stored choice is a stored enum: its values are never renumbered or reused, whatever a build carries (AGENTS.md).

**The lines, and what this ADR does for each**

*The chip* (the owner's 3: the ESP32, ADR-007).
- The seam is `platform/include`. An ESP32 build has nothing of `platform/rp2`, of the Pico's boards or of the Pico SDK; it has ESP-IDF, `platform/esp32s3` and its board, and every module above them unchanged.
- This ADR: the interfaces; third-party code by ESP-IDF's names; no ESP-IDF name taken by a module of ours; stored records the same under both compilers; the PC build as the modules' second build.
- Left to ADR-007: everything of ESP-IDF.

*The screen and its UI* (the owner's 1: a larger LCD with another UI, likely not on the Pico W, perhaps not on the Pico 2 W).
- ADR-010 already split the menu: the tree and what is open (`menu_nav.c`), a view that draws it (`menu_view_12864.c`), the panel's driver (`display.c`). Another display is another view.
- But the screen is named well outside the UI today (the review, above): by the Wi-Fi, the update and the jobs. Until those are cut, no build can take another screen without edits in each.
- This ADR:
  - `ui/` is the only layer that names a screen. Nothing below requires a module of `ui/`: the jobs' calls into the menu and the others' includes of the display go (gate 6).
  - The 12864 is three modules: its view (`view_12864`, with the fonts and LVGL's settings, `lv_conf.h`, which follow the panel's depth), its panel's driver (`uc1701`), and the knob and buttons (`mini_12864`). `menu` names none of them.
  - That takes a cut inside `menu.c`, which today is both the menu's task and the 12864's one-bit frame (its buffer, its lock, the pages sent to the panel). The frame's part goes to the view; the task, the keys and the jobs' screens stay. It is a move of code within the UI, shown by `menu_tour --compare` and the menu's frame times.
  - The view is a "one of".
- **The web's mirror and `snap` copy the 12864's frame** as it is, 128 by 64 at one bit. They ask the view for it; with another view, what they get and how the page draws it is that screen's ADR's.
- **Two readings of "alternative UI"**, both of which then fit:
  - another view of the one menu table: ADR-010's plan. A new setting is placed once;
  - a UI of its own, with its own pages and its own way round them (touch): the whole of `ui/` is the "one of". A new setting is then placed twice, and each UI has its own `menu_tour` baseline.
- ADR-005's rule 1, the owner's of 2026-11-13, says "one screen UI". The second reading changes it. To settle (below).
- Left to the screen's own ADR: its panel, its view or its UI, its board, LVGL's cost at its depth.

*The dispenser* (the owner's 2: a thrower, or a trickler by the kernel, not rotary tubes).
- **The tubes' modules today:** `motors`, `tmc2209`, the step pulses, `charge` (the charge mode), `charge_control`, `cleanup`, `calibration`'s powder part, `servo_gate`, and the powder profile's tube figures. The simulated scale follows the tubes' speeds, so it is theirs too.
- **What any dispenser shares:** the scale; a job with a target (start, stop, status, events); the settings, the API and the menu as frames; storage; the update; the network.
- **No dispenser interface is designed here.** Its shape depends on what a thrower needs, which nobody has written down; made now, it would be the tubes' own shape with a new name.
- This ADR:
  - the tubes' modules hand their own parts over at start-up (their settings tables, API routes, console commands, save handlers), so `web/`, `console/` and `storage` stop naming them (gates 6 and 9);
  - at its end, progress.md lists every module that still names a tubes' module, and for what. That list is where the thrower's ADR starts. Expected in it: `jobs`, the menu's table, the API's commands, the hard fault's handler;
  - a dispenser's stored records are its own slots. Another dispenser takes new slots, never the tubes'; and the EEPROM keeps an absent module's space (above), so a unit moved to another build and back has both.
- **A build each, or a switch:** the dispenser is a "several, one chosen". Where both fit (the Pico 2 W's 4 MB, the ESP32's 16 MB) one build links both and a stored choice picks at start; where they do not (the Pico W), a build each. The choice is a new stored enum, appended, by the ADR that adds the second dispenser.
- Left to the thrower's ADR: the interface, its modules, its records, its screens and pages, its bench rules.

*A build's name, and an update.*
- Today a board has one build, and a release's files are named by board. With a second build for a board, an update must offer a unit its own kind, or a tubes' unit takes a thrower's image.
- Not built here: it is a change to the update (ADR-004), and there is one build a board. The first ADR that makes a second build for a board does it: the image says which build it is, the manifest lists by board and build, and the unit refuses another kind unless asked.
- This ADR's part: the `features` made from the board's list, which is where a build's name comes from.

#### More lines (the agent's candidates)

The owner asked for more. Each is a line the cuts above already serve or nearly serve; none is designed or built here, and the owner strikes or adds.

| Line | Kind | What it needs of this ADR |
|---|---|---|
| The update's own fetch left out (ADR-005's fallback for the Pico W: updates by file only) | in or out | `update_fetch` (the poll, the HTTPS client, the roots) a module apart from `update` (the job, the slots). It has a trigger already, so the layout splits it now |
| A board with no radio (a Pico, a Pico 2): no network, no pages, set up at the screen (ADR-010 put every needed setting there) | in or out, a group | nothing new: what needs the network requires `wireless`, so leaving out the radio alone fails at configure |
| The scale's makes, each a module; a scale not on a serial port (Bluetooth, ADR-005's extended tier) | several, one chosen | the makes hand themselves to `scale` in place of its eleven `extern`s (gate 6); the reader apart from the parser is handed on (below) |
| An agent's side of the API (ADR-016: a fuller record of each throw, a PIN) | in or out | routes handed to `web/` by the module that owns them (gate 9) |
| Storage that is not the CAT24C256: a controller board without it, on the chip's flash | one of | the storage interface, a block read and written. Already here |
| Another motor driver chip, or a second controller board | one of, by board | a board as a folder that includes shared files. Already here |
| A second language on the screen and the pages | not served | the words are in the code and the pages. It would be an ADR of its own; nothing here helps or hinders it |

### A build for the PC, for tests only

- **The PC is never a board** (the owner: underneath there is always an ESP32 or a Pico to drive the motors). The PC build has no product, and turns no motor.
- **What it builds:** the modules meant to be free of the chip, with stand-ins where a test needs one (an EEPROM in RAM, the RTOS's headers).
- **What it runs:** today's host tests (`host_test.py`, `step2_sim.py`, `calibration_sim.py`), their sources and include paths taken from the modules in place of their lists by hand.
- **How:** a small CMake project under `tests/host` that takes the chosen modules' folders as they are, with `ot_module` given for the PC (a library by the PC's compiler, `freertos` a target of the stand-ins). It is the module call's second build, before ESP-IDF's.
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
- **The ring buffer's unchecked `malloc`**, with C only.

### Handed on to an ADR of its own

These change what a user sees, or need tests that are not small:
- **A CRC that fails writes the defaults at once** (`common.c`): one bad read wipes a module's settings. When to write, and how the unit says so, is a design.
- **An EEPROM that does not answer:** the start-up's results are ignored, the unit comes up blank, then calls through NULL; the I2C calls have no time limit. The fix is a screen that fails early, proven on a unit with its EEPROM taken off.
- **The other makes' frames taken at their line endings:** no scale of theirs is on the bench to prove it.
- **The scale as one reader with a parser a make**, and with it the reader task not swapped with its make. The draft had it among the small fixes. It is not one (the review): it writes the reading of ten makes again, with one make's scale on the bench. Here each make keeps its task and its framing, and only its serial calls are swapped one for one for the platform's (gate 7). The one reader is designed with the line endings, where a parser can be shown against recorded frames.
- **The filler rule checked by `release_records.py`:** it needs each record's layout from the compiler, at v1.21 and now.

Its exit: that ADR is drafted before this one closes.

Gone since they were handed on: the menu's one-byte writes into enums (`mui_menu.c`, removed by ADR-010), and the unit's id printed with no ending (`unit_id.c`, written again by ADR-008, makes it with its ending, by `snprintf`).

## Gates

Each proven on the unit before the next. Ten gates, written again after the review (2026-11-27): the draft's first gate is three (the build's mechanics tried, the move, the libraries), each with one thing to prove; the rest are the draft's, in its order.

**The check a move is judged by,** used by gates 1 to 3 and 5: the image's symbols before and after (`arm-none-eabi-nm -S`: each function and object by name and size). Our code is at `-O0`, so a file moved or built as a library gives every function the size it had. A function gone, new or of another size is a change to explain. It is quick, it needs no unit, and it would have shown the hard fault's handler dropped.

### Gate 1: the build's mechanics, tried on two modules

Nothing moves. It settles whether a module can be a static library at all, before 120 files depend on it.

1. `ot_module` written (the Pico SDK's branch only), and two modules made where their files lie today: `fault_handler.c`, linked only by its place on the link line, and one service that uses FreeRTOS and the SDK (`events.c`).
2. The four third-party targets (`freertos`, `lwip`, `mbedtls`, `lvgl`) and the SDK's headers for `platform/rp2`, with no source in them.
3. Shown, on both boards' builds:
   - **linked whole:** `isr_hardfault` in the map comes from our file; `pulses` passes on the Pico W, all three of its faults;
   - **the same compile:** one file's compile command before and after (`compile_commands.json`): the same definitions and the same third-party include paths, but for their order;
   - **third-party code once, at its level:** every SDK, lwIP, cyw43 and mbedTLS source compiled once, at `-Os` (`sha256.c` at `-O2`, FreeRTOS's kernel at the build's);
   - **the image:** its symbols as before; its size within 1 KB;
   - **the others still build:** the Pico W's bootloader, a test image (`-DRC_TEST_FAULT=1`), a release (`build_release.py`).
4. If a point cannot be had, the fallback (above, "Modules") is tried the same way, and the owner is told which it is before gate 2.

What was found goes into progress.md. It stays in the ADR's branch and is merged with gate 2.

**Built 2026-11-27** ([progress](progress.md)), on the PC for both boards and on the bench's Pico W: `pulses` passed, all three faults, and the menu is pixel for pixel what main's firmware showed on the same unit that morning. A module can be a static library: both boards' images are symbol for symbol what they were. Found on the way: a plain static library does drop the hard fault's handler, with no error; the SDK's libraries answer by who asks, so the console is now chosen for every target; and the image's own include paths cannot be handed out in parts.

### Gate 2: the move

One thing: every file in its module's folder. Still one program, built as today.

1. Every file moved by `git mv` (so its history follows), a module's public headers into its `include/`. The layout below, as approved.
2. The build finds the sources in the module folders in place of `src/`, and every module's `include/` is on the one include path. No library yet, no `#include` line changed.
3. What names a path follows, in the same commit: the build scripts and `scripts/` (the version, the pages, the fonts, the release), the three host tests, `release_records.py`, `tools/size_report.py` and `tools/ui_mock`, CI, `.vscode`, the skills, and AGENTS.md's table of where code goes. `src/generated` stays what the build makes, in a place of its own.
4. Proof: the symbols as before, both boards. Then the Pico W on the probe: `flash`, `menu_tour --compare`, `charge_cycle`; the host tests and `release_records.py`. The Pico 2 W the same when it is on the probe (a change to the build: both boards).
5. Merged by a PR of its own at once: a move costs most while other branches are open.

**Built 2026-11-27** ([progress](progress.md)): 138 files moved, both boards' images symbol for symbol what they were, the Pico W's bootloader byte for byte; on the Pico W `pulses`, the menu tour against its baseline and a charge cycle pass. The Pico 2 W's run waits (the owner, above). Merged with gate 1 by #139.

### Gate 3: the modules as libraries

One thing: each module built on its own. No file moves.

1. Each module's build file with its one call, requiring what its files include today, loops and all (CMake takes a loop between static libraries). Every module linked whole.
2. The boards' folders: each board's list of modules, their optimisation, the third-party settings' headers, the SDK's libraries and the third-party `-Os` rule; the flash layout from `targets/`.
3. What the build makes goes to the module that uses it: the PIO headers to `platform/rp2`, the pages to `api`, the fonts to the view, the version to `app`, the roots to `update_fetch`.
4. Includes that nothing uses dropped (`eeprom.c`'s ten, `scale.h`'s `app.h`): the only lines of code this gate changes.
5. The module graph (CMake's `--graphviz`) into progress.md, its loops and its upward arrows marked: gate 6's list.
6. Proof: the symbols as before, both boards; a module made to include a header it does not require fails to compile (tried once, not kept); `pulses`; the five release builds and the test images build; the size within 1 KB; then `flash`, `menu_tour --compare`, `charge_cycle` on both boards.
7. Merged by a PR of its own.

**Built 2026-11-27** ([progress](progress.md)): 34 modules, each with its build file, both boards' images symbol for symbol what they were; on the Pico W `pulses`, the menu tour and a charge cycle pass. What differs from the steps above:
- five files did move, to folders of their own (`boards/common/settings`, `ui/view_12864/lv_conf`), so that requiring FreeRTOS or LVGL does not hand a module the pin map or the view's header;
- `app.h` was dropped from fourteen more files that used nothing of it, the same kind of include as the two named;
- the graph in progress.md is `tools/module_graph.py`'s, read from the modules' build files, not CMake's `--graphviz`, which draws the SDK's hundred targets with ours: 25 requirements go up, 21 modules are in 2 loops (one of 19), 20 modules require the Pico SDK;
- a module's optimisation is still the build's own: a board's list of them comes with gate 9.

### Gate 4: the PC build

1. The CMake project under `tests/host`, `ot_module` given for the PC, the stand-ins as its `freertos` target.
2. The three host tests take their sources and include paths from it, their lists by hand gone. CI runs it as it runs them today.
3. The meter written down: the stand-in headers, six today.
4. Proof: each test ends `VERDICT PASS` on Windows and in CI; a module of the PC build made to include an SDK header fails to build there (tried once).

**Built 2026-11-27** ([progress](progress.md)): `tests/host/CMakeLists.txt` builds four modules whole from their own build files (`http_json`, `tmc2209`, `calibration_method`, `charge_control`), and the three tests from them; each test passes on Windows, and the project builds and its test passes on Linux. What differs from the steps above:
- **six modules are taken in part**, which the steps had not foreseen. The loader's test needs one source of `storage` and one of `charge`, and neither module is free of the chip; four more are needed for their headers. A part is the module's headers and what they require, from its own build file, and only the sources the PC build names. Two source names by hand are what is left of the lists;
- **the meter is two counts:** the stand-in headers, six as before (three of FreeRTOS's, three of the Pico SDK's, in a folder each), and the modules taken in part, six;
- `ring`, the fifth free module, waits for gate 5: it is C++ and the PC build is C only;
- CI's host test takes CMake and Ninja from the toolchain it already fetches.

### Gate 5: C only

1. `cleanup_mode.cpp` renamed `.c`. Proof: its functions' sizes compared, each difference explained (another compiler front end reads the same lines); a purge on the unit.
2. The ring buffer in C (`ring`), its array the caller's. Checked once on the PC against the old class with the same readings (above). Then `charge_mode.cpp` on it, and renamed `.c`.
3. Proof: `charge_cycle`; the job task's stack by `stats tasks` before and after; the image's size.
4. `mini_12864_module.cpp` and the interrupt class wait for gate 7's inputs.

The unit on the probe is enough (the application's layer).

**Built 2026-11-27** ([progress](progress.md)): `cleanup_mode` and `charge_mode` are C; the class is `float_ring`, a struct and five functions over its user's array, with no heap; on the Pico W a charge cycle, the menu tour and the API's check (a purge in it) pass, the job task's stack 16 bytes deeper. What differs from the steps above:
- **one line would have changed meaning in C with no error:** the zero test's `abs()` of a float, which is the float's in C++ and the integer's in C. It is `fabsf`. The ADR had said the file used only the class of C++;
- **the sizes were not the measure:** 19 functions changed size by a few bytes with nothing changed in their lines (the two front ends pick other instructions at `-O0`). The proof is what each function calls and touches, counted by name: the same, but for the ring, `fabsf`, and C's `isnan` and `isfinite` done in place;
- a ring the cup-return wait made and never used is gone, and of the class's 17 methods only the four the charge used are carried over;
- the purge was run by `api_check`, which starts and stops one by the API.

### Gate 6: the loops cut

One cut a commit. After each, the module's `REQUIRES` line loses a name, and from then on the compiler refuses its return.

In this order, the cheapest first, and those that free the screen and the tubes (the lines above) before the rest:
1. **The jobs and the menu.** The four jobs wake the menu and ask whether it shows a page. A first idea, not read through: turned round, the menu follows the activity (`activity`), which the jobs already mark, and a job asks `activity` whether a start may be taken. Each of the four is read first, and what the question guards today is kept.
2. **The screen named from below:** the includes of `display.h` and `mini_12864_module.h` in the Wi-Fi, the access point, the update and the modes. Each use read, then cut or turned into something the UI reads. With it, the frame's part of `menu.c` to the view (above, "The screen and its UI").
3. **The update:** its includes of the menu, the display and the web; the event log's calls into it (the version's text and the reset's record handed to `events` at start-up).
4. **The web started by the Wi-Fi** (`rest_endpoints_init`): started by `app`.
5. **The restart closing the web's clients:** a list of things to do before a restart, which `web` joins.
6. **The settings' description and the web server's header:** the settings' HTTP side to `api`, the tables, checks and stores staying.
7. **Other modules' state by `extern`:** `scale_config` (16 files) and `charge_mode_config` (8) behind their module's calls; the scale's eleven handles handed to `scale` by the makes; `neopixel_led_config`.

Proof, each cut: both boards build; the path it touched run on the unit, by the command that has it (a restart and its clients: `api_check`; the update: `slot_trial` and `update` from the local OTA server; a job's start refused and taken: `charge_cycle`, a purge, `reach`).
At the gate's end: `tools/module_graph.py` with no loop and no requirement upward (25 and 2 at gate 3); `menu_tour --compare`, `charge_cycle`, `api_check`, `settings_roundtrip`. The unit on the probe, and the Pico W for the network's cuts.

**Built 2026-11-27** ([progress](progress.md)): `tools/module_graph.py` counts no upward requirement and no loop (25, and 21 modules in 2 loops, at gate 3). On the Pico W each cut's path was run before the next, and at the end the menu tour, a charge cycle, the API's check, the settings' round trip, every screen reached, the three fault stops, the scale's frames, and an update over the air, twice. What differs from the steps above:
- **the gate was larger than its seven cuts.** The built libraries showed more: the console driving the menu and reading the web server; the API copying the menu's frame; three drivers reaching into services (the scale's makes, the EEPROM's, the display's); the hard fault's handler calling the motors; and loops among the services the text had not named (the job task and its jobs, the update and its fetch, the settings and the Wi-Fi, the scale and the charge). All are cut;
- **every cut is one move:** what a lower module needs of a higher one is handed to it at start-up, by the higher module or by `app`'s wiring, and it calls what it was handed. Thirteen such hand-overs, listed in progress.md;
- **the settings' HTTP side did not move as code** (cut 6): the settings answer the API in a buffer and name no web; the routes and the server's calls are the API's. The same end, with 745 lines left where they were;
- **the makes are handed to the scale by `app`**, not by themselves: a make that registered itself would name the scale, upward;
- two variables are still reached by `extern`, inside `ui/` (the knob's queue and the display's settings): gate 7's inputs;
- the update's own proof was an update over the air from the local OTA server, in place of `slot_trial`: it runs the same install and trial, and the fetch besides;
- the Pico W's RAM at link time went from 252.1 KB to 252.4 KB of 256 KB, for the hand-overs' tables.

### Gate 7: the platform's interfaces

One interface a commit, each made as "How an interface is made" says: today's header lifted, its rules written in it, its row against ESP-IDF in progress.md.

The order is by what is nearest done, so the first ones show the way:

| | Interface | From | Its proof on the unit |
|---|---|---|---|
| 1 | step pulses | `step_pulses.h`, as it is | `pulses`; `motor`'s counts; a soak's late feeds |
| 2 | the image slots | `update_slots.h`, as it is | `slot_trial` confirmed and back; an update from the local OTA server |
| 3 | the system | the restart, the USB loader, the id, the random number, the time, the fault's hook | `reset`; the unit's id and its hotspot's key as before; `pulses` |
| 4 | a task | the 14 `xTaskCreate` | `stats tasks`: every stack as before |
| 5 | storage | the CAT24C256's I2C calls | `settings --compare` across a restart; `settings_roundtrip` |
| 6 | a serial port | the scale's makes and the motor drivers' line, call for call | `scale_frames` with the A&D connected; `tmc <coarse\|fine> setup <n>`; a real reading in `state` |
| 7 | the display's bus | the UC1701's SPI and its three pins | `menu_tour --compare` |
| 8 | inputs | the interrupt class, in C; with it `mini_12864_module.cpp` | `knob`'s tap, `press`, `reach` |
| 9 | an LED strip | the WS2812's PIO | the owner's eye: the LEDs are the 12864's backlight, which no snap shows |
| 10 | a servo | the PWM slice's register | no gate is on the bench: the pulse's width read from the chip's registers through the probe, before and after |
| 11 | the network | cyw43 in `wireless.c`, the access point's DHCP server, mDNS | `stream_check`, `web_robust`, a soak with `--load`; the set-up's join from the hotspot |

Last, `common.h` without the SDK. At the end no `pico/`, `hardware/` or cyw43 include outside `platform/` and `boards/`, shown by a search and by the PC build's stand-ins, the RTOS's only.

Both boards, each interface (drivers, the PIO, timing against the chip); the network on the Pico W.

**Built 2026-11-27** ([progress](progress.md)): `tools/module_graph.py` ends `VERDICT PASS`: no module but `platform/rp2` requires the Pico SDK (21 at gate 6), and no file outside `platform/` and `boards/` includes it or the radio's driver, which the tool now searches for itself. The PC build's stand-ins are FreeRTOS's alone. Each call is in progress.md beside the ESP-IDF call that would give it (v6.1.1's documentation, read). On the Pico W each interface's path was run before the next. What differs from the steps above:
- **the interfaces are buses** (the owner's pick, before cut 1): GPIO, I2C, SPI and a serial port, the drivers above them, in place of storage as blocks, the display's bus and the inputs as events;
- **the image slots grew** by what an image says of itself, its write into the free slot and the trial's watchdog: `update.c` named the flash, the image's format and the watchdog itself;
- **the console** (RTT) is in the system's interface, which the list did not name; **a critical section** is in the task's, which the paper check found ESP-IDF's port to differ in;
- **the hotspot's DNS server went to the platform** with the DHCP server: both are lwIP's raw API, which ESP-IDF does not support, and so is lwIP's mDNS responder;
- **the pin map is plain numbers**, and the board's target no longer carries the SDK to every module;
- the Pico 2 W was built at every commit and not run, by the owner's word: the whole ADR on the Pico W first.

### Gate 8: stored records apart from the compiler

As above. Proof: each record's size and each field's offset on both images before and after (`ptype /o`), the same; `release_records.py`, taught the declared change, and `host_test.py` pass; `settings --compare` on the unit across the flash.

### Gate 9: what a board carries

1. **The parts handed over** from `app`'s wiring: each module's settings table, routes, console commands and save handler. The servo gate first: its 15 lines in four files become its own module's, and `app` is the one place that asks whether it is there.
2. **`features` made from the board's list.**
3. **Each module's optimisation from the board's file.**
4. **The EEPROM's space kept** for an absent module: never read, reset or written.
5. **The three kinds of pick, each shown once on a real build:**
   - in or out: the Pico W built with the servo gate left out by its board's list runs, its `features` says absent, its menu and pages have no trace of it; built with it again, it reads its record back as stored;
   - one of: a board's list with no view, and with two, refused at configure;
   - several, one chosen: a test build with one scale's make left out, on a unit whose stored make is that one. The unit says so at start, a charge refuses, and no other make runs. (Proposed: below, "To settle".)
6. **The lines' lists into progress.md:** every module that still names a tubes' module, and every one outside `ui/` that still names the screen (none), each with what for.

Proof besides: `menu_tour --compare`, `charge_cycle`, `api_check`; the size by module (`tools/size_report.py`), both boards.

### Gate 10: the small fixes, and the close

1. Each fix with its own check (above).
2. The ADR for what was handed on is drafted.
3. The end's checks (below), the owner's real throw among them.
4. Closed in its branch, then its PR.

## How the unit shows it works

After every gate, on the Pico W on the probe (the owner, 2026-11-27: the whole ADR on the Pico W first, the riskier board; both boards built and compared on the PC at every gate; the Pico 2 W's runs after the last):
- `rc_run.py flash`, then `menu_tour --compare` equal to the unit's baseline, snap for snap, then `charge_cycle`;
- the size report within a few KB of before, on both boards (within 1 KB for gates 1 to 3, which change no code);
- `release_records.py` and the host tests pass;
- a gate that touches a stored struct: its size and its fields' offsets the same as before, on both images;
- a gate that moves files or changes how they are linked (1 to 3): the image's symbols as before, and `pulses`, since the usual proof does not see a handler the linker dropped;
- what each gate names for itself (above, "Gates").

At the end:
- the owner's real throw;
- no `pico/`, `hardware/` or cyw43 include outside `platform/` and `boards/`;
- no module requiring one in a layer above it, and none below `ui/` requiring one of `ui/` (the module graph);
- a unit updated from the release before keeps its settings (`rc_run.py settings`, compared);
- the Pico W built with the servo gate left out by its board's list runs, and a build with it then reads its record back as stored;
- the three kinds of pick each shown on a real build (gate 9);
- the lines' lists in progress.md: what still names the tubes, and that nothing outside `ui/` names the screen;
- each interface's row against ESP-IDF in progress.md;
- the PC build's stand-ins the RTOS's only.

## Not in this ADR

- ADR-007's work: the ESP-IDF build, the module call's ESP-IDF branch, `platform/esp32s3`, `boards/esp32s3_pico`.
- Any build but the base build: a larger screen or its UI, another dispenser or its interface, a board with no radio. Each is an ADR of its own; this one makes the cuts they need.
- A build's name in its image and in a release's manifest: the first ADR that makes a second build for a board.
- A simulated unit on the PC.
- The fixes handed on to an ADR of their own.
- Any change a user can see.
- The screens (ADR-010) and the controller (ADR-009).

## To settle before approval

From the draft, each with the review's recommendation:
- **The layout** below: every module's name and every file's place.
- **The stored enums against AGENTS.md's rule** that an old field is never retyped. By its letter, writing `decimal_places_t` as `uint8_t` is a retype; by its intent (the bytes and what they mean kept) it is not. The owner's word is needed before `release_records.py` is taught the difference. Recommended: yes, by its intent, the check taught to compare a field's width and its values.
- **lwIP outside the platform**, a change from the draft. Recommended: yes, with the radio, the DHCP server and mDNS behind the network's interface.
- **The move and the libraries each merged on their own** (gates 2 and 3), as soon as each is proven. Recommended: yes.

From the owner's direction of 2026-11-27 on later builds:
- **The dispenser: a build each, or a switch.** The owner's words are read here as both: one build with both and a stored switch where they fit, a build each where they do not. Is that the meaning?
- **"Alternative UI" against ADR-005's rule 1** ("one screen UI", the owner's, 2026-11-13). Another view of the one menu table keeps the rule. A UI of its own changes it: every new setting is then placed in two UIs. The cuts here allow either. Recommended: the rule kept for small boards (never a cut-down second version of the same thing), and a larger screen's UI decided by its own ADR, once its panel and its input are known.
- **The "several, one chosen" pick shown on the scale's makes** (gate 9): a test build with one make left out, on a unit whose stored make is that one. It is the only proof the rule gets before a second dispenser exists, and gate 6 hands the makes to `scale` anyway. The base build still carries all ten. Recommended: yes.
- **The scale as one reader handed on** (above), in place of the draft's small fix. Recommended: yes.
- **The more lines:** which the owner strikes, and what is missing.

## The layout, as gate 2 built it

Every folder below is under `src/`. The table is the draft's of 2026-11-27, which the owner approved, with what gate 2 settled:
- **A module's headers for others are in its `include/`**: 51 of the 54. A header no other module includes stays beside its sources (`dnsserver.h`, `generic_scale.h`, `remote_control.h`).
- **A file a later gate splits went whole** to the module that keeps most of it: `menu.c` to `ui/menu` (its frame to the view in gate 6); `wireless.c`, `unit_id.c` and `servo_gate.c` to their services (their chip parts to `platform/rp2` in gate 7).
- **`services/system`** is `system_control.c`, which the draft's table named only by the part gate 7 takes to the platform.
- **`boards/common`** holds what both boards share today (the pin map, the flash layout, the third-party code's settings); `boards/pico2_w` its partition table. A board's own list of modules comes with gate 3.
- **`platform/include`** has `update_slots.h`; the other interfaces' headers come up from `platform/rp2/include` one by one in gate 7.
- **The portal** is `web/api/portal` (it was `src/web`, and `web/` is a layer now).
- **`fonts/` and `scripts/` stay at the repository's root**, though the table gives the fonts to the view: `tools/ui_mock` on the PC reads the same files, and nothing of the build is clearer for moving them.
- `src/generated/` is where the build writes, as before.

Today's files by their module. A file split between a module and the platform is in both, its part named.

| Folder | Module | Today's files |
|---|---|---|
| `app/` | `app` | `app.c`, `app.h`; the version (`scripts/gen_version.py`) |
| `ui/` | `menu` | `menu.c` but for the 12864's frame, `menu_nav.c` and their headers |
| `ui/` | `view_12864` | `menu_view_12864.c`; the frame's part of `menu.c` (gate 6); the fonts (`fonts/`, `scripts/bdf2lvgl.py`); `lv_conf.h` |
| `ui/` | `mini_12864` | `mini_12864_module.cpp`, `mini_12864_settings.c`, `button.c` |
| `web/` | `http_server` | `http_server.c` |
| `web/` | `api` | `api_v1.c`, `api_v1_commands.c`, `rest_endpoints.c`, `web/` (the portal, `scripts/web_build.py`) |
| `console/` | `remote_control` | `rc_console.c`, `remote_control.c` (not `console`: ESP-IDF's own) |
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
| `services/` | `update` | `update.c` (the job, the slots' use) |
| `services/` | `update_fetch` | `update_poll.c`, `https_client.c`, `github_roots.pem`: apart, as the part ADR-005's fallback leaves out |
| `services/` | `system` | `system_control.c`, but for the restart and the USB loader |
| `services/` | `wireless` | `wireless.c` but for cyw43, `access_point_mode.c`, `dnsserver.c`, `setup.c`, `unit_id.c` (its hashes) |
| `drivers/` | `tmc2209` | `tmc2209.c`, `tmc_uart.c` |
| `drivers/` | `uc1701` | `display.c` |
| `drivers/` | `cat24c256` | `cat24c256_eeprom.c` |
| `drivers/` | `scale_protocols` | the ten `*_scale.c` of the makes, `generic_scale.h` |
| `common/` | `http_json` | `http_json.c` |
| `common/` | `ring` | `FloatRingBuffer.cpp` |
| `platform/` | `rp2` | `step_pulses.c`, `stepper.pio`, `ws2812.pio`, `gpio_irq_handler.h`, `update_slots.h` (to `include/`), `update_slots_rp2040.c`, `update_slots_rp2350.c`, `update_boot.h`, `picobin_image.c` (with the bootloader), `fault_handler.c`, `freertos_hooks.c`, `dhcpserver.c`; the restart and the USB loader of `system_control.c` (the web's clients closed first, by `app`); the cyw43 part of `wireless.c`, the servo's PWM register, the unit's unique id |
| `boards/` | `common`, `pico2_w`; `pico_w` from gate 3 | `targets/` (the pin map, the flash layout, the partition table), `configuration.h`, `FreeRTOSConfig.h`, `lwipopts.h`, `lwipopts_examples_common.h`, `mbedtls_config.h` |

The bootloader stays a program of its own, linking `platform/rp2`'s image reader.

The tubes' modules in this table, for the dispenser's line: `charge`, `charge_control`, `cleanup`, `calibration` (its powder part), `calibration_method`, `motors`, `servo_gate`, `tmc2209`, the step pulses in `platform/rp2`, and `simulated_scale.c` in `scale`. They stay flat in their layers: a folder for them would be drawn before the thrower's ADR says where the line runs.
