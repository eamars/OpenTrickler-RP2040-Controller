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
  Since ADR-009's gate F.1: the charge ends "Cup lifted", that reason alone, both tubes still;
- light_cup: the same with a cup of 5 gr (the reading 5 under the empty cup): the same end;
- cup_lifted_quiet: the scale silent and the cup lifted at once, during the coarse stream: the charge ends "Scale: no
  reading" alone, both tubes still;
- drift: the reading a digit lower from mid-coarse on (`sim offset`): a throw, no "Cup lifted";
- no_powder: no powder from the start (`sim flow 0`): the charge ends "No powder", both tubes still, no throw;
- no_powder_drift: the same, the reading a digit low from the throw's start (a digit low before it never passes the
  zero): the same end;
- third: a third of the powder from the start (`sim flow 0.333`): a throw, neither "limit" nor "No powder";
- far_over: the reading at the target and 100 tolerances and a digit mid-coarse (`sim weight 23.04`): the throw ends
  "limit".

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
CASES = ("none", "bad", "quiet_coarse", "quiet_fine", "quiet_move", "lost", "stop", "driver", "cup_lifted", "light_cup",
         "cup_lifted_quiet", "drift", "no_powder", "no_powder_drift", "third", "far_over")
FROM_START = {"no_powder": ("sim flow 0",), "no_powder_drift": ("sim flow 0",), "third": ("sim flow 0.333",)}
AT_STAGE_TOO = ("no_powder_drift",)     # also a fault put in once the throw has begun
LIFTED = ("cup_lifted", "light_cup", "cup_lifted_quiet")
# The most each case pours into the real cup, in grains (the hopper is filled: rc_run's cup check): a throw's target
# and room; a third of the simulated powder is three times as much real; no powder pours real powder until it is found
MOST_POUR = {"none": TARGET_GR + 10.0, "bad": TARGET_GR + 10.0, "quiet_coarse": TARGET_GR + 10.0,
             "quiet_fine": TARGET_GR + 10.0, "quiet_move": TARGET_GR + 10.0, "drift": TARGET_GR + 10.0,
             "driver": TARGET_GR + 10.0, "third": 3.0 * TARGET_GR + 10.0, "no_powder": 50.0, "no_powder_drift": 50.0}
