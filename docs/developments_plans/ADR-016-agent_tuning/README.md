# ADR-016: an AI agent helps tune a powder's profile, over the API

Status: **Decided** 2026-11-28: approved as written by the owner ("Approve as written"), the evening it was written in full from the owner's answers of that day. **Re-gated later that evening at the owner's word** ("Yes, rewrite it"): the API's three gates as approved, in their order, and the auto-tuner no gate and not touched (below, "The auto-tuner is not touched"); the rewritten text approved by him ("Approve, merge it"). First drafted 2026-11-27 with the use case as the owner and the agent traced it, and PR #113's features folded in as the owner asked. Nothing is built.

## What we want

A person at the unit, loading real rounds, gets each powder thrown well. The unit's own auto-tuner does it first, with no AI. When it cannot, the person gives an AI agent on the same network the unit's address and asks it to help: the person puts the cup on and empties it, the agent watches the throws, changes settings, and says what it did and why. The agent has the unit and its API and nothing else: no probe, no console, no source. Everything it needs to know, the unit tells it.

## Owner's direction

2026-11-27:
- On starting from the use case: "We need to trace back to the use case and requirements first, before writing anything down."
- On who the AI is: "I'd intent targeting at real AI agent users, not copy and paste channel."
- On writing: "I'd allow AI agents to write. I have the passcode/pin idea in mind, to stop people messing with the unit intentionally, or unintentionally for a unit shared with multiple users."
- On an agent starting something that pours: "No need. The user need to present to make sure the cup is properly placed, but I don't want you to eliminate the current capability. The scope is to extend, not remove."
- On what it is for: "Most questions to me are why my scale doesn't talk to the board, and in most cases are the setup issue. I don't think AI can help. The integration of AI is to help people tuning for the powder profile."
- On a scale that does not talk: "The common failure modes are 'The scale doesn't talk'. So AI can help, but only a little, as from OpenTrickler perspective nothing is received."
- On where this is built: "I don't think we need to include everything in ADR-009. IN fact I will merge ADR-009 now, and the API extension is to be added as separate ADR."
- On the API's freedom: "no v2.0 is released, so any API change is permitted."
- On PR #113: "Then perhaps fold PR#113 features, adjusted for our code base into ADR-016? And keep a note for PR#113 and leave it open for now, until ADR-016 is fully closed?", then "Go ahead". Read here as: #113's features are judged against this ADR's use case and taken where they serve it, in our code's form, with its author as co-author of the commits that use the work; the PR stays open, with a note, until this ADR is closed.
- Earlier, of the API (2026-11-15, in `ADR-003-http_rest_server/api_v1.md`): "does the API capable of supporting the ADR-009 and the future AI agent assisted development and tuning needs?", and "tuning will need some access to huge amount of data".

