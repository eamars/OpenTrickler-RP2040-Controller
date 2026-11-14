"""
The remote control's host runner (ADR-001, step 3).

Reaches a unit through a debug probe on its SWD port: openocd flashes and resets the chip, and openocd's RTT
server carries the unit's console. Every session is kept in logs/rc/<time>_<command>.log. A run ends with
`VERDICT PASS` or `VERDICT FAIL <reasons>` and exits 0 or 1.

Counted as failures: a console line with `assertion` or `PANIC`, a command with no reply within 5 s, an `@err`
reply, `up=` going backwards when no restart was asked for, and a processor core found halted.

Usage, from the repository root:
    venv\\Scripts\\python tools\\remote_control\\rc_run.py send "state" ["stats" ...]
    venv\\Scripts\\python tools\\remote_control\\rc_run.py flash [--elf build_w\\app.elf]
    venv\\Scripts\\python tools\\remote_control\\rc_run.py reset
    venv\\Scripts\\python tools\\remote_control\\rc_run.py watch 30
    venv\\Scripts\\python tools\\remote_control\\rc_run.py snap
    venv\\Scripts\\python tools\\remote_control\\rc_run.py menu_tour [--save <folder>] [--compare <folder>]
    venv\\Scripts\\python tools\\remote_control\\rc_run.py settings --host <address> [--save <folder>] [--compare <folder>]
    venv\\Scripts\\python tools\\remote_control\\rc_run.py soak 3600 --host <address>
"""
import argparse
import base64
import datetime
import json
import queue
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import urllib.request
import zlib
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
PICO_SDK_ROOT = Path.home() / ".pico-sdk"
OPENOCD_DIR = PICO_SDK_ROOT / "openocd" / "0.12.0+dev"
TOOLCHAIN_BIN = PICO_SDK_ROOT / "toolchain" / "14_2_Rel1" / "bin"
LOG_DIR = REPO / "logs" / "rc"

# reset: the writes that make the watchdog reset the whole chip, as watchdog_reboot() does: scratch 4 cleared
# (a normal boot), every power domain but the oscillators selected, then the trigger. openocd's own `reset run`
# left core 0 locked up in a double fault on the bench's RP2040 (ADR-001 progress), and resets less of the chip.
# step_status: the IO_BANK0 GPIOx_STATUS register of each trickler's STEP pin (GP3, GP8); its OUTTOPAD bit is
# the level driven onto the pin. A stepper's pulse is low for 13 PIO cycles (about 100 ns) and high for the rest
# of its period, so a running motor samples almost always high and a stopped one always low.
BOARDS = {
    "pico_w": {
        "target": "target/rp2040.cfg", "build": "build_w", "cores": ["rp2040.core0", "rp2040.core1"],
        "reset": ["mww 0x4005801c 0", "mww 0x40010008 0x1fffc", "mww 0x40058000 0x80000000"],
        "step_status": {"coarse": 0x40014018, "fine": 0x40014040},
    },
    "pico2_w": {
        "target": "target/rp2350.cfg", "build": "build", "cores": ["rp2350.cm0", "rp2350.cm1"],
        "reset": None,  # not yet worked out for the RP2350: no Pico 2 W on the bench
        "step_status": None,
    },
}
GPIO_STATUS_OUTTOPAD = 0x200
STEP_SAMPLES = 200
STEP_RUNNING_FRACTION = 0.5     # a pin high in more than this share of the samples is pulsing

CHARGE_POLL_S = 0.25
CHARGE_STATE_NAMES = {0: "exit", 1: "waiting for zero", 2: "charging", 3: "waiting for cup removal",
                      4: "waiting for cup return"}
CHARGE_CYCLE_ORDER = [1, 2, 3, 4, 1]    # a charge, the cup off and back (by the simulated scale), the next charge

TCL_PORT = 6666
RTT_PORT = 19021
REPLY_TIMEOUT_S = 5.0
CONSOLE_TIMEOUT_S = 20.0    # from a reset until the console answers
TCL_TERMINATOR = b"\x1a"
RTT_SEARCH_SIZE = 0x400     # bytes searched for the control block, from the address the ELF gives
RTT_POLL_MS = 10            # how often openocd reads the RTT buffers (its default is 100 ms)

SNAP_TRIES = 4              # snaps taken until two in a row are equal
PNG_SCALE = 4               # each display pixel is 4 x 4 in the PNG
MENU_TOUR_IDS = range(1, 100)
# Forms that show values that change from build to build or unit to unit: reported, not judged, by --compare
MENU_TOUR_LIVE_FORMS = {36: "version info: the version, hash and build of the image"}

