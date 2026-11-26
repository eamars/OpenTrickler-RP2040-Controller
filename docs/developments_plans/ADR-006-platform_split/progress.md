# ADR-006: progress

What was built and measured. The plan is in the [README](README.md). The builds compared are kept on this PC under `build_adr006/`, which git does not track.

## Gate 1: the build's mechanics, tried on two modules

2026-11-27, approved by the owner alone ("Approve"). On the branch `adr006_platform_split`, from main at 93277cf. Nothing moved: two files of `src/` are built as modules where they lie, and the rest as before.

**The verdict: a module can be a static library.** Both boards' images are, symbol for symbol, what they were, and on the Pico W the step pulses stop on a hard fault as before. Three things had to be found first, each below.

### What was built

- `cmake/ot_module.cmake`: `ot_module()`, with the arguments of ESP-IDF's `idf_component_register` and no others; `ot_third_party()`, third-party code as a module sees it; `ot_image_settings()` and `ot_link_modules()`.
- Two modules, in `src/CMakeLists.txt`:
  - `fault_handler` (`fault_handler.c`): one function, called by nothing of ours, in place of the SDK's weak `isr_hardfault`;
  - `events` (`events.c`): a service on FreeRTOS and the SDK, with other modules' headers and a generated one (`version.h`).
- Five third-party names: `freertos`, `lwip`, `mbedtls`, `lvgl`, and `rp2_sdk` for the Pico SDK itself.
- `tools/symbols_compare.py`: two images compared by their symbols, each function and object by name and size, ending `VERDICT PASS` or `FAIL`. The check gates 2, 3 and 5 are judged by.
- The console chosen for every target (`PICO_STDIO_RTT`, in the root `CMakeLists.txt`) in place of `pico_enable_stdio_rtt()` on the image.

### Before anything was changed

Both boards built from main's code (93277cf) into `build_adr006/before_w` and `before_2w`, with their compile commands kept:

| | Pico W | Pico 2 W |
|---|---|---|
| Image (`app.bin`) | 938,308 bytes | 923,692 bytes |
| Sized symbols | 4,688 | 4,693 |
| Bootloader | 6,488 bytes | none |

### Found 1: a static library drops the hard fault's handler, with no error

The review said so from reading; here it is built. The two modules linked as plain static libraries, the Pico W's image:
- linked with no error and no warning;
- `isr_hardfault` in its map is the SDK's own, in `crt0`;
- `symbols_compare.py` against the baseline: four functions gone, `isr_hardfault`, `motors_fault_stop`, `step_pulses_fault_stop` and `gpio_set_outover`. That is the whole of what stops the motors on a hard fault.

Linked whole (`$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`), `isr_hardfault` comes from `src/libot_fault_handler.a(fault_handler.c.obj)` on both boards, and the four are back.

- CMake defines that link feature for the systems it knows, and none for the Pico SDK's (`CMAKE_SYSTEM_NAME PICO`): the first configure failed on it. `ot_module.cmake` gives CMake's own three lines for GNU ld.
- The image's other libraries are given by `target_link_libraries`' plain signature, and CMake takes one signature or the other for a target: the modules are linked by the plain one too.

### Found 2: the SDK's libraries answer by who asks

A module's include paths come from the libraries a third-party name stands for (`$<TARGET_PROPERTY:<library>,INTERFACE_INCLUDE_DIRECTORIES>`, which CMake follows through all they need). The first build of it gave the two modules one path the image does not have, and lacked three it has: the UART console's headers in place of the RTT's.

- Why: `pico_stdio` links the console a target chose for itself (`$<TARGET_PROPERTY:PICO_TARGET_STDIO_RTT>`, read from whoever is being built), and falls back on the SDK's default for a target that chose none. `pico_enable_stdio_rtt(app 1)` chose for the image only. A module is a target of its own.
- 17 such per-target choices are read in the SDK's build files (`PICO_TARGET_STDIO_*`, `PICO_TARGET_BINARY_TYPE`, the `*_IMPL` ones, the linker script's). The image set three of the console's and no other.
- The fix is at the choice, not at the modules: the console is chosen for every target by the SDK's own global switches (`PICO_STDIO_RTT 1`, `PICO_STDIO_UART 0`, `PICO_STDIO_USB 0`), set before the SDK starts. The image's own compile commands did not change by it, not one.
- **It reaches the Pico W's bootloader**, the one other target: its SDK sources now compile with `LIB_PICO_STDIO_RTT` in place of `LIB_PICO_STDIO_UART`, and the RTT's two sources compile for it in place of the UART's one. The bootloader uses no console, and the linker drops either: **`bootloader.bin` is byte for byte the same**, 6,488 bytes, and its 86 symbols are the same.
- The rule that follows, written into the README: nothing a module sees is chosen on the image alone.

### Found 3: the image's own include paths cannot be handed out in parts

Tried in place of Found 2's fix: the third-party names as parts of the image's own include paths, picked by where they lie (`$<FILTER:$<TARGET_PROPERTY:app,INCLUDE_DIRECTORIES>,...>`), so that a module's paths would be the image's by construction.

