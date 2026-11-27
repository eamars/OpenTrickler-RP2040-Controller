# ADR-017: the settings' storage and the scale's frames, when they fail

Status: **Draft** 2026-11-27. Not approved. It holds what ADR-006 handed on: faults found in the code whose fixes change what a user sees, or need tests that are not small (the owner, 2026-11-27: "Small bug fixes are acceptable, as long as they don't need extensive tests"). Nothing here is designed yet, and nothing is built. Not placed in the order: the owner's to say.

## What we want

- A unit whose settings cannot be read, or whose EEPROM does not answer, says so at its start and does not lose what it had.
- A part that does not start (the EEPROM, the radio) is shown, not met later as a fault somewhere else (the owner's "fail early", ADR-012).
- Every make of scale read the same way: a frame taken only at its line ending, its number read by the make's own parser, a frame with no number counted as a bad one.
- Each shown on a unit, or on the PC against recorded frames where no scale of that make is on the bench.

## Owner's direction

- 2026-11-27 (ADR-006): small bug fixes in ADR-006, "as long as they don't need extensive tests"; the rest an ADR of its own, drafted before ADR-006 closes.
- 2026-11-15 (ADR-012, step 6): fail early, a fault shown before a mode is entered.
- 2026-11-26 (AGENTS.md): never insert a field into a stored record, always append; v1.21 the compatibility floor.

## Why here

- It is ADR-006's handed-on work, each item read in the code by ADR-006's review (2026-11-27).
- **Storage before ADR-014** is the agent's recommendation: ADR-014 adds a stored record, and a record that fails its CRC today wipes its module's settings.
- **The scale's reader before ADR-007** is the agent's recommendation: ADR-007 moves the serial port to the ESP32, and one reader is one port's user in place of ten.
- ADR-016 is not placed either. The order is the owner's.

## What we know

Read in the code 2026-11-27, at ADR-006's gate 10 (`077f109`), but where a line says otherwise.

### The settings' storage

- **A record that fails its CRC is replaced by its defaults at once** (`load_config_layout`, `common.c`): the defaults are written over it, or held as the defaults on an update's trial. One bad read of a sound record wipes the module's settings; nothing tells the user but the console.
- **The start ignores the EEPROM's own start** (`app.c`: `eeprom_init();`, its result not read), and each module's settings are loaded from it after.
- **The I2C calls have no time limit** (`chip_i2c_rp2.c`: `i2c_write_blocking`, `i2c_read_blocking`). A chip that does not answer is a NACK, which returns; a bus held low is not.
- **An EEPROM that does not answer leaves the unit blank, then calls through NULL** (ADR-006's review; not read again here).
- **The erase runs outside the EEPROM's lock** (`eeprom_erase`, `eeprom.c`): a save from another task (the menu, a charge) can interleave with it. Found in ADR-003 and handed on with no ADR named (HANDOVER.md). Since ADR-006's gate 10 it says whether every page was written, and its two callers restart either way.
- **The filler rule is checked by no tool** (AGENTS.md: an appended field never starts in the old struct's tail padding). `tests/release_records.py` reads each record's fields by type and name, not where the compiler lays them; three fields sit in v1.21's padding today, each reading v1.21's 0 as its default (ADR-009's progress).

### The radio

- **A radio that does not start leaves the unit running without the network** since ADR-006's gate 10 (it stopped a core before). The screen's Wi-Fi page then says "Not joined", as it does for a unit set up and not yet joined: nothing says the radio failed.

### The scale's frames

- **Each make is a task of its own** that reads the serial port and frames and parses its bytes itself (`src/drivers/scale_protocols`, ten makes; ADR-006's gate 7 swapped their serial calls for the platform's, one for one).
- **The A&D's frames are taken at their line ending, and only a whole one** (`and_scale.c`, since ADR-012): a frame stitched across a hand-over from the simulated scale had been seen (ADR-013). The other makes' are not checked to be.
- **The Generic scale's frame with no number is dropped, not counted**: it publishes nothing, where ADR-013's one door counts a bad frame (`publish_measurement` with NAN). Its sign and a leading point are read since ADR-006's gate 10.
- **Only the A&D FX-300i is on the bench.** `rc_run.py scale_frames` records its frames through the tap (`scale tap on`).

## What we do (ideas, not designed)

### Gate 1: the settings' storage

1. A record that fails its CRC: read again before anything is decided; then the unit runs on the defaults without writing them, and says so at its start and on the screen, until a save. When it writes, and what the screen says, is the design.
2. The EEPROM's start checked: a chip that does not answer is shown at the start, nothing written, the unit's settings its defaults, the motors not started (to settle). The I2C calls given a time limit.
3. The erase under the EEPROM's lock, and a failed erase shown before the restart.
4. A part that does not start shown the same way: the radio's failure says "No radio" in place of "Not joined".
5. The filler rule in `release_records.py`: each record's layout from the compiler (`ptype /o` of each `eeprom_*_t`, at v1.21 and now), an appended field starting past the old struct's end.

Shown: a test build whose EEPROM is at an address no chip answers (no chip taken off the board), a test build that reads one record with a CRC that fails, `settings --compare` before and after each; the radio's test build of ADR-006 (`-DRC_TEST_RADIO_FAILS=ON`) on the screen; `release_records.py` failing on a field put in the padding (tried once, not kept).

### Gate 2: the scale as one reader

1. One reader task on the serial port: it takes a frame at its line ending (the make says which) and hands it to the make's parser; a make is a parser and its zero command, with no task.
2. A frame with no number counted as a bad frame for every make, the Generic's included.
3. Each make's parser checked on the PC against frames: the A&D's recorded on the bench (`scale_frames`), the others' from their manuals, each frame and what it must read as.

Shown: `scale_frames` on the A&D with its counts as before, `charge_fault bad` and `quiet` as ADR-013 ran them, the parsers' check on the PC (`tests/host`).

## How the unit shows it works

- `menu_tour --compare` after each gate, with the pages it changes saved again in the same commit.
- `charge_cycle` and the settings compared across every flash.
- What each gate names above.

## Not in this ADR

- A new make of scale.
- The gate's move queue letting a write replace a move a mode waits for (`servo_gate.c`, found in ADR-003): a servo gate's, with no gate on the bench.
- What ADR-016 asks of a throw's record.

## To settle before approval

- **The order:** this ADR's place among ADR-014, ADR-016 and ADR-007. Recommended: next after ADR-006, storage before ADR-014 adds a record.
- **A record that fails its CRC:** run on the defaults unwritten until a save (recommended), or written as today with the user told.
- **An EEPROM that does not answer:** whether the unit runs at all (the motors, a charge) on settings it cannot keep. Recommended: the menu and the console only, a charge refused, as a missing scale's charge is (ADR-006, gate 9).
- **One ADR with two gates, or two ADRs:** recommended one, by the owner's "stage gates, not small ADRs".
