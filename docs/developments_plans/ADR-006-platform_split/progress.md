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

## Gate 2: the move

2026-11-27, approved by the owner with the layout ("I'd like to work on gate 2 as approved"). The layers under `src/`, asked of the owner before anything moved. Commit bd00cf0.

**The verdict: every file is in its module's folder, and both boards' images are symbol for symbol what they were.**

### What was done

- **138 files moved by `git mv`**, each a rename to git: 134 from `src/` (the portal's twelve among them) and the four of `targets/`. None changed but four, in a comment that names a path.
- **Which headers are for others was worked out, not chosen:** a header included from outside its module, or by such a header of its own module, went to the module's `include/`; the settings headers that third-party code finds by name (`lv_conf.h`, `FreeRTOSConfig.h`, `lwipopts.h`, `mbedtls_config.h`) with them. 51 of 54. Three are a module's own: `dnsserver.h`, `generic_scale.h`, `remote_control.h`.
- **The build:** the sources found two folders down (`src/*/*/*.c`), every module's `include/` on the one include path. No `#include` line changed. Gate 1's two modules are built from their new places, `platform/rp2/fault_handler.c` and `services/events/events.c`.
- **What names a path followed, in the same commit:** the build files and the flash layout's; `scripts/web_build.py`; `tests/host_test.py`, `step2_sim.py`, `calibration_sim.py` (their sources and include paths); `tests/release_records.py` (today's headers by their new paths, v1.21's by its flat `src/`, and its search for a stored struct not listed, two folders down); `tests/features_check.py`; `tools/size_report.py` (the pages' part); and 52 mentions of a moved file's path in AGENTS.md, the skills, the tools and comments. The ADRs' own records were left as they were written.
- **Left where they were:** `fonts/` and `scripts/`, which the PC's mock tools read too; `src/generated/`; the bootloader's folder.

The two scripts that did it are on this PC only (`build_adr006/move.py`, `repath.py`): they have run, and git holds what they did.

### The proofs, on the PC

Against gate 1's images (`build_adr006/after_w`, `after_2w`):

| | Pico W | Pico 2 W |
|---|---|---|
| Symbols | 4,688, `VERDICT PASS` | 4,693, `VERDICT PASS` |
| `app.bin` | 938,436 bytes, 120 more | 923,796 bytes, 96 more |
| Bootloader | byte for byte the same | none |

