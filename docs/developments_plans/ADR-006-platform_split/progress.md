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

**Gate 5 is built**, and merged by #142 (the owner, after his review).

### For gate 6

- Our code is C but for the display's module, so every module but `mini_12864` can be built by a C compiler alone.
- A file moved from C++ to C is judged by what each function calls, not by its size: `build_adr006/refs_compare.py` is the scratch script that did it here.

## Gate 6: the loops cut

2026-11-27, approved by the owner ("Approve gate 6"), after PR #142 merged gate 5. On the branch `adr006_gate6`, a commit a cut, b12c808 to 8afa8cb.

**The verdict: no module requires a module of a layer above it, and no modules require each other.** `tools/module_graph.py` counted 25 upward requirements and 21 modules in 2 loops at gate 3; it counts none of either now, and the compiler refuses their return. On the Pico W every path a cut touched runs as before, an update over the air among them.

### The gate was larger than its text

The gate named seven cuts. Read off the built libraries (which names each module takes from each other), the graph held more, and the end condition needs them all:
- **the console drove the menu** (`press`, `screen`, `snap`, `knob`: 8 of the menu's functions and the knob's queue), and read the web server for `net`;
- **the web's API copied the menu's frame** and asked it for the hello;
- **three drivers reached up into services:** the scale's ten makes published into the scale; the EEPROM's driver was declared by the storage it serves, the wrong way round; the display's driver took a delay from the storage's helpers;
- **the hard fault's handler**, in the platform, called the motors;
- **loops among the services:** the event log and the update; the job task and the four jobs; the settings and the Wi-Fi; the Wi-Fi and the update; the update and its fetch; the scale and the charge; storage and the restart.

### The owner's decision: the UI hands its parts down

The ADR put `ui/` above `web/` and `console/`, and also had the web's mirror and the console's `snap` "ask the view". One had to give. Asked (2026-11-27): **`ui/` stays on top, and hands its parts to what is below it.** The console's four screen commands are the UI's, registered with the console at start-up; the API is handed the frame and the hello. Neither the web nor the console names a screen, so another UI, or none, needs no edit in them.

### How a cut is made: handed over at start-up

Every cut is the same move. Where a lower module called a higher one, the higher now hands the lower what it needs, once, at start-up, and the lower calls what it was handed:

