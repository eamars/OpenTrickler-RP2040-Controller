"""A bench series replayed in the simulation as the unit ran it (ADR-009, gate S).

The simulation's own controllers calibrate their rig. A series on the rig ran on the calibration the unit held, which
the series kept as the unit echoed it (`ccal`: its measured part, its first errors, the rules it made of them), and
on the constants it echoed (`ccon`). So the question "is the rig's model right?" is asked with the unit's own
calibration and constants: the rules made here from the echo are checked against the unit's own echoed rules.

Each series of `recordings/` becomes a controller named `replay:<folder>`, made in each worker from the folder's
`series.json` (this module is imported by every worker of a study that imports it).
"""

import dataclasses
import json
import re
import statistics
from pathlib import Path

import bad_gains_study
import controllers
import staged

HERE = Path(__file__).resolve().parent
RECORDINGS = HERE / "recordings"
RULES_TOLERANCE = 2e-3          # the unit's echoed rules are printed to six digits


def _numbers(line: str) -> dict:
    return {name: float(value) for name, value in re.findall(r"(\w+)=(-?[\d.eE+-]+)", line)}


def unit_calibration(lines: list, finish: str, band_gr: float) -> tuple:
    """The calibration the unit held, from its `ccal` echo: (Calibration with its rules and its stop placed, the
    unit's own rules as it echoed them)."""
    head, stops, moves, errors, rules = {}, {"coarse": [], "fine": []}, [], [], {}
    for line in lines:
        words = line.split()
        if len(words) < 2 or words[0] != "@cal":
            continue
        if words[1] == "head":
            head = _numbers(line)
        elif words[1] == "stop":
            n = _numbers(line)
            stops[words[2]].append(staged.Stop(speed=n["speed"], unseen_gr=n["unseen"], spread_gr=n["spread"],
                                               quiet_s=n["quiet"], lag_s=n["lag"] if n["lag"] >= 0.0 else None))
        elif words[1] == "move":
            n = _numbers(line)
            moves.append(staged.Move(turns=n["turns"], mean_gr=n["mean"], most_gr=n["most"], wait_s=n["wait"]))
        elif words[1] == "errors":
            errors.extend(float(word) for word in words[2:])
        elif words[1] == "rules":
            rules.update(_numbers(line))
    cal = staged.Calibration(digit_gr=head["digit"], interval_s=head["interval"],
                             coarse_gr_turn=head["coarse_gr_turn"], fine_gr_turn=head["fine_gr_turn"],
                             coarse_stops=stops["coarse"], fine_stops=stops["fine"], moves=moves,
                             watch_s=head["watch"], coarse_share=head["coarse_share"])
    if finish != "glide":
        raise ValueError(f"a {finish} series: only the glide's are replayed")
    staged.set_rules(cal, finish, band_gr)
    if errors:
        # The stop placed from the first errors, as the unit's charge_control_place_stop places it
        cal.first_errors = list(errors)
        shift = statistics.median(errors) - cal.aim_gr
        cal.stop_room_gr += shift
        cal.glide_room_gr += shift
        cal.coarse_room_gr += shift
    return cal, rules


def rules_differ(cal, rules: dict) -> list:
    """The rules made here against the unit's echoed ones: the names that differ."""
    pairs = {"floor": cal.floor_rps, "stop_room": cal.stop_room_gr, "aim": cal.aim_gr, "glide_room": cal.glide_room_gr,
             "glide": cal.glide_s, "coarse_room": cal.coarse_room_gr, "coarse_glide": cal.coarse_glide_s,
             "rest": cal.rest_s}
    return [f"{name} {mine:.5f}, the unit's {rules[name]:.5f}" for name, mine in pairs.items()
            if name in rules and abs(mine - rules[name]) > RULES_TOLERANCE]


def constants(lines: list) -> dict:
    """The unit's constants as `ccon` echoed them."""
    out = {}
    for line in lines:
        match = re.match(r"@ccon (\w+)=(\S+)", line)
        if match:
            out[match.group(1)] = float(match.group(2))
    return out


