"""ADR-009 step 1's calibration, simulated for the web's mock (`web_test.py`): a provisional API, the mock's only.

The unit has no calibration API yet: ADR-009's gate 3 makes it. This plays one on the stand-in so the web's pages can be
designed on it (`web-ui-design`, "the mock comes first"), with the 12864's words (`tools/ui_mock/calibration_12864.py`):

- `GET /api/v1/calibration`: the records (the cup, the scale, the selected powder) and the job, or null;
- `POST /api/v1/calibration/start` `{"part": "cup"|"scale"|"powder"}`: a part started alone (`"mock": "fail"` makes
  a powder's fail, to show it);
- `POST /api/v1/calibration/answer` `{"answer": "next"|"full"|"keep"|"done"|"go_on"}`: the person's answer at a step;
- `PATCH /api/v1/calibration/fill` `{"fine_rps", "coarse_rps"}`: the tubes' speeds while the cup is filled;
- `POST /api/v1/calibration/save`: a scale's or a powder's result kept; `POST /api/v1/calibration/stop`: the part stopped.

The figures are the bench's of 2026-11-25 (ADR-009's progress).
"""

PATH = "/api/v1/calibration"
SCALE_S = 24.0                  # a scale's part, as the mock plays it (the bench's took 4.6 minutes)
POWDER_S = 30.0
LIFT_S = 3.0                    # the cup lifted and put back, as the mock plays it
POWDER_PAUSE_AT = 0.6           # the powder's part pauses here for a full cup
CUP_GR_A_TURN = {"fine": 0.8, "coarse": 10.0}
CUP_WEIGHT_GR = 77.3
SCALE_RESULT = {"small_change_s": 0.7, "fast_flow_s": 0.6, "slow_flow_s": 1.2}
POWDER_RESULT = {"fine": 0.02, "coarse": 0.16}      # each tube's least speed, rev/s
POWDER_WAS = {"fine": 0.08, "coarse": 0.1}


def error(code, message):
    return {"error": {"code": code, "message": message}}


