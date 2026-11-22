"""The staged controller under faults, on the unit with its simulated scale (ADR-009, gate 2).

Each case starts a charge of TARGET_GR on the simulated scale (10 readings a second: a pause after 300 ms without a
reading, lost after 1 s), puts one fault in when the throw reaches a stage, reads `state` every POLL_S, and judges the
case by its pass, written here before the first run:

- none: a throw, no pause, no end on a fault;
- bad: three frames that do not parse during the coarse stream: no pause, a throw, the bad frames counted;
- quiet_coarse, quiet_fine, quiet_move: no reading for 0.6 s during the coarse stream, the fine tube's creep, a small
  move: a pause and a resume; both tubes still from 0.45 s after the last reading to its end (three intervals and
  the ramp); then a throw;
- lost: no reading for 5 s during the coarse stream: the charge ends ("Scale: no reading"), both tubes still;
- stop: `charge stop` during the coarse stream: both tubes still within 0.2 s, the charge over within 2 s, no throw;
- driver: the fine driver read as shorted (`tmc fine fault short`) during the fine stream: the charge ends, both
  tubes still;
- cup_lifted: the cup lifted for 2 s during the coarse stream (the reading 150 gr under the empty cup) and put back:
  no tube turns while the cup is off (a person who lifts the cup mid-throw must not have powder poured on the pan).

The tubes do not need powder: the simulated scale makes its own. Run with the repository's venv:
    python robust_staged.py --repo C:/workspace/OpenTrickler-RP2040-Controller [--cases quiet_fine,lost] [--controller pid]
"""

import argparse
import sys
import time
from pathlib import Path

TARGET_GR = 20.0
POLL_S = 0.03
CASE_MOST_S = 40.0
QUIET_MS = 600
LOST_MS = 5000
CASES = ("none", "bad", "quiet_coarse", "quiet_fine", "quiet_move", "lost", "stop", "driver", "cup_lifted")


def fields(text: str) -> dict:
    out = {}
    for part in (text or "").split():
        if "=" in part:
            name, value = part.split("=", 1)
            try:
                out[name] = float(value)
            except ValueError:
                out[name] = value
    return out


