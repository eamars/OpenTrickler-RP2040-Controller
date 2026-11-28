# ADR-014: a charge profile beside the powder profile

Status: **Open** 2026-11-28: approved by the owner as written, its firmware built and proven on the Pico W the same day ([progress](progress.md)), then parked by the owner until the afternoon; taken up again that evening, its web's part built as a mock for the owner, with the close left ("Left", in progress). First drafted 2026-11-15 to hold the owner's idea; written in full on 2026-11-28 from the issues and PRs, the code on main (391a90e) and the owner's four answers of that day.

## What we want

- **What is thrown is kept apart from how a powder flows.** Two calibres on one powder share that powder's profile and differ in their weight.
- **The weight is there after a power cycle.** Nobody dials it in again at every start (PR #109's need).
- **A unit that never uses the feature notices nothing but that.** It has one charge profile, which remembers its weight.

## Owner's direction

- 2026-11-15: "My original idea was introducing the 'charge profile' than a 'powder profile', for example, two caliber can share the same powder, but charge as different weight." It was not built before, for its complexity. PR #109, which keeps the last weight inside the powder profile, is closed for this reason (ADR-002).
- Issue #73 (the owner, 2026-01-03): a charge profile holds a fixed set point, the powder profile's index, "Pre-charge settings??". Its comment (2026-02-26): it "should also include charge mode settings which selects different stop thresholds as different calibre may require different charging accuracy."
- 2026-11-28, asked for this ADR: what the PR holds, the interface and the storage, each value's life cycle and failure modes, the EEPROM it takes. "Noted a very tiny percent of user used 128 kbit EEPROM than the 256K one due to the component shortage, but just very few of them."
- 2026-11-28, the four answers:
  - a change of the target at the unit **edits the chosen profile**;
  - **eight** profiles;
  - a profile holds its name, weight and powder, **nothing more**: the thresholds and the precharge stay where they are;
  - no unit is stored: "No unit, the user can put unit in its charge profile name if they like, just like how I organize the powder profile today."
- 2026-11-28: "I don't have the 128kbit board."
- **2026-11-28, the approval:** "Approve as written". Read here as: the ADR as it stands, and each of the five items under "Settled at the approval" as recommended there; the word of item 4 is still the owner's to choose at the mock.
- **2026-11-28, at the screen's mock:** the profile's name as the row itself; the word "Load"; the charge's line as today.
- **2026-11-28, on the bench's Pico W reporting a 16 KB EEPROM:** "So yes this MIGHT be my home build, for which ran 128Kbit EEPROM. The production units on Pico 2W should be 256Kbit so we don't need to worry that much rightn ow, but in the future." Read here as: nothing more is built for the smaller chip in this ADR; what was found is written where a later ADR giving out a slot meets it (`eeprom.h`, AGENTS.md).
- **2026-11-28, the park:** "Can you write down the progress and park the action for now? And perhaps leave for later implementation this afternoon."
- **2026-11-28, evening, at the unit and the web's mock:** "I like how web UI and front display is designed. The only comment is I'd keep the "Load" under the charge, and display maixmum number of X characters that doesn't overlaps with the key (load), and the right arrow showing this is a submenu." Read here as: the Charge page's row is "Load" and the name, cut at a whole character where it does not fit, with ">"; this replaces the name as the row, chosen at the mock that morning.
- 2026-11-26 (AGENTS.md): never insert a field into a stored record, always append. 2026-11-28: an upgrade is from a release to a release; the floor is v1.21.

## Why here

After ADR-006 (the owner, 2026-11-26) and after ADR-017, both built. ADR-017 is what this ADR stands on: it adds a stored record that is saved at every change of weight at the screen, and since ADR-017 a save is whole or not at all and a record that cannot be read costs its module nothing but its defaults. Its row was given its place by ADR-010 (`menu_design.md`), its paths by ADR-003 (`api_v1.md`).

## What we know

Read in the code on main, 2026-11-28, unless said otherwise.

- **The target is stored nowhere.** It is the menu's five digits in RAM (`charge_mode_get_target`), 0 after every start. A start with none is refused (`no_target`).
- **The powder in use is a field of the powders' record** (`current_profile_idx`, `profile.h`). Choosing a powder at the screen stores all eight powders with it (`powder_choose`, `menu_nav.c`).
- **A powder** is 72 bytes, eight to a record of 580, in the slot at 9 KB.
- **What else a charge uses is in the charge's own record** (`settings/charge`): the two stop thresholds, the margins, the gate at the coarse stop, the back-off, the precharge, the decimals, the tuner's and the forecast's switches.
- **The tuner begins afresh at a change of powder or of target** (`charge_step2_configure`): its windows' times are one target's. What it finds is kept into the powder's profile.
- **A coarse Kp is not the same at every weight.** ADR-009's simulation (progress, 2026-11-26): a coarse Kp at the tuner's bound was sound at 10 and 30 gr and overshot at 60 gr. The fine tube works on the last few grains whatever the target.
- **The EEPROM's map** (`eeprom.h`): 1 to 5 KB and 7 to 12 KB hold records; the spare slot, where every save writes first, is at 31 KB. 0 KB holds the old id record on every v1 unit. 13 to 20 KB and 22 KB may hold ADR-009's first build's records on the two bench units. **6 KB** (`EEPROM_APP_CONFIG_BASE_ADDR`) has been defined since v1 and no commit ever read or wrote it (`git log -S`): it is blank on every unit.
- **No firmware knows the chip's size.** The driver sends a 16-bit address and the erase writes 32 KB. A 128 kbit chip of this family looks at 14 address bits, so 31 KB would land on 15 KB there. That is the family's addressing as it is generally documented: the part on those boards is not known, and none is at hand to try. v1.21 wrote up to 10 KB only, so such a unit worked.
- **The Pico W's room** (`build_w` on this PC, as built 2026-11-28): flash 927.3 KB of 1,016; RAM at link time 252.6 KB of 256.
- **The settings' table** takes 12 resources and has 10.
- **PR #109's author** was invited to this ADR by the closing note and has not written.

## What we do

### The one idea

The unit always has one charge profile chosen, and **the target and the powder are that profile's own fields**. There is no second, working target. So what the screen shows is always what is stored or about to be, and nothing has to say "changed from the profile".

- Choosing another charge profile brings its weight and its powder.
- Changing the target or the powder changes the chosen profile.
- At the screen a change is stored when it is confirmed (ADR-010's rule 5). Over the API a write is live until saved, as every write is.

### The record

A header and an array, as the powders', in the slot at 6 KB.

```c
typedef struct {
    char name[16];
    float target;               // 0: not set
    uint8_t powder_profile;     // 0 to 7
    uint8_t unused[3];          // named, so nothing is appended into padding
} charge_profile_t;             // 24 bytes

typedef struct {
    uint16_t zero;              // a record's first two bytes
    uint16_t chosen;
    charge_profile_t profiles[8];
} eeprom_charge_profile_data_t; // 196 bytes, 200 with its CRC
```

- **The defaults:** "Load 1" to "Load 8", no target, each on the powder the unit had chosen (the powders' `current_profile_idx` as stored), the first chosen. So an upgraded unit charges with the powder it had.
- **A new field** is appended to each profile: the loader reads an array's elements at any size. Within the 1 KB slot a profile may grow to 125 bytes.
- **The powder in use becomes the chosen charge's.** The powders' `current_profile_idx` stays in its record as stored, read when the defaults above are made and by nothing after, as `rev` and `compatibility` are kept. It is no longer written, so choosing a powder no longer stores the eight powders, and `powder_profiles.saved` no longer moves with the choice.

### The screen

```
Charge
  Start
  Load   308 match ›      the chosen one's name
  Target   44.00 gr ›     its weight
  Powder   AR2208,gr ›    its powder
```

- **"Load" and the name** (the owner at the unit, 2026-11-28, evening). A name of 15 characters is 3 px too wide beside "Load": it is cut at a whole character, as many as fit, the key kept whole ("Load 6.5CM 140 ELD- ›"). The same holds for every choice's name beside its key (a powder's too); a number is never cut. The name as the row, chosen at the mock that morning, was dropped for it.
- A click opens the choice's own page, titled **Load**: the eight by name, the chosen one checked.
- **The word is "Load"** (the owner, the same day): the page's title, the names never set ("Load 3") and the web's page. "Charge" stays the job and its page.
- The same three rows on the Change page a charge opens between throws.
- **The charge's own screen keeps its line** (the owner, the same day): "of 44.00, AR2208,gr", the target and the powder. Nothing leaves it.
- Names are typed on the web only, as the powders' are.
- The mock: `tools/ui_mock/charge_profiles_12864.py`, its sheet in [mockups](mockups/charge_profiles_12864.png), with the layouts passed over beside the one chosen.

### The API

- **`GET /api/v1/charge_profiles`**: `saved`, and each profile's `id` and `name`.
- **`GET` and `PATCH /api/v1/charge_profiles/{id}`**: `name` (1 to 15 bytes, printable ASCII), `target` (a number, null when not set; what the digits can show), `powder_profile` (0 to 7). A write to the chosen one is refused during a throw (409 `busy`); the others are free.
- **`POST /api/v1/charge_profiles/save`** and **`/revert`**: the eight and the choice, one block.
- **`/api/v1/charge`**: `charge_profile` and `charge_profile_name` in the GET; `charge_profile` in the PATCH and the start. `target` and `powder_profile` stay, and now read and write the chosen profile.
- **Events:** a charge's and a throw's `started` carry `charge_profile` beside `target` and `powder_profile`, in a field of the event those kinds do not use, so the log takes no more RAM.
- Each follows from the settings' table, so the schema, the export (`GET /api/v1/settings`) and the console's `setting charge_profiles/3` come with it.

### The web

- **The Trickler page:** a picker above Target and Powder. The page stores what it changes, so a weight set there is kept.
- **A page for the eight,** beside Powders: name, weight and powder, its form drawn from the schema, the powder chosen by name.
- A mock on the stand-in first (`tests/web_test.py`).

### Each value's life

| Value | Made | Changed by | Refused | Kept |
|---|---|---|---|---|
| The chosen profile | The first, on a blank slot | The screen (stored); `PATCH /api/v1/charge`, a start (live) | During a throw | With the eight |
| A profile's target | Not set | The screen's editor (stored); a PATCH, a start (live) | During a throw, for the chosen one; a weight the digits cannot show | With the eight |
| A profile's powder | The unit's stored choice | The screen (stored); a PATCH, a start (live) | During a throw, for the chosen one; an id no powder has | With the eight |
| A profile's name | "Load 1" to "Load 8" | The web, the API | Not printable ASCII | With the eight |

A store at the screen stores the block, so it also stores what the API left live in the other seven, as a powder's choice does today.

### When it fails

- **The record damaged, read two ways, or the EEPROM not answering:** ADR-017's rule. The module runs on its defaults, nothing is written, and "Faults at the start" names it. On the defaults no target is set, so a start is refused until the person sets one: the unit never throws a weight nobody set.
- **The chosen index or a powder's id out of range** (a later version's record, a value damaged under a good CRC): the first runs in its place, in RAM, not stored.
- **A power cut in a save:** finished from the spare slot at the next start, or the record as it was.
- **An update's trial:** the write is held with the others, and written when the image confirms itself.
- **A factory reset:** the slot blank, the defaults again.
- **A change of decimal places:** refused today when the one target cannot be shown at the new decimals. It checks all eight, and names the profile that stands in the way.
- **A scale switched to another unit:** the number stays and means another weight, as every weight on the unit does. The owner's answer: the unit goes in the name.
- **Two charges far apart on one powder, the tuner on:** each change of charge begins the tuner afresh from the gains the other left in the powder. It converges again; it is not built around. Should throws show a second coarse Kp is needed, it is a field appended to the charge profile then.
- **A copy of the settings from before this version** holds no charge profiles: a restore leaves them as they are.

### The EEPROM

| | 256 kbit | 128 kbit |
|---|---|---|
| The chip | 32 KB | 16 KB |
| This record | 200 bytes in a 1 KB slot: 3.1% | the same slot: 6.25% |
| Slots free after it | 0 KB (once cleared), 13, 14 and 16 to 30 KB | 0 KB (once cleared), 13 and 14 KB |

- **15 KB is kept empty for good,** and `eeprom.h` says why: it is where the spare slot would lie on a 128 kbit chip. Unproven until such a board is tried.
- **A save** writes the spare slot and then the place, four 64-byte pages each. At a hundred changes of weight a day a chip's million write cycles outlast the unit.
- **RAM:** 196 bytes of the Pico W's 3.4 KB left at link time.

### What the PR holds

One PR, one gate.

1. The mocks: the screen's and the web's.
2. A new module, `services/charge_profiles`: the record, its table, its load after the powders'.
3. The charge, the API and the events as above; the portal's two pages.
4. The record named where records are listed: the update's reset list, the screen's "Faults at the start".
5. `api_v1.md`, `menu_design.md`, AGENTS.md, and the README and the manuals where they show the Charge page.
6. The tools that name the charge's fields (`rc_run.py`, `unit_api.py`), and the stand-in's recording.
7. The unit says its EEPROM's size, on the console and in `GET /api/v1/debug` ("Settled at the approval", 5).

## How the unit shows it works

- **Two charge profiles on one powder throw two weights,** and a change to the powder's gains shows in both (`charge_cycle` on each, the simulated scale).
- **The weight is there after a power cycle,** set at the screen and set from the Trickler page.
- **A unit on v1.21 updated to this** keeps its eight powders and the one chosen, has "Load 1" on that powder with no target, and charges once a target is set (the settings copied before and compared after).
- **On the PC** (`host_test.py`): a blank slot's defaults; a record with longer profiles read and stored at this size; a chosen index and a powder's id out of range.
- **`release_records.py`** finds the new record by its use and passes, both compilers' layouts the same.
- `menu_tour` against the baseline, saved again for the pages that change; `reach`; `api_check`; `settings_roundtrip`.
- **The Pico W's image links,** its RAM at link time reported.
- Application layer: proven on the unit on the probe (AGENTS.md).

## Not in this ADR

- Stop thresholds, the precharge or gains per charge profile (the owner, 2026-11-28: nothing more). Each is an append when it is wanted.
- A unit stored with a weight (the owner, 2026-11-28).
- Ladder loading (issue #106) and a press to skip a throw (issue #75).
- Making or deleting a profile: eight slots, as the powders.
- Tuning a powder (ADR-009, ADR-016).

## Settled at the approval

Each was put with the agent's recommendation, and the owner approved the ADR as written (2026-11-28): each is decided as recommended. Item 5 so joins what the PR holds.

1. **The slot: 6 KB.** Blank on every unit, and inside a 128 kbit chip. The other free slots each hold something on some unit.
2. **The powders' `current_profile_idx` is kept and no longer written.** One truth for the powder in use. Said here as a stored field's use ending, not its layout changing. (The proposal of this morning said it would go on being written; that would store two records at every choice of powder.)
3. **`api_v1.md`'s line is changed before anything is built on it.** It says a target given beside a charge profile "overrides it for that charge", which needs a second target. Here it writes the profile. With that, the powder's choice is stored by `charge_profiles/save`, no longer by `powder_profiles/save`: a change to an API called stable, made while v2 has only candidates and no release.
4. **The word on the screen and the web.** "Charge" is already the job and the page, so a "Charge" row on the Charge page reads twice. Recommended: **Load**, a reloader's own word for a calibre's recipe ("Load: 308 match", "Load 1" to "Load 8"), the API's name staying `charge_profiles`. The owner chooses at the mock. **Chosen 2026-11-28:** "Load", and on the Charge page no word at all, the name being the row; that evening, at the unit, "Load" as the row's key and the name beside it, cut to fit ("The screen").
5. **Whether the unit says its EEPROM's size** (a suggestion, not in the scope above). It can tell without writing: the same bytes read at 1 KB and at 17 KB are one chip of 16 KB. Shown in `GET /api/v1/debug` and on the console, a user with such a board could then tell us what the owner cannot try. Recommended: yes, as it is a few lines and the only proof within reach; left out if the owner would rather not grow the PR.