# The web API paths `settings` reads. Each answers a GET with no parameters with what is stored, and does nothing
# else; a parameter can change the unit (AGENTS.md), so none is sent. profile_config with no parameter gives the
# selected profile, and profile_summary every profile's name.
SETTINGS_PATHS = ["charge_mode_config", "coarse_motor_config", "fine_motor_config", "mini_12864_config",
                  "neopixel_led_config", "profile_config", "profile_summary", "scale_config", "servo_gate_config",
                  "system_control", "wireless_config"]
# Fields that change with the image, not with the stored settings: reported, not judged, by --compare
SETTINGS_LIVE_FIELDS = {"system_control": {"s1": "the version", "s2": "the vcs hash", "s3": "the build type"}}
HTTP_TIMEOUT_S = 5.0

MIRROR_BYTES = 128 * 64 // 8    # the 12864 display's buffer, one bit a pixel
SOAK_POLL_S = 1.0           # the mirror page reads the screen once a second
SOAK_STATS_S = 60.0         # state and stats once a minute
SOAK_SETTLE_S = 300.0       # heap_free may not end lower than its lowest in the first five minutes


class SessionLog:
    """One session's log: every line, timestamped, to the file and to stdout."""

    def __init__(self, name):
        """
        Open logs/rc/<time>_<name>.log.

        Args:
            name: what the session runs, for the file name.
        """
        LOG_DIR.mkdir(parents=True, exist_ok=True)
        stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.path = LOG_DIR / f"{stamp}_{name}.log"
        self._file = open(self.path, "w", encoding="utf-8")
        self._lock = threading.Lock()

    def write(self, source, text):
        """
        Write one line.

        Args:
            source: where the line came from ("unit", "openocd", "rc").
            text: the line, without its line ending.
        """
        stamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        line = f"{stamp} {source:7} {text}"
        with self._lock:
            self._file.write(line + "\n")
            self._file.flush()
            if not text.startswith("@sd "):     # a snap's data: in the file, not on the screen
                print(line, flush=True)

    def close(self):
        self._file.close()


def toolchain_tool(name):
    """
    Find one of the compiler's tools (nm, addr2line).

    Args:
        name: the tool's name without the arm-none-eabi- prefix.

    Returns:
        The tool's path.
    """
    candidate = TOOLCHAIN_BIN / f"arm-none-eabi-{name}.exe"
    if candidate.exists():
        return str(candidate)
    found = shutil.which(f"arm-none-eabi-{name}")
    if found is None:
        raise FileNotFoundError(f"arm-none-eabi-{name} is not in {TOOLCHAIN_BIN} nor on PATH")
    return found


def elf_symbol_address(elf, symbol):
    """
    Read a symbol's address from an ELF.

    Args:
        elf: the ELF's path.
        symbol: the symbol's name.

    Returns:
        The address, or None when the ELF has no such symbol.
    """
    output = subprocess.run([toolchain_tool("nm"), str(elf)], capture_output=True, text=True, check=True).stdout
    for line in output.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == symbol:
            return int(parts[0], 16)
    return None


def elf_function_at(elf, address):
    """
    Name the function holding an address.

    Args:
        elf: the ELF's path.
        address: the code address.

    Returns:
        "function at file:line" as addr2line gives it.
    """
    output = subprocess.run([toolchain_tool("addr2line"), "-f", "-e", str(elf), hex(address)],
                            capture_output=True, text=True, check=True).stdout.split()
    return " at ".join(output)


