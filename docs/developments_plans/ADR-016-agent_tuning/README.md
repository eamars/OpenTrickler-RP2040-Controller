# ADR-016: an AI agent helps tune a powder's profile, over the API

Status: **Draft** 2026-11-27. Not approved. It holds the use case as the owner and the agent traced it on 2026-11-27, the owner's answers, what the API gives today, and PR #113's features folded in as the owner asked the same day. Nothing here is designed yet, and nothing is built.

## What we want

A person at the unit, loading real rounds, asks an AI agent on the same network to help tune one powder's profile. The person puts the cup on and empties it; the agent watches the throws, changes settings, and says what it did and why. The agent has the unit's API and nothing else: no probe, no console.

## Owner's direction (2026-11-27)

- On starting from the use case: "We need to trace back to the use case and requirements first, before writing anything down."
- On who the AI is: "I'd intent targeting at real AI agent users, not copy and paste channel."
- On writing: "I'd allow AI agents to write. I have the passcode/pin idea in mind, to stop people messing with the unit intentionally, or unintentionally for a unit shared with multiple users."
- On an agent starting something that pours: "No need. The user need to present to make sure the cup is properly placed, but I don't want you to eliminate the current capability. The scope is to extend, not remove."
- On what it is for: "Most questions to me are why my scale doesn't talk to the board, and in most cases are the setup issue. I don't think AI can help. The integration of AI is to help people tuning for the powder profile."
- On a scale that does not talk: "The common failure modes are 'The scale doesn't talk'. So AI can help, but only a little, as from OpenTrickler perspective nothing is received."
- On where this is built: "I don't think we need to include everything in ADR-009. IN fact I will merge ADR-009 now, and the API extension is to be added as separate ADR."
- On the API's freedom: "no v2.0 is released, so any API change is permitted."
- **On PR #113:** "Then perhaps fold PR#113 features, adjusted for our code base into ADR-016? And keep a note for PR#113 and leave it open for now, until ADR-016 is fully closed?" Weighed against the PR's own notes (below), agreed, and: "Go ahead". Read here as: #113's features are judged against this ADR's use case and taken where they serve it, in our code's form, with its author as co-author of the commits that use the work; the PR is not merged as it stands, and stays open, with a note, until this ADR is closed.
- Earlier, of the API (2026-11-15, in `ADR-003-http_rest_server/api_v1.md`): "does the API capable of supporting the ADR-009 and the future AI agent assisted development and tuning needs?", and "tuning will need some access to huge amount of data".

## The use cases, as traced

What a person might ask an AI to do with the unit, and where each stands. Only the first is this ADR's.

| What they say | This ADR | Today |
|---|---|---|
| "Tune this powder for me" (the agent changes settings over many throws, the person at the cup) | **Yes: the one it is for** | A gain tried is a PATCH, live for the next real charge; a throw tells only its result, final weight and whole time |
| "Why does it keep throwing over, or so slowly?" (asked while tuning) | Yes, as part of the first | The same gap: nothing says where in the throw the miss came from |
| "Log this session and give me its mean and spread" | Falls out of the first, not a goal | Works for an agent that follows the event stream from the start |
| "I changed powder, set it up" | No | The calibration and the settings already say it |
| "Is my unit healthy, is there an update?" | No | Works |
| "Throw a ladder of weights" | No: ADR-014 and issue #106 | The target is set by a PATCH between throws |
| "Why doesn't my scale talk?" | No (the owner: only a little; below) | The unit shows no reading |
| A person pastes the unit's data into a chat AI | No (the owner: real agents, not copy and paste) | |
| "What gains do others use for this powder?" (issue #10) | No: outside the unit | |

## PR #113, folded in

PR #113 ("Add powder analysis", by DevDaniel1408, open since 2026-10-26) tunes a powder's profile in a session of its own, on the 12864 and a web page. ADR-002 left its end to ADR-009, which was designed without it and took nothing of it but the D-term fix (ADR-002's, `adc5f56`, its author co-author). Read for this ADR in its developer notes and its tuner's header (`git show pr113:manuals/powder_analysis_developer.md`, `pr113:src/powder_analysis_tuner.h`), 2026-11-27:

- **Its tuner is driven as a loop:** the next trial's settings, a throw run on them, the throw's result reported, until done. Its logic has nothing of the hardware in it and is tested on a PC against a simulated powder. That is the shape of an agent on the API.
- **What a throw reports to it:** the settled weight, the time to the coarse stop, the whole time, whether it timed out, and the weight of the sample that stopped the throw; from the last two, the fall after the stop.
- **What it keeps:** a protocol of up to 60 trials (each its phase, gains, weight, coarse and whole time, verdict, fine minimum speed, fall), and the last throw's samples (300 at most: time, weight, each tube's P and D terms and speed).
- **Its procedure:** the smallest coarse Kp that passes the coarse stop within 7.7 s, by a bracketing search on coarse-only throws; the coarse Kd by doubling from 0.1; the smallest fine Kp whose full throw ends within 13 s; then rounds of 10 throws, 9 hits verifying, 2 overs giving the round up and lowering the fine Kp by a tenth or the fine minimum speed by a quarter, three times at most; 60 throws at most. Start values by the powder's shape (ball, stick, flake, disc) and grain size. The fine Kd is not searched.
- **Its threshold recommendation:** from the median fall, a fine threshold half a scale step off a multiple of the step, so a float's rounding never decides the stop.
- **Its cost:** about 70 KB of flash and 27 KB of RAM for the module, by its own figures. The Pico W's image today is 916 KB of its 1,016 KB slot, and its RAM is 98% taken at link time.