class Case:
    def __init__(self, session, name: str):
        self.session, self.name = session, name
        self.samples = []       # (seconds since the fault, state)
        self.fault_at = None
        self.events_before = 0

    def state(self) -> dict:
        return fields(self.session.send("state"))

    def events(self) -> list:
        return [line for _, line in self.session.console.charge_events][self.events_before:]

    def reached(self, now: dict) -> bool:
        charging = int(now.get("charge", -1)) == 2
        coarse, fine, weight = now.get("coarse", 0.0), now.get("fine", 0.0), now.get("weight", 0.0)
        if self.name in ("bad", "quiet_coarse", "lost", "stop", "cup_lifted"):
            return charging and coarse > 0.0 and weight >= 5.0
        if self.name == "quiet_fine":
            return charging and coarse == 0.0 and 0.0 < fine < 0.1 and weight >= TARGET_GR - 1.0
        if self.name == "quiet_move":
            return charging and coarse == 0.0 and abs(fine - 0.2) < 1e-6
        if self.name == "driver":
            return charging and coarse == 0.0 and fine > 0.0
        return charging

    def put_in(self) -> None:
        send = self.session.send
        if self.name == "bad":
            for _ in range(3):
                send("sim bad")
                time.sleep(0.15)
        elif self.name.startswith("quiet"):
            send(f"sim quiet {QUIET_MS}")
        elif self.name == "lost":
            send(f"sim quiet {LOST_MS}")
        elif self.name == "stop":
            send("charge stop")
        elif self.name == "driver":
            send("tmc fine fault short")
        elif self.name == "cup_lifted":
            send("sim weight -150")

    def run(self) -> tuple:
        send = self.session.send
        bad_before = fields(send("stats")).get("bad_frames", 0.0)
        self.events_before = len(self.session.console.charge_events)
        reply = send(f"charge {TARGET_GR:g}")
        if not reply or not reply.startswith("@ok"):
            return False, f"not started: {reply}"
        start = time.monotonic()
        while self.fault_at is None and time.monotonic() - start < CASE_MOST_S:
            now = self.state()
            if self.reached(now):
                self.fault_at = time.monotonic()
                self.put_in()
                break
            if any("Charge: throw" in line for line in self.events()) and self.name != "none":
                break
            time.sleep(POLL_S)
        if self.fault_at is None and self.name != "none":
            return None, "the stage was not reached in this throw"
        if self.fault_at is None:
            self.fault_at = start
        lifted_until = self.fault_at + 2.0
        put_back = False
        while time.monotonic() - self.fault_at < CASE_MOST_S:
            now = self.state()
            t = time.monotonic() - self.fault_at
            self.samples.append((t, now))
            if self.name == "cup_lifted" and not put_back and time.monotonic() >= lifted_until:
                send("sim weight off")
                put_back = True
            events = self.events()
            if any("Charge: throw" in line or "Charge: ended" in line for line in events):
                break
            if int(now.get("charge", -1)) == 0 and t >= 1.2:     # the tubes watched for a while after an end
                break
            time.sleep(POLL_S)
        time.sleep(0.5)
        bad_after = fields(send("stats")).get("bad_frames", 0.0)
        return self.judge(bad_after - bad_before)

    def still(self, t0: float, t1: float) -> bool:
        window = [s for t, s in self.samples if t0 <= t <= t1]
        return bool(window) and all(s.get("coarse", 0.0) == 0.0 and s.get("fine", 0.0) == 0.0 for s in window)

    def judge(self, bad_frames: float) -> tuple:
        events = self.events()
        text = " | ".join(line for line in events if line.startswith("Charge:"))
        paused = any("Charge: paused" in line for line in events)
        resumed = any("Charge: resumed" in line for line in events)
        ended = any("Charge: ended" in line for line in events)
        throw = any("Charge: throw" in line for line in events)
        last = self.samples[-1][1] if self.samples else {}
        stopped = last.get("coarse", 0.0) == 0.0 and last.get("fine", 0.0) == 0.0
        if self.name == "none":
            return throw and not paused and not ended, text
        if self.name == "bad":
            return throw and not paused and not ended and bad_frames >= 3, f"bad frames {bad_frames:.0f}; {text}"
        if self.name.startswith("quiet"):
            quiet_still = self.still(0.45, QUIET_MS / 1000.0)
            return paused and resumed and throw and quiet_still, f"still in the quiet: {quiet_still}; {text}"
        if self.name == "lost":
            return ended and not throw and stopped and "no reading" in text, text
        if self.name == "stop":
            soon = self.still(0.2, 1.0)
            over = any(int(s.get("charge", -1)) == 0 for t, s in self.samples if t <= 2.0)
            return soon and over and not throw, f"still from 0.2 s: {soon}, over within 2 s: {over}; {text}"
        if self.name == "driver":
            return ended and stopped, text
        if self.name == "cup_lifted":
            off = [s for t, s in self.samples if 0.3 <= t <= 2.0]
            poured = [s for s in off if s.get("coarse", 0.0) > 0.0 or s.get("fine", 0.0) > 0.0]
            return not poured, f"turning while the cup was off in {len(poured)} of {len(off)} reads; {text}"
        return False, "no such case"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--board", default="pico2_w")
    parser.add_argument("--cases", default=",".join(CASES))
    parser.add_argument("--controller", choices=("staged", "pid"), default="staged")
    args = parser.parse_args()
    sys.path.insert(0, str(args.repo / "tools" / "remote_control"))
    import argparse as _argparse
    import rc_run
    session = rc_run.Session(_argparse.Namespace(board=args.board, elf=None), "adr009_robust")
    results = []
    try:
        session.send(f"setting settings/charge controller={args.controller}")
        session.send("sim on")
        for name in args.cases.split(","):
            case = Case(session, name)
            verdict, detail = case.run()
            if int(fields(session.send("state")).get("charge", 0)) != 0:
                session.send("charge stop")
            session.send("sim weight off")
            session.send("tmc fine fault none")
            session.send("motor coarse 0")
            session.send("motor fine 0")
            # the cup emptied, as a person empties it after a throw cut short: the simulated scale zeroed
            session.send("sim off")
            session.send("sim on")
            time.sleep(1.0)
            word = "PASS" if verdict else ("NOT RUN" if verdict is None else "FAIL")
            results.append((name, word))
            print(f"{name:13s} {word:8s} {detail}", flush=True)
    finally:
        if int(fields(session.send("state")).get("charge", 0)) != 0:
            session.send("charge stop")
        session.send("sim weight off")
        session.send("tmc fine fault none")
        session.send("motor coarse 0")
        session.send("motor fine 0")
        session.send("sim off")
        session.send("setting settings/charge revert")
        session.finish()
    print("CASES " + " ".join(f"{n}={w}" for n, w in results))
    return 0 if all(w == "PASS" for _, w in results) else 1


if __name__ == "__main__":
    sys.exit(main())
