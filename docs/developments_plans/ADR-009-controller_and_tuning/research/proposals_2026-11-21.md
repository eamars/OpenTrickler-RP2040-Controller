# ADR-009: two proposals for the charge controller and its calibration

2026-11-21. Asked for by the owner: "I'd like you to make two proposals based on the knowledge and data to date, with confident ranking from high to med. Each proposal shall include the detailed procedure for both trickle control, as well as calibration routine. And remember, those proposals will be used on ANY opentrickler build/material/finish, and ANY powder, and ANY scale." And: "There are more than 7 scale to be supported, so I'm not planning to evaluate each of them ... the later calibration routine must account for that, and your simulation today should assume the inconsistent measurement lag subject to the weight change speed."

**Neither proposal is approved.** Both were written as code and tried in the simulation (`tools/charge_sim/staged.py`), on the two records made from the bench's recordings and on four other scales that stand in for scales never measured. **Both have since thrown on the real unit, with AR2209** (the same afternoon; the next section). Every figure below says whether it is the bench's or the simulation's. `../legacy/progress.md` has the runs.

## After the bench trial (2026-11-21, afternoon): what stands, what fell, what changed

**What was run:** the PC as the controller, through the unit's console, on the Pico 2 W with the A&D and the air-exposed AR2209 (`trial_guide.md`): the calibration on the unit, then for each proposal and band ten rehearsals and 25 throws of 10 gr. Three short runs came first and found faults, each mended at the desk before the unit turned again.

| AR2209 on the unit, 25 throws a set | Good | Over | Under | A throw, median | Moves a throw | Its check |
|---|---|---|---|---|---|---|
| Proposal 1 within 0.02 gr | 56% | 24% | 20% | 25.1 s | 4.4 | at least 60% good: **fails** |
| Proposal 2 within 0.02 gr | 68% | 32% | 0% | 13.2 s | 0.7 | at least 52% good, at most 48% over: **passes** |
| Proposal 1 within 0.04 gr | 84% | 12% | 4% | 14.7 s | 1.8 | at least 84%: **passes**, on the line |
| Proposal 2 within 0.04 gr | 84% | 16% | 0% | 12.8 s | 0.6 | at least 84%: **passes**, on the line |

Proposal 2's first rest lay within a digit of its aim in 15 throws of 25, where its own check asked for 20: **fails**.

**What stands:**
- **The frame holds on a real unit.** The forecast, one law a tube, the verdict at rest, a calibration that takes every number from the unit. With the harder of the two powders it gave 68% within 0.02 gr and 84% within 0.04 gr in 13 s a throw, where today's loop puts 85% of its throws over with this powder.
- **The stream's stop is as tight as the simulation said.** First rests spread by 1.7 to 2.2 digits for proposal 2 and 2.2 to 3.5 for proposal 1's stop from its dribble. Every figure of proposal 2 but one, and every figure of both sets within 0.04 gr, fell inside the forecast written before the run.

**What fell:**
- **The ranking, for this powder.** I gave proposal 1 the higher confidence. With AR2209 on the unit proposal 2 is the more accurate within 0.02 gr, in half the time, and level within 0.04 gr.
- **The small move.** Proposal 1's finish rests on a small move that adds a little. This powder gathers at the tube's lip and falls as a clump: the smallest move (0.024 of a turn) gave nothing 66 times in 100, one to three digits 21 times and four to nine digits 14 times; the next size gave four digits and more nearly half the time. The simulation's moves were kinder (one to three digits 32 times in 100).

**The ceiling with this powder, from the trial's own moves and stops, no model** (the finish played on what was measured, the best choice at every rest; `tools/charge_sim/bench/best_finish.py`): **about 70% good within 0.02 gr and 90% within 0.04 gr, whatever the finish.** Where the stream's stop is aimed changes that by a few points only (65 to 71% from ten digits short to the target itself). It decides the moves a throw makes, and so its time: 1.5 a throw aimed a digit short, 4 aimed nine short. From a rest two to four digits short a throw is rescued little better than one time in two.

**What changed in the frame,** each from a fault the unit showed (sections 2 and 3.2 below are as the code now stands):
1. **The lag is the powder's time from turned out to shown, measured over a whole run,** not what is unseen at the moment of a stop. The stop's figure also held the latency of whoever turns the tubes, and a forecast built on it counted a fast coarse stream twice.
2. **Both tubes keep clear of the coarse tube's powder still unseen,** by three times its spread a turn (5 to 6% with AR2209). A small charge is all unseen when the coarse tube stops.
3. **The fine tube takes its stop on its own powder alone:** a forecast at its margin while coarse powder is unseen pauses it.
4. **A rest far short goes back to the stream:** when the stream would pour more from it than the largest small move gives. Before, a throw that stopped two grains short had eight small moves and no more.
5. **A small move is a turn, made to the unit's step count:** the calibration keeps the turn each size really was.