| The lower module | What it is handed | By |
|---|---|---|
| `activity` | the screen's wake, and whether it takes a start | the menu |
| `jobs` | each job's run | `app` |
| the console | a command of another module's | the menu (`press`, `screen`, `snap`, `knob`), the web server (`net`) |
| the API | the screen's frame and its hello | the menu |
| the menu | its view | `app` (the 12864's) |
| the event log | the update's two texts for its own events | the update |
| the update | its fetch: the poll's find, a GET, the console's `check` and `poll` | `update_fetch` |
| the Wi-Fi | what follows the network: the web started, the update told, the poll told | `app` |
| the restart | what ends before it | the web (its clients closed) |
| the settings' schema and export | the commands' share, the unit's id | the API |
| the scale | its makes, by their stored numbers; where a throw's target is read | `app` |
| a make | where its readings go | the scale |
| the hard fault's handler | the stop it makes first | the motors |

Where nothing is handed over, the lower module does nothing and says so in its reply: a build without a screen answers the display's routes 404; one without a fetch says "this build updates by file only".

### The cuts

1. **The jobs and the menu** (b12c808). A job asked `menu_get_current_form_id() < 0` before a start and called `menu_wake()` after. Both go through `activity` now, which already held the start's request. The job task's `switch` over the four jobs is a table `app` fills.
2. **The screen named from below**, in four commits:
   - the display's two headers out of the charge, the cleanup, the update, the Wi-Fi's two files and the pages' file. None used anything of them; the two modes also declared the knob's queue and never touched it;
   - the console's screen commands to `ui/menu/menu_console.c`, `net` to `web/http_server/http_server_console.c`, by `remote_control_register()`;
   - the API's frame and hello by `api_v1_set_screen()`;
   - **the 12864's frame to its view.** LVGL, the one frame, its lock and the pages sent to the panel were in the menu's task; they are `view_12864`'s. The menu draws through `menu_view_t` (`menu_view.h`), handed by `app`, and names no view, no panel and no LVGL.
3. **The update** (f43bb27). The event log wrote an update's version and reset by calling the update: the update hands it those two texts. And the update named its fetch while its fetch named it: the update now owns a small interface (`update_fetcher_t`) that `update_fetch` fills, so `update` builds without it, as ADR-005's fallback wants.
4. **The web started by the Wi-Fi** (03ae8b4): `app` hands the Wi-Fi a listener, and starts the pages and the server from it.
5. **The restart** (b875396): `system_before_restart()` is the list; the web joins with the closing of its clients. The board's name, which the update took from the API, is `system_board_name()`.
6. **The settings and the web** (8b75ab7). The settings keep their tables, checks, stores and JSON, and answer the API in a buffer it gives (`settings_answer_t`); the routes and the server's calls are `web/api/api_v1_settings.c`. 745 lines did not have to move: each handler lost its two HTTP types and kept its body.
7. **State by `extern`**, in two commits:
   - `scale_config`, `charge_mode_config` and `neopixel_led_config` behind five read-only calls. The ADR counted 16 files naming the scale's state; ten of them, the makes, declared it and used none of it;
   - **the makes handed to the scale** (`scale_make.h`): a make gives its readings to a host it is handed, and `app` hands each make to the scale by its stored number. A stored make the build does not carry is read as the A&D's, as an unknown number always was.
8. **The drivers and the platform** (a16cc45): the EEPROM's driver has a header of its own, and storage requires it; the display's reset pulse waits by itself; the hard fault's handler stops what the motors handed it.

### Found on the way

- **A build's block version stops counting at 255 commits past its tag.** 2.0.3-317 and 2.0.3-318 both carry block 2.0.3-255, so the poll never finds a later bench build newer. A release is a tag, so no user meets it. The over-the-air test used a test block version (`-DRC_TEST_BLOCK_VERSION`). Not changed.
- **Storage's `eeprom.h` had the driver's include guard** (`CAT24C256_EEPROM_H_`), from when it was the driver's header. The driver's new header took the name, and storage's vanished from its own source until its guard was its own.
- **The delay helper in storage had one user, the display.** With it gone the loader's file loses three includes, and the PC build two of its three SDK stand-ins.
- **The Pico W's RAM at link time is the number to watch.** The first build of cut 3 kept the download's three buffers static: 584 bytes, 252.1 KB to 253.0 KB of 256. They are on the stack again (8afa8cb), as they were.
- **`rc_run.py` finds the console by the ELF it is given.** An ELF rebuilt since the flash put the first over-the-air test's console commands nowhere; the test now goes by the API.
- **Two variables are still reached by `extern`, both inside `ui/`:** the knob's queue and the display's settings, between the menu and `mini_12864`. They go with gate 7's inputs.

### The modules now

`python tools/module_graph.py --modules --markdown`:

34 modules:
- app/app: requires storage; its sources also activity api board calibration charge cleanup events freertos http_server jobs leds menu mini_12864 motors profile remote_control rp2_sdk scale servo_gate uc1701 update update_fetch view_12864 wireless
- ui/menu: requires activity; its sources also api calibration charge cleanup events freertos http_server jobs mini_12864 motors profile remote_control rp2_sdk scale servo_gate settings storage system update update_fetch version wireless
- ui/mini_12864: requires nothing; its sources also board freertos rp2 rp2_sdk settings storage uc1701
- ui/view_12864: requires menu; its sources also freertos lvgl rp2_sdk uc1701
- web/api: requires nothing; its sources also activity calibration charge cleanup events freertos http_json http_server leds lwip motors profile rp2_sdk scale servo_gate settings storage system update update_fetch version wireless
- web/http_server: requires nothing; its sources also freertos lwip remote_control rp2_sdk
- console/remote_control: requires nothing; its sources also activity calibration charge freertos motors profile rp2_sdk scale settings storage system update version wireless
- services/activity: requires nothing; its sources also freertos
- services/calibration: requires activity calibration_method; its sources also charge events freertos jobs motors profile rp2_sdk scale settings
- services/calibration_method: requires nothing
- services/charge: requires activity events leds storage; its sources also charge_control freertos jobs motors profile ring rp2_sdk scale servo_gate settings
- services/charge_control: requires nothing
- services/cleanup: requires activity motors; its sources also charge events freertos jobs scale servo_gate storage
- services/events: requires nothing; its sources also activity freertos http_json rp2_sdk version
- services/jobs: requires activity; its sources also freertos
- services/leds: requires freertos storage; its sources also activity board rp2 settings
- services/motors: requires freertos rp2 storage; its sources also board events rp2_sdk settings tmc2209
- services/profile: requires nothing; its sources also activity settings storage
- services/scale: requires freertos scale_protocols; its sources also board motors rp2_sdk settings storage
- services/servo_gate: requires freertos; its sources also board rp2_sdk settings storage
- services/settings: requires activity; its sources also events http_json storage version
- services/storage: requires freertos rp2_sdk; its sources also cat24c256 system
- services/system: requires nothing; its sources also freertos rp2_sdk
- services/update: requires activity; its sources also events freertos http_json jobs mbedtls platform rp2 rp2_sdk storage system version wireless
- services/update_fetch: requires update; its sources also activity events freertos lwip mbedtls rp2_sdk settings storage version wireless
- services/wireless: requires activity; its sources also events freertos lwip mbedtls rp2 rp2_sdk settings storage version
- drivers/cat24c256: requires nothing; its sources also board freertos rp2_sdk
- drivers/scale_protocols: requires nothing; its sources also board freertos rp2_sdk
- drivers/tmc2209: requires nothing
- drivers/uc1701: requires nothing; its sources also board freertos rp2_sdk
- common/http_json: requires nothing
- common/ring: requires nothing
- platform/platform: requires nothing
- platform/rp2: requires lwip rp2_sdk; its sources also board freertos platform
0 requirements go up, against the layers:
0 loops:
21 modules besides platform/rp2 require the Pico SDK itself:
- app/app console/remote_control drivers/cat24c256 drivers/scale_protocols drivers/uc1701 services/calibration services/charge services/events services/motors services/scale services/servo_gate services/storage services/system services/update services/update_fetch services/wireless ui/menu ui/mini_12864 ui/view_12864 web/api web/http_server
VERDICT FAIL 0 requirements go up, 0 modules in 0 loops, 21 modules on the Pico SDK

It ends `VERDICT FAIL 0 requirements go up, 0 modules in 0 loops, 21 modules on the Pico SDK`: the last count is gate 7's. It was 20 at gate 3: the view now asks the time itself, which the menu did for it.

### The meter of the PC build

| | Gate 4 | Now |
|---|---|---|
| Stand-in headers, FreeRTOS's | 3 | 3 |
| Stand-in headers, the Pico SDK's | 3 | 1 (`hardware/pio.h`, for a type `common.h` names) |
| Modules built whole | 4 | 5 |
| Modules taken in part | 6 | 6 |

### The proofs, on the PC

- Both boards build at every commit. `app.bin`: the Pico W 937,964 to 941,916 bytes (3,952 more), the Pico 2 W 923,268 to 926,892 (3,624 more): the tables and the calls through them.
- `tools/size_report.py`, the Pico W: flash 914.7 KB to 918.5 KB of its 1,016 KB slot; **RAM at link time 252.1 KB to 252.4 KB of 256 KB**.
- A build without the servo gate (`-DFEATURE_SERVO_GATE=OFF`), the two fault test images, and a whole release (its five builds, at cut 6) build.
- `tests/host_test.py`, `release_records.py`, `features_check.py`: `VERDICT PASS`.

### On the unit

2026-11-27, the bench's Pico W, the bench ready and the hopper empty (the owner, asked before gate 5's runs the same afternoon). Each cut was flashed and its path run before the next; every run below ended `VERDICT PASS`.

| After | Image | Run |
|---|---|---|
| cut 1 | 2.0.3-310 | `charge_cycle` (13.0 s), `reach`, `api_check`, `menu_tour`, the settings the same |
| cuts 2b, 2c | 2.0.3-313 | `reach`, `menu_tour`, `api_check`; `help` lists the handed commands; `net` and `knob` answer |
| cut 2d | 2.0.3-314 | `menu_tour`, `reach`, `charge_cycle` (12.9 s), `api_check` |
| cuts 3 to 5 | 2.0.3-317 | `api_check`, `charge_cycle` (13.0 s), `menu_tour`, the settings the same; `update` and `update poll` |
| cut 6 | 2.0.3-318 | installed over the air (below); `api_check`, `settings_roundtrip`, the settings the same, `charge_cycle` (13.1 s), `menu_tour` |
| all | 2.0.3-321 | `menu_tour`, `charge_cycle` (12.9 s), `api_check`, `settings_roundtrip`, `reach`, `pulses`, `scale_frames`, the settings the same |
| all | 2.0.3-322 | over the air again; then `menu_tour`, `charge_cycle` (13.1 s), the settings the same |

