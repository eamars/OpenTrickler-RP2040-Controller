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
