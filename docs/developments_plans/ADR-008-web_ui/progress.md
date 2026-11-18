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

### Next in gate 1

1. The iPhone on the probe image, joining the access point, then the station joined with the phone on it: deferred by the owner (2026-11-18) to when the owner is at the unit, at the start of gate 4 at the latest. No Android phone: taken to work the same, not tested (the owner).
2. The owner reads `features.md` and settles what it proposes to leave out, and the cleanup's place.
3. The styles' proposal, with its tools in the build.
4. The mock.
