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
import collections
import datetime
import http.client
import hashlib
import json
import math
import os
import queue
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import traceback
import urllib.error
import urllib.parse
import urllib.request
import zlib
from pathlib import Path

import unit_api


REPO = Path(__file__).resolve().parents[2]
PICO_SDK_ROOT = Path.home() / ".pico-sdk"
# The versions, from toolchain.env (ADR-011): one NAME=value a line
VERSIONS = dict(re.findall(r"^([A-Z_]+)=(\S+)", (REPO / "toolchain.env").read_text(encoding="utf-8"), re.M))
OPENOCD_DIR = PICO_SDK_ROOT / "openocd" / VERSIONS["OPENOCD"]
# setup_toolchain.ps1's folder for the compiler: 15_2_Rel1 for Arm's 15.2.rel1, as the VS Code extension names it
TOOLCHAIN_BIN = PICO_SDK_ROOT / "toolchain" / VERSIONS["ARM_GCC"].replace(".", "_").replace("rel", "Rel") / "bin"
LOG_DIR = REPO / "logs" / "rc"
# The cup (the owner, 2026-11-23): the hopper stays filled, and a simulated charge, a purge and the pulses pour real
# powder. Every command that can turn a motor reads the real scale first, refuses a run whose most pour would take the
# cup past this, reads it again after, and writes both into the ledger
CUP_MOST_GR = 600.0
CUP_LEDGER = REPO / "logs" / "adr009" / "cup_ledger.csv"
# A reading this far below the ledger's last is a drop between runs. It is never taken as the cup emptied on its own:
# a zero of the scale with powder in the cup reads the same (2026-11-24, 02:13). Without the owner's word
# (--cup-emptied) the drop is carried: the cup is counted as the reading and every drop carried since the owner's last
# word, and the ledger says so (`offset=` in its note)
CUP_EMPTIED_GR = 20.0
# The most grains a turn either tube may give: a run's most pour is its motors' bounded turns at this (the bench's
# coarse tube poured 9.5 a turn, 2026-11-23)
CUP_GR_A_TURN_MOST = 12.0
# A motor turned from the console stops by itself after its seconds (the unit's bound, ADR-009, gate F.1); pulses'
# cases each give theirs: (a) the ramp and the reset, (b) and (c) the test build's fault 10 s after its start and a margin
PULSES_RUN_S = {"reset": 3.0, "assert": 14.0, "hard fault": 14.0}
MOTOR_RUN_DEFAULT_S = 15.0      # the unit's bound for a `motor` line given no seconds

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
        # Its flash layout (ADR-004, src/update_boot.h): our bootloader at the start, slot A after it, slot B after
        # A. A full install writes the bootloader and slot A's image (built with APP_SLOT=A)
        "layout": {"first": "bootloader.bin", "slot_a": 0x10004000, "slot_b": 0x10102000, "flash_bytes": 0x200000},
        # What a watchdog restart keeps and a power cut clears: the watchdog's eight scratch registers
        "power_scratch": [0x4005800C],
    },
    # The RP2350's registers, from the SDK's headers (watchdog.h, psm.h, io_bank0.h): the same reset as the
    # RP2040's, every power domain but the two oscillators (PSM_WDSEL_BITS less ROSC and XOSC, as
    # watchdog_reboot() selects them), and the same STEP pins (one pin map for both boards). Written for
    # ADR-011's pass on the Pico 2 W; proven when a Pico 2 W is on the bench.
    "pico2_w": {
        "target": "target/rp2350.cfg", "build": "build", "cores": ["rp2350.cm0", "rp2350.cm1"],
        "reset": ["mww 0x400d801c 0", "mww 0x40018008 0x01fffff3", "mww 0x400d8000 0x80000000"],
        "step_status": {"coarse": 0x40028018, "fine": 0x40028040},
        # Its flash layout (ADR-004, targets/pico2_w_partition_table.json): the ROM's partition table at the start,
        # slot A after it, slot B after A. The image is written as its .bin into a slot, not by its ELF, which is
        # linked at the start of flash, where the ROM maps the slot it boots
        "layout": {"first": "partition_table.bin", "slot_a": 0x10002000, "slot_b": 0x101C2000, "flash_bytes": 0x400000},
        # What a watchdog restart keeps and a power cut clears: the watchdog's eight scratch registers and the power
        # manager's, where the update's note lives (src/update_slots_rp2350.c)
        "power_scratch": [0x400D800C, 0x401000B0],
    },
}
SECTOR_BYTES = 4096
PICOBIN_BLOCK_START = 0xFFFFDED3     # boot/picobin.h: a metadata block's first word
GPIO_STATUS_OUTTOPAD = 0x200
STEP_SAMPLES = 200
# A STEP pin is high half the time while its motor turns (ADR-012), and stays low, or high, when it does not
STEP_RUNNING_FRACTION = 0.1     # a pin high in more than this share of the samples, and in less than
STEP_STUCK_FRACTION = 0.9       # this share, is pulsing

CHARGE_POLL_S = 0.25
UF2_BLOCK = 512                    # flash_uf2: a UF2 block, its magic words, and the flag of a block not for flash
UF2_MAGIC = (0x0A324655, 0x9E5D5157)
UF2_NOT_MAIN_FLASH = 0x00000001
RECORDINGS_THROW_S = 60.0          # recordings_check: the longest a simulated throw and its verdict take
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
# Pages that show values that change from build to build, unit to unit or moment to moment: reported, not judged,
# by --compare (ADR-010, gate 4: the menu's pages by id, in place of MUI's numbered forms)
MENU_TOUR_LIVE_PAGES = {"settings/firmware": "the version, the build and the unit's id",
                        "charge": "the target, which lives in RAM until ADR-014 and a charge or the API sets",
                        "wifi": "the network's address and signal",
                        "settings/scale": "the scale's reading"}
# Pages whose picture holds a secret: snapped into the session's folder (logs/, not in git), never into a baseline
# folder, and not compared (ADR-008, gate 4: the set-up's code is the hotspot's key)
MENU_TOUR_SECRET_PAGES = {"setup": "the code is the unit's hotspot key"}
# The remote mark stays 30 s after a write over the web (menu_nav.c, MENU_REMOTE_MS), and the Wi-Fi's is missing
# while the station joins: the tour waits for both to settle
MENU_TOUR_REMOTE_WAIT_S = 35
REACH_THROW_S = 60.0                # reach's simulated throw of 30 gr pours in about 10 s

# The settings over API v1 (ADR-003, gate 3). GET /api/v1/settings gives every settings resource and powder profile
# in one document, which `settings` keeps and compares field by field; a GET changes nothing (api_v1.md). The
# settings page reads a resource as its section is opened: the soak reads them so
SETTINGS_EXPORT = "api/v1/settings"
SETTINGS_PAGE_PATHS = ["api/v1/settings/charge", "api/v1/settings/motors/coarse", "api/v1/settings/motors/fine",
                       "api/v1/settings/scale", "api/v1/settings/gate", "api/v1/settings/leds",
                       "api/v1/settings/display", "api/v1/settings/wifi", "api/v1/powder_profiles",
                       "api/v1/powder_profiles/0"]
# Routes the index does not list, by name (ADR-008, gate 2): the pages, and the debug routes (not part of v1)
API_CHECK_UNLISTED = {"/", "/mobile", "/wizard", "/display_mirror"}     # /wizard: a redirect to the set-up
STREAM_READ_S = 20              # the event stream's read: past its state again after 15 s quiet
STREAM_KEEP_ALIVE_S = 15        # ADR-008: the state again after this long quiet
STREAM_STALL_S = 10             # the server's stall limit: a stream whose client stops reading is closed after it
STREAM_BACK_S = 90              # the unit back on the network after a restart or a Wi-Fi leave
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
SOAK_QUIET_S = 10.0         # after the browsers stop, the server's idle connections closed (5 s), heap_free read
# --load (ADR-003, gate 2): what a tuning session asks of the unit while it charges
SOAK_LOAD_ROWS = 600         # a throw's samples: 600 rows ...
SOAK_LOAD_COLUMNS = 16       # ... of 16 numbers, pulled back to back
SOAK_LOAD_FAST_S = 0.25      # and a client asking for the charge four times a second
SOAK_RECORDINGS_S = 1.0         # soak --load: the recordings' list read, each new throw's samples pulled whole
# api_check (ADR-003, gate 4): what may move between two reads of the same path with nothing done between them
API_CHECK_VOLATILE = {"uptime_s", "now_ms", "rssi", "weight", "bad_frames", "usual_interval_ms"}
API_CHECK_STATE_S = 15.0        # a state looked for after an action
API_CHECK_BACK_S = 60.0         # from a restart until the unit answers on the network again
API_CHECK_POLL_S = 45.0         # an update's poll: three HTTPS handshakes took 15 s on the Pico W (ADR-010, gate 5)
PULSES_MENU_WAIT_S = 4.0        # the menu's first screen, after a flash, before a motor is taken (pulses)
API_CHECK_TLS_RESERVE = 2048   # what the TLS client leaves the heap (https_client.c, HTTPS_HEAP_RESERVE)
API_CHECK_TLS_ROOM = 8192       # what an HTTPS fetch must leave over it (ADR-008; ADR-005's trigger)
# Two clients follow the event log through every soak with held reads (ADR-003, gate 4): each must see every event
SOAK_FOLLOWERS = 2
SOAK_EVENTS_WAIT_S = 10
# --upload (ADR-003, gate 4): ADR-004's image, a body taken in pieces, sent back to back
SOAK_UPLOAD_BYTES = 700 * 1024
SOAK_UPLOAD_TIMEOUT_S = 30.0    # Python's sendall times the whole body: some 4 s at the bench's 190 KB/s
# update (ADR-004, gate 2)
UPDATE_UPLOAD_TIMEOUT_S = 120.0     # the image, written into flash a sector at a time as it comes
UPDATE_RECEIVING_S = 10.0           # from the start until the menu has begun the job
UPDATE_BACK_ON_NETWORK_S = 60.0     # from the install until the unit answers again
UPDATE_SETTLED_S = 120.0            # from the install until the trial has ended: confirmed, or back after its minute
UPDATE_RECEIVING_IDLE_S = 30.0      # update.c: a job that gets no bytes for this long ends (time)

# scale_frames (ADR-013): the motors as in a throw on the bench's settings (the coarse trickler at its 5 rev/s cap)
SCALE_FRAMES_COARSE_RPS = 5.0
SCALE_FRAMES_FINE_RPS = 2.0
SCALE_FRAMES_TEXTS = 5     # the most frequent texts shown per phase, of the frames that did not read as a number

# charge_fault (ADR-013)
CHARGE_FAULT_POLL_S = 0.05     # how often state is read around the fault
CHARGE_FAULT_RAMP_S = 0.25     # a ramp down from full speed (0.1 s at 50 rev/s2) and a poll or two


def miss_kind(error):
    """
    Name what kind of miss a failed web read was (ADR-003, gate 1).

    Args:
        error: what the read raised, or None when it answered and the answer was wrong.

    Returns:
        "timeout", "reset", "refused", "wrong" or "other".
    """
    if isinstance(error, urllib.error.URLError) and isinstance(error.reason, BaseException):
        error = error.reason
    if error is None or isinstance(error, (ValueError, http.client.HTTPException)):
        return "wrong"
    if isinstance(error, (TimeoutError, socket.timeout)):
        return "timeout"
    if isinstance(error, (ConnectionResetError, ConnectionAbortedError, BrokenPipeError)):
        return "reset"
    if isinstance(error, ConnectionRefusedError):
        return "refused"
    return "other"


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
                when the answer is right; an OSError, a ValueError or an HTTP error counts as a miss.
            log: the SessionLog.
        """
        super().__init__(daemon=True)
        self.name, self.interval_s, self.reads, self.log = name, interval_s, reads, log
        self.stop_event = threading.Event()
        self.good = 0
        self.missed = 0
        self.kinds = collections.Counter()   # misses by miss_kind()
        self.run_now = 0        # misses in a row, now
        self.run_longest = 0
        self.seconds = []       # how long each right read took, from the connection to the last byte

    def times(self):
        """The right reads' median, 95th percentile and longest, in seconds, as text."""
        if not self.seconds:
            return "no right read timed"
        ordered = sorted(self.seconds)
        return (f"{ordered[len(ordered) // 2]:.2f} s median, {ordered[int(0.95 * (len(ordered) - 1))]:.2f} s 95%, "
                f"{ordered[-1]:.2f} s longest")

    def run(self):
        while not self.stop_event.is_set():
            started = time.monotonic()
            for label, read in self.reads:
                read_started = time.monotonic()
                try:
                    ok, why, error = read(), "a wrong answer", None
                except (OSError, ValueError, http.client.HTTPException) as caught:
                    # An HTTP error is neither of the other two: uncaught, it ended the poller and its misses
                    ok, why, error = False, f"{type(caught).__name__}: {caught}", caught
                if ok:
                    self.seconds.append(time.monotonic() - read_started)
                    self.good += 1
                    self.run_now = 0
                else:
                    self.missed += 1
                    self.kinds[miss_kind(error)] += 1
                    self.run_now += 1
                    self.run_longest = max(self.run_longest, self.run_now)
                    self.log.write("rc", f"soak: {self.name}{f' {label}' if label else ''}: {why}")
            self.stop_event.wait(max(0.0, self.interval_s - (time.monotonic() - started)))


class EventFollower(threading.Thread):
    """
    A client that follows the unit's event log with held reads (?wait), as a page that shows each change does
    (ADR-003, gate 4). A read that fails is tried again from where it left off, so an event is missed only when it
    is no longer kept; what it saw is judged at the end.
    """

    def __init__(self, name, host, log):
        """
        Args:
            name: which follower, for the log.
            host: the unit's address.
            log: the SessionLog.
        """
        super().__init__(daemon=True)
        self.name, self.log = name, log
        self.unit = unit_api.Unit(host)
        self.stop_event = threading.Event()
        self.events = []
        self.failed_reads = 0

    def run(self):
        # Its starting point is taken before it starts (soak), so a first read that fails here is tried again
        while not self.stop_event.is_set():
            try:
                for event in self.unit.follow(wait_s=SOAK_EVENTS_WAIT_S, for_s=SOAK_EVENTS_WAIT_S):
                    self.events.append(event)
            except (OSError, ValueError, http.client.HTTPException, unit_api.ApiError) as error:
                self.failed_reads += 1
                self.log.write("rc", f"soak: {self.name}: {type(error).__name__}: {error}")

    def problems(self):
        """
        What is wrong with what it saw: a gap in the numbers, or an event the schema does not list as it came.

        Returns:
            The problems found; empty when there is none.
        """
        found = []
        numbers = [event["seq"] for event in self.events]
        gaps = [(a, b) for a, b in zip(numbers, numbers[1:]) if b != a + 1]
        if gaps:
            found.append(f"gaps in the numbers: {gaps[:5]}")
        for event in self.events:
            found.extend(self.unit.event_problems(event))
        return found


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
    Decode a snap: the `@snap <w> <h> mono_rows` line and its `@sd <base64>` lines.

    The frame is LVGL's, as the web's mirror serves it (ADR-010, gate 4): rows of w / 8 bytes, the leftmost pixel the
    top bit, 1 lit, the panel's inversion applied. It is the picture a person at the unit sees: the panel turns
    itself when mounted upside down.

    Args:
        data_lines: the lines the snap command sent before its reply.

    Returns:
        The rows, top first, each a list of bools (True for a lit pixel), or None if the snap is incomplete.
    """
    if not data_lines:
        return None
    match = re.match(r"@snap (\d+) (\d+) mono_rows$", data_lines[0])
    if not match:
        return None
    width, height = (int(group) for group in match.groups())
    payload = b"".join(base64.b64decode(line[4:]) for line in data_lines[1:] if line.startswith("@sd "))
    stride = width // 8
    if len(payload) != stride * height:
        return None
    return [[bool(payload[y * stride + x // 8] >> (7 - x % 8) & 1) for x in range(width)] for y in range(height)]


def write_png(path, rows):
    """
    Write rows of pixels as a 1-bit PNG, lit pixels light on dark as the display's negative glass shows them. The same rows always give the same
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
            bits.extend([1 if lit else 0] * PNG_SCALE)
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


