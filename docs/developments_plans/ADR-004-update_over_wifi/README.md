# ADR-004: firmware update over Wi-Fi

Status: **Open** 2026-11-16. Drafted 2026-11-13, written in full 2026-11-16 from the owner's direction that day, and approved by the owner the same day as written ("Approve as written"), starting with gate 1 on the Pico 2 W. Gate 3's release candidates on GitHub were added later that day from the owner's direction, for the owner's approval ("Release candidates on GitHub"). What was built and measured is in [progress](progress.md).

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

**2026-11-16**, later, on the offer's hand-on to ADR-008 and ADR-010:
- The web page sets the update source, GitHub's releases by default.
- The web page also takes a firmware file uploaded by hand, for "prosumers": beside the offer, not in its place. The first draft's verdict above stands for the offer itself.
- The screen only polls the source the web set, and offers the update from a row in the main menu, as the Open Weapon Mount Computer does; never a pop-up at every start.
- No authentication for now: the unit has no password, and a PIN would make the screen mandatory. Perhaps an optional feature later.

**2026-11-16**, later, on releases:
- No release for users, and no instructions given out, until every ADR is closed.
- Release candidates (v2.0.x) go out on GitHub first, to try GitHub's side before the final firmware.
- Each release carries a baseline for a first install, the update files for both boards (the Pico W's two, each on its own) and the manifest. Each file's name carries the version.
- GitHub's side is made as the Open Weapon Mount Computer's, so both are released the same way.
- No fetch by the unit until one of the UIs comes, unless it can be tested through the API today.
- The owner's USB checks on both units are made with a candidate's baseline.
- Agreed the same day, on the agent's proposal: a candidate is an ordinary version published as a pre-release, public, its notes saying it is a test build; its tag is pushed on this ADR's branch, before the ADR closes.

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

**On the RP2040** the bootloader reads both slots' metadata blocks, picks by the rules, and jumps into it as PR #111's does (every peripheral but the flash's reset first). The image's SHA-256 is checked by the running image before it asks for a trial, on both chips with the same code (gate 1, "the image checked before it is tried", progress): a trial is only ever asked by the running image, so the bootloader needs no hash code. The running image asks for a trial through one of the watchdog's scratch registers, which a watchdog restart keeps and a power cut clears; the bootloader arms the watchdog before it jumps into an image on trial. An image confirms itself by clearing the mark in its own last metadata block: a write that only clears bits, so nothing is erased. picotool hashes the block with the mark cleared, so the hash still holds once the image is confirmed (its `bintool.cpp`, read 2026-11-16). **The bootloader never writes flash.** With no good image in either slot it starts the USB loader, so a unit with nothing to boot shows up as a drive without BOOTSEL. It is ours for good: it changes only by USB, so it does only this.

**The one difference left:** the RP2040 cannot run one binary from either slot. Each Pico W release is linked twice, once for slot A and once for slot B. The release's manifest names both, and the unit takes the one for its free slot. Nobody chooses between them.

**On the RP2350** the ROM does all of it: the partition table, the choice, the trial and its watchdog, the confirmation. The application calls the ROM to learn its free slot, to restart into a trial (`REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE`), and to confirm.

**Why not PR #111's copy.** The room is the same, because a staging area is as large as a slot. A copy rewrites the application at every update, and the old image is gone once it is copied. Two slots leave the bootloader nothing to write, give both chips the way back, and make the two chips behave alike.

**When an image confirms itself:** once it has started, joined the network (an address from DHCP) and its web server listens. A version that cannot reach the network is never kept, since the next update would need USB. While it waits for the network it keeps the trial's watchdog fed, up to a minute *p*.

### The layouts

**Pico W (2 MB):**

| From | Size | What |
|---|---|---|
| `0x10000000` | 16 KB | The bootloader, boot2 in its first 256 bytes: 6.4 KB measured (gate 1, with the SDK's minimal runtime), from a budget of 32 KB |
| `0x10004000` | 1,016 KB | Slot A |
| `0x10102000` | 1,016 KB | Slot B |

The image with gate 1's update code (793.2 KB, 2026-11-16) leaves 222.8 KB in a slot; PR #111's layout would have left 166.8 KB. LVGL (ADR-010) is what is likeliest to fill it.

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
- and once for the release, **`manifest.json`:** the version, a note, an importance (normal, critical), and for each board and slot its file, size, SHA-256 and block version (written as `GET /api/v1/update` writes `running.block`, read from the image itself), so a unit knows whether it is newer before it fetches it. The offer (ADR-008, ADR-010) reads it; until then `rc_run.py` does.

The start of slot B is blanked because the higher version boots: an older or equal image put in slot A by USB would otherwise lose to a newer one left in B. So a USB install always wins, and the full install is also how a unit is brought back.

**The first install** is today's manual (`manuals/firmware_update_via_usb.md`, BOOTSEL and one file), with the full install as its file. The settings in the EEPROM stay, unless a stored struct changed. Why PR #111's combined `.uf2` failed is looked for in gate 1, before ours is trusted. A first guess, that the merged file kept each part's own block count, is wrong: its script numbers the blocks across the whole file (`scripts/merge_initial_flash_uf2.py` at its head, read 2026-11-16). Ours is judged on the unit, by USB.

**A downgrade** is not an update. Over the network the unit takes only a newer version. To go back, a person erases the settings on the running firmware (the menu, or `POST /api/v1/system/erase`), then installs the older release's full install by USB. Its manual is written in gate 3.

### The update through the API

- **A job,** with the shape every job has (api_v1.md): `/api/v1/update` with its `state`; `POST .../start`, `PUT .../image`, `POST .../install`, `POST .../stop`; `activity` is `update` while it runs; its events in the event log. It is refused while another job runs, and no other job starts while it runs. That settles ADR-003's overrun: an upload and a tuning session never run together. The menu takes it as it takes a charge, and it holds the screen at the unit ("Firmware update", the share received, "Keep the power on"), so nobody starts a charge there during it, and the reset button stops it.
- **`GET /api/v1/update`** says what the unit runs (version, slot), what it takes (its board and free slot: the manifest's name for the file), and where an update stands.
- **The image** is written into the free slot as it comes, a 4 KB sector at a time, with the SDK's `flash_safe_execute`, which keeps the other core out of flash while it writes. It is checked as it comes and once whole: its chip, its board, its slot's address, its size, a version newer than the running one, its SHA-256. A wrong one is refused with a code of its own and never installed.
- **Install** restarts into a trial. The unit comes back on the new version, confirmed, or on the old one, with an event that says which.
- **Wi-Fi joins again** when its network comes back, so a unit that started before its router can be updated (ADR-012's hand-on).
- **`rc_run.py update --host <address> --manifest <path or address>`** reads a manifest, takes the file the unit asks for, and passes when the unit reports the new version confirmed. It plays the offer's part until the UIs come, as `ota_host.py` does on the Open Weapon Mount Computer's bench.

### Release candidates on GitHub (gate 3's addition, 2026-11-16, for the owner's approval)

Nothing goes to users yet. Before the final firmware, release candidates go out on GitHub to try GitHub's side: the files, their addresses, the manifest, and an update from them on both units. The process is the Open Weapon Mount Computer's (its `.github/workflows/firmware.yml` and `docs/releases/README.md`, read 2026-11-16), so the owner releases both projects alike.

- **Tags of three numbers, `vX.Y.Z`,** as the Open Weapon Mount Computer's. The first is `v2.0.0`, a new major version: the flash layout is new, API v1 has replaced the old paths, and the way back to an older release is an erase.
  - A build at a tag is `X.Y.Z`, as the unit shows it; a build N commits after it is `X.Y.Z-N`. An old tag of two numbers counts as `X.Y.0`, so today's build is 1.20.0-218.
  - The metadata block keeps its two 16-bit numbers: `X × 256 + Y`, then `Z × 256 + N`, N held at 255. A release is newer than every build before it, a build after a tag newer than the tag, and today's builds keep the numbers they have now (276 and 218).
- **A candidate is an ordinary version,** published as a GitHub pre-release: `v2.0.0`, `v2.0.1`, and so on. The final is the one the owner promotes by unticking "pre-release": the very files tried, not a rebuild.
  - GitHub's latest release skips pre-releases, so a unit reading `releases/latest/download/manifest.json`, the default source (ADR-008), is never offered a candidate. Today the latest release is v1.20, which has no manifest.
  - Versions only go up, so a unit's "is it newer" needs no rule for candidates.
- **The notes, written by hand,** as the Open Weapon Mount Computer's:
  - `docs/releases/<tag>.md`, the release page's text, and `docs/releases/<tag>_note.txt`, the short note the offer shows (the manifest's `note`), both committed before the tag; `docs/releases/README.md` gives the steps;
  - a candidate's page says it is a test build, not for use, and gives no install steps. The manuals stay in the repository;
  - CI on a tag refuses a firmware that does not report the tag's version, and a tag without its two files.
- **Each release carries** these files, each name with the version as the unit shows it:
  - for a first install, each board's full install: `OpenTrickler-2.0.0-pico_w_full_install.uf2`, `OpenTrickler-2.0.0-pico2_w_full_install.uf2`;
  - the update images, the Pico W's two each on its own: `OpenTrickler-2.0.0-pico2_w_update.bin`, `OpenTrickler-2.0.0-pico_w_a_update.bin`, `OpenTrickler-2.0.0-pico_w_b_update.bin`;
  - each board's erase, for a unit on v1.20 or before and for the way back: `OpenTrickler-2.0.0-pico_w_erase.uf2`, `OpenTrickler-2.0.0-pico2_w_erase.uf2`;
  - `manifest.json` and `SHA256SUMS`, under the same names in every release, so `releases/latest/download/manifest.json` always leads to the latest release's.
  The unit finds its image by the manifest's entry for `takes`, not by a file's name, so the names cost the update nothing. The manuals name a file by the end of its name.
- **The manifest** keeps its fields (`manifest_version`, `version`, `note`, `importance`, the Open Weapon Mount Computer's names) and gives each image an address too, `url`, pinned to its tag (`https://github.com/<repository>/releases/download/<tag>/<file>`), as the Open Weapon Mount Computer's does. `file`, `size`, `sha256` and `block` stay. A build that is not a tag's has no address.
- **CI on a tag** drafts the release as a pre-release, with every file, its notes, and the SHA-256 in its text. The owner publishes it.
- **The unit does not fetch** until a UI comes (the owner): HTTPS on the unit, which must follow GitHub's redirect to another host, is not built, and its cost is not measured. GitHub's side is tried through the API instead. `rc_run.py update --manifest <address>` reads the release's manifest, downloads the file the unit takes from its `url`, checks its size, SHA-256 and block version, and sends it as today: the same files and addresses a unit's own fetch will use.
- **The order:**
  1. built and shown on the PC, with a local tag that is never pushed, for CI's checks and the names;
  2. on the owner's word, the tag `v2.0.0` pushed on this branch's last commit: one push, which CI builds and drafts;
  3. the owner publishes the draft as a pre-release; `rc_run.py update` from its address on both units;
  4. the owner's USB checks and power pulls, with its files;
  5. ADR-004 closes, and its PR is merged with a merge commit, as every PR here is, so the tag stays in main's history. A squash would leave it out, and main's builds would count from v1.20, older than 2.0.0.

### Gates

**Gate 1: the layout and the boot path, both boards.**
- The RP2040 bootloader; the Pico 2 W's partition table; the metadata block and the version in every image; the Pico W linked for each slot; the full install for each board.
- The probe's flashing (`rc_run.py flash`) learns the layout; a console command asks for a trial of what is in the other slot, which the probe writes directly. No image comes over the network in this gate.
- Why PR #111's combined `.uf2` failed.

**Gate 2: the update through the API.** The job, the checks, the trial and its confirmation, Wi-Fi joining again, `rc_run.py update`.

**Gate 3: releases.** CI on a version tag drafts a release with each board's files, the manifest and their SHA-256, as the Open Weapon Mount Computer's does; the owner publishes it after trying it on a unit. The first install's manual and the downgrade's, each tried by someone following only the manual. Its release candidates on GitHub (above), added 2026-11-16 for the owner's approval.

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
- CI refuses a tag whose firmware reports another version, and a tag without its notes: shown on the PC with CI's own check and a local tag.
- A tag's draft release is a pre-release that carries every file, each name with its version, and its manifest names each with a SHA-256 and an address that match.
- Once it is published, `releases/latest/download/manifest.json` does not lead to it.
- `rc_run.py update` from the release's address on both units.
- The owner's USB checks (gate 1's) made with the release's own files.
- A unit on v1.20 moved by the manual alone, by someone who did not write it.

## Not in this ADR

- **The offer:** on the web page (ADR-008) and on the screen (ADR-010). Until then an update is for debugging, through the API. What each has to do is written in its README ("Handed on by ADR-004", 2026-11-16).
- **Where the offer learns of a release.** The Open Weapon Mount Computer reads its manifest over HTTPS. On the Pico W, TLS takes flash and RAM that ADR-005 would measure. The owner's direction (2026-11-16, in ADR-008's and ADR-010's hand-ons): the web page sets the update source, GitHub's releases by default, and also takes a file uploaded by hand; the screen only polls that source, and offers from a row in the main menu, never a pop-up at every start. This ADR only puts the manifest in every release.
- **Authentication:** none (the owner, 2026-11-16). The unit has no password, and a PIN would make the screen mandatory, which some users may not want; perhaps an optional feature later. So anyone on the unit's network can install an image they sealed themselves: the SHA-256 shows an image is whole, not whose it is.
- **The final release, and instructions to users:** once every ADR is closed (the owner, 2026-11-16). Until then only release candidates, as pre-releases.
- **A downgrade over the network,** and keeping settings across one.
- **Signed images,** and updating the bootloader over Wi-Fi.
- **The ESP32 (ADR-007).**

## To settle before approval

- **The layouts' numbers:** a 32 KB budget for the bootloader, then fixed by what it measures; the Pico 2 W's 504 KB spare.
- **When an image confirms itself:** once it has joined the network and its web server listens, as above.
- **PR #111:** not merged. Its RP2040 jump is taken, with a `Co-authored-by:` line for its author, and the PR is closed with thanks and a note that names what was taken, on the owner's word, once gate 1 is merged.
