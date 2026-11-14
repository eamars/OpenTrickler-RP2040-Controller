# ADR-011: progress

What was built and measured. The plan is in the [README](README.md).

Images, size reports, build logs and settings copies of every step are kept on this PC under `logs/adr011/<step>/`, which git does not track.

## Before the first step

Measured 2026-11-14, on this PC, at ac1d07e: main's firmware with this ADR's documents.

**Sizes** (Arm 14.2.Rel1, Debug, `tools/size_report.py`)

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 701.9 KB | 676.4 KB |
| RAM at link time | 200.6 KB | 201.1 KB |

- Both boards build from clean with 4 warnings, all inside u8g2.

**The unit**
- It runs 1.20.46 at f484e19, the code of main.
- Its settings, copied with `rc_run.py settings`: 11 paths, kept in `logs/adr011/step0/settings/`.

**The soak, first as planned: an hour of the mirror alone.** Stopped at 31 minutes, when the owner asked for charges and web traffic together in its place (2026-11-14).
- `heap_free` was 96,248 at all 31 readings, `heap_min` 95,080. No restart.
- 2 of about 1,800 reads of the mirror timed out (5 s), at 20:07:27 and 20:17:12; the read after each was right.

**The soak as the owner asked: charges and two browsers.** A trial of 2 minutes on this firmware, 4 charges, all passed, `heap_free` steady:
- 33 of 407 web reads missed (8%); one poller missed 3 in a row.
- The misses fall in every state of the charge, before the first charge too. They follow the load of the web, not the motors.
- So the web reads are judged against the same run before this ADR, not a fixed rule.

**The baseline soak:** 30 minutes on this firmware, 20:29 to 20:59, with no limit on the web reads so that it measures them. `VERDICT PASS`.
- 62 charges, every one passed, each ending at 30.000 for 30.
- `heap_free` 96,248 at all 63 readings. No restart, no assert.
- 433 of 6,655 web reads missed: 6.51%.

| Poller | Right | Missed | Longest run |
|---|---|---|---|
| Browser 1, mirror | 1,152 | 75 | 3 |
| Browser 1, charge state | 1,955 | 120 | 3 |
| Browser 2, mirror | 1,182 | 72 | 2 |
| Browser 2, charge state | 1,867 | 140 | 3 |
| Portal page | 51 | 10 | 2 |
| Settings, all 11 paths in one read | 15 | 16 | 4 |

- By kind: 333 timed out (5 s); 100 were charge states that came back as broken JSON.

**The limits every step of this ADR is held to,** from that run: at most 10% of the web reads missed and at most 6 in a row on one poller (`--max-miss-share 0.10 --max-miss-run 6`). 10% is about one and a half times the baseline's share; the 2-minute trial gave 8%, so a run's share moves by a point or two. 6 in a row is twice the longest run of the pollers that make one request a read (3); the settings poller's 4 above was a run of whole sweeps, see step 1. A step past either limit made the web worse and is looked into. Charges, restarts and the heap are judged as before, with no allowance.

