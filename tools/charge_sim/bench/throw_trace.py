"""A recorded throw's trace: the reading and both speeds every 0.25 s, and the moments each tube stopped."""
import json
import sys
from pathlib import Path

runs = [r for r in json.loads((Path(sys.argv[1]) / "runs.json").read_text(encoding="utf-8")) if r["note"] == "throw"]
which = [int(a) for a in sys.argv[2:]] or range(len(runs))
for index in which:
    throw = runs[index]
    target = throw["target"]
    samples = throw["samples"]
    first = samples[0][0]
    print(f"\nthrow {index + 1}: target {target}, {len(samples)} samples; time from the charge's start, "
          f"reading less target, coarse rev/s, fine rev/s")
    last_print = None
    for t, charge, weight, coarse, fine in samples:
        since = t - first
        if last_print is None or since - last_print >= 0.25:
            print(f"  {since:5.2f} s state {charge} {weight - target:+8.2f} coarse {coarse:5.2f} fine {fine:5.2f}")
            last_print = since
    coarse_off = next((s for i, s in enumerate(samples) if s[3] == 0.0 and i and samples[i - 1][3] > 0.0), None)
    fine_off = next((s for i, s in enumerate(samples) if s[4] == 0.0 and i and samples[i - 1][4] > 0.0), None)
    top_coarse = max(s[3] for s in samples)
    top_fine = max(s[4] for s in samples)
    print(f"  top speeds: coarse {top_coarse:.2f}, fine {top_fine:.2f} rev/s")
    if coarse_off:
        print(f"  coarse stopped at {coarse_off[0] - first:.2f} s, reading {coarse_off[2] - target:+.2f}")
    if fine_off:
        print(f"  fine stopped at {fine_off[0] - first:.2f} s, reading {fine_off[2] - target:+.2f}")
