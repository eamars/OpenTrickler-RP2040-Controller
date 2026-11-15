# ADR-003: a real HTTP REST server

Status: **Open** 2026-11-15. Drafted 2026-11-13 and written again 2026-11-15 from the owner's answers; the direction is settled ("Settled with the owner"). **Gate 1 is built**: approved by the owner 2026-11-15 ("Approve gate 1"), built and proven on the Pico W the same day, and merged by the owner by its own PR (#128, merge 0d02bdb). lwIP's heap was the web's fault: at 4,000 bytes it failed 7,404 allocations and 8.92% of the web's reads were missed in a soak; at 16,384, none and 0 of 3,774. Its Pico 2 W run is left, before the ADR closes. What was built and measured is in [progress.md](progress.md). **Gate 2 approved** by the owner 2026-11-15 ("Yes to both": gate 2, and `?wait=s` on the event log in gate 4), with the API's table ([api_v1.md](api_v1.md)) agreed by the author and a second model in five rounds, and the rest of the interface approved by the owner ("the rest of the interface looks good to me"). **Gate 2 is built** on the Pico W the same day (1.20.179 at 4db625f) and closed by the owner ("Close gate 2"): our own server on lwIP's sockets, `http_rest.c` deleted and the old paths served through an adapter until gate 4, lwIP's heap 40,960 bytes, sized for every connection lwIP keeps. Its soaks pass with a tuning session's load and without, and the charge is as without it. What that load costs the web, a second a small read while a throw-sized reply streams, is handed to ADR-009, which decides a recording's format. It merges with gates 3 and 4 in this ADR's PR. **Left:** gates 3 and 4, to the same table; gates 1 and 2 on the Pico 2 W, before the ADR closes.

## What we want

- A method, a path and a body in; a status and a body out.
- A read never changes anything.
- A wrong value is refused, with a reason, and nothing is changed.
- What a person does with the web page, an agent can do with only the unit's name, and it learns how from the unit itself.
- The API is stable: once v1 is out, it only grows.
- Handlers know nothing of lwIP.
- One server for every board.
- The web keeps up with two browsers and a charge on the Pico W.

## Owner's direction

2026-11-13:
- The HTTP stack was meant to be a REST server and was not built in an elegant way. That part needs a rewrite.
- One implementation: a feature is never built a second way for another board.

2026-11-15:
- "The current HTTP stack is a copy paste version of the example file server, for which I think your judgement is correct that we should write our own, with full RESTFul API support instead of make shift solutions."
- "I'd like the future agent to be able to read and operate the API as well. When designing the interface, consider how the future AI agent can help to operate and manage the execution."
- "I need stable API from now on."
- "Don't use small ADR. If you need staged release, you can have smaller stage gates within ADR-003."
- The web's memory is fixed as the first step, developed and tested on the Pico W, which "has higher of memory issue (due to smaller RAM and flash)". The Pico 2 W later: "I think this is lower risk."

## Settled with the owner (2026-11-15)

- **Our own server, with a full REST API.** Not lwIP's server: it stays a file server, and handlers dress up as files. Not a library: ESP-IDF's server is ESP only, CivetWeb is large, and Mongoose's licence would have to be checked against this project's GPL-3.0.
- **Today's paths and codes go,** with no time beside the new ones. Nothing outside the built-in pages calls them now ("Not right now"). The owner's OWMC firmware, not released, finds units by mDNS `_opentrickler._tcp` and polls `/rest/charge_mode_state` for `s0` to `s5` (`main/ui/views/opentrickler_remote_controller_view.c`). It moves to the new API when it comes back. The mDNS service stays.
- **Each setting described once,** in this ADR.
- **The REST API is the agent's door.** The console over Wi-Fi, as OWMC's ADR-013 built it, is not in this ADR. The console and the API share one table of names, so the two say the same.
- **`http_rest.c` is deleted, not refactored** (the owner, merging gate 1: "As long as the module is deleted, then I'm happy to merge"). Gate 1's changes to it are a stopgap until gate 4. The ADR does not close while the file, or any copy of lwIP's file server, is in the tree.
- **Gate 1 first,** on the Pico W, merged by its own PR. Gates 2 to 4 are reached in this ADR's branch and merged in one PR at its close, so no release carries two servers or half an API.
- **The soaks of ADR-011's two firmwares in turn are dropped.** They change no decision: ADR-011 stays, and the old server goes. In their place both boards meet the same soak limits at the end. If the Pico 2 W alone does not, the RP2350's FreeRTOS port is the first suspect.

