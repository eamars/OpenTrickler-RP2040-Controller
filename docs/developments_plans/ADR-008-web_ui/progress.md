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

### Next in gate 1

1. The iPhone on the probe image, joining the access point, then the station joined with the phone on it: deferred by the owner (2026-11-18) to when the owner is at the unit, at the start of gate 4 at the latest. No Android phone: taken to work the same, not tested (the owner).
2. The owner reads `features.md` and settles what it proposes to leave out, and the cleanup's place.
3. The styles' proposal, with its tools in the build.
4. The mock.
