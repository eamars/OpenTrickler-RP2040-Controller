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
NOT_RECORDED = {EVENTS, EVENTS + "/stream"}   # made here: the log of what the stand-in did, and its stream
STEP = 1.5                          # a charge's weight rises this much at each read of the charge
POUR_MS = 450                       # the charge moves on no faster than the page's old 500 ms read
STREAM_MAX = 3                      # the event stream (ADR-008, gate 2): at most this many at once, as the unit
STREAM_KEEP_ALIVE_S = 15            # the state again after this long quiet
HOLD_READS = 4                      # a throw's verdict held this many reads of the charge (2 s at the page's 500 ms)
# The recording is kept in git: the owner's network is not. (The Wi-Fi key is never read: the unit never shows it)
PRIVATE = {"/api/v1/network": {"ssid": "dummy_ssid", "address": "192.168.4.1"},
           "/api/v1/settings/wifi": {"ssid": "dummy_ssid"}}


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
# The set-up (ADR-008, commissioning; gate 4 builds it), proposed and played here. A unit on its own hotspot hears
# these networks, each scan a part of them, merged by name, strongest first (gate 1's probe: 7 and 10 of about 18)
NETWORKS = [
    {"ssid": "Smith home", "rssi": -48, "auth": "wpa2_aes"},
    {"ssid": "Garage extender", "rssi": -63, "auth": "wpa2_mixed"},
    {"ssid": "NETGEAR-21", "rssi": -74, "auth": "wpa2_aes"},
    {"ssid": "Bakery guest", "rssi": -81, "auth": "open"},
    {"ssid": "Neighbour 2.4", "rssi": -86, "auth": "wpa2_aes"},
    {"ssid": "TP-Link_E8C4", "rssi": -89, "auth": "wpa2_mixed"},
]
SCAN_MS = 1300                      # two scans of about 650 ms (gate 1's probe)
JOIN_MS = 4000                      # the station's join, the hotspot kept up meanwhile (5 s on the probe)
WRONG_KEY = "wrongkey"              # the key the stand-in's networks refuse, to show a wrong key told at once
JOINED_ADDRESS = "192.168.1.57"
SETUP_PATHS = {"/api/v1/wifi/scan", "/api/v1/wifi/join", "/api/v1/setup/done"}


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
    # The station's network as the network's (ADR-008, gate 4), and no networks heard: they are the neighbours' names.
    # The stand-in plays its own (NETWORKS)
    station = replies["/api/v1/network"].get("station")
    if station is not None and station["ssid"] is not None:
        station.update(ssid=PRIVATE["/api/v1/network"]["ssid"], address=JOINED_ADDRESS)
    replies["/api/v1/wifi/networks"] = {"scanning": False, "networks": []}
    DATA.write_text(json.dumps(replies, indent=1), encoding="utf-8")
    print(f"{len(replies)} paths of {unit.index['hostname']} (firmware {unit.index['firmware']['version']}) "
          f"recorded into {DATA}")


