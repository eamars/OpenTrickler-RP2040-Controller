"""Sum up flick-test output lines (`<kind> <gain> <steps> <ms>`): kernels per trial by kind."""
import sys
from collections import defaultdict

KERNEL_GR = 0.021
kinds = defaultdict(list)
for path in sys.argv[1:]:
    for line in open(path, encoding="utf-8"):
        parts = line.split()
        if len(parts) == 4 and parts[0] in ("nudge", "shake", "shake_wide", "retract"):
            kinds[parts[0]].append(float(parts[1]))
for kind, gains in kinds.items():
    kernels = [max(0, round(g / KERNEL_GR)) for g in gains]
    released = [k for k in kernels if k > 0]
    mean = sum(kernels) / len(kernels)
    fano = (sum(k * k for k in kernels) / len(kernels) - mean * mean) / mean if mean else float("nan")
    print(f"{kind:11s} trials {len(kernels):3d}  gr/trial {sum(gains) / len(gains):.4f}  kernels/trial {mean:.2f}  "
          f"released in {len(released):2d}  of those: one {sum(k == 1 for k in released)}, two {sum(k == 2 for k in released)}, "
          f"three or more {sum(k >= 3 for k in released)}  largest {max(kernels)}  fano {fano:.2f}")
    print("            " + "".join(str(min(k, 9)) for k in kernels))