**Found on the way**
- **Every JSON handler of the web API answers from one static buffer.** It writes the reply into the buffer and gives lwIP a pointer to it; lwIP sends it later. A second request to the same path, from another browser, rewrites the buffer while the first answer is still being sent, and the first browser gets a mix: the 100 broken charge states above. 13 handlers in 9 files do it (`charge_mode.cpp`, `cleanup_mode.cpp`, `motors.c`, `scale.c`, `servo_gate.c`, `neopixel_led.c`, `system_control.c`, `wireless.c`, `mini_12864_module.cpp`). Two phones watching one charge see a wrong state now and then. For ADR-003.
- `/display_buffer` answers with the bare 1,024 bytes of the screen and no HTTP header, though its handler sets `FS_FILE_FLAGS_HEADER_INCLUDED`. A browser takes it; Python's HTTP client does not, so `soak` reads it over a plain socket. For ADR-003.
- Deleting `build\` for a clean build deletes `build/.placeholder`, a tracked file.

## Step 1: the SDK with its tools

Built 2026-11-14.

**What moved**
- The Pico SDK from 2.1.1 plus one commit to 2.3.1. With it: lwIP 2.2.1, cyw43-driver 1.1.1, TinyUSB 0.18.0, Mbed TLS 3.6.6.
- picotool and `pioasm` 2.3.1 (tools release `v2.3.1-0`), CMake 4.3.4, Ninja 1.13.2. The build's cache shows each one in use.
- The compiler did not move: 14.2.Rel1.

**The build**
- Both boards build from clean with no change to our code. `http_rest.c`, the copy of lwIP's server, builds against lwIP 2.2.1 as it is.
- The same 4 warnings inside u8g2. CMake 4 adds one note: u8g2's `cmake_minimum_required(VERSION 3.5)` will stop working in a later CMake. u8g2 is ADR-010's.

**Sizes**

| | Pico W | Pico 2 W |
|---|---|---|
| Flash | 701.9 to 647.9 KB (−54.0) | 676.4 to 626.4 KB (−50.0) |
| RAM at link time | 200.6 to 196.9 KB (−3.7) | 201.1 to 197.0 KB (−4.1) |

- All of the fall is the C and C++ runtime: 71.8 to 16.9 KB on Pico W, 69.0 to 19.2 KB on Pico 2 W. The other parts moved by less than 1 KB.
- **Why.** SDK 2.3.0 supplies the guards of C++ static initialisers (`__cxa_guard_acquire` and the rest) itself (SDK commit 6685d000). They came from libstdc++'s `guard.o` before, whose error path calls `std::terminate`, and that linked the verbose terminate handler: the name demangler (`cp-demangle.o`, 38 KB on Pico W), the exception and unwind code, and newlib's FILE writing. None of it is in the image now; the map names none of those objects.
- The firmware is built with `-fno-exceptions`, so nothing it does reached that code. A failed guard now ends in the SDK's `panic`.

**CI** ([run 43568615732](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/runs/43568615732), started by hand on the branch; still `ubuntu-24.04` with Ubuntu's compiler, 13.2.1): passed.

| | Pico W | Pico 2 W |
|---|---|---|
| Flash, CI | 648.9 KB | 627.1 KB |
| Flash, this PC | 647.9 KB | 626.4 KB |
| C and C++ runtime, CI | 16.5 KB | 18.7 KB |

- CI's image and this PC's are now within 1 KB. The 40 KB between them that ADR-001 measured was all in the C and C++ runtime, and it went with the code above. That CI's old image lacked exactly that code is likely, not checked: CI's old map was not read.

**On the unit** (the bench's Pico W, 2026-11-14, from 21:00)
- `settings` before the flash: 11 paths copied (`logs/adr011/step1/settings_before/`).
- `flash`: written and verified; the unit runs 1.20.59 at d9e5570 10.3 s after the start.
- `settings --compare`: every stored value as before; only the version and its hash differ, as they should. The first try, 6 s after the start, had one read time out; `settings` now tries each path up to three times, since how the web answers is soak's to judge. The second try passed.
- `menu_tour --compare tests/menu_baseline/pico_w`: all 22 forms equal, but for form 36, the version, which is not judged.
- `charge_cycle`: passed, 30.000 for 30, 16.7 s from charging to the cup prompt.
- `pulses`: the pulses stop on a reset through the probe and on an assert. After a hard fault they run on, as before this ADR.

**The soak, first run** (21:02 to 21:32): `VERDICT FAIL`, on "settings missed 7 in a row, more than 6". All else as good as the baseline or better:

| | Baseline | Step 1 |
|---|---|---|
| Charges | 62, all passed | 62, all passed |
| `heap_free` | 96,248 throughout | 97,320 at the start, 96,152 from the first charge on |
| Web reads missed | 433 of 6,655, 6.51% | 409 of 6,834, 5.98% |
| Longest run, pollers of one request a read | 3 | 3 |
| Settings sweeps missed | 16 of 31, longest run 4 | 15 of 31, longest run 7 |

- **The cause was the test.** The settings poller did its 11 requests as one read, missed when any of them failed. At about 6% a request, a sweep misses about half the time (1 − 0.94¹¹) on either firmware, as both runs show. A run of misses of a coin is 4 to 7 long in 31 tries by chance, and says nothing about a server that stopped.
- **Fixed in `rc_run.py`:** each request of a round is one read now, the settings' too, so a share and a run mean the same for every poller. A trial of 65 s after the fix: 362 reads, 5.8% missed, the settings poller 22 reads.
- The broken JSON came back as in the baseline: 116 of the misses, all from the charge state.
- The heap's one step down at the first charge is an allocation made once; it held at 96,152 to the end.

**The soak, second run** (21:35 to 22:03, with each request a read): `VERDICT FAIL`, on console replies lost to the probe's link, not on the unit.
- The web: 392 of 6,500 reads missed, 6.03%; longest run 2. Within the limits (10%, 6), and as the baseline.
- `heap_free` 96,152 at all 56 readings. 55 charges, every one passed, each 30.000 for 30.
- **At 22:02:39, during the 56th charge, the probe's link dropped.** openocd read the debug port's id again, then "Failed to read memory at 0x20001518" (the RTT block) and "rtt: Failed to read up-channel 0 description", and read RTT no more. Every console command after it got no reply within 5 s, and the run failed on those.
- **The unit was well throughout.** Its last state before the drop: charging, 29.8 of 30, `up=3637`. On a new connection a minute later the replies waiting in its buffer came through: it had carried out the run's `state`, `charge stop`, `sim off` and `stats`, `up` going 3637, 3647, 3657, 3709, never back. `heap_free` 96,152. The web was answered to the end of the run.
- **On this bench before:** RTT read failures were seen only across a reset of the chip, where they are expected. This is the first drop with the chip running: one in about 2.5 hours of soak with the motors turning, none in the baseline's hour. Whether the new firmware, the motors' noise on the SWD wires or chance caused it, one case cannot say. The drops are counted from here on, so a drop that comes back only on newer firmware shows.
- **Fixed in `rc_run.py`:** when a reply is late and openocd has reported an RTT read failure, RTT is started again once and the wait starts over, without sending the line again. A unit that truly stopped answering still fails. Tried on the bench by stopping RTT by hand: the late reply came, nothing was sent twice, `VERDICT PASS`.

**Step 1 passes,** judged on the second run's record rather than a third run: every measure of the unit is within its limits (charges, the heap, no restart, the web), and each failure in the verdict is a reply the unit gave and the probe did not carry. Steps 2 and 3 run the soak with the fixed tool.
