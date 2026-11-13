# ADR-004: firmware update over Wi-Fi

Status: **Draft** 2026-11-13. Not approved. Captures what three attempts taught and the choices still open.

## What we want

A user installs a release from the browser, with no cable. And whatever an update does, a unit can be brought back by USB.

## Owner's direction (2026-11-13)

- Implement OTA.
- USB always recovers a unit.
- One implementation. The part that touches the chip may differ between chips; nothing above it may.

## Why here

It changes the flash layout, which costs every user one update by cable. Do that once, before the large changes, and the releases after it reach units over Wi-Fi. It comes after ADR-003 because the upload needs a server that takes a body.

## What we know (checked 2026-11-13)

**Room**

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 2 MB | 4 MB |
| Image today (v1.20) | 689 KB | 663 KB |
| Of that, the radio's firmware | 232 KB | 232 KB |

- Settings live in the EEPROM on the controller board, not in the chip's flash. A new flash layout does not touch them.

**What the chips give**
- The USB loader is in ROM on both chips: the BOOTSEL button, or `reset_usb_boot()` from code. Nothing written to flash can remove it.
- The RP2350's boot ROM has two image slots of its own and a trial boot that falls back when the new image does not confirm itself. The SDK in this repo (2.1.1) has the calls for it in `pico/bootrom.h`. The RP2040's ROM has neither.

**Three attempts**
- **PR #28 (2023), picowota.** A bootloader with its own Wi-Fi stack, about 400 KB, because the radio's driver was linked twice. Updates came through a serial flashing tool. The owner asked for the web interface in its place.
- **PR #44 (2024), the owner's.** Two banks with a linker script each and a third boot stage. Not finished.
- **PR #111 (2026).** Its design:
  - 64 KB bootloader at the start of flash, the application after it, the upper half of the chip as a staging area;
  - the browser uploads an image to the staging area; it is checked as it is written (a magic number, two CRC32s, the board type, the size);
  - "install" marks it pending and restarts; the bootloader copies it over the application before anything else runs, then clears the mark. A power cut during the copy leaves the staged image whole, and the copy starts again;
  - its author first let the running application copy over itself, under FreeRTOS with the second core running, and that bricked test hardware. The lesson: the running image never writes its own region.
  - Stated limits: no rollback; the bootloader itself is updated only by USB; one `.uf2` holding the bootloader and the application failed on hardware for reasons not found, so the first move is two USB flashes.
  - It works on its author's Pico W. Its Pico 2 W build fails in CI.
- With that layout the application has about 960 KB on Pico W and about 1,980 KB on Pico 2 W.

## Ideas

**One update interface, the flash part behind it.**
- Above: status (the running version, what is staged), begin, write a piece, finish and check, install. The REST endpoints, the web page and the checks sit here and are the same on every board.
- Below, per chip: where the staged image goes and how it becomes the running one. The ESP32 brings ESP-IDF's two slots with rollback in ADR-007.

**The flash part on the RP chips.** Three ways:

| Way | For | Against |
|---|---|---|
| PR #111's bootloader and staging area on both chips | One thing to own; already works on Pico W | Not yet working on Pico 2 W; no rollback; we own a bootloader |
| The RP2350's own two slots on Pico 2 W, #111's design on Pico W | Rollback on the main board; no bootloader of ours there | Two things to own |
| The RP2350's own two slots, and no update over Wi-Fi on Pico W | Nothing of ours in the boot path | Pico W owners keep the cable |

First view: #111's design on both, tried on the Pico 2 W first. One design is less to own, and USB brings back a unit in every case, so rollback is a comfort and not a need. If it cannot be made to work on the RP2350 for a reason we can name, the second way.

**Refuse at the wrong moment.** No upload and no install while a charge or a cleanup runs.

**Releases.** A CI job builds a tag for every board and attaches, for each, the `.uf2` for USB and the update file. The unit shows its version beside the staged one.

**The first move from v1.20.** One update by USB, written as a manual and tried by someone following only the manual.

## How the unit shows it works

- Upload and install from the browser; the unit restarts on the new version; settings are as they were.
- Power pulled during the copy: the unit finishes the copy at the next start.
- An image for the other board, a cut-off upload, a changed byte: each is refused before anything is installed.
- An image that cannot start is installed on purpose, and the unit is brought back by USB. This is the owner's rule, shown.
- The move from v1.20 by USB, done from the manual.
- `charge_cycle` and `menu_tour` pass after an update.
- The size report shows the application inside its room on both boards.

## Not in this ADR

- A check for new releases on GitHub. It needs TLS, which is an extended module (ADR-005).
- Signed images.
- Updating the bootloader over Wi-Fi.

## To settle before approval

- ADR-002's verdict on PR #111: merged after changes, or rebuilt from its design.
- The flash part on the RP chips: which of the three ways, after the trial on the Pico 2 W.
- Whether an older version may be installed over a newer one.
