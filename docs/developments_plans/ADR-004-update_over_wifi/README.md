# ADR-004: firmware update over Wi-Fi

Status: **Draft** 2026-11-13, written in full 2026-11-16 from the owner's direction that day, for approval. Not approved.

## What we want

A person who owns a unit is offered an update and says yes, on the web page or at the unit's screen. They need not know where firmware comes from, which file is theirs, or that the power must stay on. Whatever an update does, USB brings a unit back.

This ADR builds what that rests on: the flash layout, the boot path, the image's format, the files a release carries, and the update through the API. The offer itself comes with the new web UI (ADR-008) and the new screens (ADR-010). Until then an update is reached through the REST API only, for debugging, and is not for users.

## Owner's direction

**2026-11-13**
- Implement OTA.
- USB always recovers a unit.
- One implementation. The part that touches the chip may differ between chips; nothing above it may.

**2026-11-16**, on the first draft's upload from the web page:
- It is "too prosumer": a user would have to know where to get the firmware, how to load it, not to cut the power during an update, and not to load the wrong board's file.
- The update is offered, as the Open Weapon Mount Computer's OTA mode offers it (its `main/services/ota_mode.c`), from the web interface or from the screen.
- The web UI and LVGL come first (ADR-008, ADR-010). Until then, OTA through the REST API only, for debugging: "not production ready until the UI is released".
- The RP2040 should load a binary of the same format as the RP2350, so the application's format does not differ by target.
- The ESP32 (ADR-007) is outside this ADR.
- A downgrade is a separate feature. EEPROM compatibility cannot be promised, so a downgrade is a full erase.
- The first install is a bundle on GitHub, the bootloader and the application together.
- PR #111 is a good reference; it is not merged if it does not fit the roadmap.

## Why here

It changes the flash layout, which costs every user one install by cable. Do that once, before the large changes, and every release after it can reach units over Wi-Fi. It comes after ADR-003 because the upload needs a server that takes a body in pieces, and before ADR-005 because a board's room is only known once its layout is.

## What we know

### Room (measured 2026-11-16, `tools/size_report.py`, Debug images at bfeb8bf)

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 2 MB | 4 MB |
| Image today | 775.8 KB | 751.6 KB |
| Of that, the radio's firmware (checked 2026-11-13) | 232 KB | 232 KB |

Settings live in the EEPROM on the controller board, not in the chip's flash. A new flash layout does not touch them.

### What the chips give (read in the SDK 2.3.1, `pico/bootrom.h` and `pico_crt0`, 2026-11-16)

