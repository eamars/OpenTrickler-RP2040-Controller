# ADR-003: progress

## Gate 1: the web's memory, on today's server

Approved by the owner 2026-11-15 ("Approve gate 1"). Built and proven on the Pico W, on the probe with no scale sending to it (`weight=inf`). Its settings were copied before the first flash, at 1.20.137: `logs/adr003/picow/before_gate1/` (this PC only).

### Step 1: lwIP's memory in the console

Built at ce16b50 (1.20.154).
- `net` gives lwIP's heap, and the pools of TCP connections, TCP segments, pbufs that point at data (`pbuf`) and receive buffers (`pbuf_pool`): each in use, most ever in use, size, and failed allocations. The counters are 32 bits, and on whether `NDEBUG` is defined or not (`lwipopts.h`).
- The soak reads `net` at its start and its end, logs both, and fails when an `_err` count rose during the run. It sums its misses by kind: timeout, reset, refused, wrong.
- Found on the way: an HTTP error (`http.client.HTTPException`, a reply cut short) is neither an `OSError` nor a `ValueError`. The soak's poller did not catch it, so one ended its thread, and that poller's reads stopped being counted, with no word in the log. It is caught and counted as a wrong answer now.
- On the unit: `flash`, `menu_tour --compare tests/menu_baseline/pico_w` (form 36, the version, not judged) and `charge_cycle` each `VERDICT PASS`; `settings --compare` against the copy: nothing changed.
- **Just after a start, lwIP's heap holds 1,028 bytes of its 4,000 at rest and has been up to 2,560** (DHCP and mDNS at start-up). Under 3,000 bytes are left for the web.

### Step 2: the diagnosis

`soak 600` of 1.20.154 (`net` only, the heap at 4,000 bytes), 15:28 to 15:38: **the heap is the cause.**

| | |
|---|---|
| Charges | 21, every one passed |
| Web reads missed | 190 of 2,129, **8.92%**, longest run 4 |
| By kind | timeout 139, wrong answer 42, reset 9 |
| lwIP's heap | most used **3,920 of 4,000**; **7,404 allocations failed** in the run |
| Pools | none failed. TCP connections up to 12 of 12 (11 at the start, most in TIME_WAIT from the checks before); segments up to 9 of 32; receive buffers 1 of 24 |
| FreeRTOS heap | 95,272 free throughout |

- Every web connection's state, and every segment's header, comes out of a heap that is full: 7,404 times in ten minutes lwIP asked it for memory and got none. The pools never ran out. The verdict is `FAIL` on both counts, as the diagnosis expected.
- The TCP connections' pool is at its limit too, but lwIP takes the oldest connection in TIME_WAIT when it needs one, and counts no failure for it: none failed. It is watched in the next soaks.
- Log: `logs/rc/20261115_152804_soak.log` (this PC only).

### Steps 3 to 5: the two fixes, and the heap measured

Built at 0398515 (1.20.155): a request's values static (step 3), a reply in RAM copied when it is sent (step 4, `HTTP_IS_DATA_VOLATILE`: data at or above `SRAM_BASE`, the same on both chips), and a heap of 24,000 bytes to measure with. `soak 600`, 15:39 to 15:49, right after step 2's:

| | |
|---|---|
| Charges | 21, every one passed |
| Web reads missed | 3 of 3,743, **0.08%**, longest run 1 (`VERDICT PASS`) |
| By kind | timeout 2, refused 1, all three within 3 s at 15:48:48 |
| lwIP's heap | most used **8,824**; no allocation failed; 1,028 at rest at the start and at the end |
| Pools | none failed; TCP connections 12 of 12 at the end, segments up to 7 of 32 |

- Against step 2, beside it: 3,743 reads answered where 2,129 were asked in the same time. A missed read waits 5 s, so the pollers asked less while the heap failed.
- No wrong answer in 3,743, where step 2 had 42.
- The refused connection: the PC got a reset to its first packet. Its cause was not found; one in 3,743, with the two timeouts in the same 3 s, looks like the Wi-Fi.
- **The heap's size:** 8,824 and half as much again, 13,236, rounded up to 13,312 bytes (13 KB). The 24,000 was for this run only.
- Log: `logs/rc/20261115_153923_soak.log`.
