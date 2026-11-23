"""
A client of the unit's API v1 (ADR-003, gate 4) that works from what the unit says of itself: its index
(GET /api/v1) and its schema (GET /api/v1/schema), not from paths written here. A write or an action is checked
against the schema before it is sent, so a mistake is named without a round trip; the unit checks it again.

    unit = Unit("192.168.2.162")
    unit.read("charge")                                        # GET /api/v1/charge
    unit.write("settings/charge", fine_stop_threshold=0.03)    # PATCH, checked against the resource's fields
    unit.save("settings/charge")                               # POST to the save path the schema names
    unit.act("charge/start", target=30)                        # POST of an action, checked against its fields
    for event in unit.follow(wait_s=30):                       # the event log, from where it left off
        ...

rc_run.py's proofs use it, and an agent can:
    python unit_api.py <host>                  what the unit is, its paths and its actions
    python unit_api.py <host> <path>           a read, as JSON
"""
import json
import math
import re
import sys
import time
import urllib.error
import urllib.request
from typing import NamedTuple

API = "/api/v1"
TIMEOUT_S = 5.0
WAIT_S_MAX = 30         # the event log's longest ?wait (api_v1.md)
EVENT_BASE = {"seq", "at_ms", "source", "kind"}     # what every event carries, beside its kind's own
COLOUR = re.compile(r"^#[0-9a-fA-F]{6}$")


class ApiError(Exception):
    """A reply the unit refused with, in api_v1.md's form: {"error": {"code", "message", "field", ...}}."""

    def __init__(self, method, path, status, reply):
        """
        Args:
            method: the request's method.
            path: the request's path.
            status: the HTTP status.
            reply: the reply's JSON, or its bytes when it was not JSON.
        """
        self.method, self.path, self.status, self.reply = method, path, status, reply
        error = reply.get("error", {}) if isinstance(reply, dict) else {}
        self.code = error.get("code")
        self.field = error.get("field")
        self.message = error.get("message")
        super().__init__(f"{method} {path}: {status} {self.code or ''} {self.message or reply!r}".strip())


class Reply(NamedTuple):
    """A write's or an action's answer: 200 done, 202 under way (a start, a stop, a restart)."""
    status: int
    body: object


def full_path(path):
    """
    The path as the unit serves it.

    Args:
        path: "charge", "/charge", or "/api/v1/charge".

    Returns:
        "/api/v1/charge"; "/api/v1" for "" or "/".
    """
    if path.startswith(API):
        return path
    return API + ("/" + path.lstrip("/") if path.strip("/") else "")


def template_of(path):
    """A path with a number in place of its id, as the schema names it: /api/v1/powder_profiles/3 -> .../{id}."""
    return re.sub(r"/\d+$", "/{id}", path)


def value_problem(field, value):
    """
    Whether a value is one the field takes, by the schema's description of it, as the unit checks it.

    Args:
        field: the field's description from the schema.
        value: the value as it would be sent.

    Returns:
        None when it is taken; else the reason, in the unit's error codes' words ("wrong_type: ...").
    """
    name = field["name"]
    if value is None:
        return None if field.get("nullable") else f"wrong_type: {name} may not be null"
    kind = field["type"]
    if kind in ("number", "integer"):
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            return f"wrong_type: {name} is a number"
        if kind == "integer" and value != math.floor(value):
            return f"wrong_type: {name} is a whole number"
        if "min" in field and value < field["min"]:
            return f"out_of_range: {name} is {field['min']} at least"
        if "max" in field and value > field["max"]:
            return f"out_of_range: {name} is {field['max']} at most"
        return None
    if kind == "boolean":
        return None if isinstance(value, bool) else f"wrong_type: {name} is true or false"
    if kind == "string":
        if not isinstance(value, str):
            return f"wrong_type: {name} is a string"
        size = len(value.encode())
        if size < field.get("min_length", 0) or size > field["max_length"]:
            return f"out_of_range: {name} is {field.get('min_length', 0)} to {field['max_length']} bytes"
        return None
    if kind == "colour":
        return None if isinstance(value, str) and COLOUR.match(value) else f"wrong_type: {name} is \"#rrggbb\""
    if kind == "choice":
        values = [choice["value"] for choice in field["choices"]]
        return None if value in values else f"not_a_choice: {name} is one of {values}"
    return f"wrong_type: {name} is of a type this client does not know ({kind})"