- **An update over the air, twice** (the local OTA server, without its name; the test release taken out of the served folder after). The unit polled, found the release, fetched its 941,068 bytes itself through the handed-over fetch, installed it, restarted into the other slot and confirmed once on its network. The second time, on the last image, the start's own poll had found the release before it was asked: the Wi-Fi's listener at work.
- **`pulses`:** the step pulses stopped on a reset through the probe, on an assert and on a hard fault. The last is the handler stopping what the motors handed it.
- **`scale_frames`** (5 s idle, two hand-overs to the simulated scale and back, a restart; no motor turned): 105, 168 and 116 frames from the A&D, every one read as a number, through the make the wiring handed the scale. Its default run reckons 5,460 gr for its motors' part and the cup check refuses it.
- **The menu's frame times** across cut 2d, after a tour: drawn in 41 ms before and 33 ms after (65 and 57 at most), sent in 5.1 and 4.9 ms. LVGL's heap the same (12,424 bytes at most, 2,416 free in one piece at the least).
- **Stacks never touched:** the job task's 2,968 bytes at gate 5 and 2,968 at the end; the scale's 736 and 736; the menu's 1,392 before the frame's move and 1,352 after it, each after a tour (one call deeper to draw). The heap's low mark 39,816 at gate 5 and 40,624 at the end.

The unit is left on 2.0.3-322 (`8afa8cb`), slot A, its settings as before gate 1. The Pico 2 W waits, by the owner's word; its image builds at every commit.

**Gate 6 is built**, and merged by #143 (the owner).

### For gate 7

- The 21 modules on the Pico SDK are its list. `ui/menu` is on it for the time alone.
- A hand-over at start-up is how a platform interface reaches a module too: the hard fault's stop is the first one made.
- The makes read the serial port by the SDK's own calls: they are the first drivers to move to the platform's serial port.

## Gate 7: the platform's interfaces

2026-11-27, approved by the owner ("PR merged, continue with the next gate"), after PR #143 merged gate 6. On the branch `adr006_gate7`, a commit an interface, a34d94f to d5ab3b8.

**The verdict: no file outside `platform/` and `boards/` includes the Pico SDK or the radio's driver, and only `platform/rp2` requires the SDK.** `tools/module_graph.py` ends `VERDICT PASS`: 21 modules on the SDK at gate 6, none now, and the search for an SDK include, which the tool now makes itself, finds none. The PC build's stand-ins are FreeRTOS's alone. On the Pico W every path an interface touched runs as before.

### The owner's pick: buses

The gate's list had storage as blocks, the display's bus with its three pins inside and the inputs as events; the ADR's drivers' line had the drivers above the platform, on its serial port, SPI and I2C, and the motors' enable and direction pins fitted nothing on the list. Asked before cut 1 (2026-11-27): **buses.** GPIO (outputs, inputs, edges), I2C, SPI and a serial port, beside the step pulses, the image slots, the system, a task, an LED strip, a servo and the network. The CAT24C256, the UC1701, the TMC2209 and the scale's makes stay drivers above them, written once for every chip.

### The interfaces

Each a header in `src/platform/include`, its rules written in it, given for the RP chips by a file in `src/platform/rp2`. The new ones are named `chip_`: no header or call of ESP-IDF's or the Pico SDK's is so named (the paper check, below), where `spi_bus_` is ESP-IDF's and `led_strip_` its LED component's.

| Cut | Header | What it is | From |
|---|---|---|---|
| 1 | `step_pulses.h` | the step pulses, lifted as they were | `platform/rp2/include` |
| 2 | `update_slots.h` | the slots as they were, and what an image says of itself (as it comes, and whole), its hash, a piece written into the free slot, the trial's watchdog, the running version | `update.c`'s picobin, flash and watchdog calls |
| 3 | `chip.h`, `fault_handler.h` | the time, a busy wait, the restart, the chip's id, a random number, the console; the fault's hook lifted as it was | the timer, watchdog, unique id, random and stdio calls of 19 files |
| 4 | `rtos_port.h` | a task's start with its stack in bytes; a critical section, from a task or (cut 9) an interrupt | the 15 `xTaskCreate` and the 12 `taskENTER_CRITICAL` |
| 5, 9 | `chip_gpio.h` | outputs; inputs and their edges, each pin's own call | the motors', the display's and the knob's pins; the C++ interrupt class |
| 6 | `chip_i2c.h` | a bus by its number; a write, and a write then a read with no stop between | the CAT24C256's |
| 7 | `chip_serial.h` | a port by its number, its format and rate; a byte or none, a byte within a time, a write | the scale's, its ten makes' and the motor drivers' |
| 8 | `chip_spi.h` | a bus written only, back once the last bit is out | the UC1701's |
| 10 | `chip_leds.h` | a WS2812 strip a number, each LED's bits | the LEDs' PIO |
| 11 | `chip_servo.h` | two pulses at 50 Hz on one timer, both widths at once | the servo gate's PWM slice |
| 12 | `chip_net.h` | the station's join, leave, link, addresses, signal and scan; the hotspot with its DHCP and DNS servers; mDNS's name and services; the radio's LED | `wireless.c`'s and `access_point_mode.c`'s cyw43 and raw lwIP |
| 13 | (the pin map) | the ports plain numbers; the board's target carries nothing of the SDK; the module graph's search | `configuration.h` and `base.cmake` |

### Where it went past the gate's text

- **The image slots grew.** "`update_slots.h`, as it is" left `update.c` on the flash, the image's format and the watchdog. They are the slots' now, which is how ESP-IDF's update calls group them too. The hash's and the board's readers moved as they were (the image's symbols: the same sizes).
- **The console** was on no interface: `chip.h` has its start, its read within a time and its room (RTT drops a line that does not fit).
- **A critical section** joined the task's call: the paper check found ESP-IDF's takes a lock, so our 12 calls with no argument would not compile there. The knob's tap took its own lock from the SDK; it is the RTOS's section now, from the interrupt as from a task.
- **The DNS server moved to the platform** with the DHCP server: both are lwIP's raw API, which ESP-IDF does not support. mDNS's responder is too.
- **The pin map is plain numbers** (`SCALE_UART 0`, not `uart0`), and the board's target no longer hands every module the SDK's and lwIP's paths.
- **The module graph holds the search.** FreeRTOS's and lwIP's headers on the Pico carry the SDK's include paths, so the compiler cannot refuse an SDK include in a module that names either: the tool searches every file outside `platform/` and `boards/`, and its verdict counts what it finds.
- **The two variables the menu reached by `extern`** (gate 6's), the knob's queue and the display's inversion, are calls (`button_send`, `mini_12864_inverted_display`).
- **No call for the USB loader:** nothing of ours asks for it.
- The image is **1.4 KB smaller** on the Pico W (941,916 bytes at gate 6, 940,508 now): at `-O0` each file kept its own copy of the SDK's inline helpers (the UART's in each of ten makes), and the C++ class is gone.

### Found on the way

