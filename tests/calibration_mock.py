"""ADR-009 step 1's calibration on the stand-in (`web_test.py`): the unit's API as calibration.c answers it.

The firmware's job (gate 3) is played by the stand-in's clock, so the web's pages are designed and checked on it
with no unit (`web-ui-design`), in the 12864's words (`tools/ui_mock/calibration_12864.py`). Its replies have the
unit's shape (api_v1.md, Calibration):

- `GET /api/v1/calibration`: `state`, `part`, `step`, `by`, `progress`, `line`, `weight`, `pouring`, `pour_turns`,
  `capacity`, `cup_weight`, `result`, `failure`, and `records` (`cup`, `scale`, `powder`);
- `POST /api/v1/calibration/start` `{"part": "cup"|"scale"|"powder"}`: a part started alone (`"mock": "fail"`, the
  stand-in's own, makes a powder's fail, to show it);
- `POST /api/v1/calibration/answer` `{"answer": "next"|"full"|"keep"|"done"|"go_on"}`;
- `POST /api/v1/calibration/pour` `{"turns"}`: the coarse tube turned at least these turns from now, never added to;
- `POST /api/v1/calibration/save`; `POST /api/v1/calibration/stop`.

The figures are the bench's of 2026-11-25 (ADR-009's progress).
"""

PATH = "/api/v1/calibration"
ZERO_S = 1.5                    # the scale zeroed with the empty cup, as the mock plays it
LIFT_S = 3.0                    # the cup lifted and put back
SCALE_S = 24.0                  # a scale's part (the bench's took about 2 minutes)
POWDER_S = 30.0
POWDER_PAUSE_AT = 0.6           # the powder's part pauses here for a full cup
PER_TURN = {"fine": 0.8, "coarse": 10.0}
FILL_RPS = 2.0                  # the coarse tube while it pours the fill (calibration.c)
POUR_MOST_TURNS = 0.5           # a pour asks at most this (CALIBRATION_POUR_MOST_TURNS)
CUP_WEIGHT = 77.3
SCALE_RESULT = {"settle_ms": 270.0, "lag": [{"flow_per_s": 0.19, "ms": 920.0}, {"flow_per_s": 0.39, "ms": 870.0},
                                            {"flow_per_s": 0.77, "ms": 640.0}, {"flow_per_s": 2.9, "ms": 600.0},
                                            {"flow_per_s": 9.7, "ms": 570.0}],
                "step": 0.01, "rest_sd": 0.004, "margin": 0.01, "elapsed_s": 110.0, "poured": 58.0, "failure": None}
POWDER_RESULT = {"fine_min_speed_rps": 0.02, "coarse_min_speed_rps": 0.16, "fine_weight_per_turn": 0.787,
                 "elapsed_s": 300.0, "poured": 60.0,
                 "failure": None}
ANSWERS = {"cup_on": "next", "fill": "full", "keep": "keep", "empty": "done", "failed": "done", "paused": "go_on"}


def error(code, message):
    return {"error": {"code": code, "message": message}}


