# ADR-010: the screen's menu, designed

Draft for the owner, 2026-11-17. Not approved. The owner answered its decisions and its list of what may have been missed the same day (see the end); the gates are in the [README](README.md). This covers the logic only: what is nested under what, what the screen offers and what is left to the web, how the knob and the buttons drive it, the update's offer, and the split that lets another display come later. No pixels: the look is designed once this is settled.

## What the owner asked (2026-11-17)

- The menu's order: what is nested under what, and what is left out of the screen and reached only from the web.
- Only sensible options on the mini 12864, for a person with a rotary encoder and a button.
- The update's poll and its offer.
- Drop what the menu does only to get round u8g2's MUI, such as the Wireless menu's extra OK.
- A split like MVC (similar, not identical), so a 3.5" TFT, an AMOLED like the Open Weapon Mount Computer's, or a larger TFT can come later.

## What is there today

22 forms in `mui_menu.c`, their parents kept by hand in `menu.c`:

```
1 Main ─┬ 10 Select profile ─ 11/12 Weight (2 or 3 dp) ─ 13 Warning ─ charge mode
        ├ 20 Warning ─ cleanup mode
        ├ 40 Wireless ─ 41 "Press OK to view" ─ Wi-Fi info (a mode)
        └ 30 Settings ─┬ 31 Scale ─┬ 53 Driver, baud rate, OK
                       │           └ 51 Warning ─ calibration (the A&D's CAL)
                       ├ 32 Profile manager ─ 33 list, 34 gains (1/2), 38 speeds (2/2)
                       ├ 37 EEPROM ─┬ 60 Save
                       │            └ 61 Erase
                       ├ 39 Servo gate: Disable, Close, Open, OK
                       ├ 35 Reboot
                       └ 36 Version
```

What MUI forced (read in the code):
- **W1.** A screen that changes while it is shown cannot be a form. The Wi-Fi info and every mode draw from their own task, with the menu's task blocked meanwhile. Hence form 41's extra OK, and the API's `view` activity, which refuses a start while such a screen is open.
- **W2.** There is no number field. The weight is five one-digit fields, in two forms (2 or 3 decimals), with a button (`B1`) to choose between them.
- **W3.** A form has no parent. Every form carries Back and Next buttons or a "<-Return" row, and forms are reached by number.
- **W4.** A list cannot show a live value: the scale's make is chosen with no reading to show whether it works.
- **W5.** The modes read the buttons themselves (`button_wait_for_input` in charge, cleanup, update, calibration and the Wi-Fi info). A mode is the menu's task stuck in a loop, so a start from the API waits for the menu to get back to its forms.
- **W6.** A setting stays live until "Save to EEPROM" (form 60), which the person has to remember. A profile picked at Start is lost at restart unless saved.

Also found while reading:
- the target lives in RAM (`charge_weight_digits`) and is 0 after every start. Storing it is ADR-014's: the weight last used is kept with the charge profile;
- the scale's list offers "Simulated (no scale)" as a stored choice. On a unit with powder in it, that choice pours by a made-up weight.

## Principles

