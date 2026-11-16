# ADR-015: commissioning a unit

Status: **Draft** 2026-11-17. Not approved. It holds the owner's direction as it was given and what the code does today. Nothing here is designed yet.

## What we want

A person takes a new unit, or one just reset, from its first start to its first charge by one designed process, not by steps found here and there.

## Owner's direction (2026-11-17)

- The QR code proposed in ADR-010's menu design, to open the web page or to join the unit's access point: "I'm not sure about the QR code. Perhaps that's a feature for future ADR. I'd like to design the commissioning process properly, not ad-hoc."

## Why here

Proposed: after ADR-010 and before ADR-008. Commissioning has a screen half, which is ADR-010's new Wi-Fi page and menu, and a web half, which is ADR-008's set-up wizard. Designing it before the wizard page is built means the wizard is built once. The owner may place it elsewhere.

## What we know (read in the code 2026-11-17)

- **The first start.** With no Wi-Fi network set, or none joined within its join timeout, the unit becomes its own access point (`wireless.c`, `access_point_mode.c`):
  - its name is `opentrickler-` and four characters of the unit's id;
  - its key is `opentrickler`, written in the code and the same on every unit;
  - its address is 192.168.4.1, with the unit's own DHCP and DNS servers.
- **The wizard.** On the access point, `/` serves `wizard.html`, which does one thing: it sets the Wi-Fi network (`/api/v1/settings/wifi`, then its save), then restarts the unit. Nothing else is set up by it.
- **Defaults.** Every other setting starts at its default: the scale is the A&D (`scale.c`), and the motors, the charge and the display are at their defaults. A person finds them on the web portal or on the screen.
- **A network that is gone.** At each start the unit tries its network for the join timeout, and then becomes its own access point. A station that is set up but not joined tries again every 60 s (ADR-004).
- **What the screen shows.** The Wi-Fi information: the access point's name and key, or the network joined, and the address. Today it is the menu's form 41; in ADR-010 it becomes the Wi-Fi page.
- **Finding the unit on a network.** mDNS, as `opentrickler-<id>.local`, with the services `_http._tcp` and `_opentrickler._tcp` (`wireless.c`). The Open Weapon Mount Computer finds units by the second.
- **A factory reset** (`POST /api/v1/system/erase`, or the menu's Erase) erases the stored settings. The unit then comes back as its own access point, with a new id and so a new host name.
- **The unit's id.** It comes from `rnd()` (`eeprom.c`), which gives ids with few random bits: the bench's is `9CE00000`, and the host name shows four characters of it. Two units can end up with the same name (found in ADR-003, HANDOVER).

## Ideas

- The QR code from ADR-010's design, for the page's address or for joining the access point.

## Not in this ADR

- A password or other authentication (none for now: ADR-008's hand-on).

## To settle before approval

- What commissioning covers: the Wi-Fi alone, or the scale, the hardware's settings and a first charge too.
- Where each step runs: the screen, the web, or both.
- The QR code.
- The unit's id and name, and the access point's key.
- Its place in the order.