CASE_MOST_POUR = 20.0           # a case cut short mid-coarse


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
        if self.name in ("bad", "quiet_coarse", "lost", "stop", "cup_lifted", "light_cup", "cup_lifted_quiet", "drift",
                         "far_over"):
            return charging and coarse > 0.0 and weight >= 5.0
        if self.name == "quiet_fine":
            return charging and coarse == 0.0 and 0.0 < fine < 0.1 and weight >= TARGET_GR - 1.0
        if self.name == "quiet_move":
            return charging and coarse == 0.0 and abs(fine - 0.2) < 1e-6
        if self.name == "driver":
            return charging and coarse == 0.0 and fine > 0.0
        if self.name == "no_powder_drift":
            return charging and coarse > 0.0
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
        elif self.name == "light_cup":
            send("sim weight -5")
        elif self.name == "cup_lifted_quiet":
            send("sim quiet 5000")
            send("sim weight -150")
        elif self.name in ("drift", "no_powder_drift"):
            send("sim offset -0.02")
        elif self.name == "far_over":
            send(f"sim weight {TARGET_GR + 3.04:g}")

    def run(self) -> tuple:
        send = self.session.send
        bad_before = fields(send("stats")).get("bad_frames", 0.0)
        self.events_before = len(self.session.console.charge_events)
        for line in FROM_START.get(self.name, ()):
            send(line)
        reply = send(f"charge {TARGET_GR:g}")
        if not reply or not reply.startswith("@ok"):
            return False, f"not started: {reply}"
        start = time.monotonic()
        if self.name in FROM_START and self.name not in AT_STAGE_TOO:
            self.fault_at = start
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
            if self.name in LIFTED and not put_back and time.monotonic() >= lifted_until:
                send("sim weight off")
                put_back = True
            events = self.events()
            if any("Charge: throw" in line or "Charge: ended" in line for line in events):
                break
            if int(now.get("charge", -1)) == 0 and t >= 1.2:     # the tubes watched for a while after an end
                break
            time.sleep(POLL_S)
        time.sleep(0.5)
        # the tubes judged after the motors' ramp: a sample taken at the end's own line may catch them slowing
        self.samples.append((time.monotonic() - self.fault_at, self.state()))
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
        throw = any("Charge: throw" in line and "result=cup" not in line and "result=no_powder" not in line
                    for line in events)
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
        ends = sum(1 for line in events if "Charge: ended" in line)
        if self.name in ("cup_lifted", "light_cup"):
            off = [s for t, s in self.samples if 0.3 <= t <= 2.0]
            poured = [s for s in off if s.get("coarse", 0.0) > 0.0 or s.get("fine", 0.0) > 0.0]
            ok = not poured and ended and ends == 1 and "Cup lifted" in text and not throw and stopped
            return ok, f"turning while the cup was off in {len(poured)} of {len(off)} reads; {text}"
        if self.name == "cup_lifted_quiet":
            ok = ended and ends == 1 and "no reading" in text and "Cup lifted" not in text and stopped
            return ok, text
        if self.name == "drift":
            return throw and not ended and "Cup lifted" not in text, text
        if self.name in ("no_powder", "no_powder_drift"):
            return ended and ends == 1 and "No powder" in text and not throw and stopped, text
        if self.name == "third":
            return throw and not ended and "result=limit" not in text, text
        if self.name == "far_over":
            return throw and "result=limit" in text, text
        return False, "no such case"


def charge_over(session, most_s: float = 10.0) -> bool:
    """The charge stopped and gone: a charge ended on a fault holds its screen until a press, which `charge stop`
    gives, and the mode then takes a moment to leave. True once `state` says no charge."""
    until = time.monotonic() + most_s
    while time.monotonic() < until:
        if int(fields(session.send("state")).get("charge", 0)) == 0:
            return True
        session.send("charge stop")
        time.sleep(0.5)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--board", default="pico2_w")
    parser.add_argument("--cases", default=",".join(CASES))
    parser.add_argument("--controller", choices=("staged", "pid"), default="staged")
    parser.add_argument("--most-pour", type=float, help="the most the cases pour, in grains (default: by case)")
    args = parser.parse_args()
    sys.path.insert(0, str(args.repo / "tools" / "remote_control"))
    import argparse as _argparse
    import rc_run
    session = rc_run.Session(_argparse.Namespace(board=args.board, elf=None), "adr009_robust")
    results = []
    names = args.cases.split(",")
    most = args.most_pour if args.most_pour is not None else sum(MOST_POUR.get(n, CASE_MOST_POUR) for n in names)
    go, before = session.cup_before("robust_staged " + args.cases, most, None)
    if not go:
        print(f"refused: the cup ({before}) and the cases' most pour ({most:.0f} gr)")
        return session.finish()
    try:
        session.send(f"setting settings/charge controller={args.controller}")
        session.send("sim on")
        for name in args.cases.split(","):
            case = Case(session, name)
            verdict, detail = case.run()
            if not charge_over(session):
                verdict, detail = False, f"the charge did not end after its case; {detail}"
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
        charge_over(session)
        session.send("sim weight off")
        session.send("tmc fine fault none")
        session.send("motor coarse 0")
        session.send("motor fine 0")
        session.send("sim off")
        session.send("setting settings/charge revert")
        session.cup_after("robust_staged " + args.cases, before, most)
        session.finish()
    print("CASES " + " ".join(f"{n}={w}" for n, w in results))
    return 0 if all(w == "PASS" for _, w in results) else 1


if __name__ == "__main__":
    sys.exit(main())
