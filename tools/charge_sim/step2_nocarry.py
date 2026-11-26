"""The no-carry gap on the twin: step 2's charge on the bench unit's own AR2209 profile and step 1 records (2026-11-26).

Each throw: its time, its true error, the weight a turn it took (none, own, carried), whether it reached the coarse
tube's quiet before its stop, and the carry after it. Then the same from the tuner's side: windows from this baseline.
"""
import dataclasses
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import rigs  # noqa: E402
import step2  # noqa: E402
import step2_tuner  # noqa: E402
from legacy_pid import Profile  # noqa: E402
from throw import Session  # noqa: E402

# The bench's Pico 2 W, its stored AR2209 profile and charge settings, read over its API on 2026-11-26
BENCH = Profile(coarse_kp=0.02, coarse_ki=0.0, coarse_kd=0.3, coarse_min_rps=0.16, coarse_max_rps=5.0,
                fine_kp=2.0, fine_ki=0.0, fine_kd=10.0, fine_min_rps=0.0336, fine_max_rps=5.0,
                coarse_stop_threshold=5.0, fine_stop_threshold=0.03, backoff_enable=True, backoff_turns=0.25,
                backoff_speed_rps=1.0)
# Its step 1 records, as stored (settings/scale and the profile's least speeds)
RECORDS = step2.Records(settle_s=0.570066,
                        lag=((0.24, 0.835538), (0.345, 0.9944383), (0.8467, 0.6285708), (2.8833, 0.6469072),
                             (10.4, 0.5888882)),
                        fine_least_rps=0.0336, coarse_least_rps=0.16)


def throws(rig_name, profile, n, target, seed=11):
    rig = rigs.get_rig(rig_name)
    session = Session(rig, seed)
    c = step2.Step2(profile, RECORDS, rig.acceleration_rps2, before="last")
    out = []
    for i in range(n):
        k_at_start = c.carried()
        result = step2.throw_one(session, c, target, profile.fine_stop_threshold)
        reached_quiet = bool(c.rows)
        out.append((i + 1, result.elapsed_s, result.true_error, k_at_start, c.own[0] if c.own else None,
                    reached_quiet, c.carried(), c.quiet_at))
    return out


def main():
    for rig_name in ("pico2w_ar2209_night", "pico2w_ar2209_f2"):
        for target in (10.0, 30.0):
            print(f"{rig_name}, {target:g} gr, the bench's profile (fine kp 2, top 5 rev/s) and records:")
            for n, t, e, k0, own, quiet, k1, q in throws(rig_name, BENCH, 8, target):
                print(f"  throw {n}: {t:5.2f} s  {e:+.3f} gr  k at start {k0 if k0 is None else round(k0, 4)}  "
                      f"own {own if own is None else round(own, 4)}  quiet reached {quiet}  carried after "
                      f"{k1 if k1 is None else round(k1, 4)}")


if __name__ == "__main__":
    main()