class StandIn:
    """The unit's state as the stand-in keeps it: the recorded replies, changed by the writes and actions."""

    def __init__(self, replies, offer=None, ends="confirmed", hotspot=False):
        """
        Args:
            replies: the recording, path to reply.
            offer: an importance (normal, critical) for a release found from the start, or None: found by Check now.
            ends: how an update ends, one of ENDS: kept, gone back, refused at its check, or a power cut in its trial.
            hotspot: start as a unit never set up, on its own hotspot, as after a factory reset.
        """
        self.replies = replies
        self.schema = replies["/api/v1/schema"]
        # What the EEPROM holds, by path: a save copies the live values here, a revert copies them back
        self.stored = {path: copy.deepcopy(reply) for path, reply in replies.items() if self.resource(path) is not None}
        self.importance = offer or "normal"
        self.checked_ms = None
        if "back_within_s" in replies["/api/v1/update"]:
            sys.exit("the recording has the update's back_within_s: the unit gives it now, so take out BACK_WITHIN_S")
        self.ends = ends
        self.job = None                 # the update job: its phase and times, or None
        self.propose_setup(hotspot)
        self.down_until_ms = None       # the unit restarting: nothing answers until then
        if offer:
            self.replies["/api/v1/update"].update(found=dict(OFFER, importance=offer), poll={"state": "found", "error": None})
        self.lock = threading.Lock()
        self.streams = 0                # event streams open
        self.poured_ms = 0              # when the charge last moved on
        self.events = []
        self.held = 0                   # reads of the charge since its last step, while the cup is lifted and returned
        self.started_ms = time.monotonic() * 1000
        self.add_event("system", "started", firmware=replies["/api/v1"]["firmware"]["version"])

    def now_ms(self):
        return int(time.monotonic() * 1000 - self.started_ms)

    def propose_setup(self, hotspot):
        """
        The set-up as the stand-in plays it. The unit reports the station's join and the networks (gate 4, step 3); the
        stand-in plays the scan and the join on them. `set_up` in the system's reply (stored; a factory reset clears it)
        is still proposed here until step 4 builds it, and refused once the unit has it.

        Args:
            hotspot: start as a unit never set up, on its own hotspot.
        """
        system = self.replies["/api/v1/system"]
        network = self.replies["/api/v1/network"]
        if "set_up" in system:
            sys.exit("the recording has set_up: the unit makes it now, so take it out of propose_setup")
        system["set_up"] = not hotspot
        if hotspot:
            network["station"] = {"state": "idle", "ssid": None, "address": None, "reason": None}
        self.scan = {"until_ms": None, "heard": []}
        self.joining = None             # a join under way: its network, key and end
        if hotspot:
            hostname = network["hostname"]
            network.update(mode="access_point", ssid=hostname, address="192.168.4.1", rssi=None)
            self.replies["/api/v1/settings/wifi"].update(enable=False, ssid="", saved=True)
            self.stored["/api/v1/settings/wifi"] = copy.deepcopy(self.replies["/api/v1/settings/wifi"])

    def networks(self):
        """GET /api/v1/wifi/networks: what the scans heard, merged by name, strongest first; scanning while one runs."""
        if self.scan["until_ms"] is not None and self.now_ms() >= self.scan["until_ms"]:
            self.scan["until_ms"] = None
            self.scan["heard"] = sorted(NETWORKS, key=lambda n: n["rssi"], reverse=True)
        return {"scanning": self.scan["until_ms"] is not None, "networks": self.scan["heard"]}

    def join_tick(self):
        """A join under way ends after its time: joined and stored, or refused, the hotspot up either way."""
        if self.joining is None or self.now_ms() < self.joining["until_ms"]:
            return
        joining, self.joining = self.joining, None
        station = self.replies["/api/v1/network"]["station"]
        if joining["key"] == WRONG_KEY:
            station.update(state="failed", ssid=joining["ssid"], address=None, reason="wrong_key")
            return
        station.update(state="joined", ssid=joining["ssid"], address=JOINED_ADDRESS, reason=None)
        wifi = self.replies["/api/v1/settings/wifi"]
        wifi.update(enable=True, ssid=joining["ssid"], auth=joining["auth"], saved=True)
        self.stored["/api/v1/settings/wifi"] = copy.deepcopy(wifi)

    def setup_act(self, path, body):
        """The set-up's proposed actions: a scan, a join, the set-up done."""
        network = self.replies["/api/v1/network"]
        if path == "/api/v1/wifi/scan":
            self.scan["until_ms"] = self.now_ms() + SCAN_MS
            return 202, self.networks()
        if path == "/api/v1/wifi/join":
            if not isinstance(body.get("ssid"), str) or not body["ssid"]:
                return 400, error("wrong_type", "ssid: the network's name")
            self.joining = {"ssid": body["ssid"], "key": body.get("key", ""), "auth": body.get("auth", "wpa2_mixed"),
                            "until_ms": self.now_ms() + JOIN_MS}
            network["station"].update(state="joining", ssid=body["ssid"], address=None, reason=None)
            return 202, network
        # Set up: remembered; joined, the hotspot stops and the unit is on its network alone
        self.replies["/api/v1/system"]["set_up"] = True
        if network["station"]["state"] == "joined":
            network.update(mode="station", ssid=network["station"]["ssid"], address=network["station"]["address"],
                           rssi=-58)
        return 200, {"set_up": True, "hotspot": network["mode"] == "access_point"}

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
        if path == "/api/v1/wifi/networks":
            return 200, self.networks()
        if path not in self.replies:
            return 404, error("not_found", "no such path on this build")
        reply = self.replies[path]
        if path == "/api/v1/network":
            self.join_tick()
        if path == FRAME:
            return 200, base64.b64decode(reply["base64"])
        if path == "/api/v1/charge" and self.now_ms() - self.poured_ms >= POUR_MS:
            # At the pace of the page's old read, however often it is read now (gate 3: a stream reads it ten
            # times a second)
            self.poured_ms = self.now_ms()
            self.pour()
        if path == "/api/v1/update":
            self.poll()
            reply = dict(reply, back_within_s=BACK_WITHIN_S) if reply["state"] == "installing" else reply
        if path == "/api/v1/gate":
            self.gate()
        if path == "/api/v1/scale":
            # A scale that talks: the A&D's frames read as a weight near nought; another make set, none read
            talks = self.replies["/api/v1/settings/scale"]["driver"] == "and_fxi"
            reply.update(weight=round((self.now_ms() // 700 % 3 - 1) * 0.02, 2) if talks else None,
                         unit="gr" if talks else None, driver=self.replies["/api/v1/settings/scale"]["driver"])
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
        if method == "POST" and path in SETUP_PATHS:
            return self.setup_act(path, body)
        # A save: what was written there is stored (the save of all, everything)
        saved = [r["path"] for r in self.schema["resources"] if path in (r["save"], "/api/v1/settings/save")]
        if method == "POST" and saved:
            # Its event once for each save that stored what was not stored, as the unit (ADR-008, gate 2)
            news = []
            for inner in self.replies:
                if unit_api.template_of(inner) in saved:
                    save = self.resource(inner)["save"]
                    if self.replies[inner]["saved"] is False and save not in news:
                        news.append(save)
                    self.replies[inner]["saved"] = True
                    self.stored[inner] = copy.deepcopy(self.replies[inner])
            for save in news:
                self.add_event("setting", "stored", by="api", path=save)
            return 200, {"saved": True}
        # A revert (proposed, ADR-008): the slot's resources as the EEPROM holds them
        reverted = [r["path"] for r in self.schema["resources"] if path == r["revert"]]
        if method == "POST" and reverted:
            for inner in self.replies:
                if unit_api.template_of(inner) in reverted:
                    changed = dict(self.replies[inner], saved=None) != dict(self.stored[inner], saved=None)
                    self.replies[inner].clear()
                    self.replies[inner].update(copy.deepcopy(self.stored[inner]))
                    if changed:
                        self.add_event("setting", "changed", by="api", path=inner)
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
            before = dict(self.replies[path])
            self.replies[path].update(body)
            if self.replies[path] != before:
                self.add_event("setting", "changed", by="api", path=path)
            # Not stored is the resource's own (proposed, ADR-008, gate 2): this powder or this motor against what the
            # EEPROM holds of it, not the whole slot's record. A value written back as stored is stored again
            live, held = (dict(r, saved=None) for r in (self.replies[path], self.stored[path]))
            self.replies[path]["saved"] = live == held
            return 200, self.replies[path]
        return self.act(path, body)

    def act(self, path, body):
        """An action the schema lists, its body checked: what it does to the recorded state."""
        charge = self.replies["/api/v1/charge"]
        purge = self.replies["/api/v1/purge"]
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
        if path in ("/api/v1/charge/stop", "/api/v1/purge/stop", "/api/v1/stop"):
            stopped = system["activity"] != "none"
            if system["activity"] == "charge":
                # As a unit reports a stopped charge: no verdict, and the pan's own reading (none here: no scale)
                charge.update(state="idle", in_throw=False, weight=None,
                              last={"throw": charge["throw"], "result": "none", "reason": "stopped", "weight": 0})
                self.held = 0
                self.add_event("charge", "stopped", by="api", reason="stopped")
            if system["activity"] == "purge":
                purge.update(state="idle", speed_rps=0.0, time_left_s=None)
                self.add_event("purge", "stopped", by="api", reason="stopped")
            system["activity"] = "none"
            if path == "/api/v1/stop":
                return (202 if stopped else 200), {"stopped": stopped, "activity": system["activity"]}
            return (202 if stopped else 200), (charge if "charge" in path else purge)
        if path == "/api/v1/purge/start":
            if busy:
                return 409, error("busy", "another job runs")
            purge.update(state="running", speed_rps=0.0, time_left_s=None)
            system["activity"] = "purge"
            self.add_event("purge", "started", by="api")
            return 202, purge
        if path == "/api/v1/purge":
            if purge["state"] != "running":
                return 409, error("not_running", "no purge runs")
            purge.update(speed_rps=body["speed_rps"], time_left_s=body.get("time_s"))
            return 200, purge
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
        if path == "/api/v1/system/factory_reset":
            return 202, {"resetting": True}
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

        def stream(self, query):
            """
            The event stream (ADR-008, gate 2), as the unit plays it: the unit, the state, then each event with its id
            and the state as it changes and after 15 s quiet; at most STREAM_MAX at once. A restart ends it
            with no close, as on the unit: the connection goes silent.
            """
            with stand_in.lock:
                if stand_in.streams >= STREAM_MAX:
                    self.send_response(503)
                    self.send_header("Retry-After", "5")
                    body = json.dumps(error("busy", "as many streams are open as the unit keeps: try again")).encode()
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                    return
                stand_in.streams += 1
            try:
                with stand_in.lock:
                    boot = stand_in.replies["/api/v1"]["boot"]
                    newest = len(stand_in.events)
                    last = self.headers.get("Last-Event-ID") or ""
                    after = None
                    if last.startswith(f"{boot}-"):
                        after = int(last.split("-")[1])
                    elif "after" in query:
                        after = int(query["after"])
                    resumed = after is not None and after <= newest
                    sent = after if resumed else newest
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                unit = {"boot": boot, "first": 1, "newest": newest, "resumed": resumed}
                self.wfile.write(f"retry: 3000\nevent: unit\ndata: {json.dumps(unit)}\n\n".encode())
                last_state, wrote = None, time.monotonic()
                while True:
                    with stand_in.lock:
                        if stand_in.down_until_ms is not None or stand_in.replies["/api/v1"]["boot"] != boot:
                            break
                        events = stand_in.events[sent:]
                        sent += len(events)
                        state = {"activity": stand_in.replies["/api/v1/system"]["activity"],
                                 "charge": stand_in.get("/api/v1/charge", {})[1],
                                 "purge": stand_in.replies["/api/v1/purge"]}
                    out = "".join(f"id: {boot}-{event['seq']}\ndata: {json.dumps(event)}\n\n" for event in events)
                    if state != last_state or (not out and time.monotonic() - wrote >= STREAM_KEEP_ALIVE_S):
                        out += f"event: state\ndata: {json.dumps(state)}\n\n"
                        last_state = json.loads(json.dumps(state))
                    if out:
                        self.wfile.write(out.encode())
                        self.wfile.flush()
                        wrote = time.monotonic()
                    time.sleep(0.1)
                # A restart: silent, as a unit gone from the network, until the page gives up on it
                time.sleep(STREAM_KEEP_ALIVE_S + 10)
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass
            finally:
                with stand_in.lock:
                    stand_in.streams -= 1
                self.close_connection = True

        def do_GET(self):
            if self.gone():
                return
            url = urllib.parse.urlsplit(self.path)
            if url.path == "/api/v1/events/stream":
                self.stream(dict(urllib.parse.parse_qsl(url.query)))
                return
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
    parser.add_argument("--hotspot", action="store_true",
                        help="start as a unit never set up, on its own hotspot, as after a factory reset")
    parser.add_argument("--update-ends", choices=ENDS, default="confirmed",
                        help="how an update ends: kept, gone back, refused at its check, or a power cut in its trial")
    args = parser.parse_args()
    if args.record:
        record(args.record)
        return
    stand_in = StandIn(json.loads(DATA.read_text(encoding="utf-8")), args.offer, args.update_ends, args.hotspot)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(stand_in))
    print(f"the pages and {len(stand_in.replies)} recorded paths at http://127.0.0.1:{args.port}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