def raw_exchange(host, payload, timeout_s=5.0, rcvbuf=None, read_limit=None):
    """
    One request sent as bytes, as given, and what came back (ADR-003, gate 2's wrong requests).

    Args:
        host: the unit's address.
        payload: the bytes to send, the request as it is.
        timeout_s: how long to wait for each read.
        rcvbuf: a receive buffer this small, to stop a reply part way (a client that does not read).
        read_limit: read no more than this, then return with the socket still open in the third value.

    Returns:
        (status, headers, body, sock): the status (None when no reply came), headers as a lower-cased dict, the
        body read, and the socket when read_limit kept it open, else None.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if rcvbuf is not None:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
    sock.settimeout(timeout_s)
    sock.connect((host, 80))
    sock.sendall(payload)
    data = b""
    try:
        while b"\r\n\r\n" not in data:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if read_limit is not None and len(data) >= read_limit:
                break
    except (socket.timeout, OSError):
        pass
    head, _, body = data.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n") if head else []
    status = None
    if lines and lines[0].startswith("HTTP/"):
        parts = lines[0].split(" ")
        status = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        headers[name.strip().lower()] = value.strip()
    if read_limit is not None:
        return status, headers, body, sock
    length = int(headers.get("content-length", "-1"))
    try:
        while length < 0 or len(body) < length:
            chunk = sock.recv(4096)
            if not chunk:
                break
            body += chunk
    except (socket.timeout, OSError):
        pass
    sock.close()
    return status, headers, body, None


class EventStream:
    """
    A client of the unit's event stream (ADR-008, gate 2), as a browser's event source reads it: each message as
    (event, id, data), a comment as ("comment", None, text). Its socket times out, so a read never hangs a check.
    """

    def __init__(self, host, last_id=None, timeout=STREAM_READ_S):
        self.conn = http.client.HTTPConnection(host, 80, timeout=timeout)
        headers = {"Accept": "text/event-stream"}
        if last_id is not None:
            headers["Last-Event-ID"] = last_id
        self.conn.request("GET", "/api/v1/events/stream", headers=headers)
        self.reply = self.conn.getresponse()
        self.status = self.reply.status
        self.closed = False

    def next(self, seconds):
        """
        The next message within `seconds`.

        Returns:
            (event, id, data, the time it came), ("closed", None, None, time) when the unit closed it, or None.
        """
        end = time.monotonic() + seconds
        event, ident, data = None, None, []
        while time.monotonic() < end:
            try:
                line = self.reply.readline()
            except (socket.timeout, TimeoutError):
                return None
            except (OSError, http.client.HTTPException):
                self.closed = True
                return ("closed", None, None, time.monotonic())
            if not line:
                self.closed = True
                return ("closed", None, None, time.monotonic())
            line = line.decode(errors="replace").rstrip("\n")
            if line == "":
                if data:
                    return (event or "message", ident, "\n".join(data), time.monotonic())
                event, ident, data = None, None, []
            elif line.startswith(":"):
                return ("comment", None, line, time.monotonic())
            elif line.startswith("event: "):
                event = line[7:]
            elif line.startswith("id: "):
                ident = line[4:]
            elif line.startswith("data: "):
                data.append(line[6:])
        return None

    def until(self, wanted, seconds):
        """The first message whose event is `wanted` within `seconds`, the others passed over; or None."""
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            message = self.next(end - time.monotonic())
            if message is None or message[0] == "closed" or message[0] == wanted:
                return message
        return None

    def close(self):
        self.conn.close()


class StreamFollower(threading.Thread):
    """
    A browser's page on the event stream (ADR-008, gate 2), in place of polling the charge: the events it sees and
    the states. Silent past the keep-alive, or closed, it opens the stream again from its last id, as a page does;
    a reconnect read afresh is events missed, judged at the end.
    """

    def __init__(self, name, host, log):
        super().__init__(daemon=True)
        self.name, self.host, self.log = name, host, log
        self.unit = unit_api.Unit(host)
        self.stop_event = threading.Event()
        self.events = []
        self.states = 0
        self.failed_reads = 0       # reconnects
        self.afresh = 0             # reconnects that could not go on from the last id
        self.last_id = None
        self.stream = None

    def start_following(self):
        """Open it now, while the unit is quiet: it goes on from the newest event, as the held reads do."""
        self.stream = EventStream(self.host)
        unit = self.stream.until("unit", 5)
        if self.stream.status != 200 or unit is None:
            raise OSError(f"the stream did not start: {self.stream.status}")

    def run(self):
        while not self.stop_event.is_set():
            message = self.stream.next(STREAM_KEEP_ALIVE_S + 5) if self.stream is not None else None
            if message is not None and message[0] == "message":
                self.events.append(json.loads(message[2]))
                self.last_id = message[1]
            elif message is not None and message[0] == "state":
                self.states += 1
            elif message is not None and message[0] == "unit":
                if not json.loads(message[2])["resumed"]:
                    self.afresh += 1
            elif message is None or message[0] == "closed":
                if self.stop_event.is_set():
                    break
                self.failed_reads += 1
                self.log.write("rc", f"soak: {self.name}: {'closed' if message else 'silent'}, opened again from "
                                     f"{self.last_id}")
                try:
                    if self.stream is not None:
                        self.stream.close()
                    self.stream = EventStream(self.host, self.last_id)
                except (OSError, http.client.HTTPException) as error:
                    self.log.write("rc", f"soak: {self.name}: {type(error).__name__}: {error}")
                    self.stream = None
                    time.sleep(1.0)
        if self.stream is not None:
            self.stream.close()

    def problems(self):
        found = []
        numbers = [event["seq"] for event in self.events]
        gaps = [(a, b) for a, b in zip(numbers, numbers[1:]) if b != a + 1]
        if gaps:
            found.append(f"gaps in the numbers: {gaps[:5]}")
        if self.afresh:
            found.append(f"{self.afresh} reconnects read afresh: events missed")
        for event in self.events:
            found.extend(self.unit.event_problems(event))
        return found


def http_call(host, method, path, body=None):
    """
    One request to API v1 with a JSON body (ADR-003, gate 4), its status and its JSON, an error's included.

    Args:
        host: the unit's address.
        method: "POST", "PATCH" or "GET".
        path: from "/api/v1/...".
        body: the JSON body, or None.

    Returns:
        (status, the reply's JSON or None).
    """
    data = None if body is None else json.dumps(body).encode()
    request = urllib.request.Request(f"http://{host}{path}", data=data, method=method)
    if data is not None:
        request.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(request, timeout=HTTP_TIMEOUT_S) as reply:
            status, text = reply.status, reply.read()
    except urllib.error.HTTPError as error:
        status, text = error.code, error.read()
    try:
        return status, json.loads(text)
    except ValueError:
        return status, None


def read_until_closed(sock):
    """
    What a unit sends on a connection until it closes it, or until the socket's timeout.

    Args:
        sock: a connected socket, with its timeout set.

    Returns:
        The bytes.
    """
    got = b""
    try:
        while True:
            more = sock.recv(4096)
            if not more:
                break
            got += more
    except OSError:
        pass
    return got


def http_put_bytes(host, path, data, timeout_s):
    """
    A PUT of bytes as application/octet-stream (ADR-003, gate 4: a body taken in pieces), its status and its JSON.

    Args:
        host: the unit's address.
        path: from "/api/v1/...".
        data: the bytes.
        timeout_s: for the whole body, which Python's sendall times as one.

    Returns:
        (status, the reply's JSON or None).
    """
    request = urllib.request.Request(f"http://{host}{path}", data=data, method="PUT")
    request.add_header("Content-Type", "application/octet-stream")
    try:
        with urllib.request.urlopen(request, timeout=timeout_s) as reply:
            status, text = reply.status, reply.read()
    except urllib.error.HTTPError as error:
        status, text = error.code, error.read()
    try:
        return status, json.loads(text)
    except ValueError:
        return status, None


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


def read_manifest(source):
    """
    A release's manifest (ADR-004), from a file or from its address: GitHub's, or a mirror's. From an address, each file
    is fetched from its own `url` when the manifest gives one (a tag's release), else from beside the manifest, so the
    same files and addresses a unit's own fetch will use are what is tried.

    Args:
        source: a path to manifest.json, its files beside it; or its address (http:// or https://).

    Returns:
        (the manifest, a function giving an entry's bytes).
    """
    if re.match(r"https?://", source):
        def fetch(url):
            # GitHub's release files redirect to another host, which urllib follows
            with urllib.request.urlopen(url, timeout=UPDATE_UPLOAD_TIMEOUT_S) as reply:
                return reply.read()

        manifest = json.loads(fetch(source))
        return manifest, lambda entry: fetch(entry.get("url") or urllib.parse.urljoin(source, entry["file"]))
    path = Path(source)
    manifest = json.loads(path.read_text(encoding="utf-8"))
    return manifest, lambda entry: (path.parent / entry["file"]).read_bytes()


def block_version(elf):
    """
    The version in an image's metadata block (ADR-004), read from the .bin beside its ELF as src/picobin_image.c reads
    it: the first block within the first 4 KB, then the block loop, the last image definition's version. picotool's
    `info` does not read an RP2040 image that does not start at the start of flash, as a Pico W slot's does.

    Args:
        elf: the image's ELF.

    Returns:
        "first.second", or None when the image has no version in a block.
    """
    return block_version_of(Path(elf).with_suffix(".bin").read_bytes())


def block_version_text(data):
    """
    An image's block version as the firmware writes it (`running.block`, `last.version`): (512, 256) is "2.0.1",
    (276, 218) is "1.20.0-218" (scripts/gen_version.py's block_text).

    Args:
        data: the image's bytes.

    Returns:
        The text, or None when the image has no version in a block.
    """
    version = block_version_of(data)
    if version is None:
        return None
    first, second = (int(part) for part in version.split("."))
    commits = second & 0xFF
    return f"{first >> 8}.{first & 0xFF}.{second >> 8}{f'-{commits}' if commits else ''}"


def block_version_of(data):
    """
    The version in an image's metadata block, from its bytes (block_version).

    Args:
        data: the image's bytes.

    Returns:
        "first.second", or None when the image has no version in a block.
    """
    first = next((at for at in range(0, min(len(data), 0x1000) - 3, 4)
                  if struct.unpack_from("<I", data, at)[0] == PICOBIN_BLOCK_START), None)
    if first is None:
        return None
    version, at = None, first
    for _ in range(8):
        index = at + 4
        image_def, block_version_found = False, None
        while index + 4 <= len(data):
            header = struct.unpack_from("<I", data, index)[0]
            kind = header & 0xFF
            words = (header >> 8) & (0xFFFF if kind & 0x80 else 0xFF)
            if kind == 0xFF:    # the last item: then the link to the next block
                at += struct.unpack_from("<i", data, index + 4)[0]
                break
            if kind == 0x42 and (header >> 16) & 0xF == 1:     # an executable's image type
                image_def = True
            if kind == 0x48:
                minor, major = struct.unpack_from("<2H", data, index + 4)
                block_version_found = f"{major}.{minor}"
            index += max(words, 1) * 4
        if image_def:
            version = block_version_found
        if at == first:
            return version
    return None


def expected_block_version(version):
    """
    The metadata block's version a firmware version is given (scripts/gen_version.py's block_version).

    Args:
        version: the firmware's version string: "2.0.1", "2.0.1-5" (5 commits after its tag), either "-dirty".

    Returns:
        "first.second", or None for a version that is not X.Y.Z.
    """
    found = re.match(r"(\d+)\.(\d+)\.(\d+)(?:-(\d+))?", version)
    if not found:
        return None
    major, minor, patch, commits = (int(part or 0) for part in found.groups())
    return f"{major * 256 + minor}.{patch * 256 + min(commits, 255)}"


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
        # Another openocd on the Tcl port holds the probe, and this session's commands would go to it unseen: one
        # left by a session that died at its start flashed a unit while its own openocd found no probe (ADR-004)
        try:
            socket.create_connection(("127.0.0.1", TCL_PORT), timeout=1).close()
        except OSError:
            pass
        else:
            raise RuntimeError(f"another openocd holds the Tcl port {TCL_PORT}: stop it first")
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
        until CORE_STATE_WAIT_S has passed. On the RP2350 openocd names that moment "reset" (seen 0.2 s after a
        restart, ADR-004), so "reset" is asked again too.

        Returns:
            "running", "halted", "reset", or "unknown" when openocd still does not know.
        """
        deadline = time.monotonic() + CORE_STATE_WAIT_S
        while True:
            state = self.cmd(f"{core} curstate").strip()
            if state not in ("unknown", "reset") or time.monotonic() > deadline:
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
        # A thread that dies (a poller, an event follower) fails the session, its traceback in the log: an uncaught
        # exception would otherwise end it with a line on stderr only, and its counts would read as nothing seen
        threading.excepthook = self.thread_died
        self.last_up = None
        self.log.write("rc", f"board {args.board}, elf {self.elf}")
        self.openocd = Openocd(self.board, self.log)
        try:
            self.openocd.rtt_start(self.elf)
        except Exception:
            self.openocd.close()    # or it holds the probe and the Tcl port for the next session
            raise
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

    def thread_died(self, args):
        """threading.excepthook: log a thread's uncaught exception and count it as a failure."""
        text = "".join(traceback.format_exception(args.exc_type, args.exc_value, args.exc_traceback)).rstrip()
        self.log.write("rc", f"a thread died: {args.thread.name if args.thread else '?'}\n{text}")
        self.fail(f"the thread {args.thread.name if args.thread else '?'} died: {args.exc_type.__name__}: "
                  f"{args.exc_value}")

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
        if (line.split()[0] == "reboot" or line.split()[:2] == ["update", "try"]) and reply.startswith("@ok"):
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

    def flash_uf2(self, uf2):
        """
        A release's full install written through the probe as the USB drive takes it (ADR-009, gate R: the update from
        2.0.3, whose release keeps no ELF here): the whole flash erased, then each run of the UF2's blocks that is not
        erased flash written and verified at its own address, and the chip started. The console is not read: the
        image's version is the API's to say (`GET /api/v1`).

        Args:
            uf2: the full install's .uf2.
        """
        layout = self.board.get("layout")
        if layout is None:
            self.fail("flash_uf2: this board's flash layout is not known")
            return
        data = Path(uf2).read_bytes()
        pages = {}
        for offset in range(0, len(data), UF2_BLOCK):
            block = data[offset:offset + UF2_BLOCK]
            magic0, magic1, flags, address, size = struct.unpack("<IIIII", block[:20])
            if magic0 != UF2_MAGIC[0] or magic1 != UF2_MAGIC[1]:
                self.fail(f"flash_uf2: {uf2} is not a UF2 (block at {offset})")
                return
            if not flags & UF2_NOT_MAIN_FLASH:
                pages[address] = block[32:32 + size]
        runs = []       # (address, bytes): contiguous pages that are not erased flash
        for address in sorted(pages):
            payload = pages[address]
            if payload == b"\xff" * len(payload):
                continue
            if runs and runs[-1][0] + len(runs[-1][1]) == address:
                runs[-1] = (runs[-1][0], runs[-1][1] + payload)
            else:
                runs.append((address, payload))
        folder = LOG_DIR / "uf2" / Path(uf2).stem
        folder.mkdir(parents=True, exist_ok=True)
        writes = ["halt", f"flash erase_address 0x10000000 {layout['flash_bytes']:#x}"]
        for number, (address, payload) in enumerate(runs):
            part = folder / f"run{number}_{address:#x}.bin"
            part.write_bytes(payload)
            last = number == len(runs) - 1
            writes.append(f"program {{{part.as_posix()}}} verify{' reset' if last else ''} {address:#x}")
            self.log.write("rc", f"flash_uf2: {len(payload)} bytes at {address:#x}")
        done_before = len(self.openocd.output)
        for write in writes:
            self.openocd.cmd(write)
        said = self.openocd.output[done_before:]
        verified = sum("Verified OK" in line for line in said)
        self.log.write("rc", f"flash_uf2: {len(runs)} runs written; openocd said verified {verified} times")
        if verified < len(runs):
            self.fail(f"flash_uf2: openocd verified {verified} of {len(runs)} runs")

    def flash(self, elf=None):
        """
        Write an image through the probe, start it, and check the unit runs the build's version. On a board with a
        flash layout (ADR-004) this is the full install: the whole flash erased, then what starts it (the Pico 2 W's
        partition table, the Pico W's bootloader, beside the ELF) and the image's .bin (beside its ELF) in slot A.

        Args:
            elf: the ELF to write; by default the session's. It becomes the session's ELF.
        """
        if elf is not None:
            self.elf = Path(elf)
        if not self.elf.exists():
            self.fail(f"{self.elf} does not exist")
            return
        version, vcs = build_version(self.board["build"])
        layout = self.board.get("layout")
        if layout is not None:
            # The block's version is taken when CMake configures, the version string at every build: a build
            # that was not configured again carries an old one
            found, wanted = block_version(self.elf), expected_block_version(version)
            if found != wanted:
                self.fail(f"{self.elf}'s metadata block is version {found}, its build {version} wants {wanted}")
                return
            first, image = self.elf.parent / layout["first"], self.elf.with_suffix(".bin")
            for needed in (first, image):
                if not needed.exists():
                    self.fail(f"{needed} does not exist")
                    return
            # As the full install by USB does: the whole flash erased, so no other image is kept (the owner). The
            # erase needs the chip halted, which program does for itself
            writes = ["halt", f"flash erase_address 0x10000000 {layout['flash_bytes']:#x}",
                      f"program {{{first.as_posix()}}} verify 0x10000000",
                      f"program {{{image.as_posix()}}} verify reset {layout['slot_a']:#x}"]
        else:
            writes = [f"program {{{self.elf.as_posix()}}} verify reset"]
        started = time.monotonic()
        done_before = len(self.openocd.output)
        for write in writes:
            self.openocd.cmd(write)
        said = self.openocd.output[done_before:]
        verified = sum("Verified OK" in line for line in said)
        erased = sum(line.startswith("erased address") for line in said)
        if verified != sum(write.startswith("program") for write in writes) or \
                erased != sum(write.startswith("flash erase_address") for write in writes):
            self.fail(f"openocd verified {verified} and erased {erased} of the writes: {writes}")
            return
        self.log.write("rc", f"flashed and verified in {time.monotonic() - started:.1f} s")
        reply = self.wait_for_console()
        if reply is None:
            return
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

    def power_cut(self):
        """
        A power cut, as near as the probe comes (ADR-004, gate 2): both cores halted where they are, the scratch
        registers a watchdog restart keeps and a power cut clears set to zero, then the whole chip restarted by its
        watchdog. Not emulated: a flash erase or program cut half-way, since the flash chip keeps its power.
        """
        for core in self.board["cores"]:
            self.openocd.cmd(f"targets {core}")
            self.openocd.cmd("halt")
        for base in self.board["power_scratch"]:
            for index in range(8):
                self.openocd.cmd(f"mww {base + 4 * index:#x} 0")
        for write in self.board["reset"]:
            self.openocd.cmd(write)
        self.last_up = None     # a restart asked for: up= starts again
        self.log.write("rc", "power cut: the cores halted, the scratch registers cleared, the chip restarted")

    def back_after(self, host, boot_before, since):
        """
        The unit once it is back on the network with a new boot.

        Args:
            host: the unit's address.
            boot_before: GET /api/v1's `boot` before the restart.
            since: when the restart was made (time.monotonic()).

        Returns:
            (its unit_api.Unit, its GET /api/v1/update), or (None, None) when it is not back within
            UPDATE_BACK_ON_NETWORK_S.
        """
        while time.monotonic() < since + UPDATE_BACK_ON_NETWORK_S:
            time.sleep(1.0)
            try:
                back = unit_api.Unit(host)
                if back.index["boot"] == boot_before:
                    continue
                status, state = back.request("GET", "update")
                if status == 200:
                    return back, state
            except (OSError, ValueError, http.client.HTTPException, unit_api.ApiError):
                continue
        return None, None

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

    # --- Every element reachable (ADR-010, gate 6; the owner: "a generic test to test if all element can be reached")

    def screen_now(self):
        """
        The screen as the console gives it, parsed.

        Returns:
            A dict: id, kind, focus, back (the title is Back), rows (each: label, info, opens, bold), button (or None),
            reached, at (the editor's digit, or -1), figure, left and right (the softkeys' texts) and softkey (left,
            right or None).
        """
        self.send("screen")
        out = {"id": "", "kind": "", "title": "", "focus": -1, "back": False, "rows": [], "lines": 0, "button": None, "reached": False,
               "at": -1, "figure": "", "left": "", "right": "", "softkey": None}
        for line in self.console.data_lines:
            if line.startswith("@screen "):
                out["id"] = re.search(r"id=(\S+)", line).group(1)
                out["kind"] = re.search(r"kind=(\S+)", line).group(1)
                out["title"] = re.search(r'title="([^"]*)"', line).group(1)
                out["focus"] = int(re.search(r"focus=(-?\d+)", line).group(1))
                out["back"] = line.endswith(" back")
            elif line.startswith("@row "):
                out["rows"].append({"label": re.search(r'label="([^"]*)"', line).group(1),
                                    "info": line.endswith(" info") or " info " in line,
                                    "opens": " opens" in line})
            elif line.startswith("@line "):
                out["lines"] += 1
            elif line.startswith("@button "):
                out["button"] = re.search(r'"([^"]*)"', line).group(1)
                out["reached"] = line.endswith(" reached")
            elif line.startswith("@figure "):
                out["figure"] = re.search(r'"([^"]*)"', line).group(1)
                out["at"] = int(re.search(r"at=(-?\d+)", line).group(1))
            elif line.startswith("@softkeys "):
                out["left"] = re.search(r'left="([^"]*)"', line).group(1)
                out["right"] = re.search(r'right="([^"]*)"', line).group(1)
                found = re.search(r"focus=(left|right)", line)
                out["softkey"] = found.group(1) if found else None
        return out

    def element_of(self, screen):
        """What has the focus: "title", "row<n>", "button", "left", "right", "digit<n>", or None."""
        if screen["softkey"] is not None:
            return screen["softkey"]
        if screen["kind"] == "edit":
            return f"digit{screen['at']}" if screen["at"] >= 0 else None
        if screen["focus"] == -1 and screen["back"]:
            return "title"
        if screen["kind"] == "notes" and screen["reached"]:
            return "button"
        if screen["kind"] in ("list", "control") and 0 <= screen["focus"] < len(screen["rows"]):
            return f"row{screen['focus']}"
        return None

    def elements_of(self, screen):
        """Every element the screen shows that the knob must reach."""
        wanted = set()
        if screen["back"]:
            wanted.add("title")
        if screen["kind"] in ("list", "control"):
            wanted |= {f"row{idx}" for idx, row in enumerate(screen["rows"]) if not row["info"]}
        if screen["kind"] == "notes" and screen["button"]:
            wanted.add("button")
        if screen["kind"] == "edit":
            wanted |= {f"digit{idx}" for idx, char in enumerate(screen["figure"]) if char.isdigit()}
        if screen["left"]:
            wanted.add("left")
        if screen["right"]:
            wanted.add("right")
        return wanted

    def press_and_read(self, key, times=1):
        self.send(f"press {key} {times}")
        time.sleep(0.25)
        return self.screen_now()

    def sweep(self, what):
        """
        Turn the screen open now to its top, then down a detent at a time, noting what has the focus: every element
        it shows must be reached. Only turns: nothing is clicked, so nothing changes or starts.

        Args:
            what: how the screen was reached, for the log.

        Returns:
            The screen as it was found.
        """
        screen = self.screen_now()
        wanted = self.elements_of(screen)
        steps = len(wanted) + len(screen["rows"]) + screen["lines"] + len(screen["figure"]) + 3
        seen = set()
        state = self.press_and_read("ccw", steps)
        seen.add(self.element_of(state))
        for _ in range(steps):
            state = self.press_and_read("cw")
            seen.add(self.element_of(state))
        missing = wanted - seen
        name = f"{screen['id']} ({what})"
        if missing:
            self.fail(f"reach: {name}: not reached: {' '.join(sorted(missing))}")
        else:
            self.log.write("rc", f"reach: {name}: every element reached: {' '.join(sorted(wanted))}")
        return screen

    def reach(self):
        """
        Every page by its id, every screen a page's row opens (its choices, the target's editor, its pages, its
        dialogs: opened by a click, left by Back), and the charge's and the cleanup's screens (a simulated charge,
        zeroing and then between throws, with the page its Change opens; a cleanup at speed 0): each turned through
        by sweep(). Nothing is chosen, set or started but those two jobs, which are stopped. Not covered: a job in
        fault, the update's screen, a message.
        """
        reply = self.send("screen list")
        if reply is None or not reply.startswith("@ok screen"):
            self.fail(f"the menu gives no pages: {reply}")
            return
        page_ids = [line.split(" ", 1)[1] for line in self.console.data_lines if line.startswith("@page ")]
        for page_id in page_ids:
            self.send(f"screen {page_id}")
            screen = self.sweep(f"page {page_id}")
            if screen["kind"] != "list":
                continue
            for idx, row in enumerate(screen["rows"]):
                if not row["opens"]:
                    continue
                # To the row, from the title: a detent at a time, then a click opens what it opens
                self.send(f"screen {page_id}")
                state = self.press_and_read("ccw", len(screen["rows"]) + 2)
                for _ in range(len(screen["rows"]) + 2):
                    if state["focus"] == idx:
                        break
                    state = self.press_and_read("cw")
                if state["focus"] != idx:
                    self.fail(f"reach: {page_id}: row {idx} ({row['label']}) not reached to open")
                    continue
                self.press_and_read("enc")
                self.sweep(f"{page_id}, row {row['label']}")
                self.send("press rst 1")
                time.sleep(0.25)
        # The jobs: a simulated charge, then a cleanup at speed 0, each stopped
        self.send("sim on")
        self.send("charge 30")
        time.sleep(1.0)
        self.sweep("a charge, zeroing")
        # The throw poured: the screen between throws, Stop and Change, and the page Change opens. Change comes about
        # a second after the title, once the throw's verdict is in
        deadline = time.monotonic() + REACH_THROW_S
        state = self.screen_now()
        while (state["title"] != "Lift the cup" or state["right"] != "Change") and time.monotonic() < deadline:
            time.sleep(0.5)
            state = self.screen_now()
        if state["title"] != "Lift the cup" or state["right"] != "Change":
            self.fail(f"reach: the simulated throw did not end with Change in {REACH_THROW_S:.0f} s: "
                      f"{state['title']} {state['right']}")
        else:
            # The simulated scale lifts its own cup 3 s after the verdict, and the next throw closes Change: the pan
            # held at the throw's weight keeps the screen between throws for the sweeps, as a person's cup left on
            # does (the verdict's second went at ADR-009's gate F.5, and the window with it)
            self.send("sim weight 30")
            self.sweep("a charge, between throws")
            state = self.press_and_read("ccw", 4)
            for _ in range(4):
                if state["softkey"] == "right":
                    break
                state = self.press_and_read("cw")
            if state["softkey"] != "right" or state["right"] != "Change":
                self.fail(f"reach: Change not reached between throws: {state['softkey']} {state['right']}")
            else:
                # Read again just before the click: a click meant for Change that lands on the next throw's Zero
                # zeroes the scale (2026-11-24, 02:13: on the simulated one, harmless; on the real one, with powder in
                # the cup, the cup's count is lost). The click only on Change, the simulated scale on
                state = self.screen_now()
                if state["title"] != "Lift the cup" or state["right"] != "Change" or state["softkey"] != "right":
                    self.fail(f"reach: the screen moved on before Change's click: {state['title']} {state['right']}")
                    self.console.command("charge stop")
                    self.console.command("sim weight off")
                    return
                self.press_and_read("enc")
                self.sweep("a charge, its Change")
                self.send("press rst 1")
                time.sleep(0.25)
        # Back from Change may have stopped the charge already, when the next throw had begun and closed the page:
        # the stop is not judged, and the simulated scale's switch below waits for the charge to have ended
        self.console.command("charge stop")
        self.console.command("sim weight off")
        # The simulated scale is switched off once the charge has ended: refused while it runs
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            reply = self.console.command("sim off")
            if reply is not None and reply.startswith("@ok"):
                break
            time.sleep(0.5)
        else:
            self.fail("reach: the simulated scale could not be switched off after the charge")
        self.send("screen ack/purge")
        self.press_and_read("enc")
        time.sleep(0.5)
        self.sweep("a cleanup at speed 0")
        self.send("press rst 1")
        time.sleep(0.5)
        self.send("screen main")

    def menu_tour(self, save_dir, compare_dir):
        """
        Open every page of the menu in turn by its id and snap it, pressing nothing, so no setting changes and
        nothing starts (ADR-010, gate 4: `screen list`, then `screen <id>`). A page's picture is page_<id>.png, its
        id's slashes as dashes.

        Args:
            save_dir: where the pictures go (default: logs/rc/snaps/<session>/).
            compare_dir: a folder of earlier pictures; a page missing from either side, or a picture that
                differs, fails, except the pages in MENU_TOUR_LIVE_PAGES, which are reported. The pages in
                MENU_TOUR_SECRET_PAGES go to the session's folder only, whatever save_dir is, and are not compared.
        """
        reply = self.console.command("screen list")
        if reply is None or not reply.startswith("@ok screen"):
            self.fail(f"the menu gives no pages: {reply}")
            return
        page_ids = [line.split(" ", 1)[1] for line in self.console.data_lines if line.startswith("@page ")]
        # Every page's title shows the marks, so the tour waits for them to settle, or every picture would differ from
        # one taken at rest: the remote one stays 30 s after a write over the web, and the Wi-Fi's (W or AP) is not
        # shown while the station joins, some 25 s after a start (seen on the Pico W right after a flash)
        deadline = time.monotonic() + MENU_TOUR_REMOTE_WAIT_S
        while time.monotonic() < deadline:
            self.console.command("screen")
            marks = next((line for line in self.console.data_lines if line.startswith("@screen ")), "")
            if not re.search(r"marks=\S*R", marks) and re.search(r"marks=(W|AP)", marks):
                break
            time.sleep(1.0)
        session_dir = LOG_DIR / "snaps" / self.log.path.stem
        out_dir = Path(save_dir) if save_dir else session_dir
        found = []
        for page_id in page_ids:
            reply = self.console.command(f"screen {page_id}")
            if reply is None or not reply.startswith("@ok"):
                self.fail(f"page {page_id}: {reply or 'no reply'}")
                continue
            if page_id in MENU_TOUR_SECRET_PAGES:
                # Snapped into the session's own folder only, never a baseline in git, and not compared
                if self.save_snap(session_dir / f"page_{page_id.replace('/', '-')}.png") is not None:
                    self.log.write("rc", f"page {page_id}: kept out of {out_dir} ({MENU_TOUR_SECRET_PAGES[page_id]})")
                continue
            if self.save_snap(out_dir / f"page_{page_id.replace('/', '-')}.png") is not None:
                found.append(page_id)
        self.console.command("screen main")
        self.log.write("rc", f"menu_tour: {len(found)} pages: {' '.join(found)}")
        self.log.write("rc", f"menu_tour: pictures in {out_dir}")

        if compare_dir is None:
            return
        compare_dir = Path(compare_dir)
        baseline = {path.stem[len("page_"):].replace("-", "/") for path in compare_dir.glob("page_*.png")}
        for page_id in sorted(baseline - set(found)):
            self.fail(f"page {page_id} is in {compare_dir} and was not found")
        for page_id in found:
            name = f"page_{page_id.replace('/', '-')}.png"
            reference = compare_dir / name
            if not reference.exists():
                self.fail(f"page {page_id} is new: not in {compare_dir}")
                continue
            if reference.read_bytes() == (out_dir / name).read_bytes():
                continue
            if page_id in MENU_TOUR_LIVE_PAGES:
                self.log.write("rc", f"page {page_id} differs, not judged ({MENU_TOUR_LIVE_PAGES[page_id]})")
            else:
                self.fail(f"page {page_id} differs from {reference}")

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

    def net(self):
        """
        Ask `net` (ADR-003, gate 1): lwIP's heap and pools, each in use, most ever, size and failed allocations.

        Returns:
            Its numbers as ints in a dict, or None (counted as a failure).
        """
        reply = self.send("net")
        if reply is None or not reply.startswith("@ok net"):
            return None
        return {key: int(value) for key, value in re.findall(r"\b(\w+)=(\d+)", reply)}

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
        read = None
        # Up to SETTINGS_TRIES tries: this compares what is stored; how well the web answers is soak's to judge
        for attempt in range(1, SETTINGS_TRIES + 1):
            try:
                read = json.loads(http_get(host, SETTINGS_EXPORT))
                break
            except (OSError, ValueError) as error:
                self.log.write("rc", f"settings: {SETTINGS_EXPORT}, try {attempt}: {error}")
        if read is None or "resources" not in read:
            self.fail(f"settings: {SETTINGS_EXPORT}: no right answer in {SETTINGS_TRIES} tries")
            return
        (out_dir / "settings.json").write_text(json.dumps(read, indent=1, sort_keys=True) + "\n", encoding="utf-8")
        self.log.write("rc", f"settings: {len(read['resources'])} resources read, firmware "
                             f"{read['firmware']['version']}, kept in {out_dir}")

        if compare_dir is None:
            return
        reference = Path(compare_dir) / "settings.json"
        if not reference.exists():
            self.fail(f"settings: {reference} does not exist (a copy of API v1's export)")
            return
        before = json.loads(reference.read_text(encoding="utf-8"))
        # The firmware's version is not a setting: reported, not judged
        if before.get("firmware") != read.get("firmware"):
            self.log.write("rc", f"settings: the firmware differs, not judged: {before.get('firmware')} -> "
                                 f"{read.get('firmware')}")
        old_resources, new_resources = before["resources"], read["resources"]
        for path in sorted(set(old_resources) | set(new_resources)):
            if path not in old_resources or path not in new_resources:
                self.fail(f"settings: {path} is {'gone' if path in old_resources else 'new'}")
                continue
            old, new = old_resources[path], new_resources[path]
            for key in sorted(set(old) | set(new)):
                if old.get(key) != new.get(key):
                    self.fail(f"settings: {path} {key} was {old.get(key)!r}, is {new.get(key)!r}")

    def web_debug(self, host):
        """
        GET /api/v1/debug (ADR-003): lwIP's heap and pools and the server's own counts.

        Returns:
            The reply as a dict, or None.
        """
        try:
            return json.loads(http_get(host, "api/v1/debug"))
        except (OSError, ValueError) as error:
            self.log.write("rc", f"api/v1/debug: {error}")
            return None

    def settings_restore(self, host, restore_dir):
        """
        Write an earlier copy back over the web API and store it (ADR-008, gate 4: a factory reset erases every
        setting, the powders included). Each resource of the copy is written whole in one PATCH, its fields the schema
        lets a client write; then each save path once. The Wi-Fi's key is write only and never in a copy: the one
        stored stays. The caller then compares (`settings --restore` does).

        Args:
            host: the unit's address.
            restore_dir: the folder of the copy (`settings --save`).
        """
        copy = json.loads((Path(restore_dir) / "settings.json").read_text(encoding="utf-8"))
        unit = unit_api.Unit(host)
        refused_records = []
        for path, values in copy["resources"].items():
            resource = unit.resource(path)
            if resource is None:
                self.fail(f"settings: {path} is not in this unit's schema")
                continue
            writable = {field["name"] for field in resource["fields"] if field.get("access", "read_write") == "read_write"}
            body = {name: value for name, value in values.items() if name in writable}
            # In as many PATCHes as the unit's body limit needs, each list whole, each naming what is required (a
            # record's unit: ADR-009, gate R)
            required = [field["name"] for field in resource["fields"] if field.get("required")]
            failed = None
            for part in unit_api.bodies(body, always=required):
                status, reply = unit.request("PATCH", path, part)
                code = reply.get("error", {}).get("code") if isinstance(reply, dict) else None
                if status == 409 and code == "not_this_unit":
                    # Another unit's own data: refused by the unit, and named; the shared parts go on
                    refused_records.append(path)
                    self.log.write("rc", f"settings: {path} refused: another unit's own data (not_this_unit)")
                    break
                self.log.write("rc", f"settings: {path} written back, {len(part)} fields: {status}")
                if status != 200:
                    failed = f"settings: {path}: {status} {json.dumps(reply)}"
                    break
            # Each resource stored as soon as it is written whole, before the next: a powder's record is held until
            # its save, one at a time (ADR-009, gate R). One with a part refused is put back as stored, never saved
            # half written
            if failed is not None:
                self.fail(failed)
                status, reply = unit.request("POST", resource["revert"], {})
                self.log.write("rc", f"settings: {path} not saved, reverted: {status}")
                continue
            status, reply = unit.request("POST", resource["save"], {})
            self.log.write("rc", f"settings: {resource['save']}: {status}")
            if status != 200:
                self.fail(f"settings: {resource['save']}: {status} {json.dumps(reply)}")
        if refused_records:
            self.log.write("rc", f"settings: {len(refused_records)} records refused as another unit's own: "
                                 f"{', '.join(refused_records)}")
        return refused_records

    def settings_roundtrip(self, host, target):
        """
        The proof of gate 3's tables (ADR-003): every setting read, written back as it was read, and read again,
        unchanged; for each field a value outside its limits refused, the resource unchanged; a write during a
        throw refused; and each resource as saved at the end as at the start. Nothing on the unit is changed.

        Args:
            host: the unit's address.
            target: the charge started for the write during a throw, with the simulated scale.
        """
        def call(method, path, body=None):
            data = None if body is None else json.dumps(body).encode()
            request = urllib.request.Request(f"http://{host}{path}", data=data, method=method)
            if data is not None:
                request.add_header("Content-Type", "application/json")
            try:
                with urllib.request.urlopen(request, timeout=HTTP_TIMEOUT_S) as reply:
                    return reply.status, json.loads(reply.read())
            except urllib.error.HTTPError as error:
                return error.code, json.loads(error.read())

        def outside(field):
            """A value of the field's that it must refuse, and why; None when it has none to refuse."""
            kind = field["type"]
            if kind in ("number", "integer"):
                if "max" in field:
                    return field["max"] + max(1, abs(field["max"]) // 2), "above its most"
                if "min" in field:
                    return field["min"] - 1, "below its least"
                return None
            if kind == "choice":
                numbers = isinstance(field["choices"][0]["value"], (int, float))
                return (123457 if numbers else "not_a_choice"), "not one of its choices"
            if kind == "string":
                return "x" * (field["max_length"] + 1), "too long"
            if kind == "boolean":
                return "yes", "not true or false"
            if kind == "colour":
                return "red", "not #rrggbb"
            if kind == "list":
                # One row more than it holds, each of its fields at its least
                row = {cell["name"]: cell.get("min", 0) for cell in field["row"]}
                return [dict(row) for _ in range(field["rows_max"] + 1)], "more rows than it holds"
            return None

        status, schema = call("GET", "/api/v1/schema")
        status_export, export = call("GET", "/api/v1/settings")
        if status != 200 or status_export != 200:
            self.fail(f"settings_roundtrip: the schema ({status}) or the export ({status_export}) not read")
            return
        fields_of = {resource["path"]: resource["fields"] for resource in schema["resources"]}
        written = refused = 0
        for path, before in export["resources"].items():
            fields = fields_of[re.sub(r"/\d+$", "/{id}", path)]
            writable = {f["name"] for f in fields if f.get("access") == "read_write"}
            body = {name: value for name, value in before.items() if name in writable}
            required = [f["name"] for f in fields if f.get("required")]
            for part in unit_api.bodies(body, always=required):
                status, reply = call("PATCH", path, part)
                if status != 200:
                    break
            status_again, again = call("GET", path)
            if status != 200 or again != before:
                changed = {k: (before.get(k), again.get(k)) for k in set(before) | set(again)
                           if before.get(k) != again.get(k)}
                self.fail(f"settings_roundtrip: {path} written back: {status} {reply.get('error')}, changed {changed}")
            written += len(body)
            for field in fields:
                bad = outside(field)
                if bad is None:
                    continue
                value, why = bad
                # With what every write must carry, so the refusal is the field's own (a record's unit: gate R)
                status, reply = call("PATCH", path, dict({name: before[name] for name in required if name in before},
                                                         **{field["name"]: value}))
                status_again, again = call("GET", path)
                if status != 400 or again != before:
                    self.fail(f"settings_roundtrip: {path} {field['name']}={value!r} ({why}): {status}, "
                              f"{'unchanged' if again == before else 'CHANGED'}")
                refused += 1
        self.log.write("rc", f"settings_roundtrip: {len(export['resources'])} resources, {written} fields written back "
                             f"unchanged, {refused} values outside their limits refused")

        # A write during a throw: the charge's settings and the motors' refused, the charge undisturbed
        charge_settings = export["resources"]["/api/v1/settings/charge"]
        if self.send("sim on") is None or self.send(f"charge {target:g}") is None or self.failures:
            return
        deadline = time.monotonic() + 30
        state = None
        while time.monotonic() < deadline:
            state = call("GET", "/api/v1/charge")[1].get("state")
            if state == "charging":
                break
            time.sleep(0.2)
        if state != "charging":
            self.fail(f"settings_roundtrip: the charge did not start pouring ({state})")
        else:
            for path, body in (("/api/v1/settings/charge",
                                {"fine_stop_threshold": charge_settings["fine_stop_threshold"]}),
                               ("/api/v1/settings/motors/coarse",
                                {"current_ma": export["resources"]["/api/v1/settings/motors/coarse"]["current_ma"]})):
                status, reply = call("PATCH", path, body)
                self.log.write("rc", f"settings_roundtrip: {path} during a throw: {status} "
                                     f"{reply.get('error', {}).get('message', '')}")
                if status != 409:
                    self.fail(f"settings_roundtrip: {path} written during a throw: {status}, not 409")
        self.send("charge stop")
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and call("GET", "/api/v1/charge")[1].get("state") != "idle":
            time.sleep(0.2)
        self.send("sim off")

        # Nothing changed: each resource as stored as it was
        status, after = call("GET", "/api/v1/settings")
        if after.get("resources") != export["resources"]:
            self.fail("settings_roundtrip: the settings at the end differ from those at the start")

    def api_check_poll_room(self):
        """
        The room an HTTPS fetch leaves in the FreeRTOS heap (ADR-008: a poll ran it dry; ADR-005's trigger for the Pico
        W's fallback): judged over HTTPS only, where mbedTLS holds its peak; a poll over plain HTTP is reported, not
        judged. A test build trusts the test roots in place of GitHub's, so its fetch holds what a user's does.
        """
        reply = self.console.command("update poll")
        fields = dict(re.findall(r'(\w+)=("[^"]*"|\S+)', reply or ""))
        handshakes = [int(ms) for ms in fields.get("handshake_ms", "0").split(",") if ms.isdigit()]
        low = int(fields.get("heap_low", "0"))
        refused = int(fields.get("alloc_refused", "0"))
        if not any(handshakes):
            self.log.write("rc", "api_check: the poll's room not judged: over plain HTTP")
            return
        need = API_CHECK_TLS_RESERVE + API_CHECK_TLS_ROOM
        self.log.write("rc", f"api_check: an HTTPS fetch left {low} bytes of the heap at its least "
                             f"({low - API_CHECK_TLS_RESERVE} over the reserve), {refused} blocks refused")
        if refused > 0 or low < need:
            self.fail(f"api_check: an HTTPS fetch left {low} bytes of the heap, under {need} "
                      f"(the reserve and {API_CHECK_TLS_ROOM} over it); {refused} blocks refused")

    def api_check_recording(self, unit, throw_started):
        """
        The recording of api_check's own throw, stopped during its zero or its pour (ADR-009, gate 2's rest): named
        by its events and the charge's last, complete, its reason the stop's. A throw of a controller that records
        none has a recording of null, and nothing more is judged.
        """
        if throw_started is None:
            return
        rec_id = throw_started.get("recording")
        last = unit.read("charge")["last"]
        if rec_id is None:
            self.log.write("rc", "api_check: the throw has no recording (not the charge controller's)")
            if last.get("recording") is not None:
                self.fail(f"api_check: the charge's last names recording {last['recording']}, its throw none")
            return
        if last.get("recording") != rec_id:
            self.fail(f"api_check: the charge's last names recording {last.get('recording')}, not {rec_id}")
        entry = unit.read(f"recordings/{rec_id}")
        problems = []
        if entry["id"] != rec_id or entry["throw"] != throw_started["throw"]:
            problems.append(f"id {entry['id']}, throw {entry['throw']}")
        if not entry["complete"] or entry["summary"]["reason"] != "stopped":
            problems.append(f"complete {entry['complete']}, reason {entry['summary']['reason']}")
        if entry["samples_kept"]:
            samples = unit.read(f"recordings/{rec_id}/samples")
            if len(samples["rows"]) != entry["rows"] or not samples["complete"]:
                problems.append(f"samples: {len(samples['rows'])} rows of {entry['rows']}, complete "
                                f"{samples['complete']}")
        self.log.write("rc", f"api_check: recording {rec_id}: {entry['rows']} rows, samples_kept "
                             f"{entry['samples_kept']}, settings {'taken' if entry['settings'] else 'null'}")
        for problem in problems:
            self.fail(f"api_check: recording {rec_id}: {problem}")

    def api_check(self, host, target):
        """
        The close's proof of API v1 (ADR-003, gate 4), by the client that works from the unit's own description
        (unit_api.py). Every GET path the index lists is read twice: each answer checked against the schema's
        tables (a settings resource's fields; a state among its kind's; each event among the kinds, carrying what
        its kind does), the two reads the same, and the settings and the event log unchanged after: a GET changes
        nothing. Then every action the schema lists, each where it is safe, with the answer api_v1.md gives: the
        stops with nothing to stop; refusals with their codes, changing nothing; the saves of what is stored; the
        gate; the zero; a charge and a purge started and stopped, with their events; and last a restart, after
        which the boot is new and the system's start is the log's first event. Fails on an action the schema lists
        and this did not call. The hopper is taken to be empty; the simulated scale stands in for the scale.

        Args:
            host: the unit's address.
            target: the charge's target, set when none is (the menu's digits are not stored: none after a start).
        """
        unit = unit_api.Unit(host)
        index, schema = unit.index, unit.schema
        errors = set(schema["errors"])
        for name in ("api", "firmware", "board", "unit_id", "hostname", "boot", "uptime_s", "features", "guide",
                     "resources"):
            if name not in index:
                self.fail(f"api_check: the index has no {name}")
        self.log.write("rc", f"api_check: {index['hostname']}, {index['board']}, firmware "
                             f"{index['firmware']['version']}: {len(index['resources'])} paths, "
                             f"{len(schema['resources'])} settings resources, {len(schema['actions'])} actions")
        if unit.read("charge")["state"] != "idle" or unit.read("system")["activity"] != "none":
            self.fail("api_check: the unit is not idle at the menu")
            return

        # Every GET twice: each answer against the tables, and a GET changes nothing
        export_before = unit.read("settings")
        unit.start_following()
        display = unit.read("display")
        frame_bytes = display["width"] * display["height"] // 8
        paths = unit.paths("GET")
        for path in paths:
            if path.endswith("/events/stream"):
                # It never ends (ADR-008, gate 2): read as a stream, its start the unit, then the state
                stream = EventStream(host)
                unit_message, state_message = stream.until("unit", 5), stream.until("state", 5)
                stream.close()
                self.log.write("rc", f"api_check: {path}: {stream.status}, the unit and the state "
                                     f"{'read' if unit_message and state_message else 'not read'}")
                if stream.status != 200 or unit_message is None or state_message is None:
                    self.fail(f"api_check: {path}: not a stream that starts with the unit and the state")
                continue
            first = unit.read(path)
            second = unit.read(path)
            for problem in self.api_answer_problems(unit, path, first, frame_bytes):
                self.fail(f"api_check: {path}: {problem}")
            if self.api_stable(first) != self.api_stable(second):
                self.fail(f"api_check: {path} read twice, and the two differ")
        new_events = unit.events()["events"]
        if new_events:
            self.fail(f"api_check: the reads were followed by events: {new_events}")
        if unit.read("settings") != export_before:
            self.fail("api_check: the settings differ after the reads")
        self.log.write("rc", f"api_check: {len(paths)} paths read twice, each answer as the schema says, nothing "
                             "changed")

        # Every action
        called = set()

        def expect(method, path, body, status, code=None):
            """One action sent as given, unchecked by the client; its answer judged. Returns the reply."""
            called.add((method, unit_api.full_path(path)))
            got_status, reply = unit.request(method, path, body)
            got_code = reply.get("error", {}).get("code") if isinstance(reply, dict) else None
            self.log.write("rc", f"api_check: {method} {path} {json.dumps(body)}: {got_status} {got_code or ''}")
            if got_code is not None and got_code not in errors:
                self.fail(f"api_check: {method} {path}: the code {got_code} is not in the schema's errors")
            if got_status != status or (code is not None and got_code != code):
                self.fail(f"api_check: {method} {path} {json.dumps(body)}: {got_status} {got_code}, not {status} "
                          f"{code or ''}")
            return reply

        def wait_state(path, wanted):
            """Read path until its state is one of wanted; the last state read."""
            deadline = time.monotonic() + API_CHECK_STATE_S
            state = None
            while time.monotonic() < deadline:
                state = unit.read(path)["state"]
                if state in wanted:
                    return state
                time.sleep(0.2)
            self.fail(f"api_check: {path} stayed {state}, not {wanted}")
            return state

        def wait_event(source, kind, **carries):
            """Follow the log to the next such event; fail when what it carries is not as given."""
            def done(event):
                return (event["source"], event["kind"]) == (source, kind)

            for event in unit.follow(wait_s=2, until=done, for_s=API_CHECK_STATE_S):
                for problem in unit.event_problems(event):
                    self.fail(f"api_check: event {event}: {problem}")
                if (event["source"], event["kind"]) == (source, kind):
                    wrong = {name: event.get(name) for name, value in carries.items() if event.get(name) != value}
                    self.log.write("rc", f"api_check: event {json.dumps(event)}")
                    if wrong:
                        self.fail(f"api_check: {source} {kind} carried {wrong}, not {carries}")
                    return event
            self.fail(f"api_check: no {source} {kind} event within {API_CHECK_STATE_S:.0f} s")
            return None

        expect("POST", "charge/stop", {}, 200)
        expect("POST", "purge/stop", {}, 200)
        expect("POST", "stop", {}, 200)
        if unit.read("charge")["target"] is None:
            expect("PATCH", "charge", {"target": target}, 200)
        charge = unit.read("charge")
        expect("PATCH", "charge", {"target": charge["target"], "powder_profile": charge["powder_profile"]}, 200)
        expect("PATCH", "charge", {"target": 1000}, 400, "out_of_range")
        expect("POST", "charge/start", {"target": 0}, 400, "out_of_range")
        try:
            unit.write("charge", target=1000)
        except ValueError as error:
            self.log.write("rc", f"api_check: the client refused it before sending: {error}")
        else:
            self.fail("api_check: the client sent a target over the schema's most")
        after = unit.read("charge")
        if (after["state"], after["target"]) != ("idle", charge["target"]):
            self.fail(f"api_check: the refused writes changed the charge: {after['state']}, {after['target']}")
        expect("PATCH", "purge", {"speed_rps": 1}, 409, "not_running")
        expect("POST", "system/factory_reset", {}, 400, "confirm_missing")
        # The gate, where the build carries it (ADR-008, gate 2: the index's features); without it, its paths are gone
        carries = {feature["name"]: feature["present"] for feature in index.get("features", [])}
        if carries.get("servo_gate", True):
            gate = unit.read("gate")
            if gate["state"] == "disabled":
                expect("PATCH", "gate", {"ratio": 0.5}, 409, "gate_disabled")
            else:
                expect("PATCH", "gate", {"ratio": gate["ratio"]}, 200)
        else:
            for method, path in (("GET", "/api/v1/gate"), ("GET", "/api/v1/settings/gate")):
                if http_call(host, method, path)[0] != 404:
                    self.fail(f"api_check: the build has no servo gate, but {method} {path} answers")
            if any("gate" in action["path"] for action in schema["actions"]) or "gate" in schema["states"]:
                self.fail("api_check: the build has no servo gate, but the schema lists it")
        for path, resource in export_before["resources"].items():
            save = unit.resource(path)["save"]
            if resource["saved"] and ("POST", save) not in called:
                expect("POST", save, {}, 200)
            # Its revert (ADR-008, gate 2): of what is stored, it changes nothing
            revert = unit.resource(path).get("revert")
            if revert is not None and resource["saved"] and ("POST", revert) not in called:
                expect("POST", revert, {}, 200)
        if all(resource["saved"] for resource in export_before["resources"].values()):
            expect("POST", "settings/save", {}, 200)
        if unit.read("settings") != export_before:
            self.fail("api_check: the settings differ after the refusals and the saves")

        # A charge and a purge, by the API, with the simulated scale
        if self.send("sim on") is None:
            return
        scale = unit.read("scale")
        expect("POST", "scale/zero", {}, 200 if "zero" in scale["actions"] else 501)
        expect("POST", "charge/start", {}, 202)
        wait_event("charge", "started", by="api", target=charge["target"], powder_profile=charge["powder_profile"])
        throw_started = wait_event("throw", "started")
        wait_state("charge", ("zeroing", "charging"))
        expect("POST", "charge/start", {}, 409, "busy")
        expect("POST", "purge/start", {}, 409, "busy")
        # The owner's hello for agents, listed since ADR-008's gate 2: refused over a charge, so it never draws here
        expect("POST", "agent/hello", {"name": "api_check"}, 409, "busy")
        expect("POST", "system/restart", {}, 409, "busy")
        expect("POST", "charge/stop", {}, 202)
        wait_event("charge", "stopped", by="api", reason="stopped")
        wait_state("charge", ("idle",))
        self.api_check_recording(unit, throw_started)
        expect("POST", "purge/start", {}, 202)
        wait_event("purge", "started", by="api")
        wait_state("purge", ("running",))
        expect("PATCH", "purge", {"speed_rps": 1, "time_s": 2}, 200)
        wait_event("purge", "stopped", by="unit", reason="time")
        wait_state("purge", ("idle",))
        expect("POST", "purge/start", {}, 202)
        wait_state("purge", ("running",))
        expect("POST", "stop", {}, 202)
        wait_event("purge", "stopped", by="api", reason="stopped")
        wait_state("purge", ("idle",))
        self.send("sim off")

        # The update (ADR-004): the job's shape, nothing installed (rc_run.py update installs an image)
        expect("POST", "update/install", {}, 409, "not_ready")
        expect("POST", "update/stop", {}, 200)
        expect("POST", "update/start", {}, 202)
        wait_event("update", "started", by="api")
        wait_state("update", ("receiving",))
        expect("POST", "update/start", {}, 409, "busy")
        expect("POST", "charge/start", {}, 409, "busy")
        expect("POST", "system/restart", {}, 409, "busy")
        # An image that is none, refused in its first 4 KB: the job ends with nothing written into a slot's start
        called.add(("PUT", unit_api.full_path("update/image")))
        got_status, reply = unit.request("PUT", "update/image", data=bytes(4096),
                                         content_type="application/octet-stream")
        got_code = reply.get("error", {}).get("code") if isinstance(reply, dict) else None
        self.log.write("rc", f"api_check: PUT update/image 4 KB of zeros: {got_status} {got_code}")
        if got_status != 400 or got_code != "not_an_image":
            self.fail(f"api_check: PUT update/image of zeros: {got_status} {got_code}, not 400 not_an_image")
        wait_event("update", "stopped", by="api", reason="refused", code="not_an_image")
        wait_state("update", ("idle",))
        expect("POST", "update/start", {}, 202)
        wait_state("update", ("receiving",))
        expect("POST", "stop", {}, 202)
        wait_event("update", "stopped", by="api", reason="stopped")
        wait_state("update", ("idle",))

        # The poll (ADR-010, gate 5): asked, and waited out. A download is asked only where it cannot start: a wrong
        # type, and true only while nothing is found (rc_run.py offer installs one)
        network = unit.read("network")
        if network.get("mode") == "station":
            # The start's own poll first: just after a flash it may still check, and a second is refused 409 busy
            deadline = time.monotonic() + API_CHECK_POLL_S
            while unit.read("update")["poll"]["state"] == "checking" and time.monotonic() < deadline:
                time.sleep(0.5)
            expect("POST", "update/check", {}, 202)
            deadline = time.monotonic() + API_CHECK_POLL_S
            poll = unit.read("update")["poll"]
            while poll["state"] == "checking" and time.monotonic() < deadline:
                time.sleep(0.5)
                poll = unit.read("update")["poll"]
            self.log.write("rc", f"api_check: the poll ended {json.dumps(poll)}")
            if poll["state"] == "checking":
                self.fail(f"api_check: the poll still checks after {API_CHECK_POLL_S:.0f} s")
            self.api_check_poll_room()
        else:
            expect("POST", "update/check", {}, 409, "no_network")

        # The set-up's scan and join (ADR-008, gate 4): a scan asked and waited out; a join's body checked first,
        # then, on a unit on its network, refused, as it would leave it. A join that runs is the set-up's proof's
        expect("POST", "wifi/scan", {}, 202)
        deadline = time.monotonic() + 15.0
        heard = unit.read("wifi/networks")
        while heard["scanning"] and time.monotonic() < deadline:
            time.sleep(0.5)
            heard = unit.read("wifi/networks")
        self.log.write("rc", f"api_check: the scan heard {len(heard['networks'])} networks")
        if heard["scanning"]:
            self.fail("api_check: the scan still runs after 15 s")
        expect("POST", "wifi/join", {"ssid": "api_check", "password": "short", "auth": "wpa2_mixed"}, 400, "out_of_range")
        expect("POST", "wifi/join", {"ssid": "api_check", "auth": "wep"}, 400, "not_a_choice")
        if network.get("mode") == "station":
            expect("POST", "wifi/join", {"ssid": "api_check", "password": "api_check_pw", "auth": "wpa2_mixed"}, 409,
                   "not_on_hotspot")
        # The set-up's step (gate 4's design): a step refused, then one taken; the screen follows it only on the
        # unit's own hotspot, so on its network nothing moves on the screen
        expect("PATCH", "setup", {"step": "done"}, 400, "not_a_choice")
        stepped = expect("PATCH", "setup", {"step": "wifi", "network": None}, 200)
        if stepped.get("step") != "wifi":
            self.fail(f"api_check: PATCH setup answered {stepped}")
        # The set-up done: on a unit on its network, remembered and nothing else (no hotspot to stop)
        expect("POST", "setup/done", {"now": True}, 400, "unknown_field")
        if network.get("mode") == "station":
            done = expect("POST", "setup/done", {}, 200)
            if done != {"set_up": True, "hotspot": False} or unit.read("system").get("set_up") is not True:
                self.fail(f"api_check: setup/done answered {done}, set_up {unit.read('system').get('set_up')}")
        expect("POST", "update/start", {"download": "yes"}, 400, "wrong_type")
        if unit.read("update")["found"] is None:
            expect("POST", "update/start", {"download": True}, 409, "not_found")

        # Last, a restart: a new boot, and the system's start the log's first event
        boot = index["boot"]
        expect("POST", "system/restart", {}, 202)
        time.sleep(1.5)
        self.wait_for_console()
        unit.forget()
        deadline = time.monotonic() + API_CHECK_BACK_S
        back = None
        while back is None and time.monotonic() < deadline:
            try:
                back = unit.index
            except OSError:
                time.sleep(1.0)
        if back is None:
            self.fail(f"api_check: the unit did not answer within {API_CHECK_BACK_S:.0f} s of its restart")
            return
        first = unit.events(after=0)["events"][0]
        self.log.write("rc", f"api_check: back after the restart, boot {boot} then {back['boot']}, its first event "
                             f"{json.dumps(first)}")
        if back["boot"] == boot or (first["seq"], first["source"], first["kind"]) != (1, "system", "started") or \
                first["firmware"] != back["firmware"]["version"]:
            self.fail("api_check: after the restart, not a new boot with the system's start first")
        if unit.read("settings") != export_before:
            self.fail("api_check: the settings differ after the restart")

        listed = {(action["method"], action["path"]) for action in schema["actions"]}
        for method, path in sorted(listed - called):
            self.fail(f"api_check: the schema's {method} {path} was not called")
        self.log.write("rc", f"api_check: {len(called & listed)} of the schema's {len(listed)} actions called")

        # The API describes itself (ADR-008, gate 2): a GET of every action that has no GET of its own gives its
        # usage with an example; every route the server holds is in the index, the schema, or a named exception; an
        # error says where to read
        gets = set(unit.paths("GET"))
        usages = 0
        for action in schema["actions"]:
            if action["path"] in gets or "{id}" in action["path"]:
                continue
            status, reply = http_call(host, "GET", action["path"])
            usage = (reply or {}).get("usage") or {}
            if status != 200 or usage.get("path") != action["path"] or "example" not in usage or "see" not in reply:
                self.fail(f"api_check: GET {action['path']}: {status}, not its usage with an example: {reply}")
            usages += 1
        self.log.write("rc", f"api_check: {usages} actions' usage read by their GET")
        status, reply = http_call(host, "GET", "/api/v1/nope")
        if status != 404 or (reply or {}).get("error", {}).get("see") != "/api/v1":
            self.fail(f"api_check: a path not there: {status} {reply}, not 404 with see /api/v1")
        status, reply = http_call(host, "PATCH", "/api/v1/stop", {})
        if status != 405 or (reply or {}).get("error", {}).get("see") != "/api/v1/stop":
            self.fail(f"api_check: a method a path does not take: {status} {reply}, not 405 with see the path")
        status, routes = http_call(host, "GET", "/api/v1/debug/routes")
        known = gets | set(unit.paths("POST")) | set(unit.paths("PATCH")) | set(unit.paths("PUT")) |             {action["path"] for action in schema["actions"]} | {r["path"] for r in schema["resources"]} | \
            {entry["path"] for entry in index["resources"]}
        not_listed = [f"{method} {path}" for method, path in (routes or {}).get("routes", [])
                      if path not in known and path not in API_CHECK_UNLISTED and not path.startswith("/api/v1/debug")]
        self.log.write("rc", f"api_check: {len((routes or {}).get('routes', []))} routes, not listed: {not_listed}")
        if status != 200 or not_listed:
            self.fail(f"api_check: routes in neither the index nor the schema: {not_listed}")

    @staticmethod
    def api_stable(reply):
        """A reply without what moves between two reads with nothing done (API_CHECK_VOLATILE); bytes by length."""
        if isinstance(reply, bytes):
            return len(reply)
        return {name: value for name, value in reply.items() if name not in API_CHECK_VOLATILE}

    @staticmethod
    def api_answer_problems(unit, path, reply, frame_bytes):
        """
        An answer against the schema's tables.

        Args:
            unit: the unit_api.Unit.
            path: the path read.
            reply: what it answered.
            frame_bytes: the display frame's length.

        Returns:
            The problems found; empty when there is none.
        """
        resource = unit.resource(path)
        if resource is not None:
            return unit_api.fields_problems(resource["fields"], reply, writing=False)
        name = path[len(unit_api.API) + 1:]
        if name == "settings":
            return [f"{inner}: {problem}" for inner, fields in reply["resources"].items()
                    for problem in unit_api.fields_problems(unit.resource(inner)["fields"], fields, writing=False)]
        if name in ("charge", "purge", "gate", "update"):
            return [] if reply["state"] in unit.states(name) else [f"state {reply['state']} is not in the schema's"]
        if name == "motors":
            return [f"{motor}'s driver {reply[motor]['driver']} is not in the schema's" for motor in ("coarse", "fine")
                    if reply[motor]["driver"] not in unit.states("driver")]
        if name == "settings/motors":
            # The motors listed are the motors' settings resources the schema has, by their ids and labels
            prefix = unit_api.full_path("settings/motors/")
            listed = {motor["id"]: motor["label"] for motor in reply["motors"]}
            held = {resource["path"][len(prefix):]: resource["label"] for resource in unit.schema["resources"]
                    if resource["path"].startswith(prefix)}
            return [] if listed == held else [f"it lists {listed}; the schema's motors are {held}"]
        if name == "system":
            return [] if reply["activity"] in unit.states("activity") else [f"activity {reply['activity']}"]
        if name == "events":
            return [problem for event in reply["events"] for problem in unit.event_problems(event)]
        if name == "display/frame":
            return [] if isinstance(reply, bytes) and len(reply) == frame_bytes else ["not the display's frame"]
        # The recordings (ADR-009, gate 2's rest; api_v1.md, "Recordings")
        entry_fields = {"boot", "id", "kind", "job", "throw", "tag", "started_at_ms", "duration_ms", "rows",
                        "complete", "truncated", "samples_kept", "summary"}
        if name == "recordings":
            ids = [entry["id"] for entry in reply["recordings"]]
            return ([f"recording {entry.get('id')} lacks {sorted(entry_fields - set(entry))}"
                     for entry in reply["recordings"] if entry_fields - set(entry)] +
                    ([] if ids == sorted(ids, reverse=True) else [f"not newest first: {ids}"]))
        if re.fullmatch(r"recordings/\d+", name):
            missing = (entry_fields | {"settings", "marks"}) - set(reply)
            return [f"lacks {sorted(missing)}"] if missing else []
        if re.fullmatch(r"recordings/\d+/samples", name):
            widths = {len(row) for row in reply["rows"]}
            return [] if widths <= {len(reply["columns"])} else [f"rows of {sorted(widths)} for "
                                                                   f"{len(reply['columns'])} columns"]
        return []

    def stream_check(self, host):
        """
        The event stream's life cycle (ADR-008, gate 2), each stage with its expected end: the start (the unit, then
        the state); an event as it happens, with its id; a reconnect that goes on from its Last-Event-ID, nothing
        missed; one whose event is no longer kept, read afresh; the live weight; the state again after 15 s quiet; the
        cap; a client that stops reading, closed by the stall limit and its heap given back; a Wi-Fi leave and a
        restart, each ending every stream, the restart's reconnect read afresh. Nothing stored changes.

        Args:
            host: the unit's address.
        """
        def debug():
            status, reply = http_call(host, "GET", "/api/v1/debug")
            return reply["server"] if status == 200 else None

        def wait_back(boot=None):
            deadline = time.monotonic() + STREAM_BACK_S
            while time.monotonic() < deadline:
                try:
                    status, index = http_call(host, "GET", "/api/v1")
                    if status == 200 and (boot is None or index["boot"] != boot):
                        return index
                except OSError:
                    pass
                time.sleep(1.0)
            return None

        index = wait_back()
        if index is None:
            self.fail("stream_check: the unit does not answer")
            return
        boot = index["boot"]
        self.send("setting settings/charge decimal_places=2")

        # The start: the unit, read afresh, then the state
        stream = EventStream(host)
        unit = stream.until("unit", 5)
        state = stream.until("state", 5)
        self.log.write("rc", f"stream_check: start {stream.status}, unit {unit and unit[2]}")
        if stream.status != 200 or unit is None or state is None:
            self.fail("stream_check: the start is not 200, the unit, then the state")
            return
        start = json.loads(unit[2])
        if start["boot"] != boot or start["resumed"]:
            self.fail(f"stream_check: the start's unit is not this boot, read afresh: {unit[2]}")

        # An event as it happens, with its id
        self.send("setting settings/charge decimal_places=3")
        sent_at = time.monotonic()
        message = stream.until("message", 5)
        if message is None:
            self.fail("stream_check: no event within 5 s of a setting changed")
            return
        event = json.loads(message[2])
        self.log.write("rc", f"stream_check: event {message[1]} {message[2]} after {message[3] - sent_at:.2f} s")
        if (event["source"], event["kind"], event["by"]) != ("setting", "changed", "console") or \
                message[1] != f"{boot}-{event['seq']}":
            self.fail(f"stream_check: the event or its id is not the setting changed: {message[1]} {message[2]}")
        last_id = message[1]
        stream.close()

        # A reconnect from its last id: what came meanwhile first, nothing missed
        self.send("setting settings/charge decimal_places=2")
        stream = EventStream(host, last_id)
        unit = stream.until("unit", 5)
        missed = stream.until("message", 5)
        self.log.write("rc", f"stream_check: reconnect from {last_id}: {unit and unit[2]}, then {missed and missed[1]}")
        if unit is None or not json.loads(unit[2])["resumed"] or missed is None or \
                missed[1] != f"{boot}-{event['seq'] + 1}":
            self.fail("stream_check: a reconnect from its last id did not go on from it")
        stream.close()

        # A reconnect whose event is no longer kept: read afresh
        for i in range(70):
            self.send(f"setting settings/charge decimal_places={3 if i % 2 == 0 else 2}")
        stream = EventStream(host, last_id)
        unit = stream.until("unit", 5)
        self.log.write("rc", f"stream_check: reconnect from {last_id} after 70 events: {unit and unit[2]}")
        if unit is None or json.loads(unit[2])["resumed"]:
            self.fail("stream_check: a reconnect from an event no longer kept was not read afresh")

        # The live weight: the simulated scale's weight set, the state with it within a second
        self.send("sim on")
        self.send("sim weight 12.5")
        set_at = time.monotonic()
        seen = None
        while seen is None and time.monotonic() - set_at < 3:
            message = stream.until("state", 3)
            if message is not None and message[0] == "state" and \
                    abs((json.loads(message[2])["charge"]["weight"] or 0) - 12.5) < 0.01:
                seen = message[3] - set_at
        self.log.write("rc", f"stream_check: the weight 12.5 seen on the stream after {seen} s")
        if seen is None or seen > 1.0:
            self.fail("stream_check: the weight set was not on the stream within 1 s")
        self.send("sim weight off")
        self.send("sim off")

        # Quiet: the state again once the keep-alive's time passes with nothing sent (not a comment: a browser's
        # event source never shows its page one). The scale may settle first after the simulation: the gap counts
        last_at, took, kind = time.monotonic(), None, None
        deadline = last_at + 3 * STREAM_KEEP_ALIVE_S
        while time.monotonic() < deadline:
            message = stream.next(STREAM_KEEP_ALIVE_S + 5)
            if message is None or message[0] == "closed":
                break
            gap, last_at = message[3] - last_at, message[3]
            if gap >= STREAM_KEEP_ALIVE_S - 1:
                took, kind = gap, message[0]
                break
        self.log.write("rc", f"stream_check: quiet, then {kind} after {took} s")
        if took is None or kind != "state" or took > STREAM_KEEP_ALIVE_S + 2:
            self.fail("stream_check: no state again about 15 s into a quiet stream")

        # The cap: this one and two more open, a fourth refused with when to try again
        more = [EventStream(host), EventStream(host)]
        fourth = EventStream(host)
        retry = fourth.reply.getheader("Retry-After")
        self.log.write("rc", f"stream_check: the cap: {[s.status for s in more]}, the fourth {fourth.status} "
                             f"Retry-After {retry}; {debug()}")
        if [s.status for s in more] != [200, 200] or fourth.status != 503 or retry is None:
            self.fail("stream_check: three streams not taken, or the fourth not refused with Retry-After")
        for s in more + [fourth, stream]:
            s.close()
        time.sleep(1.0)

        # A client that stops reading: the weight changed ten times a second fills its connection; the unit closes it
        # within the stall limit of its send buffer staying full, and lwIP's heap is given back
        before = debug()
        net_before = self.send("net")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)
        sock.settimeout(5)
        sock.connect((host, 80))
        sock.sendall(f"GET /api/v1/events/stream HTTP/1.1\r\nHost: {host}\r\n\r\n".encode())
        self.send("sim on")
        closed_after = None
        opened = time.monotonic()
        weight = 0.0
        while time.monotonic() - opened < 60:
            weight += 0.1
            self.send(f"sim weight {weight:.1f}")
            server = debug()
            if server is not None and server["slots_live"] == 0:
                closed_after = time.monotonic() - opened
                break
            time.sleep(0.1)
        self.send("sim weight off")
        self.send("sim off")
        sock.close()
        after = debug()
        self.log.write("rc", f"stream_check: a client that stops reading: closed after {closed_after} s; "
                             f"stalled {before['aborted_stalled']} then {after['aborted_stalled']}")
        if closed_after is None or after["aborted_stalled"] <= before["aborted_stalled"]:
            self.fail("stream_check: a stream whose client stopped reading was not closed by the stall limit")
        time.sleep(2.0)
        net_after = self.send("net")
        if net_before and net_after:
            used = [int(re.search(r"heap_used=(\d+)", line).group(1)) for line in (net_before, net_after)]
            self.log.write("rc", f"stream_check: lwIP's heap in use {used[0]} before, {used[1]} after")
            if used[1] > used[0] + 1024:
                self.fail("stream_check: lwIP's heap was not given back after the stalled stream")

        # The network changes: a unit that leaves its Wi-Fi, or restarts, sends no close; the client knows it by
        # hearing nothing past the state that comes again after 15 s quiet, and opens the stream again

        def last_event(stream):
            self.send("setting settings/charge decimal_places=3")
            message = stream.until("message", 5)
            self.send("setting settings/charge decimal_places=2")
            message_back = stream.until("message", 5)
            return (message_back or message or (None, f"{boot}-1"))[1]

        def silent(stream, what):
            """Nothing past the keep-alive and a margin, or closed: the client's cue to open it again"""
            started = time.monotonic()
            heard = []
            while time.monotonic() - started < 60:
                message = stream.next(STREAM_KEEP_ALIVE_S + 5)
                if message is None or message[0] == "closed":
                    self.log.write("rc", f"stream_check: {what}: {'closed' if message else 'silent'} after "
                                         f"{time.monotonic() - started:.1f} s, having heard {heard}")
                    return True
                heard.append(message[0])
            self.fail(f"stream_check: {what}: the old stream still talks after 60 s")
            return False

        # A Wi-Fi leave: back within a minute, the same boot, and the reconnect goes on from its last id
        stream = EventStream(host)
        stream.until("state", 5)
        last_id = last_event(stream)
        self.send("wifi leave")
        silent(stream, "a Wi-Fi leave")
        stream.close()
        if wait_back() is None:
            self.fail("stream_check: the unit did not come back after the Wi-Fi leave")
            return
        self.send("setting settings/charge decimal_places=3")
        self.send("setting settings/charge decimal_places=2")
        stream = EventStream(host, last_id)
        unit = stream.until("unit", 5)
        missed = stream.until("message", 5)
        self.log.write("rc", f"stream_check: reconnect from {last_id} after the Wi-Fi leave: {unit and unit[2]}, "
                             f"then {missed and missed[1]}")
        if unit is None or not json.loads(unit[2])["resumed"] or missed is None:
            self.fail("stream_check: after a Wi-Fi leave the reconnect did not go on from its last id")
        # The old stream's slot freed: by the client's close, or the stall limit once its bytes find no client
        deadline = time.monotonic() + 3 * STREAM_STALL_S
        live = None
        while time.monotonic() < deadline:
            live = debug()["slots_live"]
            if live <= 1:
                break
            time.sleep(1.0)
        self.log.write("rc", f"stream_check: streams open after the Wi-Fi leave: {live}")
        if live is None or live > 1:
            self.fail("stream_check: the stream from before the Wi-Fi leave still holds its slot")
        stream.close()

        # A restart: the reconnect from its last id is read afresh, of the new boot
        stream = EventStream(host)
        stream.until("state", 5)
        last_id = last_event(stream)
        status, _ = http_call(host, "POST", "/api/v1/system/restart", {})
        self.log.write("rc", f"stream_check: a restart asked ({status})")
        silent(stream, "a restart")
        stream.close()
        time.sleep(1.5)
        self.wait_for_console()
        back = wait_back(boot)
        if back is None:
            self.fail("stream_check: the unit did not come back after its restart")
            return
        stream = EventStream(host, last_id)
        unit = stream.until("unit", 5)
        self.log.write("rc", f"stream_check: reconnect from {last_id} after the restart: {unit and unit[2]}")
        if unit is None or json.loads(unit[2])["resumed"] or json.loads(unit[2])["boot"] != back["boot"]:
            self.fail("stream_check: after a restart the reconnect was not read afresh, of the new boot")
        stream.close()

    def web_robust(self, host):
        """
        Requests that are wrong, cut off or awkward (ADR-003, gate 2), each answered or closed, with the unit still
        answering after each and its console still counting `up=`. Then a client that stops reading part way
        through a page, and one that sends less of a body than it said: each must be aborted by the stall limit,
        and lwIP's heap given back.

        Args:
            host: the unit's address.
        """
        own = f"Host: {host}\r\n".encode()

        def write(method, path, body, content_type=b"application/json"):
            return (method + b" " + path + b" HTTP/1.1\r\n" + own + b"Content-Type: " + content_type +
                    b"\r\nContent-Length: " + str(len(body)).encode() + b"\r\nConnection: close\r\n\r\n" + body)

        # The writes (gate 3) are each refused, or change nothing: the unit's settings are as they were
        leds = b"/api/v1/settings/leds"
        cases = [
            ("a write that is not JSON", write(b"PATCH", leds, b"{nope"), 400),
            ("a JSON list", write(b"PATCH", leds, b"[1,2]"), 400),
            ("a field that is an object", write(b"PATCH", leds, b'{"led1_colour":{"r":1}}'), 400),
            ("a field given twice", write(b"PATCH", leds, b'{"pwm3_chain_count":2,"pwm3_chain_count":3}'), 400),
            ("an unknown field", write(b"PATCH", leds, b'{"nope":1}'), 400),
            ("saved, sent back with a GET's fields", write(b"PATCH", leds, b'{"saved":true}'), 400),
            ("a value of the wrong type", write(b"PATCH", leds, b'{"pwm3_chain_count":"2"}'), 400),
            ("a PATCH of text", write(b"PATCH", leds, b"{}", b"text/plain"), 415),
            ("an empty PATCH", write(b"PATCH", leds, b"{}"), 200),
            ("a save with a field", write(b"POST", leds + b"/save", b'{"x":1}'), 400),
            ("no such powder profile", write(b"PATCH", b"/api/v1/powder_profiles/9", b"{}"), 404),
            ("a value with no '='", b"GET /api/v1/charge?ee HTTP/1.1\r\n" + own + b"Connection: close\r\n\r\n", 200),
            ("an old /rest/ path, gone in gate 4", b"GET /rest/charge_mode_state HTTP/1.1\r\n" + own +
             b"Connection: close\r\n\r\n", 404),
            ("a Cookie of 6 KB, passed over", b"GET /api/v1/charge HTTP/1.1\r\n" + own +
             b"Cookie: a=" + b"x" * 6000 + b"\r\nConnection: close\r\n\r\n", 200),
            ("headers over 8 KB", b"GET /api/v1/charge HTTP/1.1\r\n" + own + b"Cookie: a=" + b"x" * 5000 +
             b"\r\nCookie: b=" + b"y" * 4000 + b"\r\n\r\n", 413),
            ("a Host of 300 bytes", b"GET /api/v1/charge HTTP/1.1\r\nHost: " + b"h" * 300 + b"\r\n\r\n", 413),
            ("a request line of 700 bytes", b"GET /api/v1/charge?x=" + b"a" * 700 + b" HTTP/1.1\r\n" + own +
             b"\r\n", 414),
            ("an unknown method", b"BREW /api/v1/charge HTTP/1.1\r\n" + own + b"\r\n", 405),
            ("OPTIONS (no CORS)", b"OPTIONS /api/v1/charge HTTP/1.1\r\n" + own + b"Origin: http://evil.example\r\n"
             b"\r\n", 405),
            ("HTTP/2.0", b"GET /api/v1/charge HTTP/2.0\r\n" + own + b"\r\n", 400),
            ("another Host, the API", b"GET /api/v1/charge HTTP/1.1\r\nHost: evil.example\r\n\r\n", 403),
            ("another Host, an old path", b"GET /rest/charge_mode_state HTTP/1.1\r\nHost: evil.example\r\n\r\n",
             403),
            ("another Host, a page", b"GET / HTTP/1.1\r\nHost: evil.example\r\nConnection: close\r\n\r\n", 302),
            ("a POST with no type", b"POST /api/v1/charge HTTP/1.1\r\n" + own + b"Content-Length: 0\r\n\r\n", 415),
            ("a POST of text from a form", b"POST /api/v1/charge HTTP/1.1\r\n" + own +
             b"Content-Type: text/plain\r\nContent-Length: 2\r\n\r\n{}", 415),
            ("a JSON POST to a path that only reads", b"POST /api/v1/charge HTTP/1.1\r\n" + own +
             b"Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}", 405),
            ("a body longer than it says", b"POST /api/v1/charge HTTP/1.1\r\n" + own +
             b"Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}" + b"z" * 20, 405),
            ("a chunked body", b"POST /api/v1/charge HTTP/1.1\r\n" + own + b"Content-Type: application/json\r\n"
             b"Transfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n", 411),
            ("a body of 5,000 bytes", b"POST /api/v1/charge HTTP/1.1\r\n" + own +
             b"Content-Type: application/json\r\nContent-Length: 5000\r\n\r\n", 413),
            ("a request cut off", b"GET /api/v1/ch", None),
            ("no such path", b"GET /nothing HTTP/1.1\r\n" + own + b"\r\n", 404),
            ("HTTP/1.0 with no Host", b"GET /api/v1/charge HTTP/1.0\r\n\r\n", 200),
        ]
        before = self.web_debug(host)
        for name, payload, want in cases:
            try:
                status, headers, _, _ = raw_exchange(host, payload, timeout_s=3.0)
            except OSError as error:
                status, headers = None, {}
                self.log.write("rc", f"web_robust: {name}: {error}")
            ok = status == want
            if want == 302 and ok:
                ok = headers.get("location") == f"http://{host}/"
            self.log.write("rc", f"web_robust: {name}: {status} (want {want}){'' if ok else ' FAIL'}")
            if not ok:
                self.fail(f"web_robust: {name}: {status}, not {want}")
            try:
                json.loads(http_get(host, "api/v1/charge"))
            except (OSError, ValueError) as error:
                self.fail(f"web_robust: the unit did not answer after \"{name}\": {error}")

        # The page's ETag: unchanged since, so 304 and no body
        status, headers, _, _ = raw_exchange(host, b"GET / HTTP/1.1\r\n" + own + b"Connection: close\r\n\r\n")
        etag = headers.get("etag", "")
        status, _, body, _ = raw_exchange(host, b"GET / HTTP/1.1\r\n" + own + b"If-None-Match: " + etag.encode() +
                                          b"\r\nConnection: close\r\n\r\n")
        self.log.write("rc", f"web_robust: the page again with its ETag {etag}: {status}, {len(body)} bytes")
        if status != 304 or body:
            self.fail(f"web_robust: the page with its ETag gave {status} and {len(body)} bytes, not 304 and none")

        # The schema's ETag (gate 3): a stream, answered 304 with nothing made
        status, headers, _, _ = raw_exchange(host, b"GET /api/v1/schema HTTP/1.1\r\n" + own +
                                             b"Connection: close\r\n\r\n")
        etag = headers.get("etag", "")
        status, _, body, _ = raw_exchange(host, b"GET /api/v1/schema HTTP/1.1\r\n" + own + b"If-None-Match: " +
                                          etag.encode() + b"\r\nConnection: close\r\n\r\n")
        self.log.write("rc", f"web_robust: the schema again with its ETag {etag}: {status}, {len(body)} bytes")
        if not etag or status != 304 or body:
            self.fail(f"web_robust: the schema with its ETag {etag!r} gave {status} and {len(body)} bytes, not 304")

        # Seven clients at once, each with a request: six slots, so the seventh waits its turn or takes an idle one
        socks = []
        for _ in range(7):
            sock = socket.create_connection((host, 80), timeout=10.0)
            sock.sendall(b"GET /api/v1/charge HTTP/1.1\r\n" + own + b"\r\n")
            socks.append(sock)
        answered = 0
        for sock in socks:
            try:
                data = sock.recv(4096)
                answered += data.startswith(b"HTTP/1.1 200")
            except OSError:
                pass
            sock.close()
        self.log.write("rc", f"web_robust: seven clients at once: {answered} answered")
        if answered != 7:
            self.fail(f"web_robust: seven clients at once: {answered} of 7 answered")

        # Parked requests (gate 2's slot, for ?wait): woken by their condition, by their time, and with two held a
        # third answered at once
        def wait_request(until_ms, wait_s, results, key):
            started = time.monotonic()
            try:
                reply = json.loads(http_get(host, f"api/v1/debug/wait?until_ms={until_ms}&wait={wait_s}"))
                results[key] = (reply.get("woke"), time.monotonic() - started, reply.get("uptime_ms", 0))
            except (OSError, ValueError) as error:
                results[key] = (f"no answer: {error}", time.monotonic() - started, 0)

        results = {}
        wait_request(0, 1, results, "now")
        uptime = results["now"][2]
        wait_request(uptime + 1500, 4, results, "condition")
        wait_request(uptime + 10_000_000, 2, results, "time")
        threads = [threading.Thread(target=wait_request, args=(uptime + 10_000_000, 3, results, f"held {n}"))
                   for n in (1, 2)]
        for thread in threads:
            thread.start()
        time.sleep(0.5)
        wait_request(uptime + 10_000_000, 3, results, "third")
        for thread in threads:
            thread.join()
        for key in ("condition", "time", "held 1", "held 2", "third"):
            woke, took, _ = results[key]
            self.log.write("rc", f"web_robust: parked, {key}: woke by {woke} after {took:.2f} s")
        checks = [
            results["condition"][0] == "condition" and 1.2 < results["condition"][1] < 2.5,
            results["time"][0] == "time" and 1.8 < results["time"][1] < 3.0,
            all(results[f"held {n}"][0] == "time" and 2.8 < results[f"held {n}"][1] < 4.0 for n in (1, 2)),
            results["third"][0] == "time" and results["third"][1] < 1.0,
        ]
        if not all(checks):
            self.fail(f"web_robust: parked requests not as expected: {checks}")

        # A client that stops reading part way through a page, and one that sends less of a body than it said
        _, _, _, stalled_read = raw_exchange(host, b"GET / HTTP/1.1\r\n" + own + b"\r\n", rcvbuf=1024,
                                             read_limit=1024)
        short = socket.create_connection((host, 80), timeout=5.0)
        short.sendall(b"POST /api/v1/charge HTTP/1.1\r\n" + own + b"Content-Type: application/json\r\n"
                      b"Content-Length: 100\r\n\r\n{\"a\":")
        time.sleep(14.0)
        after = self.web_debug(host)
        for sock in (stalled_read, short):
            try:
                sock.close()
            except OSError:
                pass
        if before is None or after is None:
            self.fail("web_robust: api/v1/debug did not answer")
            return
        aborted = after["server"]["aborted_stalled"] - before["server"]["aborted_stalled"]
        self.log.write("rc", f"web_robust: stalled and aborted: {aborted}; lwIP's heap {after['net']['heap']}; "
                             f"server {after['server']}")
        if aborted < 2:
            self.fail(f"web_robust: {aborted} of the 2 stalled connections aborted within 14 s")
        state = self.state()
        if state is None:
            self.fail("web_robust: the console did not answer at the end")

    def soak(self, host, seconds, target, threshold, max_miss_share, max_miss_run, load=False, upload=False,
             stream=False):
        """
        Charges back to back with the simulated scale, each judged as charge_cycle judges one, while two
        browsers use the web pages (SOAK_BROWSERS and the intervals above); `state` and `stats` between charges.
        Fails on a failed charge, on a restart or an assert, on more than max_miss_share of the web reads with
        no right answer or max_miss_run in a row on one poller, when heap_free read quiet after the run is lower
        than read quiet before it (memory taken and not given back), and when lwIP's heap or a pool failed an
        allocation during the run (`net`, ADR-003). Misses are summed by kind: timeout, reset, wrong answer.

        Args:
            host: the unit's address.
            seconds: how long; the charge under way when the time is up is finished.
            target: each charge's weight.
            threshold: the unit's fine stop threshold.
            max_miss_share: the most of the web reads that may miss, 0 to 1.
            max_miss_run: the most misses in a row one poller may have.
            load: also a tuning session's load (SOAK_LOAD_*).
            upload: also ADR-004's image, SOAK_UPLOAD_BYTES sent back to back, each checked by its CRC-32.
            stream: each browser follows the event stream (ADR-008, gate 2) in place of polling the charge.
        """
        page_size = {}

        def read_page():
            body = http_get(host, "")
            page_size.setdefault("first", len(body))
            return len(body) == page_size["first"] and len(body) > 0

        def read_state():
            return "state" in json.loads(http_get(host, "api/v1/charge"))

        def read_setting(path):
            # A settings resource carries saved; the export, its resources
            return lambda: bool(set(json.loads(http_get(host, path))) & {"saved", "resources"})

        def read_mirror_right():
            return len(http_get(host, "api/v1/display/frame")) == MIRROR_BYTES

        def read_load():
            reply = json.loads(http_get(host, f"api/v1/debug/load?rows={SOAK_LOAD_ROWS}&columns={SOAK_LOAD_COLUMNS}"))
            return len(reply["rows"]) == SOAK_LOAD_ROWS and len(reply["rows"][-1]) == SOAK_LOAD_COLUMNS

        pulled = set()

        def read_recordings():
            # A tuner's client (ADR-009, gate 2's rest): each throw's samples pulled whole as it completes
            listed = json.loads(http_get(host, "api/v1/recordings"))["recordings"]
            for entry in listed:
                if entry["id"] in pulled or not entry["complete"] or not entry["samples_kept"]:
                    continue
                samples = json.loads(http_get(host, f"api/v1/recordings/{entry['id']}/samples"))
                pulled.add(entry["id"])
                if len(samples["rows"]) != entry["rows"] or not samples["complete"]:
                    return False
            return True

        upload_data = os.urandom(SOAK_UPLOAD_BYTES)
        upload_crc = zlib.crc32(upload_data)

        def read_upload():
            status, reply = http_put_bytes(host, "/api/v1/debug/upload", upload_data, SOAK_UPLOAD_TIMEOUT_S)
            return status == 200 and reply["bytes"] == len(upload_data) and reply["crc32"] == upload_crc

        pollers = []
        streams = []
        for browser in range(1, SOAK_BROWSERS + 1):
            pollers.append(WebPoller(f"browser {browser} mirror", SOAK_MIRROR_S, [("", read_mirror_right)], self.log))
            if stream:
                streams.append(StreamFollower(f"browser {browser} stream", host, self.log))
            else:
                pollers.append(WebPoller(f"browser {browser} charge state", SOAK_STATE_S, [("", read_state)],
                                         self.log))
        pollers.append(WebPoller("portal page", SOAK_PAGE_S, [("", read_page)], self.log))
        pollers.append(WebPoller("settings", SOAK_SETTINGS_S,
                                 [(path, read_setting(path)) for path in SETTINGS_PAGE_PATHS + [SETTINGS_EXPORT]],
                                 self.log))
        if load:
            pollers.append(WebPoller("tuning load", 0.0, [("", read_load)], self.log))
            pollers.append(WebPoller("fast client", SOAK_LOAD_FAST_S, [("", read_state)], self.log))
            pollers.append(WebPoller("recordings", SOAK_RECORDINGS_S, [("", read_recordings)], self.log))
        if upload:
            pollers.append(WebPoller("upload 700 KB", 0.0, [("", read_upload)], self.log))

        followers = [EventFollower(f"events {n}", host, self.log) for n in range(1, SOAK_FOLLOWERS + 1)]

        # The first charge after a start creates the charge screen's task, kept for good (charge_mode.cpp): read
        # quiet before the run, its 1,168 bytes were memory not given back (ADR-003, gate 2). So one charge first
        self.charge_cycle(target, threshold, timeout_s=120.0)
        if self.failures:
            return
        quiet_start = self.stats()
        motor_start = self.send("motor")
        net_start = self.net()
        if net_start is not None:
            self.log.write("rc", "soak: net at the start: " + " ".join(f"{k}={v}" for k, v in net_start.items()))
        started = time.monotonic()
        heap = []   # (seconds since the start, heap_free)
        cycles = 0
        # The followers' starting point, taken while the unit is quiet: both from the same event, and a read that
        # fails here ends the soak rather than leaving a follower that never began (two did, the first time)
        for follower in followers:
            follower.unit.start_following()
        for follower in streams:
            follower.start_following()
        followers += streams
        for poller in pollers + followers:
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
        for poller in pollers + followers:
            poller.stop_event.set()
        for poller in pollers:
            poller.join(timeout=max(HTTP_TIMEOUT_S * (len(SETTINGS_PAGE_PATHS) + 1), SOAK_UPLOAD_TIMEOUT_S) + 1)
        for follower in followers:
            follower.join(timeout=max(2 * SOAK_EVENTS_WAIT_S + HTTP_TIMEOUT_S, STREAM_KEEP_ALIVE_S + 10))
        time.sleep(SOAK_QUIET_S)
        quiet_end = self.stats()

        self.log.write("rc", f"soak: {cycles} charges in {time.monotonic() - started:.0f} s")
        if load:
            self.log.write("rc", f"soak: {len(pulled)} recordings' samples pulled whole")
        good = sum(poller.good for poller in pollers)
        missed = sum(poller.missed for poller in pollers)
        kinds = collections.Counter()
        for poller in pollers:
            kinds.update(poller.kinds)
            self.log.write("rc", f"soak: {poller.name}: {poller.good} right, {poller.missed} missed, "
                                 f"longest run {poller.run_longest}; {poller.times()}")
        share = missed / (good + missed) if good + missed else 1.0
        self.log.write("rc", f"soak: {missed} of {good + missed} web reads missed ({share:.2%}), longest run "
                             f"{max(poller.run_longest for poller in pollers)}")
        self.log.write("rc", "soak: misses by kind: "
                             + (", ".join(f"{kind} {count}" for kind, count in kinds.most_common()) or "none"))
        # Each follower saw every event, the same as the other: the charges' and each throw's, with no gap
        seen = [[event["seq"] for event in follower.events] for follower in followers]
        for follower in followers:
            throws = sum(1 for event in follower.events if (event["source"], event["kind"]) == ("throw", "finished"))
            self.log.write("rc", f"soak: {follower.name}: {len(follower.events)} events, {throws} throws finished, "
                                 f"{follower.failed_reads} reads failed and tried again"
                                 + (f", {follower.states} states" if isinstance(follower, StreamFollower) else ""))
            for problem in follower.problems():
                self.fail(f"soak: {follower.name}: {problem}")
            if throws < cycles:
                self.fail(f"soak: {follower.name} saw {throws} throws finished in {cycles} charges")
        if any(numbers != seen[0] for numbers in seen):
            self.fail("soak: the event followers did not see the same events")
        net_end = self.net()
        motor_end = self.send("motor")
        late = {}
        for reply, at in ((motor_start, "start"), (motor_end, "end")):
            if reply is not None:
                self.log.write("rc", f"soak: motor at the {at}: {reply}")
                late[at] = {key: int(value) for key, value in re.findall(r"\b(\w+_underruns)=(\d+)", reply)}
        if "start" in late and "end" in late:
            for key, value in late["end"].items():
                if value > late["start"].get(key, 0):
                    self.fail(f"soak: {key} rose by {value - late['start'].get(key, 0)}: step pulses were fed late")
        if net_start is not None and net_end is not None:
            self.log.write("rc", "soak: net at the end: " + " ".join(f"{k}={v}" for k, v in net_end.items()))
            for key in (key for key in net_end if key.endswith("_err")):
                rise = net_end[key] - net_start.get(key, 0)
                if rise > 0:
                    self.fail(f"soak: lwIP's {key} rose by {rise}: allocations failed during the soak")
        if share > max_miss_share:
            self.fail(f"soak: {share:.2%} of the web reads missed, more than {max_miss_share:.2%}")
        for poller in pollers:
            if poller.run_longest > max_miss_run:
                self.fail(f"soak: {poller.name} missed {poller.run_longest} in a row, more than {max_miss_run}")
        if len(heap) < 2 or quiet_start is None or quiet_end is None:
            self.fail("soak: too few stats read")
            return
        # Each open socket holds a FreeRTOS queue or two, so heap_free moves with the connections open when it is
        # read (ADR-003, gate 2). Memory not given back is judged between two quiet moments: before the browsers,
        # and SOAK_QUIET_S after they stop, when the server has closed its idle connections
        self.log.write("rc", f"soak: heap_free quiet before {quiet_start['heap_free']}, quiet after "
                             f"{quiet_end['heap_free']}, lowest of the run {min(free for _, free in heap)}")
        if quiet_end["heap_free"] < quiet_start["heap_free"]:
            self.fail(f"soak: heap_free quiet after the run {quiet_end['heap_free']}, lower than "
                      f"{quiet_start['heap_free']} before it: memory not given back")

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
            # Until charge mode has set its first state the unit still shows 0, the last one's exit: it takes the
            # mode longer to start when it must set a motor driver up again first (ADR-012)
            if not seen and charge == 0:
                continue
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

    def recordings_wait(self, unit, rec_id, timeout_s):
        """The recording read once it is complete; None, failed, when it is not by timeout_s."""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            status, entry = unit.request("GET", f"recordings/{rec_id}")
            if status == 200 and entry.get("complete"):
                return entry
            time.sleep(CHARGE_POLL_S)
        self.fail(f"recordings_check: recording {rec_id} not complete within {timeout_s:.0f} s")
        return None

    def recordings_compare(self, unit, rec_id, line):
        """
        One throw's recording through the API against the console's: its rows against `ccrec <id>`, row for row to
        the console's digits, and its summary against the throw's own line. Returns the rows compared.
        """
        entry = self.recordings_wait(unit, rec_id, RECORDINGS_THROW_S)
        if entry is None:
            return 0
        problems = []
        figures = dict(re.findall(r"(\w+)=(\S+)", line))
        summary = entry["summary"]
        for name in ("moves", "streams"):
            if summary.get(name) != int(figures[name]):
                problems.append(f"summary {name} {summary.get(name)}, the line's {figures[name]}")
        if figures["first"] != "nan" and abs(summary["first"] - float(figures["first"])) > 0.0005 + 1e-6:
            problems.append(f"summary first {summary['first']}, the line's {figures['first']}")
        if not entry["samples_kept"]:
            self.fail(f"recordings_check: recording {rec_id}: its samples are not kept")
            return 0
        samples = unit.read(f"recordings/{rec_id}/samples")
        self.console.data_lines = []
        reply = self.send(f"ccrec {rec_id}")
        console_rows = [[float(x) for x in data.split()[1:]] for data in self.console.data_lines
                        if data.startswith("@r ")]
        api_rows = samples["rows"]
        if reply is None or len(api_rows) != len(console_rows) or len(api_rows) != entry["rows"]:
            problems.append(f"rows: {len(api_rows)} through the API, {len(console_rows)} by ccrec, {entry['rows']} "
                            "in its entry")
        columns = samples["columns"]
        # The console's columns and its digits: the API's six significant digits are within half its last digit
        pairs = (("t_ms", 0, 0.0), ("weight", 1, 0.0005), ("ctl_forecast", 2, 0.00005), ("coarse_rps", 3, 0.0005),
                 ("fine_rps", 4, 0.0005))
        for number, (api_row, console_row) in enumerate(zip(api_rows, console_rows)):
            for name, at, half in pairs:
                value = api_row[columns.index(name)]
                if abs(value - console_row[at]) > half + abs(value) * 5e-6 + 1e-9:
                    problems.append(f"row {number} {name}: {value} through the API, {console_row[at]} by ccrec")
                    break
            if len(problems) > 5:
                break
        self.log.write("rc", f"recordings_check: recording {rec_id}: {len(api_rows)} rows, thinned "
                             f"{entry.get('thinned')}, {entry['summary']['result']}, {len(entry['marks'] or [])} "
                             f"marks, settings {'taken' if entry['settings'] else 'null'}: "
                             f"{'the same' if not problems else 'differ'}")
        for problem in problems:
            self.fail(f"recordings_check: recording {rec_id}: {problem}")
        return len(api_rows)

    def recordings_check(self, host, target, throws):
        """
        Every throw's recording read through the API is the console's (ADR-009, gate 2's rest), with the simulated
        scale: `throws` throws of one charge, the cup lifted and put back between them by the simulated scale, as a
        user's charge goes on, each throw's rows through GET /api/v1/recordings/{id}/samples against `ccrec <id>` row
        for row, its summary against its line. Not a console series: the simulated scale lifts its cup at every
        verdict, which a series, pouring into the same cup, never asks for. Then a charge whose samples cannot be had
        (`ccrec nosamples on`, as when the Pico W's heap has not their bytes): it throws to its verdict all the same,
        its recording says samples_kept false, and its samples' path answers 410. The hopper is taken to be empty.
        """
        unit = unit_api.Unit(host)
        if unit.read("charge")["state"] != "idle":
            self.fail("recordings_check: the unit is not idle at the menu")
            return
        if self.send("sim on") is None or self.failures:
            return
        heap_before = self.console.command("stats")
        heap_during = None
        try:
            # The throws of one charge
            self.console.charge_events.clear()
            reply = self.send(f"charge {target:g}")
            if reply is None or not reply.startswith("@ok"):
                return
            seen, compared, started, last_id = 0, 0, time.monotonic(), None
            while seen < throws and time.monotonic() - started < throws * RECORDINGS_THROW_S:
                lines = [line for _, line in self.console.charge_events if line.startswith("Charge: throw ")]
                for line in lines[seen:]:
                    rec = re.search(r"recording=(\d+)", line)
                    if rec is None or rec.group(1) == "0":
                        self.fail(f"recordings_check: a throw with no recording: {line}")
                    else:
                        compared += self.recordings_compare(unit, int(rec.group(1)), line)
                        last_id = int(rec.group(1))
                    seen += 1
                if any("Charge: ended" in line for _, line in self.console.charge_events):
                    self.fail("recordings_check: the series ended on a fault")
                    break
                time.sleep(CHARGE_POLL_S)
            self.log.write("rc", f"recordings_check: {seen} of {throws} throws, {compared} rows compared")
            if seen < throws:
                self.fail(f"recordings_check: {seen} of {throws} throws seen")
            heap_during = self.console.command("stats")
            self.recordings_stop(unit)

            # A charge whose samples cannot be had
            self.console.charge_events.clear()
            if self.send("ccrec nosamples on") is None:
                return
            reply = self.send(f"charge {target:g}")
            if reply is None or not reply.startswith("@ok"):
                return
            line, started = None, time.monotonic()
            while line is None and time.monotonic() - started < RECORDINGS_THROW_S:
                lines = [text for _, text in self.console.charge_events if text.startswith("Charge: throw ")]
                line = lines[0] if lines else None
                time.sleep(CHARGE_POLL_S)
            if line is None:
                self.fail("recordings_check: with no samples, the charge threw nothing")
            else:
                rec_id = int(re.search(r"recording=(\d+)", line).group(1))
                entry = self.recordings_wait(unit, rec_id, RECORDINGS_THROW_S) if rec_id else None
                status, _ = unit.request("GET", f"recordings/{rec_id}/samples")
                self.log.write("rc", f"recordings_check: with no samples, the charge threw to "
                                     f"{entry['summary']['result'] if entry else '?'}; recording {rec_id} "
                                     f"samples_kept {entry['samples_kept'] if entry else '?'}, its samples {status}")
                if entry is None or entry["samples_kept"] or status != 410:
                    self.fail(f"recordings_check: with no samples: recording {rec_id}, samples_kept "
                              f"{entry['samples_kept'] if entry else None}, its samples answered {status}, not 410")
            self.recordings_stop(unit)
            # The Pico 2 W keeps them across charges: the first charge's last throw has its samples still
            if unit.index["board"] == "pico2_w" and last_id is not None:
                kept = unit.read(f"recordings/{last_id}")["samples_kept"]
                self.log.write("rc", f"recordings_check: after it, recording {last_id}'s samples kept: {kept}")
                if not kept:
                    self.fail(f"recordings_check: recording {last_id}'s samples went with a later charge's")
        finally:
            self.send("ccrec nosamples off")
            self.send("sim off")
        heap_after = self.console.command("stats")
        for name, reply in (("before", heap_before), ("during the series", heap_during), ("after", heap_after)):
            self.log.write("rc", f"recordings_check: the heap {name}: {reply}")

    def recordings_stop(self, unit):
        """The charge stopped, when it runs, and waited for until it is idle."""
        if unit.read("charge")["state"] != "idle":
            self.send("charge stop")
        deadline = time.monotonic() + RECORDINGS_THROW_S
        while time.monotonic() < deadline and unit.read("charge")["state"] != "idle":
            time.sleep(CHARGE_POLL_S)

    def charge_fault(self, fault, host, target, at_weight, quiet_ms, rate, purge_speed, expect, watch_s,
                     repeat=1, want=None, want_none=False):
        """
        A charge with the simulated scale and one fault put in partway (ADR-013), `state` read every
        CHARGE_FAULT_POLL_S from the fault to watch_s after it. Logs each change of the charge's state or of a
        trickler's speed, and judges whether the fault was seen:
          bad: a reading that is not a number. Seen when a trickler goes to its full speed, or the charge ends
              while short of the target by more than the stop threshold;
          quiet: no reading for quiet_ms. Seen when a trickler still turns 3 intervals and a ramp after the
              last reading, inside the quiet time;
          purge: the web's purge speed sent during the charge. Seen when a trickler takes that speed.

        Args:
            fault: "bad", "quiet" or "purge".
            host: the unit's address, for the maxima and for the purge request.
            target: the charge's weight.
            at_weight: the weight at which the fault goes in (charging).
            quiet_ms: how long the scale is quiet, for "quiet".
            rate: the simulated scale's readings a second.
            purge_speed: the purge speed sent, for "purge".
            expect: "seen" (the code before the fix) or "not-seen" (after it); the other fails.
            watch_s: how long `state` is read after the fault.
            repeat: how many times the fault goes in, each after the charge resumed from the one before.
            want: words that must come in the charge's "Charge: ..." lines, in that order ("paused", "resumed").
            want_none: no pause and no end may come.
          none: nothing put in, the simulated scale at `rate`. Seen when the charge pauses or ends.
        """
        def read_json(path):
            # A unit just flashed may not be on Wi-Fi yet: up to SETTINGS_TRIES tries, then a failure, not a crash
            for attempt in range(1, SETTINGS_TRIES + 1):
                try:
                    return json.loads(http_get(host, path))
                except (OSError, ValueError) as error:
                    self.log.write("rc", f"charge_fault: {path}, try {attempt}: {error}")
                    time.sleep(2.0)
            return None

        charge = read_json("api/v1/charge")
        readings = [read_json(path) for path in
                    (f"api/v1/powder_profiles/{charge['powder_profile'] if charge else 0}",
                     "api/v1/settings/motors/coarse", "api/v1/settings/motors/fine", "api/v1/settings/charge")]
        if charge is None or any(reading is None for reading in readings):
            self.fail("charge_fault: the unit's settings could not be read over the web")
            return
        profile, coarse_motor, fine_motor, charge_config = readings
        coarse_max = min(float(profile["coarse_max_speed_rps"]), float(coarse_motor["max_speed_rps"]))
        fine_max = min(float(profile["fine_max_speed_rps"]), float(fine_motor["max_speed_rps"]))
        threshold = float(charge_config["fine_stop_threshold"])
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
            elif fault == "purge":
                # A purge's speed sent during a charge: API v1 refuses it (409 not_running), so no motor sees it
                status, reply = http_call(host, "PATCH", "/api/v1/purge", {"speed_rps": purge_speed})
                self.log.write("rc", f"charge_fault: a purge speed during the charge: {status} "
                                     f"{(reply or {}).get('error', {}).get('code', '')}")
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
        elif fault == "purge":
            took = [at for at, _, _, coarse, fine in samples
                    if abs(coarse - purge_speed) < 0.01 or abs(fine - purge_speed) < 0.01]
            seen = bool(took)
            how = f"a trickler at the purge speed {took[0]:.2f} s after it was sent" if took else ""
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
            bound = min(motors_s + 5.0, 120.0)      # the unit stops them by itself after this
            self.send(f"motor coarse {SCALE_FRAMES_COARSE_RPS:g} {bound:g}")
            self.send(f"motor fine {SCALE_FRAMES_FINE_RPS:g} {bound:g}")
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
        (b) the assert test build, (c) the hard-fault test build. Each passes when the STEP pin stops pulsing:
        the PIO makes only the steps it is fed, and the hard-fault handler stops it (ADR-012). Ends on the normal
        build.

        Args:
            assert_elf: the ELF built with RC_TEST_FAULT=1.
            hardfault_elf: the ELF built with RC_TEST_FAULT=2.
        """
        if self.board["step_status"] is None:
            self.fail("the step pins are not worked out for this board")
            return
        normal_elf = self.elf

        def pulsing():
            return STEP_RUNNING_FRACTION < self.step_activity()["coarse"] < STEP_STUCK_FRACTION

        def start_coarse(case):
            # Just after a flash the menu may not have drawn its first screen, and a motor is refused until it has
            # ("a mode is running"); the test builds fault 10 s after their start, so it is waited for a little. The
            # run is bounded by the unit itself: it stops after its seconds whatever happens here
            line = f"motor coarse 1 {PULSES_RUN_S[case]:g}"
            deadline = time.monotonic() + PULSES_MENU_WAIT_S
            reply = self.console.command(line)
            while reply is not None and "a mode is running" in reply and time.monotonic() < deadline:
                time.sleep(0.2)
                reply = self.console.command(line)
            if reply is None or not reply.startswith("@ok"):
                self.fail(f'{case}: "motor coarse 1": {reply}')
                return False
            time.sleep(1.5)     # the ramp
            if not pulsing():
                self.fail(f"{case}: the coarse trickler did not start pulsing")
                return False
            return True

        # (a) a reset through the probe
        if start_coarse("reset"):
            for write in self.board["reset"]:
                self.openocd.cmd(write)
            time.sleep(0.5)
            stopped = not pulsing()
            self.log.write("rc", f"(a) reset through the probe: pulses {'stopped' if stopped else 'RUNNING'}")
            if not stopped:
                self.fail("(a) the step pulses ran on after a reset through the probe")
            self.wait_for_console()

        # A case whose test image did not flash is skipped: the image running would never fault, and the coarse tube
        # would pour until the next flash (2026-11-23: 286 gr into the cup from two stale test images)
        def flashed_ok(elf):
            before = len(self.failures)
            self.flash(elf)
            if len(self.failures) > before:
                self.console.command("motor coarse 0")
                self.fail(f"pulses: {Path(elf).name} did not flash: its case is not run")
                return False
            return True

        # (b) a failed assert enters the USB loader
        assert_flashed = flashed_ok(assert_elf)
        flashed = time.monotonic()
        self.console.expect_fault = True
        if assert_flashed and start_coarse("assert"):
            self.cup_watch(flashed + 13)
            stopped = not pulsing()
            self.log.write("rc", f"(b) assert: pulses {'stopped' if stopped else 'RUNNING'}")
            if not stopped:
                self.fail("(b) the step pulses ran on after an assert")
        self.console.expect_fault = False

        # (c) a hard fault: its handler stops the motors before the core halts
        hardfault_flashed = flashed_ok(hardfault_elf)
        flashed = time.monotonic()
        if hardfault_flashed and start_coarse("hard fault"):
            self.cup_watch(flashed + 13)
            stopped = not pulsing()
            self.log.write("rc", f"(c) hard fault: pulses {'stopped' if stopped else 'RUNNING'}")
            if not stopped:
                self.fail("(c) the step pulses ran on after a hard fault")

        self.flash(normal_elf)

    def update_state(self, elf, wait_s):
        """
        The unit's slots, from the console's `update`, asked of the image `elf` names until it answers. An image's
        RTT control block lies where its own ELF says, and RAM keeps the last image's across a restart, so the
        console is looked for at the address of the image expected to run. Never a failure by itself: a caller
        looking across a restart judges what it finds.

        Args:
            elf: the ELF of the image expected to run; it becomes the session's.
            wait_s: how long to ask.

        Returns:
            Its fields as a dict ("slot", "trial", "confirmed", "block", "free", ...), or None.
        """
        self.elf = Path(elf)
        deadline = time.monotonic() + wait_s
        while time.monotonic() < deadline:
            self.openocd.rtt_start(self.elf)
            reply = self.console.command("update", timeout=1.0, recover=False)
            if reply is not None and reply.startswith("@ok update"):
                self.last_up = None     # a restart was looked for: up= starts again
                self.link_seen = len(self.openocd.output)
                return dict(field.split("=", 1) for field in reply.split()[2:] if "=" in field)
        self.link_seen = len(self.openocd.output)
        return None

    def slot_trial(self, image_elf, expect, corrupt, reason=None, reset=None):
        """
        A trial of an image in the other slot (ADR-004, gate 1). The image's .bin is written through the probe into
        the slot the unit does not run from, the unit is restarted on what it runs, and a trial is asked
        (`update try`). Judged by what runs after:
          confirmed: the new image runs, confirms itself once on its network, and still runs after a restart;
          back: the new image runs on trial, does not confirm itself, and the unit goes back to the old one when the
                trial's watchdog runs out;
          refused: the new image never runs (with --corrupt: a byte changed, so its hash does not hold).
        The session's ELF (--elf) is the image the unit runs at the start.

        Args:
            image_elf: the update image's ELF, built with APP_TRIAL_IMAGE; its .bin beside it is what is written.
            expect: "confirmed", "back" or "refused".
            corrupt: change one byte in the middle of the image before writing it.
        """
        if self.board.get("layout") is None:
            self.fail("slot_trial: this board has no slots")
            return
        old_elf, new_elf = self.elf, Path(image_elf)
        before = self.update_state(old_elf, 5)
        if before is None or before.get("known") != "1":
            self.fail(f"slot_trial: the unit does not know its slots: {before}")
            return
        old_slot = before["slot"]
        new_slot = "b" if old_slot == "a" else "a"
        free_offset = int(before["free"].split("+")[0], 16)
        image = new_elf.with_suffix(".bin")
        data = bytearray(image.read_bytes())
        new_block = block_version(new_elf)
        if corrupt:
            data[len(data) // 2] ^= 0x01
            image = LOG_DIR / f"{self.log.path.stem}_corrupt.bin"
            image.write_bytes(bytes(data))
        self.log.write("rc", f"slot_trial: running slot {old_slot} (block {before['block']}); writing {image.name} "
                             f"(block {new_block}{', one byte changed' if corrupt else ''}) into slot {new_slot} "
                             f"at {free_offset:#x}")

        # Written, then the unit restarted on what it runs: the new image, on trial, boots only when asked for
        done_before = len(self.openocd.output)
        self.openocd.cmd(f"program {{{image.as_posix()}}} verify {0x10000000 + free_offset:#x}")
        if not any("Verified OK" in line for line in self.openocd.output[done_before:]):
            self.fail("slot_trial: openocd did not verify the image")
            return
        self.reset()
        after_write = self.update_state(old_elf, 5)
        if after_write is None or after_write["slot"] != old_slot:
            self.fail(f"slot_trial: after the write and a restart the unit should run slot {old_slot}: {after_write}")
            return

        self.log.write("rc", "> update try")
        asked = self.console.command("update try", timeout=REPLY_TIMEOUT_S, recover=False)
        if expect == "refused" and asked is not None and asked.startswith("@err refused"):
            # The firmware checks the image before it asks the trial: refused there, nothing restarts. A corrupted
            # image is refused by its hash, not for another reason (review Q5)
            self.log.write("rc", f"slot_trial: refused before the trial: {asked}")
            if corrupt and "hash_mismatch" not in asked:
                self.fail(f"slot_trial: a corrupted image should be refused by its hash: {asked}")
            state = self.update_state(old_elf, 5)
            if state is None or state["slot"] != old_slot or state["trial"] != "0":
                self.fail(f"slot_trial: after the refusal slot {old_slot} should run: {state}")
            return
        if asked is None or not asked.startswith("@ok"):
            self.fail(f"slot_trial: update try: {asked}")
            return
        started = time.monotonic()
        time.sleep(1.5)     # the restart, 100 ms after the reply

        if expect == "refused":
            state = self.update_state(old_elf, CONSOLE_TIMEOUT_S)
            self.log.write("rc", f"slot_trial: after the trial was asked: {state}")
            if state is None or state["slot"] != old_slot or state["trial"] != "0":
                self.fail(f"slot_trial: the image should have been refused, slot {old_slot} still running: {state}")
            return

        tried = self.update_state(new_elf, CONSOLE_TIMEOUT_S)
        self.log.write("rc", f"slot_trial: after the trial was asked: {tried}")
        if tried is None or tried["slot"] != new_slot or tried["trial"] != "1":
            self.fail(f"slot_trial: slot {new_slot} should run on trial: {tried}")
            return
        if expect == "power-trial":
            # The power cut while the image runs on trial (ADR-008, gate 2): back on the old image at once, and the
            # unit says why. The image is one that never confirms itself, so it is still on trial when cut
            time.sleep(10.0)
            self.power_cut()
            state = None
            deadline = time.monotonic() + 30
            while state is None and time.monotonic() < deadline:
                time.sleep(1.0)
                state = self.update_state(old_elf, CONSOLE_TIMEOUT_S)
            self.log.write("rc", f"slot_trial: after the power cut: {state}")
            if state is None or state["slot"] != old_slot or state["trial"] != "0":
                self.fail(f"slot_trial: after a power cut on trial the unit should be back on slot {old_slot}: {state}")
                return
            self.slot_trial_last(state, "2", reason, reset, old_elf)
            return
        if expect == "confirmed":
            deadline = started + 60
            state = tried
            while state is not None and state["confirmed"] != "1" and time.monotonic() < deadline:
                time.sleep(1.0)
                state = self.update_state(new_elf, 5)
            if state is None or state["confirmed"] != "1":
                self.fail(f"slot_trial: slot {new_slot} did not confirm itself within 60 s: {state}")
                return
            self.log.write("rc", f"slot_trial: slot {new_slot} confirmed {time.monotonic() - started:.1f} s after "
                                 "the trial was asked")
            self.reset()
            kept = self.update_state(new_elf, 5)
            if kept is None or kept["slot"] != new_slot or kept["trial"] != "0" or kept["block"] != new_block:
                self.fail(f"slot_trial: after a restart slot {new_slot} (block {new_block}) should run: {kept}")
                return
            self.log.write("rc", f"slot_trial: after a restart the unit runs slot {new_slot}, block {kept['block']}")
            self.slot_trial_last(kept, "1", reason, reset, new_elf)
            return

        # back: the trial's watchdog is fed for its minute, then it restarts the unit into the old image
        deadline = started + 120
        state = tried
        while time.monotonic() < deadline:
            time.sleep(2.0)
            state = self.update_state(new_elf, 3)
            if state is None:
                state = self.update_state(old_elf, CONSOLE_TIMEOUT_S)
            if state is not None and state["slot"] == old_slot:
                break
        if state is None or state["slot"] != old_slot or state["trial"] != "0":
            self.fail(f"slot_trial: the unit should be back on slot {old_slot} within 120 s: {state}")
            return
        self.log.write("rc", f"slot_trial: back on slot {old_slot} {time.monotonic() - started:.1f} s after the "
                             f"trial was asked, block {state['block']}")
        self.slot_trial_last(state, "2", reason, reset, old_elf)

    def slot_trial_last(self, state, result, reason, reset, elf):
        """
        How the trial ended, as the unit keeps it (ADR-008, gate 2): `last` (1 confirmed, 2 back), its reason and the
        settings it reset, read after the end and again after a restart, which must not forget it.
        """
        for moment in ("after the end", "after a restart"):
            if moment == "after a restart":
                self.reset()
                state = self.update_state(elf, CONSOLE_TIMEOUT_S)
            self.log.write("rc", f"slot_trial: last {moment}: {state and (state.get('last'), state.get('reason'), state.get('reset'))}")
            if state is None or state.get("last") != result or \
                    (reason is not None and state.get("reason") != reason) or \
                    (reset is not None and int(state.get("reset", "0"), 16) != reset):
                self.fail(f"slot_trial: last {moment} should be {result}, reason {reason}, reset {reset}: {state}")

    def update(self, host, manifest_path, expect, refused_code, new_elf, browsers=False):
        """
        An update through the API (ADR-004, gate 2), as the offer will make it. The manifest is read, the image the
        unit takes (GET /api/v1/update, `takes`) found in it and checked by its SHA-256, the job started, the image
        sent, and then: with --expect refused, the refusal and its code judged, nothing installed; else installed, and
        the unit followed through its restart until the trial has ended, confirmed or back. The settings must be as
        they were, and the event log must tell the same story.

        Args:
            host: the unit's address.
            manifest_path: the release's manifest.json, its files beside it; or its address (read_manifest).
            expect: "confirmed", "back" (and a start during the trial refused), "refused", "cut", "busy", or "second"
                (a second upload while the first is open refused, and one too large, the first then taken whole).
            refused_code: with "refused", the error code the image must be refused with.
            new_elf: the ELF of the image installed, so the console is found again after the restart; or None.
            browsers: two browsers use the web pages while the image is sent, as the soak's do: their reads judged
                by the soak's limits, and lwIP's heap and pools by `net` before and after.
        """
        manifest, image_of = read_manifest(str(manifest_path))
        # A unit just flashed or restarted is not on the network yet
        unit, deadline = None, time.monotonic() + UPDATE_BACK_ON_NETWORK_S
        while unit is None and time.monotonic() < deadline:
            try:
                unit = unit_api.Unit(host)
            except (OSError, ValueError, http.client.HTTPException, unit_api.ApiError):
                time.sleep(1.0)
        if unit is None:
            self.fail(f"update: {host} does not answer")
            return
        status, before = unit.request("GET", "update")
        if status != 200:
            self.fail(f"update: GET /api/v1/update answered {status}: {before}")
            return
        image_entry = manifest["images"].get(before["takes"])
        if image_entry is None:
            self.fail(f"update: the manifest has no image for {before['takes']}: {sorted(manifest['images'])}")
            return
        image = image_of(image_entry)
        if hashlib.sha256(image).hexdigest() != image_entry["sha256"] or len(image) != image_entry["size"]:
            self.fail(f"update: {image_entry['file']} is not the file the manifest names")
            return
        if "block" in image_entry and image_entry["block"] != block_version_text(image):
            self.fail(f"update: the manifest's block {image_entry['block']} is not the image's, "
                      f"{block_version_text(image)}")
            return
        export_before = unit.read("settings")
        boot_before = unit.index["boot"]
        self.log.write("rc", f"update: {host} runs {before['running']} and takes {before['takes']}: "
                             f"{image_entry['file']}, {len(image)} bytes, release {manifest['version']}")

        if expect == "busy":
            # A charge on the simulated scale: the update may not start beside it
            self.send("sim on")
            status, charge = unit.request("POST", "charge/start", body={"target": 30})
            if status != 202:
                self.fail(f"update: the charge did not start: {status} {charge}")
            time.sleep(1.0)
            status, refused = unit.request("POST", "update/start", body={})
            code = (refused or {}).get("error", {}).get("code")
            self.log.write("rc", f"update: start during a charge: {status} {refused}")
            if status != 409 or code != "busy":
                self.fail(f"update: a start during a charge should be 409 busy: {status} {refused}")
            unit.request("POST", "charge/stop", body={})
            time.sleep(1.0)
            self.send("sim off")
            _, after = unit.request("GET", "update")
            if after["state"] != "idle" or after["running"] != before["running"]:
                self.fail(f"update: after the refused start nothing should have changed: {after}")
            return

        status, started = unit.request("POST", "update/start", body={})
        if status != 202:
            self.fail(f"update: start answered {status}: {started}")
            return
        deadline = time.monotonic() + UPDATE_RECEIVING_S
        state = started
        while state["state"] != "receiving" and time.monotonic() < deadline:
            time.sleep(0.2)
            _, state = unit.request("GET", "update")
        if state["state"] != "receiving":
            self.fail(f"update: the menu did not begin the job: {state}")
            return

        if expect == "second":
            # The first upload open; a second, and one larger than the slot, refused while it is; then the first
            # taken whole: they changed nothing of it (review Q1). The job then stopped, nothing installed
            head = (f"PUT /api/v1/update/image HTTP/1.1\r\nHost: {host}\r\nContent-Type: application/octet-stream\r\n"
                    f"Content-Length: {len(image)}\r\nConnection: close\r\n\r\n").encode()
            half = len(image) // 2
            with socket.create_connection((host, 80), timeout=UPDATE_UPLOAD_TIMEOUT_S) as sock:
                sock.sendall(head + image[:half])
                time.sleep(1.0)
                # The second's head only: it is answered as soon as its headers are in
                with socket.create_connection((host, 80), timeout=HTTP_TIMEOUT_S) as other:
                    other.sendall(head)
                    second = read_until_closed(other).decode(errors="replace")
                self.log.write("rc", f"update: a second upload while the first is open: {second!r}")
                if " 409 " not in second.split("\r\n")[0] or '"busy"' not in second:
                    self.fail(f"update: a second upload should be 409 busy: {second!r}")
                too_large = (f"PUT /api/v1/update/image HTTP/1.1\r\nHost: {host}\r\n"
                             f"Content-Type: application/octet-stream\r\nContent-Length: {before['free_bytes'] + 1}\r\n"
                             f"Connection: close\r\n\r\n").encode()
                with socket.create_connection((host, 80), timeout=HTTP_TIMEOUT_S) as other:
                    other.sendall(too_large)
                    answer = read_until_closed(other).decode(errors="replace").split("\r\n")[0]
                self.log.write("rc", f"update: one larger than the slot while the first is open: {answer}")
                if " 413 " not in answer:
                    self.fail(f"update: an upload larger than the slot should be 413: {answer}")
                sock.sendall(image[half:])
                reply = b""
                while b"\r\n\r\n" not in reply:
                    got = sock.recv(4096)
                    if not got:
                        break
                    reply += got
            first_line = reply.decode(errors="replace").split("\r\n")[0]
            _, ready = unit.request("GET", "update")
            self.log.write("rc", f"update: the first upload: {first_line}; {ready}")
            if " 200 " not in first_line or ready["state"] != "ready" or ready["received"] != len(image):
                self.fail(f"update: the first upload should be taken whole: {first_line} {ready}")
            unit.request("POST", "update/stop", body={})
            time.sleep(2.0)
            _, ended = unit.request("GET", "update")
            if ended["state"] != "idle" or (ended["last"] or {}).get("result") != "stopped" or \
                    ended["running"] != before["running"]:
                self.fail(f"update: after the stop nothing should be installed: {ended}")
            return

        if expect == "power-upload":
            # Half the image in, then the power cut as the rest comes: the unit comes back on the image it ran, the
            # free slot's half image not booted, the job gone with the RAM it lived in, the settings as they were
            head = (f"PUT /api/v1/update/image HTTP/1.1\r\nHost: {host}\r\nContent-Type: application/octet-stream\r\n"
                    f"Content-Length: {len(image)}\r\nConnection: close\r\n\r\n").encode()
            with socket.create_connection((host, 80), timeout=UPDATE_UPLOAD_TIMEOUT_S) as sock:
                sock.sendall(head + image[:len(image) // 2])
                time.sleep(1.0)
                _, during = unit.request("GET", "update")
                self.log.write("rc", f"update: half the image sent: {during}")
                if during["state"] != "receiving" or not during["received"]:
                    self.fail(f"update: the job should be taking the image when the power goes: {during}")
                cut = time.monotonic()
                self.power_cut()
            back_unit, back = self.back_after(host, boot_before, cut)
            self.log.write("rc", f"update: {time.monotonic() - cut:.1f} s after the power cut: {back}")
            if back is None or back["state"] != "idle" or back["running"] != before["running"]:
                self.fail(f"update: after the power cut the unit should be back on its image, idle: {back}")
            elif back_unit.read("settings") != export_before:
                self.fail("update: the settings differ after the power cut")
            return

        if expect == "cut":
            # Half the image, then the connection closed: the job waits for it again, then ends by its time
            head = (f"PUT /api/v1/update/image HTTP/1.1\r\nHost: {host}\r\nContent-Type: application/octet-stream\r\n"
                    f"Content-Length: {len(image)}\r\nConnection: close\r\n\r\n").encode()
            with socket.create_connection((host, 80), timeout=UPDATE_UPLOAD_TIMEOUT_S) as sock:
                sock.sendall(head + image[:len(image) // 2])
                time.sleep(2.0)
            time.sleep(2.0)
            _, waiting = unit.request("GET", "update")
            self.log.write("rc", f"update: half the image sent, the connection closed: {waiting}")
            if waiting["state"] != "receiving":
                self.fail(f"update: after a cut-off upload the job should still be receiving: {waiting}")
            time.sleep(UPDATE_RECEIVING_IDLE_S + 3.0)
            _, ended = unit.request("GET", "update")
            self.log.write("rc", f"update: {UPDATE_RECEIVING_IDLE_S + 3.0:.0f} s later: {ended}")
            if ended["state"] != "idle" or (ended["last"] or {}).get("result") != "time" or \
                    ended["running"] != before["running"]:
                self.fail(f"update: the job should have ended by its time, nothing installed: {ended}")
            return

        pollers, net_start = [], None
        if browsers:
            def read_mirror_right():
                return len(http_get(host, "api/v1/display/frame")) == MIRROR_BYTES

            def read_state():
                return "state" in json.loads(http_get(host, "api/v1/charge"))

            for browser in range(1, SOAK_BROWSERS + 1):
                pollers.append(WebPoller(f"browser {browser} mirror", SOAK_MIRROR_S, [("", read_mirror_right)],
                                         self.log))
                pollers.append(WebPoller(f"browser {browser} charge state", SOAK_STATE_S, [("", read_state)],
                                         self.log))
            net_start = self.net()
            for poller in pollers:
                poller.start()
        sent = time.monotonic()
        status, received = http_put_bytes(host, "/api/v1/update/image", image, UPDATE_UPLOAD_TIMEOUT_S)
        sent_s = time.monotonic() - sent
        if browsers:
            for poller in pollers:
                poller.stop_event.set()
            for poller in pollers:
                poller.join(timeout=HTTP_TIMEOUT_S + 1)
            good = sum(poller.good for poller in pollers)
            missed = sum(poller.missed for poller in pollers)
            share = missed / (good + missed) if good + missed else 1.0
            for poller in pollers:
                self.log.write("rc", f"update: {poller.name}: {poller.good} right, {poller.missed} missed, longest "
                                     f"run {poller.run_longest}; {poller.times()}")
                if poller.run_longest > SOAK_MISS_RUN:
                    self.fail(f"update: {poller.name} missed {poller.run_longest} in a row during the upload")
            self.log.write("rc", f"update: {missed} of {good + missed} web reads missed during the upload "
                                 f"({share:.2%})")
            if share > SOAK_MISS_SHARE:
                self.fail(f"update: {share:.2%} of the web reads missed during the upload")
            net_end = self.net()
            if net_start is not None and net_end is not None:
                self.log.write("rc", "update: net after the upload: " +
                               " ".join(f"{k}={v}" for k, v in net_end.items()))
                for key in (key for key in net_end if key.endswith("_err")):
                    if net_end[key] > net_start.get(key, 0):
                        self.fail(f"update: lwIP's {key} rose by {net_end[key] - net_start.get(key, 0)} during "
                                  "the upload")
        self.log.write("rc", f"update: the image sent in {sent_s:.1f} s ({len(image) / 1024 / sent_s:.0f} KB/s): "
                             f"{status} {received}")
        if expect == "refused":
            code = (received or {}).get("error", {}).get("code")
            _, after = unit.request("GET", "update")
            if status != 400 or code != refused_code:
                self.fail(f"update: the image should be refused with {refused_code}: {status} {received}")
            if after["state"] != "idle" or (after["last"] or {}).get("result") != "refused" or \
                    after["running"] != before["running"]:
                self.fail(f"update: after the refusal the job should have ended, nothing installed: {after}")
            if unit.read("settings") != export_before:
                self.fail("update: the settings differ after the refusal")
            return
        if status != 200 or received.get("state") != "ready":
            self.fail(f"update: the image was not taken: {status} {received}")
            return

        status, installing = unit.request("POST", "update/install", body={})
        if status != 202:
            self.fail(f"update: install answered {status}: {installing}")
            return
        installed = time.monotonic()
        self.last_up = None     # a restart asked for: up= starts again

        # The unit restarts into the trial and comes back on the network
        index = None
        while time.monotonic() < installed + UPDATE_BACK_ON_NETWORK_S:
            time.sleep(1.0)
            try:
                index = unit_api.Unit(host).index
            except (OSError, ValueError, http.client.HTTPException, unit_api.ApiError):
                continue
            if index["boot"] != boot_before:
                break
        if index is None or index["boot"] == boot_before:
            self.fail(f"update: the unit did not come back on the network within {UPDATE_BACK_ON_NETWORK_S:.0f} s")
            return
        self.log.write("rc", f"update: back on the network {time.monotonic() - installed:.1f} s after the install")
        if expect == "power-trial":
            # The image sent running on trial, then the power cut: the unit comes back on the image it ran at once,
            # not after the trial's minute. An image that confirms itself would be past its trial before this
            # looks, so the image sent is one that never does (RC_TEST_NO_CONFIRM)
            on_trial = None
            try:
                _, on_trial = unit_api.Unit(host).request("GET", "update")
            except (OSError, ValueError, http.client.HTTPException, unit_api.ApiError) as error:
                self.log.write("rc", f"update: the unit on trial did not answer: {error}")
            self.log.write("rc", f"update: on trial: {on_trial}")
            if on_trial is None or not on_trial["running"]["trial"] or \
                    on_trial["running"]["block"] != block_version_text(image):
                self.fail(f"update: the image sent should be running on trial: {on_trial}")
                return
            boot_trial = unit_api.Unit(host).index["boot"]
            cut = time.monotonic()
            self.power_cut()
            back_unit, back = self.back_after(host, boot_trial, cut)
            self.log.write("rc", f"update: {time.monotonic() - cut:.1f} s after the power cut: {back}")
            if back is None or back["running"] != before["running"]:
                self.fail(f"update: after the power cut the unit should be back on the image it ran: {back}")
            elif back_unit.read("settings") != export_before:
                self.fail("update: the settings differ after the power cut")
            return
        if expect == "back":
            # The image on trial is not confirmed: the free slot is the way back, and no update may write it (review D2)
            try:
                trial_unit = unit_api.Unit(host)
                _, on_trial = trial_unit.request("GET", "update")
                if on_trial["running"]["trial"]:
                    status, refused = trial_unit.request("POST", "update/start", body={})
                    self.log.write("rc", f"update: a start during the trial: {status} {refused}")
                    if status != 409 or (refused or {}).get("error", {}).get("code") != "busy":
                        self.fail(f"update: a start during the trial should be 409 busy: {status} {refused}")
                else:
                    self.log.write("rc", f"update: back on the old image already: {on_trial['running']}")
            except (OSError, ValueError, http.client.HTTPException, unit_api.ApiError) as error:
                self.log.write("rc", f"update: the unit on trial did not answer: {error}")

        # The trial ends: confirmed once on its network, or left after its minute
        settled = None
        while time.monotonic() < installed + UPDATE_SETTLED_S:
            try:
                status, settled = unit_api.Unit(host).request("GET", "update")
            except (OSError, ValueError, http.client.HTTPException, unit_api.ApiError):
                time.sleep(1.0)
                continue
            if status == 200 and not settled["running"]["trial"] and settled["last"] is not None:
                break
            time.sleep(1.0)
        self.log.write("rc", f"update: {time.monotonic() - installed:.1f} s after the install: {settled}")
        if settled is None or settled["last"] is None or settled["last"]["result"] != expect:
            self.fail(f"update: the trial should have ended {expect}: {settled}")
            return
        # Judged by the image sent, not by what the firmware says of itself (review Q5)
        sent_block = block_version_text(image)
        if settled["last"]["version"] != sent_block:
            self.fail(f"update: `last` should name the image sent, {sent_block}: {settled['last']}")
        wanted_block = sent_block if expect == "confirmed" else before["running"]["block"]
        if settled["running"]["block"] != wanted_block:
            self.fail(f"update: the unit should run {wanted_block}: {settled['running']}")
        events = unit_api.Unit(host).events()["events"]
        kinds = [event["kind"] for event in events if event["source"] == "update"]
        if expect not in kinds:
            self.fail(f"update: the event log has no update {expect}: {kinds}")
        if unit_api.Unit(host).read("settings")["resources"] != export_before["resources"]:
            self.fail("update: the settings differ after the update")
        if new_elf is not None and expect == "confirmed":
            self.update_state(new_elf, CONSOLE_TIMEOUT_S)
        else:
            self.update_state(self.elf, CONSOLE_TIMEOUT_S)

    def cup_reading(self):
        """
        The real scale's reading, the simulated one switched off for it (never while a charge runs).

        Returns:
            The weight, or None when the scale gives no number (nan, inf, no reply).
        """
        state = self.console.command("state") or ""
        # a charge a run left going (charge_cycle ends at the next one's start) is stopped first: the run is over
        for _ in range(20):
            if " charge=0" in state or " charge=" not in state:
                break
            self.console.command("charge stop")
            time.sleep(0.5)
            state = self.console.command("state") or ""
        if " sim=1" in state and " charge=0" in state:
            self.console.command("sim off")
            time.sleep(1.0)
            state = self.console.command("state") or ""
        # Just after the simulation's end the real scale may not have sent yet (nan): a few seconds for it
        until = time.monotonic() + 3.0
        while True:
            match = re.search(r" weight=(\S+)", state)
            if match is None or " sim=1" in state:
                return None
            try:
                weight = float(match.group(1))
            except ValueError:
                weight = float("nan")
            if math.isfinite(weight) or time.monotonic() >= until:
                return weight if math.isfinite(weight) else None
            time.sleep(0.3)
            state = self.console.command("state") or ""

    def cup_watch(self, until):
        """
        Wait until the monotonic time `until`, the real scale read twice a second: at 600 gr both motors stop and the
        run fails. A console gone silent (a test build's fault) is waited out.
        """
        while time.monotonic() < until:
            state = self.console.command("state", timeout=1.0, recover=False) or ""
            match = re.search(r" weight=(\S+)", state)
            if match is not None and " sim=0" in state:
                try:
                    weight = float(match.group(1))
                except ValueError:
                    weight = float("nan")
                if math.isfinite(weight) and weight + getattr(self, "cup_counted_offset", 0.0) >= CUP_MOST_GR:
                    self.console.command("motor coarse 0")
                    self.console.command("motor fine 0")
                    self.fail(f"cup: {weight:.2f} gr reached while a motor turned: both stopped")
                    return
            time.sleep(max(0.0, min(0.5, until - time.monotonic())))

    def cup_before(self, command, most, unchecked, emptied=None):
        """
        The cup read before a run that can turn a motor.

        Args:
            command: the run, as the ledger names it.
            most: the most the run pours, in grains.
            unchecked: the reason given with --no-cup-check, or None.
            emptied: the owner's words that the cup was emptied (--cup-emptied), or None.

        Returns:
            (may the run go, the reading before or None).
        """
        weight = self.cup_reading()
        if weight is None:
            if unchecked:
                self.cup_ledger(command, None, None, most, f"no reading; unchecked: {unchecked}")
                return True, None
            self.fail("cup: the real scale gives no reading; a run that turns a motor is refused (--no-cup-check "
                      "<reason> to run it all the same)")
            return False, None
        last = self.cup_last()
        offset = self.cup_offset()
        if emptied:
            # the owner's word: the count starts again from the reading
            self.cup_ledger("(between runs)", last, weight, 0.0, f"emptied: the owner's word: {emptied}; offset=0")
            offset = 0.0
        elif last is not None and weight < last - CUP_EMPTIED_GR:
            offset += last - weight
            self.cup_ledger("(between runs)", last, weight, 0.0,
                            f"a drop of {last - weight:.2f} with no owner's word: carried, not taken as emptied; "
                            f"offset={offset:.2f}")
            self.log.write("rc", f"cup: WARNING the reading dropped {last - weight:.2f} gr between runs, with no owner's "
                                 f"word that the cup was emptied: counted as {weight + offset:.2f} gr (--cup-emptied "
                                 "<the owner's words> once the owner has emptied it)")
        counted = weight + offset
        self.cup_counted_offset = offset
        self.log.write("rc", f"cup: {weight:.2f} gr before {command} (counted {counted:.2f}), its most pour {most:.0f} gr")
        if counted + most > CUP_MOST_GR:
            self.fail(f"cup: counted {counted:.2f} + {most:.0f} > {CUP_MOST_GR:.0f}, empty the cup")
            self.cup_ledger(command, weight, None, most, "refused")
            return False, weight
        return True, weight

    def cup_after(self, command, before, most):
        """The cup read after the run, into the ledger."""
        weight = self.cup_reading()
        self.log.write("rc", f"cup: {weight} gr after {command}")
        note = ""
        if weight is not None and before is not None:
            poured = weight - before
            note = f"poured {poured:.1f} of most {most:.0f}"
            if poured > most:
                note += ": MORE THAN STATED"
                self.fail(f"cup: the run poured {poured:.1f} gr, more than its stated most {most:.0f}")
        self.cup_ledger(command, before, weight, most, note)

    @staticmethod
    def cup_offset():
        """The drops carried since the owner's last word, from the ledger's latest `offset=`; 0 with none."""
        if not CUP_LEDGER.exists():
            return 0.0
        for line in reversed(CUP_LEDGER.read_text(encoding="utf-8").splitlines()[1:]):
            match = re.search(r"offset=([-\d.]+)", line)
            if match:
                return float(match.group(1))
        return 0.0

    @staticmethod
    def cup_last():
        """The ledger's last reading, or None."""
        if not CUP_LEDGER.exists():
            return None
        for line in reversed(CUP_LEDGER.read_text(encoding="utf-8").splitlines()[1:]):
            fields = line.split(",")
            for value in (fields[3], fields[2]):
                try:
                    return float(value)
                except ValueError:
                    continue
        return None

    @staticmethod
    def cup_ledger(command, before, after, most, note):
        """One line of the ledger: the clock's time, the run, the reading before and after, its most, a note."""
        CUP_LEDGER.parent.mkdir(parents=True, exist_ok=True)
        new = not CUP_LEDGER.exists()
        with CUP_LEDGER.open("a", encoding="utf-8") as ledger:
            if new:
                ledger.write("time,command,before_gr,after_gr,most_gr,note\n")
            text = lambda value: "" if value is None else f"{value:.2f}"
            ledger.write(f"{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')},{command.replace(',', ';')},"
                         f"{text(before)},"
                         f"{text(after)},{most:.0f},{note.replace(',', ';')}\n")

    def finish(self):
        """
        Check the cores, stop openocd, print the verdict.

        Returns:
            The exit code: 0 for PASS, 1 for FAIL.
        """
        for core in self.openocd.halted_cores(self.elf):
            self.fail(f"core not running: {core}")
        # Nothing this session turned is left turning, however it ended (refused while a mode runs: the mode's own)
        for tube in ("coarse", "fine"):
            self.console.command(f"motor {tube} 0", timeout=1.0, recover=False)
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
    parser.add_argument("--most-pour", type=float, help="the most this run may pour, in grains (default: the run's own)")
    parser.add_argument("--cup-emptied", metavar="WORDS",
                        help="the owner's words that the cup was emptied, quoted: the cup's count starts again from the "
                             "reading. Without them a drop between runs is carried, never taken as emptied")
    parser.add_argument("--no-cup-check", metavar="REASON",
                        help="run with no reading of the real scale, for this reason (written into the ledger)")
    sub = parser.add_subparsers(dest="command", required=True)
    send_parser = sub.add_parser("send", help="send command lines, one after another; `pause <seconds>` waits")
    send_parser.add_argument("lines", nargs="+")
    sub.add_parser("flash", help="flash the build through the probe and check it runs")
    sub.add_parser("reset", help="reset the chip through the probe")
    watch_parser = sub.add_parser("watch", help="log the console for a time")
    watch_parser.add_argument("seconds", type=float)
    sub.add_parser("snap", help="save the screen as a PNG under logs/rc/snaps/")
    sub.add_parser("reach", help="every element of every screen reached by the knob, turning only")
    tour_parser = sub.add_parser("menu_tour", help="snap every page of the menu, pressing nothing")
    tour_parser.add_argument("--save", help="keep the pictures in this folder")
    tour_parser.add_argument("--compare", help="fail on a form missing from, or differing from, this folder")
    cycle_parser = sub.add_parser("charge_cycle", help="a charge with the simulated scale, cup off and back")
    cycle_parser.add_argument("--target", type=float, default=30.0)
    cycle_parser.add_argument("--threshold", type=float, default=0.03, help="the unit's fine stop threshold")
    cycle_parser.add_argument("--timeout", type=float, default=120.0)
    uf2_parser = sub.add_parser("flash_uf2", help="a release's full install written through the probe, as by USB")
    uf2_parser.add_argument("uf2", help="the full install's .uf2")
    recordings_parser = sub.add_parser("recordings_check", help="each throw's recording through the API against the "
                                       "console's, and a charge with no samples (ADR-009, gate 2's rest)")
    recordings_parser.add_argument("--host", required=True, help="the unit's address")
    recordings_parser.add_argument("--target", type=float, default=30.0)
    recordings_parser.add_argument("--throws", type=int, default=5)
    pulses_parser = sub.add_parser("pulses", help="do the step pulses stop on a reset, an assert, a hard fault?")
    pulses_parser.add_argument("--assert-elf", default=str(REPO / "build_w_fault1" / "app.elf"))
    pulses_parser.add_argument("--hardfault-elf", default=str(REPO / "build_w_fault2" / "app.elf"))
    update_parser = sub.add_parser("update", help="an update through the API, from a release's manifest (ADR-004)")
    update_parser.add_argument("--host", required=True, help="the unit's address")
    update_parser.add_argument("--manifest", required=True,
                               help="the release's manifest.json, its files beside it, or its address")
    update_parser.add_argument("--expect", choices=["confirmed", "back", "refused", "cut", "busy", "second",
                                                    "power-upload", "power-trial"],
                               default="confirmed",
                               help="cut: half the image, the connection closed; busy: a start during a charge; "
                                    "second: a second upload while the first is open; power-upload, power-trial: "
                                    "the power cut by the probe half-way through the upload, or while the image "
                                    "sent runs on trial (one that never confirms itself)")
    update_parser.add_argument("--refused-code", help="with --expect refused: the error code it must give")
    update_parser.add_argument("--new-elf", help="the installed image's ELF, to find its console after the restart")
    update_parser.add_argument("--browsers", action="store_true",
                               help="two browsers use the web pages while the image is sent, as the soak's do")
    trial_parser = sub.add_parser("slot_trial", help="an image written into the other slot and tried (ADR-004)")
    trial_parser.add_argument("--image", required=True,
                              help="the update image's ELF (built with APP_TRIAL_IMAGE); its .bin is written")
    trial_parser.add_argument("--expect", choices=["confirmed", "back", "refused", "power-trial"], required=True)
    trial_parser.add_argument("--reason", help="the reason `last` must give (ADR-008, gate 2): did_not_start, "
                                               "no_network, not_confirmed, power_cut")
    trial_parser.add_argument("--reset", type=lambda text: int(text, 0),
                              help="the settings `last` must name as reset, their bits (0x20 the LEDs)")
    trial_parser.add_argument("--corrupt", action="store_true", help="change one byte of the image first")
    settings_parser = sub.add_parser("settings", help="copy the stored settings over the web API, and compare")
    settings_parser.add_argument("--host", required=True, help="the unit's address")
    settings_parser.add_argument("--save", help="keep the copy in this folder")
    settings_parser.add_argument("--compare", help="fail on a field that differs from the copy in this folder")
    settings_parser.add_argument("--restore", help="write the copy in this folder back and store it, then compare: "
                                                   "after a factory reset (the Wi-Fi's key is never in a copy)")
    robust_parser = sub.add_parser("web_robust", help="wrong and awkward requests, each answered or closed (ADR-003)")
    robust_parser.add_argument("--host", required=True, help="the unit's address")
    stream_parser = sub.add_parser("stream_check", help="the event stream's life cycle, stage by stage (ADR-008, "
                                                        "gate 2); restarts the unit")
    stream_parser.add_argument("--host", required=True, help="the unit's address")
    api_parser = sub.add_parser("api_check", help="every path the index lists read twice and checked against the "
                                                  "schema; every action called (ADR-003, gate 4); ends with a restart")
    api_parser.add_argument("--host", required=True, help="the unit's address")
    api_parser.add_argument("--target", type=float, default=30.0, help="the charge's target, set when none is")
    roundtrip_parser = sub.add_parser("settings_roundtrip",
                                      help="every setting written back unchanged, each value outside its limits "
                                           "refused, a write during a throw refused (ADR-003, gate 3)")
    roundtrip_parser.add_argument("--host", required=True, help="the unit's address")
    roundtrip_parser.add_argument("--target", type=float, default=30.0, help="the charge's weight for the throw")
    soak_parser = sub.add_parser("soak", help="charges back to back while two browsers use the web pages")
    soak_parser.add_argument("seconds", type=float, help="how long; chosen for the change being proven")
    soak_parser.add_argument("--host", required=True, help="the unit's address")
    soak_parser.add_argument("--target", type=float, default=30.0)
    soak_parser.add_argument("--threshold", type=float, default=0.03, help="the unit's fine stop threshold")
    soak_parser.add_argument("--max-miss-share", type=float, default=SOAK_MISS_SHARE,
                             help="the most of the web reads that may miss, 0 to 1")
    soak_parser.add_argument("--load", action="store_true",
                             help="also a tuning session's load: a throw's samples pulled back to back, and a "
                                  "client asking four times a second (ADR-003, gate 2)")
    soak_parser.add_argument("--stream", action="store_true",
                             help="each browser follows the event stream in place of polling the charge (ADR-008)")
    soak_parser.add_argument("--upload", action="store_true",
                             help="also ADR-004's image: 700 KB sent back to back as a body in pieces, each checked "
                                  "by its CRC-32 (ADR-003, gate 4)")
    soak_parser.add_argument("--max-miss-run", type=int, default=SOAK_MISS_RUN,
                             help="the most misses in a row one poller may have")
    frames_parser = sub.add_parser("scale_frames", help="record the scale's frames through the tap, by phase")
    frames_parser.add_argument("--idle", type=float, default=120.0, help="seconds idle")
    frames_parser.add_argument("--handovers", type=int, default=5, help="hand-overs from the simulated scale")
    frames_parser.add_argument("--reboots", type=int, default=2, help="restarts")
    frames_parser.add_argument("--motors", type=float, default=60.0, help="seconds with both motors turning")
    fault_parser = sub.add_parser("charge_fault", help="a simulated charge with one fault put in partway")
    fault_parser.add_argument("fault", choices=["bad", "quiet", "purge", "none"])
    fault_parser.add_argument("--host", required=True, help="the unit's address")
    fault_parser.add_argument("--expect", choices=["seen", "not-seen"], required=True,
                              help="seen: the code before the fix; not-seen: after it")
    fault_parser.add_argument("--target", type=float, default=30.0)
    fault_parser.add_argument("--at", type=float, help="the weight the fault goes in at (default: 1 short of the target)")
    fault_parser.add_argument("--quiet-ms", type=int, default=1500)
    fault_parser.add_argument("--rate", type=float, default=10.0, help="the simulated scale's readings a second")
    fault_parser.add_argument("--purge-speed", type=float, default=3.0)
    fault_parser.add_argument("--watch", type=float, default=3.0, help="seconds watched after the fault")
    fault_parser.add_argument("--repeat", type=int, default=1, help="how many times the fault goes in")
    fault_parser.add_argument("--want", help="words that must come in the charge's events, in order, comma-separated")
    fault_parser.add_argument("--want-none", action="store_true", help="no pause and no end may come")
    args = parser.parse_args()

    session = Session(args, args.command)
    try:
        most = args.most_pour if args.most_pour is not None else cup_most(args)
        if most is None:
            run_command(session, args)
        else:
            go, before = session.cup_before(cup_name(args), most, args.no_cup_check, args.cup_emptied)
            if go:
                try:
                    run_command(session, args)
                finally:
                    session.cup_after(cup_name(args), before, most)
    except Exception:
        # A run that raises fails with its traceback in the log, and still stops its openocd, which would otherwise
        # hold the probe and the Tcl port for the next session (ADR-004)
        session.log.write("rc", traceback.format_exc().rstrip())
        session.fail(f"rc_run raised: {traceback.format_exc().strip().splitlines()[-1]}")
    sys.exit(session.finish())


# A send's lines that turn a motor
SEND_POURS = re.compile(r"^(charge \d|charge [.\d]|motor (coarse|fine) [^0]|cleanup|purge)")


def cup_most(args):
    """
    The most a run pours by its own figures, in grains; None for a run that turns no motor.

    Each is from the run's own arguments, with room for its rest and its end: a charge's target and the start of the
    next, the pulses' 12 s at 1 rev/s on both tubes, a purge's turns.
    """
    command = args.command
    if command == "charge_cycle":
        return 2.0 * args.target + 10.0
    if command == "recordings_check":
        return (2 * args.throws + 1) * args.target
    if command == "pulses":
        # The coarse tube at 1 rev/s in each case, bounded by the unit (PULSES_RUN_S): 31 turns at most
        return sum(PULSES_RUN_S.values()) * 1.0 * CUP_GR_A_TURN_MOST
    if command == "api_check":
        return args.target + 40.0       # a charge started and stopped, and a purge
    if command == "settings_roundtrip":
        return args.target + 10.0       # a throw, for the write refused during it
    if command == "soak":
        return math.ceil(args.seconds / 12.0) * args.target
    if command == "charge_fault":
        return args.repeat * args.target + 40.0
    if command == "reach":
        return 40.0                     # a simulated charge's screens while it zeroes and between throws
    if command == "scale_frames":
        return (SCALE_FRAMES_COARSE_RPS + SCALE_FRAMES_FINE_RPS) * min(args.motors + 5.0, 120.0) * CUP_GR_A_TURN_MOST
    if command == "send":
        lines = [line for line in args.lines if SEND_POURS.match(line)]
        if not lines:
            return None
        most = 0.0
        for line in lines:
            words = line.split()
            if words[0] == "charge":
                # `charge <gr> series <n>`: n throws into the one cup
                throws = int(words[3]) if len(words) > 3 and words[2] == "series" else 1
                most += throws * (float(words[1]) + 10.0)
            elif words[0] == "motor" and len(words) >= 3:
                seconds = float(words[3]) if len(words) > 3 else MOTOR_RUN_DEFAULT_S
                most += abs(float(words[2])) * seconds * CUP_GR_A_TURN_MOST
            else:
                most += 100.0           # a purge or a cleanup: its own time
        return most
    return None


def cup_name(args):
    """The run as the ledger names it."""
    if args.command == "send":
        return "send " + "; ".join(args.lines)
    return args.command


def run_command(session, args):
    """
    Run the command the arguments name.

    Args:
        session: the Session.
        args: the parsed arguments.
    """
    if args.command == "send":
        for line in args.lines:
            # `pause <seconds>` waits on the PC between lines, for a command whose work outlasts its reply
            if line.startswith("pause "):
                session.log.write("rc", f"pause {float(line.split()[1]):.1f} s")
                time.sleep(float(line.split()[1]))
            else:
                session.send(line)
    elif args.command == "snap":
        session.save_snap(LOG_DIR / "snaps" / f"{session.log.path.stem}.png")
    elif args.command == "reach":
        session.reach()
    elif args.command == "menu_tour":
        session.menu_tour(args.save, args.compare)
    elif args.command == "charge_cycle":
        session.charge_cycle(args.target, args.threshold, args.timeout)
    elif args.command == "flash_uf2":
        session.flash_uf2(args.uf2)
    elif args.command == "recordings_check":
        session.recordings_check(args.host, args.target, args.throws)
    elif args.command == "pulses":
        session.pulses(args.assert_elf, args.hardfault_elf)
    elif args.command == "update":
        session.update(args.host, args.manifest, args.expect, args.refused_code, args.new_elf, args.browsers)
    elif args.command == "slot_trial":
        session.slot_trial(args.image, args.expect, args.corrupt, args.reason, args.reset)
    elif args.command == "flash":
        session.flash()
    elif args.command == "reset":
        session.reset()
    elif args.command == "watch":
        time.sleep(args.seconds)
    elif args.command == "settings":
        if args.restore:
            session.settings_restore(args.host, args.restore)
        session.settings(args.host, args.save, args.compare or args.restore)
    elif args.command == "web_robust":
        session.web_robust(args.host)
    elif args.command == "stream_check":
        session.stream_check(args.host)
    elif args.command == "api_check":
        session.api_check(args.host, args.target)
    elif args.command == "settings_roundtrip":
        session.settings_roundtrip(args.host, args.target)
    elif args.command == "soak":
        session.soak(args.host, args.seconds, args.target, args.threshold, args.max_miss_share, args.max_miss_run,
                     args.load, args.upload, args.stream)
    elif args.command == "scale_frames":
        session.scale_frames(args.idle, args.handovers, args.reboots, args.motors)
    elif args.command == "charge_fault":
        at = args.at if args.at is not None else args.target - 1.0
        session.charge_fault(args.fault, args.host, args.target, at, args.quiet_ms, args.rate, args.purge_speed,
                             args.expect, args.watch, args.repeat,
                             args.want.split(",") if args.want else None, args.want_none)


if __name__ == "__main__":
    main()