## Why here

OTA's upload (ADR-004), the web UI (ADR-008) and the tuning pages (ADR-009) all stand on the server. Built on today's, each would be built twice.

## What we know (checked 2026-11-13; brought up to date 2026-11-15)

**How it is built**
- `http_rest.c` is a copy of lwIP's file server (`httpd.c`, 2,770 lines in the SDK) with about 190 lines added: a linked list of paths and handlers, looked up where the file used to be opened, and its own decoding of the path and the query (PR #112 made it split the query before decoding it, so a value may contain `&`).
- A handler has the signature `bool handler(struct fs_file *file, int num_params, char *params[], char *values[])`. It answers by pointing the file at a static buffer that holds the whole response, status line and headers included.
- `http_rest.h` is included by the headers of charge mode, cleanup mode, motors, the scale, profiles, the EEPROM, the display, the LEDs, the servo gate, Wi-Fi and system control, so lwIP's headers reach all of them.
- 22 paths are registered in `rest_endpoints.c`. Pages are C strings generated from `src/html/` by `scripts/html2header.py`.

**What follows from that**
- There is no method and no body. POST is off in `lwipopts.h`. A request may be 1,023 bytes and its query 512, since ADR-012. (This said 128 before; `LWIP_HTTPD_MAX_REQUEST_URI_LEN` is no longer used.)
- One GET both reads and writes. `GET /rest/charge_mode_state?s2=1` starts a charge. `GET /rest/system_control?s6=true` erases the EEPROM. A browser that prefetches a link, or a second tab, can do either.
- **Any web page can do it** (read 2026-11-15). A page from any site, open in a browser on the unit's network, can carry `<img src="http://<unit>/rest/system_control?s6=true">`. The browser sends the GET, and the unit erases its EEPROM; the page never needs the reply. A POST with a JSON body is not sent from another site without the unit's consent first (the browser's CORS preflight), and the unit gives none.
- A value goes straight into the live settings with `strtof` or `atoi` and no check of its range. It is written from the network task while the charge task reads the same struct. `charge_mode_config.charge_mode_state` takes any number the web sends.
- Fields are named by codes (`c5`, `s0`, `m3`). Each setting is written out by hand three times in its module: a comment that maps the code to its name, a chain of `strcmp`, and a `snprintf` format. It is written again in the web page and in the screen menu.
- JSON is made with `snprintf` into fixed buffers of 64 to 384 bytes. Nothing parses JSON. A new setting means a larger buffer by hand: the coarse back-off's three fields took charge mode's from 256 to 384 (PR #108).
- Reading `/rest/charge_mode_state` clears the charge events, so two browsers take them from each other.
- The web starts a charge by putting a made-up button press (`OVERRIDE_FROM_REST`) into the menu's queue and setting a global.
- There is no password. Anyone on the network can read and change everything, and start the motors.

**Who calls it** (read 2026-11-15)
- The portal, the wizard and the mirror. `/rest/button_control` is called by no page.
- `rc_run.py`: `settings`, `soak`, `charge_fault` and the mirror's reads. `tests/web_test.py` stands in for the unit with made-up answers.
- OWMC, not released ("Settled with the owner").

**The network's memory** (read in the code and the build 2026-11-15; not measured on a unit)
- lwIP's heap is 4,000 bytes (`MEM_SIZE`), the Pico examples' default, kept in our copy of their `lwipopts_examples_common.h`. Its pools: 12 TCP connections, 32 TCP segments, 24 receive buffers.
- The rest of the RAM has room. On the Pico W, 198.6 KB of 256 KB is taken at link time (`tools/size_report.py`, main at d60151a), and about 95 KB of the FreeRTOS heap's 128 KB stayed free through ADR-012's soaks.
- What takes from lwIP's heap:
  - each connection's state (`struct http_state`), with room for a request's 20 values;
  - the header of each TCP segment sent. A reply is sent from where it lies, not copied, but each of its segments holds a header from the heap until the client acknowledges it. So a page download takes heap for as long as it is in flight, and longer on a poor Wi-Fi.
- Nobody has seen how full it runs: `MEM_STATS` and `MEMP_STATS` are 0, and `LWIP_STATS` is on only while `NDEBUG` is not defined.
- ADR-012's measurement (below) is what a heap at its limit looks like: 128 bytes more for each connection lost a third of the web's reads.
- A server on sockets copies every reply into that heap (`lwip_send`): on 4,000 bytes it would do worse than today. The heap is sized first, whatever the server.

**The socket API is off** (corrected 2026-11-15). This said "lwIP's socket API is on in this build (`LWIP_SOCKETS 1`)". lwIP has no option of that name; its own are `LWIP_SOCKET` and `LWIP_NETCONN`, and `lwipopts_examples_common.h` sets both to 0. No socket function is in the Pico W image (`nm` on `build_w/app.elf`). Turning them on adds two layers of lwIP, whose flash and RAM gate 2 measures. ESP-IDF's network is lwIP too, so the same API is there either way.

**Measured under load in ADR-011** (2026-11-14; `rc_run.py soak`: charges back to back while two browsers poll, as with the portal open on a phone and a PC; its [progress](../ADR-011-dependency_update/progress.md) has the runs)
- The server misses reads under that load. On the Pico W, 6.5% of reads went unanswered within 5 s or came back wrong, on main's firmware and after ADR-011 alike. On the Pico 2 W, 4.05% on main's firmware, 6.86% after ADR-011.
- Whether ADR-011 made the Pico 2 W worse was left to this ADR. The owner dropped the question on 2026-11-15 ("Settled with the owner").
- **Every JSON handler answers from one static buffer.** It writes the reply into the buffer and gives lwIP a pointer to it, not a copy (`HTTP_IS_DATA_VOLATILE` is 0 while dynamic file reads are off); lwIP sends it later, and sends a lost segment again later still. A second request to the same path rewrites the buffer while the first answer is still being sent, and the first client gets a mix. In the soaks, 1.8% to 3.4% of the charge-state reads came back as broken JSON. 13 handlers in 9 files do it. Two phones watching one charge see a wrong state now and then.
- `/display_buffer` answers with the bare 1,024 bytes and no HTTP header, though its handler says the header is included. A browser takes it; a strict client does not.
- A browser left open on the unit tripled the timeouts of a soak: the server's room is shared by every client on the network.

**Handed on by ADR-012** (2026-11-15). Found by the audit of 2026-11-14, which read every module outside the motors for the kind of faults ADR-012 lists for them. Only the first two were read a second time: read the line before building on any other. Nothing here was tried on a unit unless it says so.
- **A GET that changes the unit.** Reading a profile with `pf` selects it: the page's "Export config" reads profiles 0 to 7 and leaves 7 selected for the next charge. And `pf` is honoured only when it is the request's first value (`profile.c`).
- **The charge target is written with no length.** `s0` goes into an 8-byte buffer: `s0=1e30` writes some 26 bytes past it (`charge_mode.cpp`).
- **Values from the web are used unchecked, and can be stored:**
  - the scale's driver and baud indexes; a bad baud index sets a rate of 0, and the scale is dead until it is put right;
  - the servo gate's duty, ratio and speed: a servo driven to its end stop;
  - the LEDs' colour order and count;
  - the precharge's time and speed: the precharge is one delay of that time with the motor turning, and RST is ignored for as long;
  - the number of decimals: a value that is neither 2 nor 3 sends the menu to form 0, and the menu then runs the last mode again, which can be a charge.
- A profile name of 16 characters is stored without its ending and is read on into the gains.
- The web writes charge mode's and cleanup mode's state variables directly, and posts button events with no time limit, whatever is running: "exit cleanup" can end a charge.
- The 404 reply gives its length as 13 for a text of 71 bytes: an unknown path gets `HTTP/1.1 404 ` and nothing more.
- Handlers run in lwIP's thread, and some block it: queue sends with no time limit; the EEPROM's erase, about 5 s of one core and outside the EEPROM's mutex; erase-and-restart inside the handler, so no reply, with its reply flags `s5` and `s6` swapped.
- **The web's memory is this tight** (measured in ADR-012, step 5, on the Pico 2 W). Each connection's state comes out of lwIP's heap of 4,000 bytes (`MEM_SIZE`). Room for 32 values a request in place of 16 made that state 128 bytes larger, and a soak then lost 32% to 39% of the web's reads, twice over, in runs of up to 63, where it lost 5% before; with room for 20, 32 bytes more, it lost 5.7%. The charges were not touched either way.
- **The Pico W pays more for those 32 bytes** (measured 2026-11-15, ADR-012's Pico W pass: `rc_run.py soak` for 300 s each, one image after another within the hour; ADR-012's [progress](../ADR-012-motor_drivers/progress.md) has the runs):

  | From | Image | Web reads missed | Longest run | Connections reset by the unit |
  |---|---|---|---|---|
  | 11:38 (600 s) | ADR-012 (1.20.131) | 19.6% | 9 | 27 |
  | 11:51 | Before ADR-012 (1.20.84, ADR-013's code before its fixes; the same server) | 10.1% | 4 | 0 |
  | 11:57 | ADR-012 | 15.2% | 12 | 23 |
  | 12:04 | ADR-012 with a request's values static, below | 4.7% | 2 | 0 |
  | 12:12 | ADR-012 | 6.9% | 7 | 4 |

  - The Wi-Fi changed within the hour: ADR-012's image lost 15.2% before noon and 6.9% after. So images compare only beside each other: before ADR-012 and ADR-012 at 11:51 and 11:57; static values and ADR-012 at 12:04 and 12:12. A run of the image before ADR-012 after noon was lost: it hung at its start (ADR-012's faults 7 and 9), so whether static values do better than before ADR-012 is not settled.
  - Only ADR-012's image resets connections, in every run, most when the Wi-Fi is poor: a slow connection holds its state longer, so more of them share the heap. A connection whose state the heap cannot hold is refused: `http_accept` returns `ERR_MEM` and lwIP aborts it (`LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED` is off), which a client sees as a reset.
  - Every charge passed in every run.
- **What to do first: a request's values out of each connection's state.** Gate 1, step 3.
  - `struct http_state` holds `params[]` and `param_vals[]`: two arrays of `LWIP_HTTPD_MAX_CGI_PARAMETERS` pointers, 160 bytes at 20, in every connection, all of it from the heap.
  - They are used only in `http_find_file`, which splits the query, decodes it and calls the handler, in lwIP's thread, before the next request is read. The query they point into, `params_buf`, is static already for that reason. lwIP itself keeps them static when CGI is reached through SSI.
  - **How**, in `http_rest.c`: delete the two fields from `struct http_state`; under `#if LWIP_HTTPD_CGI`, declare `http_cgi_params` and `http_cgi_param_vals` as static arrays of `LWIP_HTTPD_MAX_CGI_PARAMETERS`, in place of the two `#define`s that point them at `hs->`; pass the static arrays to the two `rest_handler` calls in `http_find_file`. Seven lines.
  - It holds while every handler runs to its end in lwIP's thread. A server of our own makes the same choice: a request's values belong to the request being handled, not to the connection.
  - Tried on the Pico W in ADR-012 (the table's fourth row) and not built in: web server changes belong to this ADR (the owner, 2026-11-15). The Pico 2 W has not run it.
- **Done in ADR-012, on the server as it is:** a query was cut at 127 characters and at 16 values, so the charge-mode page's "Save to EEPROM" never arrived (512 and 20 now, and a longer query is refused whole); a Wi-Fi name of 32 characters or a key of 64 was stored without its ending (refused whole now).

**What the other ADRs need of the server** (read 2026-11-15)
- ADR-004 uploads an image of some 700 KB: a request's body is handed to its handler in pieces, never held whole.
- ADR-009 downloads a throw's recording, longer than any buffer: a reply is written in pieces too.
- ADR-005 may gzip the pages: the server sends a stored page with the headers it is given, so that needs no change to the server.
- ADR-008 and ADR-010 draw settings forms and screens "from the settings' own description, if ADR-003 brings one". It does.
- ADR-009 owns the modes' cleanup. This ADR's commands stop at one entry point an action (start a charge at a weight, stop it, the cleanup speed), which the menu, the console and the web all use; what the modes do inside is ADR-009's.
- ADR-006 sets the layout by role. Until then new files go flat in `src/`.

## What we do

Four gates, each proven on the Pico W before the next. Gate 1 is merged on its own; gates 2 to 4 in this ADR's PR at its close.

### Gate 1: the web's memory, on today's server

**Built 2026-11-15** on the Pico W, as written below; lwIP's heap is 16,384 bytes, from the larger of two soaks' most used (8,824 and 10,364) and half as much again ([progress](progress.md)). It makes lwIP's memory visible, takes the two causes the code shows out of it, and sizes it from what is measured. No path, page or stored setting changes. Its numbers are the floor the new server is held to.

1. **lwIP's memory in the console.**
   - A console command `net` (the `stats` line is long already) gives lwIP's heap: its size, what is in use, the most ever used, and the allocations that failed. The same for the pools of TCP connections, TCP segments and receive buffers.
   - `MEM_STATS`, `MEMP_STATS` and `LWIP_STATS` are set on in `lwipopts.h`, so they do not depend on `NDEBUG`.
   - The soak reads `net` at its start and its end and writes it in its log. It fails on an allocation that failed during its run. It also sums its misses by kind: no answer in 5 s, a connection reset, a wrong reply.
2. **The diagnosis, before any fix.** A soak of that image. If reads are missed while lwIP reports no failed allocation, the heap is not the cause: gate 1 stops there and comes back to the owner with the numbers.
3. **A request's values out of each connection's state:** the seven lines measured in ADR-012 ("What to do first", above).
4. **A reply in RAM is copied when it is sent.** A handler's reply is copied into its connection's send queue; the pages, in flash, are sent from flash as today. One condition in `http_rest.c` (`HTTP_IS_DATA_VOLATILE`). A second request then cannot rewrite a reply in flight, and two phones no longer see a broken state. It costs heap while a reply is in flight, and the next step counts that.
5. **The heap sized from a measurement.** A soak with a heap far larger than needed (24 KB, a figure to measure with, not to keep) gives the most used under the soak's load. The heap is set to that and half as much again; a pool that failed is raised the same way. The figures, and the RAM they take at link time, go in the progress.
6. **The gate's soak,** with the heap as sized.

The images are soaked 600 s each, one after the other within the hour, with nothing else on the unit's pages: `net` only (step 2); the two fixes with 24 KB (step 5); the two fixes with the sized heap (step 6).

How the unit shows it works:
- Before the first flash, a copy of the unit's settings: `rc_run.py settings --host <Pico W> --save` (it runs 1.20.137, ADR-012's branch).
- `flash`, `menu_tour --compare tests/menu_baseline/pico_w` and `charge_cycle` each end `VERDICT PASS`.
- The gate's soak (step 6): web reads missed at most 1%, at most 2 in a row on one poller (the soak's own limits); no connection reset by the unit; no allocation failed in lwIP; every charge passed; `heap_free` given back.
- The size report: RAM at link time with the sized heap, against 198.6 KB today.
- `settings --compare` against the copy: nothing changed.
- If step 6 misses the limits while lwIP reports no failure, the misses left are not memory's. It comes back to the owner with the numbers, to merge on what it fixed or not.
- The Pico 2 W runs gate 1's image when the owner next puts it on the probe, before this ADR closes. That is not a condition of gate 1's merge (the owner, 2026-11-15: the Pico 2 W is the lower risk).

### Gate 2: our own server

Written in full 2026-11-15, for approval, with the API's table: [api_v1.md](api_v1.md). Gates 2 to 4 build to that table.

**Built 2026-11-15** on the Pico W, and closed by the owner ("Close gate 2"). Where the build differs from the text below, the text says so; what was built and measured is in [progress](progress.md).

**The server** (`http_server.c` and `.h`, new):
- One FreeRTOS task, **below the menu's task, which runs the charge loop** (6), so at 5 or under: the server formats floats on an RP2040 with no FPU, and must never take the loop's turn. lwIP's own thread is at 7. Built at 3, below the cyw43 task too (4), which takes in the clients' ACKs. It waits in `select` on the listening socket and on up to 6 clients, as many as a browser opens to one host; a seventh waits in lwIP's backlog.
- **A slot can be parked:** a request held, with a deadline and a condition, until there is something to send (api_v1.md, `?wait=s`). The task's `select` wakes at least every 50 ms to look at parked slots, so no other task has to wake it; at most two are parked, and a third request with `wait` is answered at once. Built here, so the event log's `?wait` (gate 4) and ADR-009's recordings change nothing in the server.
- **On lwIP's sockets,** with `LWIP_SOCKET` and `LWIP_NETCONN` turned on. One task serves every client through `select`, and a send that does not fit waits while the others go on: a phone on a poor Wi-Fi loading the portal holds up nobody. lwIP's netconn has no `select`: it would take a task a client, or callbacks in lwIP's own thread, which is what this ADR leaves. Sockets are also what ESP-IDF's own server uses.
- **What sockets cost is measured, not guessed:** their flash, their RAM, and lwIP's heap. `lwip_send` copies every reply into that heap, and the pages are 91 KB. Each connection's queue is held to `TCP_SND_BUF` (8 segments, 11.7 KB); if two page loads at once take too much of the heap, `TCP_SND_BUF` comes down. Built: 2 segments, and the heap sized for every connection lwIP keeps, since a closed one holds its last bytes until they are acknowledged (40 KB; [progress](progress.md)).
- **A client's state is in the server's own table,** 6 slots, not in lwIP's heap: a line of 256 bytes, a JSON body of 1 KB, and where its reply stands. About 7.5 KB of static RAM in all. Built: 2.1 KB a slot, taken from the FreeRTOS heap at start rather than the RAM taken at link time.
- **Headers are read a line at a time.** The server keeps the six it uses (`Host`, `Content-Length`, `Content-Type`, `Connection`, `If-None-Match`, `Expect`) and passes over the rest without storing them, up to 8 KB in all: a browser sends every cookie it holds for the address, such as a router's that had it before. A request line over 256 bytes is refused with 414 (built at 640: the old paths' queries reach 511 characters). `Expect: 100-continue` is answered, so `curl` does not wait a second.
- **Only the unit's own name is served** (`Host`, api_v1.md's rules): an API request under another name gets 403, a page is redirected to the unit's address. A page that has pointed its own name at the unit (DNS rebinding) is so refused.
- **HTTP/1.1, and a connection stays open between requests** (keep-alive), closed after 5 s idle. A page that polls every 500 ms then keeps one connection, where today each poll is a new one and the unit holds the last in TIME_WAIT (gate 1's pool of TCP connections, full at the end of each soak). When the table is full and a client waits, the connection idle longest is closed, so one browser's six do not lock out the next.
- **A reply that makes no progress for 10 s, while the unit has something to send, is aborted,** so lwIP frees what it held (a parked request is not stalled): a phone that left the Wi-Fi in the middle of a page would otherwise hold up to `TCP_SND_BUF` of the heap until TCP gives up, minutes later.
- **A send that fails for want of lwIP's memory waits a tick** before it is tried again: `select` says a socket is writable by its send window, not by the heap, and the task would spin.
- **lwIP's pools for sockets:** `MEMP_NUM_NETCONN` is 4 by default; six clients and the listener need 8 (built: 16, since a netconn lives until its connection has closed). One connection may queue 32 segments (`TCP_SND_QUEUELEN`), the whole of the segments' pool; `net` measures both, and they are sized from it as gate 1 sized the heap.
- **An entry point never waits.** A handler that hands work to another task does so with no wait: today's web start sends to a queue of five with no time limit (`charge_mode.cpp`), which in the server's task would hold the web. **Not built in gate 2** (found after its close): the old charge and cleanup handlers, served through the adapter, still send their button events with no time limit (`charge_mode.cpp`, `cleanup_mode.cpp`), so a full queue holds the web until the menu reads it. Moved to gate 4 by the owner ("Yes, gate 4"), where those handlers are replaced.
- A reply of known length carries `Content-Length`; one written as it is made (the schema; later ADR-009's recordings) is sent chunked. API replies carry `Cache-Control: no-store`; the schema and the pages an `ETag`, the build's hash, and are answered 304 when unchanged, so a reload does not send the portal's 91 KB again.
- **Our own small parsers:** the request line and the headers above (a body with no `Content-Length`, chunked included, is refused with 411), and JSON for flat objects (strings, numbers, true, false, null). No library: a new dependency would be its own ADR, and what is parsed is small.
- **Routes** are a table of method, path (with `{id}` for a powder profile) and handler: 404 for no such path, 405 with `Allow` for no such method.
- **A handler** gets the request (method, path, index, query, body) and writes its reply through the server's calls: a status, a header, bytes, JSON. It sees no lwIP and no socket. It runs in the server's task, so a slow one (the EEPROM's erase, 5 s) holds the web, not the network.
- A body larger than 1 KB is handed to its handler in pieces as it comes, for ADR-004's upload (a PUT of bytes). None of the routes takes one yet: 413. The largest JSON body, `settings/charge`'s, is about 650 bytes. **Not built in gate 2** (found after its close): a body over 1 KB is refused with 413, but no handler can take one in pieces yet. Moved to gate 4 by the owner ("Yes, gate 4").
- The pages are sent from flash, in pieces as the socket takes them.

**The old paths, through an adapter, until gate 4** (agreed by the owner 2026-11-15). `http_rest.c` leaves the build in this gate: one server runs, ours. The old handlers keep their signature; an adapter (`rest_adapter.c`) splits and decodes the query as `http_rest.c` did, calls the old handler, and sends the bytes it points at. An old reply has no length, so it ends its connection, as today. The adapter and the old handlers go in gate 4. No second server on another port: that would cost lwIP's memory twice.

**How the unit shows it works:**
- The image: our server on port 80; `GET /api/v1/charge` and `GET /api/v1/display` as the first new paths, the pages, and every old path through the adapter. The soak reads `/api/v1/charge` and `/api/v1/display` in place of their old two, and the settings through the old paths.
- On the Pico W: `flash`, `menu_tour --compare`, `charge_cycle`, each `VERDICT PASS`. The portal, used by hand from a browser: its pages and forms work as before.
- `soak 600` within the limits, no allocation failed in lwIP; `net`'s heap and pools, the flash and the RAM, given against gate 1's image (665.0 KB, 211.4 KB). Also measured with `net`: two page loads at once, and six clients at once.
- **The load a tuning session puts on the charge** (ADR-009's data, measured now so the server is not changed then): a reply the size of a throw's samples (600 rows of 16 numbers, from a route under `/api/v1/debug`) pulled back to back, and a client asking four times a second, through `charge_cycle` and a soak. It passes with no late feed of the step pulses (`motor`), the scale's readings as regular as without it (ADR-013's interval and longest gap), each charge's time and weight as without it, and no allocation failed. If the charge is disturbed, the answer is the tasks' order, not the protocol.
- A parked request answered when its condition comes and when its time is out, two parked at once, and a third answered at once.
- Requests that are wrong are answered or closed, and the unit goes on (`up=` keeps counting): a value with no `=`, headers of 8 KB with a long cookie, a body shorter or longer than it says, a request cut off, an unknown method, seven clients at once, a client that stops reading in the middle of a page, a request under another `Host`, a POST with no `Content-Type`.
- **If sockets need more than 32 KB of lwIP's heap to pass the soak, gate 2 stops** and comes back to the owner with netconn's case.

### Gate 3: the API, each setting described once

- **One table a module:** for each setting its name, type, unit, limits or choices, access, when it takes effect, when it is refused, a line on what it is, and where its value lives (the field in the module's struct). The module adds a check for what crosses fields (a minimum speed not above the maximum) and an apply for what follows a change (the driver set up again).
- **One code for every table:** a GET builds the reply from it, a PATCH checks the whole request on a copy and the module applies it, and save stores the module. `saved` is worked out from the CRC of the struct in RAM against that of what was last stored, so the menu's and the console's writes count. `GET /api/v1` and `GET /api/v1/schema` are made from the tables, choices with their labels.
- **One lock for what a throw reads.** "Refused during a throw" would otherwise be a check, then a write, while the throw starts in another task when a cup lands, and reads its target, gains and thresholds live. The charge holds the lock for the length of a throw, from the start of `zeroing` to its verdict (`in_throw`); a PATCH checks and applies while holding it, and one that cannot have it at once answers 409 `busy`. So a throw never sees half a write.
- **A test stand-in is never stored:** the console's simulated scale runs in place of the stored driver without writing it into the stored struct, as it does today (`scale.c`), where a save while it runs would store it.
- **The console reads and writes the same tables:** `setting <resource>`, and `setting <resource> <name>=<value> ...` (the owner: one table of names for the console and the API).
- The settings and powder profiles resources of [api_v1.md](api_v1.md), and all of them at once as the export (`GET /api/v1/settings`). The import is the client's: a PATCH a resource. The schema carries each field's label, group and step now, for ADR-008's forms and ADR-010's screen. The portal's forms, its export and import, and `rc_run.py settings` move to them module by module, and each module's old handler goes when its form has moved.
- **Proof of the tables:** every setting read, written back, and read again, unchanged (nine significant digits); each value outside its limits refused, the setting unchanged; a write during a throw refused.
- No stored struct changes its layout: the tables point into the structs as they are.

### Gate 4: commands, events, the switch

- **Commands:** charge start and stop, cleanup start, stop and speed (with its optional time), the scale's zero, the gate, save, restart and erase, and `POST /api/v1/stop`, as in [api_v1.md](api_v1.md). Each is one entry point to the application, which the web, the console and the menu all use, and which knows who asked (`by`). The made-up button press (`OVERRIDE_FROM_REST`) goes. **An entry point never waits** (gate 2's, moved here by the owner 2026-11-15): one that hands work to another task does so with no wait. A start that cannot hand it on answers 409 `busy`; a stop, which never fails (api_v1.md), does not go through a queue that can be full. The old handlers' sends with no time limit go with them.
- **A body larger than 1 KB handed to its handler in pieces as it comes** (gate 2's, moved here by the owner 2026-11-15), for ADR-004's upload, so ADR-004 changes nothing in the server. Shown as gate 2 showed the stream for ADR-009: a route under `/api/v1/debug` that takes a PUT of bytes and counts them, and an upload of 700 KB during a soak's charges, with no late step feed, the charge as without it, and no allocation failed.
- **One job at a time, said by the unit:** `activity` in `GET /api/v1/system`; a stop never fails; a charge or a cleanup that ended on a fault is in `fault` until a stop or a press acknowledges it.
- **One target:** the charge's `target` is the one the menu's weight digits show and set, so a start at the unit throws what the API set, and the other way.
- **One event log for the unit:** the newest 64 kept and numbered, with `boot` and `now_ms`; each client reads from where it left off. An event is a change, not a state. With the owner's leave, `?wait=s` on it, on gate 2's parked slot: the scripted client waits for a throw's end, and two waiters run through a soak. The charge and each numbered throw have their own events, with ADR-013's pauses and the reason a throw ended.
- `GET /api/v1/scale` (with its weight's unit where the make says it, and what the make can be asked to do), `/gate`, `/motors`, `/system`, `/network`, `/display` and `/display/frame`. The mDNS announcement adds the API's and the firmware's versions, and the host name is made from the id with its ending.
- **The switch:** the portal, the wizard and the mirror on the new API. The wizard restarts the unit once the Wi-Fi save is answered: today it sends both at once (`wizard.html`), and a restart handled first loses the settings.
- **The adapter, the old handlers, `http_rest.h` in the modules' headers and lwIP's httpd options are deleted.** The ADR closes only when no copy of lwIP's file server is in the tree (the owner, at gate 1's merge).
- **The client:** a Python module in `tools/`, which reads the unit's schema and works from it, not from paths written into it. The proof uses it, `rc_run.py` uses it, and an agent can. `tests/web_test.py`, the stand-in for page work, answers v1.
- The proof at the close (below) runs on both boards, with gate 1's Pico 2 W run done before it.

## How the unit shows it works

At the close, on the Pico W, then the Pico 2 W:
- The scripted client calls every path in the unit's description and checks each answer against the tables.
- A GET changes nothing: everything read twice, and the unit's state is the same.
- Each value outside its limits is refused with its reason, and the setting is unchanged.
- A write from another site's page is not sent by the browser: a page on the PC tries it.
- Two browsers at once each see every charge event.
- The web portal works as before, from a phone.
- `charge_cycle` passes with a browser polling throughout.
- `rc_run.py soak` within gate 1's limits on each board, with nothing else on the unit's pages. No broken reply.
- The size report shows what the server costs against today's.

## Not in this ADR

- New pages or a new look (ADR-008).
- TLS.
- A password or any other guard on the network, beyond refusing writes from other sites' pages. Whether to add one is the owner's call and its own ADR.
- A push channel for live data (server-sent events or a WebSocket). Polling stays, with a held request (`?wait=s`) on the event log if the owner allows it (api_v1.md, "For the owner", 7): each reply a whole document, not a stream.
- ADR-009's experiments and recordings, reserved in api_v1.md: built there, to the shapes fixed here.
- The console over Wi-Fi (the owner, 2026-11-15).
- What the modes do inside (ADR-009).

## Settled at gate 2's approval (2026-11-15: "Yes to both")

- **The API's table,** [api_v1.md](api_v1.md): every path, method, body, reply and status, with the names. Once approved it is v1, and only grows.
- **What it must grow into** (api_v1.md): checked 2026-11-15 against every ADR, the menu and the TODOs in `src/`, at the owner's word ("not just satisfy the current use case, but also future expansions"). The rules for growing within v1, and a place now for each later need: ADR-014's charge profiles, ADR-009's recordings, tests and tuning, ADR-004's update, ADR-005's features, ADR-008's and ADR-010's forms.
- **Reviewed by a second model** (Fable 5.2, its own session, read only), at the owner's word: 4 blocking faults, 15 to fix, 9 minor, each read in the code by the author and taken but for one, argued ("The review" in api_v1.md).
- **Settings live until saved,** with `saved` in every reply (its "For the owner", 1).
- **The names `powder_profiles` and `charge_profiles`,** and what the next throw uses kept in the charge (its "For the owner", 2).
- **The limits marked *p*** in the table, where today's code has none.
- **A write is any number of a resource's fields at once,** checked whole; refused during a throw (`zeroing`, `charging`) where the throw reads it, and the motors' during a cleanup too.
- **Sockets,** with a measured stop at 32 KB of lwIP's heap.
