"""The firmware's charge controller against its reference (ADR-009, gate 1).

For each record, finish and band, the calibration is made once by `staged.calibrate` on the simulated rig, and:

1. **The rules:** the C rules (the unit's float) made from its measured part are `staged.set_rules`' to a part in
   ten thousand.
2. **Lockstep, the port's logic:** each throw is thrown by `staged.Staged` against the simulated rig, and the
   firmware's controller built in double (`CHARGE_CONTROL_DOUBLE`, the reference's precision) is given the very same
   calls beside it: the same start, ticks and readings, in whole milliseconds. Their decisions (a change of phase, a
   move and its turns, a tube stopped, the end) must come in the same order, each within 3 ms of the other's; their
   speeds, while in the same phase, within what a twentieth of a digit of room asks; their first errors at rest the
   same. A throw's port starts from the reference's learned stop, so each throw is judged on its own. Pass: at least
   99 throws in 100 with no difference. (In float a threshold is met a millisecond apart now and then and a parcel
   due on a reading's millisecond falls the other way, which changes nothing that matters and everything that
   follows in a random powder: so float is judged by its figures.)
3. **The figures, the unit's float:** the reference and the float build thrown apart, each in a session of its own
   as a unit throws, judged at the band: the share of good throws within three standard errors of each other.
4. **The firmware's own path:** the float build given each tube's turns by count every millisecond, as the unit's
   step counter gives them (the reference reckons them from its speeds), judged by its figures as in 3.

Once: the bench trial's kept calibration (`recordings/20261121_162403_trial_pico2w_ar2209`) made into rules by
the C, as `staged.set_rules` makes them, for both finishes and bands.

Usage, with the repository's venv, from this folder:
    python check_firmware_controller.py                  200 throws a set, at the desk
    python check_firmware_controller.py --throws 40 --logic-only
                                                         CI's run: the rules and the lockstep judged, which are
                                                         exact; the figures are printed and not judged, since a
                                                         harmless change may move them by chance at 40 throws
"""

import argparse
import dataclasses
import math
import sys
import time

import json
from pathlib import Path

import firmware_controller
import rigs
import staged
from controllers import LOOSE_TOLERANCE_GR, VERDICT_TOLERANCE_GR
from throw import Session
from workers import Workers

RECORDS = ("pico2w_ar2209", "pico2w_bm2")
FINISHES = ("glide", "jog")
BANDS = ((0.02, VERDICT_TOLERANCE_GR), (0.04, LOOSE_TOLERANCE_GR))
TARGET_GR = 20.0
SEED = 4242
LOCKSTEP_SHARE = 0.99
RULES_SHARE = 1e-4
ROOM_GR = 1e-3                  # speeds agree within what this much room asks: a twentieth of a digit
EVENT_MS = 3.0
STANDARD_ERRORS = 3.0
KEPT_TRIAL = Path(__file__).resolve().parent / "recordings" / "20261121_162403_trial_pico2w_ar2209"


class Counted:
    """A controller given each tube's turns by count before every tick, as the unit's step counter gives them."""

    def __init__(self, controller, session: Session):
        self.controller = controller
        self.session = session
        self.before = None

    def __getattr__(self, name):
        return getattr(self.controller, name)

    def start(self, target_gr: float, now_ms: float) -> None:
        self.before = {"coarse": self.session.coarse.turns, "fine": self.session.fine.turns}
        self.controller.start(target_gr, now_ms)

    def on_tick(self, now_ms: float) -> None:
        self.controller.count(now_ms, {"coarse": self.session.coarse.turns - self.before["coarse"],
                                       "fine": self.session.fine.turns - self.before["fine"]})
        self.controller.on_tick(now_ms)

    def on_reading(self, now_ms: float, weight: float) -> None:
        self.controller.on_reading(now_ms, weight)


