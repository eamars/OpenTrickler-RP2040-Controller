# ADR-004: progress

## Gate 1: the layout and the boot path

Approved by the owner 2026-11-16 with the whole ADR ("Approve as written"), to start on the Pico 2 W, which is on the probe; the Pico W comes later that day. Order: the Pico 2 W's layout and its image, then its trial; the Pico W's bootloader and its images; the full installs.

### The Pico 2 W's layout, shown 2026-11-16 (1.20.202, built from the working tree on beebfea)

- **Built:** `targets/pico2_w_partition_table.json` (A at 8 KB, B at 1,800 KB paired with A, 504 KB spare that the ROM does not boot from), made into `partition_table.bin` by picotool at each build; the image linked at the start of flash with the linker's region cut to a slot (`targets/flash_layout.cmake`, `targets/flash_region.ld.in`), sealed with its SHA-256 (`pico_hash_binary`), its metadata block carrying the version `scripts/gen_version.py --block-version` gives (1.20.202 is 276.202). `rc_run.py flash` on the Pico 2 W writes the table, blanks the start of slot B, and writes the image's `.bin` into slot A, after checking that the image's block version and its version string agree.
- **The ROM maps the slot it boots** (the ADR's first claim not yet seen on the chip): after the flash, the QMI's `ATRANS0` read `0x01C00002` through the probe, a window of 1,792 KB from flash's 8 KB, at the start of the address space; the other three windows empty. The image linked for the start of flash runs from slot A.
- `settings` copied before the flash (`logs/rc/settings/20261116_123506_settings`), `flash`, `menu_tour --compare tests/menu_baseline/pico2_w` (the version form differs, not judged), `charge_cycle` (30.000 for 30), `settings --compare` with the copy: each `VERDICT PASS`, 12:35 to 12:36.
- Size: the image takes 752.2 KB of the slot's 1,792 KB (42%).
- picotool: the build uses 2.3.1 (`toolchain.env`); the trial seal on the Pico W image (README, "What we know") was made with an older 2.1.1 found on this PC.
