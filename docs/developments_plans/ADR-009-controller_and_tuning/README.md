# ADR-009: calibration and automatic tuning

Status: **Draft** 2026-11-25. This draft holds the calibration; the automatic tuning is drafted next.

The earlier README and progress are phased out and moved to [`legacy/`](legacy/).

## What we want

A calibration the user runs for the unit and for each powder. It measures what a charge needs and writes today's charge settings from it, so nobody tunes by hand. An automatic tuner then keeps those settings right (to be drafted). ADR-009 is these two and nothing more.

## Owner's direction

- The calibration records the cup, the scale, and each powder on this unit: the list below.
- The fine threshold is the user's expectation, the tolerance of a throw. The calibration never writes it.
- The PID's D term is calibrated, not left at nothing.
- The cup comes first: how the user gives its size is settled before the rest.
- The servo gate's tuning is not part of ADR-009.

## What the calibration records

| # | What | Kept by | How it is found |
|---|---|---|---|
| 1 | The cup's capacity | the unit | Given by the user, for example by trickling with the knob until the cup holds as much as they want; the reading is the capacity. Every pour of the calibration stays within it |
| 2 | The empty cup's weight | the unit | The cup lifted off the zeroed scale: the reading falls by its weight. A charge then tells a lifted cup from a light reading |
| 3 | The scale's digit | the unit | The smallest step its readings show |
| 4 | The scale's response time | the unit | How long a small change takes to show and settle, by its size (one digit, a few digits) |
| 5 | The scale's lag | the unit | How far the reading runs behind a steady flow, by flow rate. It may be one curve with item 4: a smaller change takes longer to register |
| 6 | Each tube's grains a turn | the powder's profile | Steady runs of each tube, at several speeds |
| 7 | Each tube's least speed | the powder's profile | The slowest speed at which what lands after a stop is consistent |
| 8 | Each tube's most speed | the powder's profile | The fastest speed at which what lands after a stop is consistent |
| 9 | The fine stop point | the powder's profile | What still lands after the fine tube stops at its least speed: its average and its spread |
| 10 | The coarse threshold | the powder's profile | As close to the target as the coarse tube can stop without an overcharge: both tubes' powder still falling at the coarse stop, the coarse stop's spread, and the back-off's spill when the back-off is on |
| 11 | The PID of each tube | the powder's profile | Made from the lag (5) and the grains a turn (6), below |

The "powder" items belong to this powder on this unit's tubes and scale, not to the powder alone: they are not shared between units.

## What it sets in today's settings

| Setting | From | Rule | Shown |
|---|---|---|---|
| Fine Kp | lag at the finish, fine grains a turn | at most 0.37 / (lag × grains a turn) | On the bench, 2026-11-20: 0.37 / (0.55 s × 0.75) gave a bound of 0.9; 0.8, under it, is the value tuned by hand on the owner's Pico W. The stored 2 had thrown 1.2 to 1.5 gr over; 0.8 threw +0.02 to +0.40 gr |
| Fine Kd | Kp, lag | Kp × lag: the P term then acts on the forecast, the reading and the powder still falling. Today's loop takes the derivative per millisecond, so the lag in ms | Not yet: the simulation first, the rate taken over several readings |
| Ki, both tubes | | 0: the weight is the sum of the flow, and an I term only builds up while the coarse tube pours | The shipped profiles have 0 |
| Coarse Kp, Kd | the coarse tube's lag and grains a turn | the same rules | Coarse Kp made from a calibration was thrown on the bench on 2026-11-22: more accurate than the glide, 3.6 s slower |
| Each tube's least and most speed | 7, 8 | within the motors' own limits | Not yet |
| Coarse stop threshold | 10 | the smallest that keeps clear of an overcharge | Not yet. On 2026-11-20 a stop 5 gr short left 3 gr and more falling, so the fine tube had 1.5 gr of room, not 5 |
| Fine stop offset (new) | 9 | the fine tube stops when the reading is the target less this average | Not yet. On 2026-11-20 an average of 4.6 digits landed after the slow finish's stop: stopping 0.11 gr short in place of 0.03 would have taken it away |
| Fine stop threshold | the user | stays the user's tolerance: the ± band of the verdict | |

## What we know

Measured on the bench (the Pico 2 W, an A&D FX-300i, AR2209) unless said; the figures are in `legacy/progress.md` up to 2026-11-21 and in branch `adr009_controller`'s progress after.
- **The scale is late by an amount that depends on the change** (2026-11-21): about 0.5 s behind a fast stream, about 1 s behind a slow one, about 2 s for a single digit; the same for two powders and two tubes, so mostly the scale's. It calls a weight stable 0.38 s after its last change; one kernel first shows 1.7 s after a nudge, three and more 0.9 s.
- **What lands after a fine stop scatters by about 2.5 digits at every speed tried** (2026-11-20), in stop tests and in real throws alike. No setting of today's loop removed it.
- **The fine tube's grains a turn change with speed:** the throws of 2026-11-22 suggested 0.68 at the finish and 0.83 at 1 rev/s.
- **Today's loop** (`charge_mode.cpp`): both tubes start together, each a PID loop on the reading between its least and most speed. The coarse tube stops at the target less `coarse_stop_threshold`, one setting for every powder. Both stop at the target less `fine_stop_threshold`, which is also the ± band of the verdict, judged about 1 s after the stop. The D term is per millisecond: the shipped AR2209 profile's Kd of 15 acts over about 19 ms, against the A&D's lag of about 550 ms (Kp × lag would be about 30 times it).
- **The first build's calibration** ran on the PC through the console (`tools/charge_sim/staged.py`'s `measure_on`, driven by `bench/trial.py`): it measured the data for items 4 to 9 in its own form (the settle time, each tube's grains a turn, and at each speed the lag and what lands after a stop), was given the digit, and took the tubes' least and most speeds from the user's limits. It took 8 to 13 minutes and poured 164 to 252 gr on the bench. Those tools and every recording are kept in `tools/charge_sim`.

## What is stored differently

- `coarse_stop_threshold` moves from the charge settings into each powder profile.
- A fine stop offset is added to each powder profile; `fine_stop_threshold` stays the user's tolerance.
- The unit keeps the scale's digit, response time and lag, and the cup's capacity and weight.

Each changes a stored layout: on every unit, a struct that changes size returns its module's settings to their defaults.

## To settle before approval

1. **The cup:** how the user gives its capacity (filled with the knob, or a number), and what the calibration does when a pour would pass it. First.
2. **"Consistent"** for items 7 and 8, in numbers: for example `bench/stop_spread.py`'s rule, the fastest speed whose spread is within 1.3 times the smallest.
3. **The D rule:** shown in the simulation, then on the bench, before it is written to a profile.
4. **Where the calibration runs:** its method is developed from the PC on the bench, where the tools exist; for users it is a job on the unit, with its screens.
5. **How long a calibration may take, and how much it may pour.**

## How the unit shows it works

- A calibration from nothing writes the bench unit's settings, and its throws are judged by the spread of the final weight first, then the time a throw; the average offset and the distribution in 0.02 gr bins are reported beside them. The bar is written before the run.
- No pour of the calibration passes the cup's capacity.