class Lockstep:
    """The reference drives the rig; the port gets the same calls. Each one's decisions are kept as events.

    An event is a change of phase, a small move asked for (with its turns), a tube stopped, or the end. The two
    sequences must be the same, each event within EVENT_MS of the other's: float against double meets a threshold a
    millisecond or two apart now and then, and the port, which does not drive the rig, comes back in step at the
    next decision. While the two are in the same phase their speeds must agree within what ROOM_GR of room asks.
    """

    def __init__(self, reference: staged.Staged, port: firmware_controller.FirmwareStaged):
        self.reference = reference
        self.port = port
        cal = reference.cal
        self.tolerance = {"coarse": ROOM_GR / (cal.coarse_gr_turn * cal.coarse_glide_s),
                          "fine": ROOM_GR / (cal.fine_gr_turn * cal.glide_s)}
        self.events = {"reference": [], "port": []}
        self.last = {"reference": None, "port": None}
        self.speed_off = None
        self.now_ms = 0.0

    @property
    def coarse_rps(self) -> float:
        return self.reference.coarse_rps

    @property
    def fine_rps(self) -> float:
        return self.reference.fine_rps

    @property
    def done(self) -> bool:
        return self.reference.done

    def _note(self, name: str, controller) -> None:
        state = (controller.phase, controller.move_turns is not None, controller.coarse_rps > 0.0,
                 controller.fine_rps > 0.0, controller.done)
        last = self.last[name]
        if last is None or state != last:
            detail = round(controller.move_turns, 4) if controller.move_turns is not None else None
            self.events[name].append((self.now_ms, state, detail))
        self.last[name] = state

    def _compare(self) -> None:
        ref, port = self.reference, self.port
        self._note("reference", ref)
        self._note("port", port)
        if self.speed_off is None and self.last["reference"] == self.last["port"]:
            for tube in ("coarse", "fine"):
                a, b = getattr(ref, tube + "_rps"), getattr(port, tube + "_rps")
                if abs(a - b) > self.tolerance[tube]:
                    self.speed_off = (self.now_ms / 1000.0, tube, a, b)

    def difference(self):
        """None when the two agree, else what differed first, in words."""
        ref, port = self.events["reference"], self.events["port"]
        for i in range(max(len(ref), len(port))):
            if i >= len(ref) or i >= len(port):
                extra = (ref if i < len(ref) else port)[i]
                return f"event {i}: only the {'reference' if i < len(ref) else 'port'} has {extra}"
            (t_ref, state_ref, turns_ref), (t_port, state_port, turns_port) = ref[i], port[i]
            if state_ref != state_port or abs(t_ref - t_port) > EVENT_MS or                     (turns_ref is not None and abs(turns_ref - turns_port) > 1e-4):
                return f"event {i}: reference {state_ref} at {t_ref / 1000:.3f} s, port {state_port} at {t_port / 1000:.3f} s"
        if self.speed_off is not None:
            t_s, tube, a, b = self.speed_off
            return f"{tube} speed at {t_s:.3f} s: reference {a:.5f}, port {b:.5f}"
        if (ref and port and self.reference.log and self.port.log
                and abs(self.reference.log[-1][0] - self.port.log[-1][0]) > 1e-6):
            return f"first error: reference {self.reference.log[-1][0]}, port {self.port.log[-1][0]}"
        return None

    def start(self, target_gr: float, now_ms: float) -> None:
        now_ms = self.now_ms = float(round(now_ms))     # both are given the same whole milliseconds, as the unit's
        self.reference.start(target_gr, now_ms)
        self.port.start(target_gr, now_ms)
        self._compare()

    def on_tick(self, now_ms: float) -> None:
        now_ms = self.now_ms = float(round(now_ms))
        self.reference.on_tick(now_ms)
        self.port.on_tick(now_ms)
        self._compare()

    def on_reading(self, now_ms: float, weight: float) -> None:
        now_ms = self.now_ms = float(round(now_ms))
        self.reference.on_reading(now_ms, weight)
        self.port.on_reading(now_ms, weight)
        self._compare()


def good_share(results: list) -> float:
    return sum(1 for result in results if result.verdict == "good") / len(results)


