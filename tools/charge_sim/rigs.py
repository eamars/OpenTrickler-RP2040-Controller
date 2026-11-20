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
TABLES = ("gr_per_turn", "burst_kernels", "after_stop_kernels")


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


def get_rig(name: str) -> Rig:
    """A rig by its record's name or path."""
    return rig_from_record(load_record(name))


def names() -> list:
    """The records kept, by name."""
    return sorted(path.stem for path in RECORDS.glob("*.json"))
