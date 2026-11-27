# ADR-017: the settings' storage and the scale's frames, when they fail

Status: **Draft**, written 2026-11-27 from what ADR-006 handed on, revised 2026-11-28 with [ADR-006's review](../ADR-006-platform_split/review_2026-11-28.md) ("ADR-017: the draft"), each of its facts read again in the code. Not approved. Not placed in the order: the owner's to say. Nothing is built.

## What we want

- **A load never writes, and never stops a module from starting.** A module whose settings cannot be read, or whose EEPROM does not answer, starts on its defaults, says what it found, and loses nothing that was stored. Only a save the user asked for writes.
- **A save is whole or not at all:** a power cut or a failed write leaves the last good record readable.
- **A part that does not start (the EEPROM, the radio) is shown at the start**, not met later as a fault somewhere else (the owner's "fail early", ADR-012).
- **Every make of scale read one way:** one reader on the serial port, each make saying how its frame is found and how it is read, a frame with no number counted as a bad one for every make.
- **Each shown on a unit by a test build, or on the PC**: the old reading code against the new, stream for stream, where no scale of the make is on the bench.

## Owner's direction

- 2026-11-27 (ADR-006): small bug fixes in ADR-006, "as long as they don't need extensive tests"; the rest an ADR of its own, drafted before ADR-006 closes.
- 2026-11-28: "revise ADR-017 with the review's findings".
- 2026-11-15 (ADR-012, step 6): fail early, a fault shown before a mode is entered.
- 2026-11-26 (AGENTS.md): never insert a field into a stored record, always append; v1.21 the compatibility floor.
- 2026-11-27 (ADR-006, gate 9): a stored make of scale the build does not carry runs nothing in its place.

## Why here

The agent's recommendation, the review agreeing; the order is the owner's:
- **Next after ADR-006**, which handed this on.
- **Gate 1 before ADR-014:** ADR-014 adds a stored record, and perhaps a save at every change of weight; today a record that fails its CRC wipes its module's settings, and a save cut by the power loses both its old and new record.
- **Both gates before ADR-007:** ADR-007's EEPROM lands on pins its draft calls "not confirmed", so its first start may well meet an EEPROM that does not answer; and one reader is one user of its serial driver in place of ten.
- ADR-016 is not placed either.

## What we know

Read in the code at `57cef50` (main, ADR-006 merged), 2026-11-28; the review's line numbers are of `a0ea6d5`.

### The settings' storage

- **A load that cannot use what is stored destroys it.** In `load_config_layout` (`common.c`):
  - a record that fails its CRC at its size, and at every other size it could have had, is replaced by its defaults, written at once (held as the defaults on an update's trial). One bad read of a sound record wipes the module's settings; nothing tells the user but the console;
  - a second read of the slot that fails (the search for another version's record) ends the same way, so a fault on the bus ends in the defaults written;
  - a record whose first two bytes are not 0 is taken as a chip never written, and its defaults written.
- **The settings' mender writes over a stored choice it does not know** (`mend_choices`, `settings.c`, run by `settings_register` at the start): the first choice is put in its place and stored. A stored make of scale that is a number no make has is refused for one start, as ADR-006's gate 9 decided, then stored as the A&D's, whose driver runs from the next start (the review, finding 2; AGENTS.md says so).
- **Two migrations write on purpose** at the start: the motors' current, converted once from a figure stored before ADR-012 (its mark in the upper two bytes of `motor_data_rev`), and the display's rotation, 90 or 270 stored by an older firmware made 0 or 180 (`mini_12864_module.c`).
- **Blank is not damaged.** A new unit, a slot a new record takes, and the chip after an erase all fail the CRC, and their first two bytes are `0xFFFF`; a damaged record's are 0. Today both get the same: the defaults, written.
- **A module whose load fails does not start.** Each returns at its failed load, before the rest of its start: the display's before its panel and buttons are set up (`mini_12864_module.c`), the scale's before its port, its two locks and its task (`scale.c`). What uses them after hangs or asserts. The start ignores every module's result (`app.c`), and `eeprom_init()` cannot tell a chip that does not answer: it makes a lock and sets up the bus, and never asks the chip.
- **The I2C calls have no time limit** (`chip_i2c_rp2.c`: the SDK's blocking calls). A chip that does not answer is a NACK, which returns at once; a bus held low is not.
- **A write is believed on its address alone:** `chip_i2c_write` takes anything but "no answer" as done, a write the chip stopped acknowledging part-way included, and nothing reads a record back.
- **A save cut by the power loses both records.** A record is written a 64-byte page at a time, 5 ms each (`cat24c256_eeprom.c`); the powders' record is ten pages. Power lost between pages leaves new pages, old pages and the old CRC: at the next start it matches at no size and the defaults are written.
- **A held write that fails is dropped.** At an update's confirmation each record held on the trial is written; one that does not write is printed and freed (`config_write_held`, `common.c`): the settings changed on the trial are lost, with no word on the screen.
- **The erase** (`eeprom_erase`, `eeprom.c`; since ADR-006's gate 10 it says whether every page was written):
  - it runs outside the EEPROM's lock, so a save from another task can interleave with it (found in ADR-003);
  - the lock must also be held until the chip restarts, or a save between the last page and the restart survives a factory reset;
  - it is the one write that goes round a trial's hold: the API refuses a factory reset while a job runs (`api_v1_commands.c`), not while an image is on trial, so the EEPROM is erased, the trial goes back, and the image it goes back to finds nothing.
- **The storage's memory on the Pico W:** the loader's buffers and a trial's held records come from the C heap, 3,496 bytes in all at 2.0.3-360 (`__end__` to `__HeapLimit` in the map); the review's sum of today's worst case is about 2.2 KB, not measured. The SDK's `malloc` panics when it runs out, and a panic enters the USB loader: a dead-looking unit at its start. Every static byte a later ADR adds comes out of that heap.
- **The checks of the stored records:**
  - `tests/release_records.py` reads each record's fields by type and name, not where the compiler lays them, so the filler rule (an appended field never starts in the old struct's tail padding) is checked by no tool. Three fields sit in v1.21's padding (the display's `inverted_display` and `charge_view`, the Wi-Fi's `setup_done`); each reads what v1.21 left in that byte, which is not always 0: a unit's `charge_view` read neither 0 nor 1 (`settings.c`).
  - It finds records by the name `eeprom_*_t`, so the update's note (`update_note_t`, `update.c`), stored with a CRC, is not seen.
- **A record's first two bytes are 0** in every record since v1.17 (its revision before then), at v1.21, v2.0.2 and v2.0.3 and today, the motors' mark in their upper two bytes; the loader requires it (AGENTS.md). Checked 2026-11-28 in each version's source: every default sets them 0, and v1.21 loads all nine of its records at each start, its old revision path zeroing them.

### The radio

- **A radio that does not start leaves the unit running without the network** since ADR-006's gate 10. The screen's Wi-Fi page then says "Not joined", as it does for a unit set up and not yet joined (`menu_nav.c`).
- Its state is one for "not started yet" and "did not start" (`WIRELESS_STATE_NOT_INITIALIZED`): "No radio" needs them apart.
- The radio's test build (`-DRC_TEST_RADIO_FAILS=ON`) skips the start; what a real failure leaves behind in the SDK was not run.

### The scale's frames

Each make is a task of its own that reads the serial port every 10 to 20 ms (the Sartorius every 1 ms, the G&G every 250 ms with its request), finds its frames and parses them (`src/drivers/scale_protocols`). How each finds a frame:

| How a frame is found | Makes | What goes wrong |
|---|---|---|
| At its line ending, whole, its length checked | A&D (since ADR-012) | - |
| At a line ending, no length checked | Sartorius, FA-E/EN, Generic | the Generic parses the tail of a line longer than 31 bytes as a line of its own: "...  12" then "3.45 g" reads 3.45 |
| By a count of bytes since the last line ending, published at the count | Steinberg, Creedmoor, G&G, US Solid, Radwag (which also checks its frame starts "SUI") | a byte too many, or a task resumed with its old count, reads a frame from the wrong place: the stitched frame ADR-013 found on the A&D, still in five makes |
| By a header byte and a count | JM Science | no line ending at all |
| Asked for | G&G: a request every 250 ms, its answer read | - |

- **A frame with no number:** the Generic drops it, publishing nothing, where ADR-013's one door counts a bad frame (`publish_measurement` with NaN). Its sign and a point before its digits are read since ADR-006's gate 10.
- **Only the A&D FX-300i is on the bench.** `rc_run.py scale_frames` records its frames through the tap (`scale tap on`).
- **Polled, not waited for:** a 32-byte FIFO polled every 20 ms overruns above some 1,600 bytes a second; 19,200 baud streams 1,920. No make is known to stream that hard (not measured). ESP-IDF's driver hands bytes over late, and reads with a wait.

## What we do (to design, nothing built)

### Gate 1: a load never writes

**The rule:** a load never writes, and never stops a module from starting. It leaves the module usable on its defaults and says what it found: read, read from another version, blank, damaged, or not reachable. Only a save the user asked for writes. This one rule replaces the draft's five fixes, each a guard on its own case.

1. **One mechanism, already in the tree:** an update's trial holds every write in RAM, holds a record that did not read as its defaults, and says which settings were reset (`common.c`). Off a trial the same hold serves: a record that did not read is held, not written, until a save of its module or a reset.
2. **What a load found, told apart:**
   - **blank** (first two bytes `0xFFFF`): its defaults stored, silently, as today: a new unit, a new record's slot, the chip after an erase;
   - **another version's**: read and stored at this version's size, as today (the one migration), and the two migrations above named as the only other writes at the start;
   - **damaged**: the slot read once and every size checked from that one buffer; on no match read once more and compared. The two reads differ: the bus is at fault, say so. They agree: the chip holds damage. Either way the bytes stay on the chip until the user saves that module or resets it, so a copy can still be taken; the unit runs on the defaults and says so at its start and on the screen. A charge is not refused for it: a new unit runs on the same defaults, and what is lost is the tuning, which the notice is for;
   - **not reachable** (the EEPROM does not answer): every module started on its defaults, the screen alive to say so; the menu and the console only, a charge and a calibration refused, as a missing scale's are (ADR-006, gate 9). Nothing set can be kept.
3. **The mender writes nothing:** a stored choice it does not know is kept, shown as unknown, and what needs it refuses (a stored make of scale no make has reads nothing at every start, as gate 9 decided).
4. **No module returns early:** each start runs whole whatever its load found, so the calls through NULL go because no start stops halfway, not because each is guarded.
5. **A save is whole or not at all**, and checked: written, then read back and compared; and a power cut mid-save leaves the last good record readable (to design: the new record first into a spare slot, then into its place, and at the start a place that fails its CRC with a whole record for it in the spare slot takes that). A held write that fails at a trial's confirmation is shown, not dropped.
6. **The erase under the EEPROM's lock until the chip restarts**, refused while an image is on trial, and a failed erase shown before the restart.
7. **The I2C calls given a time limit:** a change to a platform interface, so gate 7's rule applies: ESP-IDF's counterpart read first (its I2C calls take a timeout already), the rule written in `chip_i2c.h`.
8. **The storage's buffers off the C heap** (the FreeRTOS heap, 112 KB, or one static buffer), measured before and after: the Pico W's 3.5 KB C heap is no place for a 1 KB slot read.
9. **"No radio"** told from "Not joined" on the Wi-Fi page and at the start, with the radio's two states apart.
10. **The records' checks:** `release_records.py` finds the records by use (every type handed to `load_config`, `save_config` or `config_read_stored`), the update's note among them; and the filler rule checked from v1.21's layout, read once from a v1.21 build and kept as a table in the test, against CI's own ELF. Written to take any ELF, it is also ADR-007's check of every record's size and offsets by both compilers.

Shown, each by the cheapest real check that would expose its failure:
- a test build with the EEPROM at an address no chip answers (no chip taken off the board): every module started, the screen says so, a charge refused;
- a test build that holds SDA low by a plain output before the first read: the time limit reached, the start goes on;
- a test build that reads one record with a failing CRC: the unit runs on its defaults, says so, and the record's bytes are still on the chip after a restart (`settings --compare` against a copy taken first);
- a save cut by the power, by the probe as `rc_run.py update`'s power cut does (the cores halted between two pages of the powders' record, the chip restarted): the last good record read;
- a stored make no make has, put in the EEPROM by a test build (the settings refuse it by every other way): refused at the start and at the next;
- the radio's test build: "No radio" on the screen;
- the Pico W's C heap's low mark before and after;
- `release_records.py` failing on a field put in the padding and on a record not named `eeprom_*_t` (each tried once, not kept).

### Gate 2: the scale as one reader

1. **One reader task on the serial port**, reading with a wait (`chip_serial_read_within`), not polling: it finds each frame as its make says and hands it to the make's parser.
2. **A make is a table entry:** how its frame is found (a line ending; a count of bytes, after a line ending or a header byte; its length checked either way), whether it is asked for and how often (the G&G's request), its parser, and its zero command. Five makes then take a frame only whole and at its end, not at a count.
3. **A frame with no number is counted as a bad frame** for every make, the Generic's included, through ADR-013's one door.
4. **The Generic** takes no tail of a long line as a line of its own.

Shown, with no scale but the A&D:
- **on the PC, the old reading code against the new:** each make's task as it is today built on the PC over a fed serial port (ADR-006's gate 10 did this for the Generic), the old code and the new reader fed the same streams (whole frames, frames cut at every byte, a byte dropped, a byte added, two frames run together), the same readings required but for the faults this gate means to fix, each listed. ADR-006's gate 5 proved the ring that way. The A&D's frames recorded on the bench (`scale_frames`) among the streams;
- on the unit: `scale_frames` on the A&D with its counts as before; `charge_fault bad` and `quiet` as ADR-013 ran them.

## How the unit shows it works

- After each gate: `rc_run.py flash`, the settings compared with a copy taken first (`rc_run.py settings`), `menu_tour --compare` with the pages the gate changes saved again in the same commit, `charge_cycle`.
- What each gate names above.
- A unit updated from v1.21 keeps its settings (as ADR-006's end did).

## With ADR-014 and ADR-007

- **ADR-014's new record** must start with two zero bytes, take a slot of 13 to 20 KB and clear it first, join `release_records.py` and the update's reset bits (ten of sixteen used, `update.c`), and fit the Pico W's C heap until gate 1 takes the storage off it. Written into ADR-014 when it is designed.
- **ADR-007:** the one reader reads with a wait, which ESP-IDF's driver needs; the records' size-and-offset check by both compilers is gate 1's tool.

## Not in this ADR

- A new make of scale.
- The gate's move queue letting a write replace a move a mode waits for (`servo_gate.c`, found in ADR-003): a servo gate's, with no gate on the bench.
- What ADR-016 asks of a throw's record.

## To settle before approval

Each with the agent's recommendation, the review agreeing:
- **The order:** next after ADR-006; gate 1 before ADR-014; both gates before ADR-007.
- **A record that fails its CRC:** run on the defaults, unwritten, and say so; blank stays silent; the damaged bytes stay on the chip until the user saves that module or resets it; a charge is not refused for it.
- **An EEPROM that does not answer:** the menu and the console only, a charge and a calibration refused, the screen saying why.
- **A save cut by the power:** a copy in a spare slot first (one spare slot for all records, the save twice as long), or two copies of each record (twice the space: the powders' 580 bytes twice do not fit a 1 KB slot). Recommended: the spare slot.
- **One ADR with two gates:** yes, by the owner's "stage gates, not small ADRs".
