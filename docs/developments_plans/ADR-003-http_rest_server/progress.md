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

### Step 6: the gate's soak

**First, at 13,312 bytes** (4172a2b, 1.20.156), 15:51 to 16:01: `flash`, `menu_tour --compare` (form 36, the version, not judged), `charge_cycle` and `soak 600` each `VERDICT PASS`.

| | |
|---|---|
| Charges | 21, every one passed |
| Web reads missed | **0 of 3,774** |
| lwIP's heap | most used **10,364**; no allocation failed; 1,028 at rest at the start and the end |
| Pools | none failed; TCP connections 12 of 12 at the end, segments up to 7 of 32 |

- The heap's most used moved from 8,824 to 10,364, 17% more, between two soaks of the same load in the same hour: it follows the Wi-Fi, as ADR-012's misses did. 13,312 left 22% over the larger, short of the half over it the rule asks.
- **So the heap is 16,384 bytes:** 10,364 and half as much again, 15,546, rounded up to 16 KB. 3 KB more of the RAM, of the Pico W's 47 KB left at link time.
- Log: `logs/rc/20261115_155129_soak.log`.

**Then at 16,384 bytes** (2a8ccba, 1.20.157), the image gate 1 merges, 16:03 to 16:13: `flash`, `menu_tour --compare` (form 36 not judged), `charge_cycle`, `soak 600` and `settings --compare` against the copy from before each `VERDICT PASS`.

| | |
|---|---|
| Charges | 21, every one passed |
| Web reads missed | **0 of 3,774** |
| lwIP's heap | most used 10,364 of 16,384 (63%); no allocation failed; 1,028 at rest at the start and the end |
| Pools | none failed; TCP connections 12 of 12 at the end, segments up to 7 of 32 |
| FreeRTOS heap | 95,272 free throughout |
| Size (`size_report.py`) | flash 665.0 KB, RAM at link time 211.4 KB; main at d60151a: 663.6 KB and 198.6 KB |

### Gate 1: where it stands

**Built on the Pico W, 2026-11-15.** The four soaks, beside each other within the hour:

| Image | lwIP's heap | Most used | Failed allocations | Web reads missed |
|---|---|---|---|---|
| 1.20.154, `net` only | 4,000 | 3,920 | 7,404 | 8.92% (190 of 2,129) |
| 1.20.155, the two fixes | 24,000 | 8,824 | 0 | 0.08% (3 of 3,743) |
| 1.20.156, the two fixes | 13,312 | 10,364 | 0 | 0 of 3,774 |
| 1.20.157, the two fixes | 16,384 | 10,364 | 0 | 0 of 3,774 |

- The web's misses on the Pico W were lwIP's heap: the Pico examples' 4,000 bytes, of which under 3,000 were left after start-up.
- The pool of TCP connections ends each soak full, 12 of 12, most in TIME_WAIT, and lwIP takes the oldest of those when it needs one: it never failed. The server of gate 2 is measured against it with `net`.
- No stored struct changed; the unit's settings are as they were.
- **Left of gate 1:** its image on the Pico 2 W, when the owner next puts that unit on the probe, before this ADR closes (the owner, 2026-11-15). Not a condition of its merge.
- **Merged by the owner** 2026-11-15: PR #128, merge 0d02bdb, on the condition that `http_rest.c` is deleted in a later gate ("As long as the module is deleted, then I'm happy to merge"). Gates 2 to 4 go on in the same branch, from the merge.

## Gate 2: our own server

Approved by the owner 2026-11-15 ("Yes to both": gate 2, and `?wait` on the event log in gate 4). Built and proven on the Pico W.

### What was built

