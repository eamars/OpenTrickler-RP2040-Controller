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
4. Step 3, the event stream: under way.

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

