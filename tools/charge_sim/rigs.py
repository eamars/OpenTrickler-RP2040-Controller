"""The rigs the simulation runs on: each a record of what was measured on a unit, kept in `records/`.

A record is one JSON file: a scale, a powder's kernel, and each tube with that powder in it, every number with its
source (measured, solved from a recording, stated by the owner, or assumed because no recording gives it yet).
`build_rig.py` writes a record from a bench's recordings; nothing in a record is set by hand. Another unit, powder
or scale is another record, not other code.
"""

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
    import dataclasses
    base = rig.fine.gr_per_turn[0][1]
    return dataclasses.replace(rig, fine=dataclasses.replace(
        rig.fine, gr_per_turn=tuple((speed, share * base) for speed, share in FINE_BY_SPEED)))


VARIANTS = {"fine_by_speed": _fine_by_speed}


def get_rig(name: str) -> Rig:
    """A rig by its record's name or path, with a variant after a "+" (VARIANTS)."""
    name, _, variant = name.partition("+")
    rig = rig_from_record(load_record(name))
    return VARIANTS[variant](rig) if variant else rig


def names() -> list:
    """The records kept, by name."""
    return sorted(path.stem for path in RECORDS.glob("*.json"))