At 89dbf75 and ce67426:
- **`http_server.c`**: one task at priority 4 (below the menu's 6, which runs the charge loop, and the charge screen's 5), `select()` over the listener and 6 slots, waking every 50 ms. The slots, 2.1 KB each, come out of the FreeRTOS heap at start, not out of the RAM taken at link time. Headers are read a line at a time; the server keeps `Host`, `Content-Length`, `Content-Type`, `Connection`, `If-None-Match`, `Expect` and `Transfer-Encoding`, and passes over the rest, up to 8 KB. Keep-alive with a 5 s idle limit; a 10 s stall limit that resets the connection, so lwIP frees what it held; the idlest connection closed to take a new client; `Host` checked (403 for the API and the old paths, a redirect for pages); 415 for a write that is not JSON; 405 with `Allow`; routes with `{id}`; replies from the slot's buffer, from flash with an `ETag` (304 when unchanged), or chunked as they are made; parked requests, two at most.
- **`rest_adapter.c`** serves the old `/rest/...` handlers: the query split and decoded as `http_rest.c` did (20 values, 511 characters), each reply copied at once (so the next request to the same path cannot rewrite one in flight), sent again with our headers and kept alive. A value with no `=` is given as `""`: `http_rest.c` gave the handlers NULL, which every handler used as a string. `http_rest.h` keeps only the old handlers' shape.
- **`http_rest.c` is deleted.** lwIP's httpd options are gone from `lwipopts.h`.
- **`api_v1.c`**: `GET /api/v1/charge` (what gate 2 has of it: state, target, powder profile and its name, weight, elapsed), `/api/v1/display` and `/api/v1/display/frame`; `/api/v1/debug` (the FreeRTOS heap, lwIP's heap and pools with the netconns, the server's counts), `/debug/load` (rows of numbers as a recording's samples will be) and `/debug/wait` (a parked request).
- The pages are served from flash by routes of their own, the generated header (html2header.py) passed over.
- **lwIP:** `LWIP_SOCKET` and `LWIP_NETCONN` on (the old `LWIP_SOCKETS 1` was no lwIP option), lwIP's names only, `SO_LINGER` for the resets, mailboxes of 8, 10 netconns, and a socket's send buffer of 4 segments (below).

### Found on the way

- **lwIP keeps an errno of its own.** The SDK builds it with `LWIP_PROVIDE_ERRNO=1` (`pico_cyw43_arch`'s CMake), so lwIP's sockets set the `int errno` lwIP declares, not newlib's (a macro over `__errno()`); the image holds both. Read with newlib's, a send that would block read as errno 0 and was taken for a failure: every page longer than the send buffer was cut off, after 2,920 to 30,660 bytes. The server now reads lwIP's, and fails to build if newlib's is in scope.
- **A socket copies what it sends into lwIP's heap.** One page load at the examples' send buffer of 8 segments (11.7 KB) took the heap to 14,888 of its 16,384 bytes. At 4 segments (5.8 KB) two page loads at once take it to the same 14,888 (eight segments in flight either way), each in about half a second, with no allocation failed.
- The first image made of the new code built with no warning, on both boards.

### Shown on the Pico W, 1.20.172 at ce67426

- `flash`, `menu_tour --compare tests/menu_baseline/pico_w` (form 36, the version, not judged) and `charge_cycle`: `VERDICT PASS`. `settings --compare` against the copy from before gate 1: nothing changed, read through the adapter.
- **The portal by hand** in a browser: the page and its polling; the Scale and the Charge Mode forms filled with the unit's values through the old paths. The charge-mode form's query, 148 characters and 16 values, sent back unchanged: taken, nothing changed.
- **`rc_run.py web_robust`** (new), `VERDICT PASS`: a value with no `=` (200), a Cookie of 6 KB passed over (200), headers over 8 KB (413), a Host of 300 bytes (413), a request line of 700 bytes (414), an unknown method and OPTIONS (405), HTTP/2.0 (400), another Host for the API and an old path (403) and for a page (302 to the unit's address), a POST with no type and one of text (415), a JSON POST to a path that only reads and one with a body longer than it said (405), a chunked body (411), a body of 5,000 bytes (413), a request cut off (closed), no such path (404), HTTP/1.0 with no Host (200), the page with its ETag (304, no body); the unit answering after each; seven clients at once, all answered; a client that stops reading part way through a page and one that sends less of a body than it said, both reset by the stall limit within 14 s, lwIP's heap back to 1,028 at rest; parked requests woken by their condition after 1.50 s and by their time after 2.06 s, two held 3.08 s and a third answered at once (0.06 s).
- **Size** (`size_report.py`): flash 708.4 KB, RAM at link time 214.2 KB, against gate 1's 665.0 and 211.4. lwIP 133.0 KB (106.4 before: its sockets and netconn), the server 12.8 KB (the old one 4.6). The slots take 12.6 KB and the task's stack 6 KB of the FreeRTOS heap: 76,944 bytes free after start, about 95,000 before.

### The soaks, and what each changed (1.20.172 to 1.20.178)

From 18:16 to 19:58, `soak 600` and `soak 600 --load` (new: what a tuning session asks of the unit, a reply of 600 rows of 16 numbers pulled back to back and a client reading the charge four times a second), a pair to each image. The version's last number is the commit count:

| Image | lwIP's heap, send buffer | Soak | Most of the heap used | Failed allocations | Reads missed |
|---|---|---|---|---|---|
| 1.20.172 (ce67426) | 16,384, 4 segments | gate | 14,972 | heap 8 | 0 of 3,780 |
| | | load | 15,212 | heap 340 | 43.13% |
| 1.20.173 (59848e1) | 24,576, 2 segments | gate | 19,508 | none | 1 of 3,645 |
| | | load | 24,212 | heap 54 | 8.46% |
| 1.20.175 (6354fbb) | 40,960, 2 segments | gate | 14,888 | none | 0 of 3,752 |
| | | load | 28,748 | TCP connections 5 | 5.31% |
| 1.20.178 (b33a262) | 40,960, 2 segments | gate | 19,508 | none | 0 of 3,790 |
| | | load | 27,208 | none | 0 of 3,148 |

What each found:
- **A socket holds what it sends in lwIP's heap until the client acknowledges it, and so does a connection the server has closed.** lwIP keeps up to 12 TCP connections (`MEMP_NUM_TCP_PCB`), closed ones included, so the heap is sized for 12 send buffers of 2 segments with their headers, about 37 KB, and lwIP's own: 40,960 bytes (e2977b8). The 16 KB more came out of the FreeRTOS heap (128 to 112 KB). The most any soak used is 28,748, under the 32 KB at which this gate was to stop and come back with netconn's case.
- **A send that finds no heap says "would block"**, and `select` says writable again at once: the task spun. A send that wrote nothing waits 10 ms (59848e1).
- **A netconn lives until its connection has closed**, not until the server lets go of it: 10 ran out 17 times in a soak. 16 (e2977b8).
- The load's JSON was invalid, its column names unquoted, so every load read at 1.20.173 was a miss (e2977b8).
- The server's task at 4 shared its priority with the cyw43 task, which takes in the clients' ACKs: 3 (6354fbb).
- **One stream held the server.** At 1.20.175, small requests beside a stream waited 0.33 s at the median and up to 1.4 s (one client beside the load, a measurement from the PC): on a good Wi-Fi the ACKs came back as fast as chunks were made, and the stream went on for its whole 1.8 s. One chunk a pass let the others in but took the stream to 4 s, one segment in flight whose ACK the PC delays (2234c44). Two a pass fill the send buffer (f77b815): small requests 0.125 s at the median and 0.20 s at most beside the stream, the stream 2.3 to 3.1 s. The 5 connections refused at 1.20.175 went with it.
- **A refusal could be lost to a reset** (b33a262). Closed with the client's bytes unread, lwIP answers with a reset, which can overtake the reply: a 413 for headers of 8 KB was lost so. A refusal that leaves input unread now stops sending, reads and drops what is left for up to 2 s, and closes at its end. `web_robust` passed three times in a row.
- **The soak's check of memory** compared the FreeRTOS heap at the end with its lowest in the first 300 s, and each open socket holds a FreeRTOS queue or two: it failed at 1.20.173 on 128 bytes. It compares two quiet moments now, before the browsers and 10 s after they stop (e2977b8).
- **`tcp_pcb` at 12 of 12** at the start and the end of every soak is no fault. Closed connections wait out TIME_WAIT there, and lwIP takes back the oldest for a new one, counting a failure only when none can be taken (`tcp_alloc`, lwIP's `tcp.c`).

The charge, at 1.20.175 and 1.20.178, with the load and without: no late feed of the step pulses (`motor`'s underruns 0), the scale's interval 100 ms in every charge with no pause, 21 charges a soak, each to 30.000. At 1.20.178 a charge took 16.905 s on average both ways (sd 0.097 s without the load, 0.169 s with it), 16.7 to 17.2 s.

### Shown on the Pico W, 1.20.179 at 4db625f

The image's sources are 4db625f's; its version reads `-dirty` for two documents not yet committed. 4db625f adds the time a stream takes to make to `/api/v1/debug`, each soak read's time, and a charge before the soak's first quiet reading: the first charge after a start creates the charge screen's task, kept for good, and a soak run straight after a flash read its 1,168 bytes as memory not given back.
- `flash`, `menu_tour --compare tests/menu_baseline/pico_w` (form 36, the version, not judged), `charge_cycle` and `web_robust`: `VERDICT PASS`, 20:18 to 20:19.
- **The gate's soak**, `soak 600`, 20:19 to 20:30: `VERDICT PASS`. 0 of 3,780 reads missed. lwIP's heap at most 16,428 of 40,960, no allocation failed; the FreeRTOS heap 59,480 quiet before and after. 22 charges (the first before the browsers), each to 30.000, 16.882 s on average (sd 0.101); the scale's interval 100 ms, no pause; no late feed of the step pulses.
- **With a tuning session's load**, `soak 600 --load`, 20:30 to 20:41: `VERDICT PASS`. 0 of 3,083 reads missed. lwIP's heap at most 27,208, no allocation failed; the FreeRTOS heap 59,480 quiet before and after. 22 charges, each to 30.000, 16.841 s on average (sd 0.171); the scale's interval 100 ms, no pause; no late feed. **The charge is as without the load.**
- **Size:** flash 708.8 KB, RAM at link time 222.7 KB, against gate 1's 665.0 and 211.4: the RAM is lwIP's heap, 24 KB more, less the 16 KB taken from the FreeRTOS heap.

### What a tuning session's load costs the web

Measured at 1.20.179. The load is what `--load` pulls back to back: 82,391 bytes, 9,600 numbers.

| | Small reads, median by poller (95%; the longest of all) | The load, median |
|---|---|---|
| Soak, no load | 0.11 to 0.12 s (0.25 to 0.26 s; 2.69 s) | |
| Soak, with the load | 1.02 to 1.08 s (1.62 to 1.66 s; 4.20 s) | 7.98 s |
| Quiet unit, one client beside the load (1.20.178) | 0.125 s (longest 0.20 s) | 2.3 to 3.1 s |
| During two charges, one client beside the load | connect 0.016 s, reply 0.438 s (longest 0.61 s) | |

- **Making the stream is half its time during a charge.** `stream_make_ms` over the load soak's 29 loads: 3.66 s of each 7.33 s, 65 ms a chunk. On a quiet unit 1.12 s of 3.14 s: 117 µs a number with newlib's `%g`, 20 ms a chunk. The server's task is at 3, below the charge's tasks, as gate 2 wants: during a charge it makes a chunk 3.3 times slower. The rest of the stream's time is its sends, two segments in flight (`TCP_SND_BUF`), and the next chunk made only once the last is with lwIP.
- **A small read waits for the server's passes, not for a connection.** The connect takes 16 ms; the reply waits while the one task makes the stream's two chunks a pass, and a new connection needs two passes (accepted, then read and answered). The more clients, the more each pass does.
- **No Nagle stall:** small reads on one kept-alive connection, as a browser polls, 86 ms at the median against 62 ms with a connection each (quiet unit).
