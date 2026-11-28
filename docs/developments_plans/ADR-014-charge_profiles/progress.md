# ADR-014: progress

What was built and measured. The plan is in the [README](README.md).

## 2026-11-28: the firmware, built and proven on the PC

Built on the branch `adr014_charge_profiles`, from main at 391a90e, by a session of its own in the owner's checkout. **Nothing here was proven on a unit:** the owner had not said the bench was ready, so no flash, no probe and no call to a unit was made. Every figure below is from the PC.

### What was built

**The module, `src/services/charge_profiles`:**
- **`charge_profiles.c`, the record.** `charge_profile_t` (24 bytes: `name[16]`, `target`, `powder_profile`, `unused[3]`) and `eeprom_charge_profile_data_t` (196 bytes: `zero`, `chosen`, the eight), as the README lays them out, in the slot at 6 KB (`EEPROM_CHARGE_PROFILES_BASE_ADDR`, the name `EEPROM_APP_CONFIG_BASE_ADDR` had and no code used). Loaded by `load_config_layout` with the header-and-array layout. It needs the storage alone, so the PC build runs it.
- **Its defaults** are made at the load, on the stack: "Load 1" to "Load 8", no target, each on the powders' stored choice, the first chosen.
- **What is mended in RAM, not stored:** a chosen index past the eight and a powder's id past the powders run as the first (the README). Two more of the same kind, found while writing the load: a target that is no weight (not a number, or below 0) is not set, since a start compares it with 0 and would have thrown it; and a name with no ending is ended, so a name in RAM is always a string.
- **`charge_profiles_settings.c`, the settings' table** (`/api/v1/charge_profiles/{id}`: `name`, `target` nullable, `powder_profile`; its save and its revert) and the three writes the charge, the screen and the API share: `charge_profiles_choose`, `charge_profiles_set_target`, `charge_profiles_set_powder`. The last two go through the table, so a target set at the screen, by `PATCH /api/v1/charge` or by a start takes the same checks, the same refusal during a throw, and gives the same `setting changed` event.
- **A target must be one the digits show.** The charge knows the decimals and the charge profiles lie below it, so the charge hands its check over at its start (`charge_profiles_set_target_check`).

**The powders (`services/profile`).** The powder in use is a variable in RAM, set by `profile_select`, which the charge profiles call at the start and at each choice. `current_profile_idx` is read once more (`profile_get_stored_choice`, for the defaults above) and written by nothing. `profile_get_selected()` and `profile_get_selected_idx()` are as they were for their callers.

**The settings (`services/settings`).** A resource may name `revert_rest`: what its block holds beside its parts. A revert put back the eight parts and not the header, so "the eight and the choice, one block" needed it. The powders' revert had the same gap for their choice.

**The charge (`services/charge`):**
- The target is `charge_profiles_target()`. The five digits in RAM, their writer and `charge_mode_set_target` are gone; the editor's own digits are the menu's.
- **Each throw takes its target and powder as it begins,** under the lock they are written under. Main wrote a new target into the running charge at the write; the next throw throws the same weight either way.
- **A change of decimal places** checks all eight targets and names the one in the way: "308 match: its target cannot be shown at those decimals; set one that can be first".
- **A start** takes `charge_profile` beside `target` and `powder_profile`. It is refused (`no_target`) when the profile it would throw has no target the digits show.

**The API and the events (`web/api`, `services/events`):** `GET /api/v1/charge_profiles`; `charge_profile` and `charge_profile_name` in `GET /api/v1/charge`; `charge_profile` in the PATCH and the start; `charge_profile` in a charge's and a throw's `started`, carried in the event's `result`, which those kinds did not use. The index, the guide and the schema's actions and events say so. The settings' own routes, the export and the schema follow from the table.

**The screen (`ui/menu`):** the Charge page is Start, the chosen load's name as a row, Target, Powder; the name's row opens a choice page titled "Load" (a row may name its choice page's title, `choice_title`). Change, between throws, is the same three rows. A load chosen, a target set and a powder chosen are each stored at the confirm. "Faults at the start" names the record "Loads". The console's `screen` describes the row and the page as any list's.

**The EEPROM's size (`services/storage`):** `config_eeprom_kb()` reads 64 bytes at 1 KB (the scale's record) and at 17 KB: the same, a chip of 16 KB; different, 32; a blank page at 1 KB, or a chip that does not answer, 0, not told. Read once and kept. On the console's `stats` (`eeprom_kb=`) and in `GET /api/v1/debug`.

**Housekeeping:** the update's reset list (`charge_profiles`, appended); `eeprom.h`'s map, with 15 KB kept empty for good; `rc_run.py`; the Trickler page's powder choice, which now stores the charge profiles.

### Choices the README did not settle

Each is as built, and the owner's to change.

