# ADR-014: a charge profile beside the powder profile

Status: **Draft** 2026-11-15. Not approved. It holds the owner's idea as it was given, and what the code says today. Nothing here is designed yet.

## What we want

What is thrown is kept apart from how a powder flows. Two calibres that use one powder share that powder's tuning and differ in their weight.

## Owner's direction (2026-11-15)

- "My original idea was introducing the 'charge profile' than a 'powder profile', for example, two caliber can share the same powder, but charge as different weight."
- It was not built before, for its complexity.
- PR #109, which keeps the last weight inside the powder profile, is not merged for this reason (ADR-002).

## Why here

After ADR-006, which changes how settings are stored: the stored profiles are then migrated once, to their new form, and not twice.

## What we know (read in the code 2026-11-15)

- **A powder profile** (`profile_t`) holds a name of 16 characters and, for each trickler, Kp, Ki, Kd and a minimum and a maximum speed. There are eight, stored as one block.
- **The target weight is stored nowhere.** It is five digits on the weight form, and they start at zero each time the unit is powered. So a user dials the weight in again at every start. That is the need behind PR #109.
- **What else a charge uses is global,** in charge mode's stored settings: the two stop thresholds, the gate's position at the coarse stop, the back-off, the precharge, the decimal places.
- **PR #109's way:** one `last_charge_weight` in each powder profile. A powder then has one weight, and a second calibre on it needs a second copy of the powder, whose tuning is no longer shared. It is closed by ADR-002; its author is NemesisXB.
- ADR-009 lists "the stop thresholds per profile" as an idea to try. Which profile they would belong to is settled here.

## Ideas

A first view, to be weighed when this ADR's turn comes:
- a charge profile is a name, a target weight, and the powder profile it uses;
- a charge starts by choosing a charge profile; its weight and its powder come with it, and the weight can still be changed at the unit;
- the weight last used is kept with the charge profile, so the need behind #109 is met.

## How the unit shows it works

- Two charge profiles on one powder throw two weights, and a change to the powder's tuning shows in both.
- The weight is there after a power cycle.
- Profiles stored by the version before are read back as they were.

## Not in this ADR

- Tuning a powder (ADR-009).
- Ladder loading and other new modes.

## To settle before approval

- What a charge profile holds beyond the name, the weight and the powder: the stop thresholds, the decimal places.
- How many there are, and whether a charge with no charge profile stays possible.
- How a unit's stored profiles become the new form.
- Its place in the order. Storage speaks for after ADR-006. Its screens speak for later: forms built in u8g2 are built again in LVGL (ADR-010), and its page belongs to the web UI (ADR-008).
