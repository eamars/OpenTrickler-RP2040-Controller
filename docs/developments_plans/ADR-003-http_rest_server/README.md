# ADR-003: a real HTTP REST server

Status: **Open** 2026-11-15. Drafted 2026-11-13 and written again 2026-11-15 from the owner's answers; the direction is settled ("Settled with the owner"). **Gate 1 is built**: approved by the owner 2026-11-15 ("Approve gate 1"), built and proven on the Pico W the same day, and merged by the owner by its own PR (#128, merge 0d02bdb). lwIP's heap was the web's fault: at 4,000 bytes it failed 7,404 allocations and 8.92% of the web's reads were missed in a soak; at 16,384, none and 0 of 3,774. Its Pico 2 W run is left, before the ADR closes. What was built and measured is in [progress.md](progress.md). **Left:** gates 2 to 4, a first view; each is written in full, the API's table with gate 2, and approved before it is built.

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

### Gate 2: our own server, tried (first view)

- One task, a few connections at once, a table of method, path and handler. Handlers run in the server's task, not in lwIP's: a slow one, such as the EEPROM's erase, holds the web, not the network.
- A request's body is read in pieces and handed to its handler; a reply is written in pieces.
- Sockets with `select`, or lwIP's netconn: chosen in gate 2's text, on the trial's flash, RAM and soak.
- The trial image serves the paths the soak reads, in the new API's form, with the old server off.
- From gate 2 to gate 4, in this ADR's branch, the new server also serves the old `/rest/...` handlers through a thin adapter, so the portal works while its pages move one module at a time. The adapter, the old handlers and `http_rest.c` are deleted together in gate 4. Not a second server on another port: that would cost lwIP's memory twice. (Agreed by the owner 2026-11-15.)
- It passes when the soak is within gate 1's limits on the Pico W, with flash and RAM given against gate 1's image.

### Gate 3: the API, each setting described once (first view)

- Paths under `/api/v1/`, bodies in JSON. A read is a GET and changes nothing. A write sends a JSON body and is refused unless it is sent as `application/json`: the guard against other sites, above.
- A wrong value is refused with 400: a code, the field, its limits and a sentence. Nothing in that request is written.
- One table a module: for each setting a name, a type, a unit, its limits, whether it can be written, a line on what it is, and where its value lives. Reads, writes, the checks and the unit's own description all come from it. A write goes to the module that owns the setting, not into its struct from the server's task.
- No stored struct changes its layout: the tables point into them.
- The portal's settings forms and `rc_run.py settings` move to it, module by module.

### Gate 4: commands, events, the switch (first view)

- Start a charge at a weight, stop it, start and stop cleanup, set its speed: requests to the application, which the web, the console and the menu all use, in place of made-up button presses. While a charge runs, a write that the charge reads is refused with 409 and the state.
- A charge and a cleanup are each a resource with a state. Their events are numbered, and a client reads on from where it left off (`?after=N`), so no client takes another's.
- `GET /api/v1` describes the unit: the API's version, the firmware's, the board, what it carries, and every resource with its fields, from the tables. The mDNS announcement adds the API's and the firmware's versions.
- The portal, the wizard and the mirror on the new API. The old server, `http_rest.c`, the old handlers and lwIP's httpd options deleted.
- The scripted client is a Python module in `tools/`, driven by the unit's description, not by a list of paths written into it. The proof uses it, `rc_run.py` uses it, and an agent can. `tests/web_test.py` answers the new API.

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
- A push channel for live data (server-sent events or a WebSocket). Polling stays; ADR-008 or ADR-009 may ask for more.
- The console over Wi-Fi (the owner, 2026-11-15).
- What the modes do inside (ADR-009).

## To settle before gate 2 is approved

- The API's table: every path, method, body, reply and status, with the names.
- Sockets or netconn: the trial's design and what decides it.
- A write: one setting at a time, a module's at once, or either.
- Which writes are refused while a charge or a cleanup runs.
