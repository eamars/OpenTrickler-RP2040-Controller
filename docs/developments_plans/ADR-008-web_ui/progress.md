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

### Every feature placed: the features.md check (2026-11-18)

The owner chose it before the update's view ("features.md first"). Each of features.md's 96 rows, and its nine new items (now N1 to N9), was read against the mock's code, then marked where the pages show it: a comment `features: <row>` beside the code that does it, in `src/web/`. `tests/features_check.py` reads both and lists every row as shown, left out (by its "Proposed" column) or still to come with the gate that brings it (its `TO_COME`); it fails on a row in none of them, on a mark of a row left out or to come, and on a mark of no row. With `--final`, the proof of gates 3 and 4, a row still to come fails too, so the list cannot outlive its gates.

Read against the code, nine rows were not in the mock, though a page had been taken as covering them:

| Row | Missing | Made |
|---|---|---|
| 35, 36, 37 | The gate's Open and Close, its position, and its state as the unit reads it | In Settings, Servo gate, over its settings: how open (0 to 100 %, as a person reads a gate; the API's ratio is how closed) between two keys, Open and Close; locked while it is not fitted, with why; "Moves it now; nothing is stored." The stand-in's gate follows its Fitted, as the unit's does |
| 58 | When a setting takes effect, but for "at restart" | The schema's `applies` in each help line unless it is at once ("Takes effect the next throw") |
| 61, 95 | The tuning guide | A link under the powders, "on GitHub: needs the internet" |
| 66 | The build's type and commit | "Build: Debug, 3ce28b4" among the unit's facts |
| 67 | Save All | Proposed left out: each part stores from its own bar and the list shows which are not stored; a restore stores all by itself |
| 73 | A backup from before API v1 | Restored as today's portal does, its old codes read as v1's fields (checked: the scale's make, a decimal place, a gate not moved, a powder) |
| 88 | The screen's size and format | Read from `GET /api/v1/display`; a format not known is said, not drawn wrong |
| 94 | The page's icon | The Trickler's icon, inside the page |

Row 74 is now what the mock does: an import stores what it wrote and says a restart may be needed; nothing erased, nothing else asked. A help line's second sentence now starts with a capital ("Takes effect at restart").

The check: 105 rows, 71 shown, 14 left out (3 by the owner, 2 ADR-009's, 9 proposed), 20 to come (the set-up's 13, gate 3's routes and ETag, the update's view, gate 2's echo and report). `VERDICT PASS`; `--final` fails on the 20, as it should now. Seen at 390 x 664 and 1280 x 800 in dark; no console errors. The whole portal 82,429 bytes, 22,699 gzipped.

### The update's view: the job, the restart followed, the end in words (2026-11-18)

The owner: "Yes, the install". The update is drawn whole, from Install to how it ended, on the life cycle read the same day (README, "The update's life cycle, read whole").

- **The stand-in plays the job** on a clock, ADR-004's times made shorter: starting, connecting, the download as bytes against the manifest's size, the check, the install, then a restart in which nothing answers (each connection closed with no reply, as from a unit gone), then the new image on trial and confirmed. `tests/web_test.py --update-ends confirmed|back|refused|power` picks how it ends; the upload by hand (`PUT /api/v1/update/image`), Stop and Restart (gone 5 s, back on a new boot) are played too. It gives `back_within_s` while installing, ADR-008's proposal for gate 2, and refuses to start once the unit gives it.
- **The page** (`src/web/update.js`, the update's part moved there from `unit.js`):
  - the Update card becomes the job while one runs, in the screen's words (`menu_nav.c`): Connecting, Downloading 2.1.0 and its percent with a bar, Checking, Installing; "Keep the power on."; Stop until it installs, its place kept when it goes;
  - every page but the Unit's shows a strip with the job, how far, and its Stop: the charge's ("Charging") and the purge's too, on the pages that are not the Trickler (the skill's rule 9, which no page had built). On the Trickler, an update's strip takes the switch's row: the switch is of no use while the update holds the unit, and the figure keeps its 40 px on a 375 x 553 phone;
  - the restart is followed: "Restarting on 2.1.0" and no "Unit not reached" banner, then "Starting 2.1.0" while on trial, to a new boot. Kept: the page loads itself again on the Unit page (the new firmware serves its own pages), whose card says "Last update: 2.1.0 installed." Gone back: "2.1.0 was not kept: the unit went back to 2.0.2". A power cut in the trial, which the unit cannot tell (gap 2): the page's own "2.1.0 was not kept: the unit is on 2.0.2. The power may have been cut." Refused: in plain words by its code ("the file is for another board"). Stopped: "Nothing installed.";
  - every client follows a job whoever began it (the screen, another page, an agent): the watch reads the unit's own state once a second, which gate 2's event stream replaces;
  - Install from a file sends the image with its progress ("Sending <file>");
  - Restart is followed the same way to its new boot, "Restarted." (`features.md` row 68).
- **Found and fixed while driving it:**

| Found | Made |
|---|---|
| A file was sent at once after the start; the unit takes one only once its job is receiving (it is starting until the menu begins it), so the upload was refused and the job left waiting, the page silent | the page waits for receiving; a send that fails other than by the image's refusal is said and the job stopped |
| "Restarting on 2.1.0" for a file, whose version is not known before the restart | the version only from a download of the release found; a file restarts "on the new firmware" |
| "Checking for newer firmware." left as a message beside the card's own "Checking..." | the card alone says it |
| How a job ended said twice on the Unit page, by a message and by the card's "Last update" | the card there, a message elsewhere |
| The last update's line under the offer | above it: what happened to what runs, before what is offered |
| "Installing · 100 %" and "Checking 100 %" | a percent only while bytes come; its place kept |
| The card's "%" placed off by the Trickler's unit style | a style of its own |
| The one confirmation's action red for an install and a restart, red being for stop and faults | red only for what loses something (the factory reset); the action's colour otherwise |
| "Keep running" wrapping onto two lines in its dialog | a dialog's two buttons side by side where they fit, stacked where they do not, never wrapped |
| With the strip on the Trickler of a 375 x 553 phone, the weight squeezed under Start | the strip takes the switch's row (above); a smaller figure, tried first, would have shrunk the 40 px the owner accepted, and was undone |

Driven at 390 x 664 and 375 x 553 (each end: kept, gone back, refused, a power cut; a file refused and a file kept; Stop; Restart; a charge's strip and its Stop from Settings), 1280 x 800 in dark (the job's card). The page logs no error of its own; the browser logs each connection the stand-in drops while it restarts. `features_check.py`: 105 rows, 74 shown, 14 left out, 17 to come, `VERDICT PASS`. The whole portal 99,489 bytes, 27,234 gzipped.

### The set-up drawn (2026-11-18)

The owner settled the word ("Keep commissioning in the ADR, Set up on the page", written into "The words") and asked to continue with the design. The set-up is drawn in the mock (`src/web/setup.js`), its four steps and its proposed API written into the README ("The set-up as drawn"). The stand-in plays a unit never set up with `tests/web_test.py --hotspot`: on its hotspot, six networks heard, a join that takes 4 s, refused for the key `wrongkey`, a scale that talks as the A&D and is silent as any other make.

Driven at 390 x 664: the root opened on the set-up by itself; a network chosen (the cursor in its key), a wrong key said at once and the key ready again, the right key joined with no restart; the scale's live weight, a silent make told after about 3 s with its speed; both drivers found; Ready, then Finish and the unit's address on its network. Then the root again: the Trickler, not the set-up. A second unit, every step skipped, "Start using it" on its own hotspot, to the Trickler, and not offered again. 1280 x 800 in dark: one column, no navigation. No console errors.

Found and fixed while driving it:

| Found | Made |
|---|---|
| A finished scan drew the list again with nothing chosen, and a page shown again (a phone woken) was drawn afresh: a key being typed was wiped | the step keeps what is chosen across its redraws; the set-up resumes when shown again, its reads going on, rather than being drawn again |
| The lock on a network that needs a key not drawn (an SVG made as HTML) | drawn in SVG's namespace |
| The make's list in the browser's bare style | the forms' style |
| "Stored." and "On it, the unit is ..." after a join | "Its address there: ..." |
| "Wi-Fi" wrapping in the Ready facts, beside "Smith home, as opentrickler-9CE0.local" | the network's name alone (the address on the last step); a fact's name never wraps |
| "Driver found" in green: normal is grey | no colour; a driver not found in red |
| The last step's heading "Set up" under "Set up this unit" | "Done" |

`features_check.py`: 105 rows, 85 shown, 14 left out, 6 to come (gate 3's routes and ETag, gate 2's echo and report), `VERDICT PASS`. The whole portal 120,762 bytes, 32,546 gzipped, under the budget of about 50 KB.

### The set-up on the screen drawn (2026-11-18)

The owner: "Yes, the screen". The 12864's half of the set-up drawn in the screen's grid and faces (`tools/ui_mock/setup_12864.py`, a sheet of its own beside the menu's, `mockups/setup_12864.png`), written into the README ("The set-up as drawn", on the screen): the main menu's "Set up this unit" row, the code that joins the hotspot with the text beside it, the screen once a phone has joined, the Wi-Fi page on the hotspot with its key and "Join by camera". `segno` 1.6.1 makes the code (in `requirements.txt`); the key is an example, the unit's own made from its chip.

The agent's review: the text beside the code was drawn half off the glass (its first line's baseline taken before the faces were loaded), now centred over Back; every other line holds its width (measured: the widest, "or open 192.168.4.1", 96 of 128 px). The code checked: OpenCV's reader (installed for the check, then removed: not one of the host tools) decoded the drawn screen, and the panel alone inside a dark bezel, though its quiet zone is 1.5 modules. The phones decide at the unit.

The code's screen, a seventh kind, allowed by the owner the same day ("Allow it"): the screen's skill has it as rule 5's seventh kind, with Back at the right on it alone.

### Gate 1 settled (2026-11-19)

The owner: "I have reviewed the mock up, but I'm not going to review on my mobile device, and I'm happy to go ahead to the next stage of the development." Read here as:
- **The look and the styles:** plain CSS of our own, no tool at run time (`scripts/web_build.py` joins and gzips); the portal as the mock draws it, 120,762 bytes, 32,546 gzipped, under the budget of about 50 KB.
- **Left out:** every row `features.md` proposed to leave out is decided (10, 11, 12, 38, 45, 67, 75, 76, 77). All but one are dead code or a stop no one asked for; row 67, the settings' "Save All Changes to EEPROM", is a control taken away, each part storing from its own bar.
- **The keep-awake switch** kept by the browser, as proposed.
- **Commissioning** as drawn, on the page and the screen.
- **Not done in gate 1, carried:** the owner's own phone and tablet (the three widths were driven in the agent's browser only); the keep-awake video and its battery on a real phone; the iPhone's sign-in sheet and the code read by its camera. All are at the unit with a phone, at gate 4's start, with the iPhone on the probe image ("To find out first"). A page that reads badly on a phone is seen there at the latest, on the pages then built.

`features_check.py`: 105 rows, 85 shown, 14 left out (all decided), 6 to come, `VERDICT PASS`. The Pico W image builds (RAM 99.68% at link).

## Gate 2, the API review

### Next in gate 2

1. The plan written into the README ("Gate 2's plan", 2026-11-19): six steps in order, each proven on the Pico W before the next. Approved by the owner 2026-11-19 ("I approve the gate 2 and now proceed"), with `leds`, `save` and `powder_profiles` kept in the API. The whole ADR goes by one PR, #134, a draft until the ADR closes (the owner, the same day).
2. Step 1, the words: built and proven on the Pico W (2026-11-19).
3. Step 2, a setting changed as an event: built and proven on the Pico W (2026-11-19).
4. Step 3, the event stream: built and proven on the Pico W (2026-11-19).
5. Step 4, `saved` for each resource and the revert: built and proven on the Pico W (2026-11-19).
6. Step 5, the API that describes itself and the unit's report: built and proven on the Pico W (2026-11-19).
7. Step 6, the update's life cycle: built and proven on the Pico W (2026-11-19).
8. Gate 2's whole proof on the Pico W: passed (2026-11-19), below.

### Step 1, the words: built (2026-11-19)

- **The API:** `cleanup` is `purge` (its four paths, `activity`, its events' source, its state's kind, `no_purge_runs`, the guide's and the schema's lines); `system/erase` is `system/factory_reset`, confirmed by `factory_reset`, answered `{"resetting": true}`. The handlers' names follow. `leds`, `save` and `powder_profiles` kept (approved with the plan).
- **The settings' words** in the C tables, as the mock showed them: a group named as its resource gone (the scale's, the gate's, the Wi-Fi's, the display's, the update's, the motors' "Motor", the powder's), the scale's "Make", the gate's "Fitted", the Wi-Fi's "Join a network", "Coarse motor" and "Fine motor", "Powder", "Lights", the lights' group "Display board", and "trickler" out of the motors' and the powders' lines. The two labels the screen shows by their label, the gate's and the Wi-Fi's switch, keep the screen's word as their `short_label`, so the screen is unchanged until gate 3.
- **Every reader in the same change:** `rc_run.py` (`api_check`, `charge_fault purge`, its `--purge-speed`), `unit_api.py`, api_v1.md (with a note of the renames; the review's record keeps its words), AGENTS.md, the new pages, and today's portal's fetches. The stand-in's `PROPOSED_WORDS` emptied and taken out; its recording made as the renamed firmware answers, until the unit is recorded again, whose diff then checks it.
- **Checked off the unit:** both boards build (the Pico W's flash 96.92% of its slot, RAM 99.68% at link, as before); the mock on the stand-in at 390 px: a purge asked with the cup, started, its speed sent, the strip "Purging" on Settings, the gate's "Fitted" and the scale's "Make", the stop and its `purge` events, the factory reset by its new path; `features_check.py` `VERDICT PASS`.
- **Proven on the Pico W** (the bench given by the owner, 2026-11-19): its settings copied before (`logs/adr008/gate2/step1_before`), `flash` (2.0.3-39 at 277b002), the settings the same after; `menu_tour --compare` `VERDICT PASS` (the charge's target not judged, as always); `api_check` `VERDICT PASS` on the new names (`purge/start`, its `purge` events by api and by the unit at its time, `system/factory_reset` refused without its confirmation); `settings_roundtrip` and `charge_cycle` `VERDICT PASS`. The stand-in recorded again from the unit: against the recording made by hand, only what changes from run to run (the version, the uptime, the unit's current choices), the order of two keys, and three lines the unit makes from the labels ("stores Lights", the motors' list), which the hand had missed; the unit's is kept.
- **Found:** the Pico W's stored `invert_display` is on (the copies of 2026-11-18 and today), and its menu baseline was saved with it off: every page of the first tour differed only by that. The tour was run again with it off, live and not stored, and put back on after. Which is wanted is the owner's: the setting stored off, or the baseline saved again with it on.

### Step 2, a setting changed as an event: built and proven (2026-11-19)

- **What:** two events of a new source, `setting`. `changed` (`by`, `path`): a resource written and what it holds changed, from the one place every write goes through (`settings_patch`), so the API's PATCH, the console's `setting` and the menu's rows all give it; a write that changes nothing gives none. `stored` (`by`, `path`: the save's): a store of what was not stored, from every save (the API's, the save of all, the menu's, the console's). A powder's path carries its id. The schema and api_v1.md list both; the stand-in gives the same on its writes and saves.
- **Proven on the Pico W** (2.0.3-41 at 80bac33): from the console, a change (`by` console) and the same value again (no event); from the API, a change back (`by` api, `saved` true again), a powder's name written as it was (no event), a save of what was stored (no event); the console's change and save twice (`changed`, `stored`, each `by` console); a powder's name (`/api/v1/powder_profiles/3`); the menu's Servo gate switched on and off (`changed` and `stored`, each `by` button). `api_check` and `settings_roundtrip` `VERDICT PASS`; the settings the same as the copy before step 1. The stand-in recorded again.
- **Not an event:** the powder chosen for the charge (the charge's, not a setting: it comes with the charge's state on the stream, step 3).

### Step 3, the event stream: built (2026-11-19)

- **In the server** (`http_server.c`): a reply that stays open, `http_server_reply_live`. It is a chunked stream whose `more` may give nothing for now; the slot then waits (`SLOT_WAITING`), out of `select`'s writes and off the stall clock, until its `ready` says there is more, looked at each 50 ms tick. Bytes the client leaves unread are the stall limit's as any reply's. At most 3 at once (`HTTP_SERVER_LIVE_MAX`, beside the 2 parked), so one of the 6 slots is always left for a request; a fourth is answered 503 `busy` with `Retry-After: 5`. `Last-Event-ID` is read as `If-None-Match` is; a client that sends on a stream is closed. `net` and `GET /api/v1/debug` give `live`, `live_opened`, `live_refused`. The server knows nothing of server-sent events: that is the API's.
- **`GET /api/v1/events/stream`** (`api_v1_commands.c`): `retry: 3000`, `unit` (`boot`, `first`, `newest`, `resumed`), `state` (the job, the charge as its GET with its weight, the purge), then each event with `id: <boot>-<seq>`, the state again when it changes (no oftener than every 100 ms; a cheap signature of the modes' status is compared each tick, the message made only when it differs), and `: quiet` after 15 s with nothing sent. Its state is 20 bytes in the slot. A path of its own, not `Accept` on `/api/v1/events` (the README's table says why).
- **Flash:** 4.1 KB (the Pico W's image 97.42% of its slot, some 26 KB left).
- **`rc_run.py stream_check`** walks the life cycle on the unit; on the Pico W (2.0.3-42 at ce0de30) every stage passed, `VERDICT PASS`: the start; an event 30 to 50 ms after the setting changed; a reconnect from its last id going on with the event missed; after 70 events, the old id read afresh (`first` 10, `resumed` false); the simulated scale's weight on the stream 0.13 s after it was set; the comment at 15.0 s quiet; three streams taken and a fourth refused 503 with `Retry-After 5`; a client that stopped reading closed by the stall limit about 14 s after it stopped, `aborted_stalled` 0 to 1, lwIP's heap in use 1,028 bytes before and after; a Wi-Fi leave and a restart each leaving the old stream silent (20 s with nothing, not closed), the reconnect after the leave going on from its last id (same `boot`) with the old slot freed, after the restart read afresh (a new `boot`).
- **Found while proving it:** a unit that restarts or leaves its network sends no close: the stream goes silent. The comment after 15 s is then the client's cue, and the page (gate 3) opens the stream again after some 20 s of nothing; a browser's `EventSource` alone may wait far longer. And it does not try again after an error status, so the page retries a 503 itself. Both are in the README's life cycle.
- **The stand-in plays it** (`tests/web_test.py`), as the unit: the unit, the state, the events with their ids, the comment, 503 past three, and silence on a restart. The pages still poll: gate 3 moves them onto it. (The stand-in's charge moves on with its reads; a stream reads it ten times a second, which gate 3 looks at when the pages follow the stream.)
- **`soak --stream`:** each of the two browsers follows the stream in place of polling the charge every 500 ms, judged as a follower is.
- **The soak against polling, and what it found** (Pico W, 10 minutes each, 27 charges each, two event followers on held reads throughout). First, at ce0de30: polling `VERDICT PASS` (0 of 3,698 reads missed); streams `VERDICT FAIL`, 20 of 1,330 other reads missed (19 resets, 1 timeout), though each stream saw all 162 events and some 2,100 states. `closed_for_room` was 19, the resets exactly: with two streams and two held reads in four of the six slots, a new connection made room by closing the idlest one waiting for a request, and that could be one just taken whose request was still coming. The invariant broken: the server never closes a connection it has not answered. Made so (`served`, set by a reply): only a kept-alive connection already answered is closed for room; with none, the new one waits in the listen backlog. It was the server's (ADR-003's) and only the stream's load showed it, so it is mended here.
- **Again on the mended server** (2.0.3-43 at 8e6281b, the fix beside it): `stream_check` `VERDICT PASS`; streams `VERDICT PASS`, 0 of 1,337 reads missed, `closed_for_room` 0, each stream every one of 162 events, some 2,150 states; polling `VERDICT PASS`, 0 of 3,697; `heap_free` quiet before and after the same (50,288) in both, lwIP's heap never failed (`heap_err` 0, the most in use 17,968 of 40,960 over both runs). On the stream the two browsers made 2,360 fewer requests in ten minutes and saw the charge about 3.5 times a second against twice. The cap of 3 streams and 2 parked stays: nothing in the soak asked for more.

### Step 4, `saved` for each resource and the revert: built and proven (2026-11-19)

- **`saved` for each resource** (README item 8): a powder, or a motor, is saved when its record is as stored (the record's CRC, as before), or else when its own part is as the EEPROM holds it, read then and only then. No copy of what is stored is kept to drift: the EEPROM is the truth. Each table names where its part lies in the EEPROM (`addr`; a powder's `profiles[id]`, a motor's `motor_data[i]`). No stored layout changes; RAM none.
- **The revert** (item 7): `POST <the save's path, /revert>`, every resource stored there given back what the EEPROM holds, each part as a write gives it: its refusals looked at first and refused whole (409 `busy`), what is stored checked as a value would be, its `apply`, and its `setting changed` event where it changed. In the schema (`revert`, and an action), the index ("puts back what is stored of ..."), the console (`setting <path> revert`), api_v1.md and AGENTS.md. The stand-in's `propose_revert` taken out: the unit makes it.
- **The menu's powder chosen** is stored through the settings (`settings_store`), not by the profile's own save, so it is a `setting stored` event `by` button as every other store.
- **Found and mended on the way:** the schema wrote every save in one chunk, and with a revert beside each it overflowed (the reply cut off): now one save a call. The server's 80 routes were full with the reverts (`PUT /api/v1/debug/upload` lost at the start, printed): the table is now from the FreeRTOS heap, 112, as the slots are (static RAM 99.68% to 98.95%). `api_check` and the stand-in's recording read every GET path to its end, the stream never ends: `api_check` reads it as a stream (the unit, then the state), the recording leaves it out. And `api_check` run just after a start met the start's own poll (409 `busy` on Check now): it waits it out first.
- **Flash:** 2.4 KB (the Pico W's image 97.65% of its slot).
- **Proven on the Pico W** (2.0.3-45 at 546c807, with these changes): a powder's name changed, that powder `saved` false and another true; the powders reverted, the name back and its event; the coarse motor changed, the fine one `saved` true; the motors' revert refused 409 during a purge, taken after it, `saved` true again; the menu's powder chosen, `setting stored` by button (and put back). `api_check` (every revert called), `settings_roundtrip`, `web_robust`, `charge_cycle` and `menu_tour --compare` (the screen's invert off for it, live, put back) `VERDICT PASS`; `api_check` again straight after a restart, `VERDICT PASS`. The stand-in recorded again.

### Step 5, the API that describes itself: items 1 to 5 built and proven (2026-11-19)

- **The index's slow making, its cause:** a path a chunk. Some 50 chunks of a hundred bytes, each its own segment and its own wait for the client's ACK: the time was the chunks', not the bytes'. Read again: the schema's 34.8 KB came in about 25 full chunks in 0.9 s, some 30 ms a chunk either way. Now as many items as fit a chunk: 8.9 KB in 0.25 to 0.32 s (three reads), from 1.5 s. Step 4's saves, made one a chunk to mend the overflow, had slowed the schema the same way (1.2 to 1.3 s): packed too, 37.9 KB in 1.09 to 1.17 s, the same as recorded byte for byte.
- **A GET of every action gives its usage** (item 2): the server asks the API (`http_server_set_usage`) before it answers 405 to a GET; the API gives the action from the schema's own text, with an `example` (written into the schema where a body is more than `{}`: the charge's start, the update's start, the factory reset, hello) and `see`. The saves and reverts from the settings' tables. One description, three readers.
- **Every error under `/api/v1` says where to read** (item 3): `see` in the server's errors (404 the index, 405 the path's own GET, else the schema), the API's and the settings'.
- **The root points to the index** (item 1): `service-doc` and `service-desc` links in today's portal's and wizard's heads (the mock had them since gate 1).
- **Every route is in the index** (item 4): `GET /api/v1/debug/routes` lists the server's routes; `api_check` fails on one in neither the index, the schema, nor its named exceptions (the four pages, the debug routes): 82 routes, none unlisted.
- **Hello listed** (item 5): an action of the kind `agent` in the schema, a path in the index, AGENTS.md brought up to date. `api_check` calls it during its charge, refused 409 `busy`, so it never draws on a test's screen.
- **Flash:** 3.4 KB with the index's packing (the Pico W's image 97.98% of its slot).
- **Proven on the Pico W** (2.0.3-46): `api_check` with its new checks (33 actions' usage by their GET, a 404 and a 405 with their `see`, the routes), `web_robust` and `settings_roundtrip` `VERDICT PASS`. The stand-in recorded again.
- **Item 6, the unit's report of what it carries:** built the same day, below. Its first build option is the servo gate alone, not the LEDs as the plan said: the LED module also lights the 12864's backlight (a NeoPixel), so a build without it darkens the screen of every board that has one. The servo gate can be left out (the bench's unit has none fitted).

### Step 5, item 6: what a build carries, reported (2026-11-19)

- **The option:** `FEATURE_SERVO_GATE` in CMake, on by default on both boards, printed as it configures; off, `servo_gate.c` is out of the build. Its callers (the charge, the purge, the menu, the API) reached into the module's struct in 25 places; they now ask it by four functions (`servo_gate_enabled`, `_state`, `_ratio`, `_set_ratio`), which a build without it answers from the header: no gate. The gate's route, its schema action and state, its index entry and its menu page are built only with it; its settings were the module's own and go with it. The purge's screen keeps its Gate row (Off, and a move says "no servo gate in this build"): the screen's words are gate 3's.
- **The report:** the index's `features`, `[{"name": "servo_gate", "present": ..., "about": ...}]`.
- **Not the LEDs** (the plan named them): their module lights the 12864's backlight, a NeoPixel, so a build without it darkens every screen.
- **Found on the way, a fault:** the build without the gate hard-faulted at the menu's Settings. Its rows were counted by hand (`.row_count = 8`): with the gate's row compiled out, seven, and the menu read past the list. The class mended, not the one page: every page's count is now taken from its list (`ROW_COUNT`), fifteen of them; none is written by hand anywhere else in `src/`.
- **Flash:** the build without the gate is 6.5 KB smaller; with it, the image is 98.04% of the Pico W's slot.
- **Proven on the Pico W:** without the gate (`build_w_nogate`): `features` says absent, `GET /api/v1/gate` and `/api/v1/settings/gate` 404, Settings lists no Servo gate, `api_check` `VERDICT PASS` (76 routes, the schema without the gate, checked by its new rule for a part absent), `charge_cycle` `VERDICT PASS`. With it again (2.0.3-47): the stored settings the same as the copy before step 1 (the gate's own, untouched by the build that did not know them), `api_check`, `charge_cycle` and `menu_tour --compare` `VERDICT PASS`. The stand-in recorded again, with the report.

### Step 6, the update's life cycle made whole: built and proven (2026-11-19)

The owner asked the agent to own the Pico W's tests overnight (2026-11-19: "take the responsibility of Pico W test ... implement as much as you could, then leave the decision to tomorrow").

- **Gap 1, a trial writes into the EEPROM before it is kept:** on trial, `save_config` writes nothing: each record is held in RAM, the latest of each address, and written when the image confirms itself (`config_hold_writes`, `config_write_held`, `common.c`). That covers more than the plan's "defaults kept in RAM": the defaults of a layout that changed, the motors' one-time conversion, and any save during the trial. The invariant: a trial changes nothing the image it may go back to relies on. A trial whose Wi-Fi record did not read never confirms (its own hotspot is not its network).
- **Gaps 2 to 4, a power cut not seen, the end forgotten, a back without why:** the trial's note in an EEPROM slot of its own (12K, `EEPROM_UPDATE_NOTE_BASE_ADDR`; no struct of today's changes): the slot tried, its version, how far it got (asked, started, joined, no network), written as it goes and never held; and how the last update ended (`last_result`, `last_reason`, `last_reset`), kept until the next. A note there with the scratch note gone is a trial the power ended. `last` gains `reason` (did_not_start, no_network, not_confirmed, power_cut) and `reset` (the settings reset, by name); the `back` and `confirmed` events carry them; the screen's line says why (not "did not start" when only the router was away).
- **Gap 5:** the trial waits the Wi-Fi join's own time and 30 s, at least 60 s.
- **Smaller:** `checking` between `receiving` and `ready`; `back_within_s` while installing.
- **Test tools:** `RC_TEST_LAYOUT_CHANGE` (the LEDs' record a word longer: a reset there changes colours only; the update's source would have pointed the unit at GitHub, against the bench rules); `slot_trial --expect power-trial`, `--reason`, `--reset`, each judged after the end and again after a restart; the console's `update` gives `reason` and `reset`.
- **Flash:** 3.8 KB (the image 98.41% of the slot before gate 3's pages).
- **Proven on the Pico W** (the normal image 2.0.3-48 in A, test images in B at 512.900 to 512.904), each `VERDICT PASS`:
  - **back** (never confirms, the LEDs' layout changed): back 67.4 s after the trial was asked, `last` back `not_confirmed`, the same after a restart; every stored setting the same as the copy taken first, the LEDs' too: the trial's defaults were held, never written.
  - **power cut on trial** (`slot_trial --expect power-trial`): back at once, `last` back `power_cut`, the same after a restart.
  - **no network** (a network name that is nowhere stored for it, put back after): back 67.5 s after the trial was asked (the join's 30 s and 30 s), `last` back `no_network`, the same after a restart; the settings as before once the name was back.
  - **kept** (the LEDs' layout changed): confirmed 4.8 s after the trial was asked, `last` confirmed with `reset` the LEDs, the same after a restart; the LEDs' colours the only settings changed. The normal image put back, the LEDs' colours stored again from the copy: every setting the same as at the start.
  - A first try at "no network" with `RC_TEST_WIFI_FIRST_JOIN_FAILS` was not one: that build skips only its first join, joined later and confirmed. It is kept for what it is (a first join that fails).

### Gate 2's whole proof (2026-11-19, the step 6 image)

`flash`'s image 2.0.3-48 at 9ecbcff: `menu_tour --compare` (the screen's invert off for it, live, put back), `charge_cycle`, `api_check`, `settings_roundtrip`, `web_robust`, and `soak 600 --stream` (27 charges, 0 of 1,335 reads missed, each browser's stream all 162 events and some 2,170 states, `heap_free` 47,592 quiet before and after) each `VERDICT PASS`; the settings the same at the end as at the start. `stream_check` failed only its quiet stage, against a tool already changed for the next image (gate 3 found the stream's keep-alive comment invisible to a browser; the state again replaces it): run again on the next image. `heap_free` quiet is 2.7 KB below step 4's: the server's routes, moved to the FreeRTOS heap (112 of 24 bytes).

## Gate 3, the pages built

Started by the agent overnight on the owner's word (2026-11-19: "implement as much as you could, then leave the decision to tomorrow"), to its plan in the README ("Gate 3's plan", written the same night for the owner's review). Every step is on the ADR's one branch and can be taken back by its commits.

### Steps 1 to 5 (2026-11-19, overnight)

- **Step 1, the unit serves the new pages:** `scripts/web_build.py --header` makes the portal gzipped into `src/generated/web_portal.gz.h` in the firmware's build, whenever a file of `src/web` changes; the server sends a static reply gzipped (`Content-Encoding: gzip`, `Vary: Accept-Encoding`) with the build's `ETag`. The portal at `/`, `/mobile` and `/display_mirror` (the last opens the Unit page, where the mirror is); today's portal and mirror pages removed. The wizard stays at `/` on the unit's own hotspot and at `/wizard` until gate 4. The Pico W's image from 98.41% of its slot to 92.51% (962,472 bytes), the portal 34.1 KB gzipped. Read on the unit: 200 with its `ETag`, 304 and nothing with it; the portal page read in 0.44 s (median in the soak) against about 1 s for today's.
- **Step 2, the pages follow the unit:** `stream.js`, one event stream for the page: closed when the tab is hidden and opened when shown, opened again after 20 s heard nothing, tried again 5 s after a refusal; the Trickler drawn from its `state`, the job's strip from its `activity` (the update read only while one runs or a restart is followed), a settings part shown read again on its `setting` events unless a field of it is being typed in, and each page reading again what it shows when the stream starts afresh. The stand-in's charge moves at the old read's pace however often it is read.
  - **Found, and mended in the firmware:** the stream's keep-alive was a comment, and a browser's `EventSource` never shows its page one: a quiet unit and one gone looked the same. It is now the state again after 15 s quiet (the firmware, the stand-in, `stream_check`, api_v1.md and the README's table together).
  - **On the stand-in:** a charge started by another client followed to "Return the cup" with no read of the charge by the page; a setting written by another client shown "not stored" on the open form, and Undo shown; a restart: the stream opened again by the page 20 s after it last heard, and the pages read afresh.
  - **On the Pico W** (2.0.3-50): a simulated throw followed on the page from zeroing to the next zeroing, "Good: lift the cup" between, with no read of the charge. `menu_tour --compare`, `stream_check` (the state again at 15.0 s quiet), `charge_cycle`, `api_check`, `web_robust`, `settings_roundtrip` and `soak 600 --stream` (0 of 1,332 reads missed, each stream all 162 events, `heap_free` the same before and after) each `VERDICT PASS`; the settings unchanged.
- **Step 3, what the unit carries:** the Settings list reads the index's `features` once and leaves out a part whose feature is absent (the servo gate's). On the Pico W (`servo_gate` present) the gate's part is shown, its ratio read from the unit. `features_check.py`: 105 rows, 89 shown, 14 left out, one to come: row 4, the wizard's address, gate 4's.
- **Step 4, the screen's words:** Cleanup is Purge on the main menu, its cup's question (`ack/purge`) and its job (`job/purge`), and in its faults' messages. The Pico W's menu baseline saved again: against the old one only the main menu and the cup's page differed. `menu_tour --compare` on the new one, `reach`, `charge_cycle` and `api_check` `VERDICT PASS`. ADR-010's mock tools keep their words: they are that ADR's record.
- **Step 5, the rows `features.md` marks broken:** the new pages have them right, seen on the Pico W: Restart and Factory reset two actions of their own on the Unit page, each with its own confirmation, none chosen in advance (rows 70, 74); the gate's part reads the gate (row 37: "Off: turn on Fitted below to move it").
- **A detail on the unit:** its target lives in RAM, unset after a start; the target's box was blank where the screen says "not set": it says so now, as its placeholder.

**At the head** (2.0.3-53 at d4e775b, a clean build): `flash`, `menu_tour --compare` on the new baseline, `stream_check`, `api_check` and `charge_cycle` `VERDICT PASS`, the settings the same as before gate 2's whole proof. The Pico 2 W's image builds (50.74% of its slot).

### Left in gate 3

- **The pages at a tablet's and a computer's width on the unit**, beside the mock's (the phone's width seen).
- **The keep-awake video** where the Wake Lock is not given (plain HTTP): with the phones' checks at gate 4's start, as only a real phone shows whether it keeps the screen on and what it costs.
- **The owner's own phone** on the pages (carried from gate 1).
- **The Pico 2 W** at its next turn on the probe (it runs 2.0.3 at 192.168.2.166, read over the web only).


## Gate 4, commissioning built

Its plan approved by the owner 2026-11-19 ("Approve, start step 1"), with the iPhone moved to before step 4; the bench given with it.

### Step 1, the id and the hotspot's key from the chip: built and proven (2026-11-19)

- **What:** `unit_id.c`. The id is the first four bytes of SHA-256("opentrickler unit id", the chip's unique id), eight capital hex digits; the key eight characters of the 31 the screen draws apart (no i, l, o, 0 or 1), the first 64 bits of SHA-256("opentrickler hotspot key", the chip's id) written in base 31, some 40 bits. Two hashes in place of the README's "other bits of the id": the id, which mDNS announces, tells nothing of the key. `rnd()` is gone. The key is made once before the scheduler starts, beside the host name (`access_point_mode_init`), so no task reads it half made.
- **Stored: nothing; the metadata record is gone** (a stored layout changed, said here as AGENTS.md asks). First built with the id stored in the metadata's 8 characters and a unit's random id replaced by the chip's once. The owner asked what failure justified a new id (2026-11-19): two units on one network with the same four characters, so `opentrickler-XXXX.local` reaches the other one; not seen, but `rnd()`'s ids are weak (the Pico W's `9CE00000`, the Pico 2 W's `49933331`), and a factory reset drew a new one, renaming the unit. Asked whether a permanent id exists: each Pico has one from its maker (the Pico W's flash chip's 64-bit id, the RP2350's own from its OTP, both read by `pico_get_unique_board_id`); the controller board has none (the CAT24C256 has no serial number). The owner: "I don't need to record the controller board ID, so Pico ID, and don't store to the board is my preference." So the id is read from the Pico wherever it is wanted (`unit_id_get`), never stored, the same across a factory reset; it follows the Pico, not the board. The record at 0K held only the id and an old revision, and is no longer read or written (`eeprom_metadata_t`, `eeprom_get_board_id`, `eeprom_config_save` removed); its slot is free, its old bytes left there. `update.c` keeps its name "unit" so the other records keep their bits in a stored note. Every unit already out is renamed once by the update that brings this (the owner's choice, in `docs/releases/next.md`). The id shown stays a hash of the Pico's id, not the id itself: the key is made from the same id, and the Pico W's flash ids share their first digits across a batch.
- **Flash and RAM:** the Pico W's image 92.56% of its slot (963,008 bytes; 962,472 at gate 3's head), RAM 99.03% at link. The Pico 2 W's image builds (50.76%).
- **Proven on the Pico W** (2.0.3-56, its settings copied first, `logs/adr008/gate4/step1_before`):
  - the id `9CE00000` became `4A821948`, the host name `opentrickler-4A82`, reached at `opentrickler-4A82.local`; the same id after a restart; the stored settings the same as the copy, before and after `api_check`'s restart;
  - on the probe image (`build_w_approbe`, its own hotspot at the start): the Wi-Fi page shows Hotspot `opentrickler-4A82`, Key `wqtsa7ad`; this PC's Wi-Fi joined with that key (WPA2-Personal) and read `/api/v1/system` at 192.168.4.1; the old key `opentrickler` did not join; no reply of the 58 GET paths the index lists holds the key (the profile removed from this PC after);
  - the normal image back: `menu_tour --compare` (the screen's invert off for it, live, put back; the Wi-Fi and Firmware pages carry the name and the id, not judged), `charge_cycle` and `api_check` `VERDICT PASS`.
- **Again with the id never stored** (2.0.3-57): the same id, `4A821948`, by the console, the API and at `opentrickler-4A82.local`; the settings the same as the copy; `menu_tour --compare` (invert as before), `charge_cycle` and `api_check` `VERDICT PASS`. The Pico W's flash 92.54% of its slot, the Pico 2 W's 50.75%.
- **The id's life cycle, whole** (the owner, 2026-11-19: "complete the life cycle of the ID, where it was used and how they are impacted ... and how to handle the old ID from the EEPROM"):
  - **Made:** at every start, from the Pico's unique id (`unit_id_get`); nothing to create, store or lose. The host name and the hotspot's key are made from it once, before the scheduler starts.
  - **Its readers, and what each sees** (all read at run time, so each changed with the image, once):
    - the host name `opentrickler-XXXX`: the mDNS name on the station, the hotspot's name, and the server's own-name check (a request by it is not sent on to the unit's address);
    - the API: the index's `unit_id` and `hostname`, `/system`'s `unit_id`, `/network`'s `hostname`, the settings' export's `unit_id`: the same shape, the new value (api_v1.md says where it comes from);
    - the screen: the Firmware page's Unit row, the Wi-Fi page's names; the console's `state id=`;
    - the pages: the Unit page's Unit id, the settings' file name (`opentrickler_<id>_settings.json`), a powder's file's `unit_id`. Only written, never checked: a file exported under the old id still imports;
    - the tools: `api_check` reads `unit_id` and `hostname` as present, not as values; `settings --compare` judges the resources only, so copies taken under the old id still compare (proven above); `unit_api.py` prints them. The stand-in recorded again from the unit (`4A821948`), with gate 2's step 6 and gate 3's stream text it had missed;
    - the OWMC (read in its checkout): it finds units by `_opentrickler._tcp` each time and keeps no name (its FIXME: "load from NVS"); untouched. When it keeps one, `unit_id` is the one to keep: it no longer changes;
    - a person: a bookmark or a saved hotspot of the old name finds nothing, once (`docs/releases/next.md`). The router's address reservation is by the radio's MAC, untouched.
  - **The name the router is told** by DHCP was `PicoW` on every unit, not its name: the SDK's `CYW43_HOST_NAME`, as `wireless.c`'s own `#define` never reached the driver, compiled apart (heard on this PC's port 67 as the Pico W started: `hostname='PicoW'`). Older than this step; mended on the owner's word ("fix the issue as you see them"): the station's interface is given the unit's host name as it is brought up, before any address is asked, and the dead `#define` is gone. Heard after: `opentrickler-4A82` in the start's DISCOVER and REQUEST, and again after `wifi leave` and the rejoin. A router's list of clients now names each unit as mDNS does.
  - **The old record at 0K** (the revision, the id, a CRC, as every firmware before this wrote it): left as it is (the owner, 2026-11-19: "For old record I'm ok to leave as is ... Once we stop using it, the EEPROM space MAY be reused in the future"). Nothing reads it. A clearing once off trial was built and proven first (a trial left it and went back; the flash cleared it, "it held 4A821948"; a restart did nothing), then taken out by that word; so the Pico W's slot is blank and the Pico 2 W's will keep its record. The slot's note in `eeprom.h` says what a module given it must do: clear it first, as a record whose first two bytes are 0, as the old one's are, passes `load_config`'s old revision check.
  - **After:** a factory reset makes the whole EEPROM 0xFF, the slot with it, and leaves the id. An older image installed by USB (no going back, ADR-004, but a person can) reads its old record, or draws a new random id where there is none; its own name, until the next update. `update.c` keeps the slot's name ("unit") so the other records keep their bits in a stored note.
  - **The Pico 2 W** (`49933331`, `opentrickler-4993`) takes its new name at its next flash; its old record stays.
- **Not run on the unit:** a factory reset. It erases the Wi-Fi's key too, which the agent does not hold to put back; and the id no longer reads the EEPROM at all, so a reset cannot change it.
- **Users told:** a unit's name changes once, and the shared key goes, written into `docs/releases/next.md`, the list a release's notes now carry over (`docs/releases/README.md`, step 1). The manual's password (`manuals/connect_to_wireless.md`) says where the unit shows its key.
- **Found, for step 3:** the screen's mirror (`/api/v1/display/frame`) shows the Wi-Fi page as it is. Today the key is on it only while the unit is its own hotspot, when a reader of the mirror is on the hotspot and so has the key. Once step 3 keeps the hotspot up while the station joins, the home network can read the mirror while the key is shown: step 3 looks at it.
- **A person who does not use the screen** (asked of the owner 2026-11-19): the screen is standard, and the set-up asks the person to look for `opentrickler-XXXX` among the networks (the owner); the key is read from the screen beside the name, once.

### Step 2, the code on the screen: built and proven (2026-11-19)

- **What:** the seventh kind of screen, the code (the skill's rule 5): `setup`, a page of the kind `PAGE_CODE` (`menu_nav.c`), drawn as the mock drew it (`mockups/setup_12864.png`): the code at the left in a square of 64 x 64, its modules 2 x 2 with a quiet zone of 3 px, no title band, "Scan this / with your / phone's / camera." beside it on baselines 12 to 45, Back its one softkey at the right. Its text is the phones' Wi-Fi format, `WIFI:T:WPA;S:<hotspot>;P:<key>;;`.
  - **A phone joined:** the hotspot's DHCP server counts the addresses it gives (`dhcp_server_t.given`, never reset); one given while the code is shown turns it into a dialog, "A phone joined. / Go on with it there, / or open 192.168.4.1", Back focused at the left, "Show the code" at the right, which shows the code again. A dialog of three lines now (26, 37, 48; one or two as before on 29 and 40). The hotspot's address is written once (`ACCESS_POINT_MODE_ADDRESS`).
  - **Where it opens:** the main menu's first row "Set up this unit", and the Wi-Fi page's button "Join by camera" in place of the switch, both while the unit is its own hotspot with no network named. Step 4 asks the stored `set_up` instead. A named network keeps the switch, as gate 1 settled.
  - **The console:** `screen` gives kind `code` and an `@code "<text>"` line before its lines.
- **Drawn:** LVGL draws the square, the view paints the modules into the frame at the flush, as it paints the chart and the bubble (`menu_view_paint`); the modules are LVGL's copy of Nayuki's encoder (`qrcodegen`), level L, no boost, version 3 at most (2 px a module fills the square).
- **Found on the unit, and mended:** the Pico W's panel is shown inverted (its stored `invert_display`), and the code came out inverted with it: lit modules on a dark ground as a person sees it, which readers take unevenly. The invariant: a person sees dark modules on a light ground, however the panel is shown. The menu tells the view when the panel inverts (`menu_view_set_inverted`, the setting and the hello's flash alike), and the view draws the square dark and the modules lit on an inverted panel.
- **The code checked from the unit's own pixels**, with no reader downloaded: a reader written from the standard (the function patterns, the mask, the codewords in their zigzag, Reed-Solomon over GF(256) by 0x11D) read `snap`'s picture as a person sees it. With the panel inverted and not: version 3-L, mask 2, byte mode, 43 bytes, `WIFI:T:WPA;S:opentrickler-4A82;P:wqtsa7ad;;`, its 15 error-correction codewords right; the wrong polarity does not read. `segno`, made with the same text, differs only in its padding (a zero byte before the pad codewords), both valid. A phone's camera at the unit is still the last check (the plan's iPhone, before step 4).
- **The flash:** LVGL builds its encoder only with the QR widget, the widget only with the canvas, the canvas only with the image (`lv_conf_internal.h` refuses otherwise): the widget and the canvas are left out at the link, the image's support stays. The image 969,960 bytes (93.23% of the Pico W's slot), 7.2 KB more: the encoder 2.4 KB, the view's code 0.8 KB, the image's support and the rest about 4 KB. RAM at link 99.20% (452 bytes more). The Pico 2 W's image 51.13%.
- **LVGL's heap** (`screen`): the most used 12,532 of 14,336 bytes, the least free in one piece 2,288 (11,720 and 3,228 at step 1): four labels and the square more, made once at the start. A glyph takes at most 384 bytes from it.
- **Proven on the Pico W:**
  - normal image: `screen setup` shows the code, Back focused; `menu_tour --compare` `VERDICT PASS` (the screen's invert off for it, live, put back; the code's page kept out of the baseline, `rc_run.py`'s `MENU_TOUR_SECRET_PAGES`: its picture is the bench unit's hotspot key, and the baselines are in a public repository); `reach` `VERDICT PASS`, the code's Back reached; `charge_cycle`, `api_check` and the settings against the copy `VERDICT PASS`;
  - probe image (its own hotspot): this PC joined the hotspot with the code's key while the code was shown; the screen became the dialog within the second (`DHCPS: client connected`), a turn focused "Show the code", a click showed the code again, a click on Back went to the main menu.
- **Not seen on the unit:** the main menu's row and the Wi-Fi page's "Join by camera" with no network named. The settings refuse an empty network name (at least one character), and a factory reset, which leaves it empty, erases the owner's Wi-Fi key too. Seen with a named network: neither shown, the switch kept. Seen at step 4's reset.
- **For step 4's proof:** a set-up joins a real network with its key. The agent does not type the owner's Wi-Fi key: either the owner types it on the phone, or the set-up is proven on a test network the agent may join (a phone's own hotspot with a test key).

### The iPhone at the unit, on the probe image (2026-11-19)

The plan's iPhone check before step 4 ("To find out first"), with the code of step 2. The Pico W on the probe image (its own hotspot at the start), the code on its screen, its panel inverted as stored. The owner: "Scanned it, camera read the code and joined. The page automatically populated and I entered the password."
- **The code:** the iPhone's camera read it from the 12864 (version 3-L, 2 px a module, a quiet zone of 3 px, dark on light as seen) and joined the hotspot with no key typed. That was on the panel inverted (as stored); with it not inverted (live, put back after), the camera read it too and offered to join (the owner: "Yes, it offers").
- **The sign-in sheet opened by itself**, with today's wizard in it (the 302 to the unit, gate 1's probe), and the set-up worked inside it: its form went through, the unit stored the Wi-Fi and restarted.
- **The old wizard's flaw, seen:** the key typed for the home network was wrong, and that was found only after the restart: the normal image put back said "Not joined: Wrong key" and stood as its hotspot again. Steps 3 and 4 are built against exactly this: the join tried with the hotspot up, its end said on the page, the key typed again with no restart.
- **Mended on the way:** the new set-up's network name and key, and every text setting on the settings pages, tell a phone not to capitalise, correct or spell-check them (`autocapitalize`, `autocorrect`, `spellcheck`): "Show the key" makes the key a text field, which an iPhone would otherwise change as it is typed.
- **Not seen:** whether the screen showed "A phone joined" (the wizard's restart came first), whether the sheet closed or the phone left the hotspot at the restart, and whether the last step's link opens in the phone's browser: step 4's pages, on the unit.

### Step 3, the networks and the join with the hotspot kept up: built and proven (2026-11-19)

- **The station's state, one for every join** (`wireless.c`): the start's, the minute's retry and the set-up's each set it: idle, joining, joined, or failed with why (`wrong_key` from the radio's bad authentication, `timeout`, `failed`). The radio cannot tell a network not found from one slow to give an address, so `not_found`, which the mock proposed, is not claimed. `GET /api/v1/network` gives it as `station`, with the station's address on that network.
- **The scan:** `POST /api/v1/wifi/scan`, two scans merged by name, its strongest signal and that one's security, sorted strongest first once they end, hidden networks and WEP alone left out; `GET /api/v1/wifi/networks`. The table, 24 networks, from the FreeRTOS heap at the first scan; a reader takes one network at a time, so no stack holds the list. Security as the settings name it: the radio tells WPA2 from WPA, not AES from TKIP (WPA2 alone is `wpa2_aes`, both `wpa2_mixed`).
- **The join:** `POST /api/v1/wifi/join` (`ssid`, `key`, `auth`), 202 with the network, its station joining, in the Wi-Fi task. Only while the unit is its own hotspot: on its network a join would leave it, so it is refused 409 `not_on_hotspot` (a new code, in the schema). A unit that started with Wi-Fi off never brought its station up: the join brings it up beside the hotspot, with the unit's DHCP name. Joined: the network stored by whom it was asked (`setting changed` and `stored` events), and in effect at once (`settings_in_effect`: no restart pending, and the minute's retry takes it as the network the unit started with); announced by mDNS there; the hotspot kept up. Refused: the station leaves the network, so it stops trying, and nothing is stored; the key is wiped either way. A request by the station's address is the unit's own too while both are up.
- **The console:** `wifi scan` and `wifi join` in every build, the probe's own taken out. `wifi join` joins the stored network with its stored key through the same join: how the success is proven on the bench without the owner's key passing through an agent.
- **Found and mended on the way:**
  - **A stack overflow:** the console's join stored the network from the Wi-Fi task, and it overflowed its 2 KB (the assert, the unit halted, nothing stored). The cause was mine: the write's fields, an array for 64 on the stack for 4 (1.3 KB). Now 8 at most, and the task given room for a settings write, which keeps two copies of the struct: its deepest use measured 1,640 bytes, so 768 words (3 KB), 1,980 bytes never touched at rest.
  - **The factory reset's line in api_v1.md** still said a reset brings a new id and host name, false since step 1: mended.
  - **The stand-in's recording** would have kept the station's network name (the owner's) and the networks heard (the neighbours'): the recorder hides the first as it hides the network's, and records no networks; the stand-in plays its own. Its guard now refuses only `set_up`, step 4's.
- **The screen's mirror and the key** (found in step 1): with the hotspot up and the station joined, the home network can read the mirror, and on it the Wi-Fi page's key or the code. Left as it is: the home network already reaches the whole API (no password for now, the owner), the motors included, which is more than the hotspot's key gives; and the hotspot is up only until the set-up ends (step 4) or the next start.
- **Flash and RAM:** the Pico W's image 978,888 bytes (94.09% of its slot), 8.9 KB more, most of it the schema's and the index's words; RAM at link 99.26%. The Wi-Fi task's stack 1 KB more, from the FreeRTOS heap. The Pico 2 W's image 51.61%.
- **Proven on the Pico W:**
  - on its network: `wifi scan` heard 4 networks by name in 1.35 s; `api_check` with its new calls (a scan waited out, a join's body refused 400 `out_of_range` and `not_a_choice`, a join refused 409 `not_on_hotspot`), `charge_cycle`, `menu_tour --compare` and the settings against the copy `VERDICT PASS`; the stand-in recorded again;
  - as a reset unit is: Wi-Fi turned off and stored, the unit restarted as its hotspot with its station never brought up, this PC on the hotspot pinging it every 0.3 s. Over the hotspot: a scan, 5 networks in 1.6 s; the stored network with a wrong key, `failed` `wrong_key` 6.2 s after it was asked, the hotspot up throughout, nothing stored (Wi-Fi still off, `saved`), no restart pending. Then `wifi join`: joined in 3.2 s; Wi-Fi on and stored (`changed` and `stored`, by console), no restart pending; the unit answered on the hotspot (192.168.4.1) and on the home network by its address and by `opentrickler-4A82.local`; this PC missed one ping, some 0.9 s, as the hotspot moved from channel 3 to the router's 11, and followed it (gate 1 saw the same);
  - restarted, the unit on its network as before, the settings the same as the copy taken before step 1.
  - The set-up's page on the stand-in, as a unit never set up: the scan's list, a wrong key told ("Not joined: the key is wrong. Try again."), the right one joined, its address there.
- **Not seen:** a join to a network with a key the agent typed that is not the stored one's: the owner's key is not the agent's to type. Step 4's proof joins with the owner at the phone, or on a test network.

### Step 4 built; its proof at the unit failed: the set-up designed again (2026-11-19)

- **Built** (ddf4c60): `set_up` (a flag in the Wi-Fi record's filler, the size kept; or a network named), `POST /api/v1/setup/done`, the portal at `/` on the hotspot opening the set-up, `/wizard` a redirect to it, the wizard and `html2header.py` gone (6 KB), DHCP's option 114, the screen's offer asking `set_up`, `rc_run.py settings --restore`. On the Pico W on its network: `api_check`, `charge_cycle`, `menu_tour`, the settings, `features_check --final` (none to come) `VERDICT PASS`; the set-up end to end on the stand-in.
- **At the unit, with the owner's iPhone** (the Pico W factory-reset: never set up, its hotspot the same name and key, so the id survived the reset as step 1 meant): the code read, the hotspot joined, the sign-in sheet opened on the set-up by itself. Then: "Key is confusing ... Selecting SSID and password on the same screen doesn't make sense ... The wrong password warning doesn't disappear until it is touched ... I stuck at Joining <SSID> view", and "the mini display doesn't change neither". The unit's log: the scan, then the join of the owner's network, joined in 3.3 s and stored. The hotspot moved from channel 3 to the router's 1 with the join (read from this PC after); the iPhone did not follow; the page took its failed reads for "still joining"; the 12864 stayed on "A phone joined". The warning was the page's refusal before any join (none was tried before the one that worked), in the page-wide bar that waits for a tap.
- **Owned:** "To find out first" named the channel move before gate 1; only a Windows client was seen to follow it, and no phone was on the hotspot while the unit joined. The set-up was drawn for one screen, though two show it.
- **Designed again** (the owner: "Yes, design it"), README "The set-up, designed again": two screens, one process (the unit keeps the set-up's step, `GET`/`PATCH /api/v1/setup`, and both draw it); every stage on both screens, its end and the way back, failure modes included; the Wi-Fi as views (the list, a network's password, joining with its seconds counting, joined, lost); "password" everywhere, the API's `key` renamed before the first user release; a message under what it is about; the hotspot started on the strongest network's channel and a join taking its access point on that channel (the driver's `cyw43_wifi_ap_set_channel` and `cyw43_arch_wifi_connect_bssid_timeout_ms`, read; to be seen on the unit first).
- **Mocked:** the 12864's stages, `mockups/setup_12864.png` (16 screens, each line measured; a long network name; "Key" now "Password"); the phone's on the stand-in (`tests/web_test.py --hotspot`), which plays the step and three test passwords: `wrongkey` (a wrong password), `noanswer1` (no answer), `losephone` (the phone off the hotspot for 12 s while the join succeeds). Walked at a phone's width: the list; a password view; a short password said under the field and gone as typed; a wrong password and no answer said there; joining, its seconds counting; the lost view, the only message (the page-wide bar holds back through the set-up), naming the unit's hotspot; heard again, the page going on to "Joined" by itself; Ready saying what comes before the hotspot stops.
- **The page and the built firmware now differ** until the build gate: the page sends `password` and reads `/api/v1/setup`, which the unit does not have yet. Nothing is flashed from it.

### The set-up as designed again: built, proven from this PC (2026-11-19)

The owner settled the design: "Settled, build it".
- **"Password" everywhere:** the join's `password`, `settings/wifi`'s `password` (its stored field unchanged), the reason `wrong_password`, the screen's Wi-Fi page.
- **The radio:** a hotspot's start scans first (some 1.5 s) and takes the strongest access point's channel (`cyw43_wifi_ap_set_channel`); the scan keeps every access point by BSSID beside the networks by name (32 and 24, from the heap), and a join takes its network's access point on the hotspot's channel when one was heard (`cyw43_arch_wifi_connect_bssid_timeout_ms`). On the Pico W: the hotspot on channel 11, the strongest heard; this PC on it pinging every 0.2 s through the stored network's join, which took the mesh's node on 11: 116 of 116 answered (60 to 160 ms for 3 s), the hotspot never moved. Before, a join moved it from 3 to 1 or 11, and this PC missed some 0.9 s; the iPhone lost it.
- **The set-up's stage on the unit** (`setup.c`): its step, kept in RAM; `GET`/`PATCH /api/v1/setup` (`set_up`, `step`, `network`, `phone`, `hotspot`); events `setup` `step`, and the set-up's join's `wifi` `joining`, `joined`, `not_joined` with its reason; `joining_ms` in the station, so both screens count the same seconds.
- **The 12864 follows it** (`menu_nav.c`, the `setup` page): its stage worked out from what the unit knows (the step, the join's state, a phone on the hotspot), so the screen is true whether or not the phone still reaches the unit: the code, a phone joined, the password typed on the phone, joining with its seconds, joined, a wrong password, no answer, the scale's weight, the motors, ready, done with the address. The screen goes to the set-up when its step changes, on the hotspot or at its end, unless a job holds it; Back leaves it until the next change; "Show the code" and back; OK at the end to the main menu.
- **The page** tells the unit its step and the network chosen; the joining view counts the unit's seconds.
- **Flash and RAM:** the Pico W's image 981,552 bytes (94.35%), 8.7 KB more than step 4's build; RAM at link 99.28%.
- **Proven on the Pico W from this PC** (Wi-Fi turned off and stored, so its hotspot started, never brought up as a station): this PC got an address and the screen went to "A phone joined. Choose your Wi-Fi on your phone." by itself; the page on the unit (http://192.168.4.1/#setup) listed the networks at once (the start's scan); the owner's network chosen, the screen "Type the password of <it> on your phone."; a wrong password typed on the page: both screens joining ("4 s of 30" on the unit), then "Wrong password" on both, the field focused on the page, nothing stored; the join with the stored password (`wifi join`): "Joined" on both; the scale (no scale sending on the bench: "Nothing from the scale" on the 12864, no weight on the page), the motors ("Coarse driver: found", "Fine driver: found"), Ready; Finish: the page said where to open the unit before it left, the hotspot stopped 2 s after, the screen "Set up: Done / Open on <it>: / opentrickler-4A82.local / or <address>" with the station's mark. `api_check` (the step refused and taken, done), `charge_cycle`, `menu_tour` and `reach` (the set-up page's softkey reached) `VERDICT PASS`.
- **Not yet:** the owner's iPhone end to end after a factory reset (the owner types the password); the Pico W's own settings put back after it (`settings --restore`), then `menu_tour --compare`.

### The set-up at the unit with the owner's iPhone, as designed again (2026-11-19)

The Pico W factory-reset over the API (never set up, its hotspot `opentrickler-4A82`, "Set up this unit" leading the main menu); the owner set it up from the iPhone at the unit and typed the password: "Done. One observation: It failed twice in the row that first join attempt failed and second time it works."
- **The unit's record** (its event log and console): the page opened (step wifi, 115.2 s after the start), the owner's network chosen (122.5 s), one join, on channel 1, the hotspot's own (130.2 s), joined and stored 3.1 s later, then scale, motors, ready, done (150.4 s), the hotspot stopped 2 s after. The phone stayed on the hotspot throughout: one address given, no second.
- **The first try never reached the unit's radio:** no `wifi joining` event before the one that joined. So it was refused before the join: the page's own check, the unit's refusal of the request (which leaves no event), or a request that got no answer. Which one is the owner's to say (the words the phone showed), and decides the fix.
- **After:** the owner's settings written back from the copy taken before (`settings --restore`, then the unit restarted): the settings the same as the copy, `menu_tour --compare` `VERDICT PASS`.

### The first try's refusal chased from this PC (2026-11-19)

The owner: it happened in both iPhone runs, the first join refused and the second taken. In the first run's page the length check did not exist, so what the two share is the first `POST /api/v1/wifi/join` failing at the unit or on the way to it.
- **A refusal now leaves a line on the console** (`http_server.c`): every reply of 400 or more with its method, path and the unit's own words (never a body sent to it), a request dropped because it came while the last was still answered, a request or a reply reset as stalled, and the server's slots all held (a new connection waiting in the backlog) with what holds each slot and for how long.
- **On the Pico W** (Wi-Fi off and stored, so its hotspot started, on channel 11, the strongest network's; this PC on the hotspot; the set-up page in the browser pane at a phone's width): four tries of the first join.
  - The first: the password field empty at Join, the page's own check said "A Wi-Fi password has 8 to 63 characters". Not caught by the page's hooks (put in after it), and the pane's viewport changed size between its clicks, so it is not taken as a reproduction.
  - The next three, with hooks on every redraw, every fetch and every keystroke: the password stayed, the join went, refused as a wrong password, said under the field. No redraw of the set-up but the person's own.
  - **The unit refused nothing** through the run but `/favicon.ico` (404), no slot held, nothing stalled.
- **The link between this PC and the hotspot dropped packets for stretches:** a `GET /api/v1/setup` on a kept-alive connection answered 10.4 s after it was sent, a new connection 7 s to open (Windows' SYN tries at 1, 2 and 4 s), a reload's first read 4.4 s; pings every 0.5 s lost for 8 s, then for most of 29 s, then none in the next 35 s, and none of 240 over the next 2 minutes. Neither side logged a thing while it lasted (Windows' Wi-Fi log, the unit's console).
- **Not settled:** which of the two ended the iPhone's first tries. A post sent into such a stretch fails or hangs, and the page says "The unit did not take it."; the words the phone showed would tell. Where the drops come from (this PC's radio, the unit's on a channel shared with the mesh's strongest node, or the air) is not known.
- After: the unit's Wi-Fi on and stored again, restarted, on its network; `api_check`, `menu_tour --compare` (the display's inversion set off live, then reverted), `charge_cycle` `VERDICT PASS`; the settings the same as `logs/adr008/gate4/step4_before`.

### The iPhone's link read from the unit (2026-11-19)

- **`net log on|off`** (never stored): each connection taken and each request on the console, with the client's address and the unit's time. The set-up page asks `GET /api/v1/setup` every 2 s, so a phone's own reads show its link's rhythm from the unit's side.
- **The run:** the settings copied (`logs/adr008/gate4/iphone2_before`), the Pico W factory-reset over the API, the log on, this PC off the hotspot; the owner set the unit up from the iPhone (the hotspot on channel 1).
  - The sign-in sheet: `/hotspot-detect.html` and `/` asked twice (iOS's check, then the sheet), the set-up's reads at once after.
  - **The first join taken:** one `POST /api/v1/wifi/join` 12 s after the list was drawn, joined and stored 3.2 s later; scale, motors, done; the hotspot stopped 2 s after.
  - **The link clean:** 17 of the set-up's reads from the phone, 1.5 to 2.2 s apart, no gap; nothing refused, stalled or dropped. The slots all held once, for 90 ms, at the motors' step (the phone's parallel reads), the new connection then taken.
- **Verdict:** the first try's refusal did not come back, from the iPhone or from this PC with hooks on the page; the stretches of lost packets were seen only from this PC. Not chased further: the unit now says a refusal, a drop, a stall and a wait for a slot on its console, and `net log` gives a client's rhythm, so a next one names itself. The words the phone showed in the two earlier runs are not known.
- **After:** the settings written back from the copy, the unit restarted: the settings the same as the copy (its update source this PC's server again, which the reset had made GitHub's), `menu_tour --compare` (the inversion off live, then reverted) and `charge_cycle` `VERDICT PASS`.

### Gate 4's proof on the Pico W (2026-11-19, 2.0.3-69 and -70)

- **On its network:** `api_check`, `settings_roundtrip`, `web_robust` `VERDICT PASS`.
- **The soak with a client on the hotspot:** the unit its own hotspot (Wi-Fi off and stored; channel 11), this PC its client, `soak 600 --stream --host 192.168.4.1`: 27 charges in 627 s, 0 of 1,336 web reads missed (the mirrors' longest 1.08 s, the portal's 0.59 s), each stream and each held read all 162 events, no late feed of the step pulses, no lwIP allocation failed, `heap_free` 45,032 before and after. Pings every 0.5 s alongside: 2 of 1,278 lost, so the stretches seen earlier from this PC did not come back. `VERDICT PASS`.
- **The update end to end from the page** (what PR #111 waited for, with the screen's half from ADR-010): a test release, v9.1.0, of this commit (a local tag, deleted after; `build_release.py`, kept in `logs/adr008/gate4/releases/`, out of the served folder), served by `tools/ota_server.py` as the bench host. `update check`: the bench host asked, 9.1.0 found. In the browser at a phone's width, the Unit page: "Version 9.1.0 is available / From the bench host 192.168.2.13", Install, its confirmation ("Keep the power on until it is back ..."), Install; "Downloading 9.1.0" with its share; the page then by itself on "Firmware 9.1.0 / Last update: 9.1.0 installed. / No newer firmware found.", the offer's mark gone from the Unit tab. The unit: 983,308 bytes in 20.6 s (two hops, heap low mark 32,992, the job's stack 1,144 left), installed, slot B on trial, confirmed on its network (`last` `confirmed`). The restart's own view was not caught in a screenshot: the page was read 6 s and 16 s after the download's 98%.
- **After:** the server stopped and v9.1.0 moved out of its folder; this commit's build (2.0.3-70) flashed by the probe into slot A; the settings the same as `logs/adr008/gate4/iphone2_before`; `menu_tour --compare` (the inversion off live, then reverted) and `charge_cycle` `VERDICT PASS`.
- **Left in gate 4:** nothing on the Pico W. The Pico 2 W at its next turn on the probe, for the ADR's proof on both boards.

### The pages at a tablet's and a computer's width; two faults of the update found and mended (2026-11-19)

The owner, on #111: "I want ADR-008 to be merged in one PR" (#111 is closed with its note when #134 is merged; README).
- **The widths, on the Pico W:** at 768 x 1024 every page with the side rail, one column; at 1280 x 800 the Powders and Settings lists beside their part. Nothing wider than the window on any page (read in the page: `scrollWidth` at most the window's, no element past its right edge).
- **"Last update: 9.1.0 installed" under "Firmware 2.0.3-70":** the last update's note outlived the probe's full install of another image (as it would a USB one). The note now keeps the version it was written under (`on_version`); at a start under another image, with no trial running, it is let go. **The stored layout changed:** the note (`update_note_t`, its own EEPROM slot at 12K) from 16 to 20 bytes, so every unit's note reads as none once, after the update that brings this: one past result forgotten, no setting. Proven: v9.1.1 installed over the air, then this commit flashed by the probe: "the last update's note was written under another image; let go", `last` null.
- **The update's page froze on "Downloading 9.1.1, 95%" for some 20 s,** until the new firmware's page loaded by itself; "Installing" and "Restarting" were never shown. Read with a hook on the page's reads and the stream: during the download each read took 1 to 3 s, and the page asked three every second (plus the mirror's frame), so they piled up; the stream's `received` and `installing` never reached the page; and the unit restarted with its connections open, so the reads hung (dozens pending, none failed). Mended at each:
  - **A restart the unit means closes its connections first** (`http_server_close_all`: an update's trial, the API's restart and factory reset, the screen's restart): the server sends what it holds, a stream with an event waiting included, then closes each connection and its listener; if no restart comes within 5 s it listens again. `stream_check`'s restart: the old stream "closed after 0.5 s", where it was left silent before (README's table and AGENTS.md say so now).
  - **The page's job reads never overlap,** and are given up after 5 s (a unit gone without a word).
  - **The `received` and `installing` events move the job's view on** at once, and a late read never takes it back.
  - Proven with v9.1.2 from the page, the hook on: the download's share every 2 s (1 to 95 %), `received` and `installing` at 21.3 s and "Installing" 0.1 s later, the next read given up at 5 s ("Restarting on 9.1.2"), the stream afresh, "Last update: 9.1.2 installed." at 28.6 s.
- **After (this part):** v9.1.1 and v9.1.2 kept in `logs/adr008/gate4/releases/`, the server stopped; 2.0.3-73 flashed by the probe; `api_check`, `stream_check` (with no browser on the unit: one open in the pane holds a stream, and its cap's stage then fails), `web_robust`, `menu_tour --compare`, `charge_cycle` `VERDICT PASS`; the settings the same as the copy. No soak: the server's change acts only at a restart, and its new console lines only on a refusal.

### The owner's own phone on the pages (2026-11-19, gate 3's last checks)

The owner's iPhone, Chrome, on the home network, at `opentrickler-4a82.local`; the Pico W with the simulated scale on (not stored), its console logged. What the owner found, and what was done:
- **The readout squeezed to nothing under Start charge** (a screenshot: the figure's unit and the bar drawn over the key). The Trickler took its height from `100svh`, which Chrome on the iPhone gave some 170 px short of the height it showed; the readout, the only row that gives, got none. Now `100dvh` (the height shown; `svh` kept as the line before it), with a floor of 500 px below which the page scrolls and never overlaps, the margin under it keeping the key clear of the navigation. Read on the stand-in at 375 wide: 512 tall, the page scrolls 52 px, the readout 76 px, the key above the navigation; 600 and 684 (the phone's height shown), no scroll, the readout 112 and 168 px.
- **A key tapped twice zoomed the page** (the target's - and +; the owner: as on the old pages). `touch-action: manipulation` on the root: no zoom on a double tap, pinching still zooms.
- **"Zeroed." in red**, a refusal's colour: it is a thing done, now green. Every other message without a kind was read: each is a refusal.
- **"Tuning guide on GitHub: needs the internet"**: the owner, "The 'need the internet' part is not needed"; the line is "Tuning guide on GitHub" (features.md row 61 says so).
- **A servo gate tuning feature** (the owner: "perhaps offer that in initial commissioning as well as in the settings menu ... consider if you can opt in the ADR-008"): weighed, and the owner chose "A gate in ADR-009" (written there, on main).
- **Then, on the build with the fixes:** "All fine": two charges started and followed from the phone to their end (the console: 18:56 and 18:57), the screen kept awake through them, the readout above the key, no zoom. The console said nothing refused but one "Check now" while a poll ran (409 busy, as the API says), and the phone's six connections at once held every slot for some 100 ms at a time, the next taken then.
- The simulated scale off after. **Left in gate 3:** nothing. **Left in the ADR:** the Pico 2 W at its turn on the probe.