- **A scale frame can be cut where its task is handed over** (`scale_frames`' hand-overs: 2 of 167 at cut 7). Not new: ADR-013's runs of 2026-11-15 cut 7 of 2,521 and 12 of 2,520 the same way. The idle and restart phases read every frame.
- **An RGBW strip's white is sent as 0.** Each LED's word is moved up 8 bits for the state machine, which suits a 24-bit LED; for a 32-bit one the white's byte falls off. Kept as it was, and said in `chip_leds.h`; seen in the code only, the bench has no RGBW strip.
- **Shutter 0's width drives SERVO1's pin, and shutter 1's SERVO0's.** The slice's register was always written so; kept, and said in `servo_gate.c`. The bench has no gate fitted.
- **An output now drives its level from its first moment.** The SDK's set-up drove a pin low before the code set it, and the TMC2209's enable pin is low for "on": with the enable not inverted (the default), the drivers were on for that moment at every start. Not measured.
- **The chip's input override reaches what the firmware reads, not the edge detector.** That is how the knob was proven with no hand on it (below): the override sets the level, and the pin's interrupt-force bit fires the interrupt.
- `wireless.c` used the SDK's `MAX()`, which reached it through FreeRTOS's port: written out.

### Each call against ESP-IDF, on paper

ESP-IDF v6.1.1's documentation for the ESP32-S3, read 2026-11-27, nothing built (the managed components `espressif/led_strip` 3.1.1 and `espressif/mdns` 1.15.1). A row is a call of ours, the ESP-IDF call that would give it, and what differs. "Not settled" is where the pages read do not say. The full notes, with a page for each claim, are kept in the gate's scratch (`build_adr006/esp_idf_rows.md`, untracked).

**Step pulses** (`step_pulses.h`, lifted as it was)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `step_pulses_init(channel, pin)` | `rmt_new_tx_channel`, `rmt_enable`; a PCNT unit on the same pin for the count | The S3 has 4 RMT transmit channels: two steppers and two LED strips take all of them |
| `step_pulses_set_rate`, `step_pulses_feed` every ms | each feed a transaction, `rmt_transmit` with `loop_count` the millisecond's steps, a short queue | A period in ticks, not a rate; the fraction of a step carried by the feed. Whether two transactions follow with no gap: not settled |
| `step_pulses_get_stats` | `pcnt_unit_get_count` | A 16-bit counter, widened by `accum_count` |
| `step_pulses_fault_stop` | none public: a register stop is HAL's, which is not public API | With no fault hook either (below), the stop is the queue running dry |

**The image slots** (`update_slots.h`, grown by the image's reading, its write and the trial's watchdog)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `update_slots_read` | `esp_ota_get_running_partition`, `esp_ota_get_next_update_partition`, `esp_ota_get_state_partition` | A slot is a partition handle; "on trial" is `PENDING_VERIFY` |
| `update_slots_read_first` | `esp_image_header_t`, then `esp_app_desc_t` at byte 32: its `version` at bytes 48 to 79 | The version is text there, read into our number; an image runs from either slot (`fits` always) and carries no trial mark (`trial` always) |
| `update_slots_write` | `esp_ota_begin` (it erases), `esp_ota_write_with_offset` | |
| `update_slots_read_free`, `update_slots_free_hash_holds` | `esp_ota_end` validates; `esp_ota_get_partition_description` | Whether `esp_ota_end` checks the appended SHA-256: not settled. The board's name has no field: ours to add |
| `update_slots_try_other` | `esp_ota_set_boot_partition`, `esp_restart`, with the bootloader's rollback on | |
| `update_slots_confirm` | `esp_ota_mark_app_valid_cancel_rollback` | The same model: the new image confirms itself |
| `update_slots_trial_watchdog`, `_feed` | the bootloader's RTC watchdog kept on into the app (`BOOTLOADER_WDT_DISABLE_IN_USER_CODE`), fed and stopped by `wdt_hal_*` | HAL calls, not public API |
| `update_slots_note_write`, `_read` | `RTC_NOINIT_ATTR` memory; the cause by `esp_reset_reason` | Whether it survives a watchdog reset: not settled |
| `update_slots_running_version` | `esp_app_get_description()->version` | Text, parsed |

**The system** (`chip.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_time_us` | `esp_timer_get_time` | Signed, from just before `app_main`; tasks and interrupts alike |
| `chip_busy_wait_us` | `esp_rom_delay_us` | |
| `chip_restart(after_ms)` | `esp_restart`, after an `esp_timer` one-shot for a delay | `esp_register_shutdown_handler` is `system_before_restart`'s like |
| `chip_id` (8 bytes) | `esp_efuse_mac_get_default`: 6 bytes | The 6 padded, or the id defined as a hash of what the chip gives |
| `chip_random32` | `esp_random` | Truly random only with the radio on |
| `chip_console_start`, `chip_console_read`, `chip_console_room` | the USB Serial/JTAG console: `usb_serial_jtag_driver_install`, `usb_serial_jtag_read_bytes` | Ticks, not microseconds. No RTT: OpenOCD's apptrace carries no input, so the console through the probe is a USB port or a UART there |
| `fault_handler_set_stop` | **none documented**: a linker wrap of an internal panic function is all there is | For ADR-007 to decide |
| (the USB loader) | none documented | Nothing of ours asks for it: no call was made |

**The RTOS's port** (`rtos_port.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `rtos_task_start(..., stack_bytes, ...)` | `xTaskCreate`: its stack in bytes already | The reason for the call: the RP chips' port counts words |
| `rtos_critical_enter`, `_exit`, `_enter_from_isr`, `_exit_from_isr` | `taskENTER_CRITICAL(&lock)`, `taskENTER_CRITICAL_ISR(&lock)` with one `portMUX_TYPE` of ours | Found by this check: our 12 calls with no argument would not compile there |
| (the tick, 1000 Hz) | Kconfig's `FREERTOS_HZ`, 100 by default | Set to 1000 there, not by our `FreeRTOSConfig.h` |

**GPIO** (`chip_gpio.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_gpio_output`, `chip_gpio_put` | `gpio_config`, `gpio_set_level` | |
| `chip_gpio_input`, `chip_gpio_get` | `gpio_config` with its pull-up, `gpio_get_level` | Whether `gpio_get_level` may run in an interrupt: not stated |
| `chip_gpio_on_edges(pin, edges, call)` | `gpio_install_isr_service`, `gpio_isr_handler_add` per pin, `GPIO_INTR_ANYEDGE` or `_NEGEDGE` | A handler is told no edge, so ours is told only the pin and reads the level, as every one of ours does |

**I2C** (`chip_i2c.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_i2c_open(bus, pins, rate, pull_ups)` | `i2c_new_master_bus` | The rate is a device's: a device handle a (bus, address), made by the platform at its first use |
| `chip_i2c_write` | `i2c_master_transmit` | |
| `chip_i2c_write_read` | `i2c_master_transmit_receive` | One call, as ESP-IDF has only that for a write with no stop before its read |

**A serial port** (`chip_serial.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_serial_open`, `_format`, `_baud` | `uart_param_config`, `uart_set_pin`, `uart_driver_install`; `uart_set_word_length`, `_parity`, `_stop_bits`, `uart_set_baudrate` | |
| `chip_serial_read` (a byte, or none) | `uart_get_buffered_data_len`, then `uart_read_bytes(…, 1, 0)` | Through the driver's ring buffer: a byte lags the wire by up to its read timeout |
| `chip_serial_read_within(timeout_us)` | `uart_read_bytes(…, ticks)` | Ticks: the TMC2209's 2 ms is 2 ticks at 1000 Hz |
| `chip_serial_write` | `uart_write_bytes` | |

**SPI** (`chip_spi.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_spi_open(bus, pins, rate)` | `spi_bus_initialize`, `spi_bus_add_device` with no chip select | |
| `chip_spi_write` (any length, back once the last bit is out) | `spi_device_polling_transmit` | Without DMA at most 64 bytes a transfer, the UC1701's page 132: the platform splits it, or uses DMA. Lengths in bits |

**An LED strip** (`chip_leds.h`; not `led_strip.h`, which is the component's)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_leds_open(strip, pin, rgbw)` | `led_strip_new_rmt_device`, its format GRB or GRBW | An RMT channel a strip |
| `chip_leds_put(strip, bits)` | `led_strip_set_pixel`, then `led_strip_refresh` | The component takes colours and sends a frame; ours sends each LED's bits as it is given them. The ESP32-S3's side would keep the frame and refresh it |

**A servo** (`chip_servo.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_servo_open(pin0, pin1)` | LEDC: `ledc_timer_config` at 50 Hz, `ledc_channel_config` twice on its timer | 14 bits at most on the S3: some 1.2 µs a step |
| `chip_servo_set(width0, width1)` | `ledc_set_duty_and_update` twice | Not one write: the two take effect at the next period each |

**The network** (`chip_net.h`)

| Ours | ESP-IDF | What differs |
|---|---|---|
| `chip_net_start` | `esp_netif_init`, `esp_event_loop_create_default`, `esp_wifi_init`, `esp_wifi_set_storage(WIFI_STORAGE_RAM)` | The settings are the EEPROM's, not the Wi-Fi driver's flash |
| `chip_net_station_up` | `esp_netif_create_default_wifi_sta`, `esp_netif_set_hostname` | |
| `chip_net_join(ssid, key, security, bssid, timeout)` | `esp_wifi_set_config`, `esp_wifi_connect`, then the events (`IP_EVENT_STA_GOT_IP`, `WIFI_EVENT_STA_DISCONNECTED` and its reason) | No timeout of its own and no retry: the ESP32-S3's side waits on the events with ours |
| `chip_net_leave` | `esp_wifi_disconnect` | |
| `chip_net_joined`, `_station_address`, `_hotspot_address`, `_rssi` | kept from the events; `esp_netif_get_ip_info`, `esp_wifi_sta_get_rssi` | |
| `chip_net_scan_start`, `chip_net_scanning` | `esp_wifi_scan_start`, `WIFI_EVENT_SCAN_DONE`, `esp_wifi_scan_get_ap_records` | A scan only in station or station-and-hotspot mode: the set-up's scan beside the hotspot needs both |
| `chip_net_hotspot_start`, `_stop`, `_addresses_given` | `esp_wifi_set_mode(WIFI_MODE_APSTA)`, the `.ap` config; esp_netif's DHCP server; a DNS server on a socket (the `captive_portal` example's); `IP_EVENT_ASSIGNED_IP_TO_CLIENT` counted | Our DHCP and DNS servers are lwIP's raw API, which ESP-IDF does not support: they stay the RP chips' |
| `chip_net_announce` | the `espressif/mdns` component: `mdns_init`, `mdns_hostname_set`, `mdns_service_add` with its text items | lwIP's own responder is raw API too |
| `chip_net_led` | none: a plain pin on an S3 board | |
| (lwIP's sockets, `select`) | the same | 10 sockets by default: ours want some 9 |

### The proofs, on the PC

- Both boards build at every commit. `app.bin`: the Pico W 941,916 bytes at gate 6, 940,508 at the end (1,408 fewer); the Pico 2 W 926,892 and 925,796 (1,096 fewer). Cut 13 changed no code: every symbol of the image the same as cut 12's (`tools/symbols_compare.py`).
- `tools/size_report.py`, the Pico W: flash 918.5 KB at gate 6, 917.2 KB of its 1,016 KB slot; **RAM at link time 252.4 KB of 256 KB, as at gate 6.** The Pico 2 W: flash 903.2 KB of 1,792 KB, RAM 285.3 KB of 512 KB.
- A build without the servo gate (`-DFEATURE_SERVO_GATE=OFF`, 933,708 bytes), the two fault test images, the update's test images and the hotspot's probe image build.
- `tests/host_test.py`, `release_records.py`, `features_check.py`, `tools/module_graph.py`: `VERDICT PASS`.
- **Cut 13's build found five files** that had a standard header (`string.h`, `stdio.h`, `stdlib.h`, `ctype.h`) only through the SDK's, which the board's target had brought them: each includes its own.

### On the unit

2026-11-27, the bench's Pico W, the bench ready and the hopper empty (the owner, the same session). Each cut was flashed and its path run before the next; every run below ended `VERDICT PASS` but the first soak, which ended `VERDICT FAIL` on its heap (said in its row).

| After | Image | Run |
|---|---|---|
| cut 2 | 2.0.3-326 | `slot_trial --expect back` with a test image that never confirms (block 2.1.1): fed 60 s, then the watchdog, back on slot A 67.8 s after the trial was asked; an update over the air from the local OTA server (block 2.1.0, 942,444 bytes, fetched by the unit itself), confirmed on slot B; the settings the same |
| cut 3 | 2.0.3-327 | the unit's id, and its hotspot's name and key (the set-up code's fingerprint) as at gate 6; `menu_tour`; `pulses`, its two fault images built at the cut; `charge_cycle` (12.9 s); `api_check`, its restart among it |
| cut 4 | 2.0.3-328 | `stats tasks` 25 s after a start, against cut 3's: within what two starts of one image differ by (up to 140 bytes; the poll's priority 2, or 4 inherited from a lock it held); `charge_cycle` (13.0 s), `api_check`, `menu_tour`, `wifi scan` (5 networks), an update over the air again, confirmed |
| cut 5 | 2.0.3-329 | `menu_tour`, `charge_cycle` (12.8 s), the coarse motor turned both ways with no late feed; the panel's pages sent in 5.1 ms |
| cut 6 | 2.0.3-330 | the settings the same after the flash; `api_check` (12 resources stored, a restart); the settings the same; `settings_roundtrip` |
| cut 7 | 2.0.3-331 | `scale_frames` with the A&D (104, 167 and 115 frames; above, the hand-overs'); `tmc coarse setup 3`, `tmc fine setup 3`: no retry; a real reading in `state`; `charge_cycle` (12.9 s) |
| cut 8 | 2.0.3-332 | `menu_tour`; the pages sent in 5.1 ms |
| cut 9 | 2.0.3-333 | the knob driven through the probe (`knob_probe.py`, below): the tap `r l d X… u x…`, the click opened the focused row, the reset button went back, as on cut 8; `reach`, `menu_tour`, `charge_cycle` (13.0 s) |
| cut 10 | 2.0.3-334 | the LEDs' two state machines running on their pins (pio1, 13 and 28); `charge_cycle` (12.9 s), which lights them by its states; `menu_tour`; the settings the same |
| cut 11 | 2.0.3-335 | the servo slice's registers through the probe the same as cut 10's at rest, closed and open (divider 38.1875, top 65,465, widths 5,898 and 2,097, then swapped) |
| cut 12 | 2.0.3-336 | the settings the same, `menu_tour`; the unit's name by mDNS (`ping opentrickler-4A82.local`) and both its services with their three text items; `api_check`, `stream_check`, `web_robust`; a soak of 600 s with `--load`, `VERDICT FAIL` (24 charges; 1 web read of 6,335 missed, a reset; every event; no late feed; no lwIP allocation failed) whose heap read quiet ended 96 bytes below its start: the unit had restarted just before (`stream_check`), and something kept from then on was made during the run, for a second soak of 300 s, `VERDICT PASS`, gave every byte back (51,616 before and after; 12 charges, 1 read of 2,959 missed), and the day's earlier soaks, on gate 6's code, began at 51,608, within 8 bytes of where this one ended; the set-up's join from the hotspot, on a probe image (`-DRC_TEST_AP_PROBE=ON`): the hotspot up on the strongest network's channel after a scan, a scan beside it (4 networks), then `wifi join`: the stored network joined on the hotspot's channel, the hotspot kept up; the API's network the hotspot at 192.168.4.1 and the station joined |

- **The knob with no hand on it** (`build_adr006/knob_probe.py`, scratch): the chip's input override (IO_BANK0's INOVER) holds a pin's level low or lets it be, and the pin's interrupt-force bit fires its interrupt, which runs again while the bit is set, each time reading the same levels. A turn A-first ends in one detent clockwise whatever the repeats; the button's first run is its press, the repeats bounces. Each override is put back at the end. Run on cut 8's image first, then on cut 9's: the same.
- **The LEDs' light** is the owner's eye, as the gate says: the 12864's backlight. What the probe shows is their state machines claimed and running on their pins.

### At the gate's end

2.0.3-338 (`d5ab3b8`), flashed by the probe; every run `VERDICT PASS`:
- the settings the same as before gate 1 (`logs/adr006/picow_before_gate1`), after the flash and at the end; `menu_tour`; `charge_cycle` (12.8 s, and 13.0 s again later); `api_check`; `settings_roundtrip`; `reach`;
- the knob through the probe (the tap `r l d X… u x…`); the servo slice's registers as at cut 10; `scale_frames` with the A&D, every frame a number (105, 168 and 116); `pulses`, its fault images built at the cut;
- **an update over the air** from the local OTA server: a test image built at the cut (block 2.1.0, 940,508 bytes), fetched by the unit itself, tried in slot B and confirmed on its network; then the gate's image flashed back into slot A;
- **the stacks never touched,** after a tour and a charge, as gate 6 read them: the job task's 2,968 bytes (2,968 at gate 6), the menu's 1,352 (1,352); the scale's 728 (736: the serial read is a call deeper).

The test release is kept out of the served folder, in `logs/adr006/gate7_release_v2.1.0`; the local OTA server was stopped after each use. The unit is left on 2.0.3-338, slot A, its settings as before gate 1. The Pico 2 W waits, by the owner's word; its image builds at every commit.

**Gate 7 is built**, and merged by #144 (the owner).

### For gate 8 and ADR-007

- **The fault's hook has no ESP-IDF counterpart.** A linker wrap of an internal panic function is all there is: ADR-007's to decide, with the step pulses' stop, which has only HAL calls there.
- **The console through the probe** is RTT's; ESP-IDF's would be a USB port or a UART.
- **The network there is events,** with no join timeout: `chip_net_join` is that side's to build on them. The set-up's scan beside the hotspot needs the station and the hotspot both.
- **The chip's id is 6 bytes there**, 8 here: ADR-007 pads it, or makes the id a hash of what the chip gives.
- The tick is 1000 Hz in our `FreeRTOSConfig.h`; there it is Kconfig's.

## Gate 8: stored records apart from the compiler

2026-11-27, approved by the owner ("Gate 7 of ADR-006 is merged. ... continue to the next gate"), after PR #144 merged gate 7. On the branch `adr006_gate8`, 845e681 and d599e89.

**The verdict: every stored field is of a fixed width, and the Pico boards are unaffected.** Each stored enum names its integer, `typedef enum : uint8_t {...}`, and keeps its name. Both boards' images are byte for byte what they were but for the version's text and the image's seal, and every record's size and every field's offset are the same on both (gdb's `ptype /o`).

### What was built

- **The ten stored enums are C23's enums with a fixed type,** `typedef enum : uint8_t {...} scale_driver_t;`: still enums to the compiler (`-Wswitch` warns of a value a `switch` misses, gdb names the values), one byte by the language rather than by the Pico's ABI, and a value past a byte does not compile. The structs read as before. The eight of v1.21's records, and the two that were a `uint8_t` with a comment naming its enum:

| Record | Field | At byte | Its enum |
|---|---|---|---|
| `eeprom_charge_mode_data_t` | `decimal_places` | 36 | `decimal_places_t` |
| `eeprom_scale_data_t` | `scale_driver`, `scale_baudrate`, `scale_uart_format` | 2, 3, 4 | `scale_driver_t`, `scale_baudrate_t`, `scale_uart_format_t` |
| `eeprom_neopixel_led_metadata_t` | `pwm_out_led_chain_count`, `pwm_out_led_colour_order` | 16, 17 | `neopixel_led_chain_count_t`, `neopixel_colour_order_t` |
| `mini_12864_module_config_t` | `display_rotation`, `charge_view` | 5, 7 | `display_rotation_t`, `display_charge_view_t` (was a `uint8_t`, ADR-010) |
| `eeprom_wireless_metadata_t` | `auth` | 98 | `wireless_auth_t`, which was `cyw43_auth_t`: ours, with cyw43's name |
| `eeprom_update_config_t` | `channel` | 2 | `update_channel_t` (was a `uint8_t`, ADR-010) |

- **`tests/release_records.py` holds the rule from now on:**
  - a stored field is of a fixed width (`uint8_t` to `uint32_t`, `int8_t` to `int32_t`, `float`, `bool`, `char`), a stored struct of its list, the colour's union (`rgbw_u32_t`, a `uint32_t` and its four bytes, packed), or an enum that names one of those integers;
  - an enum of a v1.21 field names `uint8_t`, the byte it lay in;
  - each v1.21 value means what it meant, through a rename (`wireless_auth_t`, v1.21's `cyw43_auth_t`).
- **AGENTS.md** says the rule, with the owner's word on it.
- Two headers that used `uint8_t` and `bool` with no include of their own (`neopixel_led.h`, `mini_12864_module.h`) include `stdint.h` and `stdbool.h`.

### The first build, and the owner's choice

The first build (845e681) held each field in a `uint8_t` whose comment named its enum, as `charge_view` and `channel` did, and the check read the comments. The owner asked for the old type names kept, the type made one byte by a typedef. Two ways were put (the README, the owner's direction): a plain `typedef uint8_t`, the type no longer an enum, or an enum with a fixed type, tried first on a test file with the compilers the builds use:
- arm-none-eabi-gcc 15.2 takes `enum : uint8_t` in the build's `gnu11` (only `-Wpedantic` remarks on it, which the build does not use), and in `gnu17` and `gnu23`;
- it stays one byte with `-fno-short-enums`, which stands in for a compiler that does not shorten an enum;
- zig cc 0.13 (the PC build's, in `c11`) and Ubuntu's gcc 15.2 in `c11` take it; both compilers refuse a value of 300 in it;
- ESP-IDF's compiler is not tried: GCC 13 and later have it, which is all that is known.

The owner took the enum with a fixed type (d599e89). Both builds' images are the same code.

### Found on the way

- **Why it matters on a unit:** the EEPROM is on the controller board, and stays there whatever chip is put in its socket (ADR-007's ESP32-S3-Pico goes in the Pico's). A record another compiler laid out otherwise would not match its CRC at its size, and the loader, looking at the other sizes, could take it for another version's and read its fields shifted.
- **The LEDs' colours** (`neopixel_led_colours_t`, a struct in the LEDs' record) were not in the check's list: they are, v1.21's three fields first.
- Before the change the two boards' layouts were already the same as each other.

### The proofs, on the PC

- **Each record's layout** before and after, both images (`build_adr006/g8_layouts.py`, scratch: gdb's `ptype /o` of every stored type, each field's offset, size and name judged): the same, `VERDICT PASS` on each board.
- **The images**, 940,508 bytes (the Pico W) and 925,796 (the Pico 2 W), as before. Of their bytes 40 differ on each, in two runs: the version's text (`2.0.3-342`, `bb73076`, to `2.0.3-344`, `d599e89`) and the image's SHA-256 seal. `tools/symbols_compare.py`: `VERDICT PASS`, both. RAM at link time 252.4 KB on the Pico W, as before. The first build's images were the same but for their version.
- **The check tried against five faults,** each put in and taken out (not kept), each failed: an enum written plain; one widened to `uint16_t`; the field written a bare `uint8_t`; a value renumbered; a field a `size_t`.
- `tests/release_records.py`, `tests/host_test.py` (v1.21's records read, both ways), `tools/module_graph.py`, `tests/features_check.py`: `VERDICT PASS`.

### On the unit

2026-11-27, the bench's Pico W, the bench ready and the hopper empty (the owner, the same session). Every run `VERDICT PASS`:
- before the first flash, on 2.0.3-338 (gate 7): the settings the same as before gate 1 (`logs/adr006/picow_before_gate1`), and copied again (`logs/adr006/picow_before_gate8`);
- the first build, 2.0.3-343 (`845e681`): `flash`; the settings the same after it and after the runs; `menu_tour --compare` against the baseline (the inversion off for it, as AGENTS.md says); `charge_cycle`, 13.0 s from charging to the cup prompt, the cup 0.0 gr before and after;
- the gate's image, 2.0.3-344 (`d599e89`): the same, `charge_cycle` 12.9 s. The unit is left on it, slot A.

**Gate 8 is built**, and merged by #145 (the owner).

### For gate 9

- The filler rule (an appended field never starts in the old struct's tail padding) is still checked by no tool: handed on, with the other fixes, to the ADR of its own (the README, "Handed on").
- Whether `bool` and the alignments are the same with ESP-IDF's compiler, and whether it takes `enum : uint8_t` in the C mode ESP-IDF builds in, is not checked: ADR-007's to see, on its first build.

## Gate 9: what a board carries

2026-11-27, approved by the owner ("merged, please proceed"), after PR #145 merged gate 8. On the branch `adr006_gate9`, f2f60a9 to 1effd85.

**The verdict: a part a build may leave out is declared once, and only the start-up's wiring asks whether it is in the build.** The Pico W built without the servo gate runs with no trace of it on the screen, in the pages' API or in its index and schema, and built with it again reads its record back as stored. A board's list with no view, or two, is refused as the build configures. A stored make of scale the build does not carry has nothing read in its place: the unit says so, and a charge and a calibration are refused.

### The owner's two choices (before cut 1)

- **The gate handed over:** an always-present `services/gate`, which `app.c` hands the servo gate to, in place of the servo gate's header answering "no gate" when its source was left out. Each later optional part needs no stub header of its own, and `app.c` is the one place that asks.
- **An absent make refused, nothing standing in:** in place of today's fallback to the A&D's driver, and with no A&D to the simulated scale.

### What was there already

Each module hands its settings table and its EEPROM save handler over in its own init (`settings_register`, `eeprom_register_handler`), and no console command names the gate: step 1's "parts handed over" came down to the servo gate's users.

### The cuts

| Cut | What | Where |
|---|---|---|
| 1 | The gate's calls in `services/gate` (`gate.h`): its ratio and states, a struct of its calls, `gate_set()`, and each call answering as a unit with no gate when none is handed. The servo gate gives its calls (`servo_gate_calls()`); `app.c` hands them over when the build carries it. Charge mode, the cleanup, the menu and the API ask the gate; the API adds the gate's routes, its index line and its schema entries only then; the menu shows its page (a page's `shown`, new), its row in Settings and the purge screen's Gate row only then | `services/gate`, `app.c`, `api_v1.c`, `api_v1_commands.c`, `menu_nav.c`, the charge and the cleanup |
| 2 | `ot_feature(servo_gate services/servo_gate "...")`: its option, its module out of the board's list when off, `FEATURE_<NAME>` for `app/`, the `Features:` line, and `ot_features.h` in each build's folder, which the index's `features` is made from. `tools/module_graph.py` fails on a part's switch read outside `app/` | `cmake/ot_module.cmake`, `base.cmake`, `api_v1.c` |
| 3 | `ot_fills(platform)` (`platform/rp2`) and `ot_fills(view)` (`view_12864`); `ot_check_places(platform view)` refuses none or two | `cmake/ot_module.cmake`, the root `CMakeLists.txt` |
| 4 | `OT_MODULE_OPTIONS_<module>` in a board's file, applied after the image's own; neither board names one | `cmake/ot_module.cmake`, both `board.cmake` |
| 5 | A stored make the build does not carry, or a number no make has: no make runs, the start says so, `GET /api/v1/scale`'s `driver` is null, a charge and a calibration are refused (`409 no_scale`, the console's `@err no_scale`, the screen's "No scale"); the simulation still runs when asked. A test build leaves the A&D's make out (`-DRC_TEST_NO_AND_FXI=ON`) | `scale.c`, the charge, the calibration, the API, the console, the menu |
| - | `tools/size_report.py` gives our flash by module (each module's library) | |

### Found on the way

- **Before cut 5 the simulated scale could stand in for a real one.** A stored make the build did not carry, or a number no make has, was read by the A&D's driver, and in a build without the A&D by the simulated scale, whose weight follows the motors while they pour real powder. No build lacked the A&D, so only a damaged record reached the first.
- **The purge screen showed "Gate: Off" in a build without the gate**, and its click said there was none: the row is gone there.
- **What the hand-over costs:** the gate's API and menu code is in every build, unused without a gate. The Pico W's image without the gate is 4.3 KB smaller than the base build's (938,132 bytes against 942,516), where at gate 7 it was 8.2 KB smaller, that code compiled out.
- **The schema's gate entries now come after the others,** in its actions and its states: their order only.
- **The base build's image grew 2,008 bytes** on the Pico W (940,508 at gate 8, 942,516): each index entry and each menu page a pointer more (its `present` or `shown`), the `features` loop, the gate's calls and the absent make's checks, all at `-O0`.
- **A module at `-Os` by its board,** tried once with the menu (`-DOT_MODULE_OPTIONS_menu=-Os`, not kept): its files compile `-Og -O0 -Os`, the last holding, every other module's at `-O0`; the Pico W's image 15 KB smaller (926,516 bytes).
- Two headers used `uint8_t` and `bool` with no include of their own (`neopixel_led.h`, `mini_12864_module.h`; gate 8 found it): they include them.

### The lines' lists

**What still names a tubes' module** (`charge`, `charge_control`, `cleanup`, `calibration`, `calibration_method`, `motors`, `servo_gate`, `tmc2209`; the step pulses in `platform/rp2`; the simulated scale in `scale`), each besides the tubes' modules themselves, from `tools/module_graph.py --modules`:

| Module | Names | What for |
|---|---|---|
| `app/app` | `charge`, `cleanup`, `calibration`, `motors`, the servo gate where carried | the wiring: each started and handed over |
| `ui/menu` | `charge`, `cleanup`, `calibration`, `motors`; the gate through `services/gate` | the menu's table and the jobs' screens |
| `web/api` | `charge`, `cleanup`, `calibration`, `motors`; the gate through `services/gate` | the API's commands |
| `console/remote_control` | `charge`, `calibration`, `motors` | the console's `charge`, `motor`, `tmc`, `sim` |
| `services/scale` | `motors` | the simulated scale follows the tubes' speeds |

As the ADR expected, but for two: the job task names none (handed its runs at gate 6), nor the hard fault's handler (handed the motors' stop); the console and the scale's simulation were not on its list. This table is where the thrower's ADR starts.

**What names the screen outside `ui/`:** nothing but `app/`'s wiring. No service, no module of the web and not the console requires `menu`, a view, `mini_12864` or `uc1701`.

### The proofs, on the PC

- Every build at 1effd85: the Pico W 942,516 bytes, flash 919.1 KB of its 1,016 KB slot, RAM at link time 252.4 KB of 256 KB as at gate 8; the Pico 2 W 928,156 bytes (905.5 KB of 1,792 KB, RAM 285.3 KB); the Pico W without the gate (`Features: servo_gate=0`) 938,132; the test build without the A&D 942,068.
- **Our flash by module** (`size_report.py`, the Pico W): 380.7 KB in all, the API 104.2 (the pages 41.6 of it), the menu 45.2, the settings 21.1, the view 20.8, the HTTP server 19.5, the platform's RP side 17.6, the update's fetch 17.5, the charge 16.8; the servo gate 2.9, the gate 0.2.
- **Tried once each, not kept:** a board's list without the view (refused: "no module for the view"), and with a second view module (refused: "2 modules for the view (view_12864, view_second)"); a part's switch read in `menu_nav.c` (`module_graph.py`: `VERDICT FAIL`, the file named).
- `tools/module_graph.py`, `tests/host_test.py`, `tests/release_records.py`, `tests/features_check.py`: `VERDICT PASS`. No new warning.

### On the unit

2026-11-27, the bench's Pico W, the bench ready and the hopper empty (the owner, the same session). Its settings copied first (`logs/adr006/picow_before_gate9`); its gate's record is not the defaults (shutter 0 closes at 0.032).

| Image | Runs |
|---|---|
| the base build, 2.0.3-351 (`1effd85`) | `VERDICT PASS` each: the settings the same as before gate 1; `menu_tour --compare`; `charge_cycle` (12.7 s); `reach`; `api_check` |
| without the servo gate | the index's `features`: `servo_gate` not present; `GET /api/v1/gate` 404 "no such path on this build"; neither the index's resources nor the schema names the gate; `screen list` 17 pages, no `settings/gate`; `menu_tour --compare`: Settings differs by its scroll bar alone (the "Servo gate" row gone), `settings/gate` not found, every other judged page the same; the settings copy: `settings/gate` gone, nothing else; the purge screen Speed, then Stop and Zero, no Gate row; `charge_cycle` (13.0 s), `reach`, `api_check` (every resource saved): `VERDICT PASS` |
| without the A&D's make (its stored make the A&D's, the A&D connected) | the start: "Scale: the stored make (0) is not in this build: no scale reads, and a charge and a calibration are refused"; `state` `scale="none: the stored make is not in this build"`, `weight=inf` (the Pico W prints NaN so): the A&D is read by nothing; `GET /api/v1/scale` `driver` null; `POST /api/v1/charge/start` and `/api/v1/calibration/start` 409 `no_scale`; the console's `charge 30` and `calibrate scale` `@err no_scale` (run with `--no-cup-check`, each start expected refused); the screen's start, "No scale / Its make is not / in this build" |
| the base build again | the settings the same as before gate 9 and as before gate 1: the gate's record read back as stored (`saved=true`, shutter 0 at 0.032), after the build without it had saved every resource it had; the index's `servo_gate` present, `GET /api/v1/gate` 200; the A&D read again |

The unit is left on 2.0.3-351, slot A, its settings as before gate 1. The Pico 2 W waits, by the owner's word; its image builds.

**Gate 9 is built.**

### For gate 10

- The small fixes, the ADR for what was handed on, the end's checks (the owner's real throw among them), and the Pico 2 W's runs, which wait for the whole ADR.
- The gate's API and menu code is in every build (above): a build that leaves the gate out for its flash keeps some 4 KB of it. Compiling it out would put the part's switch back outside `app/`; a later build that needs those 4 KB brings it up.
