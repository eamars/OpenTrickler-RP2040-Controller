"""A series of throws by the unit's own staged controller (ADR-009, gate 2), from the PC through the console.

The firmware runs the controller (src/charge_control.c) every millisecond; the PC only sets it up and listens:
1. the calibration is written into the chosen powder profile's slot with `ccal` (from a kept trial's calibration,
   `staged.to_json`'s shape, or a throw set's `calibration`), for the finish and band asked;
2. the charge's controller is set to the staged one, live and not stored (`setting settings/charge controller=staged`);
3. `charge <target> series <n>`: n throws into the same cup, each from the weight the cup rests at, no cup lifted;
4. each throw's line (`Charge: throw ...`) is read as it comes, and its rows (`ccrec`) before the next begins.

A throw's settled weight is the next one's base less its own (what fell late is in it); the last throw's, the cup
read once more after the series. Kept in <repo>/logs/adr009/<time>_series_<name>/series.json.

The bench rules hold (AGENTS.md): the owner says the bench is ready and what is in the hopper; an empty cup, or one
with room, on the scale. A series ends by itself after its throws, when the cup holds 600 gr, or with Ctrl+C, which
sends `charge stop`.

Usage, with the repository's venv:
    python series.py --repo C:\\workspace\\OpenTrickler-RP2040-Controller --board pico2_w --name ar2209_glide \\
        --calibration ../recordings/20261121_162403_trial_pico2w_ar2209/calibration.json \\
        --errors-from ../recordings/20261121_162403_trial_pico2w_ar2209/throws_glide_0.02.json \\
        --finish glide --band 0.02 --target 10 --throws 25
    python series.py --repo ... --name dry --throws 2 ...          two throws first, to see it runs
"""

import argparse
import datetime
import json
import re
import statistics
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import staged       # noqa: E402  (../staged.py)

THROW_MOST_S = 150.0
POLL_S = 1.0
SETTLE_S = 5.0
THROW_LINE = re.compile(r"Charge: throw (.*)")


def calibration_commands(cal: staged.Calibration, errors: list, finish: str, band_gr: float) -> list:
    """The `ccal` lines that write a calibration: its measured part, the rehearsals' first errors, the finish."""
    lines = ["ccal clear",
             f"ccal head {cal.digit_gr:.6g} {cal.interval_s:.6g} {cal.coarse_gr_turn:.6g} {cal.fine_gr_turn:.6g} "
             f"{cal.watch_s:.6g} {cal.coarse_share:.6g}"]
    for tube, stops in (("coarse", cal.coarse_stops), ("fine", cal.fine_stops)):
        for i, stop in enumerate(stops):
            lag = -1.0 if stop.lag_s is None else stop.lag_s
            lines.append(f"ccal stop {tube} {i} {stop.speed:.6g} {stop.unseen_gr:.6g} {stop.spread_gr:.6g} "
                         f"{stop.quiet_s:.6g} {lag:.6g}")
    for i, move in enumerate(cal.moves):
        lines.append(f"ccal move {i} {move.turns:.6g} {move.mean_gr:.6g} {move.most_gr:.6g} {move.wait_s:.6g}")
    for start in range(0, len(errors), 6):
        lines.append("ccal errors " + " ".join(f"{error:.6g}" for error in errors[start:start + 6]))
    lines.append(f"ccal finish {finish} {band_gr:.6g}")
    lines.append("ccal save")
    return lines


def fields(text: str) -> dict:
    out = {}
    for part in text.split():
        if "=" in part:
            name, value = part.split("=", 1)
            try:
                out[name] = float(value)
            except ValueError:
                out[name] = value
    return out