def run_set(job: tuple) -> dict:
    record, finish, band, tolerance, throws, pid = job
    rig = rigs.get_rig(record)
    measured = staged.measure(rig, SEED)
    expected = staged.for_finish(measured, finish, band)
    in_c = firmware_controller.rules_in_c(measured, finish, band)
    rules_off = [(name, in_c[name], getattr(expected, name)) for name in firmware_controller.RULES
                 if abs(in_c[name] - getattr(expected, name)) > RULES_SHARE * max(1e-3, abs(getattr(expected, name)))]
    cal = staged.calibrate(rig, SEED, finish, band)
    law = staged.pid_law_from(cal) if pid else None

    def made_reference():
        controller = staged.Staged(cal, rig.acceleration_rps2)
        controller.pid = law
        return controller

    def made_port(port):
        if law is not None:
            port.set_pid(law)
        return port

    # Lockstep, a throw at a time
    reference = made_reference()
    session = Session(rig, SEED + 1)
    differences = []
    for i in range(throws):
        port = made_port(firmware_controller.FirmwareStaged(dataclasses.replace(cal, stop_room_gr=reference.stop_room),
                                                            rig.acceleration_rps2, double=True))
        # and from its learnt fine figure (2026-11-22)
        port.set_fine_learning(reference.fine_learn_n, reference.fine_learn_sum, reference.cal.fine_gr_turn,
                               reference.stream_learn_n, reference.stream_learn_sum, reference.fine_gr_stream)
        twin = Lockstep(reference, port)
        session.throw(twin, TARGET_GR, tolerance)
        difference = twin.difference()
        if difference is not None:
            differences.append((i, difference))

    # The figures, each thrown apart
    apart = {}
    for name, controller in (("reference", made_reference()),
                             ("port", made_port(firmware_controller.FirmwareStaged(cal, rig.acceleration_rps2)))):
        session_apart = Session(rig, SEED + 2)
        results = [session_apart.throw(controller, TARGET_GR, tolerance) for _ in range(throws)]
        apart[name] = (good_share(results), sorted(r.elapsed_s for r in results)[throws // 2])
    session_counted = Session(rig, SEED + 2)
    counted = Counted(made_port(firmware_controller.FirmwareStaged(cal, rig.acceleration_rps2)), session_counted)
    results = [session_counted.throw(counted, TARGET_GR, tolerance) for _ in range(throws)]
    apart["counted"] = (good_share(results), sorted(r.elapsed_s for r in results)[throws // 2])
    return {"record": record, "finish": finish, "band": band, "throws": throws, "differences": differences,
            "rules_off": rules_off, "apart": apart}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--throws", type=int, default=200)
    parser.add_argument("--logic-only", action="store_true", help="judge the rules and the lockstep only")
    parser.add_argument("--pid", action="store_true", help="the PID speed law in the frame (option 2, "
                        "staged.pid_law_from), in place of the glide's")
    args = parser.parse_args()

    firmware_controller.build()
    firmware_controller.build(double=True)
    jobs = [(record, finish, band, tolerance, args.throws, args.pid) for record in RECORDS for finish in FINISHES
            for band, tolerance in BANDS]
    started = time.time()
    results = Workers().map(run_set, jobs)
    failed = []
    for result in results:
        label = f"{result['record']} {result['finish']} {result['band']:.2f}"
        throws = result["throws"]
        in_step = throws - len(result["differences"])
        (ref_good, ref_s), (port_good, port_s), (count_good, count_s) = (
            result["apart"]["reference"], result["apart"]["port"], result["apart"]["counted"])
        p = (ref_good + port_good) / 2.0
        allowed = STANDARD_ERRORS * math.sqrt(max(p * (1.0 - p), 0.25 / throws) * 2.0 / throws)
        checks = {"rules": not result["rules_off"], "lockstep": in_step >= LOCKSTEP_SHARE * throws,
                  "figures": abs(ref_good - port_good) <= allowed, "counted": abs(ref_good - count_good) <= allowed}
        if args.logic_only:
            checks = {name: good for name, good in checks.items() if name in ("rules", "lockstep")}
        ok = all(checks.values())
        print(f"{label:26} lockstep {in_step}/{throws}; good {ref_good:.0%} / float {port_good:.0%} / counted "
              f"{count_good:.0%} (allowed {allowed:.0%}), median {ref_s:.1f} / {port_s:.1f} / {count_s:.1f} s  "
              f"{'PASS' if ok else 'FAIL ' + ', '.join(name for name, good in checks.items() if not good)}")
        for name, c_value, py_value in result["rules_off"]:
            print(f"    rule {name}: C {c_value:.6g}, Python {py_value:.6g}")
        for i, difference in result["differences"][:5]:
            print(f"    throw {i}: {difference}")
        if not ok:
            failed.append(label)
    kept = staged.from_json(json.loads((KEPT_TRIAL / "calibration.json").read_text()))
    for finish in FINISHES:
        for band, _ in BANDS:
            expected = staged.for_finish(kept, finish, band)
            in_c = firmware_controller.rules_in_c(kept, finish, band)
            off = [name for name in firmware_controller.RULES
                   if abs(in_c[name] - getattr(expected, name)) > RULES_SHARE * max(1e-3, abs(getattr(expected, name)))]
            print(f"the trial's calibration, {finish} {band:.2f}: rules {'agree' if not off else 'differ: ' + ', '.join(off)}")
            if off:
                failed.append(f"kept calibration {finish} {band:.2f}")
    print(f"{time.time() - started:.0f} s")
    print("VERDICT PASS" if not failed else "VERDICT FAIL " + "; ".join(failed))
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
