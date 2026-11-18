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

    def __init__(self, replies):
        """
        Args:
            replies: the recording, path to reply.
        """
        self.replies = replies
        self.schema = replies["/api/v1/schema"]
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
        if path == "/api/v1/settings":
            reply["resources"] = {inner: self.replies[inner] for inner in reply["resources"]}
        return 200, reply

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
            return 200, {"saved": True}
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
            self.replies[path]["saved"] = False
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
                charge.update(state="idle", in_throw=False)
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
            gate = self.replies["/api/v1/gate"]
            if gate["state"] == "disabled":
                return 409, error("gate_disabled", "the gate is off in its settings")
            gate.update(ratio=body["ratio"], state={0: "open", 1: "closed"}.get(body["ratio"], "partial"))
            return 200, gate
        if path == "/api/v1/scale/zero":
            return 200, {"zeroed": True}
        if path == "/api/v1/system/restart":
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

        def do_GET(self):
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

    return Handler


def main():
    """Record a unit's replies, or serve the pages and the recording."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--record", metavar="HOST", help="record a unit's replies into tests/web_test_data.json")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()
    if args.record:
        record(args.record)
        return
    stand_in = StandIn(json.loads(DATA.read_text(encoding="utf-8")))
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(stand_in))
    print(f"the pages and {len(stand_in.replies)} recorded paths at http://127.0.0.1:{args.port}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
