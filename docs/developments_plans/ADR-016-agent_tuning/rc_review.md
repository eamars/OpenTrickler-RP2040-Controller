# ADR-016: the release candidate's review

An independent final check of `adr016_gate1` at `e34116e6` (v2.0.3-418) against `origin/main` (`7b64c05`), 2026-11-29, before ADR-016 is closed. Read by one session, with no subagents. No unit was touched: nothing here was seen on hardware. No source was changed.

## Verdict

**Ready for the release candidate.** Nothing found must be fixed before it.

- No write gets past the PIN's guard, the stored records keep the EEPROM's rules, the layers hold, and both boards build within their room.
- Two small faults in the throw record are worth their one-line fixes before the tag (findings 1 and 2). Neither changes what a throw does; each can put a wrong figure in a record.
- The reset of a held read at a soak's start is **acceptable, and is not what the progress says it is** (finding 3): it is lwIP's accept mailbox, 8 long, refusing the ninth connection of nine that arrive together. It comes from ADR-003's settings, not from the throws, the six slots or the two held reads.
- Three statements in `progress.md` are not true as written (finding 6). They should be put right before the close.
- The Pico 2 W has not run this branch. Its build passes here; a flash, a tour and a charge cycle on it before the tag are worth their few minutes (finding 7).

## What was done

| Check | Result |
|---|---|
| `git fetch`; `origin/main` is `7b64c05`, the branch 12 commits on it | as the task said |
| `git diff origin/main...adr016_gate1`, every source file of it read whole or in its hunks with the code round them | findings below |
| Pico W build, clean tree, into a folder of this review's own | flash 951.0 KB of 1,016 KB, RAM at link time 253.0 KB of 256 KB |
| Pico 2 W build, the same | flash 937.1 KB of 1,792 KB, RAM 285.9 KB of 512 KB |
| `tests/release_records.py --elf` on both images | `VERDICT PASS`; the Wi-Fi record v1.21's 108 bytes, 116 now, its fields where they lay, `setup_done` in its known filler |
| `tools/module_graph.py` | `VERDICT PASS` |
| `tests/host_test.py` | `VERDICT PASS` |
| Our own code's compiler warnings | six, all in lines this branch did not write (`charge_mode.c:36`, `menu_nav.c` 1702, 2981, 3225, 3228, 3321) |

