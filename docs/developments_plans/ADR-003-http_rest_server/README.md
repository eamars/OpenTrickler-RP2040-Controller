# ADR-003: a real HTTP REST server

Status: **Draft** 2026-11-13. Not approved. This captures the problem and the ideas; the design is brought to the owner as choices before the ADR is approved.

## What we want

- A method, a path and a body in; a status and a body out.
- A read never changes anything.
- A wrong value is refused, with a reason.
- Handlers know nothing of lwIP.
- One server for every board.

## Owner's direction (2026-11-13)

- The HTTP stack was meant to be a REST server and was not built in an elegant way. That part needs a rewrite.
- One implementation: a feature is never built a second way for another board.

## Why here

OTA's upload (ADR-004), the web UI (ADR-008) and the tuning pages (ADR-009) all stand on the server. Built on today's, each would be built twice.

## What we know (checked 2026-11-13)

**How it is built**
- `http_rest.c` is a copy of lwIP's file server (`httpd.c`, 2,770 lines in the SDK) with about 150 lines added: a linked list of paths and handlers, looked up where the file used to be opened.
- A handler has the signature `bool handler(struct fs_file *file, int num_params, char *params[], char *values[])`. It answers by pointing the file at a static buffer that holds the whole response, status line and headers included.
- `http_rest.h` is included by the headers of charge mode, motors, the scale, profiles, the EEPROM, the display and more, so lwIP's headers reach all of them.
- 22 paths are registered in `rest_endpoints.c`. Pages are C strings generated from `src/html/` by `scripts/html2header.py`.

**What follows from that**
- There is no method and no body. POST is off in `lwipopts.h`, and a request line is 128 characters at most.
- One GET both reads and writes. `GET /rest/charge_mode_state?s2=1` starts a charge. `GET /rest/system_control?s6=true` erases the EEPROM. A browser that prefetches a link, or a second tab, can do either.
- A value goes straight into the live settings with `strtof` or `atoi` and no check of its range. It is written from the network task while the charge task reads the same struct.
- Fields are named by codes (`c5`, `s0`, `m3`). Each setting is written out by hand three times in its module: a comment that maps the code to its name, a chain of `strcmp`, and a `snprintf` format. It is written again in the web page and in the screen menu.
- JSON is made with `snprintf` into fixed buffers of 64 to 256 bytes. Nothing parses JSON.
- Reading `/rest/charge_mode_state` clears the charge events, so two browsers take them from each other.
- The web starts a charge by putting a made-up button press (`OVERRIDE_FROM_REST`) into the menu's queue and setting a global.
- There is no password. Anyone on the network can read and change everything, and start the motors.

**What both platforms have**
- lwIP's socket API is on in this build (`LWIP_SOCKETS 1`, with FreeRTOS). ESP-IDF has the same API.

## Ideas

**The server.** Three ways, to be weighed with numbers:

| Way | For | Against |
|---|---|---|
| lwIP's own server from the SDK, not copied, through its hooks for POST and custom files | The least code of our own | It stays a file server: handlers still dress up as files. Whether ESP-IDF can build it is to be checked |
| A small server of our own on sockets: one task, a few connections, a table of method, path and handler, a body read in pieces | The same code on every board; methods, bodies and uploads are plain | We own it: perhaps 600 to 900 lines, and the corner cases of HTTP |
| A library | Less to own | Each has a catch: ESP-IDF's server is ESP only; Mongoose is GPL-2.0 or commercial, to be checked against this project's licence; CivetWeb is large |

First view: our own small server on sockets, perhaps with a small parser library inside. It is the only one that is one implementation on both platforms and handles an upload naturally. A trial on the unit (flash, RAM, several browser connections at once) comes before the decision.

**Each setting described once.** One table per module: a name, a type, limits, and where the value lives. From it:
- a read gives every setting under its real name;
- a write is checked against the limits and refused with the reason;
- later, the web form (ADR-008) and a settings screen (ADR-010) are drawn from the same table.

This replaces the three hand-written lists per module. It is the largest idea here and the owner should weigh it on its own.

**Writes go through one door.** A write is handed to the module that owns the setting, not poked into its struct from the network task.

**Commands, not made-up button presses.** "Start a charge" and "stop" are requests to the application, which the web, the console (ADR-001) and the menu all use.

**Events each client can read.** A counter or a short log, read from where that client left off, in place of clear-on-read.

**The body's format.** JSON needs a small parser (a library, or our own for the flat objects we use). A form-encoded body needs none. To be chosen with the server.

## How the unit shows it works

- A scripted client on the PC calls every endpoint and checks the answer against a written list.
- A GET changes nothing: the client reads everything twice and the unit's state is the same.
- A value outside its limits is refused with a status and a reason, and the setting is unchanged.
- Two browsers at once each see every charge event.
- The web portal works as before; it is moved to the new API in the same ADR, because a server nothing uses is not proven.
- `charge_cycle` passes with a browser polling throughout.
- The size report shows what the server costs against today's.

## Not in this ADR

- New pages or a new look (ADR-008).
- TLS.
- A password or any other guard on the network. It is listed above as a fact; whether to add one is the owner's call and its own ADR.
- A push channel for live data (server-sent events or a WebSocket). Polling stays; ADR-008 or ADR-009 may ask for more.

## To settle before approval

- **Whether today's paths and codes must keep working.** The unit announces `_opentrickler._tcp` by mDNS, which suggests something other than the built-in pages may call it. If so, the old paths stay for a time beside the new ones; if not, they go.
- **The server:** which of the three ways, after the trial.
- **Each setting described once:** in this ADR, or left as it is.