# Series whose calibration echo was not kept, and the series whose echo shows the slot they ran on. The frame's PID
# blocks of 20:13 set their gains after loading the slot, and only those lines were kept. A PID-law throw then carried
# nothing into the slot (its own list came at 20:27), so p1 and p2 ran on the slot as g1 left it: g2's echo
ECHO_FROM = {"20261122_201657_series_frame_p1_pico2w_ar2209": "20261122_202219_series_frame_g2_pico2w_ar2209",
             "20261122_201944_series_frame_p2_pico2w_ar2209": "20261122_202219_series_frame_g2_pico2w_ar2209"}


def series_of(folder: str) -> dict:
    """A series as kept, its calibration's echo taken from another where ECHO_FROM says so."""
    series = json.loads((RECORDINGS / folder / "series.json").read_text(encoding="utf-8"))
    if folder in ECHO_FROM:
        series["ccal"] = json.loads((RECORDINGS / ECHO_FROM[folder] / "series.json").read_text(encoding="utf-8"))["ccal"]
    return series


def pid_law(series: dict, cal) -> staged.PidLaw:
    """The PID law's gains a series' unit ran: the offer, the bench unit's stored profile, or those the series set."""
    name = series["name"]
    if name.startswith("offer"):
        return staged.pid_offer(cal)
    gains = series.get("gains") or {}
    if not gains:
        return bad_gains_study.GAIN_SETS["the bench unit's stored"](cal)
    return staged.PidLaw(coarse_kp=gains["coarse_kp"], coarse_ki=gains["coarse_ki"], coarse_kd=gains["coarse_kd"],
                         coarse_min_rps=gains["coarse_min_speed_rps"], coarse_max_rps=gains["coarse_max_speed_rps"],
                         fine_kp=gains["fine_kp"], fine_ki=gains["fine_ki"], fine_kd=gains["fine_kd"],
                         fine_max_rps=gains["fine_max_speed_rps"])


def make_replay(folder: str):
    """A controller that throws as the series' unit did: its calibration, its constants, its law."""
    def make(rig, seed=0):
        series = series_of(folder)
        k = constants(series.get("ccon", []))
        # The constants the simulation holds at module level, set afresh each time, as this series had them
        staged.FINE_TOP_RPS = k.get("fine_top_rps", staged.FINE_TOP_RPS)
        staged.LEARN_SHARE = k.get("learn_share", staged.LEARN_SHARE)
        staged.LEARN_FINE_FORM = "split" if k.get("learn_fine_split", 1.0) else "one"
        cal, _ = unit_calibration(series["ccal"], series["finish"], series["band_gr"])
        controller = staged.Staged(cal, rig.acceleration_rps2)
        controller.learn_fine_prior = k.get("learn_fine_prior", staged.LEARN_FINE_PRIOR)
        controller.pid_envelope = bool(k.get("pid_envelope", 1.0))
        if k.get("speed_law", 0.0):
            controller.pid = pid_law(series, cal)
        tolerance = controllers.LOOSE_TOLERANCE_GR if series["band_gr"] >= 0.04 else controllers.VERDICT_TOLERANCE_GR
        return controller, tolerance
    return make


def staged_series() -> list:
    """Every series of the staged controller with its calibration echoed: its folder's name."""
    found = []
    for folder in sorted(RECORDINGS.glob("2026112*_series_*")):
        path = folder / "series.json"
        if not path.exists():
            continue
        series = series_of(folder.name)
        if series.get("controller", "staged") == "staged" and series.get("finish") == "glide" and                 any(line.startswith("@cal head") for line in series.get("ccal", [])):
            found.append(folder.name)
    return found


for _folder in staged_series():
    controllers.CONTROLLERS[f"replay:{_folder}"] = make_replay(_folder)


def check_rules() -> list:
    """Each series' rules made here against the unit's: (folder, differences)."""
    out = []
    for folder in staged_series():
        series = series_of(folder)
        cal, rules = unit_calibration(series["ccal"], series["finish"], series["band_gr"])
        out.append((folder, rules_differ(cal, rules)))
    return out


if __name__ == "__main__":
    for _name, _differ in check_rules():
        print(f"{_name}: {'the same as the unit' if not _differ else '; '.join(_differ)}")