Where each part goes:

| Of PR #113 | In this ADR |
|---|---|
| What a throw reports, the trials kept, the samples | The unit's side: the throw's record and the throws kept (ideas 1 and 2); the samples are idea 4's question |
| The procedure, its start values, the threshold recommendation | The agent's playbook (idea 8): followed by an agent, or run as a script with no AI. No firmware |
| A trial on settings that are not the stored profile; a coarse-only throw | Idea 4's question |
| The session on the 12864 (16 screens), its web page with a chart and CSV, its manuals | Not ported: the Pico W's room, and a tuning session on the unit is what ADR-009 chose not to have ("Not in this ADR") |
| Its tests on a PC against a simulated powder | Ours exist (`tests/step2_sim.py`, the twin); the playbook is proven on them |

**Its end:** PR #113 stays open, with a note that says this, and is closed with a note naming what was taken when this ADR is closed.

## Why here

Not placed: its turn in the order is the owner's to say. It follows ADR-009, whose tuner it works beside, and it changes the charge's API, which ADR-014 changes too. PR #113 waits on it, open since October, so its place wants settling.

## What we know (read in the code and api_v1.md, 2026-11-27)

- **What an agent can do today:** read and write every charge, powder, scale and motor setting, each with its limits and a line on what it is (`GET /api/v1/schema`); set the target and the powder; start and stop a charge, a purge and a calibration; follow the event log, by held reads or as a stream; read the live weight (the stream's `state`, no oftener than every 100 ms) and each motor's speed (`GET /api/v1/motors`); read the calibration's records. The API describes itself: an action's GET gives its usage, every error says where to read.
- **What a throw tells:** `throw finished` carries its number, its result (good, under, over), its final weight and `elapsed_s`, the whole throw's time. That is what the unit's own tuner judged by until ADR-009's review; since then the tuner times the fine tube's part, from the coarse tube's stop, which the charge mode knows and gives to the console's line only.
- **What a throw does not tell:** when and at what weight the coarse tube stopped; the weight at the stop; what landed after it; the pauses; the gains it was thrown on. So an over from the coarse tube's powder still landing cannot be told from one of a fine tube too fast.
- **What is kept:** the newest 64 events of every kind. A throw is two of them or more, so about 30 throws at most, fewer when it pauses or settings are written. An agent that connects mid-session sees only those.
- **A value tried is a PATCH:** live at once and not stored. Each settings resource says whether it is `saved` and has a revert; a restart puts back what is stored. A write to the charge's settings, or to the powder in use, is refused while a throw is under way.
- **The unit's own tuner** (`auto_tune`, off by default): moves the fine Kp and Kd over windows of 25 throws and stores better ones into the profile, `by` unit. A window's trial gains are in RAM and in no reply. It begins afresh whenever the profile's gains are changed elsewhere, so an agent that writes gains while it is on restarts it each time. It never moves the thresholds, the speeds, Ki, or the coarse Kp upward.
- **No password:** anyone on the network reads and writes. api_v1.md kept the place for one: "A password, its own ADR: a header, paths unchanged".
- **The weight's unit** is known for the A&D alone (`GET /api/v1/scale`, `unit`); for every other make it is null.
- **Reserved in api_v1.md for ADR-009, and not built by it** ("Experiments and recordings"): a test throw with settings for that throw alone, its recording (a summary, then samples: a throw is 400 to 650 rows at the A&D's rate), and a cup declared by a press at the unit. Written before there was a tuner on the unit.
- **A client in Python exists** (`tools/remote_control/unit_api.py`): it works from the index and the schema and checks a write before sending it. And an agent may say hello (`POST /api/v1/agent/hello`).

## Ideas

A first view of what the use case asks of the unit, each to be weighed. None is a decision.

1. **A fuller record of each throw:** the coarse stop's time and weight, the weight at the stop, what landed after, the fine tube's part, the pauses, and the gains it was thrown on. PR #113's list is the start: the settled weight, the coarse and whole times, the weight at the stop and the fall. A few figures a throw: what an agent reasons over well.
2. **The last few dozen throw records kept on the unit,** apart from the event log, so an agent that joins late can read them. PR #113 keeps 60.
3. **The agent and the unit's tuner not at odds:** the record says which gains a throw used; the guide says to switch `auto_tune` off, or to leave the gains to it and tune only what it never moves.
4. **A trial that is not a PATCH, and a throw's samples: open.** With a person present, a PATCH that is live and not stored, `saved`, the revert and the restart may be all a trial needs. PR #113 counts against that: it runs each trial on settings of its own, runs the coarse tube alone, and keeps a throw's samples. The fuller record may make the coarse-only throw needless (a full throw then tells the coarse tube's time and overshoot); whether the rest is needed decides what becomes of the test throw and the recordings reserved in api_v1.md.
5. **A PIN** (the owner's idea): set on a unit, a write needs it. Whether a start needs it, whether a read stays free, how a person sets and clears it at the unit, and what a factory reset does to it.
6. **A guide for agents,** beside the API's own description: what tuning needs of the person, what to put back at the end.
7. **Nothing taken away:** every start the API has stays.
8. **A playbook for tuning,** from PR #113's procedure in our terms: which setting to move for which miss, in what order, by how much, when to stop, and start values by the powder's shape. An agent follows it; the same steps run as a script through `unit_api.py` with no AI, which is also how it is proven on the simulated scale and the twin. It lives with the tools, not in the firmware.
9. **A fine threshold recommended, never written:** from the fall after the stop, as PR #113 reckons it, shown for the person to take or leave. The owner's direction in ADR-009 stands: the fine threshold is the user's.

## To settle before approval

- Which of the ideas are in, and the throw record's fields.
- The PIN's reach (idea 5).
- Whether a trial needs more than a PATCH and a revert, and a throw's samples (idea 4), and so what becomes of the section reserved in api_v1.md: built, replaced or dropped.
- The playbook's form (idea 8): a document an agent reads, a script, or both; and how much of PR #113's procedure holds beside the unit's own tuner, which already moves the fine gains.
- How it is shown without powder (the simulated scale, which a charge on the bench already uses) and what needs real throws.
- Its place in the order of the ADRs.

## How the unit shows it works

A first view: an agent given only the unit's address, with the owner at the cup, tunes a powder on the bench. It says from the unit's own figures why throws miss, changes a setting, and the next throws show it or the setting is put back. When it ends, nothing is live that is not stored. With a PIN set, a write without it is refused. At its close, PR #113 is closed with a note of what was taken, and the commits that took it name its author.

## Not in this ADR

- **A tuning session on the unit's screen, and PR #113's web page:** the Pico W has no room for the module as written, and ADR-009 chose a tuner with no session. A person without an agent has the unit's own tuner, or the playbook as a script.
- **A scale that does not talk.** The owner: an AI helps only a little, as the unit receives nothing. Noted, not planned: the unit counts frames that did not parse (`bad_frames`) and neither bytes received nor serial errors, so it cannot tell no bytes at all from a wrong baud rate; a count and a plain "nothing received" would help a person with or without an AI.
- Data pasted into a chat, and an export made for it.
- Support questions, set-up, health.
- Ladders and charge profiles (ADR-014).
- Gains shared between users (issue #10).
- Anything that removes a start the API has.
