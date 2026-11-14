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
    venv\\Scripts\\python tools\\remote_control\\rc_run.py soak <seconds> --host <address>
"""
import argparse
import base64
import datetime
import json
import math
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
# The versions, from toolchain.env (ADR-011): one NAME=value a line
VERSIONS = dict(re.findall(r"^([A-Z_]+)=(\S+)", (REPO / "toolchain.env").read_text(encoding="utf-8"), re.M))
OPENOCD_DIR = PICO_SDK_ROOT / "openocd" / VERSIONS["OPENOCD"]
# setup_toolchain.ps1's folder for the compiler: 15_2_Rel1 for Arm's 15.2.rel1, as the VS Code extension names it
TOOLCHAIN_BIN = PICO_SDK_ROOT / "toolchain" / VERSIONS["ARM_GCC"].replace(".", "_").replace("rel", "Rel") / "bin"
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
    # The RP2350's registers, from the SDK's headers (watchdog.h, psm.h, io_bank0.h): the same reset as the
    # RP2040's, every power domain but the two oscillators (PSM_WDSEL_BITS less ROSC and XOSC, as
    # watchdog_reboot() selects them), and the same STEP pins (one pin map for both boards). Written for
    # ADR-011's pass on the Pico 2 W; proven when a Pico 2 W is on the bench.
    "pico2_w": {
        "target": "target/rp2350.cfg", "build": "build", "cores": ["rp2350.cm0", "rp2350.cm1"],
        "reset": ["mww 0x400d801c 0", "mww 0x40018008 0x01fffff3", "mww 0x400d8000 0x80000000"],
        "step_status": {"coarse": 0x40028018, "fine": 0x40028040},
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
CORE_STATE_WAIT_S = 5.0   # how long openocd may not know a core's state before it counts as not running
CONSOLE_TIMEOUT_S = 20.0    # from a reset until the console answers
TCL_TERMINATOR = b"\x1a"
RTT_SEARCH_SIZE = 0x400     # bytes searched for the control block, from the address the ELF gives
RTT_POLL_MS = 10            # how often openocd reads the RTT buffers (its default is 100 ms)

# Snaps taken until two in a row are equal. The console copies the screen's buffer with no lock while a task may
# be drawing into it, so a copy can catch a frame half drawn. The menu draws on a change; the charge's screen
# redraws every 20 ms, and 4 tries were not always enough there (ADR-013). The firmware's display lock, which no
# drawing task takes, is the real cure (the screens' ADR).
SNAP_TRIES = 10
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
SETTINGS_TRIES = 3

MIRROR_BYTES = 128 * 64 // 8    # the 12864 display's buffer, one bit a pixel
# soak: charges back to back while two browsers use the web pages, as with the portal open on a phone and a PC.
# Each browser reads the screen's mirror as the mirror page does and polls the charge as the portal's charge page
# does; one of them reloads the portal, and the settings are read as the settings page does. Reads only.
SOAK_BROWSERS = 2
SOAK_MIRROR_S = 1.0         # display_mirror.html: once a second
SOAK_STATE_S = 0.5          # web_portal.html, while a charge runs: every 500 ms
SOAK_PAGE_S = 30.0          # the portal page reloaded
SOAK_SETTINGS_S = 60.0      # every settings path
# The default limits are what a sound server should meet (ADR-003). Main's server of 2026-11-14 misses about 8% of
# these reads, so an ADR that does not touch it passes limits taken from a run before its change (ADR-011).
SOAK_MISS_SHARE = 0.01      # the most of the web reads that may miss
SOAK_MISS_RUN = 2           # the most misses in a row on one poller
SOAK_SETTLE_S = 300.0       # heap_free may not end lower than its lowest in the first five minutes

# scale_frames (ADR-013): the motors as in a throw on the bench's settings (the coarse trickler at its 5 rev/s cap)
SCALE_FRAMES_COARSE_RPS = 5.0
SCALE_FRAMES_FINE_RPS = 2.0
SCALE_FRAMES_TEXTS = 5     # the most frequent texts shown per phase, of the frames that did not read as a number

# charge_fault (ADR-013)
CHARGE_FAULT_POLL_S = 0.05     # how often state is read around the fault
CHARGE_FAULT_RAMP_S = 0.25     # a ramp down from full speed (0.1 s at 50 rev/s2) and a poll or two


class WebPoller(threading.Thread):
    """
    Web reads repeated at an interval until stopped. Each round does its requests one after another, as a page
    does when it opens, and each request counts as one read, right or missed: so a share or a run of misses
    means the same for every poller.
    """

    def __init__(self, name, interval_s, reads, log):
        """
        Args:
            name: what it reads, for the log.
            interval_s: seconds from the start of one round to the start of the next.
            reads: (label, function) for each request of a round. A function does one request and returns True
                when the answer is right; an OSError or a ValueError counts as no answer.
            log: the SessionLog.
        """
        super().__init__(daemon=True)
        self.name, self.interval_s, self.reads, self.log = name, interval_s, reads, log
        self.stop_event = threading.Event()
        self.good = 0
        self.missed = 0
        self.run_now = 0        # misses in a row, now
        self.run_longest = 0

    def run(self):
        while not self.stop_event.is_set():
            started = time.monotonic()
            for label, read in self.reads:
                try:
                    ok, why = read(), "a wrong answer"
                except (OSError, ValueError) as error:
                    ok, why = False, str(error)
                if ok:
                    self.good += 1
                    self.run_now = 0
                else:
                    self.missed += 1
                    self.run_now += 1
                    self.run_longest = max(self.run_longest, self.run_now)
                    self.log.write("rc", f"soak: {self.name}{f' {label}' if label else ''}: {why}")
            self.stop_event.wait(max(0.0, self.interval_s - (time.monotonic() - started)))


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
            if not text.startswith(("@sd ", "@sf ")):     # a snap's data, a tapped frame: in the file, not on the screen
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

    def core_state(self, core):
        """
        A core's state as openocd knows it. "unknown" is openocd not knowing yet, not the core's state: upstream
        FreeRTOS resets core 1 as the scheduler starts (ADR-011, step 2), and on the RP2040, where each core has
        its own debug port, openocd needs a moment after that to see core 1 again. So "unknown" is asked again
        until CORE_STATE_WAIT_S has passed.

        Returns:
            "running", "halted", "reset", or "unknown" when openocd still does not know.
        """
        deadline = time.monotonic() + CORE_STATE_WAIT_S
        while True:
            state = self.cmd(f"{core} curstate").strip()
            if state != "unknown" or time.monotonic() > deadline:
                return state
            time.sleep(0.5)

    def where_cores_are(self, elf):
        """
        Halt each core, read where it is, and let it run on: evidence for a unit that stopped answering.

        Returns:
            One description per core.
        """
        found = []
        for core in self.board["cores"]:
            state = self.core_state(core)
            self.cmd(f"targets {core}")
            self.cmd("halt")
            where = []
            for register in ("pc", "lr"):
                match = re.search(r"0x([0-9a-fA-F]+)", self.cmd(f"reg {register}"))
                if match:
                    value = int(match.group(1), 16)
                    where.append(f"{register}={value:#010x} ({elf_function_at(elf, value)})")
            found.append(f"{core} ({state}): " + ", ".join(where))
        for core in reversed(self.board["cores"]):
            self.cmd(f"targets {core}")
            self.cmd("resume")
        return found

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
            state = self.core_state(core)
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

    def __init__(self, log, failures, recover_link=None):
        """
        Connect to the RTT server and start reading lines.

        Args:
            log: the SessionLog.
            failures: the run's list of failure reasons, appended to when a line shows one.
            recover_link: called when a reply is late; returns True when it found the probe's link lost and
                started RTT again, so the reply can still come.
        """
        self.log = log
        self.failures = failures
        self.recover_link = recover_link
        self.expect_fault = False   # set while a test build is meant to assert: its line is then not a failure
        self.tap_lines = []         # (host time, "@sf ..." line), from the scale tap
        self.charge_events = []     # (host time, "Charge: ..." line), the charge's pauses and ends
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
                if line.startswith("@sf "):
                    # A frame from the scale tap (ADR-013): kept apart, with when it was read, so the replies to
                    # commands are not mixed with them and no drain() loses them
                    self.tap_lines.append((time.monotonic(), line))
                    continue
                if line.startswith("Charge: "):
                    self.charge_events.append((time.monotonic(), line))   # a pause, a resume, an end (ADR-013)
                self.lines.put(line)

    def drain(self):
        """Forget lines already read."""
        while not self.lines.empty():
            self.lines.get_nowait()

    def command(self, line, timeout=REPLY_TIMEOUT_S, recover=True):
        """
        Send one command line and wait for its reply. When the reply is late because the probe's link dropped,
        RTT is started again once and the wait starts over; the line is not sent again, since the unit got it.

        Args:
            line: the command line.
            timeout: seconds to wait.
            recover: False while a restart is awaited, when RTT failing is expected.

        Returns:
            The reply line, or None if none came in time.
        """
        name = line.split()[0]
        self.drain()
        self.data_lines = []    # the `@` lines the command sent before its reply (a snap's)
        self.log.write("rc", f"> {line}")
        self.sock.sendall(line.encode() + b"\n")
        deadline = time.monotonic() + timeout
        recovered = False
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                if recover and not recovered and self.recover_link is not None and self.recover_link():
                    recovered = True
                    deadline = time.monotonic() + timeout
                    continue
                return None
            try:
                reply = self.lines.get(timeout=remaining)
            except queue.Empty:
                continue
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
        self.link_drops = 0
        self.link_seen = len(self.openocd.output)   # openocd's lines already looked at for a dropped link
        self.console = Console(self.log, self.failures, self.recover_link)

    def recover_link(self):
        """
        A reply is late. If openocd has said since the last look that it could not read RTT, the probe's link
        dropped while the chip ran (seen once on the bench, mid-soak, with no reset): start RTT again so the
        reply waiting in the unit's buffer comes through. Counted and reported, not judged: a unit that really
        stopped answering still fails, because no reply comes after it either.

        Returns:
            True when RTT was started again.
        """
        new = self.openocd.output[self.link_seen:]
        self.link_seen = len(self.openocd.output)
        if not any("rtt: Failed" in line for line in new):
            return False
        self.link_drops += 1
        self.log.write("rc", f"the probe's link dropped (openocd could not read RTT): RTT started again, "
                             f"drop {self.link_drops}")
        self.openocd.rtt_start(self.elf)
        return True

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
        try:
            while time.monotonic() < deadline:
                self.openocd.rtt_start(self.elf)
                reply = self.console.command("state", timeout=1.0, recover=False)
                if reply is not None:
                    self.check_up(reply)
                    return reply
            self.fail(f"the console did not answer within {CONSOLE_TIMEOUT_S:.0f} s of a restart")
            for description in self.openocd.where_cores_are(self.elf):
                self.log.write("rc", f"with no console: {description}")
            return None
        finally:
            self.link_seen = len(self.openocd.output)   # RTT failing across a restart is not a dropped link

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
            # Up to SETTINGS_TRIES tries: this compares what is stored; how well the web answers is soak's to judge
            for attempt in range(1, SETTINGS_TRIES + 1):
                try:
                    read[name] = json.loads(http_get(host, f"rest/{name}"))
                    break
                except (OSError, ValueError) as error:
                    self.log.write("rc", f"settings: rest/{name}, try {attempt}: {error}")
            if name not in read:
                self.fail(f"settings: rest/{name}: no right answer in {SETTINGS_TRIES} tries")
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

    def soak(self, host, seconds, target, threshold, max_miss_share, max_miss_run):
        """
        Charges back to back with the simulated scale, each judged as charge_cycle judges one, while two
        browsers use the web pages (SOAK_BROWSERS and the intervals above); `state` and `stats` between charges.
        Fails on a failed charge, on a restart or an assert, on more than max_miss_share of the web reads with
        no right answer or max_miss_run in a row on one poller, and when heap_free ends lower than its lowest in
        the first SOAK_SETTLE_S: memory taken and not given back.

        Args:
            host: the unit's address.
            seconds: how long; the charge under way when the time is up is finished.
            target: each charge's weight.
            threshold: the unit's fine stop threshold.
            max_miss_share: the most of the web reads that may miss, 0 to 1.
            max_miss_run: the most misses in a row one poller may have.
        """
        page_size = {}

        def read_page():
            body = http_get(host, "")
            page_size.setdefault("first", len(body))
            return len(body) == page_size["first"] and len(body) > 0

        def read_state():
            return "s2" in json.loads(http_get(host, "rest/charge_mode_state"))

        def read_setting(name):
            return lambda: bool(json.loads(http_get(host, f"rest/{name}")))

        def read_mirror_right():
            return len(read_mirror(host)) == MIRROR_BYTES

        pollers = []
        for browser in range(1, SOAK_BROWSERS + 1):
            pollers.append(WebPoller(f"browser {browser} mirror", SOAK_MIRROR_S, [("", read_mirror_right)], self.log))
            pollers.append(WebPoller(f"browser {browser} charge state", SOAK_STATE_S, [("", read_state)], self.log))
        pollers.append(WebPoller("portal page", SOAK_PAGE_S, [("", read_page)], self.log))
        pollers.append(WebPoller("settings", SOAK_SETTINGS_S,
                                 [(name, read_setting(name)) for name in SETTINGS_PATHS], self.log))

        started = time.monotonic()
        heap = []   # (seconds since the start, heap_free)
        cycles = 0
        for poller in pollers:
            poller.start()
        while True:
            self.state()
            stats = self.stats()
            if stats is not None and "heap_free" in stats:
                heap.append((time.monotonic() - started, stats["heap_free"]))
            if time.monotonic() - started >= seconds or self.failures:
                break
            self.charge_cycle(target, threshold, timeout_s=120.0)
            cycles += 1
        for poller in pollers:
            poller.stop_event.set()
        for poller in pollers:
            poller.join(timeout=HTTP_TIMEOUT_S * len(SETTINGS_PATHS) + 1)

        self.log.write("rc", f"soak: {cycles} charges in {time.monotonic() - started:.0f} s")
        good = sum(poller.good for poller in pollers)
        missed = sum(poller.missed for poller in pollers)
        for poller in pollers:
            self.log.write("rc", f"soak: {poller.name}: {poller.good} right, {poller.missed} missed, "
                                 f"longest run {poller.run_longest}")
        share = missed / (good + missed) if good + missed else 1.0
        self.log.write("rc", f"soak: {missed} of {good + missed} web reads missed ({share:.2%}), longest run "
                             f"{max(poller.run_longest for poller in pollers)}")
        if share > max_miss_share:
            self.fail(f"soak: {share:.2%} of the web reads missed, more than {max_miss_share:.2%}")
        for poller in pollers:
            if poller.run_longest > max_miss_run:
                self.fail(f"soak: {poller.name} missed {poller.run_longest} in a row, more than {max_miss_run}")
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

    def charge_fault(self, fault, host, target, at_weight, quiet_ms, rate, cleanup_speed, expect, watch_s,
                     repeat=1, want=None, want_none=False):
        """
        A charge with the simulated scale and one fault put in partway (ADR-013), `state` read every
        CHARGE_FAULT_POLL_S from the fault to watch_s after it. Logs each change of the charge's state or of a
        trickler's speed, and judges whether the fault was seen:
          bad: a reading that is not a number. Seen when a trickler goes to its full speed, or the charge ends
              while short of the target by more than the stop threshold;
          quiet: no reading for quiet_ms. Seen when a trickler still turns 3 intervals and a ramp after the
              last reading, inside the quiet time;
          cleanup: the web's cleanup speed sent during the charge. Seen when a trickler takes that speed.

        Args:
            fault: "bad", "quiet" or "cleanup".
            host: the unit's address, for the maxima and for the cleanup request.
            target: the charge's weight.
            at_weight: the weight at which the fault goes in (charging).
            quiet_ms: how long the scale is quiet, for "quiet".
            rate: the simulated scale's readings a second.
            cleanup_speed: the cleanup speed sent, for "cleanup".
            expect: "seen" (the code before the fix) or "not-seen" (after it); the other fails.
            watch_s: how long `state` is read after the fault.
            repeat: how many times the fault goes in, each after the charge resumed from the one before.
            want: words that must come in the charge's "Charge: ..." lines, in that order ("paused", "resumed").
            want_none: no pause and no end may come.
          none: nothing put in, the simulated scale at `rate`. Seen when the charge pauses or ends.
        """
        profile = json.loads(http_get(host, "rest/profile_config"))
        coarse_max = min(float(profile["p7"]), float(json.loads(http_get(host, "rest/coarse_motor_config"))["m4"]))
        fine_max = min(float(profile["p12"]), float(json.loads(http_get(host, "rest/fine_motor_config"))["m4"]))
        threshold = float(json.loads(http_get(host, "rest/charge_mode_config"))["c6"])
        self.log.write("rc", f"charge_fault: {fault} at {at_weight:g} of {target:g}; full speed coarse {coarse_max:g}, "
                             f"fine {fine_max:g} rev/s; fine stop threshold {threshold:g}; {rate:g} readings a second")

        if self.send("sim on") is None or self.send(f"sim rate {rate:g}") is None or self.failures:
            return
        self.send(f"charge {target:g}")
        deadline = time.monotonic() + 120
        before = None
        while time.monotonic() < deadline:
            before = self.state()
            if before and before["charge"] == "2" and float(before["weight"]) >= at_weight:
                break
            time.sleep(CHARGE_FAULT_POLL_S)
        else:
            self.fail(f"charge_fault: the charge did not reach {at_weight:g} within 120 s")
            self.send("charge stop")
            return

        events_from = len(self.console.charge_events)
        started = time.monotonic()
        samples = []    # (seconds after the first fault, charge, weight, coarse, fine)

        def watch(until_event):
            """Read state for watch_s, or until a "Charge: resumed" or "Charge: ended" line comes after until_event."""
            watch_start = time.monotonic()
            while time.monotonic() - watch_start < watch_s:
                state = self.state()
                if state:
                    samples.append((time.monotonic() - started, state["charge"], state["weight"],
                                    float(state["coarse"]), float(state["fine"])))
                if until_event is not None and any(("resumed" in line or "ended" in line)
                                                   for _, line in self.console.charge_events[until_event:]):
                    return
                time.sleep(CHARGE_FAULT_POLL_S)

        for round_idx in range(repeat):
            round_events = len(self.console.charge_events)
            if fault == "bad":
                self.send("sim bad")
            elif fault == "quiet":
                self.send(f"sim quiet {quiet_ms}")
            elif fault == "cleanup":
                http_get(host, f"rest/cleanup_mode_state?s1={cleanup_speed:g}")
            watch(round_events if round_idx < repeat - 1 else None)
            # The next round goes in once the charge pours again
            if round_idx < repeat - 1:
                time.sleep(0.5)

        last = (before["charge"], before["weight"], float(before["coarse"]), float(before["fine"]))
        self.log.write("rc", f"charge_fault: before: charge {last[0]}, weight {last[1]}, coarse {last[2]:g}, fine {last[3]:g}")
        for at, charge, weight, coarse, fine in samples:
            if (charge, coarse, fine) != (last[0], last[2], last[3]):
                self.log.write("rc", f"charge_fault: +{at:.2f} s: charge {charge}, weight {weight}, coarse {coarse:g}, "
                                     f"fine {fine:g}")
                last = (charge, weight, coarse, fine)

        def number(text):
            try:
                return float(text)
            except ValueError:
                return math.nan

        if fault == "bad":
            full = any(coarse >= 0.98 * coarse_max or fine >= 0.98 * fine_max for _, _, _, coarse, fine in samples)
            short = any(charge != "2" and number(weight) < target - threshold for _, charge, weight, _, _ in samples
                        if math.isfinite(number(weight)))
            seen = full or short
            how = "a trickler at full speed" if full else ("the charge ended short" if short else "")
        elif fault == "quiet":
            late = 3.0 / rate + CHARGE_FAULT_RAMP_S
            turning = [at for at, _, _, coarse, fine in samples if late < at < quiet_ms / 1000.0 and (coarse or fine)]
            seen = bool(turning)
            how = f"a trickler still turning {turning[-1]:.2f} s into {quiet_ms} ms of quiet" if turning else ""
        elif fault == "cleanup":
            took = [at for at, _, _, coarse, fine in samples
                    if abs(coarse - cleanup_speed) < 0.01 or abs(fine - cleanup_speed) < 0.01]
            seen = bool(took)
            how = f"a trickler at the cleanup speed {took[0]:.2f} s after it was sent" if took else ""
        else:
            # none: nothing put in; the fault would be a pause or an end where the scale is only slow
            stops = [line for _, line in self.console.charge_events[events_from:] if "paused" in line or "ended" in line]
            seen = bool(stops)
            how = stops[0] if stops else ""
        self.log.write("rc", f"charge_fault: {fault}: {'SEEN, ' + how if seen else 'not seen'}")
        if (expect == "seen") != seen:
            self.fail(f"charge_fault: {fault}: expected {expect}, was {'seen' if seen else 'not seen'}")

        # The charge's own account (ADR-013): its pauses, resumes and ends, against what was wanted
        events = [line for _, line in self.console.charge_events[events_from:]]
        for line in events:
            self.log.write("rc", f"charge_fault: event: {line}")
        if want:
            pos = 0
            for word in want:
                found = next((idx for idx in range(pos, len(events)) if word in events[idx]), None)
                if found is None:
                    self.fail(f"charge_fault: wanted \"{word}\" after event {pos} and it did not come")
                    break
                pos = found + 1
        if want_none:
            stops = [line for line in events if "paused" in line or "ended" in line]
            if stops:
                self.fail(f"charge_fault: wanted no pause or end, got: {stops[0]}")
        if any("ended" in line for line in events):
            # The screen as the user sees it, before the charge is cleared
            self.save_snap(LOG_DIR / "snaps" / f"{self.log.path.stem}_fault.png")

        state = self.state()
        if state and state["charge"] != "0":
            self.send("charge stop")
            stop_deadline = time.monotonic() + 10
            while time.monotonic() < stop_deadline:
                state = self.state()
                if state and state["charge"] == "0":
                    break
                time.sleep(CHARGE_FAULT_POLL_S)
        self.send("sim rate 10")
        self.send("sim off")

    def scale_frames(self, idle_s, handovers, reboots, motors_s):
        """
        Record what the scale's driver decodes, through the scale tap (ADR-013), and sum it up by phase: idle;
        across hand-overs from the simulated scale; across restarts; with both motors turning. A measurement: it
        fails only when a phase gets no frame or a command fails.

        Args:
            idle_s: seconds of the idle phase.
            handovers: how many times the simulated scale takes over for 2 s and gives the scale back.
            reboots: how many restarts; the tap is not kept across one, so it is switched on as soon as the
                console answers, and the frames before that are not seen.
            motors_s: seconds with the coarse trickler at SCALE_FRAMES_COARSE_RPS and the fine at
                SCALE_FRAMES_FINE_RPS, the hopper empty.
        """
        phases = []     # (name, first index into the tap's lines, last index)

        def record(name, action):
            start = len(self.console.tap_lines)
            action()
            phases.append((name, start, len(self.console.tap_lines)))

        def hand_overs():
            for _ in range(handovers):
                self.send("sim on")
                time.sleep(2.0)
                self.send("sim off")
                time.sleep(4.0)

        def restarts():
            for _ in range(reboots):
                self.send("reboot")         # send() waits for the console to answer again
                self.send("scale tap on")
                time.sleep(5.0)

        def motors_turning():
            self.send(f"motor coarse {SCALE_FRAMES_COARSE_RPS:g}")
            self.send(f"motor fine {SCALE_FRAMES_FINE_RPS:g}")
            time.sleep(motors_s)
            self.send("motor coarse 0")
            self.send("motor fine 0")

        self.send("scale tap on")
        record("idle", lambda: time.sleep(idle_s))
        record("hand-overs", hand_overs)
        record("restarts", restarts)
        record("motors turning", motors_turning)
        self.send("scale tap off")

        for name, first, last in phases:
            frames = []
            malformed = 0
            for _, line in self.console.tap_lines[first:last]:
                parts = line.split()
                try:
                    frames.append((int(parts[1]), float(parts[2]), bytes.fromhex(parts[3]) if len(parts) > 3 else b""))
                except (IndexError, ValueError):
                    malformed += 1
            numbers = sum(1 for _, value, _ in frames if math.isfinite(value))
            infs = sum(1 for _, value, _ in frames if math.isinf(value))
            nans = sum(1 for _, value, _ in frames if math.isnan(value))
            summary = f"scale_frames: {name}: {len(frames)} frames, {numbers} numbers, {infs} inf, {nans} NaN"
            if malformed:
                summary += f", {malformed} lines not read"
            # The rhythm, from the unit's own clock: only where the scale runs on without a break
            if name in ("idle", "motors turning") and len(frames) > 2:
                gaps = sorted(b[0] - a[0] for a, b in zip(frames, frames[1:]) if 0 < b[0] - a[0] < 10000)
                if gaps:
                    summary += f"; between frames {gaps[len(gaps) // 2]} ms typical, {gaps[-1]} ms longest"
            self.log.write("rc", summary)
            texts = {}
            for _, value, raw in frames:
                if not math.isfinite(value):
                    texts[raw] = texts.get(raw, 0) + 1
            for raw, count in sorted(texts.items(), key=lambda item: -item[1])[:SCALE_FRAMES_TEXTS]:
                self.log.write("rc", f"scale_frames: {name}: {count} x {raw!r}")
            if not frames:
                self.fail(f"scale_frames: {name}: no frame")

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
        if self.link_drops:
            self.log.write("rc", f"the probe's link dropped {self.link_drops} times and RTT was started again; "
                                 "reported, not judged")
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
    soak_parser = sub.add_parser("soak", help="charges back to back while two browsers use the web pages")
    soak_parser.add_argument("seconds", type=float, help="how long; chosen for the change being proven")
    soak_parser.add_argument("--host", required=True, help="the unit's address")
    soak_parser.add_argument("--target", type=float, default=30.0)
    soak_parser.add_argument("--threshold", type=float, default=0.03, help="the unit's fine stop threshold")
    soak_parser.add_argument("--max-miss-share", type=float, default=SOAK_MISS_SHARE,
                             help="the most of the web reads that may miss, 0 to 1")
    soak_parser.add_argument("--max-miss-run", type=int, default=SOAK_MISS_RUN,
                             help="the most misses in a row one poller may have")
    frames_parser = sub.add_parser("scale_frames", help="record the scale's frames through the tap, by phase")
    frames_parser.add_argument("--idle", type=float, default=120.0, help="seconds idle")
    frames_parser.add_argument("--handovers", type=int, default=5, help="hand-overs from the simulated scale")
    frames_parser.add_argument("--reboots", type=int, default=2, help="restarts")
    frames_parser.add_argument("--motors", type=float, default=60.0, help="seconds with both motors turning")
    fault_parser = sub.add_parser("charge_fault", help="a simulated charge with one fault put in partway")
    fault_parser.add_argument("fault", choices=["bad", "quiet", "cleanup", "none"])
    fault_parser.add_argument("--host", required=True, help="the unit's address")
    fault_parser.add_argument("--expect", choices=["seen", "not-seen"], required=True,
                              help="seen: the code before the fix; not-seen: after it")
    fault_parser.add_argument("--target", type=float, default=30.0)
    fault_parser.add_argument("--at", type=float, help="the weight the fault goes in at (default: 1 short of the target)")
    fault_parser.add_argument("--quiet-ms", type=int, default=1500)
    fault_parser.add_argument("--rate", type=float, default=10.0, help="the simulated scale's readings a second")
    fault_parser.add_argument("--cleanup-speed", type=float, default=3.0)
    fault_parser.add_argument("--watch", type=float, default=3.0, help="seconds watched after the fault")
    fault_parser.add_argument("--repeat", type=int, default=1, help="how many times the fault goes in")
    fault_parser.add_argument("--want", help="words that must come in the charge's events, in order, comma-separated")
    fault_parser.add_argument("--want-none", action="store_true", help="no pause and no end may come")
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
        session.soak(args.host, args.seconds, args.target, args.threshold, args.max_miss_share, args.max_miss_run)
    elif args.command == "scale_frames":
        session.scale_frames(args.idle, args.handovers, args.reboots, args.motors)
    elif args.command == "charge_fault":
        at = args.at if args.at is not None else args.target - 1.0
        session.charge_fault(args.fault, args.host, args.target, at, args.quiet_ms, args.rate, args.cleanup_speed,
                             args.expect, args.watch, args.repeat,
                             args.want.split(",") if args.want else None, args.want_none)
    sys.exit(session.finish())


if __name__ == "__main__":
    main()
