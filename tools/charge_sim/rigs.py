"""The rigs the simulation runs on: each a record of what was measured on a unit, kept in `records/`.

A record is one JSON file: a scale, a powder's kernel, and each tube with that powder in it, every number with its
source (measured, solved from a recording, stated by the owner, or assumed because no recording gives it yet).
`build_rig.py` writes a record from a bench's recordings; nothing in a record is set by hand. Another unit, powder
or scale is another record, not other code.
"""

import dataclasses
import json
from pathlib import Path

from plant import Powder, Tube
from scale import ScaleModel
from throw import Rig

RECORDS = Path(__file__).parent / "records"
TABLES = ("gr_per_turn", "burst_kernels", "late_s")


def tube_from_record(fields: dict) -> Tube:
    """A tube from its part of a record: its tables become tuples, by rising speed."""
    fields = dict(fields)
    for name in TABLES:
        fields[name] = tuple(sorted((float(x), float(y)) for x, y in fields[name]))
    return Tube(**fields)


def rig_from_record(record: dict) -> Rig:
    """The simulation's rig from a record's four parts and its motors' limits."""
    return Rig(powder=Powder(**record["powder"]), coarse=tube_from_record(record["coarse"]),
               fine=tube_from_record(record["fine"]), scale=ScaleModel(**record["scale"]), **record["motors"])


def load_record(name: str) -> dict:
    """A record by its name in `records/`, or by a path to its file."""
    path = Path(name)
    if not path.suffix:
        path = RECORDS / f"{name}.json"
    return json.loads(path.read_text(encoding="utf-8"))


# Variants named after a record with "+": an assumption put on its rig to see what a controller makes of it, never a
# record of its own (a record is only what was measured)
# fine_by_speed (2026-11-22): the fine tube pours by speed as the rig's 5 gr throws suggested, 0.68 gr a turn at the
# finish (taken here at 0.1 rev/s and below) and 0.83 at 1 rev/s, AR2209 on the Pico 2 W; scaled to the record's own
# figure for another powder. Only the two ends are measured; the line between them is assumed
FINE_BY_SPEED = ((0.1, 0.68 / 0.7652), (1.0, 0.83 / 0.7652))


def _fine_by_speed(rig: Rig) -> Rig:
    base = rig.fine.gr_per_turn[0][1]
    return dataclasses.replace(rig, fine=dataclasses.replace(
        rig.fine, gr_per_turn=tuple((speed, share * base) for speed, share in FINE_BY_SPEED)))


# The use cases' variants (2026-11-26, the owner: "try more experiments, for failure modes, or out of ordinary
# exceptions"), each an assumption, none measured:
# big_kernel: the same tubes and flow with kernels three times as heavy, the lumpier powders the owner names ("some
#   larger kernel powder is hard to dispense precisely");
# gram_digit: the scale read in grams, its digit 0.001 g (0.0154 gr);
# draught: gusts on the pan, six a minute, 0.03 gr each (sd), a second long;
# missing: one report in twenty never arrives;
# fine_empty, coarse_empty: the tube's hopper empty, the tube turning and pouring next to nothing.
EMPTY_GR_A_TURN = 1e-4


def _scaled_kernel(rig: Rig) -> Rig:
    return dataclasses.replace(rig, powder=dataclasses.replace(rig.powder, kernel_gr=3.0 * rig.powder.kernel_gr))


def _scale(rig: Rig, **fields) -> Rig:
    return dataclasses.replace(rig, scale=dataclasses.replace(rig.scale, **fields))


def _empty(tube):
    return dataclasses.replace(tube, gr_per_turn=((1.0, EMPTY_GR_A_TURN),))


# grams (2026-11-26, the owner: "If a scale reports gram, while the default parameter is grain, then I need a reliable
#   way to tune down to support gram, with the auto tuner. Noted the calibration must NOT assume the unit."): the same
#   rig weighed in grams, every weight it holds divided by 15.4324 and the A&D's digit in grams, 0.001 g. Everything the
#   simulation reports of it is then in grams, though its fields keep their names in grains
GRAINS_A_GRAM = 15.4324


def _grams_tube(tube):
    return dataclasses.replace(tube, gr_per_turn=tuple((s, w / GRAINS_A_GRAM) for s, w in tube.gr_per_turn),
                               reverse_spill_gr=tube.reverse_spill_gr / GRAINS_A_GRAM)


def _grams(rig: Rig) -> Rig:
    scale = rig.scale
    return dataclasses.replace(
        rig, powder=dataclasses.replace(rig.powder, kernel_gr=rig.powder.kernel_gr / GRAINS_A_GRAM),
        coarse=_grams_tube(rig.coarse), fine=_grams_tube(rig.fine),
        scale=dataclasses.replace(scale, digit_gr=0.001, open_above_gr=scale.open_above_gr / GRAINS_A_GRAM,
                                  noise_gr=scale.noise_gr / GRAINS_A_GRAM,
                                  zero_track_gr=scale.zero_track_gr / GRAINS_A_GRAM,
                                  zero_track_gr_s=scale.zero_track_gr_s / GRAINS_A_GRAM,
                                  draught_gr=scale.draught_gr / GRAINS_A_GRAM))


VARIANTS = {"fine_by_speed": _fine_by_speed,
            "big_kernel": _scaled_kernel,
            "gram_digit": lambda rig: _scale(rig, digit_gr=0.001 * 15.4324),
            "draught": lambda rig: _scale(rig, draught_per_min=6.0, draught_gr=0.03, draught_tau_s=1.0),
            "missing": lambda rig: _scale(rig, miss_share=0.05),
            "fine_empty": lambda rig: dataclasses.replace(rig, fine=_empty(rig.fine)),
            "coarse_empty": lambda rig: dataclasses.replace(rig, coarse=_empty(rig.coarse)),
            "grams": _grams}


def get_rig(name: str) -> Rig:
    """A rig by its record's name or path, with each variant after a "+" (VARIANTS), in order."""
    name, *variants = name.split("+")
    rig = rig_from_record(load_record(name))
    for variant in variants:
        rig = VARIANTS[variant](rig)
    return rig


def names() -> list:
    """The records kept, by name."""
    return sorted(path.stem for path in RECORDS.glob("*.json"))