def state(session) -> dict:
    reply = session.send("state") or ""
    return fields(reply)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--board", default="pico2_w")
    parser.add_argument("--name", required=True)
    parser.add_argument("--calibration", type=Path, required=True,
                        help="a calibration in staged.to_json's shape, or a throw set with one under `calibration`")
    parser.add_argument("--errors-from", type=Path,
                        help="a throw set whose calibration holds the rehearsals' first errors (else the calibration's)")
    parser.add_argument("--errors", help="the rehearsals' first errors, comma separated, in place of the calibration's: "
                        "a series' own, each corrected to the rule's stop (first + stop_room - the rule's stop room)")
    parser.add_argument("--finish", choices=("glide", "jog"), default="glide")
    parser.add_argument("--band", type=float, default=0.02)
    parser.add_argument("--target", type=float, default=10.0)
    parser.add_argument("--throws", type=int, default=25)
    parser.add_argument("--ccon", default="", help="constants for this series: `name=value ...` (never stored)")
    parser.add_argument("--lag-shift", type=float, default=0.0,
                        help="seconds added to every stop's lag before it is written: -0.055 for a calibration made by "
                             "the PC through the console, whose lags ran that far behind the firmware's (2026-11-22)")
    args = parser.parse_args()

    kept = json.loads(args.calibration.read_text())
    cal = staged.from_json(kept.get("calibration", kept))
    if args.lag_shift:
        staged.shift_lags(cal, args.lag_shift)
    errors = cal.first_errors
    if args.errors_from:
        errors = json.loads(args.errors_from.read_text())["calibration"]["first_errors"]
    if args.errors:
        errors = [float(value) for value in args.errors.split(",")]

    sys.path.insert(0, str(args.repo / "tools" / "remote_control"))
    import argparse as _argparse
    import rc_run   # the checkout's own runner
    session = rc_run.Session(_argparse.Namespace(board=args.board, elf=None), f"adr009_series_{args.name}")
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    folder = args.repo / "logs" / "adr009" / f"{stamp}_series_{args.name}"
    folder.mkdir(parents=True)
    record = {"name": args.name, "started": stamp, "target_gr": args.target, "finish": args.finish,
              "band_gr": args.band, "calibration_file": str(args.calibration), "errors": errors, "ccon_asked": args.ccon,
              "lag_shift_s": args.lag_shift, "throws": []}
    started_charge = False
    try:
        now = state(session)
        if int(now.get("charge", -1)) != 0 or int(now.get("form", -1)) < 0:
            print(f"the unit is not at its menu: {now}")
            return 1
        record["firmware"] = now.get("ver")
        for line in calibration_commands(cal, errors, args.finish, args.band):
            reply = session.send(line)
            if reply is None or reply.startswith("@err"):
                print(f"{line}: {reply}")
                return 1
        record["ccal"] = list(session.console.data_lines)
        for line in record["ccal"]:
            if line.startswith("@cal rules"):
                print(line)
        if args.ccon:
            session.send(f"ccon {args.ccon}")
        session.send("ccon")
        record["ccon"] = list(session.console.data_lines)
        session.send("setting settings/charge controller=staged")
        session.console.charge_events.clear()
        reply = session.send(f"charge {args.target:g} series {args.throws}")
        if reply is None or not reply.startswith("@ok"):
            print(f"not started: {reply}")
            return 1
        started_charge = True
        last_throw = time.monotonic()
        seen = 0
        while True:
            time.sleep(POLL_S)
            events = [line for _, line in session.console.charge_events]
            throws = [THROW_LINE.match(line) for line in events]
            throws = [match.group(1) for match in throws if match]
            for text in throws[seen:]:
                entry = fields(text)
                session.send("ccrec")
                entry["rows"] = [[float(x) for x in line.split()[1:]] for line in session.console.data_lines
                                 if line.startswith("@r ")]
                record["throws"].append(entry)
                error = entry["rest"] - args.target
                print(f"throw {len(record['throws'])}: {entry['result']} rest {entry['rest']:.3f} "
                      f"({error:+.3f}) first {entry.get('first', float('nan')):+.3f} moves {int(entry['moves'])} "
                      f"streams {int(entry['streams'])} {entry['elapsed']:.1f} s", flush=True)
                last_throw = time.monotonic()
            seen = len(throws)
            if any("Charge: ended" in line for line in events):
                print("the charge ended on a fault:", [line for line in events if "ended" in line])
                break
            now = state(session)
            if int(now.get("charge", -1)) == 0:
                break
            if time.monotonic() - last_throw > THROW_MOST_S:
                print("no throw for too long: stopping")
                break
        started_charge = int(state(session).get("charge", 0)) != 0
        time.sleep(SETTLE_S)
        record["cup_after_gr"] = state(session).get("weight")
    except KeyboardInterrupt:
        print("stopped by hand")
    finally:
        if started_charge:
            session.send("charge stop")
        for tube in ("coarse", "fine"):
            session.send(f"motor {tube} 0")
        # Each throw's settled weight: the next one's base less its own
        throws = record["throws"]
        for i, entry in enumerate(throws):
            following = throws[i + 1]["base"] if i + 1 < len(throws) else record.get("cup_after_gr")
            entry["settled"] = (following - entry["base"]) if isinstance(following, float) else None
        (folder / "series.json").write_text(json.dumps(record, indent=1))
        exit_code = session.finish()

    throws = record["throws"]
    if throws:
        within = args.band + 1e-6
        good = sum(1 for t in throws if abs(t["rest"] - args.target) <= within)
        over = sum(1 for t in throws if t["rest"] - args.target > within)
        settled_good = [t for t in throws if t["settled"] is not None]
        settled_ok = sum(1 for t in settled_good if abs(t["settled"] - args.target) <= within)
        print(f"{len(throws)} throws: good {good} ({good / len(throws):.0%}), over {over}, "
              f"under {len(throws) - good - over}; median {statistics.median(t['elapsed'] for t in throws):.1f} s; "
              f"settled good {settled_ok} of {len(settled_good)}")
    print(f"kept in {folder}")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
