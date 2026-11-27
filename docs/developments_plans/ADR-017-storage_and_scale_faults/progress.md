# ADR-017: progress

What was built and measured. The plan is in the [README](README.md). The scratch scripts and builds are kept on this PC under `build_adr017/`, which git does not track.

Built on the branch `adr017`, from main at 6c6a9da, 2026-11-28 into the night, the owner away ("work on ADR-017 on your own, submit as one PR for all gates ... test on Pico W"). Gate 2 was built at the same time by a subagent in a worktree of its own, on the scale's files alone, and merged here; gate 1 is this session's.

## Folded in: a v1 backup's restore (2026-11-28)

The owner, the same evening, on the path for units older than v1.2x (a backup on v1's web page before the upgrade, restored on v2's after): "Scale setting is the real fault and needed to be fixed", and on the motors' current: "Can we implement a popup dialog to the user in the web that the new firmware will automatically scale down the old format current to keep the same real current, and the user can dial up if they need?" Then: "Before ADR-017, then immediately follow by ADR-017", and "You can fold the current change and ADR-017 into one PR".

- **The scale's make:** the portal read an old file's make 9 as the simulated scale. v1.21's 9 is FA-E/EN; the simulated scale went to 100 on 2026-11-26, and the portal's table was not changed with it. An FA-E/EN user restoring a v1 file would have run the simulated scale while the motors poured real powder. The table now reads 9 as FA-E/EN, and `tests/release_records.py` checks it against the release's numbers (it failed on the old table, passes on the new).
- **The motors' current:** a v1 file holds the figure before ADR-012, the current only with the driver's trimmer at full scale. The restore converts it as the firmware converts a stored one (×0.48, the factory trimmer's share, to 10 mA: 800 → 380, 600 → 290), and says so once, after the restore, in a sheet: "Motor current converted", the figures before and after, "If a motor stalls or slows, raise its current in Settings › Motors", with Done and Open Motors. A v2 file restores with no sheet.
- Checked on the stand-in (`tests/web_test.py`) with a v1.21 export written in v1.21's own format: the writes sent (`fa_en`; 380 and 290 mA), the sheet at a computer's width and on a phone in the dark theme, Open Motors opening Settings › Motors, a v2 file with no sheet.

## Gate 1: a load never writes

### What was built

**The loader** (`common.c`, `common.h`). A load fills the struct with the defaults first, so its module always has settings to run on, then says what it found (`config_found_t`):

| Found | What the module runs on | Written |
|---|---|---|
| read | the record | nothing |
| another version's | the record's fields, the rest the defaults | the record at this version's size (the one migration, as before) |
| finished | a save the power cut short, taken whole from the spare slot | the record in its place |
| blank (first two bytes `0xFFFF`) | the defaults | the defaults (on a trial held as them, said reset when kept), silently: a new unit, a new record's slot, the chip after an erase |
| damaged (no record at any size, the same on two reads) | the defaults | nothing: the chip keeps its bytes until the person stores that module or resets the unit |
| read two ways | the defaults | nothing: the bus is at fault |
| not reachable | the defaults | nothing, and every save refused |

The slot is read once (1 KB) and every size is checked from that buffer; only on no match is it read again, to tell damage from the bus. A record whose first two bytes are neither 0 nor blank (a record from before v1.17) is damaged: such a unit is restored from a backup (the owner, 2026-11-28), and its bytes are left as they were.

**What each load found is kept** (`config_found()`), and what is amiss listed (`config_amiss()`): damaged, read two ways, not reachable, no memory, and a write held on an update's trial that failed when the update was kept. A save of that record clears it. Such a record has no stored CRC, so its resource reads as not stored (`saved` false) on the web and the screen, which is the truth: what runs is not what the chip holds.

**A save is whole or not at all** (`save_config_now`): the record goes first into the spare slot, with its address, size and a CRC of its own, then into its place; each write is read back and compared, so a write the chip did not take is not believed. The spare is 1 KB at 31 KB (`EEPROM_SPARE_BASE_ADDR`), a slot no firmware wrote before. A place that does not read at the next start, its record whole in the spare, takes it from there and is stored ("finished"); `config_read_stored` (the update's note, a part read back) does the same. A cut while the spare is written leaves the place as it was. A save takes twice the writes it did: the powders' 580 bytes are ten pages twice, some 100 ms of the chip's 5 ms write cycles.

**A held write that fails** at a trial's confirmation is no longer printed and dropped: its settings read as not stored, and it is amiss on the screen.

**The settings' mender writes nothing** (`settings.c`): a stored choice no choice has runs its first choice in RAM only, the resource reading as not stored until the person stores it; the chip keeps what it held. A field whose module refuses an unknown value itself is kept as it is (`unknown_kept`): the scale's make, so a make no make has reads nothing at every start, not only the first (the review's finding 2; ADR-006's gate 9). It reads as `null` in the API and "?" on the screen, as an unknown choice already did.

**No module returns early at its load:** the scale's, the display's, the LEDs', the motors', the charge's, the powders', the servo gate's, the Wi-Fi's and the update's starts each run whole whatever the load found. The display's panel and buttons and the scale's port, locks and task always start.

**The EEPROM's start asks the chip** (`eeprom_init`, `cat24c256_present`: a byte read at address 0). When it does not answer, every load is "not reachable" with no bus traffic, every save is refused, and a charge and a calibration are refused, as a missing scale's are: `CHARGE_START_NO_STORAGE` and `CALIBRATION_START_NO_STORAGE`, `409 no_storage` in the API, `@err no_storage` on the console, "No settings memory / Charge cannot start" on the screen.

**The I2C calls have a time limit** (`chip_i2c.h`): a byte that takes longer than `CHIP_I2C_BYTE_LIMIT_US` (1 ms; 400 kHz takes 23 us) fails the call, so a bus held low fails a read in about a millisecond and never hangs it. ESP-IDF's counterpart takes a whole transfer's limit (`i2c_master_transmit`'s `xfer_timeout_ms`), the bytes times this. A write is believed only when the device took every byte: the SDK returns the count before a byte it did not acknowledge, which the old check (`!= PICO_ERROR_GENERIC`) took as done. The driver no longer writes an empty page after a record whose length is a whole number of pages.

**The erase** (`eeprom_erase`) is refused while an image is on trial (the image it may go back to would find nothing) and when the chip does not answer; otherwise it takes the EEPROM's lock and never gives it back, so no save lands between its last page and the restart, which its caller makes. The API refuses a factory reset on a trial (`409 on_trial`) and with no EEPROM (`409 no_storage`); the screen says "Not while an update is on trial". A failed erase is said on the screen before the restart ("Factory reset failed: settings may be left in part", 3 s): from the menu by a message and a timer, from the web through the screen's new `say` (`api_v1_screen_t`), a message the menu's task draws.

**The storage's buffers come from the FreeRTOS heap** (`pvPortMalloc`): the loader's two slot buffers, a save's spare and read-back buffers, a trial's held records, a part read back. The PC's stand-in maps them to `malloc`.

**"No radio"** (`wireless.c`): the radio's state when its start failed (`WIRELESS_STATE_NO_RADIO`) apart from "not started yet"; a scan and a leave refused for both. The Wi-Fi page says "No radio".

**The start's notice** (`menu_nav.c`): a row at the top of the main menu, "Faults at the start", while the EEPROM does not answer, a record is amiss, or the radio did not start; it opens notes ("At the start") saying what and what follows: "Settings memory: it does not answer ...", "Settings not read: Scale: damaged. ... Each runs on its defaults until you store it", "No radio". Not a pop-up: the row goes when the settings are stored. No new API field: the web shows each such resource as not stored, and a start says why it is refused.

**The records' checks** (`tests/release_records.py`):
- **Found by use:** every type whose size is handed to `load_config`, `load_config_layout`, `save_config`, `save_config_now` or `config_read_stored` outside the storage, each call giving it as `sizeof(<type>)` (five saves gave a variable's size, now their type's), and the structs inside them. The update's note (`update_note_t`) is found; the hand-kept list is gone.
- **From the compiler's layout:** each `--elf` image's DWARF read by pyelftools (pinned in `requirements.txt`): each v1.21 field where it lay (a record's last field, an array of elements that grow at their own end, may grow), no field appended into v1.21's tail padding but the three known (`KNOWN_FILLER`: the display's `inverted_display` and `charge_view`, the Wi-Fi's `setup_done`, each with what it reads), and with two images every record laid out the same in both. The charge record, read field by field through its declared layout, is not held to the filler rule.
- **v1.21's layout** (`RELEASE_LAYOUTS`) was read once from v1.21's own definitions: each typedef struct, union and enum and the numbers they use, taken from its headers at its tag, compiled by the Pico's compiler (`arm-none-eabi-gcc` 15.2, `-mcpu=cortex-m0plus -mthumb -g`) and printed by `--print-layouts` (`build_adr017/v121_layouts.py`). It agrees with what was known: the charge record 52 bytes with `decimal_places` at 24, the powders 516 bytes. v1.21's own build would have used its own submodules (u8g2, the Trinamic library, an older SDK), not on this PC.
- CI runs it after the builds, with both boards' images.

**Test builds** (each `-DRC_TEST_<name>=ON`): `EEPROM_ABSENT` (the chip looked for at an address none answers), `I2C_HELD_LOW` (the data line driven low by its pad under the bus), `RECORD_DAMAGED` (the charge's record read with a byte wrong every time, the chip untouched), `SAVE_CUT` (a save of the powders cut by a restart after three pages of its place), `SCALE_MAKE_UNKNOWN` (make 55 stored at the start).

### Shown on the PC

`tests/host_test.py`, `config_migration_test`, VERDICT PASS: every earlier case of the migration as before, and each finding put in on the fake chip, which now has a real chip's faults (`fake_eeprom.c`: not answering, a power cut after so many bytes, a byte read two ways, a byte a write does not take):
- blank: the defaults, stored, nothing said;
- damaged: the defaults run, the chip's bytes unchanged, not saved, amiss; the next start the same; stored by the person's save, no longer amiss;
- a record from before v1.17: the defaults, the chip unchanged;
- read two ways: the bus at fault, nothing written;
- not answering: the defaults, a save refused, the chip unchanged;
- a save cut in its place: the next start takes it whole from the spare and stores it; cut in the spare: the last good record read;
- **the powders' save cut at every page** (each 64 bytes of the spare's write and the place's): the old record or the new, whole, every time, never the defaults;
- a write the chip does not take: not believed, and the next start takes the record from the spare;
- on a trial, a damaged record is not written when the update is kept; a held write that fails then is not saved and amiss.

`tests/release_records.py --elf build_w/app.elf --elf build/app.elf`: VERDICT PASS. Tried once and not kept: a byte appended to the LEDs' v1.21 record, compiled, lands at 19, in its 20 bytes' padding, and fails ("starts at 19, in v1.21's tail padding").

`tools/module_graph.py`: VERDICT PASS.

### Shown on the Pico W (2026-11-28, night)

The bench's Pico W with the A&D FX-300i, its settings copied first (`rc_run.py settings --save logs/adr017/before_w`, at 2.0.3-360). Each test image built from gate 1's code (`build_adr017/t_*_w`), flashed by the probe, then the normal image put back and the settings compared with the copy.

- **The image, on the unit's own settings:** every record "read" at its start; the settings equal to the copy.
- **The EEPROM where no chip answers** (`EEPROM_ABSENT`): "the chip does not answer" and every record "does not answer: its defaults run"; the unit started whole on its own hotspot, the main menu opening on its first row, "Faults at the start", its notes "Settings memory: It does not answer. The settings run on their defaults and none can be stored. Charge and calibration cannot start." `charge 30` answered `@err no_storage`, `calibrate scale` `@err no_storage`, a save `@err failed`.
- **The bus's data line held low** (`I2C_HELD_LOW`; driven low by its pad's override under the bus, which a plain output would have taken from the bus): the start's probe ended at its time limit, 1.0 s after the start began, and the start went on as with no chip: no other bus traffic, the same notice.
- **The charge's record read damaged** (`RECORD_DAMAGED`): "0x1400 damaged: no record at any size, the same on two reads: its defaults run, the chip left as it is"; the notice "Settings not read: Charge: damaged. Each runs on its defaults until you store it"; the web's charge settings `saved: false` at their defaults. The normal image then read the record whole, the settings equal to the copy: the chip had kept its bytes.
- **A save cut by a restart** (`SAVE_CUT`): the first powder renamed and stored from the console; the save restarted the unit after three pages of its place. The next start: "0x2400 a save the power cut short, finished from the spare slot", the new name read; the normal image then read the place whole ("read"). The settings put back from the copy. The ADR named a power cut by the probe; a restart between two pages leaves the chip as such a cut does (the chip finishes the page it has taken, its write cycle its own), and needs no timing from the PC to land between pages.
- **A make no make has** (`SCALE_MAKE_UNKNOWN`, 55 stored at the start): "the stored make (55) is not in this build" at that start, at the next on the normal image and after a restart, nothing mending it; `setting settings/scale` gave `driver=null`, the screen's Make "?", `charge 30` `@err no_scale`. Put back from the copy.
- **No radio** (`RADIO_FAILS`): the Wi-Fi page "No radio", the notice "No radio: The radio did not start: the unit runs without Wi-Fi." `wifi scan` said "busy" until its words were mended to `@err no_radio`.
- **A factory reset on a trial:** an image that never confirms itself tried in slot B (`slot_trial --expect back`, `-DRC_TEST_BLOCK_VERSION=600;0`); asked once the unit reported itself on trial, the factory reset answered `409 on_trial`; the trial ended at its minute and the unit went back, its settings kept.
- **The C heap** (the Pico W's `__end__` to `__HeapLimit`): 3,496 bytes before, 3,260 after (the start's notice, the found table and the screen's message: 236 bytes of statics). Before, the loader, the save, a part's read and a trial's hold called `malloc`; after, only newlib's own number formatting does (`_Balloc`, read from the image's code). newlib's high-water mark (`__malloc_max_sbrked_mem`) read 3,496 on main's image: its first `malloc` takes the whole heap from `sbrk`, so the mark gives the room taken, not the bytes used.
- **Sizes**, the Pico W against main's (6c6a9da): flash 919.3 to 925.8 KB of 1,016; RAM at link time 252.4 to 252.6 KB.

**Found on the unit and mended:**
- **The I2C limit counted time spent waiting behind other tasks.** A restore of a v1 file from the portal failed its save at the Wi-Fi's record ("not written whole"); forty saves of every setting under the web's load failed five times, each at one record, each record left as it was. The SDK's wait for a byte checks the clock before it checks whether the byte went out, so a task preempted for more than the limit fails a byte that was sent. At 1 ms a byte that happens under load; the limit is now 1 s a byte, which only a bus that is stuck reaches: 60 of 60 saves, then 20 of 20 on the final image. A held bus then costs the start one second, its probe's.
- **A chip that did not answer was still asked** by the update's note at the start, each read waiting out the limit: `eeprom_read` and `eeprom_write` now refuse at once when the chip did not answer at the start.

## Gate 2: the scale as one reader

Built 2026-11-28 by a subagent in a worktree of its own, on gate 1's tip (491ea0d); proven on the PC. The unit's proof is the main session's (below).

### What was built

**One reader** (`src/services/scale/scale_reader.c`): the task "Scale Task" on the scale's port, priority 9 (its stack 1,536 bytes since the unit's proof, below; 1,024 before), reads every make. It waits for each byte (`chip_serial_read_within`: up to 1 s, or up to the next request where the make is asked), hands it to the make's framer, and publishes each frame found through ADR-013's door: tapped (`scale tap on` now shows every make's frames), NaN for a frame with no number, the unit where the frame carries one (the A&D's). The simulated scale is untouched; at its hand-over back, `scale_simulation_stop()` drains the port and tells the reader to start afresh (`scale_reader_restart()`).

**A make is a table entry and a parser** (`scale_make_t`, `scale_make.h`), with no task and no state; the makes stay in `drivers/scale_protocols`:

| Make | Its frame found | Length, its ending in it | Asked for | Zero |
|---|---|---|---|---|
| A&D FX-i | at LF | 17 (its CR checked by its parser, as before) | no | `Z` |
| Steinberg | at LF | 17 | no | none |
| Creedmoor | at LF | 14 | no | none |
| G&G JJB | at LF | 14 | `!p` every 250 ms | `!t` |
| US Solid | at LF | 15 | no | none |
| Radwag PS R2 | at LF | 21 | no | `T` |
| Sartorius | at CR or LF | any, to 32 | no | none |
| FA-E/EN | at CR or LF | any, to 32 | no | none |
| Generic | at LF | any, to 32 | no | none |
| JM Science | from its header `E` | 19 | no | none |

- **The framer** (`scale_framer.c`): a frame runs from one line ending to the next, or from its header for its length; a line holding only CR and LF is no frame. A line of the wrong length, one longer than 32 bytes (dropped whole), and a JM Science frame cut short by the next header are each a frame with no number. Before the first frame's start (at the start, and after a hand-over) nothing is a frame.
- **Each parser as it was**, reading the frame's own bytes with a NUL after them: the same fields, the same `strtof`. What a parser found no number in is now NaN, so counted: the Generic's line with no number, the FA-E/EN's line not starting `E`, the Radwag's 21 bytes not starting `SUI`, each dropped unseen before.
- **The zero commands are in the table**, and `scale_can_zero()` reads them there: the same three makes the switch it replaces named.
- **The Steinberg's frame is 17 bytes**, the example in its file (15 characters, CR LF), not its fields' 16, which miss the space before the unit; the number lies where the fields put it. The old code read both lengths: it took the 16 bytes after a line ending and reset at the LF. The G&G's 14 is its fields' alone (its file has no example). Neither scale has been on the bench. A wrong length reads nothing wrong and shows at once: every frame tapped as `nan` and counted bad, and no weight.
- **The makes' unused commands** (the A&D's print, sample, mode and display keys; the G&G's; the Radwag's continuous mode, keyboard, beep) had no caller and no header: gone, each make's commands kept as a line in its file.
- **The wiring:** `scale_register_make(driver, make)` from `app.c` as before, a table entry in place of a handle. `scale_makes_set_host()` and its struct are gone: a make publishes nothing, the reader does. `drivers/scale_protocols` now requires nothing (no board, no FreeRTOS, no platform), so it is built whole on the PC (`HOST_MODULES`, seven).

**The wait sleeps** (`chip_serial_rp2.c`). On the RP chips `chip_serial_read_within` spun for its whole time (the SDK's `uart_is_readable_within_us`): a reader waiting a second at a time would have held a core at priority 9. A wait of a tick (1 ms) or more, from a task with the scheduler running, now sleeps on the UART's receive interrupt (its FIFO half full, or a byte in it and 32 bits' quiet), which is masked but while a task waits; a shorter wait, or one before the scheduler starts, spins as before. ESP-IDF's `uart_read_bytes()` sleeps the same (ADR-006's table of the interface). The other caller is the motor drivers' line, 2 ms a byte: its exchanges at the start (before the scheduler) spin as before, the later ones (the once-a-second read, `tmc`, a setup) sleep until the echo and the answer are in, 32 bits after their last byte (128 us at 250 kbaud).

**What a reading's timing becomes:** a frame is published about 32 bit times after its last byte (1.7 ms at 19,200 baud), where the poll took 0 to 20 ms. The rhythm's intervals lose their 20 ms steps (ADR-013 saw 41 and 62 ms). The calibration's scale lag was measured with the poll's mean 10 ms in it, so a lag stored before this gate reads some 8 ms long (reasoned, not measured).

### Shown on the PC

**The old reading code against the new**, both built for the PC over a fed port and fed the same 4,934 streams (`build_adr017/g2_compare/` in the subagent's worktree, scratch: `take_old.py` copies the makes from adr017, `compare.py` builds both with `zig cc`, runs and judges). The old tasks ran until their bytes were used up, a cut being a poll that found nothing; the new reader the same, a cut being a wait that timed out. VERDICT PASS: what had nothing wrong in it read the same by both, and the new never read a weight the old did not, nor lost a whole frame the old read.

- **The same by both:** every make's whole frames; the reads split at every byte (each the same as whole); and the A&D's frames as the bench recorded them, 72,721 frames from the tap's `@sf` lines in 74 logs (`logs/rc`), the reads split every 37 bytes: all 80 stretches the same.
- **Different, each a fault this gate fixes** (by make: the cases the old misread and the new did not; those in which the new counts more bad frames, or fewer):

| Make | Cases | The same | Old misread, new not | More bad | Fewer bad |
|---|---|---|---|---|---|
| A&D | 610 | 295 | 4 | 1 | 297 |
| Steinberg | 531 | 272 | 95 | 88 | 15 |
| Creedmoor | 395 | 207 | 61 | 83 | 0 |
| G&G | 395 | 207 | 23 | 83 | 1 |
| US Solid | 438 | 230 | 77 | 88 | 2 |
| Radwag | 738 | 394 | 85 | 116 | 36 |
| JM Science | 628 | 438 | 57 | 19 | 12 |
| Sartorius | 416 | 189 | 143 | 1 | 26 |
| FA-E/EN | 531 | 237 | 146 | 37 | 30 |
| Generic | 252 | 158 | 42 | 7 | 0 |

  - *The old misread*: a frame stitched at the simulated scale's hand-over, every make (the A&D's 4 are a held start and a new tail making the right length: `2.34` read where 0.00 and 12.34 were sent); a byte added or dropped in a count make's frame, read at the count shifted (`5` added in the Creedmoor's `+0.32445 oz` read 50.3245); two frames run together; the tail of a long line (the Generic's `...  12` then `3.45 g` read 3.45, the Sartorius' and the FA-E/EN's the same); the tail of a partial first line when the port opened in a frame's middle (the Generic's and the Sartorius': `2.34`, `7.35`).
  - *More bad frames*: what the new counts and the old dropped unseen: a frame of the wrong length, a line with no number (`OL`), a frame cut short.
  - *Fewer bad frames*: the line under way at a hand-over or at the start is no frame now, where the A&D counted it (ADR-012's 30 hand-overs counted 12 such lines). `scale_frames` on the A&D should count fewer bad frames at its hand-overs and restarts for that alone.
  - *Read right by the old by luck, refused by the new*: a damaged count-make frame whose number survived its damage; and the first frame after a hand-over, even when the drain ended on its boundary (the reader cannot tell, and drops the line under way): one frame, some 50 ms, at each hand-over.
  - The A&D's line `X` then LF (2 bytes) is counted bad now; it was skipped as a blank line.
  - The Steinberg's frames of 16 bytes, as its fields were written: the old read them, the new refuses every one (its length, above).
- **Wrong by both, not fixed by this gate:** a frame of any length (the Sartorius, FA-E/EN, Generic), or one taken at a count from its header (the JM Science), has no length to catch a byte added or dropped in its number: `5` added at the start of the Generic's `- 0.02 gn` read 5, the `6` dropped from the Sartorius' `+   62.916 GN` read 2.916. A byte added: the Sartorius 32 cases, the Generic 23, the FA-E/EN 22, the JM Science 34; a byte dropped: the Sartorius 6, the FA-E/EN 7, the Generic 4.

**The PC's test** (`tests/host/scale_framing_test.c`, run by `tests/host_test.py`): a byte added to a Creedmoor frame refused; two Steinberg frames run together refused; a hand-over in a US Solid frame (a held `+  3` and a new `08.019GN`, which stitched read 308.019) reading neither; the Radwag's non-`SUI` and short lines counted bad; the Generic's and the FA-E/EN's lines with no number counted bad; the Generic's and the Sartorius' long lines dropped whole; a JM Science frame cut short counted bad; the A&D's own frames. VERDICT PASS.

### Measured

- `tools/module_graph.py` VERDICT PASS; `tests/host_test.py` VERDICT PASS (gate 1's cases and gate 2's); `tests/release_records.py` with both images VERDICT PASS.
- Both boards build with no new warning. The Pico W's image against gate 1's (`arm-none-eabi-size`): 953,340 to 953,676 bytes of flash (+336), 255,912 to 255,916 of RAM at link time (+4). By module (`size_report.py`): `scale_protocols` 3.1 to 2.3 KB, `scale` 7.4 to 7.9, `rp2` 17.8 to 18.4.
- The reader's stack: the bench's logs show "Scale Task" with 640 to 848 of its 1,024 bytes never touched (`stats tasks`, 2026-11-19 to 26). The reader's chain to the tap's print is 192 bytes deeper than the A&D's task's was (`-fstack-usage` at `-O0`: the task 104, the framer 32 and 40 and 40, the publish 40, the tap 144; the A&D's task was 64 and the tap). Kept at 1,024, for the unit to confirm.

### Shown on the Pico W (2026-11-28, night)

The final image (both gates), on the bench's Pico W with its A&D:
- `scale_frames --handovers 30 --reboots 2 --motors 0` (the motors' phase left out: its 60 s of both motors turning reckon 5,460 gr against the cup's 600; it measured ADR-013's noise, not the framing): idle 1,256 frames, 1,256 numbers; across 30 hand-overs 2,495, all numbers; across 2 restarts 232, all numbers; none bad; 48 ms between frames typically, 51 at the longest, no longer in the poll's 20 ms steps. On gate 2 alone the same: 2,512, 2,496 and 231, all numbers.
- `charge_fault bad --expect not-seen` and `charge_fault quiet --expect not-seen`: VERDICT PASS, as ADR-013 ran them ("Charge: paused, no reading for 300 ms", then "ended, Scale: no reading").
- The motor drivers' line, whose waits now sleep: `tmc coarse` and `tmc fine` read, `tmc coarse setup 3` and `tmc fine setup 3`, no retry and no failure; `drivers=ok,ok`; `charge_cycle` VERDICT PASS.
- **The reader's stack was found short:** after the tapped run and the faults, "Scale Task" had 128 of its 1,024 bytes never touched (`stats tasks`; the estimate above was some 450). The tap's print of each frame now runs on the reader's stack. Given 1,536 bytes: 640 never touched after the same tapped run.
- `menu_tour --compare` (the new page, "At the start", saved into the Pico W's baseline), `charge_cycle`, and `api_check` (37 paths, 45 actions, back after its restart): VERDICT PASS.

**The Pico 2 W was not run.** It was off the probe tonight, and the owner's word was to test on the Pico W. AGENTS.md proves a change to a driver and to timing against the chip on both boards: the serial wait (`chip_serial_rp2.c`) and the reader are such a change. Its run is the owner's to ask for: `flash`, `menu_tour --compare` (its baseline gains `page_start_faults.png`, and its four pages from ADR-009's review are still to be saved), `charge_cycle`, `scale_frames` if the A&D is on it, `tmc` on both drivers.

### For the unit (the main session's)

- `stats tasks` after a `scale_frames` with the tap on: "Scale Task"'s stack never touched.
- `scale_frames --handovers 30 --reboots 2` on the A&D: every frame a number; fewer bad frames at the hand-overs and restarts (above); the times between frames no longer in 20 ms steps.
- `charge_fault bad` and `charge_fault quiet`, as ADR-013 ran them.
- The motor drivers' line, which now sleeps in its waits: `tmc coarse`, `tmc fine`, `tmc coarse setup 3`, `state`'s `drivers=`, `charge_cycle`.
- A change to a driver and to timing against the chip: the Pico 2 W too, by the rule in AGENTS.md.

## Closed (2026-11-28)

Both gates built and proven on the Pico W, and the v1 backup's restore folded in, at the owner's word ("work on ADR-017 on your own, submit as one PR for all gates"; "You can fold the current change and ADR-017 into one PR"). Closed on its branch before its PR, which the owner merges.

- **Left for the owner:** the Pico 2 W's run (above), not made tonight: it was off the probe.
- **Not built, as the ADR says:** a new make of scale; the gate's move queue; what ADR-016 asks of a throw's record.
- **Handed on:**
  - ADR-014's new record joins what this ADR built: found by use by `release_records.py` (its size given as `sizeof` its type), its first two bytes 0, its layout checked against the release once a release carries it; a save of it is whole or not at all, at twice the writes.
  - The scale's lag stored by a calibration before gate 2 was measured with the poll's 0 to 20 ms in it (reasoned, not measured): a calibration made again reads it afresh.
  - Four makes' frame lengths are their fields' sizes (the Creedmoor's, the G&G's, the US Solid's, the Radwag's) and the Steinberg's its example's, none checked on a scale: a wrong one shows as every frame `nan` and no weight, never a wrong weight; the tap (`scale tap on`) gives a make's real frame.