class CalibrationMock:
    """The calibration's records and its one job, moved on by the stand-in's clock, answered as the unit answers."""

    def __init__(self, now_ms, selected, powder, store):
        """
        Args:
            now_ms: the stand-in's clock, in milliseconds.
            selected: the index of the powder the charge uses, read at each call.
            powder: a powder's (name, its fine and coarse least speeds) by its index; None for no such powder.
            store: a powder's least speeds stored into its record, by its index (fine, coarse): None when kept, else
                the setting's own refusal, and nothing is kept.
        """
        self.now_ms = now_ms
        self.selected = selected
        self.powder = powder
        self.store = store
        self.cup = {"capacity": None, "weight": None}
        self.scale = {"settle_ms": None, "lag": []}
        self.job = None
        self.was = {}

    def reply(self):
        """GET's reply: the job's state and step, what it found, and what is kept."""
        self.move_on()
        index = self.selected()
        name, fine, coarse = self.powder(index)
        job = self.job or {}
        profile = job.get("profile")
        return {"state": "running" if self.job else "idle", "part": job.get("part"), "profile": profile,
                "profile_name": self.powder(profile)[0] if profile is not None else None, "step": job.get("step"),
                "by": "api" if self.job else None, "progress": job.get("progress", 0), "line": job.get("line", ""),
                "weight": round(job.get("weight", 0.0), 2), "pouring": job.get("pour_turns", 0.0) != 0.0,
                "pour_turns": round(job.get("pour_turns", 0.0), 3), "capacity": job.get("capacity"),
                "cup_weight": job.get("cup_weight"),
                "result": job.get("result") if job.get("step") == "result" else None,
                "failure": job.get("failure") if job.get("step") == "failed" else None,
                "records": {"cup": dict(self.cup), "scale": {"settle_ms": self.scale["settle_ms"],
                                                             "lag": list(self.scale["lag"])},
                            "powder": {"profile": index, "name": name, "fine_min_speed_rps": fine,
                                       "coarse_min_speed_rps": coarse}}}

    def move_on(self):
        """What the clock does to the job: the zero, a lift seen, a pour made, a part's progress."""
        job = self.job
        if job is None:
            return
        elapsed = (self.now_ms() - job["since_ms"]) / 1000.0
        if job["step"] == "zeroing" and elapsed >= ZERO_S:
            if job["part"] == "cup":
                self.step("lift", line="Lift the cup")
            else:
                self.step("running", progress=0, line="Starting")
        elif job["step"] == "lift" and elapsed >= LIFT_S:
            self.step("back", weight=-CUP_WEIGHT, cup_weight=CUP_WEIGHT, line="Put the cup back")
        elif job["step"] == "back" and elapsed >= LIFT_S:
            self.step("fill", weight=0.0, pour_turns=0.0, line="Rotate to dispense")
        elif job["step"] == "fill":
            step = FILL_RPS * (elapsed - job.get("poured_s", 0.0))
            if job["pour_turns"] > 0:
                turned = min(job["pour_turns"], step)
                job["pour_turns"] -= turned
                job["weight"] += turned * PER_TURN["coarse"]
            elif job["pour_turns"] < 0:
                job["pour_turns"] = min(0.0, job["pour_turns"] + step)      # back: nothing leaves the cup
            job["poured_s"] = elapsed
        elif job["step"] == "running":
            length = SCALE_S if job["part"] == "scale" else POWDER_S
            progress = min(100, int(job["done"] + elapsed / length * 100))
            job["progress"] = progress
            if job["part"] == "scale":
                job["line"] = "Bursts" if progress < 25 else "The scale's lag"
            else:
                speed = 0.01 * (22.4 ** (progress / 100.0))
                job["line"] = f"{'Fine' if progress < 55 else 'Coarse'} tube {speed:.2f} rev/s"
                if job.get("fails") and progress >= 30:
                    self.step("failed", failure="Fine tube: no powder")
                elif progress >= POWDER_PAUSE_AT * 100 and not job.get("paused_once"):
                    job["paused_once"] = True
                    capacity = self.cup["capacity"] or 100.0
                    self.step("paused", line=f"It holds {capacity - 2:.0f} of {capacity:.0f}")
            if job["step"] == "running" and progress >= 100:
                self.step("result", result=dict(SCALE_RESULT) if job["part"] == "scale" else
                          dict(POWDER_RESULT, was=dict(self.was)))

    def step(self, name, **values):
        """The job moved to a step, its time from now."""
        done = self.job.get("progress", 0) if name == "running" else 0
        keep = {key: self.job[key] for key in ("part", "profile", "fails", "paused_once", "progress", "capacity",
                                               "cup_weight") if key in self.job}
        self.job = dict(keep, step=name, since_ms=self.now_ms(), done=done)
        self.job.update(values)

    def get(self, path):
        if path != PATH:
            return 404, error("not_found", "no such path on this build")
        return 200, self.reply()

    def write(self, method, path, body):
        """A start, an answer, a pour of the fill, a save or a stop, refused as the unit refuses them."""
        self.move_on()
        if method == "POST" and path == f"{PATH}/start":
            if self.job is not None:
                return 409, error("busy", "another job runs, a start is asked already, or a person has a screen open")
            part = body.get("part")
            if part not in ("cup", "scale", "powder"):
                return 400, error("not_a_choice", "part is cup, scale or powder")
            profile = body.get("profile", self.selected()) if part == "powder" else None
            if part == "powder" and (not isinstance(profile, int) or self.powder(profile) is None):
                return 400, error("out_of_range", "no such powder profile")
            if profile is not None:
                _, fine, coarse = self.powder(profile)
                self.was = {"fine_min_speed_rps": fine, "coarse_min_speed_rps": coarse}
            self.job = {"part": part, "profile": profile, "step": "cup_on", "since_ms": self.now_ms(), "done": 0,
                        "line": "Put the empty cup on the scale", "fails": body.get("mock") == "fail"}
            return 202, self.reply()
        if method == "POST" and path == f"{PATH}/stop":
            stopped = self.job is not None
            self.job = None
            return (202 if stopped else 200), self.reply()
        if self.job is None:
            return 409, error("not_running", "no calibration runs: start a part first")
        job = self.job
        if method == "POST" and path == f"{PATH}/pour":
            turns = body.get("turns")
            if not isinstance(turns, (int, float)) or isinstance(turns, bool):
                return 400, error("wrong_type", "turns are a number")
            if turns == 0 or abs(turns) > POUR_MOST_TURNS:
                return 400, error("out_of_range", "not 0, and no more than half a turn either way")
            if job["step"] != "fill":
                return 409, error("not_filling", "the cup is not being filled")
            if turns > 0:
                job["pour_turns"] = max(job["pour_turns"], turns)
            else:
                job["pour_turns"] = min(job["pour_turns"], turns) if job["pour_turns"] < 0 else turns
            return 200, self.reply()
        if method == "POST" and path == f"{PATH}/answer":
            answer = body.get("answer")
            if ANSWERS.get(job["step"]) != answer:
                return 409, error("not_now", "not an answer at this step: read step")
            if job["step"] == "cup_on":
                self.step("zeroing", line="Zeroing")
            elif job["step"] == "fill":
                self.step("keep", capacity=round(job["weight"], 2), line="Keep it as its capacity?")
            elif job["step"] == "keep":
                self.cup = {"capacity": job["capacity"], "weight": job.get("cup_weight")}
                self.step("empty", line="Empty the cup")
            elif job["step"] == "paused":
                self.step("running")
            else:
                self.job = None             # empty or failed: done
            return 200, self.reply()
        if method == "POST" and path == f"{PATH}/save":
            if job["step"] != "result":
                return 409, error("not_now", "nothing found to keep yet: read step")
            if job["part"] == "scale":
                self.scale = {"settle_ms": job["result"]["settle_ms"], "lag": list(job["result"]["lag"])}
            else:
                why = self.store(job["profile"], job["result"]["fine_min_speed_rps"],
                                 job["result"]["coarse_min_speed_rps"])
                if why is not None:
                    return 409, error("not_kept", why)
            self.job = None
            return 200, self.reply()
        return 405, error("method_not_allowed", "not on this path")
