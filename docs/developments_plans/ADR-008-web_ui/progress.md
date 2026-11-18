# ADR-008: progress

## Gate 1, the mock

### The radio and today's captive parts, probed on the Pico W (2026-11-18)

What a mock cannot show, seen on the unit first ("To find out first"). The Pico W on the probe, this workstation's own Wi-Fi (a Realtek 8922AE) as the client, the owner's router on 2.4 GHz channels 1, 6 and 11 (a mesh), none on channel 3.

**The probe image:** `-DRC_TEST_AP_PROBE=ON` (`build_w_approbe`), a test build only:
- the start's join skipped, so the unit starts as its own access point (as `RC_TEST_WIFI_FIRST_JOIN_FAILS`);
- the station joins only on the console's `wifi join`, and the access point is kept up when it does;
- `wifi scan` lists the networks heard (`@scan` lines), each BSSID once. Its table is taken from the FreeRTOS heap at the first scan: a table of 24 in static RAM did not link, the Pico W's RAM being full at link time (`region RAM` overflowed by about 1.3 KB). A list of networks in gate 4 takes its room from the heap too.

The settings were copied before the flash: `logs/adr008/picow/before_gate1/`. The logs of the runs are in `logs/adr008/gate1/` and `logs/rc/` (this PC only).

**The access point while the station joins: the client stayed on.**
- The access point started on channel 3, the CYW43's default. The station joined the owner's network on channel 1 in 5 s (`wifi join` to `link up`).
- The access point moved to channel 1 with it, and the client went with it: its link never dropped, its channel read 1 from then on, and its pings to 192.168.4.1 (every 200 ms, 800 ms to answer) missed for under 1 s (16:29:07.9 to 16:29:08.7), then answered in 3 to 8 ms.
- How the client was told of the move (a channel switch announcement, or the client finding the access point again by itself) was not captured. One Windows client is one client: the phones are seen next on the same image.
- With both up, the station reaches the network outward (the poll's GET reached this PC's port 8080 in 216 ms) and is reached inward (`/api/v1` from the PC over Ethernet). Windows kept its default route on Ethernet (metric 25 against the Wi-Fi's 55), so a PC that joins the unit's access point keeps its internet.

**A scan with a client on the access point: it works, and each scan hears part of the list.**
- Two scans, 650 ms and 655 ms each, with the client pinging: no ping missed during either.
- They heard 7 and then 10 BSSIDs (six names), where Windows lists about 18. A list of networks for a person is made from several scans, merged by name, strongest first.

**Today's captive parts, as a client sees them:**

| Asked | Answer |
|---|---|
| Any name, of the unit's DNS (`captive.apple.com`, `connectivitycheck.gstatic.com`, `www.msftconnecttest.com`, `example.com`) | 192.168.4.1 |
| iOS's check, `http://captive.apple.com/hotspot-detect.html` | 302 to `http://192.168.4.1/`, in 29 ms |
| Android's, `http://connectivitycheck.gstatic.com/generate_204` | 302, the same, 21 ms |
| Windows's, `http://www.msftconnecttest.com/connecttest.txt` | 302, the same, 21 ms |
| `http://192.168.4.1/` | 200, the wizard, 7,010 bytes in 58 ms |
| `/api/v1` under another name | 403 `wrong_host` |
| `https://192.168.4.1/`, port 443 | refused (curl's "could not connect"; 2.1 s, Windows trying the refused connection again) |

So each phone's check is told it is not on the internet, and a port 443 that is refused, not left to time out, does not keep a check waiting. Whether each phone then opens its sign-in sheet is the phones' test.

**Found on the way, for gate 2:** `GET /api/v1`, the index, is slow to make. 7,438 bytes in 1.52 to 1.58 s, against the schema's 34,791 bytes in 0.90 to 1.04 s, the settings' 3,844 in 0.19 to 0.24 s and the portal's 93,789 in 0.54 to 0.61 s (three reads of each, over the station, the first byte within 25 ms each time). It is read first by every agent, and by the page's start in the API review's design. The cause is not looked for yet.

### The inventory of today's pages (2026-11-18)

`features.md`: 96 rows, every feature of the portal, the wizard and the mirror, each with its line, the API it uses and where it is proposed to go; read in the code, not in a browser. The agent's sample of its claims, checked again in the code and on the Pico W: the `ETag` (built, a `304` seen), the Erase choice starting at Yes, the stops a page's opening makes.
- The portal's 96 `name="..."` are 79 fields: 71 settings fields in nine resources, the profile's selector, System Control's seven. The schema has three the page lacks: `charge_view`, and the update's `channel` and `source`.
- Left out, proposed: 13 rows. Decided already, 3 (the simulated scale, the two CDNs); ADR-009's, 2 (`plot_weight.html`, not served); the agent's, 8 (the stops a tab or a page's opening makes, the cleanup left running under Settings, dead code).
- **Found, and on main and v2.0.3 today:** System Control's Save, Reboot and Erase each start at Yes (`web_portal.html:753-776`: Yes is each `<select>`'s first option and nothing sets No). An Apply to store the settings also erases the EEPROM; its confirmation lists the erase, with "You cannot undo this action!". After an import the page opens the same confirmation with Erase still at Yes (`:1929-1936`). Not fixed in today's page (the owner, 2026-11-18: no release is made while an ADR is open, so nothing that may be thrown away is fixed); the new pages make it right in gate 3.
- Found: opening the portal, or its Trickler tab, stops any cleanup, one started at the unit too; Purge stops a running charge without asking; under Settings a cleanup goes on, its speed sent again, with no control shown. The gate's ratio always shows 0.500.

### The styles' proposal: the charge's page drawn two ways (2026-11-18)

The charge's page written once (`src/web/charge.html`, `app.js`), with daisyUI's class names, and drawn by two stylesheets: our own plain CSS (`plain.css`) and Tailwind with daisyUI (`daisy.css`). `scripts/web_build.py` joins each page whole and gzips it, as the unit would serve it; `tests/web_test.py` serves the result at `/mock/charge_plain.html` and `/mock/charge_daisy.html`. The tools, for the trial only, not yet in `toolchain.env`: Tailwind's standalone CLI 4.3.4 (111 MB on Windows) and daisyUI 5.7.50 as its plugin file (351 KB), each at least two weeks old; daisyUI works with the standalone CLI as a plain file, no Node (checked).

| | Raw | Gzipped |
|---|---|---|
| The script, shared | 8,467 | 2,656 |
| Our own plain CSS | 4,403 | 1,457 |
| daisyUI, every component (its default) | 377,221 | 46,686 |
| daisyUI, the page's ten components only (`include:`) | 85,452 | 12,899 |
| The page, plain CSS | 16,526 | 5,189 |
| The page, daisyUI trimmed | 97,575 | 16,690 |

- Both drawn at 375 px: the same page, the same controls; daisyUI's own look (numbered steps, a switch for the toggle), and two of its places not yet styled (the purge's heading, the tab bar's labels). Plain CSS at 1280 px in dark: follows the device.
- Driven on the stand-in through the plain page: the cup asked, the charge started, its weight rising, Stop in place of Start, the purge's start refused while a charge runs, stopped. No errors in the browser's console.
- Tailwind alone, with components of our own written in its `@apply`, was not drawn: its components are the plain CSS's, written again, with Tailwind's base and the tool added. It is weighed in words, not built.

**The owner's choice** (2026-11-18): "Plain CSS, but the current page seems ... Plain?", then, before the agent answered: research how the web is designed for phones, tablets and computers, as the screen's skill was. Tailwind and daisyUI are dropped (`daisy.css` removed, the build plain CSS only); the trial's downloads were never put in `toolchain.env`.

### The web's design rules, researched (2026-11-18)

The skill `web-ui-design` (`.agents/skills/web-ui-design/SKILL.md`), in the form of the screen's: the device and where it is used, thirteen rules with their reasons, this project's design language, the process, the sources. What it rests on, read in the primary source where one was reached: WCAG 2.2's target sizes (24 px floor, 44 px for important controls); Hoober's observations of phone grips (49% one hand) and touch accuracy (7 mm in the middle, 12 mm in the corners); Material's window size classes (600 and 840 px) and its dark theme; ISA-101's grey screens with colour kept for the abnormal; NN/g on confirmations (only the irreversible, buttons named by outcome, no default); iOS's zoom into fields under 16 px; captive-portal sheets' limits. Not found in a primary source: a target size for gloves (vendors say 20 mm and up), to be tried with the owner's gloves.

### The charge's page drawn again to the rules (2026-11-18)

- Calm neutral greys following the device's theme; the weight as the one figure in a ring; Start in the ink, Stop in red; colour only for not ready (blue), the verdict (green, yellow, red) and faults, each with its word; the screen's job titles word for word ("Charging", "Lift the cup").
- Widths: a bar at the bottom on a phone, a rail at the left from 600 px, the panel beside its controls from 840 px. Every control at least 56 px, the job's 64.
- Seen at 375 px (light), 768 px (dark, a verdict of Good set by hand for the picture) and 1280 px (light, Under set by hand): the stand-in's throw ends and the next begins within one read, too quickly to catch a verdict on screen. On a phone, Start is on the first screen once the ring was made smaller (250 px). A charge driven through the stand-in; no errors in the console.
- Size: the page 22,844 bytes, 7,264 gzipped (script 3,043, CSS 2,657).

### The Trickler: Charge and Purge on one page that never scrolls (2026-11-18)

The owner: the first tab named Trickler, and no scrolling on a control page. The skill's rule 14 written for it; the page (`src/web/trickler.html`, was `charge.html`) drawn again:
- A switch at the top, Charge or Purge, each a view of the remaining height (`100svh`). The job that runs is the view shown, and the other cannot be chosen; a page opened while a purge runs (started elsewhere) opens on it, at the unit's speed.
- The figure's area is a CSS container: the figure scales to it; with 230 px and more the ring is drawn round it, else a bar under it; under 470 px of view the steps give way to the status. The charge's time moved into the status ("Ready · 16.88 s"). The keep-awake switch left the view for the Unit's page (the wake lock stays on by default).
- The stand-in now holds a throw's verdict for four reads (2 s) and the cup's return for two, as a unit waits for the cup.

| Window | Scrolls | The figure | Ring |
|---|---|---|---|
| 375 x 553 (an iPhone SE with Safari's bars) | no | 62 px (capitals about 7 mm) | no: bar; steps hidden |
| 390 x 664 (a recent iPhone with its bars) | no | larger | no: bar; steps shown |
| 768 x 1024, dark | no | 104 px | yes |
| 1280 x 800 | no | 93 px | yes, the controls beside it |

Driven at 390 x 664: a charge to its target, "Good: lift the cup" in green held, Purge refused while charging; a purge started, its speed stepped, the page reloaded and showing the unit's 1.5 rev/s, stopped. No console errors. Size: 24,901 bytes, 7,877 gzipped.

At the iPhone SE's size the figure (62 px) is under the skill's 8 mm aim (rule 4), above the 4 mm a glance at 70 cm needs: the ring and the steps went first, as rule 14 orders, and nothing else on the view can go. Seen on the owner's phones before it is settled.

### The Trickler in three groups, one column (2026-11-18)

The owner, on the page laid side by side in a wide window: "I don't think in any case the charge weight should be put on the right hand side of the measurement. Check your web element inclusive relationship." Then: "I think the side-by-side design is just wrong", and the top row (the unit's name, "Connected") "is just a waste of space". What was wrong: the target had been grouped with Start and Zero with the powder, so a layout that moved groups moved the target away from its sibling, the powder, to the right of the weight.
- **Three groups** (the skill's rule 15): the charge (Powder, Target), the scale ("Scale", its Zero in the group's head, the weight with its ring or bar), the job (the status and time, the steps, Start or Stop). In that order at every width, one column, centred, no wider than 640 px (rule 1). The purge the same way: its reading (the speed, twice as large: three characters), its job.
- **No top row:** the link is shown only when it is lost, a banner over the page ("Unit not reached. Trying again..."); the unit's name goes to the Unit page (rule 9).
- **Zero in the flow:** laid over the reading, it covered "999.99" on a 390 px phone.

| Window | Scrolls | The weight's figure | The ring |
|---|---|---|---|
| 375 x 553 | no | 40 px ("999.99" fits) | bar |
| 390 x 664 | no | 62 px | bar |
| 768 x 1024, dark | no | larger | yes |
| 1280 x 800 | no | 66 px, inside the ring | yes |
| the owner's pane, 1316 x 1167 | no | | yes |

No console errors. Size: 25,083 bytes, 8,021 gzipped. On the tiniest phone the figure is 40 px: the owner, the same day, "For ultra tiny screen I'm not too worry about the reachability. I want the UI to look right in the first place."

**The step track dropped** (the owner, the same day: "I don't think we need the individual status AND the progress bar at the same time, as both are propagating the same information"). The four steps under the status said what the status word says; the status stays (the screen's words, the verdict's colour, the time), the track goes (`features.md` row 21), and the skill's rule 14 says one element for one piece of news. The weight's ring or bar stays: it says how far the weight has got, which nothing else does. Checked again: 390 x 664, 68 px; 375 x 553, 40 px with "999.99"; 1280 x 800, the ring larger; none scroll. 24,223 bytes, 7,818 gzipped.

### The agent as the lead designer: the Trickler reviewed whole (2026-11-18)

The owner, after a "Scale" tag that labelled nothing and a status alone on its own row: "how about you to evaluate it yourself? ... You should be the lead UI designer." The skill's Process now has the review, element by element and state by state; the agent's own review of the Trickler found and fixed:

| Found | Made |
|---|---|
| "Scale" over the reading: a label of the obvious, there only to hold Zero | gone; the reading's head is its state and its Zero |
| The status alone on a row between the reading and Start | the status in the reading's head, left; Zero right |
| The target twice: in its field, and "of 30.00" under the weight | once, in its field; the weight's unit beside the figure on its baseline, as the screen draws it |
| The last charge's time beside "Ready" | the time only while a charge runs and with its verdict |
| The target's keys live while its field was locked; the setup faded to 38% so it looked broken | the field, its keys and Zero locked together during a charge, at 55% |
| The purge's keys under the reading with a "Speed" label between them | the keys either side of the speed they set, kept in place while idle so the speed stays centred; the line on what a purge does only before it starts |
| The longest status ("Under: lift the cup · 11.80 s") wrapping onto three lines on a 375 px phone | the status never wraps; under 360 px of view the time goes first |
| The purge's reading slid under Stop (a rule from before the card had a head) | the rule removed |

Seen, each on screen with the polling stopped and the state set: Ready, Charging, Good, Under, Over, a fault with its refusal, the cup's question, the unit not reached, the purge idle and running; at 390 x 664, 375 x 553, 768 x 1024 in dark and 1280 x 800. None scrolls; no console errors. 25,008 bytes, 8,034 gzipped.

**Nothing jumps** (the owner, the same day: "a minor coordinate shift for the motor speed between stop purge and start purge"). The purge's line went at the start and the speed, centred, moved up; the same fault's class, measured: the speed's right edge moved 30 px with its sign, the weight's 30 to 40 px with each new digit. Now what a state hides keeps its room, and each figure sits in a field as wide as its longest value, right-aligned as a scale's display. Measured at 1316 x 1167: the speed at (564, 491) idle, running and idle again, its edge at 857 from -5.0 to 2.5; the weight's edge at 878 for "---", 8.42, 18.42 and 999.99. "999.99" still fits at 375 x 553 and inside the ring at 1280 x 800. The skill's review has the check.

**The right-aligned fields undone** (the owner: "Why the numbers are not aligned to the middle anymore??"; the agent had traded centring away for a sideways shift nobody had reported). The figures are centred again, and now the number alone: its unit hangs off its right, so "30.00 gr" no longer sits 17 px left of the middle. Measured at 1316 x 1167: the card's middle at 505, the weight's at 504 to 505 for "---", 8.42, 30.00 and 999.99, the purge's speed at 504, idle and running at the same height (464), "rev/s" 6 px from the speed. A figure moves sideways only when its count of digits or its sign changes, about once a throw: accepted.

### The whole portal: Powders, Settings and Unit (2026-11-18)

The owner: "I think that's good for now, and mostly importantly no worse than the previous product. Now continue", after the agent recommended one set of pages for every device over a separate PC console (a PC-first page comes with ADR-009's charts, as a page of the same portal). The portal is now one page (`src/web/index.html`) and a script in parts joined by the build: `common.js` (the API, messages, the router, the confirmation, files), `trickler.js`, `form.js`, `settings.js`, `powders.js`, `unit.js`, `main.js`.
- **Settings:** the unit's parts as a list (Scale, Charge, Motors, Servo gate, Lights, Display, Wi-Fi, Updates), each opening its form; on a window from 1000 px the list beside the form. A part the unit does not carry is absent from the list.
- **The forms are drawn from the schema,** none written by hand: each resource's groups, a control by type (a switch; a choice as segmented buttons when it has up to four short options, else a list; a number with its unit; text; a colour), its `about` and its limits under it, "takes effect at restart" where it applies so. A change is written at once and is live; a bar says "Changed, not stored" with one Store while the resource is not stored; a refusal is said under its field and the field shows the unit's value again. The simulated scale is left out of the scale's makes. The schema's unit "weight" is shown as the scale's own unit, not as a word.
- **Powders:** the eight as a list, the one the charge uses marked "In use", each opening its form, with "Export this powder" and "Import into this powder" (rows 64, 65); on a wide window the list is a table of every powder's coarse and fine Kp, Ki, Kd side by side.
- **Unit:** its facts (name, firmware, board, id, network or hotspot, address, signal in words, time running); the update (what runs, the last check, the offer with its note and Install, Check now, Install from a file naming the file this unit takes, where updates come from); the unit's screen, live, read once a second while shown; this browser's "Keep this screen awake"; Backup (export, restore from a file, stored after); Restart and Factory reset, each confirmed with buttons named by their outcome ("Erase every setting", "Keep my settings").
- **Size:** the whole portal 67,502 bytes, 18,081 gzipped (script 12.1 KB, CSS 4.0 KB gzipped): less than today's three pages (18.6 KB gzipped), under the budget of about 50 KB.

The agent's review (the skill's Process), and what it found and fixed:

| Found | Made |
|---|---|
| The forms blank: a variable used before it was declared (`form.js`) | declared first |
| Each field's label squeezed into a narrow column: the Trickler's `.field` rule (label and control side by side) applied to the forms too | the Trickler's rule scoped to its card |
| "5 weight": the schema's unit for "in the scale's unit" shown as a word | no unit word; the help line says "in the scale's unit" |
| An on or off setting drawn as a bare checkbox: the switch's style had left with the keep-awake switch | the switch restored for every form |
| On a phone, an opened part's name at a heading's size, though it is the page's title there | the page's title size |
| On a wide window the powders' table ran under the form (its fine Kd hidden) | a wider list column on that page; the list scrolls sideways before it collides |
| The forms' lists with the browser's arrow, the Trickler's with ours | one list style |
| A number's field stretched across a tablet for three digits | as wide as a number needs, its unit beside it |

Driven: a setting changed (live, the Store bar shown), a value out of its limits refused under its field; the Trickler's charge run in the whole portal, left for the Unit's page and picked up again on return. Seen at 390 x 664 (every page), 768 x 1024 in dark (Motors) and 1280 x 800 (Settings and Powders side by side with their lists). The stand-in does not make the update's actions or a restart: those answer "an action this stand-in does not make".

**Not yet:** the check that every row of `features.md` is marked in the pages (only the Trickler's views carry their rows); the pages at 375 x 553 and the owner's pane; the set-up (commissioning, gate 4's design); the stand-in's update flow.

### Where the person is, said once; the portal reviewed against the skill (2026-11-18)

The owner, on the whole portal: "Everything works almost fine, except for the view with the side bar, and the section title. It is another case of duplicated information." On a wide window a part's name was said four times in one stack: the list's marked row, the part's title, its first group ("SCALE") and its first field ("Scale"). The same class, looked for on every page:

| Found | Made |
|---|---|
| The part's title beside the list's marked row | no title side by side: the row is it. On a phone, where the part replaces the list, its name stays over it, under "‹ Settings" |
| Every page but the Trickler titled under the navigation that marks it ("Settings" by the rail's Settings) | no page title: the navigation says it, as on the Trickler |
| A group named as its resource (Scale, Gate, Wi-Fi, Display, Update, Motor, Profile) | no heading: the resource's own fields first. A field with no group draws none |
| A field named as its part (the scale's "Scale", the gate's "Servo gate", the Wi-Fi's "Wi-Fi") | "Make" (the screen's own short label already), "Fitted", "Join a network" |
| A powder's name as its title and again as its first field | no title |
| "Coarse trickler", "Coarse trickler motor", "Powder profile", "LEDs": not the words | "Coarse motor", "Powder", "Lights"; the powders' help lines say "motor" |
| The two motors stacked, each under a title, the longest page | one at a time, by a Coarse and Fine switch as the Trickler's |
| On a wide window, Settings and Powders opened on an empty pane saying "Choose a part" | the first part open, and the powder in use |
| The Unit's firmware version twice (the facts and the update); the facts under "This unit" with a "Name" row | once, in "Update" (the words' name, not "Firmware update"); the facts under the unit's own name |
| A page opened scrolled where the last one was left | every new address starts at its top |
| The switches, the forms' segmented choices, Zero and "‹ Settings" 44 px tall (rule 2: 48) | 48 px; the Trickler still fits 375 x 553 with no scroll, its figure 40 px as before |

The words are the schema's, from the C tables the screen reads too: the mock shows them from `PROPOSED_WORDS` in `tests/web_test.py`, put into the recorded schema, and gate 2 writes them into the tables (README, "The words"). Each entry is checked against the recording, so once the unit says them the stand-in refuses to start until the list is emptied. Labels the screen shows flat, with no group, are kept: "Coarse Kp", "PWM3 LEDs".

The skill has the rule (16, "Say where the person is once"), the review's check that its numbers hold (measured, not read from the CSS), and the switch for one of two alike things.

Seen: 1280 x 800 (Settings with Scale open, Motors, Wi-Fi; Powders with the powder in use; Unit), 390 x 664 (Motors, a powder, the Unit, a new page at its top), 375 x 553 (the Trickler's Charge and Purge, no scroll), 768 x 1024 in dark (Servo gate, Lights). No console errors from the page. The whole portal 69,377 bytes, 18,771 gzipped.

Found and left, being the unit's words for gate 2: the gate's shutters in "of the PWM's period", and the motor's gear ratio's line, whose "trickler" is the tube.

### The offer drawn; "Not stored" at once; Undo changes (2026-11-18)

The owner asked how a new update is shown, why "Not stored" showed in the list only after leaving the part, and how a change not stored is undone; then: mock the offer, fix the list, and "Perhaps add a button next to Store button to reset?"

- **The offer.** The stand-in's poll now finds a release: "Check now" says checking, and 1.5 s later `found` is 2.1.0 with a note, as `GET /api/v1/update` gives it; `tests/web_test.py --offer normal|critical` starts with one found. The Unit page's first card is now Update, always, so the dot on the navigation leads to it and nothing moves when an offer comes (it had moved to the top under the reader, against "nothing jumps"). The offer: "Version 2.1.0 is available", its note, Install; a critical one edged in red with "Important". The page read `found.critical`, which the API never gives: it reads `importance` now. From every page, a dot on the Unit's icon while an offer waits, red for a critical one, read at each page shown (the event stream carries it in gate 2). Seen at 390 x 664 (normal, critical) and 1280 x 800 in dark.
- **"Not stored" at once.** The list was drawn once, when the page opened; a write changed only the form's bar, so on a wide window the list said the opposite of the form. Now each write's reply goes to whatever else shows the resource: the settings' list, the powders' list and table (a figure changed shows in the table as typed). No read more: the reply has it.
- **One record, one "Not stored"**, at first: the eight powders are one EEPROM record, as are the two motors, and the unit's `saved` is the record's CRC, so a change to one powder made all eight not stored; the stand-in was made to say so. The owner, the same day: "in settings the unsaved settings will have Not stored, but the powder profile won't, and I think that's the gap" (on a wide window the powders' table had no tag at all, and a tag on all eight would have said what is not true of seven). The cause is the unit's: it cannot tell which powder changed. Proposed for gate 2 (README, the API review, item 8): `saved` for each resource, against its own part of the record. The stand-in does it now; the page tags the powder changed, in the list and in the table beside "In use", and a motor's change tags Motors. Driven: a powder's Kp changed (its tag alone, at once, on a wide window and on a phone after a reload), typed back to its stored value (the tag and the bar gone); a fine motor's gear ratio changed and typed back, the same.
- **Every field's id was twice on the Motors page**, both motors' forms being on it: a Fine field's label pointed at the Coarse field. Each id now carries its resource's path.
- **Undo changes**, beside Store: a revert of the slot (README, the API review, item 7), proposed for gate 2 and made by the stand-in now. Driven: a choice changed (the row's tag at once), undone (the value, the tag and the bar back as stored, "Changes undone: as stored."); a powder's figure changed and undone, the figure stored earlier kept. On a 375 px phone the bar's words take a line and its two buttons the next.

Fixed on the way: a store's or an undo's message was cleared by the redraw it starts (the page is drawn first now).

The whole portal 73,044 bytes, 19,779 gzipped.

### The update's life cycle read whole (2026-11-18)

The owner asked how the web shows an upgrade, then to challenge its life cycle and its failure modes, and whether the firmware reports its status during one. Read in the code, not tried on a unit, and written into the README ("The update's life cycle, read whole": its stages and five gaps), to be built in gate 2 (the API review, item 9), at the owner's word: "Record in ADR-008". The worst: a trial writes a changed struct's defaults into the EEPROM before it is confirmed, so a going back can lose that module's settings, and a changed Wi-Fi layout confirms on the unit's own hotspot. It is built before any release that changes a stored layout.

### Next in gate 1

1. The iPhone on the probe image, joining the access point, then the station joined with the phone on it: deferred by the owner (2026-11-18) to when the owner is at the unit, at the start of gate 4 at the latest. No Android phone: taken to work the same, not tested (the owner).
2. The check that every row of `features.md` is shown in the pages or marked left out (only the Trickler's views carry their rows yet).
3. The stand-in's update flow after Install (the download, the install, the restart followed, and each end in the life cycle's table), and the page's view of it: "keep the power on", how far it has got, the end in plain words (README, "What the page has to do" and "The update's life cycle, read whole"). The offer itself is drawn. The ends the unit cannot report yet (a power cut in a trial, why a back) are drawn as gate 2 makes them.
4. The set-up's pages (commissioning), drawn as the Windows client and today's code show.
5. The owner settles `features.md`'s proposals to leave out, and gate 1 by using the mock.