- It configured, and failed to compile: a module that required `freertos` and `rp2_sdk` got FreeRTOS's two paths and none of the SDK's (`pico.h: No such file or directory`).
- Why, shown by writing the lists out at generate time: on its own each part is whole (the SDK's part had 78 paths). Within one target's evaluation CMake works a property of a target out in full only the first time it meets it, so the second part of the same list came back with the image's own six paths and nothing the image's libraries give.
- So it cannot be done this way, and the first design stands: paths from the libraries named, definitions from the image.

### The proofs, on the PC

**The images** (`tools/symbols_compare.py`, before against after):

| | Pico W | Pico 2 W |
|---|---|---|
| Symbols | 4,688, `VERDICT PASS` | 4,693, `VERDICT PASS` |
| `app.bin` | 938,316 bytes, 8 more | 923,700 bytes, 8 more |
| `isr_hardfault` from | `libot_fault_handler.a` | `libot_fault_handler.a` |

The 8 bytes are in `.text` and in no symbol: the two modules' files now come last on the link line, and the padding between sections fell differently. `tools/size_report.py` gives the same parts to the tenth of a KB before and after (the Pico W: 915.0 KB of its 1,016 KB slot), and counts a module's files as the application's without a change.

**The same compile** (every compile command of the build, before against after):
- the two modules: the same definitions (84 of 84 on the Pico W, 82 of 82 on the Pico 2 W) and the same options; include paths 90 down to 66 and 67 (91 down to 65 and 66), none new. What went is what they do not require: lwIP, the radio's driver, mbedTLS, LVGL, and what the image has through those;
- every other source of the image: not one changed, on either board;
- third-party code in the image: each source compiled once, at the level it was (the Pico W: 264 at `-Os`, `sha256.c` at `-O2`).

**What a module may include:** `fault_handler.c` made to include `lvgl/lvgl.h`, which it does not require: `fatal error: lvgl/lvgl.h: No such file or directory`. Tried once, not kept. Between our own files nothing is refused yet: both modules have all of `src/` as their path until gate 2 gives each its folder.

**A loop between modules** (the Pico W's build): the two made to require each other, publicly: it configured, linked, and the symbols were the same. Tried once, not kept. Gate 3 builds on it.

**The other builds:**
- the two fault test images for `pulses` (`-DRC_TEST_FAULT=1` and `=2`, the Pico W): both build, the hard fault's handler from the module in each;
- a whole release (`scripts/build_release.py`, its five builds, into `build_adr006/`): made, the update images the size of the plain ones (938,316 and 923,700 bytes), `isr_hardfault` from the module in the Pico W's slot B image and the Pico 2 W's update image;
- `tests/host_test.py` and `tests/release_records.py`: `VERDICT PASS`.

### On the unit

2026-11-27, the bench's Pico W on the probe, the bench ready by the owner's word, the hopper empty (the owner), the A&D connected. The images rebuilt from the gate's commit (30b819a) first, so each reports 2.0.3-287.

- **Its settings copied before the flash** (`logs/adr006/picow_before_gate1`, the unit then on 2.0.3-277), and compared after everything below: 17 resources, the same.
- **`rc_run.py flash`:** the unit runs 2.0.3-287 at 30b819a.
- **`pulses`: `VERDICT PASS`.** The step pulses stopped on a reset through the probe, on an assert and on a hard fault. The third is the one a plain static library would have failed, with nothing else to show it. The cup read 0.00 gr before and 0.0 gr after.
- **The menu, before against after on the same unit.** Today's tour, pixel by pixel against the tour ADR-009's bench run took this morning on main's firmware (2.0.3-277; `src/` is the same from there to main but for the unserved `src/html` removed): 17 of 18 snaps the same, the Wi-Fi's and the scale's pages among them. The one that differs is `settings/firmware`, in the version's text.
- **`menu_tour --compare tests/menu_baseline/pico_w`: 13 pages the same, 3 not judged, and `settings/display` differs: not this gate's.** Its baseline picture has Rotation "Upside down"; the unit's stored rotation has been "As mounted" since ADR-008's gate 4, and this morning's tour on main's firmware failed on the same page the same way. Set to the baseline's rotation live, the page gains a "Restart to apply" row and differs by its scroll bar. The picture can only be made right by saving that page again: the owner's to say, since no screen changed.
- **`charge_cycle`: `VERDICT PASS`**, 30.000 for a target of 30.000, 13.0 s from charging to the cup prompt.

The Pico 2 W was not on the probe. Its image is its baseline's, symbol for symbol, with the hard fault's handler from the module; its run of `pulses` goes with gate 2's, when it is on the probe.

**Gate 1 is built.**

### For the later gates

- **`rp2_sdk` is short of what our code includes.** It stands for the libraries the image names (`pico_stdlib`, the PIO, SPI, I2C, PWM, `pico_flash`). The image gets more through the network's libraries: `hardware/flash.h`, `pico/unique_id.h`, `pico/rand.h`, `hardware/dma.h`. Gate 3 names those the modules include.
- **Three of the image's own paths** are not any third-party's and went from the modules: the board's header folder (`targets/`), the PIO headers the build makes, and the roots' header (`https/`). Gate 3 gives each to the module that uses it (the board, `platform/rp2`, `update_fetch`).
- **Every compile command compared** is how gate 3 is judged beside the symbols: the script used here is `build_adr006/cc_compare.py`, not kept in the tree; gate 3 makes it a tool if it is used again.
- **The third-party `-Os` rule** names the target `app` and holds for targets made in the root folder. It was untouched here, the image still compiling the SDK's sources itself. Gate 3 must keep the image a target of the root folder, or move the rule with it.
