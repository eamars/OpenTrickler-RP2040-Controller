"""`series.py`'s reads of a complete throw, checked at the desk with no unit and no powder (ADR-009, the review of
2026-11-24): a console reply missed and a web read that times out, as on 2026-11-22 and tonight.

Pass: in every case `read_throw` returns and raises nothing, so the arm goes on; the rows come from the console when it
answers on either try, else from the API's samples; the session's verdict counts a failure only when neither way had
the throw's rows.

Usage, with the repository's venv, from this folder:
    python check_series_reads.py
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import series   # noqa: E402

ROWS = ["@r 1 2 3", "@r 4 5 6"]


class Console:
    def __init__(self):
        self.data_lines = []


class Session:
    """The console: `ccrec` answered per `answers` in turn (None: no reply in time, as rc_run's send gives it)."""

    def __init__(self, answers):
        self.answers = list(answers)
        self.console = Console()
        self.failures = []

    def send(self, line):
        reply = self.answers.pop(0) if self.answers else "@ok"
        if reply is None:
            self.failures.append(f"no reply to \"{line}\" within 5 s")
            self.console.data_lines = []
            return None
        self.console.data_lines = list(ROWS)
        return reply


def web(outcomes):
    """A stand-in for `read_recording`: each call raises (a timeout) or answers, per `outcomes` in turn."""
    calls = list(outcomes)

    def read(repo, host, recording):
        outcome = calls.pop(0) if calls else "ok"
        if outcome == "timeout":
            raise TimeoutError("timed out")
        return {"entry": {"id": recording}, "samples": {"rows": [[1, 2]]}}
    return read


CASES = [
    # name, the console's answers, the web's, rows expected from, failures expected
    ("both answer", ["@ok ccrec"], ["ok"], "console", 0),
    ("the console missed once", [None, "@ok ccrec"], ["ok"], "console", 0),
    ("the web late once (tonight)", ["@ok ccrec"], ["timeout", "ok"], "console", 0),
    ("the console missed twice (11-22)", [None, None], ["ok"], "api", 0),
    ("the web late twice", ["@ok ccrec"], ["timeout", "timeout"], "console", 0),
    ("neither", [None, None], ["timeout", "timeout"], "none", 1),
]


def main() -> int:
    failures = []
    for name, answers, outcomes, rows_from, failed in CASES:
        series.read_recording = web(outcomes)
        session = Session(answers)
        try:
            out = series.read_throw(session, Path("."), "unit", 7)
        except Exception as error:      # noqa: BLE001  the very thing the mend forbids
            failures.append(f"{name}: raised {error!r}")
            continue
        had = "console" if out["rows"] else "api" if out["recording"] and out["recording"].get("samples") else "none"
        ok = had == rows_from and len(session.failures) == failed
        print(f"{name}: rows from {had}, session failures {len(session.failures)}; reads {out['reads']}: "
              f"{'as expected' if ok else 'NOT as expected'}")
        if not ok:
            failures.append(name)
    print("VERDICT PASS" if not failures else f"VERDICT FAIL {failures}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