2026-11-28:
- **Self-contained:** "The agent instruction should be self explained and self contained, i.e,. the agent should be able to know how, and what to do when the user provides the IP address."
- **Two roles:** "There are two roles: Development agent and assistant agent. You are the development agent in this role, and the assistant agent role will be the one ADR-016 talks about. The development agent will have access to all source code and auto tuner, while the assistant agent will by exposed with the unit and its API first. And call hello on first connection."
- **Room:** "The RAM and FLASH can be a constraint. I need you to plan ahead wisely."
- **The auto-tuner first:** "If you found any issue with auto tuner, or the auto tuner should be improved during the development, you shall prioritize on fixing the auto tuner. The AI assistant agent will be the last resort when the code is deployed to the users."
- **The tuner across sessions:** "It was discard by design. The setup property drifts across sessions, that you should read previous session records in ADR. I do not intent to keep auto tuner across session, but 50 might be too much." Asked for a goal, the owner chose "Within 25 throws", the option recommended to him; set aside by the re-gating (below).
- **The PIN:** "Read only. And the PIN should be carried by cookie?" Read here as: without the PIN, the network reads and nothing else, a stop and a hello included. The cookie was checked and taken: a browser sends it by itself, an agent sends it as a header, so it is one way for both (below).
- **What the assistant may do:** "It should prompt the assistant agent to ask for permission. But once the permission is granted the agent can do anything. Note, this should be encouraged by the agent, not to be stopped by the mechanism." Read here as: the guide tells the assistant to ask the person once; nothing in the firmware checks it.
- **Its place:** next, before ADR-007.
- **The rehearsal:** "Before the powder test, I'd like you to spawn a haiku subagent that navigate through the unit mimicking an average agent helping the power tuning, without the context. Then you shall be responsible to monitor the haiku behavior, then tune the prompt/instructions accordingly." (gate 2, below).
- **Re-gated,** that evening, after the first gate 1 had taken some two hours of the auto-tuner's simulation and no line of the API was written: "The ADR-016 is supposed to be a API heavy change, for which allowing external AI agent to help with tuning. But it looks like the execution agent turned this into another optimization nightmare." Asked whether to rewrite it with the API's gates first and the tuner no longer a gate, or to keep 25 throws as a gate: "Yes, rewrite it". Read here as: the owner's word on the auto-tuner (above) was no order to go and improve the tuner, and "50 might be too much" was a remark, not a requirement.
- **The tuner is not touched** (a requirement; it narrows "the auto-tuner first", above): "this ADR is never a tuning session and will never be. The agent may propose something new but no tuner should be touched unless a better algorithm is determined. I spent a long time deriving the current tuner, and any ad-hoc research recommendation to the tuner is likely caused by the drift of the setup. I'm over it, and unless there is justification to support, I will not allow the agent trying to improve the tuner, at all."

## Two roles

- **The development agent** builds the firmware: it has the source, `AGENTS.md`, the probe and the console, the twin and the auto-tuner's code. It builds this ADR.
- **The assistant agent** is this ADR's user: an agent a person points at a unit by its address, with the unit and its API first. It learns from the unit alone what it is, what it may do and how to help, and says hello when it first connects.

The assistant is the last resort, once the firmware is with the users: the unit's auto-tuner comes first. The development agent does not change the auto-tuner; what it finds there it proposes to the owner ("The auto-tuner is not touched", below).

## The use cases, as traced (2026-11-27)

What a person might ask an AI to do with the unit, and where each stands. Only the first is this ADR's.