class CalibrationMock:
    """The calibration's records and its one job, moved on by the mock's clock."""

    def __init__(self, now_ms, powder):
        """
        Args:
            now_ms: the stand-in's clock, in milliseconds.
            powder: the selected powder's (index, name), read at each call.
        """
        self.now_ms = now_ms
        self.powder = powder
        self.cup = {"capacity_gr": None, "weight_gr": None}
        self.scale = None
        self.powders = {}
        self.job = None

    def reply(self):
        """The records and the job, as a GET gives them."""
        self.move_on()
        index, name = self.powder()
        powder = self.powders.get(index)
        return {"cup": dict(self.cup), "scale": self.scale,
                "powder": {"profile": index, "name": name, "min_speed_rps": powder}, "job": self.job}

    def move_on(self):
        """What the clock does to the job: a lift seen, a pour made, a part's progress."""
        job = self.job
        if job is None:
            return
        elapsed = (self.now_ms() - job["since_ms"]) / 1000.0
        if job["step"] == "lift" and elapsed >= LIFT_S:
            self.step("back", weight_gr=-CUP_WEIGHT_GR)
        elif job["step"] == "back" and elapsed >= LIFT_S:
            self.cup["weight_gr"] = CUP_WEIGHT_GR
            self.step("fill", weight_gr=0.0, flow_gr_s=0.0, fine_rps=0.0, coarse_rps=0.0)
        elif job["step"] == "fill":
            flow = sum(job[f"{tube}_rps"] * CUP_GR_A_TURN[tube] for tube in CUP_GR_A_TURN)
            job["weight_gr"] = round(job["weight_gr"] + flow * (elapsed - job.get("poured_s", 0.0)), 2)
            job["poured_s"] = elapsed
            job["flow_gr_s"] = round(flow, 2)
        elif job["step"] == "running":
            length = SCALE_S if job["part"] == "scale" else POWDER_S
            progress = min(100, int(job["done_s"] + elapsed / length * 100))
            job["progress"] = progress
            if job["part"] == "scale":
                job["line"] = "Small moves" if progress < 40 else "The scale's lag"
            else:
                speed = 0.02 * (250 ** (progress / 100.0))
                job["line"] = f"{'Fine' if progress < 55 else 'Coarse'} tube {speed:.2f} rev/s"
                if job.get("fails") and progress >= 30:
                    self.step("failed", line="Fine tube: no powder", note="Its minimum is kept")
                elif progress >= POWDER_PAUSE_AT * 100 and not job.get("paused_once"):
                    job["paused_once"] = True
                    capacity = self.cup["capacity_gr"] or 150.0
                    self.step("paused", held_gr=capacity - 2, line=f"It holds {capacity - 2:.0f} of {capacity:.0f} gr")
            if job["step"] == "running" and progress >= 100:
                result = dict(SCALE_RESULT) if job["part"] == "scale" else {
                    tube: speed for tube, speed in POWDER_RESULT.items()}
                was = None if job["part"] == "scale" else dict(POWDER_WAS)
                self.step("result", result=result, was=was,
                          took="4.6 min, 62 gr" if job["part"] == "scale" else "6 min, 210 gr")

    def step(self, name, **values):
        """The job moved to a step, its time from now."""
        done_s = self.job.get("progress", 0) if name == "running" else 0
        keep = {key: self.job[key] for key in ("part", "fails", "paused_once", "progress") if key in self.job}
        self.job = dict(keep, step=name, since_ms=self.now_ms(), done_s=done_s, **values)

    def get(self, path):
        if path != PATH:
            return 404, error("not_found", "no such path on this build")
        return 200, self.reply()

    def write(self, method, path, body):
        """A start, an answer, the fill's speeds, a save or a stop."""
        self.move_on()
        if method == "POST" and path == f"{PATH}/start":
            if self.job is not None:
                return 409, error("busy", "a calibration runs")
            part = body.get("part")
            if part == "cup":
                self.job = {"part": "cup", "step": "cup_on", "since_ms": self.now_ms(), "done_s": 0}
            elif part in ("scale", "powder"):
                self.job = {"part": part, "step": "cup_on", "since_ms": self.now_ms(), "done_s": 0,
                            "fails": body.get("mock") == "fail"}
            else:
                return 400, error("not_a_choice", "part: cup, scale or powder")
            return 202, self.reply()
        if self.job is None:
            return 409, error("no_job", "no calibration runs")
        job = self.job
        if method == "PATCH" and path == f"{PATH}/fill":
            if job["step"] != "fill":
                return 409, error("not_filling", "the cup is not being filled")
            for tube in ("fine", "coarse"):
                if f"{tube}_rps" in body:
                    job[f"{tube}_rps"] = max(0.0, min(5.0, float(body[f"{tube}_rps"])))
            return 200, self.reply()
        if method == "POST" and path == f"{PATH}/answer":
            answer = body.get("answer")
            moves = {("cup_on", "next"): ("lift", {"weight_gr": 0.0}),
                     ("fill", "full"): ("keep", {"capacity_gr": job.get("weight_gr")}),
                     ("keep", "keep"): ("empty", {}),
                     ("paused", "go_on"): ("running", {})}
            if job["step"] == "cup_on" and job["part"] != "cup" and answer == "next":
                self.step("running", progress=0, line="Starting")
                return 200, self.reply()
            if job["step"] == "keep" and answer == "keep":
                self.cup["capacity_gr"] = job["capacity_gr"]
            if job["step"] == "empty" and answer == "done":
                self.job = None
                return 200, self.reply()
            if job["step"] == "failed" and answer == "done":
                self.job = None
                return 200, self.reply()
            move = moves.get((job["step"], answer))
            if move is None:
                return 409, error("not_now", f"{answer} is not an answer at {job['step']}")
            self.step(move[0], **move[1])
            return 200, self.reply()
        if method == "POST" and path == f"{PATH}/save":
            if job["step"] != "result":
                return 409, error("no_result", "nothing to keep yet")
            if job["part"] == "scale":
                self.scale = job["result"]
            else:
                self.powders[self.powder()[0]] = job["result"]
            self.job = None
            return 200, self.reply()
        if method == "POST" and path == f"{PATH}/stop":
            self.job = None
            return 202, self.reply()
        return 405, error("method_not_allowed", "not on this path")
