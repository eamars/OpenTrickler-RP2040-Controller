"""The second gap's test on the twin: on a fast profile the fine tube finishes before the coarse tube's powder in the
air has landed. Variant: until the coarse tube's quiet the fine tube runs at its least and does not stop; then it
finishes on the forecast. Seeded with step 1's weight a turn in both arms."""
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import rigs  # noqa: E402
import step2  # noqa: E402
from step2_nocarry import BENCH, RECORDS  # noqa: E402
from step2_nocarry_seed import summary  # noqa: E402
from throw import Session  # noqa: E402
from workers import Workers  # noqa: E402

SEEDS = {"pico2w_ar2209_night": 0.787, "pico2w_ar2209_f2": 0.807, "pico2w_bm2": 0.520}
THROWS = 50


class WaitsForQuiet(step2.Step2):
    """The fine tube held at its least, and no stop, until the coarse tube's powder is quiet."""

    def before_quiet(self, now_s):
        return self.coarse_moves or self.quiet_at is None or now_s < self.quiet_at

    def on_tick(self, now_ms):
        if not self.done and self.before_quiet(now_ms / 1000.0) and not self.coarse_moves:
            # no stop yet: run the tick's counting, then undo a stop it made
            fine = self.fine_rps
            super().on_tick(now_ms)
            if self.done:
                self.done = False
                self.fine_rps = self.records.fine_least_rps
                self.coarse_rps = 0.0
            return
        super().on_tick(now_ms)

    def on_reading(self, now_ms, weight):
        super().on_reading(now_ms, weight)
        if not self.coarse_moves and self.before_quiet(now_ms / 1000.0):
            if self.done:
                self.done = False
            self.fine_rps = self.records.fine_least_rps


def run(job):
    rig_name, target, variant = job
    rig = rigs.get_rig(rig_name)
    session = Session(rig, 31)
    cls = WaitsForQuiet if variant == "quiet" else step2.Step2
    c = cls(BENCH, RECORDS, rig.acceleration_rps2, before="last")
    c.history = [SEEDS[rig_name]]
    rows = []
    for _ in range(THROWS):
        had = c.carried() is not None
        result = step2.throw_one(session, c, target, BENCH.fine_stop_threshold)
        rows.append((result.true_error, result.elapsed_s, had, bool(c.rows)))
    return job, rows


def main():
    jobs = [(rig, 10.0, v) for rig in SEEDS for v in ("seeded", "quiet")]
    with Workers() as pool:
        for (rig, target, variant), rows in pool.map(run, jobs):
            print(f"{rig:22s} {target:4.0f} gr  {variant:7s} {summary(rows, BENCH.fine_stop_threshold)}")


if __name__ == "__main__":
    main()
