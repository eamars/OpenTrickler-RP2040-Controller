"""
No feature silently dropped (ADR-008): every row of features.md either shown by the new pages, left out by its
"Proposed" column, or still to come with the gate that brings it, and nothing else.

A row is shown where a comment `features: <row> ...` stands beside the code that does it, in src/web. The rows of
features.md's tables are numbered; its new items are N1, N2, ... Listed, then judged:

    venv\\Scripts\\python tests\\features_check.py [--final]

It fails on a row neither shown, left out nor to come; on a mark of a row left out, of a row still to come, or of no
row at all. With --final (the proof of gates 3 and 4) a row still to come fails too. Only the standard library.
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FEATURES = REPO / "docs" / "developments_plans" / "ADR-008-web_ui" / "features.md"
SOURCES = REPO / "src" / "web"
MARK = re.compile(r"features:((?:\s+N?\d+)+)")
ROW = re.compile(r"^\| (\d+) \|")
NEW = re.compile(r"^- \*\*(N\d+)\. ")

# The rows the mock does not show yet, each with the gate that brings it. It empties as the gates are built: with
# --final, any row here fails
TO_COME = {
    "1": "the set-up (gate 1's set-up pages, built in gate 4)",
    "2": "the set-up",
    "3": "gate 3: the unit serves the portal at /mobile too",
    "4": "the set-up",
    "5": "gate 3: /display_mirror opens the portal's Unit page, where the mirror is",
    "6": "gate 3: the pages gzipped, with their ETag",
    "78": "the set-up", "79": "the set-up", "80": "the set-up", "81": "the set-up", "82": "the set-up",
    "83": "the set-up", "84": "the set-up", "85": "the set-up",
    "N7": "the set-up",
    "N8": "gate 2: the event for a setting changed, on the event stream",
    "N9": "gate 2: the unit's report of what it carries",
}


def read_rows():
    """
    The rows of features.md and what each proposes.

    Returns:
        {row id: its "Proposed" column, or "new" for an item of "New, not in today's pages"}, in the file's order.
    """
    rows = {}
    for line in FEATURES.read_text(encoding="utf-8").splitlines():
        numbered = ROW.match(line)
        if numbered:
            rows[numbered.group(1)] = line.split("|")[5].strip()
            continue
        new = NEW.match(line)
        if new:
            rows[new.group(1)] = "new"
    return rows


def read_marks():
    """
    Every row marked in the pages' sources.

    Returns:
        {row id: ["file:line", ...]}.
    """
    marks = {}
    for path in sorted(SOURCES.iterdir()):
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            for found in MARK.finditer(line):
                for row in found.group(1).split():
                    marks.setdefault(row, []).append(f"{path.name}:{number}")
    return marks


def main():
    """List the rows by where they are, and judge them."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--final", action="store_true", help="a row still to come fails too (gates 3 and 4)")
    args = parser.parse_args()
    rows = read_rows()
    marks = read_marks()
    faults = []

    shown = [row for row in rows if row in marks]
    left_out = {row: proposed for row, proposed in rows.items() if proposed.startswith("Left out")}
    to_come = {row: gate for row, gate in TO_COME.items() if row in rows}
    print(f"{len(rows)} rows: {len(shown)} shown, {len(left_out)} left out, {len(to_come)} to come")

    print("\nLeft out (features.md says so; the owner settles those proposed):")
    for row, proposed in left_out.items():
        print(f"  {row:>4}  {proposed}")
    print("\nTo come:")
    for row, gate in to_come.items():
        print(f"  {row:>4}  {gate}")

    for row in rows:
        if row not in marks and row not in left_out and row not in to_come:
            faults.append(f"row {row} is neither shown, left out nor to come")
        if row in marks and row in left_out:
            faults.append(f"row {row} is left out but marked at {', '.join(marks[row])}")
        if row in marks and row in to_come:
            faults.append(f"row {row} is to come but marked at {', '.join(marks[row])}: take it out of TO_COME")
    for row, where in marks.items():
        if row not in rows:
            faults.append(f"{', '.join(where)} marks row {row}, which features.md does not have")
    for row in TO_COME:
        if row not in rows:
            faults.append(f"TO_COME names row {row}, which features.md does not have")
    if args.final:
        faults.extend(f"row {row} is still to come ({gate})" for row, gate in to_come.items())

    if faults:
        print("\n" + "\n".join(faults))
        print(f"\nVERDICT FAIL {len(faults)} fault(s)")
        sys.exit(1)
    print("\nVERDICT PASS")


if __name__ == "__main__":
    main()
