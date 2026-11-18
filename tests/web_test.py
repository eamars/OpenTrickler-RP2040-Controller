"""
A stand-in for the unit, for work on its pages (ADR-003, gate 4): the pages from src/html, read again at each load,
and API v1 answered from a recording of a real unit's replies. A write is checked against the recorded schema with
the client's own checks (tools/remote_control/unit_api.py), refused as the unit would refuse it, and kept in memory
until the stand-in stops. A charge started throws: its weight rises with each read of the charge, to its target.

    venv\\Scripts\\python tests\\web_test.py --record <host>    a unit's replies into tests/web_test_data.json
    venv\\Scripts\\python tests\\web_test.py [--port 8000]      the pages and the recording, at http://localhost:8000/

The recording is the unit's, not this file's: record again when the firmware's API grows. Only the Python
standard library and the client are used.
"""
import argparse
import base64
import copy
import json
import sys
import threading
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools" / "remote_control"))
import unit_api  # noqa: E402  (the client's folder, put on the path just above)

DATA = Path(__file__).resolve().with_name("web_test_data.json")
MOCK = REPO / "build_web"           # ADR-008's pages, built from src/web by scripts/web_build.py
PAGES = {"/": "web_portal.html", "/mobile": "web_portal.html", "/wizard": "wizard.html",
         "/display_mirror": "display_mirror.html"}
FRAME = "/api/v1/display/frame"
EVENTS = "/api/v1/events"
NOT_RECORDED = {EVENTS}             # made here: the log of what the stand-in did
STEP = 1.5                          # a charge's weight rises this much at each read of the charge
HOLD_READS = 4                      # a throw's verdict held this many reads of the charge (2 s at the page's 500 ms)
# The recording is kept in git: the owner's network is not. (The Wi-Fi key is never read: the unit never shows it)
PRIVATE = {"/api/v1/network": {"ssid": "dummy_ssid", "address": "192.168.4.1"},
           "/api/v1/settings/wifi": {"ssid": "dummy_ssid"}}
# The schema's words as ADR-008 proposes them ("The words"), shown by the mock before the unit says them: a group
# that repeats its resource goes (null: the resource's own fields, under no heading), a field named as its resource
# takes its own name, "trickler" leaves the motors and the powders. Each is (resource, field or None, key, the
# recording's word, the proposed one). Gate 2 writes them into the C tables and records again; the stand-in then
# refuses to start until this list is emptied, so it cannot outlive its gate.
PROPOSED_WORDS = [
    ("/api/v1/settings/scale", "driver", "group", "Scale", None),
    ("/api/v1/settings/scale", "driver", "label", "Scale", "Make"),
    ("/api/v1/settings/gate", "enable", "group", "Gate", None),
    ("/api/v1/settings/gate", "enable", "label", "Servo gate", "Fitted"),
    *[("/api/v1/settings/wifi", name, "group", "Wi-Fi", None) for name in ("enable", "ssid", "key", "auth", "timeout_ms")],
    ("/api/v1/settings/wifi", "enable", "label", "Wi-Fi", "Join a network"),
    *[("/api/v1/settings/display", name, "group", "Display", None)
      for name in ("invert_encoder", "rotation", "invert_display", "charge_view")],
    *[("/api/v1/settings/update", name, "group", "Update", None) for name in ("channel", "source")],
    ("/api/v1/settings/leds", None, "label", "LEDs", "Lights"),
    *[("/api/v1/settings/leds", name, "group", "Display", "Display board")
      for name in ("backlight_colour", "led1_colour", "led2_colour")],
    *[(f"/api/v1/settings/motors/{motor}", None, "label", f"{motor.capitalize()} trickler motor", f"{motor.capitalize()} motor")
      for motor in ("coarse", "fine")],
    *[(f"/api/v1/settings/motors/{motor}", name, "group", "Motor", None) for motor in ("coarse", "fine")
      for name in ("full_steps_per_rotation", "max_speed_rps", "acceleration_rps2", "min_speed_rps", "gear_ratio")],
    ("/api/v1/powder_profiles/{id}", None, "label", "Powder profile", "Powder"),
    ("/api/v1/powder_profiles/{id}", "name", "group", "Profile", None),
    *[("/api/v1/powder_profiles/{id}", f"{motor}_{name}", "group", f"{motor.capitalize()} trickler", None)
      for motor in ("coarse", "fine") for name in ("kp", "ki", "kd", "min_speed_rps", "max_speed_rps")],
    ("/api/v1/powder_profiles/{id}", "name", "about",
     "The profile's name, as the menu shows it: printable ASCII, the screen's font draws nothing else",
     "The powder's name, as the menu shows it: printable ASCII, the screen's font draws nothing else"),
    *[("/api/v1/powder_profiles/{id}", f"{motor}_{name}", "about", f"The {motor} trickler's {was}", f"The {motor} motor's {now}")
      for motor in ("coarse", "fine")
      for name, was, now in (("kp", "proportional gain", "proportional gain"),
                             ("ki", "integral gain", "integral gain"),
                             ("kd", "derivative gain", "derivative gain"),
                             ("min_speed_rps", "slowest while it pours; not above its maximum",
                              "slowest while it pours; not above its maximum"),
                             ("max_speed_rps", f"fastest, used up to the {motor} motor's max_speed_rps",
                              "fastest while it pours; not above its Maximum speed in Settings, Motors"))],
]