- **The bytes:** an image stores eight source paths, for its asserts. They are 111 bytes longer in all; the rest is padding.
- **Every compile command, before against after** (844 sources on the Pico W, 841 on the Pico 2 W): no source's definitions, options or third-party include paths changed. Only our own include paths did: `src` and `targets` gone, the modules' `include/` folders in their place.
- **The build's warnings** are main's own five kinds, none new.
- `tools/size_report.py`: the same parts (the Pico W 915.1 KB of its 1,016 KB slot), the pages still told apart at 41.6 KB.
- `tests/host_test.py`, `release_records.py` (every record still read against v1.21's), `features_check.py`, `calibration_sim.py`: `VERDICT PASS`.
- `tests/step2_sim.py`, the charge controller and the tuner in C beside the Python, built from their new folder: `VERDICT PASS` (it ended after the PR was opened).
- CI on the PR (#139), which builds both boards on Linux and runs the host test and the records check: passed.

### On the unit

2026-11-27, the bench's Pico W, the bench ready and the hopper empty (the owner). The images rebuilt from the commit: the unit runs 2.0.3-291 at bd00cf0.

- **`pulses`: `VERDICT PASS`**, the three faults; the cup 0.0 gr before and after.
- **`menu_tour --compare tests/menu_baseline/pico_w`: `VERDICT PASS`**, 14 pages the same and 3 not judged. The baseline's `settings/display` had been saved again first, at the owner's word (commit 4c75a82): its picture had the rotation the unit stored before ADR-008's gate 4, and failed every tour, on main's firmware too.
- **`charge_cycle`: `VERDICT PASS`**, 30.000 for a target of 30.000, 13.0 s from charging to the cup prompt.
- **The portal**, whose folder was renamed: `GET /` from the unit gives 42,286 bytes gzipped, the size the build made, 157,173 bytes of page.
- **The unit's settings** against the copy from before gate 1: 17 resources, the same.

The Pico 2 W waits, by the owner's word: the whole ADR on the Pico W first.

**Gate 2 is built.**

### For gate 3

- A module's `REQUIRES` can be read off the tree now: what its files include, by the folder each header lies in.
- The three headers that are a module's own will be refused to any other module from gate 3 on. Nothing else is refused until then.
- `boards/pico_w` does not exist yet: nothing is the Pico W's alone until a board has its list.

## Gate 3: the modules as libraries

2026-11-27, approved by the owner ("And continue with the gate"), after PR #139 merged gates 1 and 2. On the branch `adr006_gate3`. Commit dceafe6.

**The verdict: each of the 34 modules is built on its own, naming what it needs, and both boards' images are symbol for symbol what they were.** The compiler now refuses an include of anything a module does not name.

### What was built

- **A `CMakeLists.txt` in every module's folder**, one call each: `ot_module(SRCS ... INCLUDE_DIRS include REQUIRES ... PRIV_REQUIRES ...)`. What a module requires was read off its files, not chosen: `REQUIRES` is what its headers in `include/` include, `PRIV_REQUIRES` what only its sources do. Loops and all: nothing was cut to make it build.
- **The boards' files:** `src/boards/pico_w/board.cmake` and `pico2_w/board.cmake`, each the base build, whole; `boards/common/base.cmake` with what both share: the third-party code and the libraries the image links, the third-party names, the `-Os` rule, the base build's list of modules.
- **What the build makes went to the module that uses it:** the PIO headers to `platform/rp2`, the pages to `web/api`, the fonts to `ui/view_12864`, the roots to `services/update_fetch`, the version to `app/app`. `src/CMakeLists.txt` is gone. Nothing is found by a glob: a source is named in its module's `SRCS`.
- **`ot_module()`** takes its name from its folder, and a module with no sources is its headers only (`platform`, and the servo gate when a build leaves it out).
- **`tools/module_graph.py`** reads the modules' build files and prints what stands against the layers, ending in a verdict. Its counts are gate 6's and gate 7's meter.

### Found on the way

- **A module's library comes last on the link line, after an archive it needs.** The first link failed with every LVGL name undefined: LVGL is a real static library, the image named it before the modules, and the linker had passed it by the time a module asked. The other third-party code did not fail, since its sources are the image's own objects. So a module that requires `lvgl` links the real library, and CMake puts it after.
- **`rp2_sdk` was one library short:** `common.c` includes `hardware/dma.h`, which the image had through the network's libraries. `hardware_dma` is named now. It was the first build's only compile error.
- **Two folders had to be split, so a requirement would not hand over more than it says.** Requiring FreeRTOS brings `FreeRTOSConfig.h`'s folder, and requiring LVGL brings `lv_conf.h`'s. Where those lay (`boards/common`, the view's `include/`), a module requiring only FreeRTOS would have had the pin map, and one requiring only LVGL the view's own header. So the third-party settings are in `boards/common/settings`, and `lv_conf.h` in `ui/view_12864/lv_conf`. Five files moved; the gate's text said none would.
- **`app.h` gave fourteen files nothing.** It declares the start-up's two functions and includes `eeprom.h`. The ten scale makes, the two modes, the motors, the scale and the access point included it and used nothing of it. The ADR named two such includes (`eeprom.c`'s, and `scale.h`'s `app.h`); these are the same kind, and with them seven modules below `app` required `app`. Dropped; no file needed `eeprom.h` put in its place. No module requires `app` now.

### The modules, and what stands against the layers

`python tools/module_graph.py --modules --markdown`, at this gate:

34 modules:
- app/app: requires storage; its sources also activity api board charge events freertos jobs leds menu mini_12864 motors profile remote_control rp2_sdk scale servo_gate uc1701 update update_fetch wireless
- ui/menu: requires activity; its sources also api calibration charge cleanup events freertos http_server jobs lvgl mini_12864 motors profile rp2_sdk scale servo_gate settings storage system uc1701 update update_fetch version view_12864 wireless
- ui/mini_12864: requires nothing; its sources also board freertos rp2 rp2_sdk settings storage uc1701
- ui/view_12864: requires lvgl menu
- web/api: requires nothing; its sources also activity calibration charge cleanup events freertos http_json http_server leds lwip menu mini_12864 motors profile rp2_sdk scale servo_gate settings storage system uc1701 update update_fetch version wireless
- web/http_server: requires nothing; its sources also freertos lwip rp2_sdk
- console/remote_control: requires nothing; its sources also activity calibration charge freertos http_server lwip menu mini_12864 motors profile rp2_sdk scale settings storage uc1701 update version wireless
- services/activity: requires nothing; its sources also freertos
- services/calibration: requires activity calibration_method; its sources also charge events freertos jobs menu motors profile rp2_sdk scale settings
- services/calibration_method: requires nothing
- services/charge: requires activity events leds storage; its sources also charge_control freertos jobs menu mini_12864 motors profile ring rp2_sdk scale servo_gate settings uc1701
- services/charge_control: requires nothing
- services/cleanup: requires activity motors; its sources also charge events freertos jobs menu mini_12864 scale servo_gate storage uc1701
- services/events: requires nothing; its sources also activity freertos http_json rp2_sdk update version
- services/jobs: requires activity; its sources also calibration charge cleanup freertos menu update
- services/leds: requires freertos storage; its sources also activity board rp2 settings
- services/motors: requires freertos rp2 storage; its sources also board events rp2_sdk settings tmc2209
- services/profile: requires nothing; its sources also activity settings storage
- services/scale: requires freertos; its sources also board charge motors rp2_sdk settings storage
- services/servo_gate: requires freertos; its sources also board rp2_sdk settings storage
- services/settings: requires activity; its sources also api events http_json http_server storage version wireless
- services/storage: requires freertos rp2_sdk; its sources also system
- services/system: requires nothing; its sources also http_server rp2_sdk storage version
- services/update: requires activity; its sources also api events freertos http_json http_server jobs mbedtls menu mini_12864 platform rp2 rp2_sdk storage uc1701 update_fetch version wireless
- services/update_fetch: requires nothing; its sources also activity events freertos lwip mbedtls rp2_sdk settings storage update version wireless
- services/wireless: requires activity; its sources also api events freertos http_server lwip mbedtls mini_12864 rp2 rp2_sdk settings storage uc1701 update update_fetch version
- drivers/cat24c256: requires nothing; its sources also board freertos rp2_sdk storage
- drivers/scale_protocols: requires nothing; its sources also board freertos rp2_sdk scale
- drivers/tmc2209: requires nothing
- drivers/uc1701: requires nothing; its sources also board freertos rp2_sdk storage
- common/http_json: requires nothing
- common/ring: requires nothing
- platform/platform: requires nothing
- platform/rp2: requires lwip rp2_sdk; its sources also board freertos motors platform
25 requirements go up, against the layers:
- web/api requires ui/menu (its sources)
- web/api requires ui/mini_12864 (its sources)
- console/remote_control requires web/http_server (its sources)
- console/remote_control requires ui/menu (its sources)
- console/remote_control requires ui/mini_12864 (its sources)
- services/calibration requires ui/menu (its sources)
- services/charge requires ui/menu (its sources)
- services/charge requires ui/mini_12864 (its sources)
- services/cleanup requires ui/menu (its sources)
- services/cleanup requires ui/mini_12864 (its sources)
- services/jobs requires ui/menu (its sources)
- services/settings requires web/api (its sources)
- services/settings requires web/http_server (its sources)
- services/system requires web/http_server (its sources)
- services/update requires web/api (its sources)
- services/update requires web/http_server (its sources)
- services/update requires ui/menu (its sources)
- services/update requires ui/mini_12864 (its sources)
- services/wireless requires web/api (its sources)
- services/wireless requires web/http_server (its sources)
- services/wireless requires ui/mini_12864 (its sources)
- drivers/cat24c256 requires services/storage (its sources)
- drivers/scale_protocols requires services/scale (its sources)
- drivers/uc1701 requires services/storage (its sources)
- platform/rp2 requires services/motors (its sources)
2 loops:
- 19 modules: api calibration charge cleanup events jobs leds menu mini_12864 motors profile rp2 scale servo_gate settings update update_fetch view_12864 wireless
- 2 modules: storage system
20 modules besides platform/rp2 require the Pico SDK itself:
- app/app console/remote_control drivers/cat24c256 drivers/scale_protocols drivers/uc1701 services/calibration services/charge services/events services/motors services/scale services/servo_gate services/storage services/system services/update services/update_fetch services/wireless ui/menu ui/mini_12864 web/api web/http_server

The tool ends `VERDICT FAIL 25 requirements go up, 21 modules in 2 loops, 20 modules on the Pico SDK`. That is this gate's finding, not its failure: gate 6 brings the first two to none, gate 7 the third.

What it says, read plainly:
- **One loop holds 19 of the 34 modules.** The menu is in it with the jobs, the web's API, the update and the Wi-Fi, so today none of them can be taken out of a build alone.
- **Five modules are free of everything:** `tmc2209`, `charge_control`, `calibration_method`, `http_json` and `ring` require nothing, no RTOS and no chip. The compiler says so now, not a reading of the code.
- **13 of the 25 upward requirements are into `ui/`, and 8 into `web/`**: the screen named from below, which is what stands between today's tree and another screen.
- **A requirement the build files cannot show:** a file that declares another module's variable `extern` itself, without the header, needs that module and names nothing. Gate 6 turns each into a call, and the requirement appears.

### The proofs, on the PC

Against gate 2's images (`build_adr006/move_w`, `move_2w`):

| | Pico W | Pico 2 W |
|---|---|---|
| Symbols | 4,688, `VERDICT PASS` | 4,693, `VERDICT PASS` |
| `app.bin` | 938,452 bytes, 16 more | 923,796 bytes, the same |
| Bootloader | byte for byte the same | none |
| `isr_hardfault` from | `libot_rp2.a` | `libot_rp2.a` |

- **Every compile command, before against after:** no source's definitions or options changed, of 844 and 841; no third-party source's compile changed at all. 63 of our own sources now see fewer third-party include paths than they did.
- **What a module may include, tried twice and not kept:** `tmc2209.c`, a driver that requires nothing, made to include `motors.h`: `fatal error: motors.h: No such file or directory`. `http_json.c`, a helper, made to include `lwip/sockets.h`: refused the same way.
- **A build without the servo gate** (`-DFEATURE_SERVO_GATE=OFF`), against the same built from main: 4,655 symbols, `VERDICT PASS`.
- **The other builds:** the two fault test images; a whole release, its five builds, the hard fault's handler from the module in the update images.
- `tools/size_report.py`: the same parts to the tenth of a KB (the Pico W 915.1 KB of its 1,016 KB slot), the pages still told apart inside the API's library.
- `tests/host_test.py`, `release_records.py`, `features_check.py`: `VERDICT PASS`.

### On the unit

2026-11-27, the bench's Pico W, the bench ready and the hopper empty (the owner). The images rebuilt from the commit: the unit runs 2.0.3-296 at dceafe6.

- **`pulses`: `VERDICT PASS`**, the three faults; the cup 0.0 gr before and after.
- **`menu_tour --compare tests/menu_baseline/pico_w`: `VERDICT PASS`**, 14 pages the same and 3 not judged.
- **`charge_cycle`: `VERDICT PASS`**, 30.000 for a target of 30.000, 12.9 s from charging to the cup prompt.
- **The portal:** `GET /` gives 42,286 bytes gzipped, the size the build made.
- **The unit's settings** against the copy from before gate 1: 17 resources, the same.

The Pico 2 W waits, by the owner's word.

**Gate 3 is built.**

### For gate 4

- A module's build file is data now: the PC build can take the five free modules, and `storage`'s loader, by the same call with another `ot_module()`.
- `storage` requires FreeRTOS and the Pico SDK through `common.h`, which every user of a stored record includes. The host test stands in for both today.

## Gate 4: the PC build

2026-11-27, approved by the owner ("Gate 4 approved. please continue."), after PR #140 merged gate 3. On the branch `adr006_gate4`. No firmware source and no firmware build file changed: this gate needs no unit.

**The verdict: the three host tests are built from the modules' own build files, on Windows and on Linux, and a module free of the chip that names an SDK header no longer builds there.**

### What was built

- **`tests/host/CMakeLists.txt`, the PC build.** It gives `ot_module()` for the PC (a static library by the PC's compiler, some twenty lines), then adds the modules' folders as they are: the same build files the firmware reads.
- **Four modules whole:** `http_json`, `tmc2209`, `calibration_method`, `charge_control`. They require nothing, so they get nothing: not even the stand-ins are on their include path.
- **The stand-ins are the PC's `freertos` and `rp2_sdk`**, the names a module requires: `tests/host/stand_ins/freertos` and `stand_ins/rp2_sdk` (the old `stubs/`, sorted by what each stands for).
- **The tests are its targets:** `config_migration_test` (a program), `charge_lib` and `calibration_lib` (libraries Python loads). Each names the modules it links.
- **`tests/host_test.py`** configures and builds it into `build_host/` (`host_build`), then runs each test. `step2_sim.py` and `calibration_sim.py` ask it for their library. The three lists by hand (sources, and every `include/` folder under `src/` by a glob) are gone.

### Found on the way

- **The loader's test cannot take its modules whole, and the gate's text had not said so.** It needs `common.c` of `storage` and `charge_mode_layout.c` of `charge`, and neither module is free of the chip: `storage`'s other source needs `system`, `charge`'s the motors, the menu and the scale. So the PC build takes a module **in part**: its headers and what they require from its own build file, and of its sources only those the PC build names. Six modules today: `storage` and `charge` with a source each; `profile` for its header, which the test reads; `activity`, `events` and `leds` because the charge record's header includes theirs. Two source names by hand are what is left of the old lists.
- **That list is a second meter, beside the stand-ins.** A part is a module the cut has not reached. Gates 6 to 8 shorten it: the loader in a module of its own would make `storage`'s part a whole module.
- **`zig cc` has no archiver CMake knows by name:** the PC build gives the archive rule as `zig ar`. CMake takes the two-word compiler only from `CC`, which `host_build` sets.
- **`common/ring` is the fifth free module and is not in the PC build:** it is C++, and the PC build is C only. It joins when gate 5 makes it C.
- **The PC's compilers warn of four `printf` formats in `common.c`** that Arm's does not (`%d` for a `size_t`, `%lX` for a `uint32_t`: right on the chip, where both are 32 bits, wrong on a 64-bit PC). They print in error paths only. Left for gate 10's small fixes; the PC build does not fail on a warning there, only in `charge_control`, where the simulation always has.
- **CI's host test now needs CMake and Ninja.** It takes the toolchain's own from `.toolchain`, fetched before that step; nothing is installed on the runner.

### The meter

| | Gate 4 | At the ADR's end |
|---|---|---|
| Stand-in headers, FreeRTOS's | 3 (`FreeRTOS.h`, `task.h`, `semphr.h`) | these |
| Stand-in headers, the Pico SDK's | 3 (`hardware/pio.h`, `hardware/dma.h`, `pico/time.h`) | none |
| Modules built whole | 4 | every module meant to be free |
| Modules taken in part | 6 (2 with a source) | none |

The three SDK headers are all `storage`'s: `common.h` names the PIO's type, `common.c` includes the other two.

### The proofs

- **Windows, `zig cc` (Clang 18.1.6), CMake 4.3.4 and Ninja 1.13.2 from `toolchain.env`:**
  - `tests/host_test.py`: `VERDICT PASS`, from an empty `build_host/`.
  - `tests/calibration_sim.py`: `VERDICT PASS`.
  - `tests/step2_sim.py`: `VERDICT PASS`: the C asks the same speeds and takes the same tuner steps as the Python, on every rig.
- **Linux, gcc 15.2 (WSL's Ubuntu), the same project:** it configures and builds; `config_migration_test` passes (`PASS: 0 failed`); Python loads both libraries and finds their entry points. `charge_control` builds with no warning under `-Wextra -Werror` there too.
- **A free module made to include what it does not name, tried twice and not kept:**
  - `tmc2209.c` with `#include "pico/time.h"`: `fatal error: 'pico/time.h' file not found`. The stand-in of that name exists, and the module does not get it.
  - `http_json.c` with `#include "FreeRTOS.h"`: refused the same way.
- **The firmware's build is as it was:** nothing under `src/` changed, and of `cmake/ot_module.cmake` one comment.

- **CI, on PR #141** (the runner's gcc, the toolchain's CMake and Ninja from `.toolchain`): the host test passed, and the release's five builds with it.

**Gate 4 is built**, and merged by #141.

### For gate 5

- `ring` in C joins `HOST_MODULES`.

## Gate 5: C only

2026-11-27, approved by the owner ("Approve gate 5"), after PR #141 merged gate 4. On the branch `adr006_gate5`. Commit cf8d4b2.

**The verdict: the two modes and the ring buffer are C, every function of the two modes calls and touches what it did, and on the Pico W a charge and a purge run as before.** One line would have changed its meaning in C with no error, and was written to keep it.

### What was built

- **`cleanup_mode.cpp` and `charge_mode.cpp` are `.c`.**
- **`FloatRingBuffer`, the one class, is `float_ring`** (`src/common/ring`): a struct and five functions (`init`, `add`, `count`, `mean`, `sd`) over an array its user gives. The charge's three rings have their arrays on the stack, 20 or 40 bytes. No heap: the class took its array by a `malloc` it never checked.
- **The ring is in the PC build**, whole: the fifth free module.
- The C++ left is `mini_12864_module.cpp` and the interrupt class in `gpio_irq_handler.h`, for gate 7.

### Found on the way

- **`abs()` of a float means another thing in C, and the compiler says nothing.** The zero test asks `abs(mean) < margin`. In C++ that was `std::abs(float)` (read off the old object file: its one call). In C, `abs` is the integer's: the mean would be cut to a whole number first, and any mean under 1 would pass as zero. It is `fabsf` now. The ADR said `charge_mode.cpp` "uses only `FloatRingBuffer`" of C++; it also used this overload.
- **The cup-return wait made a ring it never used:** a `malloc` and a `free` at each cup's return, for nothing. Gone.
- **Of the class's 17 methods the charge used four.** The rest (the lock, the overflow flag, `dequeue`, `first`, `last`, the index) are not carried over.
- **A function's size is not the measure the gate's text took it for.** 19 functions changed size, by a few bytes each way, with nothing changed in their lines: the C and the C++ front ends pick other instructions at `-O0` (a `bool` tested another way, a `? :` laid out otherwise). So the comparison is of what each function calls and what data it touches, counted by name; the sizes are listed and their kinds named.
- **The Pico W's menu tour is run with the screen's inversion off live** (the handover says so; `AGENTS.md` said "stored upside down", which is no longer so). Run without, every judged page fails as the exact inverse of its picture. That cost a look into the menu's code before the handover was read again; `AGENTS.md` now says it at the tour.

### The proofs, on the PC

**The ring, checked once against the class** (`zig`, the old class from git beside the new C, the same readings into both: a scale's level and noise, with jumps, in rings of 5 and of 10): 214,000 means and standard deviations compared bit for bit, 0 differ; `VERDICT PASS`. Not kept as a test: the class is gone.

**What each function calls and touches**, the old object file against the new, both boards the same:

| | `charge_mode` | `cleanup_mode` |
|---|---|---|
| Functions before | 36 (3 of them C++'s own: `std::abs`, `std::isnan`, `std::isfinite`) | 8 (1: `std::isfinite`) |
| Functions after | 33 | 7 |
| The same names, the same number of times | 26 | 5 |
| Differ, each accounted for below | 7 | 2 |

- **The ring's four users** (`wait_for_zero`, `pause`, `wait_for_cup_removal`, `wait_for_cup_return`): the class's calls for the ring's, one for one; the constructor and destructor gone; the last of the four calls nothing of it any more.
- **`wait_for_zero`:** `std::abs(float)` gone, `fabsf` in its place, which the compiler does in place (the sign bit cleared). No conversion to an integer in the function.
- **`isnan` and `isfinite`** (`request_start`, `target_fits_at`; `cleanup_mode_limit_speed`, `cleanup_mode_set_speed`): C++ called its own small functions for them; C's are macros that compare in place. On the RP2040 that is the float library's compare (`__aeabi_fcmpun`, `__aeabi_fcmpgt`); on the RP2350 the FPU's own instructions. The same question asked of the same number.
- **`wait_for_complete`:** one read fewer of `charge_heap_low`, in `low = free < low ? free : low`. C reads each side of a minimum once; C++ read the variable again for the result. Nothing else touches it between.
- **The data the two files start with:** 16 objects in `charge_mode`, none in `cleanup_mode`; the bytes the same but for one table of ten strings' addresses, whose strings and order are the same (the spacing between its entries, entry by entry). Every string constant the same.

**The whole image, by name and size:**

| | Pico W | Pico 2 W |
|---|---|---|
| Names before, after | 4,441, 4,438 | 4,449, 4,446 |
| Gone | the class's 9 functions; a table renamed | the same |
| New | the ring's 6 functions; the table | the same |
| Of another size | 19, all in the two modes | 19, the same |
| `app.bin` | 938,452 to 937,964 bytes (488 fewer) | 923,788 to 923,268 (520 fewer) |

Nothing outside the two modes and the ring changed its name or size. `tools/size_report.py`: the Pico W's flash 915.1 KB to 914.7 KB of its 1,016 KB slot; RAM at link time 252.1 KB, the same.

- `tests/host_test.py` (the ring in the PC build), `release_records.py`, `features_check.py`: `VERDICT PASS`.

### On the unit

2026-11-27, the bench's Pico W, the bench ready and the hopper empty (the owner, asked again before the first contact). Before: 2.0.3-296 (`dceafe6`), as it had run since gate 3. After: 2.0.3-305 (`cf8d4b2`), flashed by the probe.

| After a restart and one `charge_cycle` | Before | After |
|---|---|---|
| The charge | 30.000 for 30.000, 13.0 s | 30.000 for 30.000, 12.9 s |
| The job task's stack never touched | 2,984 bytes | 2,968 bytes |
| The heap's low mark (`heap_min`) | 39,816 bytes | 39,816 bytes |
| The charge's own heap low mark | 40,624 bytes | 40,624 bytes |

- **The stack: 16 bytes more used**, the rings' arrays now on it. 2,968 bytes are left.
- **`charge_cycle`: `VERDICT PASS`**, the cup 0.0 gr after.
- **`menu_tour --compare tests/menu_baseline/pico_w`: `VERDICT PASS`**, with the inversion off live, then reverted.
- **`api_check`: `VERDICT PASS`** (the local OTA server up for it, without its name, and stopped after). In it the purge: started by the API, its speed and time set, stopped by the unit at its time; started again and stopped by the API. And a charge started and stopped the same way.
- **The unit's settings** against the copy from before gate 1, after the flash and after the check's restart: 17 resources, the same.

The Pico 2 W waits, by the owner's word; its image builds, and compares as the Pico W's does.

**Gate 5 is built.**

### For gate 6

- Our code is C but for the display's module, so every module but `mini_12864` can be built by a C compiler alone.
- A file moved from C++ to C is judged by what each function calls, not by its size: `build_adr006/refs_compare.py` is the scratch script that did it here.
