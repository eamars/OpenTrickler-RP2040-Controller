"""Profile 0's record made the same before every arm of an ABBA (ADR-009, F.5's ABBA, 2026-11-24).

The record is the unit's stored one with its carried stops put back to the 23:56 trial's floor rehearsals: its
measured part (the 23:56 calibration and F.3's back-off spill, as stored) stays as it is; its first errors become the
rehearsals' (`throws_glide_0.02.json` of logs/adr009/20261123_235643_trial_pico2w_ar2209_f2a) and its law the glide's,
as `ccal save` writes a calibration's. Written through the API (a PATCH, then the save), between charges only; then
read back and compared, field by field, with the record it was made from. Run it before each arm; it can be run again
at any time to make the same record.

Usage, with the repository's venv:
    python abba_record.py --host <address> [--profile 0] [--check]     --check: compare only, write nothing
"""

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "remote_control"))
import unit_api     # noqa: E402

TRIAL = REPO / "logs" / "adr009" / "20261123_235643_trial_pico2w_ar2209_f2a"


def rehearsals() -> list:
    return json.loads((TRIAL / "throws_glide_0.02.json").read_text())["calibration"]["first_errors"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--host", required=True)
    parser.add_argument("--profile", type=int, default=0)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    unit = unit_api.Unit(args.host)
    path = f"settings/powder_records/{args.profile}"
    status, record = unit.request("GET", path)
    if status != 200 or not record.get("held"):
        print(f"no record to start from: {status} {record}")
        return 1
    errors = rehearsals()
    wanted = [round(e, 6) for e in errors]
    have = [round(row["error_gr"], 6) for row in record.get("first_errors", [])]
    if args.check:
        same = have == wanted and record.get("law") == "glide"
        print(f"profile {args.profile}: first errors {have}, law {record.get('law')}: "
              f"{'the starting record' if same else 'NOT the starting record'}")
        return 0 if same else 1
    body = {"unit_id": record["unit_id"], "law": "glide", "first_errors": [{"error_gr": e} for e in errors]}
    status, reply = unit.request("PATCH", path, body)
    if status != 200:
        print(f"the write refused: {status} {reply}")
        return 1
    status, reply = unit.request("POST", "settings/powder_records/save", {})
    if status != 200:
        print(f"the save refused: {status} {reply}")
        return 1
    status, after = unit.request("GET", path)
    changed = sorted(k for k in record if k not in ("first_errors", "law", "saved") and after.get(k) != record.get(k))
    have = [round(row["error_gr"], 6) for row in after.get("first_errors", [])]
    ok = status == 200 and after.get("saved") and have == wanted and after.get("law") == "glide" and not changed
    print(f"profile {args.profile}: first errors {have}, law {after.get('law')}, saved {after.get('saved')}; "
          f"other fields changed: {changed or 'none'}: {'the starting record' if ok else 'FAILED'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