def fields_problems(fields, values, writing):
    """
    Whether a body is one a resource or an action takes, by its fields' descriptions.

    Args:
        fields: the schema's fields of the resource or the action.
        values: the body, a dict.
        writing: True for a body to send (a read-only field is refused, a required one must be there); False for a
            reply read back (a write-only field must not be there, `saved` is the resources').

    Returns:
        The problems found, each a line; empty when there is none.
    """
    by_name = {field["name"]: field for field in fields}
    problems = []
    for name, value in values.items():
        field = by_name.get(name)
        if field is None:
            if not (not writing and name == "saved"):
                problems.append(f"unknown_field: {name}")
            continue
        access = field.get("access", "read_write")
        if writing and access == "read":
            problems.append(f"read_only: {name}")
        elif not writing and access == "write_only":
            problems.append(f"{name} is write-only, and was read")
        problem = value_problem(field, value)
        if problem is not None:
            problems.append(problem)
    for field in fields:
        if writing and field.get("required") and field["name"] not in values:
            problems.append(f"bad_request: {field['name']} is required")
        if not writing and field.get("access", "read_write") != "write_only" and field["name"] not in values:
            problems.append(f"{field['name']} is missing")
    return problems


class Unit:
    """One unit, reached by its address or host name, described by its own index and schema."""

    def __init__(self, host, timeout_s=TIMEOUT_S):
        """
        Args:
            host: the unit's address or host name.
            timeout_s: each request's time limit; a held read of the event log adds its wait.
        """
        self.host = host
        self.timeout_s = timeout_s
        self._index = None
        self._schema = None
        self.boot = None        # the event log's boot when last read: a change is a restart
        self.after = 0          # the last event's number read

    # --- Requests

    def request(self, method, path, body=None, timeout_s=None, data=None, content_type="application/json"):
        """
        One request, as given: no check against the schema.

        Args:
            method: "GET", "PATCH", "POST", "PUT".
            path: as full_path takes it, with its query.
            body: a JSON body, or None.
            timeout_s: the time limit, or None for the unit's.
            data: bytes to send in place of a JSON body.
            content_type: the bytes' type.

        Returns:
            (status, the reply: its JSON, or its bytes when it is not JSON).

        Raises:
            OSError: no answer (a timeout, a reset, a refusal).
        """
        if body is not None:
            data = json.dumps(body).encode()
        request = urllib.request.Request(f"http://{self.host}{full_path(path)}", data=data, method=method)
        if data is not None:
            request.add_header("Content-Type", content_type)
        try:
            with urllib.request.urlopen(request, timeout=timeout_s or self.timeout_s) as reply:
                status, raw = reply.status, reply.read()
                kind = reply.headers.get_content_type()
        except urllib.error.HTTPError as error:
            status, raw, kind = error.code, error.read(), error.headers.get_content_type()
        if kind != "application/json":
            return status, raw
        return status, json.loads(raw)

    def _answer(self, method, path, body=None):
        """A request whose refusal raises ApiError; its Reply when it is taken."""
        status, reply = self.request(method, path, body)
        if status >= 400:
            raise ApiError(method, full_path(path), status, reply)
        return Reply(status, reply)

    # --- What the unit says of itself

    @property
    def index(self):
        """GET /api/v1, read once: the firmware, the board, the unit's id and name, the guide, the paths."""
        if self._index is None:
            self._index = self.read("")
        return self._index

    @property
    def schema(self):
        """GET /api/v1/schema, read once: every resource's fields, every action's, the states, events and codes."""
        if self._schema is None:
            self._schema = self.read("schema")
        return self._schema

    def forget(self):
        """Read the index and the schema again on their next use: after a flash, the firmware's may differ."""
        self._index = None
        self._schema = None

    def paths(self, method="GET"):
        """
        The paths the index lists for a method, with each {id} as the schema gives its ids.

        Args:
            method: the method.

        Returns:
            The full paths, in the index's order.
        """
        found = []
        for entry in self.index["resources"]:
            if method not in entry["methods"]:
                continue
            resource = self.resource(entry["path"])
            if "{id}" in entry["path"]:
                found.extend(entry["path"].replace("{id}", str(id_)) for id_ in self.ids(entry["path"], resource))
            else:
                found.append(entry["path"])
        return found

    def ids(self, path, resource=None):
        """
        The ids a path's {id} takes now: a settings resource's from the schema; a recording's from the list the unit
        keeps, and for its samples only those whose samples are kept (ADR-009).

        Args:
            path: the path with {id}.
            resource: its settings resource from the schema, if it is one.

        Returns:
            The ids.
        """
        if resource is not None:
            return resource["ids"]
        if full_path(path).startswith(full_path("recordings/{id}")):
            recordings = self.read("recordings")["recordings"]
            if path.endswith("/samples"):
                return [entry["id"] for entry in recordings if entry["samples_kept"]]
            return [entry["id"] for entry in recordings]
        return []

    def resource(self, path):
        """
        A settings resource's description from the schema.

        Args:
            path: its path, with an id or with {id}.

        Returns:
            The description, or None when the path is not a settings resource.
        """
        wanted = template_of(full_path(path))
        for resource in self.schema["resources"]:
            if resource["path"] == wanted:
                return resource
        return None

    def action(self, method, path):
        """
        An action's description from the schema: its method, path, body's fields, refused_while and about.

        Args:
            method: "POST" or "PATCH".
            path: its path.

        Returns:
            The description, or None when the schema has no such action.
        """
        wanted = full_path(path)
        for action in self.schema["actions"]:
            if action["method"] == method and action["path"] == wanted:
                return action
        return None

    def states(self, kind):
        """The values a kind of state takes (charge, purge, gate, activity, driver), by the schema."""
        return [state["value"] for state in self.schema["states"][kind]]

    # --- Reads, writes, actions

    def read(self, path, **query):
        """
        A GET, which changes nothing on the unit.

        Args:
            path: as full_path takes it.
            **query: the query's names and values.

        Returns:
            The reply's JSON, or its bytes (the display's frame).

        Raises:
            ApiError: the unit refused it.
        """
        if query:
            path += "?" + "&".join(f"{name}={value}" for name, value in query.items())
        status, reply = self.request("GET", path)
        if status != 200:
            raise ApiError("GET", full_path(path), status, reply)
        return reply

    def write(self, path, **fields):
        """
        A PATCH: a settings resource's fields, or the fields of a resource's PATCH action (the charge's target,
        the purge's speed, the gate's ratio), each checked against the schema first. Live, not stored: save stores.

        Args:
            path: the resource's path.
            **fields: the fields to write.

        Returns:
            The Reply: the resource as it is after the write.

        Raises:
            ValueError: the schema says the unit would refuse it; nothing was sent.
            ApiError: the unit refused it (busy in a throw, a limit that crosses fields).
        """
        resource = self.resource(path)
        described = resource["fields"] if resource is not None else self._action_fields("PATCH", path)
        problems = fields_problems(described, fields, writing=True)
        if problems:
            raise ValueError(f"PATCH {full_path(path)}: " + "; ".join(problems))
        return self._answer("PATCH", path, fields)

    def act(self, path, **fields):
        """
        A POST of an action, its body checked against the action's fields in the schema first.

        Args:
            path: the action's path ("charge/start", "stop", "system/restart").
            **fields: its body's fields.

        Returns:
            The Reply: 202 when the action goes on after the reply, 200 when it is done (a stop with nothing to
            stop), and the state it led to.

        Raises:
            ValueError: no such action, or the schema says the unit would refuse the body; nothing was sent.
            ApiError: the unit refused it (busy, no target, a driver's fault).
        """
        problems = fields_problems(self._action_fields("POST", path), fields, writing=True)
        if problems:
            raise ValueError(f"POST {full_path(path)}: " + "; ".join(problems))
        return self._answer("POST", path, fields)

    def save(self, path):
        """
        Store a settings resource, at the save path the schema names for it.

        Args:
            path: the resource's path.

        Returns:
            The Reply.
        """
        resource = self.resource(path)
        if resource is None:
            raise ValueError(f"{full_path(path)} is no settings resource of this unit's")
        return self._answer("POST", resource["save"], {})

    def _action_fields(self, method, path):
        """An action's fields from the schema; ValueError when the schema has no such action."""
        action = self.action(method, path)
        if action is None:
            raise ValueError(f"{method} {full_path(path)} is no action of this unit's")
        return action["fields"]

    # --- The event log

    def events(self, after=None, wait_s=None):
        """
        One read of the event log, held up to wait_s for an event after the number given.

        Args:
            after: the number to read after; None for where this client left off.
            wait_s: 1 to 30 to hold the read until there is one; None to answer at once.

        Returns:
            The log's reply: boot, now_ms, first, next, events.
        """
        query = {"after": self.after if after is None else after}
        if wait_s is not None:
            query["wait"] = min(max(1, int(wait_s)), WAIT_S_MAX)
        path = "events?" + "&".join(f"{name}={value}" for name, value in query.items())
        status, reply = self.request("GET", path, timeout_s=self.timeout_s + (wait_s or 0))
        if status != 200:
            raise ApiError("GET", full_path(path), status, reply)
        return reply

    def follow(self, wait_s=WAIT_S_MAX, until=None, for_s=None):
        """
        The unit's events, each once, in order, from where this client left off: held reads, one after another.
        A restart (another boot) starts again at the new log's first event.

        Args:
            wait_s: each read's hold.
            until: a function of an event; the events end after the first it is true for. None: no such end.
            for_s: the events end once this long has passed, at the end of a read. None: no such end.

        Yields:
            Each event, a dict with seq, at_ms, source, kind and what its kind carries.
        """
        deadline = None if for_s is None else time.monotonic() + for_s
        while deadline is None or time.monotonic() < deadline:
            reply = self.events(wait_s=wait_s)
            if self.boot is not None and reply["boot"] != self.boot:
                self.after = 0
                self.boot = reply["boot"]
                reply = self.events()
            self.boot = reply["boot"]
            for event in reply["events"]:
                self.after = event["seq"]
                yield event
                if until is not None and until(event):
                    return
            self.after = max(self.after, reply["next"])

    def start_following(self):
        """Leave the log's past behind: the next follow() begins with the next event."""
        reply = self.events(after=0)
        self.boot = reply["boot"]
        self.after = reply["next"]

    def event_problems(self, event):
        """
        Whether an event is one the schema lists, and carries no more than its kind's own.

        Args:
            event: one event as read.

        Returns:
            The problems found; empty when there is none.
        """
        for kind in self.schema["events"]:
            if kind["source"] == event["source"] and kind["kind"] == event["kind"]:
                extra = set(event) - EVENT_BASE - set(kind["carries"])
                missing = EVENT_BASE - set(event)
                return ([f"carries {sorted(extra)}, not in the schema"] if extra else []) + \
                       ([f"lacks {sorted(missing)}"] if missing else [])
        return [f"{event['source']} {event['kind']} is no kind the schema lists"]


def main():
    """Print what a unit is and what it takes, or one read: python unit_api.py <host> [path]."""
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    unit = Unit(sys.argv[1])
    if len(sys.argv) > 2:
        reply = unit.read(sys.argv[2])
        print(json.dumps(reply, indent=2) if not isinstance(reply, bytes) else f"{len(reply)} bytes")
        return
    index = unit.index
    print(f"{index['hostname']}: {index['board']}, firmware {index['firmware']['version']}, unit {index['unit_id']}")
    print(index["guide"])
    for entry in index["resources"]:
        print(f"  {' '.join(entry['methods']):12} {entry['path']}: {entry['about']}")
    print("actions:")
    for action in unit.schema["actions"]:
        names = ", ".join(field["name"] for field in action["fields"]) or "{}"
        print(f"  {action['method']:6} {action['path']} ({names}): {action.get('about', '')}")


if __name__ == "__main__":
    main()