- **Both:** the USB loader is in ROM: the BOOTSEL button, or `reset_usb_boot()` from code. Nothing written to flash can remove it.
- **The RP2350's boot ROM** reads a partition table at the start of flash and keeps two image slots, A and B. Of two good images it boots the better by their versions. An image marked "try before you buy" is booted once, on a restart that asks for it, and must confirm itself (`rom_explicit_buy`) or the ROM goes back to the other at the next restart. The ROM maps a slot to the start of flash's address before it runs the image there, so one binary serves both slots (the RP2350's datasheet; not yet seen on the chip, which gate 1 does).
- **The RP2040's ROM** has none of this. It starts whatever is at the start of flash, through the 256 bytes of boot2 there. An image runs only at the address it was linked for.
- **The same image format on both** (checked 2026-11-16). The SDK's start-up code is the same on both chips. It writes a metadata block into the image, which the RP2350's ROM reads: the image's type and chip, its version, and the try-before-you-buy mark. On the RP2040 it is off by default and turned on by one setting (`PICO_CRT0_INCLUDE_PICOBIN_IMAGE_TYPE_ITEM`). A Pico W image built with it, a version and the mark, was read by picotool as an image definition for the RP2040 at version 1.20; `picotool seal --hash` added its SHA-256, which picotool reported verified. That image was built, not run.

### The version

`scripts/gen_version.py` names a build by `git describe`: a tag `v1.20` gives 1.20.0, and each commit after it adds one to the last number (1.20.199 today). The metadata block holds two 16-bit numbers. So the block carries the release as `major × 256 + minor` and the commits since its tag as the second, which orders every build on a branch: 1.20.199 is (276, 199).

### How the Open Weapon Mount Computer does it (its ADR-015 and `ota_mode.c`, read 2026-11-16)

- Every release carries a `manifest.json`: the version, a note, an importance (normal or critical) and the image's address.
- Once each start, after Wi-Fi joins, the unit reads the manifest of the repository's latest release, over HTTPS; a host on the bench (`ota_host.py`) is asked first. A newer version is offered on the screen, at once when critical; Install downloads it.
- It runs on an ESP32-S3 with PSRAM, where TLS is cheap. It has no rollback: a bad image needs USB.

### PR #111 (read at its head, d1e99e9, 2026-11-16)

- A 64 KB bootloader at the start of flash, the application after it (960 KB on the Pico W), the upper half as a staging area.
- The browser uploads an image into the staging area, with a footer of its own: a magic number, the board, the size, a CRC32 of the image and one of the footer. "Install" marks it pending and restarts; the bootloader copies it over the application, then clears the mark. A power cut during the copy leaves the staged image whole, and the copy starts again.
- Its bootloader checks the footer (magic, board, footer CRC) but not the staged image's CRC before copying, and does not read the copy back.
- Its author's lessons: the running image never writes its own region (an application copying over itself, under FreeRTOS with the second core running, bricked test hardware); and before jumping into the application on the RP2040, every peripheral but the flash's is reset, or the radio's start fails after a copy.
- Its upload is written for `http_rest.c`, which ADR-003 removed.
- From the first draft's reading (2026-11-13): its Pico 2 W build fails in CI, and its combined `.uf2` (bootloader and application in one file) failed on hardware for a reason not found.
- PR #28 (picowota, 2023) and PR #44 (the owner's two banks, 2024) came before it; both are closed, pointing here.

### The server (ADR-003, merged 2026-11-16 by #129)

- A body is taken in pieces as it comes, for this upload (`http_server_add_upload`): 700 KB uploads, each whole by its CRC-32, back to back during a soak's charges.
- api_v1.md keeps a place for it: `/api/v1/update` as a job, its image a `PUT` of `application/octet-stream`, and an install.

### Handed on

- **By ADR-012:** a unit that starts before its router stays off Wi-Fi. It tries to join once, at start-up; when that fails it becomes its own access point until it is restarted (`wireless.c`). An update over Wi-Fi needs a unit on Wi-Fi.
- **By ADR-003:** a tuning session's load and an upload at once overrun the Pico W's network (31% of the web reads missed, the charges untouched). This ADR decides when an upload is taken.
- **By ADR-002:** PR #111 ends here, once this ADR has settled its own design: merged from its head, taken in parts with credit, or closed with thanks.
- **By ADR-011:** `picotool info` gives no firmware version for an image. The metadata block above carries one.

### CI

CI builds both boards on a push to main and on every pull request, and keeps each build's files as an artifact (`.github/workflows/cmake.yml`). No job makes a release, and none runs on a tag.

## What we do

### Two slots on both chips

Each chip keeps two image slots. The image running is in one; an update is written into the other while it runs; a restart tries the new one; the new one confirms itself or the unit goes back to the old. A power cut at any moment leaves an image that boots. The rules are the RP2350 ROM's, and the RP2040 gets a small bootloader of ours that applies the same:
- of the two slots, the good image with the higher version boots;
- an image marked "try before you buy" boots only when the running image asks for a trial, once;
- an image on trial that does not confirm itself in time is left: the watchdog restarts the unit, and the other image boots;
- a USB install always wins (below).

**On the RP2040** the bootloader reads both slots' metadata blocks, picks by the rules, checks the SHA-256 of an image it boots on trial, and jumps into it as PR #111's does (every peripheral but the flash's reset first). The running image asks for a trial through one of the watchdog's scratch registers, which a watchdog restart keeps and a power cut clears; the bootloader arms the watchdog before it jumps into an image on trial. An image confirms itself by clearing the mark in its own last metadata block: a write that only clears bits, so nothing is erased. As picotool sealed the image on 2026-11-16, that block lies after the last range its hash covers, so the hash still holds; gate 1 shows it on the chip. **The bootloader never writes flash.** With no good image in either slot it starts the USB loader, so a unit with nothing to boot shows up as a drive without BOOTSEL. It is ours for good: it changes only by USB, so it does only this.

**The one difference left:** the RP2040 cannot run one binary from either slot. Each Pico W release is linked twice, once for slot A and once for slot B. The release's manifest names both, and the unit takes the one for its free slot. Nobody chooses between them.

**On the RP2350** the ROM does all of it: the partition table, the choice, the trial and its watchdog, the confirmation. The application calls the ROM to learn its free slot, to restart into a trial (`REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE`), and to confirm.

**Why not PR #111's copy.** The room is the same, because a staging area is as large as a slot. A copy rewrites the application at every update, and the old image is gone once it is copied. Two slots leave the bootloader nothing to write, give both chips the way back, and make the two chips behave alike.

**When an image confirms itself:** once it has started, joined the network (an address from DHCP) and its web server listens. A version that cannot reach the network is never kept, since the next update would need USB. While it waits for the network it keeps the trial's watchdog fed, up to a minute *p*.

### The layouts

**Pico W (2 MB):**

| From | Size | What |
|---|---|---|
| `0x10000000` | 32 KB | The bootloader, boot2 in its first 256 bytes. Its size is measured in gate 1 before this line is fixed |
| `0x10008000` | 1,008 KB | Slot A |
| `0x10104000` | 1,008 KB | Slot B |

Today's image leaves 232 KB in a slot (PR #111's layout left 184 KB). LVGL (ADR-010) is what is likeliest to fill it.

**Pico 2 W (4 MB):**

| From | Size | What |
|---|---|---|
| `0x10000000` | 8 KB | The partition table, read by the ROM. Its size is checked in gate 1 against what the ROM reads |
| `0x10002000` | 1,792 KB | Slot A |
| `0x101C2000` | 1,792 KB | Slot B |
| `0x10382000` | 504 KB | Spare: named in the table now, used by nothing yet |

The spare part costs nothing today and keeps a later need from changing the table, which would cost every user an install by cable.

### The image

- **One format on both chips:** the SDK's own image, with the metadata block the start-up code writes (its chip, its version as above, the try-before-you-buy mark), sealed by `picotool seal --hash`. No footer of ours.
- **The board** is named in the image's binary information (`pico_board`), and `picotool info` shows it.
- **An update image** carries the mark. The full install's image (below) has it cleared, so it boots at once.
- **The linker refuses an image larger than its slot,** so a build that does not fit fails.

### The files a release carries

For each board:
- **the full install,** one `.uf2`: the bootloader (Pico W) or the partition table (Pico 2 W), the image in slot A with its mark cleared, and the start of slot B blanked;
- **the update images:** the Pico W's for slot A and for slot B, the Pico 2 W's one;
- and once for the release, **`manifest.json`:** the version, a note, an importance (normal, critical), and for each board and slot its file, size and SHA-256. The offer (ADR-008, ADR-010) reads it; until then `rc_run.py` does.

The start of slot B is blanked because the higher version boots: an older or equal image put in slot A by USB would otherwise lose to a newer one left in B. So a USB install always wins, and the full install is also how a unit is brought back.

**The first install** is today's manual (`manuals/firmware_update_via_usb.md`, BOOTSEL and one file), with the full install as its file. The settings in the EEPROM stay, unless a stored struct changed. Why PR #111's combined `.uf2` failed is found in gate 1, before ours is trusted. The first place to look: every block of a `.uf2` carries the file's block count, and a merged file that keeps each part's own count may end the copy after the first part. That is a guess to check, not a finding.

**A downgrade** is not an update. Over the network the unit takes only a newer version. To go back, a person erases the settings on the running firmware (the menu, or `POST /api/v1/system/erase`), then installs the older release's full install by USB. Its manual is written in gate 3.

### The update through the API

- **A job,** with the shape every job has (api_v1.md): `/api/v1/update` with its `state`; `POST .../start`, `PUT .../image`, `POST .../install`, `POST .../stop`; `activity` is `update` while it runs; its events in the event log. It is refused while another job runs, and no other job starts while it runs. That settles ADR-003's overrun: an upload and a tuning session never run together.
- **`GET /api/v1/update`** says what the unit runs (version, slot), what it takes (its board and free slot: the manifest's name for the file), and where an update stands.
- **The image** is written into the free slot as it comes, a 4 KB sector at a time, with the SDK's `flash_safe_execute`, which keeps the other core out of flash while it writes. It is checked as it comes and once whole: its chip, its board, its slot's address, its size, a version newer than the running one, its SHA-256. A wrong one is refused with a code of its own and never installed.
- **Install** restarts into a trial. The unit comes back on the new version, confirmed, or on the old one, with an event that says which.
- **Wi-Fi joins again** when its network comes back, so a unit that started before its router can be updated (ADR-012's hand-on).
- **`rc_run.py update --host <address> --manifest <path or address>`** reads a manifest, takes the file the unit asks for, and passes when the unit reports the new version confirmed. It plays the offer's part until the UIs come, as `ota_host.py` does on the Open Weapon Mount Computer's bench.

### Gates

**Gate 1: the layout and the boot path, both boards.**
- The RP2040 bootloader; the Pico 2 W's partition table; the metadata block and the version in every image; the Pico W linked for each slot; the full install for each board.
- The probe's flashing (`rc_run.py flash`) learns the layout; a console command asks for a trial of what is in the other slot, which the probe writes directly. No image comes over the network in this gate.
- Why PR #111's combined `.uf2` failed.

**Gate 2: the update through the API.** The job, the checks, the trial and its confirmation, Wi-Fi joining again, `rc_run.py update`.

**Gate 3: releases.** CI on a version tag drafts a release with each board's files, the manifest and their SHA-256, as the Open Weapon Mount Computer's does; the owner publishes it after trying it on a unit. The first install's manual and the downgrade's, each tried by someone following only the manual.

## How the unit shows it works

Each on the Pico W and the Pico 2 W.

**Gate 1**
- A full install through the probe boots slot A; `menu_tour` and `charge_cycle` pass; the settings are as they were (`settings --compare`).
- An image written into slot B through the probe, and a trial asked: B boots and confirms itself, and stays after the next restart.
- An image that never confirms itself (a test build): the unit is back on A after the watchdog.
- An image in B with one byte changed: not booted.
- Both slots blanked: the unit starts the USB loader by itself (the owner sees the drive).
- The owner installs the full install by USB on a unit running B: A boots.
- The size report shows each image inside its slot.

**Gate 2**
- `rc_run.py update` from a local build's manifest: the unit comes back on the new version, confirmed, with its settings as they were; `menu_tour` and `charge_cycle` pass after.
- Refused, each with its code and nothing installed: another board's image, the other slot's, a cut-off upload, a changed byte, an older version, an update during a charge (409).
- The owner pulls the power during an upload, and during a trial: each time the unit comes back on its old image.
- A unit started with its network off joins when the network comes back, and can then be updated.
- An upload while two browsers use the web pages: the pages go on answering, and lwIP's heap holds (`net`).

**Gate 3**
- A tag's draft release carries every file, and its manifest names each with a SHA-256 that matches.
- A unit on v1.20 moved by the manual alone, by someone who did not write it.
- `rc_run.py update` from the draft release's manifest on both units.

## Not in this ADR

- **The offer:** on the web page (ADR-008) and on the screen (ADR-010). Until then an update is for debugging, through the API.
- **Where the offer learns of a release.** The Open Weapon Mount Computer reads its manifest over HTTPS. On the Pico W, TLS takes flash and RAM that ADR-005 would measure. A browser could fetch the manifest and the image for the unit, which needs no TLS on either board; the screen alone needs the unit's own. Decided with ADR-008 and ADR-010; this ADR only puts the manifest in every release.
- **A downgrade over the network,** and keeping settings across one.
- **Signed images,** and updating the bootloader over Wi-Fi.
- **The ESP32 (ADR-007).**

## To settle before approval

- **The layouts' numbers:** a 32 KB budget for the bootloader, then fixed by what it measures; the Pico 2 W's 504 KB spare.
- **When an image confirms itself:** once it has joined the network and its web server listens, as above.
- **PR #111:** not merged. Its RP2040 jump is taken, with a `Co-authored-by:` line for its author, and the PR is closed with thanks and a note that names what was taken, on the owner's word, once gate 1 is merged.