1. **The screen is for the bench; the web is for setting up.** A row belongs on the screen if a person needs it during a session, or if it fixes the unit without a phone (the scale's make, the knob's direction, a factory reset). Tuning, wiring and anything typed (names, the Wi-Fi key) are web only.
2. **Nothing is hidden: every element on screen is reached by the knob.** A row, a title's "<" (Back), a notes' button and each softkey is focused by turning and done by a click; the reset button is Back everywhere as well. No long press, and no hint teaches the controls (the owner, 2026-11-17).
3. **A row is shown only while it does something** (the Open Weapon Mount Computer's rule 13). The rule that decides is the same one that would refuse its command at the API.
4. **The screen is a client, as the web is.** It reads what the API reads and acts through the same entry points, `by` button. No rule exists only on the screen.
5. **A change is stored when it is confirmed.** There is no Save.
6. **Only two things ask first: what pours and what destroys.** Before charge or cleanup, the person acknowledges that the cup is on the scale, so powder never falls onto the bare pan (the owner: a safety warning, not a technical workaround). The factory reset asks too.
7. **Shallow.** A hub, and no row deeper than three levels.
8. **No pop-ups.** An update, a fault or a result waits in a row, or on the job's own screen.
9. **A job holds the screen while it runs, whoever started it.** When it ends, the person is back on the page they left.

## The knob and the buttons

The mini 12864 gives three inputs: the encoder's turn, its press, and the reset button (GP12). A TFT may have no reset button, and a touch screen has neither. The input driver turns them into four keys, and nothing above the driver sees a pin:

| Key | From | On a list | In a number editor | On a job's screen |
|---|---|---|---|---|
| Next, Prev | one detent clockwise or anticlockwise (Invert knob swaps them) | focus moves down or up and stops at the ends; above the first row is the title, which works as Back | focus moves over the digits, then Cancel and Set; while a digit is being set, its value goes up or down | focus moves between Stop and the action; a running cleanup's turn sets its speed instead |
| Enter | a press of the knob, taken as it goes down | opens or does the focused row | starts or ends setting the focused digit; on Cancel or Set, does it | does the focused softkey (a running cleanup's click pauses it) |
| Back | the reset button, or Enter on the title's "<" or a Cancel | up one level, with focus on the row you came from | ends setting a digit, or leaves the value unchanged | stops the job (except while an update installs) |

- The knob's button is read on both edges, for its debounce: a press is taken as it goes down, its release only readies the next. No long press, no double click and no hold-to-repeat: the button is not reliable enough to time (the owner, 2026-11-17).
- Each page remembers its focus.
- **Softkeys are focused like rows** (the owner, 2026-11-17): the focused one is drawn in reverse. A dialog opens on its action, so one click goes on, but one that destroys (the factory reset) opens on Cancel. A job's new action is focused as it appears; with none (while a charge pours), Stop is. A cleanup starts paused, on Run; running, its turn sets the speed (through 0 into reverse) and a click pauses it, keeping the speed for Run.
- **Numbers:** one editor for every number, digit by digit as today. Its digits, decimals and unit come from the field's own description (`settings.h`). The target gets 2 or 3 decimals from Decimals.
- **Choices:** a list page with the current option marked. Enter picks it, stores it and goes back.
- **Toggles:** Enter flips the value and stores it.
- **Text:** never typed on the screen.

## The tree

As built through gate 4. It grows by the survey below (every setting on both), and is drawn again once the owner settles it.

```
Main menu                        title: OpenTrickler, or the motor driver that is not OK
├─ Update available ›            only while the poll knows of a newer release; "Important update" when critical
├─ Charge ›
│  ├─ Start                      → "Cup on the scale" → the charge screen; with no target set, the target editor first
│  ├─ Target        24.35 gr ›   number
│  └─ Powder        AR2208 ›     choice of the 8 powder profiles
├─ Cleanup                       → "Cup on the scale" → the cleanup screen
├─ Wi-Fi ›
│  ├─ Network       HomeNet      or "Joining HomeNet", "Not joined"
│  ├─ Address       192.168.2.168
│  ├─ Name          opentrickler-4993.local
│  ├─ Signal        -58 dBm      as a station only
│  └─ Network, Key               as its own access point only: opentrickler-4993, opentrickler
└─ Settings ›
   ├─ Scale ›
   │  ├─ Reading    24.35 gr     live; "no reading" while the scale sends nothing
   │  ├─ Zero
   │  ├─ Make       A&D FX-i ›   choice, every make but Simulated (a debug feature: the owner)
   │  ├─ Baud rate  19200 ›      choice
   │  ├─ Decimals   2 ›          choice (charge's decimal_places)
   │  └─ Restart to apply        only after a change that applies at restart (the make)
   ├─ Display ›
   │  ├─ Invert knob   Off       toggle
   │  └─ Rotation      0° ›      choice
   ├─ Firmware ›
   │  ├─ Version       2.0.2     "on trial" until the image confirms itself
   │  ├─ Check for update        Checking…, then Up to date or "2.1.0 found ›"; hidden with no internet
   │  ├─ Last update   2.0.2 confirmed    once there was one; or "went back to 2.0.1"
   │  ├─ Channel       beta      only while on beta; set on the web
   │  └─ Board, Unit id
   ├─ Restart                    "Restart to apply" while a setting waits for it
   └─ Factory reset ›            → Cancel, Erase
```

Four rows on the main menu (five with an update), as today. No row is more than three levels down.

On the charge and cleanup rows, a motor driver that is not OK opens the fault page in place of the mode, as today (ADR-012, step 6). It comes before the acknowledgement, so nobody is asked about the cup for a start that cannot happen.

**The acknowledgement,** before charge and before cleanup: "Put the cup on the scale", with one action, "Cup is on: start". Back cancels. It is a step of the start, not a page of the tree: Back from the job's screen returns to the page the start came from, not to the acknowledgement.

**The jobs' screens** are not in the tree. A job puts its screen on top, whoever started it:
- **Charge:** the state, the weight, the target, the powder, the time, the throw's number and the last verdict.
  - Actions: Zero while the job waits for zero, under the lock the charge starts its pour under. Not while it waits for the cup's return: the charge takes a reading of 0 as the cup back, so a zero with the pan bare would start a throw on it, as the API's own zero is refused there (found in the review of gate 4). Target and Powder between throws, from the verdict to the next zeroing (the API refuses both during a throw); what they open closes when the next throw begins, so Back then reaches the charge.
  - Back stops the job and returns to the Charge page, with Start focused.
- **Cleanup:** the weight, the flow, the speed and the gate. Turning sets the speed, Enter sets it to 0, Back stops.
- **Update:** Downloading n%, then Checking, then Installing (keep the power on), and the version. Back stops it until it installs.
- **Fault page:** names the driver and says charge and cleanup cannot start. Enter or Back leaves it.
- **Later (ADR-009):** the cup's ask ("put an empty cup on the pan, and press") and test throws, as screens of the same kind.

When ADR-014 brings charge profiles, a "Charge: <name>" row slots in above Target and Powder on the Charge page. Nothing else moves.

## Status on every screen

The screen echoes the unit, as it does today (the owner, 2026-11-17). Whatever job runs is on the screen, whoever started it. Every page shows live values, so a target set from the web is the one on the Charge page.

A corner of every page and every job's screen shows three marks. The remote mark is the owner's idea:
- **Wi-Fi:** joined, the unit's own access point, or none.
- **Remote:** the unit is being controlled from elsewhere. Either the running job was started by the web, the API or the console (each job already records this as `by`), or one of them wrote a setting or sent a command in the last 30 s (a proposal). Reads do not count: a page that only watches, such as the mirror or a charge page, controls nothing. The command layer records the time, since every remote write passes through it.
- **Simulated:** the simulated scale is standing in for the real one (the console's `sim on`). Its weight is made up, so a charge with powder in the hopper would pour by it. The person at the unit must see this for as long as it lasts.

When a job was not started from the knob, its screen's state line also says who started it ("Charging, from the web").

## Every setting on the screen and on the web (the survey, 2026-11-17)

The owner, at the unit, on finding the servo gate gone: "My original ask from the beginning of the ADR-010 was: are there any feature from web should be ported to the screen. And it ended up with some menus being removed?? It doesn't make sense at all. I want you to take the RIGHT approach here, survey WHAT should, or can be duplicated from the web to the front screen. Context: Not everyone use screen and web at the same time, it would be easy if both settings are configurable from both front screen and web. If the removal is for memory size, then I need data as justification. I don't want MVP here. This ADR is the final display refactor, so nothing to be left for the future."

**The rule.** Every setting and every action the web has is on the screen too. Only measured data or the hardware leaves one off, and each such case is named below with its reason. This replaces the "web only" lists under "What leaves the screen". *Narrowed by the owner the same day (below, "Needed at the unit, and can be changed there"): on the screen when needed at the unit and changed there in a few actions.*

**How.** The screen's settings pages are made from the settings tables (`settings.h`), the tables the API, the schema, the export and the web's forms are already made from:
- a resource is a page, a field a row, and the field's type its editor: a boolean a toggle, a choice a list, an integer or a number the number editor within its limits, a string the text editor, a colour the colour editor;
- a resource with fields in more than one group (the charge's thresholds, coarse stop, precharge, colours) opens on its groups, each a page;
- a write takes the same path, checks and refusals as the API's (a motor's setting while a job runs: "Not changed", "a job runs"), and is stored at once (D2); a field that applies at restart brings "Restart to apply", as the scale's make does now;
- so a setting added to a table reaches both, and the screen cannot fall behind the web again.

**What it costs: measured on the Pico W, the tighter board, at 61ecef7 (2026-11-17).**
- **Flash:** 968 KB of 1,016 KB used, 47 KB free. The whole navigator today is 21 KB at -O0 (`menu_nav.c`: 55 rows, 15 pages, the list, card, notes, dialog, editor and job screens, the update's offer); the view is 7 KB; the settings' code 16 KB.
- **The parity, estimated from those:** the table-driven pages 4 to 6 KB (no row per setting is written: the tables are in flash already), the three new editors (text, colour, the network list) 6 to 9 KB, the restored job screens and the gate's control about 2 KB. 12 to 17 KB in all, leaving 30 KB or more. It is measured when built. If it ran out, AGENTS.md's order holds: ADR-005's optimised build before anything is cut.
- **RAM:** the FreeRTOS heap 43.8 KB free (43.0 KB at its least); LVGL's heap at most 10.1 of 14 KB used, 2.0 KB free in one piece at its least; the menu task 2.4 KB of its stack never touched. A text editor adds a few labels, a few hundred bytes of LVGL's heap. A network scan keeps at most 16 networks, about 0.7 KB of the FreeRTOS heap, only while its list is open.
- **So memory rules nothing out.** The Pico 2 W has twice the flash.

**The settings** (the schema's 11 resources, 73 fields):

| On the web | Its fields | On the screen | Before this survey |
|---|---|---|---|
| Charge (`settings/charge`) | thresholds (coarse stop, fine stop, steady margin, zero margin); decimals; coarse stop (gate ratio, back-off on, its turns, its speed); precharge (on, time, speed); colours (good, under, over, not ready) | Settings › Charge › its groups | decimals only, under Scale |
| Powder profiles, 8 (`powder_profiles/{id}`) | name; coarse Kp, Ki, Kd, minimum and maximum speed; the fine's the same | Settings › Powder profiles › each, by its name › its fields | chosen only (Charge › Powder) |
| Scale | make, baud rate, frame format | Settings › Scale, with its reading and Zero | no frame format |
| Motors, coarse and fine | steps a rotation, current, microsteps, maximum speed, sense resistor, acceleration, minimum speed, gear ratio, direction inverted, enable inverted | Settings › Motors › Coarse, Fine › their groups (motor, driver, wiring) | none |
| Servo gate | on, the two shutters' closed and open duties, closing and opening speed | Settings › Servo gate, with Open, Close and Position | none (the old menu's 39 had Disable, Close, Open) |
| LEDs | backlight, LED 1 and LED 2 colours; the PWM3 chain's count, colour order, RGBW | Settings › LEDs | none |
| Display | invert knob, rotation, invert screen | Settings › Display | all |
| Wi-Fi | on, network name, key, security, join timeout | Wi-Fi › its state, then these | the state only |
| Update | channel, source | Settings › Firmware: Channel, Source | the channel shown, not set |

**The actions:**

| On the web or the API | On the screen | Before this survey |
|---|---|---|
| The charge: target, powder, start, stop | Charge, and its job | yes |
| The cleanup: start, speed, stop | Cleanup, and its job | yes (its logic is to settle) |
| The scale's zero | Scale › Zero; the charge's Zero; the cleanup's (the web's cleanup page has it) | not in the cleanup |
| The gate moved: open, close, a position (the web's cleanup page and `PATCH /api/v1/gate`) | Servo gate › Open, Close, Position; and the cleanup's | none |
| The update: check, download, install, stop | Firmware › Check for update; the offer's Install; the job's Stop | yes |
| Restart, erase | Settings › Restart, Factory reset | yes |
| Save all | stored when confirmed (D2) | gone (D2) |

**What the web shows, besides settings:**

| On the web | On the screen | Before this survey |
|---|---|---|
| The charge page: the weight, the powder, the charge's time, its progress to the target, its step | the charge's screen: the weight, the target, the powder, the time, the verdict | the powder and the time were dropped in gate 4, against this design's "Charge" below |
| The cleanup page: the speed, Zero, the gate | the cleanup's screen: the speed, the weight, the flow, the gate, Zero | the speed only: the weight, the flow and the gate dropped in gate 4, against this design's "Cleanup" below |
| System: the unit's id, version, VCS hash, build | Settings › Firmware | yes |
| Wi-Fi: why a join failed (the old screen's link status: wrong key, no network, failed) | Wi-Fi's state, in words | lost in gate 4: "Joining" whatever the reason |

**Web only, each with its reason:**
- **Export and import** of the settings and of a profile, and **the update uploaded by hand:** each is a file, which the screen has no way to take or give (the hardware). The screen downloads the same releases itself.
- **The display's mirror:** it is the screen.
- **The set-up wizard** (`wizard.html`, the access point's form for the Wi-Fi): the screen's Wi-Fi page sets the same fields. The commissioning process as a whole is ADR-015's (the owner).
- **The weight's chart** over a charge (`plot_weight.html`): to settle (below).

**The editors it needs:**
- **Number,** grown from the target's (the old menu's way, as the target's now: the turn selects a digit, a click sets it). Its digits come from the field's limits; its decimals from its step, from the charge's decimals for a weight, or from a `decimals` added to the field's description where neither says (the gains). A value outside the limits is refused, as at the API.
- **Text,** for the network's name and key, the profiles' names and the update's source, in the same way: the turn selects a character, a click sets it (the turn then goes through the characters), the last places add and delete; Cancel and Set.
- **Colour:** red, green and blue, 0 to 255 each, set as numbers; the LED shows the colour while it is set, and Cancel puts it back.
- **The network list:** the networks a scan finds, strongest first, with their security; choosing one sets the name and the security and opens the key; "Other" types a name.
- **Short labels:** the tables' labels are the web's ("Coarse stop threshold"). Where a label and its value do not fit 128 px, the field's description gets a shorter one for the screen.

**Settled by the owner, 2026-11-17** (each as recommended, but the chart, settled on the mock):
1. **This rule, and the screen made from the settings tables.** The owner: "Table driven, but this needed to be built right, and consider the future web refactor to be done in the similar way". So the description grows in the tables, not in the screen: what the screen needs that they lack (a value's decimals, a short label, the actions beside a resource's fields) is added to them and to the schema, and ADR-008's pages are drawn from the schema the same way (written into ADR-008).
2. **The cleanup as one system** (yes, as proposed). It has one value (the speed), one action (Stop), and the web's Zero and gate. As the target's editor: its screen a list, the speed, the weight and the flow in sight; the speed set by a click and the turn, live, and a second click; then Gate (open or closed, a click toggles), Zero, and Stop. No Run and no Pause: a speed of 0 is both.
3. **The charge's screen** gets back the powder and the charge's time; the throw's number as this design had it.
4. **Text by the knob for every string** (yes), a key of up to 63 characters and the source's 127 among them, for anyone without the web; the network list spares typing the name.
5. **The weight's chart:** a choice of the person's, Settings › Display › Charge view, Weight or Chart (the owner, 2026-11-17, asked how it is switched; 14 actions, stored, and on the web as every setting). With Weight the charge's screen leads with the figure read from 50 cm; with Chart, a chart of the throw (a 128 by 40 canvas, 640 bytes) under the weight and the target. Stored in the display's struct, in its last byte of filler: on a unit that is updated it starts as that byte was until it is set once, the class of fault handed on to ADR-006. The charge's struct has no filler at its end, and a field there would reset every charge setting on every unit.

Then the look of each new page and editor is mocked and settled before it is built (design gates before build gates): [mockups/parity_12864.png](mockups/parity_12864.png), drawn by `tools/ui_mock/parity_12864.py`. The mock found three labels too long beside their value ("Back-off speed", "Minimum speed", "Sense resistor"): a short label goes into the tables (and so the schema) where a group already says the rest ("Speed" under Coarse stop). A colour is a page of three numbers, red, green and blue, each set by the number editor: as one figure it does not fit.

### Needed at the unit, and can be changed there (the owner, 2026-11-17)

The owner, on that mock: "No this is no good. Like the one is bad UI design" (the number editor's screen: a whole screen for one value, its digit's mark running into the line of limits under it). "Also you only mocked up the UI, but never though about HOW a human can adjust, how many click the human need to adjust. If a setting is too hard to adjust, then that's a bad design. And I noticed bad designs already from the mockup. Then, I need you to be able tell: What needed to be adjusted on screen. What can be adjusted on screen. This determines what should be implemented on the screen."

So the rule above is narrowed: **a setting is on the screen when it is needed at the unit and a person can change it there in a few actions.** Every setting stays on the web.

**Needed at the unit** means one of:
- **S, a session:** used at the bench while charging or cleaning up;
- **W, the unit working without the web:** what a person with no phone needs to get the unit, its scale, its screen and its gate working, or to restore it.

The rest is **T, tuning and building:** set once, against the throws' figures and charts the web shows, or by ADR-009's tuning.

**What a change costs:** every detent and every click is one action, counted from the main menu at rest (`tools/ui_mock/knob_cost.py` computes it). Every value is edited **in place in its row**, with no editor screen, as the cleanup's speed:
- a toggle: a click;
- a choice: a click, the turn through the choices in the row (wrapping, so the shorter way), a click;
- a stepped number: a click, a step a detent, a click; the field's description gives the screen's step;
- the target's digits (the one value with a wide range and a fine resolution): the turn selects a digit, a click sets it, the turn sets its value, a click ends it, then the turn to Set and a click;
- text: a click on "+", the turn through some 70 characters, a click, for each character.

| What | Need | Method | The change | To reach | To change | In all |
|---|---|---|---|---|---|---|
| Target, a small change | S | digits | 30.00 to 30.10 | 2 | 10 | 12 |
| Target, a new load | S | digits | 24.35 to 42.00 | 2 | 27 | 29 |
| Powder (8 profiles) | S | choice | 1 to 5 | 3 | 6 | 9 |
| Powder details, to read | S | page |  | 4 | 1 | 5 |
| Start a charge | S | action |  | 1 | 2 | 3 |
| Zero, in a charge | S | action |  | 0 | 1 | 1 |
| Stop, in a charge or cleanup (the reset button) | S | action |  | 0 | 1 | 1 |
| Cleanup's speed | S | step 1 rev/s | 0 to 3 rev/s | 3 | 5 | 8 |
| Cleanup's gate, open or closed | S | toggle |  | 4 | 1 | 5 |
| Cleanup's zero | S | action |  | 6 | 1 | 7 |
| Cleanup's Stop, by the knob | S | action |  | 5 | 1 | 6 |
| Scale's make (9) | W | choice | A&D to Sartorius | 6 | 4 | 10 |
| Scale's baud rate (3) | W | choice | 19200 to 9600 | 7 | 3 | 10 |
| Scale's frame format (2) | W | choice | 8N1 to 7N1 | 8 | 3 | 11 |
| Decimals (2) | W | choice | 2 to 3 | 9 | 3 | 12 |
| Fine stop threshold | S | step 0.01 gr | 0.03 to 0.05 | 6 | 4 | 10 |
| Coarse stop threshold | S | step 0.1 gr | 5.0 to 3.0 | 7 | 22 | 29 |
| Invert knob | W | toggle |  | 7 | 1 | 8 |
| Invert screen | W | toggle |  | 8 | 1 | 9 |
| Rotation (2) | W | choice |  | 9 | 3 | 12 |
| Backlight colour (8 named) | W | choice | white to green | 10 | 6 | 16 |
| Charge view (2): weight or chart | S | choice |  | 11 | 3 | 14 |
| Servo gate on | W | toggle |  | 8 | 1 | 9 |
| Servo gate open, close | W | toggle |  | 9 | 1 | 10 |
| A shutter's end (the servo moves live) | W | step 0.001 | 0.050 to 0.055 | 10 | 7 | 17 |
| A motor's direction | W | toggle |  | 10 | 1 | 11 |
| Update channel (2) | W | choice |  | 10 | 3 | 13 |
| Restart | W | action |  | 10 | 1 | 11 |
| Factory reset (opens on Cancel) | W | action |  | 11 | 3 | 14 |
| Wi-Fi on or off | W | action |  | 5 | 1 | 6 |
| Back-off turns | T | step 0.125 | 0.25 to 0.5 | 15 | 4 | 19 |
| Precharge time | T | step 50 ms | 1000 to 1500 ms | 17 | 12 | 29 |
| A motor's current | T | step 50 mA | 400 to 600 mA | 11 | 6 | 17 |
| A profile's Kp (digits) | T | digits | 0.0250 to 0.0300 | 15 | 17 | 32 |
| A profile's name | T | text | AR2209 | 14 | 131 | 145 |
| Wi-Fi network, picked from a scan | W | choice | the 2nd found | 15 | 3 | 18 |
| Wi-Fi key, 12 characters | W | text | Trickle-2024 | 16 | 180 | 196 |
| Update source, a URL | T | text | http://192.168.2.13:8080/a/b | 12 | 361 | 373 |

*The paths follow the mock's tree (`knob_cost.py` names each page's rows in order); recounted 2026-11-17 after the mock was redrawn, and corrected to the firmware as built (the backlight's Green is its fifth colour; Wi-Fi's button is two turns away). Six changes were made on the unit by exactly these keys and checked through the API (gate 6's proof, progress.md).*

**What it shows:**
- Everything needed in a session, or to get the unit working without the web, costs 1 to 17 actions, but a new load's target and a large change of the coarse stop (29 each): a choice, a toggle, an action or a few steps, never deeper than three levels.
- Text costs 131 to 371 actions: a profile's name, the Wi-Fi key, the update's source. That is too hard, so **no text is typed at the knob**, which reverses the earlier "text by the knob for every string".
- The tuning values can be changed (16 to 35 actions), but are not needed at the unit.

**Needed at the unit, and can be changed there: on the screen.**
- **In a session:** the target, the powder, start, stop and zero; the cleanup's speed, gate, zero and stop; the views of the charge (the powder, its time, the throw's number) and of the cleanup (the weight, the flow, the gate).
- **To work without the web:**
  - the scale's make, baud rate and frame format, and the decimals;
  - the display's knob, screen and rotation, and the backlight's colour, from 8 named colours (the screen's contrast is the backlight's);
  - the servo gate on or off, its Open and Close, and its shutters' ends, set while the servo moves (at the unit, looking at the gate, is where they are best set);
  - a motor's direction (a motor wired the other way);
  - the update's channel, Restart, Factory reset;
  - Wi-Fi on or off, and Wi-Fi's state with the reason a join failed.

**Not needed at the unit: on the web only.**
- **Tuning:** the charge's thresholds, margins, back-off, precharge and verdict colours; the profiles' gains and speeds; the gate's speeds. They are tuned against what the web shows, and by ADR-009.
- **Building:** a motor's steps, current, microsteps, speeds, acceleration, sense resistor, gear ratio and enable's sense; the LEDs 1 and 2 and the PWM3 chain; Wi-Fi's join timeout.
- **Needed only to reach the web, and too hard by the knob:** Wi-Fi's network, key and security. The unit's own access point serves the web without them (ADR-015, commissioning).
- **A developer's, and too hard:** the update's source.

**Bad designs the cost found in the mock:**
- the number editor's own screen: every value is now edited in its row;
- four levels deep (Settings › Powder profiles › a profile › Coarse trickler › Kp; Settings › Motors › Coarse › Driver › Current): the tuning pages are not on the screen;
- a colour as three numbers of 0 to 255: a choice of named colours instead;
- the text editor: no text at the knob;
- the cleanup's Stop below the fold, three turns away: it stays a softkey, as the charge's.

**The borderlines, settled by the owner (2026-11-17):**
- **The fine and coarse stop thresholds are on the screen,** Settings › Charge, stepped 0.01 gr and 0.1 gr in their rows (10 and 29 actions). A person who only uses the screen sees their effect ("Over by 0.04") and has no other way to change them. The rest of the tuning stays on the web.
- **The profiles' names stay on the web.** The screen chooses a profile by the name the web gave it, or "Profile n". A list of known powders (about 15 actions) was offered, and declined.
- **A motor's current stays on the web,** with the rest of the build settings: a wrong current can overheat a driver.

### What the rebuild took off the screen, and where the mock puts it back

The owner: "Make sure your mockup includes my comments about removal of information since LVGL change, for example the charge mode refactor without justification." Read from the firmware before the rebuild (17316b8: `mui_menu.c`, and the charge's, the cleanup's and Wi-Fi's own screens). The mock is [mockups/parity_12864.png](mockups/parity_12864.png); each restored screen's caption begins "Restored".

| The old screen showed | The rebuild | The mock |
|---|---|---|
| The charge's time, at the title's right: live while it poured, the last throw's while it waited | dropped | at the title's right again, live while it pours, the last throw's while it waits |
| The powder's name, on the charge's bottom line | dropped | on the line under the weight: "of 30.00, AR2209" |
| The target, in the title while it poured ("Target: 30.00") | kept as "of 30.00" | kept |
| The charge's titles: "Waiting for Zero", "Remove Cup", "Return Cup" | "Waiting for zero", "Lift the cup", "Put the cup back" | "Zeroing", "Lift the cup", "Return the cup", "Paused": the ones short enough beside the time (73 px). "Waiting for zero" (81) and "Put the cup back" (82) are not |
| The cleanup's weight, flow, speed and gate | the speed only | all four, the weight and flow on one line above Speed and Gate |
| Settings › Servo Gate: Disable, Close, Open | gone (web only) | Settings › Servo gate: on or off, the shutters open or closed, and their ends |
| Profile Details, two pages: the coarse and fine Kp, Ki, Kd; their minimum and maximum speeds; and the profile's revision and compatibility ("Rev:…,Comp:…") | gone (web only) | Charge › Powder details, read only: Coarse and Fine side by side, then the speeds, then the revision |
| Wi-Fi's link status: down, joining, no IP, up, failed, no network, wrong key | "Joining" whatever the reason | the state in words: "Not joined", "Wrong key" |

**Taken off with a reason on record, and left off:** the scale's calibration (the owner: no scale supports it), Save to EEPROM (D2: stored when confirmed), the knob's zero while the charge waits for the cup's return (a zero there starts a throw on the bare pan: found in the review of gate 4), and the Wireless menu's extra OK (the owner).

**Designed, not in the old screen, and left out of the mock:** the throw's number, which this design listed for the charge's screen. There is no room for it beside the time without crowding the title.

## What leaves the screen, and where it goes

The old menu's forms, and where each went. Read with the survey above, which brings back what the web-only lists here gave up.

| Today | New home | Why |
|---|---|---|
| 1 Main | Main menu | plus the update's row |
| 10 Select profile | Charge › Powder | one row that shows its value |
| 11, 12 Weight | Charge › Target | one editor (W2) |
| 13 Warning, charge | the start's acknowledgement, "Cup on the scale" | kept: a safety warning (the owner, D1) |
| 20 Warning, cleanup | the same acknowledgement | kept (D1) |
| 30 Settings | Settings | |
| 31 Scale, 53 Driver and baud rate | Settings › Scale | a live reading, no OK (W4) |
| 51 Calibration | removed, with its mode | no scale supports it (the owner, 2026-11-17) |
| 32, 33 Profile manager | Charge › Powder | it was a second way to pick the same thing |
| 34, 38 Profile details | Settings › Powder profiles, every field, editable (the survey) | was web only: "read-only gains mean little without the web's context", undone by the owner's rule |
| 37 EEPROM, 60 Save | gone | stored when confirmed (W6, D2) |
| 61 Erase | Settings › Factory reset | |
| 39 Servo gate | Settings › Servo gate, its settings with Open, Close and Position (the survey) | was web only, "for setting up and testing; cleanup opens the gate itself": an agent's reason, never the owner's, undone by the owner's rule |
| 35 Reboot | Settings › Restart | |
| 36 Version | Settings › Firmware | plus the update's rows |
| 40 Wireless, 41 OK | Wi-Fi | a live page (W1) |

**Replaced by the survey above** (the owner, 2026-11-17: every setting is on both). What these lists said, kept as the record of what gate 4 gave up:

> **Web only, and already on today's portal,** so nothing the screen gives up becomes unreachable between this ADR and ADR-008 (`web_portal.html` and `display_mirror.html`, read 2026-11-17):
> - the charge's thresholds, margins, back-off, precharge and colours;
> - the powder profiles' names, gains and speeds;
> - both motors;
> - the gate's duties, its speeds and its manual control;
> - the LEDs;
> - the scale's frame format;
> - the Wi-Fi network, key, security and timeout;
> - the mirror.
>
> **Web only, and new:** the update's channel and source, stored with this ADR's poll and set through the API until ADR-008's page; and the upload by hand (ADR-008).

## The update at the screen

- **The poll:** once each start, quietly, after Wi-Fi joins as a station, and again from Firmware › Check for update. There is no poll on the unit's own access point, which has no internet; the row is hidden and Firmware says why.
- **Found:** "Update available" becomes the first row of the main menu, or "Important update" for a critical release. No pop-up.
- **The offer page:** the version, the first lines of its note (the full note is on the web), Install, and Later. If the last try went back, the page says so ("2.1.0 did not start; this unit is on 2.0.2").
- **Install** runs the update job:
  - downloading, n% (Back stops it, and the free slot is never booted);
  - checking;
  - installing, which cannot be stopped;
  - the restart.
  The unit then runs on trial until it joins its network. Firmware shows "on trial", then "confirmed" or "went back".
- **Shared with the web:** the same job and status serve the web's offer (ADR-008), so there is one implementation.
- **No downgrade:** the offer names only a newer version.

**The channel** follows the owner's idea of a beta channel, as phones have (2026-11-17). Proposed:
- **A stored setting, `channel`:** `stable` by default, or `beta`.
  - Stable reads `releases/latest/download/manifest.json`, which GitHub never resolves to a pre-release.
  - Beta reads the newest release, pre-releases included. The manifest has a fixed name and gives each file's full address (`scripts/release_manifest.py`), so the poll needs only the newest release's tag. How to find that tag cheaply is worked out with the poll: GitHub's list of releases carries every release's notes, while a list of one (`?per_page=1`) keeps the reply small.
- **Leaving beta downgrades nothing.** The unit keeps its version, and nothing is offered until a stable release is newer than it.
- **Where it is set:** on the web and through the API, beside the update's source. The screen sets no source (the owner, 2026-11-16). The Firmware page shows the channel only while it is beta, so a person can see why pre-releases are offered.
- **One stored struct for the channel and the source,** laid out once in a module's own slot. It is built with the poll here; ADR-008's page sets it. The source is GitHub by default, or another manifest's address, such as a mirror on the owner's network.
- **Until every ADR closes, every release is a candidate,** so stable finds nothing.
- **Tests never use GitHub's releases** (the owner, 2026-11-17). The bench points the unit's source at a local OTA server on the PC, which serves `release\` in GitHub's layout and made-up newer manifests (README, gate 1).

## The split (MVC, roughly)

```
 services: charge, cleanup, update, scale, Wi-Fi, motors, settings
     │ status, read as the API reads it          ▲ commands: the API's entry points, by button
     ▼                                            │
 menu tree (a table) ──► navigator ◄── keys: Next, Prev, Enter, Back
                         (pages, focus, the editor, the job on top)
                              │ what to show                ▲
                              ▼                             │
                    view, one per kind of display      input, one per board
                    (128×64 mono; TFT 320 to 480;      (encoder and buttons; touch)
                     AMOLED with touch)
```

- **Model.** The services and their status, read as the API reads them: the charge's status, `update_status()`, the scale's reading, the network, the drivers, the settings tables. Services neither draw nor read buttons; the mode loops leave the menu's task for a task of their own (W5).
- **Commands.** The entry points the API calls (`api_v1_commands.c`), with `by` button. The screen asks the same layer whether a command would be refused, and hides the row if so (principle 3).
- **The menu tree** is one table. Each page and row has:
  - an id (`settings/scale/make`);
  - a label;
  - a kind: page, action, choice, number, toggle or info;
  - what it reads and does. For a setting, that is its resource and field name, so the type, unit, limits and choices come from `settings.h`, described once;
  - when it is shown.
- **Navigator.** The page stack, each page's focus, the open editor, and the job on top. It knows keys, not pixels, and runs on a PC in a test.
- **View.** Draws the navigator's state: a list, an editor, or a job's screen. Only the view knows the display's size, its fonts and its toolkit. There is one view per kind of display, not per board.
- **The display's driver,** under the view: LVGL's frame sent to the panel. The 12864's is our own, for its UC1701, with no u8g2 (the owner, 2026-11-17); a TFT's comes from LVGL's drivers.
- **Input.** One per board, giving the four keys. On touch, a tap is focus and Enter on that row, and Back is a button in the header.
- **A new display** needs a new view and input. The tree, the navigator and the commands stay as they are. A TFT's charge screen with a plot is that view's choice, drawn from the same status.
- **The status is pulled.** The view samples it at its frame rate (the charge's weight is live anyway) and redraws what changed. No new events to keep.
- **LVGL draws; the navigator decides.** LVGL's encoder mode (edit or navigate, switched by its long press) and `lv_menu` would replace MUI's workarounds with LVGL's own, so neither is used.

## What may have been missed

1. **A job started from the web takes the screen**, from whatever page is open, and the page under it is kept. An editor open at that moment is cancelled, with nothing stored. The API's `view` activity then never happens; it stays in the enum, unreported, and v1 stays stable. The owner agrees, and adds that the screen shows who controls it (Status on every screen, above).
2. **"Services stop drawing" means the modes leave the menu's task.** Charge, cleanup, update and the Wi-Fi info stop calling `button_wait_for_input`; the screen sends commands instead. This is the largest piece of work. It touches `charge_mode.cpp`, the file every PR touches.
3. **A start from the web page asks nothing about the cup.** `web_portal.html` starts a charge or a cleanup with no acknowledgement (checked 2026-11-17). The safety warning holds on every way a person starts at the bench, so ADR-008's start asks it too. Whether the API should carry the acknowledgement as well (a field in the start's body that an agent has to state) is for ADR-008 to settle. Handed on to ADR-008 (the owner).
4. **Storing on confirm also stores the web's unsaved changes** to the same resource. That is acceptable, and is recorded here.
5. **The poll and pre-releases.** GitHub's "latest release" is never a pre-release, and every release stays a pre-release until every ADR closes. Until then the poll finds nothing. The end-to-end proof needs either a source on the owner's network (as the Open Weapon Mount Computer's `ota_host.py`) or a candidate channel. The owner's answer is a beta channel (The update at the screen, above).
6. **HTTPS to GitHub.**
   - The asset download redirects to another host.
   - The unit's CA roots expire or change. A unit whose roots fail can never update over Wi-Fi again, so keep more than one root; the USB full install stays the way back.
   - On the Pico W, TLS and LVGL want the same RAM: measure them together.
7. **The Scale page's make applies at restart,** so its reading shows a new make only after a restart. The simulated scale already hands over between drivers live; doing the same for the make would make the page tell at once.
8. **An agent drives the screen:**
   - stable page ids replace the form numbers;
   - a console command `screen` gives the page as text (its title, rows, values and focus), and `key next|prev|enter|back` drives it;
   - `menu_tour` walks the tree from its table and compares both the text and the snaps;
   - the new baseline replaces the old one, and the table above shows that nothing was dropped by accident.
9. **The mirror.** A frame copy suits a 128×64 (1 KB). A 480×320 colour frame is 300 KB, too much for the web's memory. On a large screen the web UI serves people and the screen's text serves agents.
10. **Future displays.**
    - An AMOLED burns in a static menu, so it dims after a time with no input and no job, and the first input afterwards only wakes it.
    - A screen with both touch and an encoder (the TFT35) drives one navigator from both.
11. **Rotation of 90° or 270° makes this panel 64 pixels wide,** and no row fits. Offer 0° and 180° on the 12864; each view gives its own choices.
12. **ADR-010's README says** "New screens or a new menu order: not in this ADR. First the same, then better." This design reverses that. Building the 22 forms in LVGL and then this tree would build the menu twice. The proof changes with it (item 8). The owner agrees with the rework, and wants a better layout proposed wherever one makes better use of the screen (Designing the look, below).
13. **The order of work agreed on 2026-11-17 (010 first) was not yet in the repository.** It is written in with this rework.

## Decided (the owner, 2026-11-17)

- **D1. The warnings before charge and cleanup stay.** "Those are safety warnings, and not technical challenges": the person acknowledges that the cup is on the scale, so powder is not dropped onto the scale itself. Each is one step of its start (the tree above). It was proposed for removal here, which was wrong.
- **D2. A change is stored when it is confirmed,** and Save to EEPROM goes.
- **D3. Storing the target is ADR-014's,** not this ADR's: the weight last used is kept with the charge profile (ADR-014's ideas; api_v1.md's "What it must grow into"). PR #109 was closed because it kept the weight inside the powder profile instead (ADR-002). Until ADR-014 lands, the target stays in RAM as today, and Start opens the target editor when none is set.
- **D4. Measure first.** LVGL (and TLS) are measured on both boards before anything is built on them, and the owner is told if the Pico W does not fit. Trimming LVGL to fit is acceptable, because most of its features cannot show on a 128×64 anyway. What is trimmed goes into the tiered feature table (ADR-005's, now ADR-008's), to be decided when a larger screen comes.
- **D6. The web's missing acknowledgement** is documented in ADR-008.
- **D7. The simulated scale is left out of the UI,** as a debug feature: out of the screen's list of makes and out of the web page's form (ADR-008). The API and the console keep it for the remote control.
- **D8. The screen echoes the unit's state,** as it does today, with marks for the remote control and the simulated scale (Status on every screen).
- **D9. ADR-010 is reworked:** the new menu is built directly, and better layouts are welcome where they make better use of the screen.

## Still open

- ~~**D5. The QR code** to open the page or to join the access point.~~ Moved to ADR-015 (the owner, 2026-11-17: "Perhaps that's a feature for future ADR. I'd like to design the commissioning process properly, not ad-hoc"). Until then the Wi-Fi page gives the address, the name and the access point's key as text, as today, and the screen offers no "more on the web" row: how a person finds the web is ADR-015's.
- **The beta channel** as proposed above, and the remote mark's 30 s.
- **How the look is mocked,** after the measurement (below).

## Designing the look: the menu on the PC

The owner, 2026-11-17: after the measurement, perhaps a better interactive mockup, as a tool for designing the UI without updating the unit.

Proposed: not a separate mock, but the menu's own code built for the PC as well.
- **What runs:** the tree, the navigator and the 12864's view, in the same C the unit runs. The services are replaced by a recording of a unit (as `tests/web_test.py` serves the pages from `tests/web_test_data.json`) or by a small simulation, such as a charge whose weight rises. The keyboard is the knob: the arrows, Enter and Esc.
- **Why the real code and not a drawing:** what the owner settles on the PC is what the unit draws, pixel for pixel, and nothing is built twice. The navigator's tests also run there in CI, and every page's picture can be made there on each PR, without a unit.
- **Two ways to run it,** chosen after the measurement:
  - in a browser, compiled with Emscripten. One toolchain covers Windows and Linux, and the owner, an agent's browser and anyone the owner sends a link to open it as a page;
  - as a window on the PC (SDL). That needs a host compiler on Windows, which `toolchain.env` does not name today.

  Recommended: the browser.
- **The library choice is not waiting on this.** Both LVGL and u8g2 run on a PC.
- **Layouts on the mock.** The owner asked for layouts that make better use of the screen; proposals are shown on the mock, side by side with today's. A starting point: today's title bar takes 13 of the 64 rows, and a list shows four rows.