**What I recommend now:**
1. **Build the frame once, as before; that part of the recommendation stands, and on a real unit now.**
2. **Do not make proposal 1's finish the default.** With a powder whose kernel is a digit it is the slower and the less accurate.
3. **Let the calibration choose the finish.** The two are one controller, and the calibration already measures what decides between them: what each move gives, and where the stream's stop rests. Playing the finish on those gives the aim with the most good throws in the fewest moves, the move to make from each shortfall, and the share of good throws to tell the user before the first throw. With AR2209 it chooses proposal 2's finish. **This is a direction, worked out at the desk in one afternoon and tried on nothing.**
4. **Before that: the same trial with BM2** (an hour; its forecast is written: proposal 1 94% and proposal 2 88% good within 0.02 gr). BM2's kernel is half a digit, and its moves may be the kind proposal 1 needs. Until then the ranking is known for one powder.
5. **One over in fifty within 0.02 gr with a powder like AR2209 is not a controller's to give:** it needs a tube that releases less at a time (section 8).

## The short answer, as the simulation gave it before the bench

*The bullets and tables of this section are the simulation's, of the controller before the bench's mends (which moved its figures by two points at most: `tools/charge_sim/fit/bands_after_bench.txt`). Its ranking and its recommendation are superseded by the section above.*

- **The two proposals are one controller.** They share a frame (section 2) and one calibration, and differ in two numbers: how slow the fine tube goes before it stops, and how far short of the target it stops. So the firmware is built once, and which finish a powder gets on a scale can be decided by the calibration.
- **Proposal 1, "stop short and jog": confidence high.** The fine tube stops from a steady dribble while clearly short, and small moves add the rest, each judged on a reading at rest. It is industry's way, its ceiling can be read off the bench's own recordings with no model, and in the simulation it was as accurate as proposal 2 or more so on every scale tried, by up to 9 points, with fewer overs on each. Its cost is time: it waits for the scale after every move.
- **Proposal 2, "glide and stop": confidence medium.** The fine tube slows to a creep the scale can follow and stops by a forecast a digit short of the target; only a throw that rests short is topped up. It is quicker, and it has more overs: when the stream brings a late lump there is no margin to take it. Its key step, a creep stopped by the reading, has not been seen on a real scale.

**On the bench's A&D** (the simulation, on the records of the bench's two powders; 500 throws of 20 gr each; of the true weight in the cup; a throw's time runs to the verdict on a reading at rest):

| Within 0.02 gr | BM2: good, over | Time: median; one in twenty over | AR2209: good, over | Time |
|---|---|---|---|---|
| Today's loop (fine gain 0.8, slow finish) | 79%, 20% | 15.3 s; 17.1 s | 15%, 85% | 10.6 s; 11.9 s |
| **Proposal 1** | **94%, 5%** | 12.7 s; 21.6 s | **74%, 17%** | 16.6 s; 29.2 s |
| **Proposal 2** | **89%, 11%** | 11.9 s; 17.8 s | **74%, 24%** | 13.0 s; 28.7 s |
| The best a finish by small moves can do, from the bench's recorded nudges, no model (`../legacy/progress.md`) | 88 to 90%; 82 to 87% at waits of 2 to 4 s | | 80%; 74 to 78% | |

| Within 0.04 gr (the looser band, as a mode) | BM2: good, over | Time | AR2209: good, over | Time |
|---|---|---|---|---|
| Today's loop (the same throws) | 90%, 10% | | 26%, 74% | |
| **Proposal 1** | 98.6%, 1.0% | 8.9 s; 13.9 s | 91%, 8% | 12.8 s; 23.8 s |
| **Proposal 2** | 98.6%, 1.4% | 9.2 s; 11.4 s | 87%, 12% | 10.2 s; 17.6 s |
| The best by small moves, from the bench's nudges | 99%; 93 to 96% | | 92 to 94%; 84 to 88% | |

**On other scales** (the simulation; each scale is the measured one with its delays stretched, a stand-in and not any make; the controller is calibrated again on each, as a unit would be; good within 0.02 gr, and the median time):