1. **A throw that would begin with no target ends the charge on a fault, "No target".** It is reached by choosing, between throws, a load whose target is not set. The README says a start with no target is refused and that the unit never throws a weight nobody set; it does not say what a running charge does. The other answer would be to refuse the choice while a charge runs.
2. **The two further mendings** above (a target that is no weight; a name with no ending).
3. **The target's row says "not set" and gives no unit,** as today's screen does. The mock's sheet drew "Not set" and "44.00 gr"; neither was among the owner's choices at it.
4. **A choice's refusal is wrapped onto the message's three lines** at the screen, where it was one line cut at the panel's edge: without it the decimals' refusal showed the load's name and little else. It holds for every choice's refusal.
5. **Choosing a charge profile gives no event of its own.** The stream's state message follows it, as it followed the powder's choice. A target or a powder written by `PATCH /api/v1/charge` or by a start now gives a `setting changed` for `charge_profiles/{id}`, where a target gave none.
6. **The Trickler page stores its powder choice and no more.** A target given at its Start is written into the chosen load live, as the README's table says of a start, and is not stored: the picker and the page's stores are the web's part of this ADR.

### Two things a copy of the settings does not do

- **It does not hold which load is chosen.** The export gives each resource's fields, and the choice is no field of the eight: it lies in their block's header, as the powders' choice did in theirs. A restore leaves the choice as the unit has it.
- **A restore across a change of decimal places can need two passes.** The decimals are refused while a stored target cannot be shown at them, and the targets are refused while the decimals cannot show them. With one target in RAM this could not last past a restart; with eight stored it can. The import writes each resource in the export's order (the charge's settings before the loads), says which it could not write, and a second restore then goes through.

### What an update to this version shows

A unit updated over Wi-Fi finds the slot at 6 KB blank on its trial, so the record is held as its defaults and the `update confirmed` event names `charge_profiles` among the settings reset. That is the loader's rule for a new record's slot (ADR-017), not a setting lost.

### Measured on the PC

| Check | Verdict |
|---|---|
| `build_pico_w.ps1` | links. Flash 933.3 KB of 1,016 (927.3 before: 6.0 KB more); RAM at link time 252.9 KB of 256 (252.6 before: 0.3 KB more). The module is 3.2 KB of the flash |
| `build_pico_2w.ps1` | links. Flash 919.7 KB of 1,792; RAM at link time 285.9 KB of 512 |
| `tests/host_test.py` | `VERDICT PASS`, the new cases in it (below) |
| `tools/module_graph.py` | `VERDICT PASS`: nothing goes up, no loop |
| `tests/release_records.py --elf build_w/app.elf --elf build/app.elf` | `VERDICT PASS`: `charge_profile_t` 24 bytes and `eeprom_charge_profile_data_t` 196, new since v1.21, found by their use, laid out the same in both images |
| `tests/calibration_sim.py` | `VERDICT PASS` (what it loads was not touched) |
| `tests/features_check.py --final` | passes |

**The host test's new cases** (`tests/host/config_migration_test.c`, by the module's own load over the EEPROM in RAM):
- the record's sizes and offsets as the README lays them out;
- a blank slot: "Load 1" to "Load 8", no target, each on the powder given, the first chosen, stored, nothing said; the next start reads it, and the powders' choice is then taken for nothing;
- the powders' stored choice out of range: each on the first powder;
- a record with longer profiles (a later version's, 8 bytes appended to each): this version's fields read, the choice too, stored at this size;
- a chosen index of 9: the first runs, the eight as stored, the chip's slot unchanged, not saved;
- a powder's id of 200, a target that is not a number, one below 0, a name with no ending: each mended in RAM, the slot unchanged, not saved; stored once the person saves;
- a damaged record: the defaults, no target set, the slot unchanged, and it is said;
- the EEPROM's size: not told on a blank chip; 16 KB where 17 KB reads what lies at 1 KB, nothing written. The 32 KB answer is every bench unit's own and was not made on the PC.

### Not done, not verified

- **The unit's proof, all of it:** two loads on one powder throwing two weights; the weight there after a power cycle; a v1.21 unit updated to this; `menu_tour`, `reach`, `api_check`, `settings_roundtrip`; the EEPROM's size read on a real chip. The screen was built to the mock's third row and has not been seen on a panel.
- **`rc_run.py`'s changes were not run:** they need a unit. `api_check` sends `charge_profile` in the charge's PATCH and start, expects it in the `started` event, and where the chosen load has no stored target writes one live and reverts it. Each command that starts a charge with a target (`charge_cycle`, `soak`, `reach`, `charge_fault`, `settings_roundtrip`, `update --expect busy`) puts back what is stored after it (`setting charge_profiles/0 revert`), since a start now writes the target into a stored resource and would otherwise leave the unit's settings differing from their copy.
- **The menu baselines:** the Charge page gains a row on both units. Not saved.
- **The portal's new pages, the stand-in's recording, the README and the manuals, the ADR's close and its PR:** left to the session that started this one. Until the stand-in is recorded again it does not have `/api/v1/charge_profiles/save`, which the Trickler page's powder choice now posts.