def propose_words(schema):
    """
    Put PROPOSED_WORDS into the recorded schema, each found first as the recording says it.

    Args:
        schema: the recorded `GET /api/v1/schema`, changed in place.
    """
    for path, name, key, recorded, proposed in PROPOSED_WORDS:
        resource = next(r for r in schema["resources"] if r["path"] == path)
        target = resource if name is None else next(f for f in resource["fields"] if f["name"] == name)
        if target.get(key) != recorded:
            sys.exit(f"the recording says {key} {target.get(key)!r} for {path} {name or ''}, not {recorded!r}: "
                     f"the unit has the words now, so take them out of PROPOSED_WORDS")
        target[key] = proposed


def propose_revert(schema):
    """
    List ADR-008's proposed revert beside each resource's save (`revert`, the save's path ending in /revert): what
    was written since the last save put back from the EEPROM. Gate 2 builds it in the unit; the stand-in then refuses
    to start until this is taken out.

    Args:
        schema: the recorded `GET /api/v1/schema`, changed in place.
    """
    for resource in schema["resources"]:
        if "revert" in resource:
            sys.exit(f"the recording has {resource['path']}'s revert: the unit makes it now, so take out propose_revert")
        resource["revert"] = resource["save"].removesuffix("/save") + "/revert"


# The release the stand-in's poll finds (ADR-008, the offer): newer than the recording's firmware, in the fields
# api_v1.md gives `found`
OFFER = {"version": "2.1.0", "block": "2.1.0", "channel": "stable", "bench": None,
         "note": "The web pages made new: the Trickler, the powders and every setting from a phone. Purge is the screen's "
                 "word for the cleanup now."}
CHECK_MS = 1500                     # a poll's time, from Check now to what it found
# The update as a job (ADR-004's times, made shorter: 800 KB came in 14 s, the unit was back about 8 s after the
# install, a trial that is not kept goes back about 75 s on). Each phase's length, in ms
JOB_STARTING_MS = 500               # asked, and the menu has not begun it
JOB_CONNECT_MS = 1500               # a download's DNS, TLS and redirects, before its length is known
JOB_DOWNLOAD_MS = 6000
JOB_CHECK_MS = 600                  # the manifest's SHA-256 and the image's seal
JOB_INSTALL_MS = 300                # the install's reply gets out before the restart
JOB_TRIAL_MS = 2500                 # the new image on its network, then confirmed
JOB_DOWN_MS = {"confirmed": 6000, "back": 20000, "power": 9000}     # the unit off the network, by how it ends
IMAGE_BYTES = 805_000
NEXT_VERSION = OFFER["version"]
# How long the page may wait through the restart before it says the unit is not back (proposed, ADR-008's life
# cycle, gate 2): the trial's limit and a start. The stand-in's is its own times'
BACK_WITHIN_S = 25
ENDS = ["confirmed", "back", "refused", "power"]
RESTART_MS = 5000                   # a plain restart: the unit away, then back on a new boot


