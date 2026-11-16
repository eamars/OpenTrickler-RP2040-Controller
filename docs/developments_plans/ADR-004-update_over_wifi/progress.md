# ADR-004: progress

## Gate 1: the layout and the boot path

Approved by the owner 2026-11-16 with the whole ADR ("Approve as written"), to start on the Pico 2 W, which is on the probe; the Pico W comes later that day. Order: the Pico 2 W's layout and its image, then its trial; the Pico W's bootloader and its images; the full installs.

### The Pico 2 W's layout, shown 2026-11-16 (1.20.202, built from the working tree on beebfea)

- **Built:** `targets/pico2_w_partition_table.json` (A at 8 KB, B at 1,800 KB paired with A, 504 KB spare that the ROM does not boot from), made into `partition_table.bin` by picotool at each build; the image linked at the start of flash with the linker's region cut to a slot (`targets/flash_layout.cmake`, `targets/flash_region.ld.in`), sealed with its SHA-256 (`pico_hash_binary`), its metadata block carrying the version `scripts/gen_version.py --block-version` gives (1.20.202 is 276.202). `rc_run.py flash` on the Pico 2 W writes the table, blanks the start of slot B, and writes the image's `.bin` into slot A, after checking that the image's block version and its version string agree.
- **The ROM maps the slot it boots** (the ADR's first claim not yet seen on the chip): after the flash, the QMI's `ATRANS0` read `0x01C00002` through the probe, a window of 1,792 KB from flash's 8 KB, at the start of the address space; the other three windows empty. The image linked for the start of flash runs from slot A.
- `settings` copied before the flash (`logs/rc/settings/20261116_123506_settings`), `flash`, `menu_tour --compare tests/menu_baseline/pico2_w` (the version form differs, not judged), `charge_cycle` (30.000 for 30), `settings --compare` with the copy: each `VERDICT PASS`, 12:35 to 12:36.
- Size: the image takes 752.2 KB of the slot's 1,792 KB (42%).
- picotool: the build uses 2.3.1 (`toolchain.env`); the trial seal on the Pico W image (README, "What we know") was made with an older 2.1.1 found on this PC.

### The Pico 2 W's trial, shown 2026-11-16 (1.20.203, built from the working tree on d7af299)

- **Built:** `update.c` (the same on every chip: the slots read at start-up; on a trial, the watchdog armed at 8 s and fed each second for at most a minute; the confirmation once the unit is on its network and the web server listens; the console's `update` and `update try`), `update_slots.h` and `update_slots_rp2350.c` (the ROM's boot information, the partition table, a flash update restart into the other slot, `rom_explicit_buy`), `update_slots_rp2040.c` (no slots until the Pico W's step). The server tells its network's owner when it first listens (`http_server_network_t.listening`), and `wireless.c` counts the unit on its network as a station when Wi-Fi is set up, its own access point when it is not: a unit set up for Wi-Fi that falls back to its access point leaves its trial. `pico_flash` is linked for `flash_safe_execute`, through which the ROM's confirmation rewrites flash while FreeRTOS runs both cores.
- **Update images** are built with `APP_TRIAL_IMAGE=ON` (the try-before-you-buy mark; picotool reads them `tbyb: not bought`). Test builds only: `RC_TEST_BLOCK_VERSION` gives a trial image a version of its own, newer than the running one built from the same commit, and `RC_TEST_NO_CONFIRM` makes one that never confirms itself.
- **`rc_run.py slot_trial --image <elf> --expect confirmed|back|refused [--corrupt]`** writes an image into the free slot through the probe, restarts the unit on what it runs, asks a trial, and judges. Each image's RTT control block lies where its own ELF says and RAM keeps the last one's across a restart, so the console is looked for at the address of the image expected to run (found when the never-confirming build's sat 0x98 bytes lower and the console was not seen).
- **Shown**, each `VERDICT PASS`, 12:46 to 12:52:
  - **confirmed, into B** (276.900 over 276.203): after the write and a restart the unit still ran A; on trial B ran, its server listened 4.3 s later and B confirmed itself; after a restart B ran, not on trial.
  - **confirmed, into A** (276.902 over 276.900): confirmed 7.8 s after the trial was asked, kept after a restart. Both branches of the slot code.
  - **back** (276.901, never confirming, into A over B at 276.900): ran on trial, was fed its minute, then the watchdog restarted the unit and it was back on B, 69.7 s after the trial was asked. The higher version did not keep an image that never confirmed itself.
  - **refused** (276.901 with one byte changed): the ROM never booted it; B ran on, not on trial. The ROM checks the image's hash, so a damaged image does not start.
  - **The full install wins:** `flash` of the normal build (276.203) over the two newer images booted it from A, slot B blanked; then `menu_tour`, `charge_cycle` and `settings --compare` with the morning's copy.
- **Found:** `rc_run.py`'s check of the cores read core 1 as `reset` 0.2 s after a restart by the watchdog, the moment upstream FreeRTOS resets core 1 as its scheduler starts, which openocd names `unknown` on the RP2040. Asked again until it runs, as `unknown` was.

### The image checked before it is tried, both chips (1.20.204)

- **A change from the ADR's text, for the owner's eye:** the README has the RP2040's bootloader check an image's SHA-256 when it boots it on trial. A trial is only ever asked by the running image (`update try` now, the upload in gate 2), so the running image checks first, with the same code on both chips, and the bootloader, which only USB can change, keeps no hash code. The RP2350's ROM checks the hash again at boot (shown, 12:51). What a damaged image does is unchanged: it never runs.
- **How picotool hashes** (its `bintool.cpp` at 2.3.1, read 2026-11-16): the data of each load map entry from where it is stored, then the block's words up to and including the hash definition, with the try-before-you-buy mark cleared in the copy hashed, so the hash still holds once the image is confirmed. Recomputed on the PC for `build_2w_trial/app.bin`, it matched the sealed value picotool verifies; without clearing the mark, it did not.
- **Built:** `picobin_image.c` (the image's blocks read, no RTOS and no printf, so the RP2040's bootloader can use it too); in `update.c`, `update try` refuses an image that is not for this chip, not marked for a trial, not newer than the one running, not linked for the free slot, or whose SHA-256 (mbedTLS's, the SDK's, with only SHA-256 built: `src/mbedtls_config.h`) does not hold. The flash grew by 11 KB (775.3 to 786.5 KB on the Pico 2 W).
- **Shown on the Pico 2 W:** confirmed into B again (the C hash took the good image), the changed byte refused before any restart ("its hash does not hold"), an image of the running version refused ("not newer than the image running"), and back again (69.3 s); each `VERDICT PASS`, 13:00 to 13:02.

### The Pico W's bootloader and images, built (1.20.205, not yet on the unit)

- **The layout fixed by measure:** the bootloader is 6.4 KB with the SDK's minimal runtime (`pico_minimize_runtime`; 24.7 KB without it, 92% of it printf, the alarm pool, stdio and mutexes it never uses). It is given 16 KB, so each slot has 1,016 KB: A at `0x10004000`, B at `0x10102000` (README's table). The image, 793.2 KB with gate 1's update code, leaves 222.8 KB in its slot.
- **Built:** `bootloader/bootloader.c` (both slots' blocks read with `picobin_image.c`; an image may boot from a slot when it is an executable for the RP2040, linked for that slot, with a stack in RAM and a reset handler in the slot; a trial asked through the watchdog's scratch registers 0 and 1, cleared as they are read; the watchdog armed for an image on trial; otherwise the confirmed image of the higher version, A on a tie; the USB loader with none; PR #111's jump), `src/update_boot.h` (the protocol both share), `src/update_slots_rp2040.c` (its slot from its link, its trial from its own mark, a trial asked and the watchdog restart, the confirmation as a one-word program that clears the mark, through `flash_safe_execute`). `targets/flash_layout.cmake` links the image for `APP_SLOT` (A or B), turns on the metadata block the RP2040 leaves off, and seals it.
- **Checked on the PC:** the slot A image's blocks read and its hash recomputes to the sealed value; the slot B update image is linked at `0x10102000` and carries the mark; `rc_run.py` reads each image's block version from its `.bin` (picotool's `info` does not read an RP2040 image away from the start of flash), and finds 276.205, 276.900, 276.901 and 276.902 for the four Pico W images.
- **PR #111's combined `.uf2`:** the first guess at why it failed (each part's own block count) is wrong: its script numbers the blocks across the file. Not found yet.

### The full installs, made (not yet tried by USB)

- **Built:** `scripts/full_install_uf2.py`, run at each build of a full install's image (`targets/flash_layout.cmake`): one `.uf2` with what starts flash, slot A's image and the start of slot B written as erased flash, every block numbered across the file. `build\full_install.uf2` (Pico 2 W, the absolute family: the table and both slots at fixed addresses, 3,090 blocks) and `build_w\full_install.uf2` (Pico W, the RP2040 family, 3,216 blocks). picotool reads the Pico 2 W's as the partition table, its hash verified.
- **RP2350-E10** (picotool's `main.cpp` and `elf2uf2.cpp` at 2.3.1, read 2026-11-16): picotool's fix adds an absolute block to a UF2 that is not of the absolute family, for the A2 chips. The Pico 2 W's full install is of the absolute family, so it takes none.
- The Pico 2 W was put back on the normal build by `flash` (1.20.206, slot A), then `menu_tour` and `charge_cycle`: each `VERDICT PASS`, 13:10 to 13:11.

## Gate 2: the update through the API

Approved by the owner 2026-11-16 ("Start gate 2"), to build on the Pico 2 W while the bench waits for the owner's hands; the Pico W runs gates 1 and 2 together when it is on the bench.

### The job, shown on the Pico 2 W 2026-11-16 (1.20.207, built from the working tree on 9114127)

- **The contract** (api_v1.md, "Firmware updates"): `GET /api/v1/update` (state, the image running with its slot and block version, `takes`, the free slot's size, the bytes received, how the last update ended), `POST .../start`, `PUT .../image`, `POST .../install`, `POST .../stop`; the update's events (started, stopped with its reason and a refusal's code, received, installing, confirmed, back); the codes `not_an_image`, `wrong_chip`, `wrong_board`, `not_an_update`, `not_newer`, `wrong_slot`, `hash_mismatch`, `not_ready`; time limits *p* of 30 s receiving with no bytes and 60 s ready and not installed.
- **Built:** the update is a job of the menu's, as a charge is (`ACTIVITY_UPDATE`, `APP_STATE_ENTER_UPDATE`): the API asks, the menu takes it, `update_mode_menu()` holds the screen ("Firmware update", the share received, "Keep the power on", RST to stop), so nobody starts a charge at the unit during it. The image is written into the free slot a 4 KB sector at a time through `flash_safe_execute`; its first block checked in the first sector (chip, mark, version), the whole once it is in (the slot it is linked for, its board from its binary information's `pico_board`, its SHA-256). Install restarts into the trial; before it a note is left for the next start (`update_slots_note_write`), so the image that runs after says `confirmed` or `back`. `scripts/release_manifest.py` writes a release's `manifest.json`; `rc_run.py update` reads one, takes the image the unit asks for, checks its SHA-256, and follows the unit through the restart; `api_check` calls the update's actions.
- **Found on the way:**
  - **The RP2350's ROM clears the watchdog's scratch registers 0 to 3 at a restart** (written 0x55504401 and 0x12345678 through the probe, a watchdog restart, read zero), though the register's description says they persist through a soft reset. The power manager's scratch registers keep them. The note lives there on the RP2350 and in the watchdog's on the RP2040, behind `update_slots.h`.
  - **A menu's start right after an update was refused although the update read idle:** the job ended in the web server's task while the menu's task still held its activity, up to 250 ms. The job now ends in its mode, `ending` until the menu is back at its forms; a refusal or a stop wakes the menu at once (`api_check`).
  - **`POST /api/v1/stop` did not stop an update:** `activity.c` named only the charge and the cleanup as jobs that take a stop. The update takes one.
  - `rc_run.py update` run straight after a flash met a unit not yet on the network: its waits take a reply cut short (`http.client.HTTPException`, as ADR-003's soak met) and a start's answers not yet 200.
- **Shown**, each `VERDICT PASS`, 13:52 to 14:09:
  - **confirmed:** the image (798,428 bytes) sent in 13.8 s, 56 KB/s; the unit back on the network 8.4 s after the install and confirmed; with two browsers reading the screen's mirror and the charge state as the portal does, 0 of 87 reads missed and no lwIP allocation failed (`net` before and after); then `api_check` (32 paths read twice, 24 of 24 actions called), `menu_tour`, `charge_cycle` and the settings compared with the morning's copy.
  - **back:** an image that never confirms itself: on trial, then back on the image before 75 s after the install, `last` and the event log saying `back` with the version tried.
  - **refused**, each with its code, the job ended and nothing installed: another chip's image (`wrong_chip`), an image naming another board (`wrong_board`), a full install's image (`not_an_update`), an older one (`not_newer`), a changed byte (`hash_mismatch`, once the whole image was in). The first three in the first 4 KB, 0.1 s after the start of the body.
  - **cut off:** half the image and the connection closed: the job stays `receiving`, then ends by its time 33 s later, nothing installed.
  - **busy:** a start during a charge, 409 `busy`.
- Size: the Pico 2 W's image 779.7 KB of its 1,792 KB slot (`tools/size_report.py`).

### Wi-Fi joins again (ADR-012's hand-on), shown on the Pico 2 W 2026-11-16 (1.20.208)

- **Built:** while Wi-Fi is set up and the station is not joined, `wireless.c` tries to join again every 60 s *p*, by its own clock in the Wi-Fi task. The access point stays up while it tries, so a person can still reach the unit; once the station joins, the access point and its DHCP and DNS servers stop, the root page is the portal again (`rest_endpoints_set_root_wizard`), mDNS is announced on the station, once, and an image on trial is told it is on its network. The console's `wifi leave` makes the station leave, as when its router goes away. A test build (`RC_TEST_WIFI_FIRST_JOIN_FAILS`) skips the start's join, as when the router is down at the start.
- **Found:** the first retry waited for the Wi-Fi task's queue to be quiet for 60 s. As its own access point the unit blinks its LED through that queue, which is never quiet that long, so it never tried again; the router leaving while it ran was caught, the router down at the start was not. The retry keeps its own time now.
- **Shown**, each by its own run, 14:13 to 14:24:
  - `wifi leave`: the unit off the network, back as a station at its address 63 s later.
  - The test build: as its own access point 15 s after its start, joined 67 s after it, the root page the portal; then `rc_run.py update` to it, confirmed (`VERDICT PASS`).
  - Then the normal build again: `api_check`, `menu_tour`, `charge_cycle` and the settings compare, each `VERDICT PASS`.

### Where gate 1 and gate 2 stand (2026-11-16, 14:30)

- **On the Pico 2 W,** both are shown, but for what needs the owner's hands: the USB checks (the full install over a unit running a newer image in slot B; the drive appearing without BOOTSEL with both slots blanked) and gate 2's power pulls (during an upload, during a trial). The unit is left on the normal build in slot A.
- **On the Pico W,** nothing has run yet. Built with gate 2's code: the bootloader (6.4 KB of 16 KB), the image for slot A (805.2 KB of 1,016 KB, 210.8 KB left), and the test images `build_w_trial_b` (B, 276.900), `build_w_noconfirm_a` (A, 276.901, never confirms) and `build_w_trial2_a` (A, 276.902). Its first flash of the new layout also needs its settings copied first.

## Gate 3: releases

Approved by the owner 2026-11-16 ("Start gate 3"), while the bench waits for the owner's hands.

### Built, and shown on the PC and the Pico 2 W (1.20.210, built from the working tree on 075bd34)

- **`scripts/build_release.py`** makes a release's five builds into `build_release\` (each board's full install, not marked; the Pico 2 W's update image; the Pico W's for slot A and for slot B), then its folder with `scripts/release_manifest.py`: `<board>_full_install.uf2`, `<name>_update.bin` (named as the unit's `takes`: `pico2_w`, `pico_w_a`, `pico_w_b`), `<board>_erase.uf2`, `manifest.json`, `SHA256SUMS` and `RELEASE_NOTES.md`, the draft's text. CI runs the same script on every build (`.github/workflows/cmake.yml`), keeps the folder as the `release` artifact beside each board's package and size report, and on a `v*` tag drafts a GitHub release from it, the tag's subject as its note. A draft only: the owner publishes it after trying it on a unit.
- **The erase** (`erase.uf2`, made by each board's build beside its full install): the start of flash and of both slots written as erased flash, so nothing boots and the unit comes back as its USB drive. It is the first step back to a release from v1.20 and before: on the Pico 2 W such a release's `app.uf2` would otherwise be written into a slot under the partition table, and which image boots after it is not known.
- **The manuals:** `manuals/firmware_update_via_usb.md` (the first install, by the board's full install; the move from v1.20 and before; a unit brought back) and `manuals/firmware_downgrade.md` (the settings erased at the unit, the erase, the older release's file, the unit set up again as its own access point).
- **Found:** written on Windows, `SHA256SUMS` had CRLF line endings, and `sha256sum -c` read every name with a carriage return in it. The release's text files are written with LF on every system.
- **Shown:** the release built on the PC as CI builds it (14:33): the five images at the same block version (276.210), every file and the manifest checked by `sha256sum -c SHA256SUMS`. `rc_run.py update` from that folder's manifest on the Pico 2 W (1.20.208 to 1.20.210, confirmed, 8.5 s after the install); then the release's full install through the probe (`--elf build_release/pico2_w/app.elf flash`), `menu_tour`, `charge_cycle` and `api_check`: each `VERDICT PASS`.
- **Not yet shown:** CI's run of the release step (on the PR's first push), a tag's draft release (the owner's tag), the full install and the erase by USB, and each manual followed by someone who did not write it; the Pico W's updates from the release's manifest.

## The review (2026-11-16)

A second model (Fable) read the three gates' code at 7aff78e; its report is `logs/adr004/code_review_2026-11-16.md` (this PC only). Each finding was then read again in the code (Opus), and each holds. All are fixed (below, "The review's fixes"). In order of harm:

- **Device:**
  - **The upload has no owner** (the report's Q1, raised here). The server takes an upload on each of its six connections, and `update_image_begin` sets the job's counters before it checks the size. Two uploads open at once, then a third PUT refused as too large, leave the limit larger than the slot, and the two go on writing past the free slot's end: on the Pico W into the running slot, or past the end of flash and so around to the bootloader. Read, not shown.
  - **D1:** the image parser checks that a block's link stays inside the slot, not that it is a multiple of 4. The RP2040 faults on an unaligned read, and the bootloader reads both slots before it picks: one damaged block in either slot, and neither boots until BOOTSEL and a full install.
  - **D2:** an update is taken while the running image is still on trial. The free slot is then the confirmed image the unit would go back to; its first sector is erased by the upload, and when the trial ends neither slot boots. Both chips. The window: a trial already failing (fallen back to its own access point) and a client starting an update within its minute.
  - **D3:** when the station joins from the fallback access point, its DHCP and DNS servers are removed without lwIP's lock, while the other core may be answering a query on them. Microseconds, once per such join.
- **Contract:**
  - **C1:** every update event and `last` point at one buffer that the next job rewrites, so an old `confirmed` event later shows another version.
  - **C2:** the Wi-Fi settings say `at_restart`, but the retry joins with what a PATCH wrote, saved or not. The wizard saves and restarts at once, so it rarely matters.
  - **C3:**
    - `received` is rounded up to a sector once the image is whole;
    - a stop while `installing` answers 202 and stops nothing;
    - `POST /api/v1/stop` before the menu takes a start leaves the job `starting`, and later starts get 409 until someone reads `GET /api/v1/update`;
    - a restart into a trial that was refused leaves its note, and the next start reports `back` for a trial that never ran;
    - `GET /api/v1` names an update `receiving` while it is `installing`.
- **Quality:**
  - **Q2:** the job's idle time can come out negative on a race, and a good upload ends by `time`. Rare.
  - **Q3:** the whole-image check trusts the image's own lengths and pointers, so a made-up image can fault the server's task. Same cause as D1.
  - **Q4:** a three-part tag (`v1.21.1`; `v1.5.1` is in the history) gives the version `unknown` and block 0.0, and CI still drafts the release.
  - **Q5:** `slot_trial --expect refused` takes any refusal; `update --expect confirmed` compares the running block with a figure the firmware copies from itself; nothing shows the bootloader's own trial watchdog, which only an image that hangs before `update_init` would.
- **Known, and kept so by the owner** (2026-11-16; README, "Not in this ADR"): an update is not authenticated. The hash shows the image is whole, not whose it is, and the API has no login, so anyone on the unit's network can install an image they built and sealed themselves. That is why D1, Q1 and Q3 matter as accidents, and for the bootloader's promise, more than as attacks.

### The review's fixes (1.20.212, built from the working tree on 7aff78e)

The owner: "Fix them now", in one pass, proven on the Pico 2 W.

- **Built:**
  - **The image's own figures are held before they are used:** a block that does not lie on a word is no block (`picobin_image.c`), so the bootloader takes that slot as empty; the whole check holds each load map entry inside its item and the slot without wrapping, the hashed words inside the slot, and the binary information's pointers on a word and inside it (D1, Q3).
  - **One upload at a time** (Q1): an upload holds the job by a token, kept in its request's context; a second is 409 `busy`, and a piece, end or abort of any other changes nothing, nor does one left from a job that has ended. The size is checked before anything of the job changes, and the sector's write refuses anything past the free slot, where flash is written.
  - **No update while the image running is on trial** (D2): 409 `busy` until it is confirmed, the console's `update try` too.
  - The access point's DHCP and DNS servers are started and removed under lwIP's lock (D3).
  - An event and `last` keep an image's block version as a number, written as text only in a reply (C1). The Wi-Fi's retry and the trial's check of its network read the settings the unit started with (C2).
  - The small ones (C3):
    - `received` is the image's bytes taken, 0 while idle;
    - a stop while `installing` is 409 `busy`;
    - `starting` is read from the start's request itself, so a start dropped or stopped is idle again at once;
    - a restart into a trial that was refused clears its note;
    - `GET /api/v1` names the update's state as `GET /api/v1/update` does.
  - The idle time is signed (Q2). An update image with no version stops CMake: a release's tag is `vMAJOR.MINOR` (Q4).
  - `rc_run.py` (Q5):
    - `slot_trial --corrupt` wants the hash's refusal;
    - `update` judges by the block version of the image it sent;
    - `--expect back` has a start during the trial refused;
    - `--expect second` sends a second upload while the first is open.
  - A test image that hangs at its first line (`-DRC_TEST_HANG_AT_START=ON`).
  - `api_v1.md` says each new refusal.
- **Shown on the Pico 2 W**, each `VERDICT PASS`, 15:28 to 15:34:
  - **The Wi-Fi's retry, under the lock:** the build that skips its start's join came up as its own access point, then joined 60 s after its start, the access point stopped.
  - **second:** half the image sent; a second upload 409 `busy`, one larger than the slot 413; then the first taken whole (`received` 801,540, its length), and stopped, nothing installed.
  - **back:** an image that never confirms: back on the network 8.4 s after the install, a start then 409 `busy` ("on trial"), back on the image before 75.4 s after the install, `last` naming the image tried (1.20.901).
  - **back by the ROM's watchdog:** the image that hangs at its first line: back on the image before 26.4 s after the install. On the Pico W the same image shows the bootloader's watchdog.
  - **confirmed:** 1.20.900 sent in 14.2 s, confirmed 8.5 s after the install, the block running and `last` both the image sent.
  - Then the normal build through the probe, 15:34 to 15:36: `menu_tour` against the Pico 2 W's baseline, `api_check` (32 paths read twice, 24 of 24 actions), `slot_trial --corrupt` refused by its hash (`hash_mismatch`), `charge_cycle`, and the settings compared with the copy taken before the first flash.
- **Left as they were:** the RP2350's confirmation holds both cores' interrupts for a sector's erase and program, once, a few seconds after a trial's start (the report's Q6: known, no harm seen). The Pico W's fault images (`build_w_fault1`, `build_w_fault2`) are from before the layout and are built again before `pulses` runs on it.
- **Not shown:** on the Pico W, all of it (it is read and built, not run), and the bootloader's own watchdog with the image that hangs.

### The manifest's block version, and the wrong slot told in the first 4 KB (2026-11-16)

The owner's later direction (README, "Owner's direction") puts a file uploaded by hand on the web page, beside the offer. On the Pico W the file it takes changes with its free slot after every update, so a wrong one is the likeliest mistake.

- **Built:**
  - each update image's entry in `manifest.json` gives its block version, read from the image itself and written as `running.block` is, so a unit knows whether a release is newer before it fetches it. The script refuses an image with none; `rc_run.py update` checks the figure against the image it sends;
  - the Pico W refuses an image linked for the other slot in its first 4 KB, as it refuses another chip's. The first sector holds the image's vector table, and its reset handler lies in the slot it is linked for. The bootloader's rule for a bootable image and the update's are one function (`update_boot_vectors_fit`, `update_boot.h`); the whole check's look at the load map stays. The Pico 2 W's images run from either slot.
- **Checked on the PC:** the images on disk for slot A start at 0x10004213, those for slot B at 0x10102213; the rule takes each for its own slot only. Both boards build with no warnings; the bootloader is 6,488 bytes.
- **Not shown:** on the Pico W, the refusal within the first 4 KB (`update --expect refused --refused-code wrong_slot`, with the other slot's image), with its other runs.

## The Pico W, shown 2026-11-16 (1.20.217, built on c1c7962; then the release, 1.20.218 at 59ec8a6)

The owner put the Pico W on the probe at 18:40; gates 1 and 2 ran on it together, then the release. Its address is now 192.168.2.168 (DHCP; .162 before).

- **Before the first flash:** its settings copied over the web, 16 resources at 1.20.194 (`logs/adr004/picow/before_adr004/`), in `rc_run.py settings`' form but by hand: a session needs the running image's ELF for its console, and 1.20.194's is not kept.
- **Gate 1, through the probe** (`flash`, then `slot_trial`), each `VERDICT PASS`, 18:43 to 18:48:
  - `flash`: the bootloader, the start of slot B blanked, 1.20.217 in A;
  - **confirmed, into B** (276.900 over 276.217): on trial, confirmed 4.8 s after the trial was asked, still B after a restart;
  - **back** (276.901, never confirming, into A over B): fed for its minute, then the bootloader's watchdog (8 s) restarted the unit: back on B 68.2 s after the trial was asked;
  - **refused:** the same image with one byte changed, refused before any restart by its hash (`hash_mismatch`); the check took 4.9 s;
  - **confirmed, into A** (276.902 over 276.900): 4.7 s, kept after a restart;
  - **the full install wins:** `flash` of 1.20.217 over the two newer images booted it from A.
- **Gate 2, over Wi-Fi** (`update`), 18:48 to 18:54:
  - **wrong slot:** a slot A image (276.902) given where the unit takes `pico_w_b`: 400 `wrong_slot` 0.1 s into the body, the job ended, nothing installed, the settings unchanged (`VERDICT PASS`);
  - **second:** half the image sent; a second upload 409 `busy`, one larger than the slot 413; the first then taken whole (826,108 bytes), and stopped, nothing installed (`VERDICT PASS`);
  - **back by the bootloader's watchdog:** the image that hangs at its first line (276.903; 40 KB, as nothing after its loop is linked): back on 1.20.217 in A and on the network 14.5 s after the install, `last` back naming 1.20.903 (`VERDICT PASS`). Only the watchdog the bootloader arms could restart it;
  - **confirmed, into B, with two browsers on the pages:** confirmed 8.5 s after the install, but `VERDICT FAIL`: 4 of 98 web reads missed (the limit is 1%), each of the four pollers once, all together in the upload's last 5 s (found, below);
  - **back** (276.901 into A over B at 276.900): a start during the trial 409 `busy` (the review's D2), back 74.3 s after the install (`VERDICT PASS`);
  - **confirmed, into A** (276.902 over 276.900): 8.5 s after the install (`VERDICT PASS`).
- **Found:**
  - **The image's check took 5 s on the Pico W,** in the web server's task, which answered no one meanwhile: the browsers, the event log's held reads, the upload's own reply. The firmware is built at `-O0` (the root `CMakeLists.txt`, since the template; ADR-005 lists it), and mbedTLS with it. SHA-256 alone is now compiled at `-O2`: the check takes 1.4 s (`update try` with a changed byte), the image is 6.4 KB smaller, and the confirmed update with two browsers passed, 0 of 92 reads missed, the longest 1.7 s, the image sent in 16.3 s instead of 20.5 s, confirmed 8.4 s after the install. The build's `-O0` itself is ADR-005's (handed on there).
  - **`rc_run.py` drove another session's openocd.** A session that died at its start (an ELF with no console) left its openocd running. The next session's own openocd found no free probe and quit, but its connection to the Tcl port had reached the one left behind, which flashed the unit while the session reported no write verified. A session now refuses to start while the Tcl port is taken, and a run that raises fails with its traceback in the log and stops its openocd. Seen once more before the second fix: an update's process ended at its start with no output kept and its openocd left behind; why is not known, and such a run now ends in `VERDICT FAIL` with its traceback.
- **The normal build again** (SHA-256 at `-O2`, 1.20.217 from the working tree), each `VERDICT PASS`, 18:58 to 19:00: `slot_trial --corrupt` (`hash_mismatch`), `menu_tour` against `tests/menu_baseline/pico_w` (22 forms; the version form differs, not judged), `api_check` (32 paths read twice, 24 of 24 actions), `charge_cycle` (30.000 for 30), and the settings compared with the copy taken before the first flash: the same.
- **Wi-Fi joins again** (the review's C2 and D3, on the RP2040): the build that skips its start's join tried again 59 s after its start and joined 3 s later; the root page was the portal again (`VERDICT PASS`). `flash`'s check of the version failed on that build, by its known rule: it expects what the last build wrote into the shared `src/generated/version.c`, and the last build was another.
- **The release** (`scripts/build_release.py` at 59ec8a6, 1.20.218; every file checked by `sha256sum -c SHA256SUMS`), each `VERDICT PASS`, 19:04 to 19:07: `update --manifest release/manifest.json` from 1.20.217 in A, the unit taking `pico_w_b_update.bin` (819,692 bytes) in 12.9 s and confirmed 8.5 s after the install; then the release's full install through the probe (`--elf build_release/pico_w/app.elf flash`) over the newer slot B, which booted 1.20.218 from A; then `menu_tour`, `charge_cycle` (30.000 for 30), `api_check` and the settings compare.
- Size: the Pico W image 799.3 KB of its 1,016 KB slot (216.7 KB left), the Pico 2 W's 775.4 KB of 1,792 KB; the bootloader 6,488 bytes.
- **Not shown:** the USB checks on both units; the power pulls; `pulses` on the Pico W's new layout (its fault images are from before it).

## The Pico 2 W again, SHA-256 at `-O2`, shown 2026-11-16 (the release, 1.20.218 at 59ec8a6)

The owner put the Pico 2 W back on the probe at 19:30, for the build's change found on the Pico W. Each `VERDICT PASS`, 19:34 to 19:38:

- its settings copied first (`logs/adr004/pico2w/before_o2/`, at 1.20.212);
- `update --manifest release/manifest.json`: 1.20.212 to 1.20.218, the image sent in 14.0 s, confirmed 8.4 s after the install, now in slot B;
- `slot_trial --corrupt` on 1.20.218: refused by its hash (`hash_mismatch`), the check taking 0.8 s; the same run on 1.20.212 at `-O0` took 3.2 s (15:35). The Pico 2 W's web had stalled too, under the browsers' 5 s timeout;
- a confirmed update with two browsers on the pages (1.20.900): 0 of 88 web reads missed, the longest 0.53 s, confirmed 8.4 s after the install;
- the release's full install through the probe over the newer slot A image, which booted 1.20.218 from A; then `menu_tour` against `tests/menu_baseline/pico2_w` (the version form differs, not judged), `charge_cycle` (30.000 for 30), `api_check` (32 paths read twice, 24 of 24 actions) and the settings compared with the copy: the same.

## Gate 3's release candidates on GitHub, built (2026-11-16)

Approved by the owner as written ("Approve, build it"; README, "Release candidates on GitHub"), with each file's name carrying its version.

- **Built** (dc6db4f, 8f4d4a4):
  - **versions of three numbers** (`scripts/gen_version.py`): `X.Y.Z` at a tag `vX.Y.Z`, `X.Y.Z-N` a build N commits after it. A tag `vX.Y` counts as `X.Y.0`, so this branch's builds are 1.20.0-N and keep their block numbers (276, N). The metadata block carries `X × 256 + Y` and `Z × 256 + N`, N held at 255. The firmware writes it as text the same way (`running.block`, `last.version`), as `release_manifest.py` and `rc_run.py` do;
  - **each release file named with its version** (`OpenTrickler-<version>-...`); `manifest.json` and `SHA256SUMS` keep their names. On a tag's release each manifest entry also gives its address (`url`);
  - **`build_release.py --tag vX.Y.Z`** refuses a tag without its notes (`docs/releases/<tag>.md`, `<tag>_note.txt`) and a firmware that does not report the tag; it takes the manifest's note from the note file and gives the addresses. CI runs it on a tag, then drafts a pre-release: the notes, the SHA-256 in its text, every file. The generated `RELEASE_NOTES.md`, which gave install steps, is gone;
  - **`docs/releases/`:** its README (the steps, as the Open Weapon Mount Computer's), `v2.0.0.md` and `v2.0.0_note.txt` (a test build, not for use, no install steps). The manuals name a file by the end of its name;
  - **`rc_run.py update --manifest <address>`:** the manifest, and the file the unit takes, fetched by their addresses (an entry's `url`, or beside the manifest).
- **Found:** `release_manifest.py` read no image's version after the change: the version's two numbers had been named as the first block's offset, so its block loop never found its way back. Its own refusal of an image with no version stopped both release builds; fixed in 8f4d4a4.
- **Shown on the PC:**
  - `build_release.py --tag v2.0.1` refused before building (`docs\releases\v2.0.1.md is missing`); `--tag v2.0.0` with no such tag refused (`the firmware reports 1.20.0-222, not the tag v2.0.0`);
  - at a local tag `v2.0.0`, deleted once its build ended and never pushed: the firmware reported 2.0.0; seven files each named `OpenTrickler-2.0.0-...`, `manifest.json` (version 2.0.0, its note from the note file, the three update images at block 2.0.0, every entry's address under `releases/download/v2.0.0/`) and `SHA256SUMS`, all eight checked by `sha256sum -c`;
  - CI's step for the page's text, run with the v2.0.0 notes: the title dropped, the SHA-256 appended, links into the repository pinned to the tag.
- **Shown on the Pico 2 W** (`VERDICT PASS`, 20:09): `rc_run.py update --manifest http://127.0.0.1:8765/manifest.json`, the release's folder served from the PC: from 1.20.218 to 1.20.0-222 (`OpenTrickler-1.20.0-222-pico2_w_update.bin`), confirmed 9.4 s after the install, the unit giving `version` and `block` alike, 1.20.0-222.
- **The first candidate's tag** (`v2.0.0` on 21c5117, pushed on the owner's word, 2026-11-16 20:11): CI's run 43822090334 passed every step and drafted "OpenTrickler v2.0.0" as a pre-release, with the notes as written, the SHA-256 in its text, and nine files. Downloaded from the draft (`gh release download`): all eight sums hold, and each manifest entry's size, SHA-256 and address (under `releases/download/v2.0.0/`) match its file; the three update images carry block 2.0.0; the note is the note file's. Without a login both `releases/download/v2.0.0/manifest.json` (a draft) and `releases/latest/download/manifest.json` (v1.20, which has none) answer 404.
- **Published by the owner as a pre-release** (20:46). Without a login, the manifest and the files answer 200 through one redirect to GitHub's download host (`release-assets.githubusercontent.com`); `releases/latest` still leads to v1.20, so `releases/latest/download/manifest.json` answers 404 and a unit's default source never sees the candidate.
- **The Pico 2 W from the release's address** (`VERDICT PASS`, 20:48): `rc_run.py update --manifest https://github.com/eamars/OpenTrickler-RP2040-Controller/releases/download/v2.0.0/manifest.json`, from 1.20.0-222 in B: `OpenTrickler-2.0.0-pico2_w_update.bin` fetched by its `url`, sent in 12.6 s, confirmed 8.4 s after the install; the unit runs 2.0.0 from A, `version` and `block` both 2.0.0.
- **The Pico W from the release's address** (`VERDICT PASS`, 20:57), from 1.20.218 in A: it took `OpenTrickler-2.0.0-pico_w_b_update.bin`, the image for its free slot, sent in 13.3 s, confirmed 8.8 s after the install; it runs 2.0.0 from B and now takes `pico_w_a`.
- **Not yet shown:** the owner's USB checks with the release's files.