**How far the reading went.** ADR-016's own change was read closely, with what it calls into: the charge's states and its pour loop, the server from a request's first byte to its reply, the settings' storage (load, save, the trial's held writes), the controller's stop. The rest of the firmware was judged by the builds and the three PC checks above, not read again line by line. Not read at all: the auto-tuner and how the charge drives it (the owner's rule). The tools (`rc_run.py`, `unit_api.py`) were read only where a finding needed them.

## Findings

Weights: **must** (before the release candidate), **should**, **note**. Sure: **read** (confirmed by reading the code), **built** (confirmed by a build or a check), **likely** (read, and fits what a log shows, not proven on a unit).

### 1. A throw that never poured can carry the last pour's time (should; read)

`src/services/charge/charge_mode.c:109` (`charge_mode_record_elapsed`), with `:709` to `:714` and `:741` to `:744`.

- **What fails.** `charge_timer_running` is set at a pour's start (`:616`) and cleared at its normal end (`:819`) and at a fault (`:581`). A pour stopped by a stop (`:709`) or in a pause (`:741`) returns with it still true, and nothing clears it after.
- **How.** Stop a charge while it pours. Start another, and stop it, or let it meet a fault, while it zeroes. That throw's record is closed through `charge_mode_throw_ended` (`:132`), which fills `elapsed_s` because the flag is still true: the seconds since the *earlier* throw's pour began. The record reads `result: none`, `gains: null`, and an `elapsed_s` of minutes.
- **Against the claim.** api_v1.md: "A figure the throw did not reach is `null`".
- **Its weight.** An agent does not judge a throw that ended short, so no tuning goes wrong by it. It is a false figure in a new feature, and its cure is the flag cleared where a pour ends, however it ends.
- **Not ADR-016's, the same flag:** on main too, a fault while zeroing after a stopped pour shows that stale time on the screen (`:578` to `:581`).
- **Not covered by a check:** `tuning_check` stops its last throw in the pour and then restarts; the soak stops each charge's next throw while it zeroes, after a pour that finished.

### 2. The throws' room is handed to readers before it is cleared (should; read)

`src/services/charge/charge_throws.c:29` to `:32`, and `:95` to `:100`.

- **What fails.** `charge_throws_ready` stores the pointer, then clears the room. FreeRTOS runs on both cores here (`configNUMBER_OF_CORES 2`), so the server's task can read `newest`, `count` and a record in between, from memory not yet cleared.
- **How.** A `GET /api/v1/charge/throws` handled in the few microseconds of the first charge's start after a start of the unit. Its reply's numbers and records would be whatever the heap held. Once at each start, and a very small chance.
- **The same kind, smaller:** `charge_throws_first` reads `newest` and `count` outside the critical section `charge_throws_close` writes them in (`:78` to `:86`). Read between the two writes, during the first 32 throws, `first` is one too high for that one reply, and a client reading from 0 is not given the oldest record though it is kept.
- **Its cure:** clear, then store the pointer; read the two numbers under the critical section.

### 3. The open question: a held read reset at a soak's start (note; likely). Acceptable

`src/boards/common/settings/lwipopts.h:41` (`DEFAULT_ACCEPTMBOX_SIZE 8`); `library/pico-sdk/lib/lwip/src/api/api_msg.c:579` and `src/core/tcp_in.c:958`; nothing of `charge_throws.c` or of the log's reader.

- **What happens.** lwIP finishes each new connection's handshake in its own task (priority 7) and posts it into the listener's mailbox, 8 long, for the server's task (priority 3) to accept, one each pass. A ninth that arrives before the server has accepted any does not fit: lwIP's `accept_function` returns `ERR_MEM` and `tcp_process` aborts the connection, which sends a reset. lwIP says nothing, and it is no pool's failure, so `net` counts nothing.
- **Why it fits.**
  - The soak starts nine clients at once, each opening a connection of its own (`urllib`); nine is one more than eight.
  - The throws' follower is the thread started last (`rc_run.py:3249`), so its connection is the ninth: it was the one reset in both soaks.
  - In the second soak the follower's retry, 14 ms later, was reset too, at 00:06:14.176; the console was answering `stats` from 14.148 to 14.180 (`logs/rc/20261129_000547_soak.log`), and the next try went through.
  - Neither soak's log has the server's "every slot held" line, so the six slots were never all held; `tcp_pcb_err` is 0, so no connection was refused for want of one.
- **What it is not.**
  - Not the slots or the two held reads. A held read past `HTTP_SERVER_PARK_MAX` is answered at once (`http_server.c:984`), never reset.
  - Not lwIP taking back an idle connection: this lwIP only ever kills one of a lower priority (`tcp.c:1736`).
  - Not the server closing a kept-alive connection for room: these clients send `Connection: close`.
- **The judgement: acceptable for the beta.** It needs nine new connections inside one gap of the server's task, which a person's browsers and one agent do not make. The client reads again, and both logs are read from where it left off, so nothing is lost. It was there on main.
- **If the owner wants it gone:** `DEFAULT_ACCEPTMBOX_SIZE` at `MEMP_NUM_TCP_PCB`, 12 (16 bytes more of the heap). Then lwIP's 12 connections are the bound: one too many is a SYN left unanswered, which the client's own TCP sends again a second later, with no reset. An ADR-003 setting, so the owner's word first; one soak's start would show it.
- **Not proven:** this is read from lwIP's code and the two logs. No unit was run for it.

### 4. A third held read is answered at once, and the soak's throws follower then asks without a pause (note; read)

`src/web/http_server/http_server.c:983` to `:994`; `tools/remote_control/rc_run.py:400` to `:410`.

- **What happens.** The soak's two event followers hold the two reads the server keeps. The throws' follower's read with `wait` is therefore answered at once with nothing, and its loop asks again at once: for most of a soak it is a client asking as fast as the unit answers (reckoned at some 15 a second, from the pollers' 0.06 to 0.08 s a read), not a held read. Read in the code; no log counts its reads.
- **What that means for the proof.** The soak with `--throws` shows that no throw is missed under that load. A held throws read answered as its throw is recorded is shown by `tuning_check` alone, with one client. Both hold; the progress's "following the throws by held reads" says more than the soak did.
- **For a user.** api_v1.md says it under ADR-003's gate 2 ("a third request with `wait` is answered at once"); the throws' row and the guide say only "held until the next one is recorded". An AI agent asks slowly, so it does no harm. A line in the guide would cover a script: a held read answered with nothing new waits a second before it asks again.

### 5. The guide's coarse-tube row against the record (note; read)

`src/web/api/agent_guide.md:20`; `src/services/charge/charge_mode.c:776` to `:795`; `src/services/charge_control/charge_controller.c:477`.

- The guide: "Over, with `fine_s` near 0: the coarse tube poured past the target."
- When one reading takes the weight from under the coarse tube's stop to within the fine threshold of the target, the controller ends the throw before it marks the coarse tube stopped. The record then has `coarse_s`, `coarse_stop` and `fine_s` all `null`, not near 0.
- The usual case, the coarse tube stopped and its powder still landing, reads as the guide says. The guide could say "near 0, or null".

### 6. Three statements in `progress.md` that are not true as written (should, before the close; read)

- **Line 98, "Not run: a factory reset clearing the PIN".** The session that built it says it was run and passed. Whichever is true, the progress should say it.
- **Line 90, "about 90 bytes static, the six server slots' cookie buffers (72)".** The slots come from the FreeRTOS heap (`http_server.c:1880`): the 72 bytes are heap, and no exception to "nothing static". What is static is the lockout's three variables, the guard's pointer, the throws' pointer and the lag's four.
- **Line 115, the reset's cause** ("the server has six slots and holds two reads at most"): finding 3.

### 7. The Pico 2 W has not run this branch (should, before the tag; built)

- Its image builds here and `release_records.py --elf` passes on it. By AGENTS.md's rule the application layer, memory and the network are proven on the Pico W.
- Gate 3 also stores a record on a second controller board's EEPROM, and this is a release candidate for both boards. A flash, a menu tour saving `settings/clear_pin` into its baseline (it fails as a new page until then), and a charge cycle would close it.

### 8. Smaller notes (read)

- **The guard stands on every write's path beginning `/api/`** (`http_server.c:873`). Every route that writes does today, the two uploads included, and a route is matched exactly and by case (`:306`), after the path's `%xx` are decoded (`:1037`), so no spelling of a path gets round it. A write route added outside `/api/` would have no guard and no check of its media type. Nothing asserts it.
- **The lockout is everyone's.** Five wrong PINs from any client refuse every client's writes for a minute, a stop too, and a client that keeps sending them keeps it so. By design: a stop is at the unit's button, and the PIN is not security.
- **A PIN set during an update's trial** (`wireless.c:843`): `config_read_stored` reads the EEPROM, not the trial's held writes, so a Wi-Fi setting stored earlier in the same trial is put back to what the chip holds. A window of seconds.
- **The lockout's clock** is 32 bits of milliseconds, compared by subtraction (`api_v1_commands.c:1822`): right across its wrap at 49 days.
- **The throw's number** is 16 bits (`charge_mode.c:86`): past 65,535 throws in one start, about two weeks without a pause, the records begin again from 0. Not a real case.
- **A PIN longer than the action's buffer** (ten digits or more) is refused as `wrong_type`, not `out_of_range` (`api_v1_commands.c:1863`).

## Looked for, and found sound

- **Writes past the guard:** POST, PUT, PATCH and DELETE under `/api/` all pass `slot_admit`, an upload's at its headers (`http_server.c:1149`) and the rest before their route (`:915`). A HEAD is a GET; OPTIONS and any other method are refused 405 before it (`:1183`). The PIN's own action needs the PIN once one is set.
- **The cookie:** `pin=` found only at the start of a cookie, the first one taken, cut at 11 characters so a longer value is wrong, and cleared with each request so a kept-alive connection never carries it on (`:388`, `:1083`). A line past 256 bytes is passed over and reads as no PIN, as api_v1.md says.
- **A record read half-written:** a reader copies a closed record whole under the critical section the close writes it in (`charge_throws.c:78`, `:107`). The open record is the charge's task's alone.
- **The held reads and the numbering:** the record is closed before its event (`charge_mode.c:924`, `:133`), so a client the event wakes finds it. A read after a number past the newest, as after a restart, is answered at once (`api_v1_commands.c:960`). A record gone while a reply is made is passed over and the client sees the gap (`:884`).
- **The EEPROM:** the PIN's two fields are appended past the release's 108 bytes, of fixed widths, outside its tail padding; a v1.21 or a v2.0.x record is read at 108 and stored at 116, the PIN off. An earlier version reading the longer record drops the two fields. The PIN is in no settings table, so in no reply, export or copy.
- **`saved` after a PIN is set:** the store goes through `save_config`, which keeps the CRC `saved` is judged by (`common.c:803`), so the Wi-Fi's settings do not read as unsaved.
- **The schema:** its fourth part closes as the third did (`settings.c:1416`); each item of the throw's table is far short of a chunk, and so is a record, by its fields (not measured).
- **The portal's image upload** is sent outside `api()`, but only after a `POST /api/v1/update/start` that asks for the PIN (`update.js:434`), so the cookie is there when it goes.
- **The guide and api_v1.md against the code:** every path, field name, limit and refusal the guide names is in the code as it says (`wait` 1 to 30 and 400 past it, the hello's 409, `busy` in a throw, `driver` in `/api/v1/scale`, the five settings' names, the three PIN refusals). Not checked, by the owner's rule: what the guide says of the auto-tuner, which repeats the README's "What we know".
- **Memory:** nothing new of size is static; the records' 2,912 bytes and the lag's 400 come from the heap once and are kept. RAM at link time 253.0 KB of 256 KB on the Pico W, as measured.