def record(host):
    """
    Read every GET path a unit's index lists, its index and schema with them, into DATA.

    Args:
        host: the unit's address.
    """
    unit = unit_api.Unit(host)
    replies = {}
    for path in unit.paths("GET"):
        if path in NOT_RECORDED:
            continue
        reply = unit.read(path)
        replies[path] = {"base64": base64.b64encode(reply).decode()} if isinstance(reply, bytes) else reply
        replies[path].update(PRIVATE.get(path, {}))
    replies["/api/v1/settings"]["resources"]["/api/v1/settings/wifi"].update(PRIVATE["/api/v1/settings/wifi"])
    DATA.write_text(json.dumps(replies, indent=1), encoding="utf-8")
    print(f"{len(replies)} paths of {unit.index['hostname']} (firmware {unit.index['firmware']['version']}) "
          f"recorded into {DATA}")


class StandIn:
    """The unit's state as the stand-in keeps it: the recorded replies, changed by the writes and actions."""

    def __init__(self, replies, offer=None, ends="confirmed"):
        """
        Args:
            replies: the recording, path to reply.
            offer: an importance (normal, critical) for a release found from the start, or None: found by Check now.
            ends: how an update ends, one of ENDS: kept, gone back, refused at its check, or a power cut in its trial.
        """
        self.replies = replies
        self.schema = replies["/api/v1/schema"]
        propose_words(self.schema)
        propose_revert(self.schema)
        # What the EEPROM holds, by path: a save copies the live values here, a revert copies them back
        self.stored = {path: copy.deepcopy(reply) for path, reply in replies.items() if self.resource(path) is not None}
        self.importance = offer or "normal"
        self.checked_ms = None
        if "back_within_s" in replies["/api/v1/update"]:
            sys.exit("the recording has the update's back_within_s: the unit gives it now, so take out BACK_WITHIN_S")
        self.ends = ends
        self.job = None                 # the update job: its phase and times, or None
        self.down_until_ms = None       # the unit restarting: nothing answers until then
        if offer:
            self.replies["/api/v1/update"].update(found=dict(OFFER, importance=offer), poll={"state": "found", "error": None})
        self.lock = threading.Lock()
        self.events = []
        self.held = 0                   # reads of the charge since its last step, while the cup is lifted and returned
        self.started_ms = time.monotonic() * 1000
        self.add_event("system", "started", firmware=replies["/api/v1"]["firmware"]["version"])

    def now_ms(self):
        return int(time.monotonic() * 1000 - self.started_ms)

    def add_event(self, source, kind, **carries):
        """Log an event, numbered from 1, as the unit's log does."""
        self.events.append({"seq": len(self.events) + 1, "at_ms": self.now_ms(), "source": source, "kind": kind,
                            **carries})

    def resource(self, path):
        """The schema's settings resource for a path, or None."""
        wanted = unit_api.template_of(path)
        return next((resource for resource in self.schema["resources"] if resource["path"] == wanted), None)

    def action(self, method, path):
        """The schema's action, or None."""
        return next((action for action in self.schema["actions"]
                     if action["method"] == method and action["path"] == path), None)

    def get(self, path, query):
        """
        A read.

        Args:
            path: the path, without its query.
            query: the query's values, by name.

        Returns:
            (status, reply): a dict for JSON, bytes for the frame.
        """
        if path == EVENTS:
            after = int(query.get("after", "0"))
            return 200, {"boot": self.replies["/api/v1"]["boot"], "now_ms": self.now_ms(), "first": 1,
                         "next": len(self.events), "events": self.events[after:]}
        if path not in self.replies:
            return 404, error("not_found", "no such path on this build")
        reply = self.replies[path]
        if path == FRAME:
            return 200, base64.b64decode(reply["base64"])
        if path == "/api/v1/charge":
            self.pour()
        if path == "/api/v1/update":
            self.poll()
            reply = dict(reply, back_within_s=BACK_WITHIN_S) if reply["state"] == "installing" else reply
        if path == "/api/v1/gate":
            self.gate()
        if path == "/api/v1/settings":
            reply["resources"] = {inner: self.replies[inner] for inner in reply["resources"]}
        return 200, reply

    def down(self):
        """
        Whether the unit is restarting: then a request gets no reply at all, as from a unit gone from the network.
        Each request moves the update job on first.
        """
        with self.lock:
            self.tick()
            return self.down_until_ms is not None

    def tick(self):
        """The update job moved on by the clock: its phases, the restart, the trial."""
        job = self.job
        if job is None:
            if self.down_until_ms is not None and self.now_ms() >= self.down_until_ms:
                self.down_until_ms = None
                self.new_boot(self.replies["/api/v1"]["firmware"]["version"])     # a plain restart is over
            return
        update = self.replies["/api/v1/update"]
        now = self.now_ms()
        since = now - job["at"]
        if job["phase"] == "starting" and since >= JOB_STARTING_MS:
            self.job_phase("receiving")
            update.update(state="receiving")
            self.add_event("update", "started", by="api")
        elif job["phase"] == "receiving" and job["download"] and since >= JOB_CONNECT_MS:
            share = min((since - JOB_CONNECT_MS) / JOB_DOWNLOAD_MS, 1.0)
            update.update(expected=IMAGE_BYTES, received=int(IMAGE_BYTES * share))
            if share >= 1.0:
                if self.ends == "refused":
                    self.job_end("refused", "hash_mismatch", "not the image its release's manifest names")
                else:
                    self.job_phase("ready")
                    update.update(state="ready")
        elif job["phase"] == "ready" and job["download"] and since >= JOB_CHECK_MS:
            self.job_install()
        elif job["phase"] == "installing" and since >= JOB_INSTALL_MS:
            self.job_phase("down")
            self.down_until_ms = now + JOB_DOWN_MS["back" if self.ends == "refused" else self.ends]
        elif job["phase"] == "down" and now >= self.down_until_ms:
            self.down_until_ms = None
            self.job_restarted()
        elif job["phase"] == "trial" and since >= JOB_TRIAL_MS:
            update["running"] = dict(update["running"], trial=False)
            update["last"] = {"result": "confirmed", "version": NEXT_VERSION}
            self.add_event("update", "confirmed", version=NEXT_VERSION)
            self.job = None

    def job_phase(self, phase):
        self.job.update(phase=phase, at=self.now_ms())

    def job_install(self):
        self.job_phase("installing")
        self.replies["/api/v1/update"].update(state="installing")
        self.add_event("update", "installing", version=NEXT_VERSION)

    def job_end(self, result, code=None, message=None):
        """The job ends before any restart: refused at its check, stopped, or out of time."""
        self.replies["/api/v1/update"].update(state="idle", received=0, expected=0, download=False,
                                              last={"result": result, "code": code, "message": message,
                                                    "version": NEXT_VERSION if result == "refused" else None})
        self.replies["/api/v1/system"]["activity"] = "none"
        self.add_event("update", "stopped", reason="refused" if result == "refused" else result, code=code)
        self.job = None

    def job_restarted(self):
        """
        The unit back on the network after the install's restart: a new boot and a new event log, on the new image on
        trial (kept), or on the old one (gone back, or a power cut in the trial, which the unit cannot tell).
        """
        index = self.replies["/api/v1"]
        update = self.replies["/api/v1/update"]
        old = update["running"]["version"]
        kept = self.ends == "confirmed"
        version = NEXT_VERSION if kept else old
        self.new_boot(version)
        running = dict(update["running"], version=version, trial=kept)
        if kept:
            running.update(block=NEXT_VERSION, slot="b" if running["slot"] == "a" else "a")
        last = None if self.ends == "power" else (None if kept else {"result": "back", "version": NEXT_VERSION})
        found = None if kept else dict(OFFER, importance=self.importance)   # the start's poll finds it again
        update.update(state="idle", received=0, expected=0, download=False, running=running, last=last,
                      found=found, poll={"state": "found" if found else "up_to_date", "error": None})
        if kept:
            update["takes"] = update["takes"][:-1] + ("b" if update["takes"].endswith("a") else "a")
        if self.ends == "back":
            self.add_event("update", "back", version=NEXT_VERSION)
        if kept:
            self.job_phase("trial")
        else:
            self.job = None

    def new_boot(self, version):
        """The unit started again: a new boot, a new event log, the firmware it runs now, no job."""
        index = self.replies["/api/v1"]
        index["boot"] = (index["boot"] * 1103515245 + 12345) % 2 ** 32
        index["uptime_s"] = 0
        for reply in (index, self.replies["/api/v1/system"]):
            reply["firmware"] = dict(reply["firmware"], version=version)
        self.replies["/api/v1/system"]["activity"] = "none"
        self.events = []
        self.add_event("system", "started", firmware=version)

    def update_start(self, body):
        """POST /api/v1/update/start: the job, fetching the release found itself, or waiting for an upload."""
        update = self.replies["/api/v1/update"]
        system = self.replies["/api/v1/system"]
        if system["activity"] != "none" or self.job is not None:
            return 409, error("busy", "another job runs")
        download = bool(body.get("download"))
        if download and update["found"] is None:
            return 409, error("not_found", "nothing found to download: check for an update first")
        self.job = {"phase": "starting", "at": self.now_ms(), "download": download}
        system["activity"] = "update"
        update.update(state="starting", received=0, expected=0, download=download, last=None)
        return 202, update

    def update_image(self, size):
        """PUT /api/v1/update/image: the whole image taken, then checked as the unit checks it."""
        update = self.replies["/api/v1/update"]
        if self.job is None or self.job["phase"] != "receiving" or self.job["download"]:
            return 409, error("not_running", "no update is receiving: POST /api/v1/update/start first")
        update.update(expected=size, received=size)
        if self.ends == "refused":
            self.job_end("refused", "wrong_board", "built for another board")
            return 400, error("wrong_board", "built for another board")
        self.job_phase("ready")
        update.update(state="ready")
        self.add_event("update", "received", version=NEXT_VERSION)
        return 200, update

    def poll(self):
        """A poll asked by Check now: checking for a moment, then the release found."""
        update = self.replies["/api/v1/update"]
        if self.checked_ms is not None and self.now_ms() - self.checked_ms >= CHECK_MS:
            self.checked_ms = None
            update.update(found=dict(OFFER, importance=self.importance), poll={"state": "found", "error": None})
            self.add_event("update", "found", version=OFFER["version"])

    def gate(self):
        """The gate as its settings make it, as on the unit: disabled while not fitted, else closed until moved."""
        gate = self.replies["/api/v1/gate"]
        if not self.replies["/api/v1/settings/gate"]["enable"]:
            gate.update(state="disabled", ratio=None)
        elif gate["state"] == "disabled":
            gate.update(state="closed", ratio=1.0)
        return gate

    def pour(self):
        """A charge that runs: its weight rises at each read, and the throw ends at its target."""
        charge = self.replies["/api/v1/charge"]
        if charge["state"] == "charging":
            charge["weight"] = round(min(charge["weight"] + STEP, charge["target"]), 3)
            if charge["weight"] >= charge["target"]:
                charge.update(state="waiting_for_cup_removal", in_throw=False,
                              last={"throw": charge["throw"], "result": "good", "reason": "finished",
                                    "weight": charge["weight"]})
                self.add_event("throw", "finished", throw=charge["throw"], result="good", weight=charge["weight"],
                               elapsed_s=2.0)
        elif charge["state"] == "waiting_for_cup_removal":
            # The verdict is held while the cup is on the pan, as a unit holds it: a few reads, then the cup is away
            self.held += 1
            if self.held >= HOLD_READS:
                self.held = 0
                charge.update(state="waiting_for_cup_return", weight=0.0)
        elif charge["state"] == "waiting_for_cup_return":
            self.held += 1
            if self.held >= HOLD_READS // 2:
                self.held = 0
                charge.update(state="charging", in_throw=True, throw=charge["throw"] + 1, weight=0.0)
                self.add_event("throw", "started", throw=charge["throw"], target=charge["target"],
                               powder_profile=charge["powder_profile"])

    def write(self, method, path, body):
        """
        A PATCH or a POST, checked against the schema as the unit checks it.

        Args:
            method: "PATCH" or "POST".
            path: the path.
            body: the JSON body.

        Returns:
            (status, reply).
        """
        # A save: what was written there is stored (the save of all, everything)
        saved = [r["path"] for r in self.schema["resources"] if path in (r["save"], "/api/v1/settings/save")]
        if method == "POST" and saved:
            for inner in self.replies:
                if unit_api.template_of(inner) in saved:
                    self.replies[inner]["saved"] = True
                    self.stored[inner] = copy.deepcopy(self.replies[inner])
            return 200, {"saved": True}
        # A revert (proposed, ADR-008): the slot's resources as the EEPROM holds them
        reverted = [r["path"] for r in self.schema["resources"] if path == r["revert"]]
        if method == "POST" and reverted:
            for inner in self.replies:
                if unit_api.template_of(inner) in reverted:
                    self.replies[inner].clear()
                    self.replies[inner].update(copy.deepcopy(self.stored[inner]))
            return 200, {"reverted": True}
        resource = self.resource(path) if method == "PATCH" else None
        action = self.action(method, path)
        if resource is None and action is None:
            return (405 if path in self.replies else 404), error("method_not_allowed", "not on this path")
        fields = resource["fields"] if resource is not None else action["fields"]
        problems = unit_api.fields_problems(fields, body, writing=True)
        if problems:
            code, _, message = problems[0].partition(": ")
            return 400, error(code, message)
        if resource is not None:
            self.replies[path].update(body)
            # Not stored is the resource's own (proposed, ADR-008, gate 2): this powder or this motor against what the
            # EEPROM holds of it, not the whole slot's record. A value written back as stored is stored again
            live, held = (dict(r, saved=None) for r in (self.replies[path], self.stored[path]))
            self.replies[path]["saved"] = live == held
            return 200, self.replies[path]
        return self.act(path, body)

    def act(self, path, body):
        """An action the schema lists, its body checked: what it does to the recorded state."""
        charge = self.replies["/api/v1/charge"]
        cleanup = self.replies["/api/v1/cleanup"]
        system = self.replies["/api/v1/system"]
        busy = system["activity"] != "none"
        if path == "/api/v1/charge":
            charge.update(body)
            return 200, charge
        if path == "/api/v1/charge/start":
            if busy:
                return 409, error("busy", "another job runs")
            charge.update(body)
            if charge["target"] is None:
                return 400, error("no_target", "no target is set: give one")
            charge.update(state="charging", in_throw=True, throw=1, weight=0.0)
            system["activity"] = "charge"
            self.add_event("charge", "started", by="api", target=charge["target"],
                           powder_profile=charge["powder_profile"])
            self.add_event("throw", "started", throw=1, target=charge["target"],
                           powder_profile=charge["powder_profile"])
            return 202, charge
        if path in ("/api/v1/charge/stop", "/api/v1/cleanup/stop", "/api/v1/stop"):
            stopped = system["activity"] != "none"
            if system["activity"] == "charge":
                # As a unit reports a stopped charge: no verdict, and the pan's own reading (none here: no scale)
                charge.update(state="idle", in_throw=False, weight=None,
                              last={"throw": charge["throw"], "result": "none", "reason": "stopped", "weight": 0})
                self.held = 0
                self.add_event("charge", "stopped", by="api", reason="stopped")
            if system["activity"] == "cleanup":
                cleanup.update(state="idle", speed_rps=0.0, time_left_s=None)
                self.add_event("cleanup", "stopped", by="api", reason="stopped")
            system["activity"] = "none"
            if path == "/api/v1/stop":
                return (202 if stopped else 200), {"stopped": stopped, "activity": system["activity"]}
            return (202 if stopped else 200), (charge if "charge" in path else cleanup)
        if path == "/api/v1/cleanup/start":
            if busy:
                return 409, error("busy", "another job runs")
            cleanup.update(state="running", speed_rps=0.0, time_left_s=None)
            system["activity"] = "cleanup"
            self.add_event("cleanup", "started", by="api")
            return 202, cleanup
        if path == "/api/v1/cleanup":
            if cleanup["state"] != "running":
                return 409, error("not_running", "no cleanup runs")
            cleanup.update(speed_rps=body["speed_rps"], time_left_s=body.get("time_s"))
            return 200, cleanup
        if path == "/api/v1/gate":
            gate = self.gate()
            if gate["state"] == "disabled":
                return 409, error("gate_disabled", "the gate is off in its settings")
            gate.update(ratio=body["ratio"], state={0: "open", 1: "closed"}.get(body["ratio"], "partial"))
            return 200, gate
        if path == "/api/v1/scale/zero":
            return 200, {"zeroed": True}
        if path == "/api/v1/update/start":
            return self.update_start(body)
        if path == "/api/v1/update/install":
            if self.job is None or self.job["phase"] != "ready":
                return 409, error("not_ready", "no image is whole and checked")
            self.job_install()
            return 202, self.replies["/api/v1/update"]
        if path == "/api/v1/update/stop":
            if self.job is None or self.job["phase"] not in ("starting", "receiving", "ready"):
                return 409, error("not_running", "no update to stop")
            self.job_end("stopped")
            return 202, self.replies["/api/v1/update"]
        if path == "/api/v1/update/check":
            update = self.replies["/api/v1/update"]
            update.update(found=None, poll={"state": "checking", "error": None})
            self.checked_ms = self.now_ms()
            return 202, update
        if path == "/api/v1/system/restart":
            self.down_until_ms = self.now_ms() + RESTART_MS
            return 202, {"restarting": True}
        if path == "/api/v1/system/erase":
            return 202, {"erasing": True}
        return 404, error("not_found", "an action this stand-in does not make")