def snap_rows(data_lines):
    """
    Decode a snap: the `@snap <w> <h> u8g2 r=<rotation>` line and its `@sd <base64>` lines.

    u8g2's buffer holds the panel as it is mounted: rows of 8 pixels, one byte per column, lowest bit on top.
    The rotation u8g2 draws with is undone for R0 and R2 (the 180 degrees of a mini 12864 mounted upside down),
    so the picture is what a person at the unit sees.

    Args:
        data_lines: the lines the snap command sent before its reply.

    Returns:
        The rows, top first, each a list of bools (True for a lit pixel), or None if the snap is incomplete.
    """
    if not data_lines:
        return None
    match = re.match(r"@snap (\d+) (\d+) u8g2 r=(\d)", data_lines[0])
    if not match:
        return None
    width, height, rotation = (int(group) for group in match.groups())
    payload = b"".join(base64.b64decode(line[4:]) for line in data_lines[1:] if line.startswith("@sd "))
    if len(payload) != width * height // 8:
        return None
    rows = [[bool(payload[(y // 8) * width + x] >> (y % 8) & 1) for x in range(width)] for y in range(height)]
    if rotation == 2:
        rows = [row[::-1] for row in rows[::-1]]
    return rows


def write_png(path, rows):
    """
    Write rows of pixels as a 1-bit PNG, lit pixels dark as on the display. The same rows always give the same
    bytes, so two pictures compare by their files.

    Args:
        path: the PNG's path.
        rows: as snap_rows() gives them.
    """
    width = len(rows[0]) * PNG_SCALE
    raw = bytearray()
    for row in rows:
        bits = []
        for lit in row:
            bits.extend([0 if lit else 1] * PNG_SCALE)
        line = bytearray([0])   # filter type: none
        for idx in range(0, width, 8):
            byte = 0
            for bit in bits[idx:idx + 8]:
                byte = (byte << 1) | bit
            line.append(byte)
        raw += line * PNG_SCALE

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    header = struct.pack(">IIBBBBB", width, len(rows) * PNG_SCALE, 1, 0, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) \
        + chunk(b"IEND", b"")
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(png)


def http_get(host, path):
    """
    GET one path of the unit's web API.

    Args:
        host: the unit's address.
        path: the path, without its leading slash.

    Returns:
        The body, as bytes.
    """
    with urllib.request.urlopen(f"http://{host}/{path}", timeout=HTTP_TIMEOUT_S) as response:
        return response.read()


def read_mirror(host):
    """
    Read the screen's buffer as the mirror page does. /display_buffer answers with the bare buffer and no HTTP
    header (its handler says the header is included, and it is not), so it is read over a plain socket.

    Args:
        host: the unit's address.

    Returns:
        The bytes until the unit closed the connection.
    """
    with socket.create_connection((host, 80), timeout=HTTP_TIMEOUT_S) as sock:
        sock.sendall(b"GET /display_buffer HTTP/1.0\r\n\r\n")
        data = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                return data
            data += chunk


def build_version(build_dir):
    """
    Read the version the last build wrote into src/generated/version.c.

    Args:
        build_dir: unused today; src/generated is shared by both boards' builds, so build the board last.

    Returns:
        (version_string, vcs_hash).
    """
    text = (REPO / "src" / "generated" / "version.c").read_text(encoding="utf-8")
    version = re.search(r'version_string = "([^"]*)"', text).group(1)
    vcs = re.search(r'vcs_hash = "([^"]*)"', text).group(1)
    return version, vcs


class Openocd:
    """One openocd process for the probe and the chip, driven through its Tcl port."""

    def __init__(self, board, log):
        """
        Start openocd and connect to its Tcl port. The chip is left running.

        Args:
            board: an entry of BOARDS.
            log: the SessionLog.
        """
        self.board = board
        self.log = log
        self.output = []    # every line openocd printed, for checks such as "Verified OK"
        exe = OPENOCD_DIR / "openocd.exe"
        command = [
            str(exe), "-s", str(OPENOCD_DIR / "scripts"),
            "-f", "interface/cmsis-dap.cfg", "-f", board["target"],
            "-c", "adapter speed 5000",
            "-c", "gdb port disabled", "-c", "telnet port disabled", "-c", f"tcl port {TCL_PORT}",
            "-c", "init",
        ]
        self.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                        errors="replace")
        threading.Thread(target=self._read_output, daemon=True).start()
        self.sock = self._connect()

    def _read_output(self):
        for line in self.process.stdout:
            line = line.rstrip()
            if line.startswith("Warn : ***"):
                continue    # the old-debugprobe banner, on every start
            self.output.append(line)
            self.log.write("openocd", line)

    def _connect(self):
        deadline = time.monotonic() + 15
        while True:
            if self.process.poll() is not None:
                raise RuntimeError("openocd stopped before its Tcl port opened; see the log")
            try:
                return socket.create_connection(("127.0.0.1", TCL_PORT), timeout=60)
            except ConnectionRefusedError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.2)

    def cmd(self, text):
        """
        Run one openocd command.

        Args:
            text: the command.

        Returns:
            Its result text.
        """
        self.sock.sendall(text.encode() + TCL_TERMINATOR)
        data = b""
        while not data.endswith(TCL_TERMINATOR):
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("openocd closed its Tcl port")
            data += chunk
        return data[:-1].decode(errors="replace")

    def rtt_start(self, elf):
        """
        (Re)start RTT on the control block the ELF names, and its TCP server.

        Args:
            elf: the ELF running on the unit, for the control block's address.
        """
        address = elf_symbol_address(elf, "_SEGGER_RTT")
        if address is None:
            raise RuntimeError(f"{elf} has no _SEGGER_RTT: is it a build with the RTT console?")
        self.cmd("rtt stop")
        self.cmd(f'rtt setup {address:#x} {RTT_SEARCH_SIZE:#x} "SEGGER RTT"')
        self.cmd("rtt start")
        self.cmd(f"rtt polling_interval {RTT_POLL_MS}")
        if not getattr(self, "_rtt_server", False):
            self.cmd(f"rtt server start {RTT_PORT} 0")
            self._rtt_server = True

    def sample_word(self, address, count):
        """
        Read one 32-bit word many times in a row, in one openocd call, with the chip running.

        Args:
            address: the word's address.
            count: how many reads.

        Returns:
            The values read.
        """
        script = (f"set r {{}}; for {{set i 0}} {{$i < {count}}} {{incr i}} "
                  f"{{ lappend r [read_memory {address:#x} 32 1] }}; return $r")
        return [int(value, 0) for value in self.cmd(script).split()]

    def halted_cores(self, elf):
        """
        Find cores that are not running.

        Args:
            elf: for naming the function a halted core is in.

        Returns:
            One description per core that is not running.
        """
        found = []
        for core in self.board["cores"]:
            state = self.cmd(f"{core} curstate").strip()
            if state == "running":
                continue
            description = f"{core} {state}"
            if state == "halted":
                self.cmd(f"targets {core}")
                pc_text = self.cmd("reg pc")
                match = re.search(r"0x([0-9a-fA-F]+)", pc_text)
                if match:
                    pc = int(match.group(1), 16)
                    description += f" at pc={pc:#010x} ({elf_function_at(elf, pc)})"
            found.append(description)
        return found

    def close(self):
        """Stop openocd. The chip is left as it is."""
        try:
            self.cmd("shutdown")
        except ConnectionError:
            pass    # it may close the port before it answers
        self.sock.close()
        self.process.wait(timeout=10)


class Console:
    """The unit's console, through openocd's RTT server."""

    def __init__(self, log, failures):
        """
        Connect to the RTT server and start reading lines.

        Args:
            log: the SessionLog.
            failures: the run's list of failure reasons, appended to when a line shows one.
        """
        self.log = log
        self.failures = failures
        self.expect_fault = False   # set while a test build is meant to assert: its line is then not a failure
        self.lines = queue.Queue()
        self.sock = socket.create_connection(("127.0.0.1", RTT_PORT), timeout=10)
        self.sock.settimeout(None)
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        buffer = b""
        while True:
            try:
                chunk = self.sock.recv(4096)
            except OSError:
                return
            if not chunk:
                return
            buffer += chunk
            *complete, buffer = buffer.split(b"\n")
            for raw in complete:
                line = raw.decode(errors="replace").rstrip("\r")
                self.log.write("unit", line)
                if ("assertion" in line.lower() or "PANIC" in line) and not self.expect_fault:
                    self.failures.append(f"console: {line}")
                self.lines.put(line)

    def drain(self):
        """Forget lines already read."""
        while not self.lines.empty():
            self.lines.get_nowait()

    def command(self, line, timeout=REPLY_TIMEOUT_S):
        """
        Send one command line and wait for its reply.

        Args:
            line: the command line.
            timeout: seconds to wait.

        Returns:
            The reply line, or None if none came in time.
        """
        name = line.split()[0]
        self.drain()
        self.data_lines = []    # the `@` lines the command sent before its reply (a snap's)
        self.log.write("rc", f"> {line}")
        self.sock.sendall(line.encode() + b"\n")
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            try:
                reply = self.lines.get(timeout=remaining)
            except queue.Empty:
                return None
            if reply.startswith(f"@ok {name}") or reply.startswith("@err"):
                return reply
            if reply.startswith("@") and not reply.startswith("@hello"):
                self.data_lines.append(reply)

    def close(self):
        self.sock.close()


class Session:
    """One run: openocd, the console, the failures counted, and the verdict."""

    def __init__(self, args, name):
        self.board = BOARDS[args.board]
        self.elf = Path(args.elf) if args.elf else REPO / self.board["build"] / "app.elf"
        self.log = SessionLog(name)
        self.failures = []
        self.last_up = None
        self.log.write("rc", f"board {args.board}, elf {self.elf}")
        self.openocd = Openocd(self.board, self.log)
        self.openocd.rtt_start(self.elf)
        self.console = Console(self.log, self.failures)

    def fail(self, reason):
        self.log.write("rc", f"FAIL {reason}")
        self.failures.append(reason)

    def check_up(self, reply):
        """Count `up=` going backwards in a state reply as a failure (a restart nobody asked for)."""
        match = re.search(r"\bup=(\d+)", reply)
        if not match:
            return
        up = int(match.group(1))
        if self.last_up is not None and up < self.last_up:
            self.fail(f"the unit restarted unasked (up={self.last_up} then up={up})")
        self.last_up = up

    def send(self, line):
        """
        Send one command and judge its reply.

        Returns:
            The reply, or None.
        """
        reply = self.console.command(line)
        if reply is None:
            self.fail(f"no reply to \"{line}\" within {REPLY_TIMEOUT_S:.0f} s")
            return None
        if reply.startswith("@err"):
            self.fail(f"\"{line}\": {reply}")
        if line.split()[0] == "state":
            self.check_up(reply)
        if line.split()[0] == "reboot" and reply.startswith("@ok"):
            time.sleep(1.5)
            self.wait_for_console()
        return reply

    def wait_for_console(self):
        """
        After a restart: restart RTT and ask `state` until the unit answers.

        Returns:
            The first state reply, or None.
        """
        self.last_up = None
        deadline = time.monotonic() + CONSOLE_TIMEOUT_S
        while time.monotonic() < deadline:
            self.openocd.rtt_start(self.elf)
            reply = self.console.command("state", timeout=1.0)
            if reply is not None:
                self.check_up(reply)
                return reply
        self.fail(f"the console did not answer within {CONSOLE_TIMEOUT_S:.0f} s of a restart")
        return None

    def flash(self, elf=None):
        """
        Write an ELF through the probe, start it, and check the unit runs the build's version.

        Args:
            elf: the ELF to write; by default the session's. It becomes the session's ELF.
        """
        if elf is not None:
            self.elf = Path(elf)
        if not self.elf.exists():
            self.fail(f"{self.elf} does not exist")
            return
        started = time.monotonic()
        done_before = len(self.openocd.output)
        self.openocd.cmd(f"program {{{self.elf.as_posix()}}} verify reset")
        if not any("Verified OK" in line for line in self.openocd.output[done_before:]):
            self.fail("openocd did not verify the image")
            return
        self.log.write("rc", f"flashed and verified in {time.monotonic() - started:.1f} s")
        reply = self.wait_for_console()
        if reply is None:
            return
        version, vcs = build_version(self.board["build"])
        if f"ver={version} " not in reply or f"vcs={vcs} " not in reply:
            self.fail(f"the unit does not run the build: expected ver={version} vcs={vcs}, got: {reply}")
        else:
            self.log.write("rc", f"the unit runs {version} at {vcs}, {time.monotonic() - started:.1f} s after the start")

    def reset(self):
        """Reset the whole chip by its watchdog, through the probe, and wait for the console."""
        if self.board["reset"] is None:
            self.fail("reset is not worked out for this board")
            return
        for write in self.board["reset"]:
            self.openocd.cmd(write)
        time.sleep(1.0)
        self.wait_for_console()

    def snap(self):
        """
        Take snaps until two in a row are equal: a single copy of the buffer can catch a frame half drawn.

        Returns:
            The rows, as snap_rows() gives them, or None (counted as a failure).
        """
        previous = None
        for _ in range(SNAP_TRIES):
            reply = self.console.command("snap")
            if reply is None or not reply.startswith("@ok"):
                self.fail(f"snap: {reply or 'no reply'}")
                return None
            rows = snap_rows(self.console.data_lines)
            if rows is not None and rows == previous:
                return rows
            previous = rows
        self.fail(f"no two snaps in a row were equal in {SNAP_TRIES} tries")
        return None

    def save_snap(self, path):
        """Take a snap and save it as a PNG. Returns the path, or None."""
        rows = self.snap()
        if rows is None:
            return None
        write_png(path, rows)
        self.log.write("rc", f"snap saved: {path}")
        return path

    def menu_tour(self, save_dir, compare_dir):
        """
        Show every form in turn and snap it, pressing nothing, so no setting changes and nothing starts.

        Args:
            save_dir: where the pictures go (default: logs/rc/snaps/<session>/).
            compare_dir: a folder of earlier pictures; a form missing from either side, or a picture that
                differs, fails, except the forms in MENU_TOUR_LIVE_FORMS, which are reported.
        """
        reply = self.console.command("form")
        if reply is None or not reply.startswith("@ok form=") or int(reply.split("=")[1]) < 0:
            self.fail(f"the menu does not have the screen: {reply}")
            return
        out_dir = Path(save_dir) if save_dir else LOG_DIR / "snaps" / self.log.path.stem
        found = []
        for form_id in MENU_TOUR_IDS:
            reply = self.console.command(f"form {form_id}")
            if reply == "@err no such form":
                continue
            if reply is None or not reply.startswith("@ok"):
                self.fail(f"form {form_id}: {reply or 'no reply'}")
                continue
            if self.save_snap(out_dir / f"form_{form_id}.png") is not None:
                found.append(form_id)
        self.console.command("form 1")
        self.log.write("rc", f"menu_tour: {len(found)} forms: {' '.join(str(form_id) for form_id in found)}")
        self.log.write("rc", f"menu_tour: pictures in {out_dir}")

        if compare_dir is None:
            return
        compare_dir = Path(compare_dir)
        baseline = {int(path.stem.split("_")[1]) for path in compare_dir.glob("form_*.png")}
        for form_id in sorted(baseline - set(found)):
            self.fail(f"form {form_id} is in {compare_dir} and was not found")
        for form_id in found:
            reference = compare_dir / f"form_{form_id}.png"
            if not reference.exists():
                self.fail(f"form {form_id} is new: not in {compare_dir}")
                continue
            if reference.read_bytes() == (out_dir / f"form_{form_id}.png").read_bytes():
                continue
            if form_id in MENU_TOUR_LIVE_FORMS:
                self.log.write("rc", f"form {form_id} differs, not judged ({MENU_TOUR_LIVE_FORMS[form_id]})")
            else:
                self.fail(f"form {form_id} differs from {reference}")

    def state(self):
        """
        Ask `state` and parse it.

        Returns:
            Its fields as a dict of strings, or None (counted as a failure).
        """
        reply = self.send("state")
        if reply is None or not reply.startswith("@ok state"):
            return None
        return dict(re.findall(r'(\w+)=("[^"]*"|\S+)', reply))

    def stats(self):
        """
        Ask `stats` and parse its numbers.

        Returns:
            heap_free, heap_min and tasks as ints in a dict, or None (counted as a failure).
        """
        reply = self.send("stats")
        if reply is None or not reply.startswith("@ok stats"):
            return None
        return {key: int(value) for key, value in re.findall(r"\b(heap_free|heap_min|tasks)=(\d+)", reply)}

    def settings(self, host, save_dir, compare_dir):
        """
        Read the stored settings over the web API, keep them as JSON, and compare them with an earlier copy.

        Args:
            host: the unit's address.
            save_dir: where the copy goes (default: logs/rc/settings/<session>/).
            compare_dir: an earlier copy; a path missing on either side or a field that differs fails, except
                the fields in SETTINGS_LIVE_FIELDS, which are reported.
        """
        out_dir = Path(save_dir) if save_dir else LOG_DIR / "settings" / self.log.path.stem
        out_dir.mkdir(parents=True, exist_ok=True)
        read = {}
        for name in SETTINGS_PATHS:
            try:
                read[name] = json.loads(http_get(host, f"rest/{name}"))
            except (OSError, ValueError) as error:
                self.fail(f"settings: rest/{name}: {error}")
                continue
            (out_dir / f"{name}.json").write_text(json.dumps(read[name], indent=1, sort_keys=True) + "\n",
                                                  encoding="utf-8")
        self.log.write("rc", f"settings: {len(read)} of {len(SETTINGS_PATHS)} paths read, kept in {out_dir}")

        if compare_dir is None:
            return
        for name in SETTINGS_PATHS:
            reference = Path(compare_dir) / f"{name}.json"
            if not reference.exists():
                self.fail(f"settings: {reference} does not exist")
                continue
            if name not in read:
                continue
            before = json.loads(reference.read_text(encoding="utf-8"))
            for key in sorted(set(before) | set(read[name])):
                old, new = before.get(key), read[name].get(key)
                if old == new:
                    continue
                live = SETTINGS_LIVE_FIELDS.get(name, {}).get(key)
                if live:
                    self.log.write("rc", f"settings: {name} {key} differs, not judged ({live}): {old!r} -> {new!r}")
                else:
                    self.fail(f"settings: {name} {key} was {old!r}, is {new!r}")

    def soak(self, host, seconds):
        """
        Read the screen's mirror over the web once a second, as its page does, for a time; `state` and `stats`
        once a minute. Fails on a read with no answer, on a restart (`up=` going backwards), and when heap_free
        ends lower than its lowest in the first SOAK_SETTLE_S: memory taken and not given back.

        Args:
            host: the unit's address.
            seconds: how long.
        """
        started = time.monotonic()
        next_stats = started
        reads = 0
        misses = 0
        heap = []   # (seconds since the start, heap_free)
        while True:
            now = time.monotonic()
            if now >= next_stats:
                self.state()
                stats = self.stats()
                if stats is not None and "heap_free" in stats:
                    heap.append((now - started, stats["heap_free"]))
                next_stats += SOAK_STATS_S
            if now - started >= seconds:
                break
            try:
                size = len(read_mirror(host))
                if size == MIRROR_BYTES:
                    reads += 1
                else:
                    misses += 1
                    self.log.write("rc", f"soak: display_buffer gave {size} bytes, not {MIRROR_BYTES}")
            except OSError as error:
                misses += 1
                self.log.write("rc", f"soak: display_buffer: {error}")
            time.sleep(max(0.0, SOAK_POLL_S - (time.monotonic() - now)))

        self.log.write("rc", f"soak: {reads} reads of the mirror, {misses} with no answer, in {seconds:.0f} s")
        if misses:
            self.fail(f"soak: {misses} of {reads + misses} reads of the mirror got no answer")
        if len(heap) < 2:
            self.fail("soak: fewer than two stats read")
            return
        settled = [free for at, free in heap if at <= SOAK_SETTLE_S] or [heap[0][1]]
        low, end = min(settled), heap[-1][1]
        self.log.write("rc", f"soak: heap_free lowest in the first {SOAK_SETTLE_S:.0f} s {low}, at the end {end}, "
                             f"lowest of the run {min(free for _, free in heap)}")
        if end < low:
            self.fail(f"soak: heap_free ended at {end}, lower than {low}, its lowest in the first "
                      f"{SOAK_SETTLE_S:.0f} s")

    def step_activity(self):
        """
        Sample each trickler's STEP pin through the probe.

        Returns:
            For each trickler, the share of samples in which the pin was driven high.
        """
        activity = {}
        for motor, address in self.board["step_status"].items():
            values = self.openocd.sample_word(address, STEP_SAMPLES)
            activity[motor] = sum(1 for value in values if value & GPIO_STATUS_OUTTOPAD) / len(values)
        self.log.write("rc", "step pins high: " + ", ".join(f"{motor} {share:.0%}" for motor, share in activity.items()))
        return activity

    def charge_cycle(self, target, threshold, timeout_s):
        """
        A charge from zero to cup return with the simulated scale, then the start of the next one.

        Passes when the charge states come in CHARGE_CYCLE_ORDER within timeout_s, the weight when the charge ends
        is no more than `threshold` under the target, and both motors are commanded to 0 at the end. The time to
        target and the overshoot are printed, not judged: they belong to the made-up powder.

        Args:
            target: the charge weight.
            threshold: the fine stop threshold, the most the charge may end under the target.
            timeout_s: the longest the states may take.
        """
        if self.send("sim on") is None or self.failures:
            return
        reply = self.send(f"charge {target:g}")
        if reply is None or not reply.startswith("@ok"):
            self.send("sim off")
            return

        seen = []           # the charge states in the order they came
        entered = {}        # time each state of the cycle was first entered
        weight_at_end = None
        started = time.monotonic()
        while time.monotonic() - started < timeout_s and seen != CHARGE_CYCLE_ORDER:
            state = self.state()
            if state is None:
                break
            charge = int(state["charge"])
            weight = float(state["weight"])
            if not seen or seen[-1] != charge:
                # The charge's weight is the one on the scale once charging has ended, before the cup is lifted
                if charge == 3 and seen and seen[-1] == 2:
                    weight_at_end = weight
                seen.append(charge)
                entered.setdefault(len(seen) - 1, time.monotonic() - started)
                self.log.write("rc", f"charge state {charge} ({CHARGE_STATE_NAMES.get(charge, '?')}) at "
                                     f"{time.monotonic() - started:.1f} s, weight {weight:.3f}")
            time.sleep(CHARGE_POLL_S)

        if seen != CHARGE_CYCLE_ORDER:
            self.fail(f"charge states came as {seen}, expected {CHARGE_CYCLE_ORDER} within {timeout_s:.0f} s")
        if weight_at_end is not None:
            self.log.write("rc", f"charge ended at {weight_at_end:.3f} for a target of {target:.3f} "
                                 f"(overshoot {weight_at_end - target:+.3f}), "
                                 f"{entered.get(2, 0) - entered.get(1, 0):.1f} s from charging to the cup prompt")
            if target - weight_at_end > threshold:
                self.fail(f"the charge ended {target - weight_at_end:.3f} under the target, more than {threshold}")
        elif seen == CHARGE_CYCLE_ORDER:
            self.fail("no weight was read when the charge ended")

        self.send("charge stop")
        deadline = time.monotonic() + 5
        state = None
        while time.monotonic() < deadline:
            state = self.state()
            if state is not None and state["charge"] == "0" and float(state["coarse"]) == 0 \
                    and float(state["fine"]) == 0:
                break
            time.sleep(CHARGE_POLL_S)
        else:
            self.fail(f"after charge stop, not out of charge mode with both motors at 0: {state}")
        self.send("sim off")

    def pulses(self, assert_elf, hardfault_elf):
        """
        Do the step pulses stop? With the coarse trickler commanded to 1 rev/s: (a) a reset through the probe,
        (b) the assert test build, (c) the hard-fault test build. (a) and (b) pass when the STEP pin stops
        pulsing; (c) is measured and reported, since a halted core leaves PIO running. Ends on the normal build.

        Args:
            assert_elf: the ELF built with RC_TEST_FAULT=1.
            hardfault_elf: the ELF built with RC_TEST_FAULT=2.
        """
        if self.board["step_status"] is None:
            self.fail("the step pins are not worked out for this board")
            return
        normal_elf = self.elf

        def start_coarse(case):
            self.send("motor coarse 1")
            time.sleep(1.5)     # the ramp
            if self.step_activity()["coarse"] <= STEP_RUNNING_FRACTION:
                self.fail(f"{case}: the coarse trickler did not start pulsing")
                return False
            return True

        # (a) a reset through the probe
        if start_coarse("reset"):
            for write in self.board["reset"]:
                self.openocd.cmd(write)
            time.sleep(0.5)
            stopped = self.step_activity()["coarse"] == 0
            self.log.write("rc", f"(a) reset through the probe: pulses {'stopped' if stopped else 'RUNNING'}")
            if not stopped:
                self.fail("(a) the step pulses ran on after a reset through the probe")
            self.wait_for_console()

        # (b) a failed assert enters the USB loader
        self.flash(assert_elf)
        flashed = time.monotonic()
        self.console.expect_fault = True
        if start_coarse("assert"):
            time.sleep(max(0.0, flashed + 13 - time.monotonic()))
            stopped = self.step_activity()["coarse"] == 0
            self.log.write("rc", f"(b) assert: pulses {'stopped' if stopped else 'RUNNING'}")
            if not stopped:
                self.fail("(b) the step pulses ran on after an assert")
        self.console.expect_fault = False

        # (c) a hard fault: the core halts, PIO does not
        self.flash(hardfault_elf)
        flashed = time.monotonic()
        if start_coarse("hard fault"):
            time.sleep(max(0.0, flashed + 13 - time.monotonic()))
            share = self.step_activity()["coarse"]
            self.log.write("rc", f"(c) hard fault: pulses {'RUNNING' if share > STEP_RUNNING_FRACTION else 'stopped'} "
                                 f"(pin high in {share:.0%} of samples); reported, not judged")

        self.flash(normal_elf)

    def finish(self):
        """
        Check the cores, stop openocd, print the verdict.

        Returns:
            The exit code: 0 for PASS, 1 for FAIL.
        """
        for core in self.openocd.halted_cores(self.elf):
            self.fail(f"core not running: {core}")
        self.console.close()
        self.openocd.close()
        if self.failures:
            verdict = "VERDICT FAIL " + "; ".join(self.failures)
        else:
            verdict = "VERDICT PASS"
        self.log.write("rc", verdict)
        self.log.write("rc", f"log: {self.log.path}")
        self.log.close()
        return 0 if not self.failures else 1


def main():
    parser = argparse.ArgumentParser(description="Drive a unit through a debug probe (ADR-001).")
    parser.add_argument("--board", choices=sorted(BOARDS), default="pico_w")
    parser.add_argument("--elf", help="the ELF on the unit (default: the board's build)")
    sub = parser.add_subparsers(dest="command", required=True)
    send_parser = sub.add_parser("send", help="send command lines, one after another")
    send_parser.add_argument("lines", nargs="+")
    sub.add_parser("flash", help="flash the build through the probe and check it runs")
    sub.add_parser("reset", help="reset the chip through the probe")
    watch_parser = sub.add_parser("watch", help="log the console for a time")
    watch_parser.add_argument("seconds", type=float)
    sub.add_parser("snap", help="save the screen as a PNG under logs/rc/snaps/")
    tour_parser = sub.add_parser("menu_tour", help="snap every form, pressing nothing")
    tour_parser.add_argument("--save", help="keep the pictures in this folder")
    tour_parser.add_argument("--compare", help="fail on a form missing from, or differing from, this folder")
    cycle_parser = sub.add_parser("charge_cycle", help="a charge with the simulated scale, cup off and back")
    cycle_parser.add_argument("--target", type=float, default=30.0)
    cycle_parser.add_argument("--threshold", type=float, default=0.03, help="the unit's fine stop threshold")
    cycle_parser.add_argument("--timeout", type=float, default=120.0)
    pulses_parser = sub.add_parser("pulses", help="do the step pulses stop on a reset, an assert, a hard fault?")
    pulses_parser.add_argument("--assert-elf", default=str(REPO / "build_w_fault1" / "app.elf"))
    pulses_parser.add_argument("--hardfault-elf", default=str(REPO / "build_w_fault2" / "app.elf"))
    settings_parser = sub.add_parser("settings", help="copy the stored settings over the web API, and compare")
    settings_parser.add_argument("--host", required=True, help="the unit's address")
    settings_parser.add_argument("--save", help="keep the copy in this folder")
    settings_parser.add_argument("--compare", help="fail on a field that differs from the copy in this folder")
    soak_parser = sub.add_parser("soak", help="read the screen's mirror over the web for a time; watch the memory")
    soak_parser.add_argument("seconds", type=float)
    soak_parser.add_argument("--host", required=True, help="the unit's address")
    args = parser.parse_args()

    session = Session(args, args.command)
    if args.command == "send":
        for line in args.lines:
            session.send(line)
    elif args.command == "snap":
        session.save_snap(LOG_DIR / "snaps" / f"{session.log.path.stem}.png")
    elif args.command == "menu_tour":
        session.menu_tour(args.save, args.compare)
    elif args.command == "charge_cycle":
        session.charge_cycle(args.target, args.threshold, args.timeout)
    elif args.command == "pulses":
        session.pulses(args.assert_elf, args.hardfault_elf)
    elif args.command == "flash":
        session.flash()
    elif args.command == "reset":
        session.reset()
    elif args.command == "watch":
        time.sleep(args.seconds)
    elif args.command == "settings":
        session.settings(args.host, args.save, args.compare)
    elif args.command == "soak":
        session.soak(args.host, args.seconds)
    sys.exit(session.finish())


if __name__ == "__main__":
    main()
