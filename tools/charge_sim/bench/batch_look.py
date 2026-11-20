"""A batch (`protocol.py`) summed up: every look at its recordings, one after another, as plain figures.

Usage:
    python batch_look.py <logs/adr009/..._batch_<name>> [--out <file>]
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
# (heading, the look, the batch's steps whose folders it reads, further arguments); a look named with "logs" takes
# the logs folder first and the folders by --folders
LOOKS = [
    ("the tubes being primed, and every trial's true turns against its powder", "turns_to_weight.py logs",
     ["fine_moves", "fine_stops_low", "fine_stops_mid", "like_throw", "slow", "steady_a", "steady_b", "steady_c",
      "steady_d", "steps", "coarse_steady_a", "coarse_steady_b", "coarse_steady_c", "coarse_moves"], []),
    ("inside steady runs, the fine tube", "within_run.py logs",
     ["steady_a", "steady_b", "steady_c", "steady_d"], ["--tube", "fine"]),
    ("inside steady runs, the coarse tube", "within_run.py logs",
     ["coarse_steady_a", "coarse_steady_b", "coarse_steady_c"], ["--tube", "coarse"]),
    ("fixed moves", "moves_look.py", ["fine_moves", "coarse_moves"], []),
    ("a move's powder against the tube's angle in its turn", "phase_look.py", ["fine_moves"], []),
    ("stops from a steady turn, plain and with the reverse quarter turn", "after_stop.py",
     ["fine_stops_low", "fine_stops_mid", "reverse_stops"], []),
    ("the fine tube slowed down, then stopped", "ramp_look.py", ["like_throw", "slow"], []),
    ("speed steps with no stop between", "steps_look.py", ["steps"], []),
    ("forward, back, forward again; and a minute at a net 0.05 rev/s", "reverse_look.py",
     ["reverse_lengths", "swings"], []),
    ("watched nudges", "nudge_watch.py", ["nudges"], []),
]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("batch", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    batch = json.loads((args.batch / "batch.json").read_text(encoding="utf-8"))
    logs = args.batch.parent
    folders = {step["test"]: step["folder"] for step in batch["steps"] if step["folder"] and step["passed"]}
    lines = [f"Batch {batch['name']}, {batch['powder']}, board {batch['board']}, started {batch['started']}.",
             "Raw folders under the checkout's logs/adr009 (not tracked); each block is made by the script named.", ""]
    for step in batch["steps"]:
        lines.append(f"  step {step['step']:2d} {step['test']:16s} {'PASS' if step['passed'] else 'FAIL'}  "
                     f"{step['runs']:4d} runs  {step['folder']}  {step['cup_gr_after']:.1f} gr in the cup after")
    for heading, look, tests, more in LOOKS:
        names = [folders[test] for test in tests if test in folders]
        if not names:
            continue
        script, *kind = look.split()
        if kind:
            command = [sys.executable, str(HERE / script), str(logs), "--folders", *names, *more]
        else:
            command = [sys.executable, str(HERE / script), *[str(logs / name) for name in names], *more]
        done = subprocess.run(command, capture_output=True, text=True, check=False)
        lines += ["", f"== {script}: {heading}", (done.stdout + done.stderr).rstrip()]
    text = "\n".join(lines) + "\n"
    print(text)
    if args.out:
        args.out.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
