"""One powder's whole capture on a unit, as one batch.

The fixed list of open-loop recordings ADR-009's simulation is made from, run one after another by `record.py`,
each its own session with its own verdict: both tubes primed first, then the fine tube (fixed moves, stops from
every speed, a real finish's speeds with no controller, slowing down, steady runs, speed steps, backward, back
and forth, watched nudges), then the coarse tube. No setting of the unit is changed and no charge is run.

Before each recording the cup's room is checked against what the recording is expected to pour. When it does not
fit, or a recording stops itself at the recorder's own limit, the batch stops and says which step to go on from
once the cup is emptied. A recording that does not pass stops the batch.

The bench rules hold: the owner's word that the bench is ready, the powder loaded, an empty cup on the pan.

Two things learned on 2026-11-20 with a second powder:
- **The two priming steps are a first fill, not proof.** A tube emptied for another powder gave nothing for four
  turns and was not steady by fifteen (the coarse tube not by four). Prime until two runs of ten turns pour the
  same (`record.py --label prime ramp --plans 1:10 --precondition-s 0`), then begin at step 3; and prime the coarse
  tube again just before its steps (17 on), since a tube that has stood still starts low.
- **A batch stopped from outside leaves the tube at its last speed.** Stop both motors at once:
  `rc_run.py send "motor fine 0" "motor coarse 0"`.

Usage, with the checkout's venv:
    python protocol.py --repo C:\\workspace\\OpenTrickler-RP2040-Controller --name bm2_pico2w --powder "ADI BM2"
    python protocol.py --repo ... --name bm2_pico2w --powder "ADI BM2" --from 13 --cup-gr 0
    python protocol.py --repo ... --name x --powder x --list
"""

import argparse
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path

RECORD = Path(__file__).parent / "record.py"

# The fine tube slowed from 1.5 to 0.03 rev/s in fifteen equal ratios of 0.2 s, and the same held at 0.03 first
SLOW = [1.5 * (0.03 / 1.5) ** (index / 14) for index in range(15)]
SLOW_PLAN = ",".join(f"{speed:.3g}:0.2" for speed in SLOW)
SLOW_HOLD_PLAN = ",".join(f"{speed:.3g}:0.2" for speed in SLOW[:-1]) + ",0.03:3.2"
# The speeds of the recorded slow-finish throws' last three seconds (2026-11-20)
LIKE_THROW_PLAN = "1.0:0.5,0.75:0.5,0.5:0.5,0.31:0.5,0.22:0.5,0.105:0.3,0.05:0.2"
STEPS_PLAN = "0.2:6,0.05:8,0.2:6,1:3,0.2:6,0.05:8,1:3,0.05:8,0.2:6"
BACK_PLANS = ";".join(f"0.2:3,-0.2:{turn / 0.2:g},0.2:6" for turn in (0.05, 0.1, 0.25, 0.5))
SWING_PLANS = ";".join(["0.05:60", ",".join(["0.25:0.1,-0.15:0.1"] * 250), ",".join(["0.45:0.1,-0.35:0.1"] * 250)])
# Fixed moves from rest to rest: (speed, turn)
FINE_MOVES = ([(0.05, turn) for turn in (0.03, 0.06, 0.12, 0.25)] + [(0.2, turn) for turn in (0.03, 0.06, 0.12, 0.25, 0.5)]
              + [(1.0, turn) for turn in (0.06, 0.12, 0.25, 0.5)])
COARSE_MOVES = [(0.5, 0.25), (0.5, 0.5), (2.0, 0.25), (2.0, 0.5)]


def moves(cells: list) -> list:
    """The ramp test's arguments for fixed moves: one plan a (speed, turn), named by both."""
    return ["--plans", ";".join(f"{speed:g}:{turn / speed:.3f}" for speed, turn in cells),
            "--names", ",".join(f"v{speed:g}_a{turn:g}" for speed, turn in cells)]