| What they say | This ADR | Today |
|---|---|---|
| "Tune this powder for me" (the agent changes settings over many throws, the person at the cup) | **Yes: the one it is for**, after the auto-tuner | A gain tried is a PATCH, live for the next real charge; a throw tells only its result, final weight and whole time |
| "Why does it keep throwing over, or so slowly?" (asked while tuning) | Yes, as part of the first | The same gap: nothing says where in the throw the miss came from |
| "Log this session and give me its mean and spread" | Falls out of the first, not a goal | Works for an agent that follows the event stream from the start |
| "I changed powder, set it up" | No | The calibration and the settings already say it |
| "Is my unit healthy, is there an update?" | No | Works |
| "Throw a ladder of weights" | No: ADR-014 and issue #106 | The loads (ADR-014) |
| "Why doesn't my scale talk?" | No (the owner: only a little); the guide says what a person can check | The unit shows no reading |
| A person pastes the unit's data into a chat AI | No (the owner: real agents, not copy and paste) | |
| "What gains do others use for this powder?" (issue #10) | No: outside the unit | |

## PR #113, folded in

PR #113 ("Add powder analysis", by DevDaniel1408, open since 2026-10-26) tunes a powder's profile in a session of its own, on the 12864 and a web page. Read in its developer notes and its tuner's header (`git show pr113:manuals/powder_analysis_developer.md`, `pr113:src/powder_analysis_tuner.h`):

- **Its tuner is driven as a loop:** the next trial's settings, a throw on them, the throw's result, until done. Its logic has nothing of the hardware in it and is tested on a PC against a simulated powder.
- **What a throw reports to it:** the settled weight, the time to the coarse stop, the whole time, whether it timed out, and the weight of the reading that stopped the throw; from the last two, the fall after the stop.
- **Its procedure:** the smallest coarse Kp that passes the coarse stop within 7.7 s; the coarse Kd by doubling from 0.1; the smallest fine Kp whose full throw ends within 13 s; then rounds of 10 throws, 9 hits verifying, 2 overs giving the round up and lowering the fine Kp by a tenth or the fine minimum speed by a quarter, three times at most. Start values by the powder's shape and grain size.
- **Its threshold recommendation:** from the median fall, a fine threshold half a scale step off a multiple of the step, so a float's rounding never decides the stop.
- **Its cost:** about 70 KB of flash and 27 KB of RAM by its own figures, more than the Pico W has left (below).

| Of PR #113 | In this ADR |
|---|---|
| What a throw reports | Gate 1's throw record, its figures and more |
| The procedure, its start values, the threshold recommendation | Gate 2's guide: what the assistant follows for what the auto-tuner does not move. No firmware |
| A trial on settings that are not the stored profile; a coarse-only throw | Not taken: a setting tried between real throws, and the record gives the coarse tube's time and overshoot from a full throw |
| The session on the 12864, its web page with a chart and CSV, its manuals | Not ported: the Pico W's room, and a tuning session on the unit is what ADR-009 chose not to have |
| Its tests on a PC against a simulated powder | Ours exist (`tests/step2_sim.py`, the twin) |

**Its end:** PR #113 stays open with a note, and is closed with a note naming what was taken when this ADR is closed, each at the owner's word. The commits that take its figures and its procedure name its author, Daniel Horbach, as co-author.

## Why here

The owner placed it next, before ADR-007 (2026-11-28): it is the current Pico platform's, PR #113 has waited since October, and the API is free to change until v2.0 is released.

## What we know (read in the code and the records, 2026-11-27 and 28)

- **The room on the Pico W** (`size_report.py` on main's build at `46cd9c5`): flash 934.8 KB of the slot's 1,016 KB, 81 KB free; RAM at link time 252.9 KB of 256 KB, so nothing new can be static; the FreeRTOS heap 51 to 54 KB free when quiet and 41.2 KB at its lowest since the start in a session with the web in use (`stats`, 2026-11-28). A charge borrows the throw's room, some 28 KB, from the heap while the forecast or the tuner is on. The Pico 2 W has some 76 KB of the heap free.
- **The auto-tuner** (ADR-009, `charge_tuner.c`; off by default, the owner's): it judges windows of 25 throws against the kept gains' latest 75. Its first window's gains are the profile's, kept as they are; the next window is a trial, stored only if it is better. So nothing is stored before about 50 throws in one sitting, at one target. Everything is in RAM, and it begins afresh at a start of the unit, at a change of powder or of target (a load chosen is a target), and when the profile's gains are changed elsewhere. That is by design: the rig drifts by about 4 good throws of 20 and 1 s within an hour (ADR-009, "Traps on record"). 25 was chosen over 20, which was too noisy on the twin; at a tolerance of 0.03 gr a window of 25 holds 6 to 12 good throws, ±30% of noise. It moves the fine kp and kd, and brings the coarse kp down when it is far too high; it never moves the thresholds, the speeds, Ki, or the coarse kp up. Its trial gains are in RAM and in no reply.
- **What an agent can do today:** read and write every charge, powder, scale and motor setting, each with its limits and a line on what it is (`GET /api/v1/schema`); choose a load and set its target; start and stop a charge, a purge and a calibration; follow the event log, by held reads or as a stream; read the live weight (the stream's `state`, at most every 100 ms); say hello (`POST /api/v1/agent/hello`, refused while a charge runs). The index (`GET /api/v1`) has a guide in plain words: 1,463 bytes of the 1,509 its chunk holds (a static assert in `api_v1.c`), so there is no room in it for more.
- **What a throw tells:** `throw finished` carries its number, its result (good, under, over), its final weight and `elapsed_s`, the whole throw's time. The charge mode knows the coarse tube's stop and the fine tube's part and gives them to the console only.
- **What a throw does not tell:** when and at what weight the coarse tube stopped; the weight at the fine stop and what landed after it; the pauses; the gains it was thrown on. So an over from the coarse tube's powder still landing cannot be told from one of a fine tube too fast.
- **What is kept:** the newest 64 events of every kind, about 30 throws at most.
- **A value tried is a PATCH:** live at once and not stored; each settings resource says whether it is `saved` and has a revert; a restart puts back what is stored. A write to the charge's settings, or to the powder in use, is refused while a throw is under way, from its zeroing to its verdict: a write fits between a verdict and the cup's return.
- **No PIN:** anyone on the network reads and writes. api_v1.md kept the place for one.
- **An agent given an address** most likely reads `http://<address>/` first: the portal's page, whose head names the index and the schema (`rel="service-doc"`, `rel="service-desc"`) and nothing more for an agent.
- **The simulated scale** has no lag and no fall after the stop: its powder stops the moment the motors do and snaps to the target. It proves what a tool or an agent does with the API, never a tuning.

## What we do

Three gates, in this order, each approved and built on its own; one may be merged by a PR of its own. Every figure an assistant reads comes in gate 1, its guide in gate 2, the PIN in gate 3. The auto-tuner is no gate and is not touched.

### The budget

ADR-016 takes at most **12 KB of flash and 4 KB of RAM** on the Pico W, nothing of it static: what it keeps is taken from the heap at its first use and kept, as the charge's controller is. Each gate measures its own (`size_report.py`; `stats`'s `heap_min` and `charge_heap_low` across a charge) and says it in the progress. A gate that would pass the budget stops and comes back to the owner before it is built further.

### The auto-tuner is not touched

A requirement of the owner's (2026-11-28, above). For whoever builds this ADR:
- **No change to the tuner:** `charge_tuner.c`, its twin `step2_tuner.py`, and how the charge mode drives it and stores what it keeps. No gate of this ADR changes them.
- **No study of the tuner,** on the twin or on a unit. Building this ADR is never a tuning session.
- **An agent may propose.** A proposal is a note in the progress, with its justification, put to the owner; nothing is built on it before his word. Figures from a few sessions on the twin or the bench are not a justification by themselves: the rig drifts by about 4 good throws of 20 and 1 s within an hour (ADR-009, "Traps on record"), and the owner derived the tuner against that.
- **One proposal on record, not taken: kept gains that are not stored.** The unit stores the tuner's kept gains only when a window closes with the step `kept` (`charge_step2_at_rest`, `charge_mode.c`). A keep whose next trial cannot move the gains closes as another step, so those gains reach no profile. Read in the code on main; in the twin's sessions already on disk, 11 of the tuner's 545 keeps closed that way (`logs/adr016/early_study_today.json`). It changes nothing the tuner decides: the profile would hold what the tuner kept. The fix as written is a commit of its own (`6685961`, on `adr016_tuner_early`), never run on a unit.
- **Not this ADR's: a window closed early.** This ADR's first gate 1, an ad-hoc change of the kind the owner rules out. Kept, not judged, on the local branch `adr016_tuner_early` (`ba847b9`); nothing in this ADR waits on it, and it is deleted at the owner's word.

### Gate 1: the throw record

Every throw, finished or ended short, leaves one record. The newest 32 are kept in RAM, about 85 bytes each (2.7 KB, from the heap at the first charge), until the unit restarts, numbered as the throws are (from 1 at each start).

`GET /api/v1/charge/throws?after=N&wait=s`: the records after throw N, oldest first, with `first` (the oldest kept) and `newest`, read as the event log is: a client that comes late or loses its connection reads back what is kept and sees what it missed. `?wait` holds the request until a throw after N is recorded. The `throw finished` event is unchanged: its number names the record.

| Field | What it is |
|---|---|
| `throw`, `at_ms` | its number, and when its verdict was read |
| `simulated` | thrown with the simulated scale: never tuned on |
| `charge_profile`, `powder_profile`, `target` | the load, the powder and the weight it was thrown at |
| `result`, `reason`, `weight` | good, under or over (none when it ended short), why it ended, the verdict's weight |
| `elapsed_s`, `coarse_s`, `fine_s` | the whole time, the time to the coarse tube's stop, and the fine tube's part from there |
| `coarse_stop`, `fine_stop` | at each stop, the reading and that tube's speed |
| `fall` | the verdict's weight less the reading at the fine stop: what landed after it (PR #113's figure) |
| `pauses` | the pauses for want of readings (ADR-013) |
| `gains` | the coarse and fine kp, ki, kd it was thrown on, and `by`: the profile's, or the auto-tuner's trial |
| `coarse_threshold`, `fine_threshold`, `forecast`, `auto_tune`, `backoff` | the rest it was thrown on |

An agent then tells an over from the coarse tube (the fine tube's part near nothing) from one of the fine tube (a large fall, a high speed at the fine stop), and sees what the auto-tuner is trying. The figures a throw does not reach are `null`.

### Gate 2: the assistant's guide, served by the unit

- **Found from the address alone:**
  - the portal's page names it in its head (a link of `rel="help"`) and in a line a reader without scripts sees;
  - `GET /llms.txt`, where some agents look, serves it too;
  - the index names it in a member of its own, as its guide has no room left.
- **`GET /api/v1/agent`:** plain text in Markdown, not compressed so any client reads it, at most 6 KB of flash. It says, for an agent that has nothing but the address:
  - **who it is for, and the first steps:** you are helping a person who is at this unit; say hello (`POST /api/v1/agent/hello`) when you first connect, or as soon as a charge is not running; if a PIN is set (the index's `pin`), ask the person for it first;
  - **permission:** ask the person once before changing anything; once they say yes, change and store whatever their goal needs, and tell them what you did and why;
  - **the auto-tuner first:** is it on for this powder; if not, offer it before tuning by hand; read what it tried in the records;
  - **reading a throw:** which figure says which miss, from the record;
  - **what to move, for which miss, by how much, and when to stop:** for what the auto-tuner does not move (the thresholds, the fine tube's lowest speed, the coarse kp up) and for the gains when the person wants it done by hand, with the auto-tuner off while the agent moves them, and said so. From PR #113's procedure and start values, in our settings' names;
  - **a fine threshold suggested from the fall,** half a scale step off its multiple, written only at the person's word;
  - **the end:** every resource either stored or put back (`saved`, the revert), the auto-tuner as it was found unless the person chose otherwise, and a short account to the person;
  - **the limits:** start and zero only when the person says an empty cup is on the pan; a scale that sends nothing gives the unit nothing to read, so check its cable, its make and its rate with the person;
  - **what a PIN is:** a lock against mistakes and meddling, not security.
- **AGENTS.md** says the development agent is the one it is for, and points to the guide's source.

### Gate 3: the PIN

- **Off by default.** A person sets one on the web (Settings), 4 to 8 digits. Without it, the network reads: every GET, the event log and its stream. Every write needs it: a setting, a store, a start, a stop and a hello included. A stop is always at the unit itself, by its button.
- **Carried by a cookie,** `pin=<digits>`. The portal sets it when the person enters the PIN, marked `SameSite=Strict` so no other site's page can use it, and asks again when a write is refused. An agent sends the same as a header, `Cookie: pin=<digits>`. A write without it is refused (401, `pin_required`); five wrong in a row refuse every write for a minute (`pin_locked`).
- **The index says whether one is set** (`pin`), never the PIN. It is in no settings copy or export, as the Wi-Fi key is not, and the console and the screen never ask for it.
- **A forgotten PIN** is cleared at the unit (a row in its Settings, with a confirmation) or by a factory reset. A change on the web needs the PIN, as every write does.
- **Stored** appended to the Wi-Fi record, whose slot has room (the EEPROM's rules: appended, a fixed width, `tests/release_records.py` passing).
- `rc_run.py` takes `--pin` for a unit that has one. The bench's units have none.

### What api_v1.md loses and gains

- "Experiments and recordings, for ADR-009 (reserved)" is replaced by "The throws" (gate 1). The test throw, the declared cup, the recordings and their samples are dropped: a setting is tried between real throws, which a person loads, and the unit has no room for samples beside the throw's own (a throw's samples were 24 KB as floats). Samples come back as a gate of an ADR of their own only if a real session meets a miss the record cannot explain.
- The password's row becomes the PIN, by cookie (gate 3).

## How the unit shows it works

Each gate on the Pico W, the tighter in RAM and the one whose memory is judged: the record takes heap and a route, the guide flash, and the PIN touches the network and the EEPROM. Every gate also runs the usual proof: `rc_run.py flash`, `menu_tour --compare`, `charge_cycle`, and `api_check` where the API changed. Each gate's figures are in its progress, its budget measured.

- **Gate 1:** a new `rc_run.py tuning_check --host <address>`, on the simulated scale, its cup checked by its most pour as every motor command's is: a charge, settings written between throws; each record's gains and thresholds what was live when it was thrown, its figures in order (coarse before fine, the fall the weight less the fine stop's reading); a read from 0 that gives every record kept; a `?wait` answered at the next throw; a restart that begins afresh; the settings left as they were found. Then a soak with a client following the throws, the heap read quiet before and after.- **Gate 2:** a fresh agent with no source and no repository, given only "My OpenTrickler is at <address>; help me tune my powder", finds the guide from the address alone, says hello, asks for permission, reads the throws, and gives its reason for a change from the record's figures. At the end nothing is live that is not stored, unless the person chose it.
  - **Before any powder, an average agent rehearses it** (the owner, 2026-11-28: "Before the powder test, I'd like you to spawn a haiku subagent that navigate through the unit mimicking an average agent helping the power tuning, without the context. Then you shall be responsible to monitor the haiku behavior, then tune the prompt/instructions accordingly."). A Haiku subagent is given only the person's words and the unit's address, with the simulated scale on. The development agent watches what it does, from the unit's side (`net log on`, the event log) and from its transcript, and changes the guide where it went wrong, never the subagent's prompt, then runs a new one; until one finds the guide, says hello, asks permission, and reasons from the record. A first run against the unit as it is, before the guide is written, shows what an agent does with none.
  - Then with the owner at the cup and real powder, which is the use case.
- **Gate 3:** with a PIN set: every write refused without it and done with it, every read free; five wrong refuse writes for a minute; the portal asks once and works after; the PIN cleared at the screen (pressed through the console) and by a factory reset; a settings copy holds no PIN; `tests/release_records.py` passes.
- **At the close:** PR #113 closed with its note, at the owner's word.

## Not in this ADR

- **The auto-tuner kept across sittings:** by design it begins afresh (the owner: the rig drifts).
- **Any change to the auto-tuner, and any study of it:** this ADR is never a tuning session (the owner; "The auto-tuner is not touched").
- **A tuning session on the unit's screen, and PR #113's web page:** the Pico W has no room for the module as written, and ADR-009 chose a tuner with no session.
- **A test throw, a declared cup, recordings, a throw's samples on the unit:** dropped (above).
- **A script that tunes with no AI:** the auto-tuner is that path; a script would be a second copy of the guide's steps to keep in step, and the simulated scale could not show it tunes.
- **A mark on the screen for settings not stored:** the web tags them, a restart puts them back, and the person is at the cup.
- **A scale that does not talk.** The owner: an AI helps only a little, as the unit receives nothing. The unit counts frames that did not parse (`bad_frames`) and neither bytes received nor serial errors, so it cannot tell no bytes at all from a wrong rate; a count and a plain "nothing received" would help a person with or without an AI. Noted, not planned.
- Data pasted into a chat, and an export made for it; support questions, set-up, health; ladders (ADR-014); gains shared between users (issue #10); anything that removes a start the API has.