def error(code, message):
    """A refusal in api_v1.md's form."""
    return {"error": {"code": code, "message": message}}


def make_handler(stand_in):
    """The request handler class, bound to one StandIn."""

    class Handler(BaseHTTPRequestHandler):
        def send(self, status, reply, content_type="application/json"):
            body = reply if isinstance(reply, bytes) else json.dumps(reply).encode()
            if isinstance(reply, bytes) and content_type == "application/json":
                content_type = "application/octet-stream"
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def gone(self):
            """While the unit restarts nothing answers: the connection is closed with no reply."""
            if stand_in.down():
                self.close_connection = True
                return True
            return False

        def do_GET(self):
            if self.gone():
                return
            url = urllib.parse.urlsplit(self.path)
            if url.path in PAGES:
                page = (REPO / "src" / "html" / PAGES[url.path]).read_bytes()
                self.send(200, page, "text/html")
                return
            # ADR-008's new pages, as scripts/web_build.py made them into build_web/
            if url.path.startswith("/mock/"):
                built = MOCK / Path(url.path).name
                if built.suffix == ".html" and built.is_file():
                    self.send(200, built.read_bytes(), "text/html")
                else:
                    self.send(404, error("not_found", "not built: run scripts/web_build.py"))
                return
            query = dict(urllib.parse.parse_qsl(url.query))
            with stand_in.lock:
                status, reply = stand_in.get(url.path, query)
            self.send(status, reply)

        def write(self, method):
            if self.gone():
                return
            length = int(self.headers.get("Content-Length", "0"))
            raw = self.rfile.read(length)
            if self.headers.get_content_type() != "application/json":
                self.send(415, error("wrong_media_type", "a write is sent as application/json"))
                return
            try:
                body = json.loads(raw or b"{}")
            except ValueError:
                self.send(400, error("bad_json", "the body is not JSON"))
                return
            with stand_in.lock:
                status, reply = stand_in.write(method, urllib.parse.urlsplit(self.path).path, body)
            self.send(status, reply)

        def do_PATCH(self):
            self.write("PATCH")

        def do_POST(self):
            self.write("POST")

        def do_PUT(self):
            if self.gone():
                return
            raw = self.rfile.read(int(self.headers.get("Content-Length", "0")))
            if urllib.parse.urlsplit(self.path).path != "/api/v1/update/image":
                self.send(405, error("method_not_allowed", "not on this path"))
                return
            with stand_in.lock:
                status, reply = stand_in.update_image(len(raw))
            self.send(status, reply)

    return Handler


def main():
    """Record a unit's replies, or serve the pages and the recording."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--record", metavar="HOST", help="record a unit's replies into tests/web_test_data.json")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--offer", choices=["normal", "critical"],
                        help="start with a release found, of this importance (else Check now finds a normal one)")
    parser.add_argument("--update-ends", choices=ENDS, default="confirmed",
                        help="how an update ends: kept, gone back, refused at its check, or a power cut in its trial")
    args = parser.parse_args()
    if args.record:
        record(args.record)
        return
    stand_in = StandIn(json.loads(DATA.read_text(encoding="utf-8")), args.offer, args.update_ends)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(stand_in))
    print(f"the pages and {len(stand_in.replies)} recorded paths at http://127.0.0.1:{args.port}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