# (name, what it records, grains it is expected to pour with a powder like AR2209, record.py's arguments)
STEPS = [
    ("prime_fine", "the fine tube 15 turns, to fill it with this powder", 12,
     ["--label", "prime", "ramp", "--tube", "fine", "--plans", "1:15", "--names", "prime", "--count", "1",
      "--precondition-s", "0", "--still-s", "5"]),
    ("prime_coarse", "the coarse tube 4 turns, to fill it with this powder", 40,
     ["--label", "prime", "ramp", "--tube", "coarse", "--plans", "1:4", "--names", "prime", "--count", "1",
      "--precondition-s", "0", "--still-s", "5"]),
    ("fine_moves", "fine tube, fixed moves of 0.03 to 0.5 turn at 0.05, 0.2 and 1 rev/s", 45,
     ["ramp", "--tube", "fine", *moves(FINE_MOVES), "--count", "24", "--still-s", "4"]),
    ("fine_stops_low", "fine tube, stops from 0.02, 0.05 and 0.1 rev/s", 15,
     ["creepstop", "--tube", "fine", "--speeds", "0.02,0.05,0.1", "--count", "20", "--stops", "held", "--run-s", "3:6",
      "--still-s", "3"]),
    ("fine_stops_mid", "fine tube, stops from 0.2, 0.5 and 1 rev/s", 50,
     ["creepstop", "--tube", "fine", "--speeds", "0.2,0.5,1", "--count", "12", "--stops", "held", "--run-s", "2:4",
      "--still-s", "3"]),
    ("like_throw", "fine tube through a real finish's speeds with no controller, stopped", 35,
     ["ramp", "--tube", "fine", "--plans", LIKE_THROW_PLAN, "--names", "like_throw", "--count", "30", "--still-s", "4"]),
    ("slow", "fine tube slowed from 1.5 to 0.03 rev/s, stopped; and held at 0.03 first", 32,
     ["ramp", "--tube", "fine", "--plans", f"{SLOW_PLAN};{SLOW_HOLD_PLAN}", "--names", "slow,slow_hold", "--count", "15",
      "--still-s", "4"]),
    ("steady_a", "fine tube steady 90 s at 0.02 and 0.05 rev/s", 7,
     ["creepstop", "--tube", "fine", "--speeds", "0.02,0.05", "--count", "1", "--stops", "held", "--run-s", "90:90",
      "--still-s", "4"]),
    ("steady_b", "fine tube steady 60 s at 0.1 and 0.2 rev/s", 16,
     ["creepstop", "--tube", "fine", "--speeds", "0.1,0.2", "--count", "1", "--stops", "held", "--run-s", "60:60",
      "--still-s", "4"]),
    ("steady_c", "fine tube steady 30 s at 0.5 rev/s", 14,
     ["creepstop", "--tube", "fine", "--speeds", "0.5", "--count", "1", "--stops", "held", "--run-s", "30:30",
      "--still-s", "4"]),
    ("steady_d", "fine tube steady 20 s at 1 rev/s", 18,
     ["creepstop", "--tube", "fine", "--speeds", "1", "--count", "1", "--stops", "held", "--run-s", "20:20",
      "--still-s", "4"]),
    ("steps", "fine tube stepped between 0.05, 0.2 and 1 rev/s with no stop", 28,
     ["ramp", "--tube", "fine", "--plans", STEPS_PLAN, "--names", "steps", "--count", "3", "--still-s", "4"]),
    ("reverse_stops", "fine tube stopped plainly and with a reverse quarter turn, from 0.05, 0.1 and 0.3 rev/s", 30,
     ["creepstop", "--tube", "fine", "--speeds", "0.05,0.1,0.3", "--count", "12", "--stops", "held,backoff",
      "--run-s", "3:5", "--still-s", "4"]),
    ("reverse_lengths", "fine tube forward, back by 0.05 to 0.5 turn, forward again", 30,
     ["ramp", "--tube", "fine", "--plans", BACK_PLANS, "--names", "back005,back010,back025,back050", "--count", "6",
      "--still-s", "4"]),
    ("swings", "fine tube a minute at a net 0.05 rev/s: steady, small swings, wide swings", 8,
     ["ramp", "--tube", "fine", "--plans", SWING_PLANS, "--names", "steady005,swing_small,swing_wide", "--count", "2",
      "--still-s", "4"]),
    ("nudges", "240 nudges of the fine tube, the reading watched 5 s after each", 5,
     ["--label", "nudgewatch", "creepstop", "--tube", "fine", "--speeds", "0.2", "--count", "240", "--stops", "held",
      "--run-s", "0.05:0.06", "--still-s", "5"]),
    ("coarse_steady_a", "coarse tube steady 20 s at 0.1 rev/s", 30,
     ["creepstop", "--tube", "coarse", "--speeds", "0.1", "--count", "1", "--stops", "held", "--run-s", "20:20",
      "--still-s", "4", "--precondition-s", "1"]),
    ("coarse_steady_b", "coarse tube steady 12 s at 0.3 rev/s", 36,
     ["creepstop", "--tube", "coarse", "--speeds", "0.3", "--count", "1", "--stops", "held", "--run-s", "12:12",
      "--still-s", "4", "--precondition-s", "0"]),
    ("coarse_steady_c", "coarse tube steady 6 s at 1 rev/s", 60,
     ["creepstop", "--tube", "coarse", "--speeds", "1", "--count", "1", "--stops", "held", "--run-s", "6:6",
      "--still-s", "4", "--precondition-s", "0"]),
    ("coarse_moves", "coarse tube, fixed moves of 0.25 and 0.5 turn at 0.5 and 2 rev/s", 75,
     ["ramp", "--tube", "coarse", *moves(COARSE_MOVES), "--count", "5", "--still-s", "4", "--precondition-s", "0"]),
]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", required=True, type=Path, help="the checkout whose rc_run.py and build match the unit")
    parser.add_argument("--board", default="pico2_w")
    parser.add_argument("--name", required=True, help="the batch's name: the unit and the powder, as a record's")
    parser.add_argument("--powder", required=True, help="the powder in the hopper, kept with the batch")
    parser.add_argument("--from", dest="first", type=int, default=1, help="the step to begin at, counted from 1")
    parser.add_argument("--to", dest="last", type=int, default=len(STEPS), help="the last step to run")
    parser.add_argument("--cup-gr", type=float, default=0.0, help="what is in the cup at the start")
    parser.add_argument("--cup-holds", type=float, default=680.0, help="the most the cup may hold")
    parser.add_argument("--list", action="store_true", help="print the steps and what they pour, and run nothing")
    args = parser.parse_args()
    if args.list:
        for index, (name, what, grains, _) in enumerate(STEPS, start=1):
            print(f"{index:2d} {name:16s} {grains:4d} gr  {what}")
        print(f"   {'in all':16s} {sum(step[2] for step in STEPS):4d} gr")
        return

    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    folder = args.repo / "logs" / "adr009" / f"{stamp}_batch_{args.name}"
    folder.mkdir(parents=True)
    batch = {"name": args.name, "powder": args.powder, "board": args.board, "started": stamp, "steps": []}
    cup = args.cup_gr
    for index, (name, what, grains, arguments) in enumerate(STEPS, start=1):
        if index < args.first or index > args.last:
            continue
        if cup + grains > args.cup_holds:
            print(f"STOPPED before step {index} ({name}): {cup:.0f} gr in the cup and about {grains} more to pour. "
                  f"Empty the cup, then go on with --from {index} --cup-gr 0", flush=True)
            break
        print(f"step {index} of {len(STEPS)}: {name}: {what}", flush=True)
        done = subprocess.run([sys.executable, str(RECORD), "--repo", str(args.repo), "--board", args.board, *arguments],
                              capture_output=True, text=True, check=False)
        output = done.stdout + done.stderr
        (folder / f"{index:02d}_{name}.log").write_text(output, encoding="utf-8")
        saved = re.search(r"saved \d+ frames and (\d+) runs to (.+)", output)
        end = re.findall(r"weight at the end (-?[\d.]+)", output)
        passed = "VERDICT PASS" in output
        cup = float(end[-1]) if end else cup + grains
        batch["steps"].append({"step": index, "test": name, "what": what, "passed": passed,
                               "folder": Path(saved.group(2).strip()).name if saved else None,
                               "runs": int(saved.group(1)) if saved else 0, "cup_gr_after": cup})
        (folder / "batch.json").write_text(json.dumps(batch, indent=1), encoding="utf-8")
        print(f"   {'PASS' if passed else 'FAIL'}; {batch['steps'][-1]['runs']} runs in {batch['steps'][-1]['folder']}; "
              f"{cup:.1f} gr in the cup", flush=True)
        if not passed:
            print(f"STOPPED: step {index} ({name}) did not pass; its log is {folder / f'{index:02d}_{name}.log'}", flush=True)
            break
        if "the cup is near full" in output:
            print(f"STOPPED after step {index} ({name}): the recorder's limit was reached during it. Empty the cup, "
                  f"then go on with --from {index} --cup-gr 0 to make that step again", flush=True)
            break
    else:
        print(f"the batch is complete: {folder}", flush=True)


if __name__ == "__main__":
    main()