| The scale | BM2: proposal 1 | Proposal 2 | Today's loop | AR2209: proposal 1 | Proposal 2 | Today's loop |
|---|---|---|---|---|---|---|
| As measured | 94%, 12.7 s | 89%, 11.9 s | 79% | 74%, 16.6 s | 74%, 13.0 s | 15% |
| As measured, but every small change held 2.5 s (the bench's own scale lies between the row above and this one) | 90%, 15.9 s | 85%, 15.2 s | not run | 78%, 22.5 s | 68%, 15.2 s | not run |
| A stream shown as measured, a single small change held 5 s, four readings a second (the owner's account of his G&G) | 89%, 36.7 s | 87%, 31.6 s | not run | 75%, 33.9 s | 68%, 24.6 s | not run |
| Twice as late in everything, a small change held 3 s, four readings a second | 90%, 31.5 s | 82%, 28.4 s | 0.2%, every throw over | 68%, 36.1 s | 60%, 25.6 s | 0.4% |
| Three times as late, a small change held 5 s, two readings a second | 72%, 58.6 s | 65%, 45.9 s | 0% | 42%; four throws in ten not ended in 90 s | 42%; three in ten not ended | 0% |

**What to take from it:**
1. **Either proposal mends what is wrong today.** AR2209 goes from 15% good to 74%. On a scale twice as late, where today's loop with the same settings puts every throw over, they keep 60 to 90%.
2. **Proposal 1 is as accurate or more so on every scale tried (level with AR2209 on the measured scale, ahead by 2 to 9 points elsewhere), with fewer overs on each; proposal 2 is the quicker,** by 1 to 4 s on the A&D and 3 to 10 s on the later scales.
3. **AR2209's 74% is the powder's ceiling in this tube, not the controller's:** the bench's own nudges allow 80% at best. That is one over in six within 0.02 gr, and one in thirteen within 0.04 gr. BM2 reaches one over in twenty within 0.02 gr and one in a hundred within 0.04 gr. One over in fifty within 0.02 gr with a powder like AR2209 needs smaller releases, which no controller gives (section 8).
4. **Within 0.02 gr neither reaches 10 s.** In the simulation as measured, proposal 2 is inside the owner's 15 s with both powders and proposal 1 with BM2 only. Within 0.04 gr both take 9 to 13 s.
5. **The simulation flatters proposal 1's time.** Its scale shows nine small moves in ten within 2 to 2.5 s; the bench's A&D took 4 s. With every small change held 2.5 s, which is later than the bench's, proposal 1 takes 15.9 s with BM2 and 22.5 s with AR2209, and proposal 2 takes 15.2 s with both. The bench's scale lies between the two rows: **expect 13 to 16 s (BM2) and 17 to 22 s (AR2209) from proposal 1 within 0.02 gr, and 12 to 15 s from proposal 2.** Within 0.04 gr: 9 to 12 s and 13 to 18 s from proposal 1, 9 to 13 s from proposal 2.
6. **A scale that is late is slow whatever is done:** half a minute a throw within 0.02 gr on the two middle stand-ins, by either proposal. Every verdict needs one wait as long as that scale takes to show a small change, and every correction another. The calibration says so before the first throw (section 3.2, step 9).

**What I recommend:** build the frame once, with proposal 1's finish as the one a unit uses by default, because its accuracy is the one I can stand behind on a scale nobody has measured. Keep proposal 2 as what it is in the code, two numbers changed, and let one bench session judge it (section 7): it is the only one of the two that can hold 15 s within 0.02 gr on the bench's scale, and the simulation is harsher on it than the bench's recordings warrant (section 5).

## 1. What "any build, any powder, any scale" asks, and the seven rules that answer it

Both proposals keep these. They are what lets one controller serve a unit nobody has seen.

1. **Nothing about a scale, a powder or a tube is built in.** Every number the controller uses is measured on the unit by its calibration, in the scale's own digits and in seconds, or follows from those by a rule written here. The user gives one thing: the band (0.02 gr, or 0.04 gr for loads that need less).
2. **Powder is counted in turns.** The bench found, for both powders and both tubes, that what a tube pours follows the turns it made and not its speed. The unit counts its own steps, so it knows what it turned out before any scale shows it.
3. **The scale's lag is a table by flow, not one number.** A scale shows a fast stream sooner than a slow one, and a slow one sooner than a single kernel. The table is measured with the unit's own tubes as the source of a known flow, on the scale that is attached. No list of scales is kept in the firmware.
4. **Decisions are timed by the unit's clock and step count, every millisecond, not by the scale's rhythm.** A scale that reports twice a second is controlled as finely as one that reports twenty times.
5. **The verdict, and every decision to add powder, is on a reading at rest.** How long "at rest" takes is measured too.
6. **What cannot be reached is said.** The calibration ends in a forecast for this powder on this scale: how many throws in ten will hold the band, and how long a throw takes.
7. **A throw fails safe.** Powder that does not come makes a throw slower or short, never over; limits end a throw with a reason.

| What varies between units | What both proposals do about it | What the calibration measures |
|---|---|---|
| The scale's digit and unit (0.02 gr on the A&D in grains, 0.001 g on others) | Work in the scale's own digits | The digit (the smallest step seen) |
| How often it reports (2 to 21 a second among the nine drivers) | Count turns between readings; time every stop by the step count | The interval (the firmware measures it already) |
| How late it shows a stream, by the stream's flow | The forecast (2.1) | What arrives after a stop from each of six flows |
| How late it shows one small change (2 to 4 s on the A&D; 5 s and more on the owner's G&G) | Wait as long as this scale needs and no longer | How long small moves took to show |
| A powder's kernel against the digit (BM2 0.6 of a digit, AR2209 1.05, long sticks 2 and more) | Nothing is assumed of the kernel: everything is in digits released | What small moves release |
| Clumping, bridging, a tube that starves | A forecast that drops what the scale did not confirm; a margin; a top-up | The spread of every stop and move |
| A tube's powder a turn (print, material, finish, generation, gear) | Speeds follow from the flows wanted, through the measured grains a turn | Grains a turn of each tube, from rest to rest |
| The charge (5 to 100 gr) | The laws are in room left, not in fixed weights; a small charge is the fine tube's alone | |
| The hopper's level, a tube that stood | Prime at the start of a calibration; in use a tube that pours less only slows the throw | Learned again from each throw |

## 2. What both proposals share: one frame

### 2.1 The forecast

The weight the controller acts on is not the reading. It is **the reading, plus the powder turned out that the reading does not show yet.**

- Every 10 ms each tube's turns since the last count become a parcel: its grains (turns x that tube's grains a turn), and the moment it is due to show (the middle of those 10 ms + the lag at that tube's present flow, from the table). The turns are the unit's own count of its steps.
- When a reading comes, every parcel due by then is dropped: the reading holds it now.
- The forecast is the latest reading plus the parcels still due. The room is the target less the forecast.
- **The lag at a flow is how long that flow's powder took from turned out to shown, over a whole run** (after the bench): the area between the powder turned out and the reading, over the powder poured. Measured at the moment of a stop it also held the latency of whoever turned the tubes (0.07 s from the PC), and the forecast then counted a coarse stream of 19 gr a second twice while its first powder showed: two throws of eight stopped 2 to 3 gr short.
- **What it still gets wrong, measured on the unit:** through a start of the coarse tube at 2 rev/s it is over by up to 1.4 gr and under by up to 0.8 gr, and usually 0.3 gr off. The first powder of a fast start shows sooner than its lag. Two other ways of letting a parcel show were tried on the same recordings and were no better. The margin of 2.2 is what covers it.

**Why each parcel has its own lag:** this is the owner's point, that the lag depends on how fast the weight changes, put into the controller. A first version used one lag for the whole recent past, that of the present speed. While the tube slowed it took fast powder for still on its way long after it had shown, and stopped 12 digits early. With a lag a parcel, the rehearsals rested within two digits of their aim with no correction.

**What it does when powder does not come** (a tube not primed, a bridge, a hopper nearly empty): the parcel falls due, the reading has not risen, and the forecast drops back to what the scale confirms. The tube turns on. So the reading has the last word, and missing powder costs time and never an over.

### 2.2 The two tubes

Each tube has one law: **a speed in proportion to the room above its own margin, between a floor and a top, and a stop for good when the room is down to its margin.**

- **The coarse tube:** speed = (room - coarse margin) / (grains a turn x coarse glide time), held between 0.3 rev/s (below it the bench's coarse tube poured in lumps) and its top. It stops, held, when the room reaches the coarse margin. There is no reverse turn: the bench found a stopped, held coarse tube drops nothing late, and that the reverse quarter turn spills 0.6 to 1.1 gr. A gate, where fitted, closes at this stop.
- **The fine tube** turns at its top speed while the coarse tube runs. Then: speed = (room - glide margin) / (grains a turn x glide time), between its floor and its top. It stops when the room is down to its stop margin. **The floor and the stop margin are where the two proposals differ.**
- A charge smaller than the coarse margin and half a coarse turn is the fine tube's alone.
- **Both tubes keep clear of the coarse tube's powder still unseen** (after the bench): the room both laws work on is less by three times the coarse tube's spread a turn, of what it has turned out and the reading does not show yet. That powder is known by its turns alone, and a turn's powder varied by 5 to 6% on the unit. A charge of 10 gr is all unseen when the coarse tube stops, so it stops 2 gr short and the fine tube carries that; in a large charge the reading has caught up and little is added.
- **The fine tube takes its stop on its own powder alone** (after the bench): while a digit or more of the coarse tube's powder is unseen, a forecast at the stop margin pauses the fine tube and does not end the stream.

### 2.3 The rest, the verdict, the top-up

- **At rest:** the calibrated wait since the stop has passed, and the reading has made no new high for 0.4 s (or two reading intervals, if that is longer).
- **The reading judged is the lowest shown since its last rise.** A reading that flickers across the band's lower edge is then taken as outside it, and the throw is topped up. Judged on the highest, one throw in ten rested just under the band and was called good (proposal 1 with BM2: 86% good against 94%).
- **Within the band:** done, good. **Above it:** done, over. **Below it:** a small move of the fine tube.
- **The move** is the largest of three calibrated sizes whose average release is no more than half the way from the present weight to the band's top. It is a turn, made to the unit's step count: the turn that size really was in the calibration. After it, that size's own calibrated wait, then rest, then judged again.
- **Far short, back to the stream** (after the bench): a rest short by more than the fine tube's stop margin and the largest move's average release together goes back to the stream, by the same law, from the reading at rest. Whatever stopped the tubes early then costs the throw time and not its weight. Nothing is learnt from such a rest.
- **Eight tries after the first rest,** moves and returns together; then the throw ends "short", and the user sees it.
- **Where the trial found this rule weak:** it is not the best play of a powder's odds. With AR2209 the best play of the measured moves gives 66% good from proposal 1's aim, and the rule gave 56% (the first section). The calibration can work out the move to make from each shortfall; not built.

### 2.4 Between throws

- **A share of each throw's first error at rest moves the fine tube's stop margin** (15% of it, no more than two digits' worth from one throw, nothing from an error under a quarter digit). This is industry's learned cut-off (`sources/dosing_control.md`, section 3.2). It takes out drift, not scatter. It ran in every throw of the trial.
- **Designed, not tried:** each tube's grains a turn trimmed the same way from every throw's turns against its weight at rest; nothing learned from a throw that was disturbed or ended by a limit; the calibration's tables of spreads and releases refined from the throws' own records.

### 2.5 Limits, and what a throw leaves behind

- No reading for three of the scale's intervals: both tubes pause (ADR-013's rule, kept).
- A tube that has turned out more than a set amount with no rise of the reading: the throw ends "no powder".
- Every throw is recorded: the readings, both speeds, the forecast, the first error at rest, each move and what it gave.

### 2.6 The numbers, and where each comes from

| Number | Rule | From the calibration's |
|---|---|---|
| Grains a turn, each tube | measured | three runs from rest to rest |
| Lag at a flow, each tube | the powder's time from turned out to shown, over the whole of each run (the area between the two, over the powder poured) | runs and stops from six flows (fine), three speeds (coarse) |
| Coarse and fine glide time | 0.6 x the lag (coarse: at its floor; fine: the longest at its middle flows), between set limits | lag table |
| Coarse margin | 3 x the spread of a coarse stop from its floor + the fine tube's glide margin + 0.2 gr | coarse stops |
| What both tubes keep clear of the coarse tube's unseen powder | 3 x the spread of its grains a turn, run to run, as a share | every coarse run of the calibration (ten) |
| Fine top speed | the highest speed measured even | fine stops |
| Rest time | the slowest the reading came to rest after stops near the fine tube's floor, + 0.3 s | fine stops |
| Move sizes | the turns meant to release 0.7, 2.5 and 6 digits, each kept as the turn the unit's steps say it was | grains a turn; the small moves |
| A move's release and its wait | measured | small moves |
| The fine tube's stop margin | by the proposal's rule, then moved so the rehearsals rest where they are meant to | rehearsals |
| Every watch of the calibration itself | half as long again as the slowest of four first moves took to show, + 1 s | probe |

## 3. Proposal 1: stop short and jog. Confidence: high before the bench; with AR2209 it failed its check

**In one sentence:** the fine tube stops from a steady dribble while still clearly short, and the rest is added by small moves, each judged on a reading at rest.

**On the unit with AR2209:** 56% good within 0.02 gr in 25 s a throw (its check asked 60%; the forecast said 74% in 17.6 s), 84% within 0.04 gr in 14.7 s. Its stop from the dribble was tight (first rests spread by 2.2 digits). Its moves were not: nine throws of 25 spent all eight tries, five were taken over by a move's clump. Not yet thrown with BM2, whose moves may be the kind it needs.

This is industry's bulk, dribble and jog, and what the research proposed on 2026-11-19 (D2), with what the bench taught since: the stop is placed by the forecast, the moves' sizes and waits are measured and not assumed, and the coarse tube does not reverse.

### 3.1 The throw, step by step

1. **Zero** with the cup on, as today.
2. **Both tubes start.** The fine tube at its top speed; the coarse tube by its law (2.2), from 2 rev/s down to 0.3.
3. **The coarse tube stops** when the room is down to its margin. In the trial the margin was 0.4 to 0.6 gr, and the coarse tube had poured 18.3 to 18.9 of the 20 gr.
4. **The fine tube dribbles** at one speed and does not creep: the slowest flow the bench found even, 8 digits a second (0.2 rev/s with AR2209, 0.3 with BM2).
5. **The fine tube stops** where the forecast is short of the target by twice the rehearsals' own spread and a digit. In the trial: 3.4 digits short with BM2, 6 with AR2209.
6. **Rest** (2.5 s in the trial), then the first judgement.
7. **Moves until the reading holds the band** (2.3). In the trial nine throws in ten needed moves: three on average with BM2, four with AR2209, each waited 2.0 to 2.5 s.
8. **The verdict,** and the learning (2.4).

### 3.2 Its calibration, step by step

**What the user does:** fills the hopper, puts a cup that holds 200 gr on the pan, starts "Calibrate this powder" in the powder's profile, and confirms the cup is on (the safety step stays). Nothing is touched until it ends. The cup's powder goes back in the hopper.

**What the unit does:**

| Step | What is turned | What is measured |
|---|---|---|
| 1. Zero, and the scale at rest | nothing | the digit; the interval |
| 2. Prime | each tube, in runs, until two runs pour the same within a tenth | (nothing kept) |
| 3. Probe | four small moves of the fine tube, each watched up to 15 s | how long this scale takes to show a small move: every later watch follows from it (3 to 4 s on the A&D, 8 to 9 s and 12 to 15 s on the later stand-ins) |
| 4. Grains a turn | each tube three times from rest to rest | grains a turn; with step 5's coarse runs, how far the coarse tube's varies from run to run |
| 5. The lag by flow | the fine tube stopped from 1, 2, 4, 8, 16 and 32 digits a second (as far as its top speed), eight stops at the two slowest and four at the others; the coarse tube from 0.3, 1 and 2 rev/s | over each run: how long its powder took from turned out to shown (the lag); after each stop: what still arrived (its spread) and when the reading came to rest |
| 6. Small moves | three sizes, 24, 12 and 8 of them | the turn each really was; what each size releases, and how long it took to show |
| 7. The rules | nothing | the numbers of 2.6 |
| 8. Rehearsals | ten throws of 1 gr, the fine tube alone, each ended at its first rest | where they rest and how widely: the stop margin is set from their spread and moved so their middle one rests on its aim |
| 9. The verdict | nothing | from the rehearsals and the moves: how many throws in ten are expected in the band, and the seconds a throw (designed, not tried) |

**On the unit** (2026-11-21, AR2209, steps 2 to 6): 13.0 minutes and 173 gr through the cup, every watch 5.3 s. Its rehearsals of 1 gr rested more widely than its throws then did (a spread of 4.1 digits against 2.2 for proposal 1), which set proposal 1's aim further short than its throws needed: **a rehearsal should be a throw's last stretch, not a short stream from rest.** Not mended.

**What it costs, in the trial:** 10 to 12.5 minutes and 135 to 155 gr through the cup on the A&D; 18 to 21 minutes on the scale twice as late; 24 to 31 minutes on the one three times as late. Most of it is waiting: each of about ninety stops and moves is watched as long as the scale needs to show a small change. The research's "about two minutes" of 2026-11-19 assumed 0.6 s where the bench's scale needs 4.

**A shorter one** (part tried): the scale's lag table kept from one powder to the next, so that a new powder skips step 5, about four of the eleven minutes. Tried with each powder using the table measured with the other: with BM2 94.2% good against 94.4%; with AR2209 69.8% against 73.8%, and 4 s slower a throw. So it holds within four points. Fewer repeats in steps 6 and 8, mended in use by the throws' own records: not tried.

**What is kept, in the owner's three parts:**
- **the scale's** (measured again when the driver, the digit or the interval is seen to differ): the digit, the interval, the watch time, the lag by flow in grains a second, the rest time;
- **the tube's with this powder** (in the powder's profile): grains a turn of each tube, the spreads, what the three moves release and their waits, the stop margin;
- **the user's:** the band.

**When it is run again:** for a new powder; when the scale or its response setting changes; and when the unit's own record says the tables no longer fit (first errors at rest that the learning cannot follow), which the unit says in words.

**Without an AI agent, and with one:** every step above is a job the firmware runs and bounds by itself, on fixed rules. An agent on the API can run the same jobs, read their recordings and write the same tables; it cannot run a motor outside the firmware's bounds (ADR-009, D6, kept).

### 3.3 The looser band

The same throw with the band at 0.04 gr. The stop and the moves are the same; a throw is done as soon as it rests within two digits, so it needs fewer moves.

### 3.4 Why high, and what it costs

- **Its accuracy does not rest on the simulation.** The bench's own nudges, drawn with no model, give a finish by small moves 74 to 78% with AR2209 and 82 to 87% with BM2 at waits of 2 to 4 s (80 and 88 to 90% at best). The simulation's 74% and 94% sit at and above that; the bench's figure for BM2 is the one to expect.
- **It asks least of a scale:** that a reading comes to rest. It holds on every scale the calibration can watch, and the calibration finds how long to watch.
- **Its margin takes what a stream does late.** It stops 3 to 6 digits short, so a lump after the stop lands inside the room left.
- **It costs time, and more on the bench than in the simulation.** A rest and three to four moves, each waited for. On the bench's A&D that is 13 to 16 s a throw with BM2 and 17 to 22 s with AR2209 within 0.02 gr, against the owner's 15 s; within 0.04 gr, 9 to 12 s and 13 to 18 s (the short answer's point 5).
- **What would prove it wrong:** a scale whose reading never comes to rest in a usable time (the third stand-in is at that edge), or a powder whose small moves mostly release four digits and more (the bench's moves of 0.06 of a turn did, four times in ten with AR2209: the smallest move must stay small).
- **The bench proved the second, in a way this list did not foresee:** with AR2209 the smallest move stayed small on average (1.1 digits) and was useless all the same. It gave nothing two times in three and a clump of four to nine digits one time in seven. What a finish by moves needs is not a small average but a move that gives one to three digits most of the time, and this powder has none.

## 4. Proposal 2: glide and stop. Confidence: medium before the bench; with AR2209 it did as forecast

**In one sentence:** the fine tube slows to a creep the scale can follow, stops where the forecast is a digit short of the target, and only a throw that rests short is topped up.

**On the unit with AR2209:** 68% good within 0.02 gr in 13.2 s a throw (the forecast: 70% in 13.0 s) and 84% within 0.04 gr in 12.8 s. Its key step is now seen on a real scale: a creep stopped by the reading rested with a spread of 1.7 to 2.2 digits, 18 throws of 25 needing no move. It has no unders and more overs (32% within 0.02 gr): three throws of 25 were over at the first rest, five were taken over by the move that mends a short one. Its own check of the stop (the first rest within a digit of the aim eight times in ten) failed: 15 of 25.

It leans on what the owner said of scales and the bench confirmed: a stream is shown sooner than a single small change. Two of its throws in three need one wait for the scale, where proposal 1's need four.

### 4.1 The throw, step by step

1. to 3. As proposal 1.
4. **The fine tube glides:** its speed falls with the room, by its law (2.2), until it reaches the creep.
   - **The creep's speed:** the flow at which the powder still on its way is about one band's worth, at the lag measured at the slowest flow: band / lag / grains a turn. In the trial: 0.03 rev/s with BM2 (a digit a second), 0.06 with AR2209.
   - **The glide margin** (where the creep begins): the stop margin plus twice the spread of a stop from four times the creep's flow. In the trial: 3 digits with BM2, 7 with AR2209, so the creep lasts about 3 s.
   - **On a scale so late** that this creep would take over 6 s, the creep is quicker and more is left to the moves.
5. **The fine tube stops** where the forecast is one digit short of the target (half the band, if that is more). Aimed at the target itself the trial gave 5 points fewer good throws and half as many overs again: a throw that rests short can be mended, one that rests over cannot.
6. **Rest** (1.8 to 2.5 s in the trial), then the first judgement. With BM2 two throws in three were done there.
7. **Moves, for a throw that rests short** (2.3): one throw in three, two to three moves.
8. **The verdict,** and the learning.

### 4.2 Its calibration

**The same routine as 3.2, step for step, with the same cost.** What differs is what leans on what:
- step 5's stops from the two slowest flows set the creep's speed here; in proposal 1 they serve only the rest time;
- step 8's rehearsals end at the creep's stop, and their middle one is moved onto the aim, a digit under the target;
- step 6's small moves are used by one throw in three, not nine in ten.

**The shorter one** (3.2) was tried here too, the scale's lag table taken from the other powder: 88.6% good against 89.0% with BM2 and 3.5 s slower a throw; 73.8% against 74.4% with AR2209.

**One thing more it needs, designed and not tried:** when the rehearsals rest more widely than the band can hold, the unit uses proposal 1's finish for that powder on that scale, and says so. That is how a scale that does not follow a slow stream is met.

### 4.3 The looser band

The same with the band at 0.04 gr: the creep is twice as fast, since twice as much may be on its way; the aim stays a digit under the target; a throw resting within two digits is done.

### 4.4 Why medium

- **It is the quicker,** and the only one of the two that held 15 s within 0.02 gr with both powders in the simulation.
- **It has more overs:** 11% against 5% with BM2, 24% against 17% with AR2209. When a stream brings a lump late, a stop a digit short has no room for it.
- **Its key step has not been seen on a real scale.** Every stop the bench recorded was timed by the PC's clock. A stop triggered by the reading comes just as a lump shows, and the bench's A&D showed 15 of 24 two-digit releases a digit at a time, the second 1.5 s after the first. The simulation's scale shows them together.
- **The simulation is harsher on it than the recordings warrant** (section 5): its fine tube pours in longer gaps than the bench's did. So its 89% and 74% may be low. I do not know by how much.
- **What would prove it right:** on the bench, thirty throws whose first rest lies within a digit of the aim eight times in ten (the bench's stops from 0.02 rev/s, by the PC's clock, held three digits in 90% of 30 with AR2209 and 75% of 20 with BM2).

## 5. The trial in the simulation: how it was run, and what limits it

- **The rigs:** the two records of this afternoon (`pico2w_bm2`, `pico2w_ar2209`), made from the bench's recordings and checked against recordings not used to make them.
- **The scale's lag depends on how fast the weight changes,** as the owner directed: the delay every reading shares (0.4 s), a further lateness by flow (none at coarse flows, up to 0.56 s at fine ones), and a single small change held back about 0.9 s more. The other scales stretch these (`scales_like.py`).
- **Nothing was set by hand for a rig.** Each session of 25 throws began with its own calibration, run on the simulated rig as the routine of 3.2 describes; the controller took every number from it.
- **What limits it:**
  1. **The simulated fine tube is lumpier than the bench's.** Above 0.5 rev/s it pours nothing for up to 1.4 s and then drops it at once; the bench's longest was 0.2 s. So the trial holds the fine tube to 0.5 rev/s, which costs each throw about a second. Below that its gaps are still longer than the bench's (0.7 to 1.2 s against 0.2 to 0.8 s). This is hardest on proposal 2.
  2. **The simulated scale shows a small move sooner than the bench's** (nine in ten within 2 to 2.5 s; the bench: 4 s). This flatters proposal 1's time (the table's second row).
  3. **AR2209's record is two digits harsh** where a finish slows through 0.2 rev/s, and its throws' spread at rest is double the bench's (this afternoon). Both proposals' AR2209 figures lean to the harsh side.
  4. **The simulated coarse tube stops more tightly than the bench's** (a spread of 0.05 to 0.13 gr against 0.12 to 0.5 gr). On the bench the coarse margin would be nearer 1 to 2 gr and the fine tube's stretch 1 to 2 s longer.
  5. **No draught, no vibration, no second unit.** And the other scales are stand-ins: nothing here is measured on any scale but the A&D.
- **What the bench then showed the simulation has wrong** (2026-11-21, afternoon; each measured, none yet mended in the records):
  1. **Its small moves are too kind.** One to three digits from the smallest move 32 times in 100, where the unit gave 21; four and more from the middle move 32 times, where the unit gave 46. This is why proposal 1's forecast failed.
  2. **Its reading rises 0.1 s later after a coarse start** than the unit's, at the same lag by a stop. The record splits the scale's delay and its filter's lag by an assumption, and only their sum was pinned. So it could not show the forecast counting a coarse stream twice.
  3. **Its coarse tube pours less a turn at speed** than the unit's did that day (8.4 against 9.5 to 9.8 gr at 2 rev/s).
  4. **Its stream's stop was right:** every figure of proposal 2 but one fell inside the forecast.

## 6. What the bench itself says, with no model

| The bench found (`../legacy/progress.md`) | What rests on it |
|---|---|
| Powder follows turns, not speed: one line through 348 trials, 0.757 gr a turn with AR2209; the same with BM2 | Counting powder in turns (2.1) |
| What arrives after a stop is 1.4 s of the flow at the slowest flows and 0.6 s at the fastest | The lag as a table by flow |
| A single small change takes the A&D 2 s to show half the time and 4 s nine times in ten | The waits; why moves are slow |
| A held coarse tube drops nothing late; its reverse quarter turn spills 0.6 to 1.1 gr | No reverse turn |
| A finish by nudges, drawn from the recorded nudges: AR2209 80% at best, 74 to 78% at waits of 2 to 4 s; BM2 88 to 90%, 82 to 87% | Proposal 1's ceiling |
| Moves of 0.03 of a turn give nothing two times in three, and four digits or more in 14% (AR2209) and 31% (BM2) of those that give; moves of 0.06 of a turn give four or more in 40% and 19% | The smallest move stays small |
| Stops from 0.02 rev/s, by the PC's clock: three digits hold 90% of 30 (AR2209), 75% of 20 (BM2); from 0.05 rev/s 54% and 75% | Proposal 2's promise, and why its creep's speed matters |
| Today's loop set from the measurements: 10 over of 12 with AR2209 | The simulation's 85% over |

## 7. What I lack, and the session that would settle the ranking

- **The trial on a real unit was run the same afternoon, with AR2209** (the first section; `trial_guide.md` has the tool and the session). **What is lacking now is the same trial with BM2:** an hour, its forecast written. It says whether the ranking turns for a fine powder too.
- **The record of AR2209 mended from the trial's own moves and starts** (section 5's last list), so that what is designed next is tried on moves as unkind as the unit's.
- **No recording on a second scale.** Nothing says how the owner's G&G shows a stream against a small change but his own account. The calibration's steps 3, 5 and 6 on it, through the same PC tool, would give its row.
- **No throw of today's loop with BM2** (the forecast of 2026-11-21 stands unchecked), and AR2209's stops in blocks by speed (this afternoon).

## 8. What neither proposal does, and why

- **No oscillation, no vibration, no reverse turn.** Nothing recorded shows any of them helping: back and forth at four swings a second was no more even than steady turning, and a reverse spills and then owes more than it took back. They remain the only road to one over in fifty within 0.02 gr for a powder like AR2209, and that is a question for the tube and the firmware's motor task, not for this controller.
- **No use of the once-a-turn wave.** The fine tube pours unevenly round its turn, and the step count says where it is; a move or a stop timed to the turn could be tighter. It needs the wave learned at each start, and is left until the plain finish is proven.
- **No use of a scale's "stable" mark.** Four of the nine drivers' frames carry one and no driver reads it. It would shorten the waits on those four and do nothing for the other five, so nothing here depends on it.
- **Nothing from GRT or any table of powders.** Its densities do not give what a turn pours, and it holds no kernel size (`sources/grt_two_powders.md`).
- **No PID.** The research said why on 2026-11-19 (`README.md`, point 3), and the bench did not change it. Today's loop on a scale twice as late: every throw over.

## 9. What changes in the research's proposals of 2026-11-19 (`../legacy/README.md`, D1 to D7)

- **D1 (one demand, shared):** kept in spirit, simpler in form: each tube has the same law on the same forecast room, with its own margin.
- **D2 (the finish is nudges):** it is proposal 1, with the stop placed by the forecast and the moves measured. Proposal 2 puts a creep before it.
- **D3 (the scale and the powder kept apart):** kept, as the three parts of 3.2. The lag is a table by flow and not one delay.
- **D5 (the verdict on a reading at rest):** kept, on the lowest reading shown.
- **D6 (automatic tuning, by the unit or by an agent):** kept. Its two minutes are eleven in the trial.
- **New:** the forecast by parcels, each with the lag of its flow; no reverse turn of the coarse tube; the rehearsals; a stop aimed under the target; the probe that sets the calibration's own waits.
- **New after the bench:** the lag measured over a run; the margin that grows with the coarse tube's unseen powder; the fine tube's stop on its own powder alone; the return to the stream; a move made to a turn by the step count. And as a direction only: the finish chosen by the calibration from its own moves and stops.
