# The legacy PID with the glide's learning: a proposal, the simulation, and the side by side on the rig

ADR-009, gate 2, 2026-11-22 evening. Not a plan: an experiment the owner asked for, built as the least that runs it. Every figure says where it was measured.

**The verdict, in short.**
- **In the simulation,** with no calibration at all, the owner's own PID gains on a forecast learnt from the throws came within 4 points of the glide (AR2209 at 10 gr: 85% against 89% good within 0.04 gr), 2.3 s slower.
- **On the rig it is worse,** in ABBA blocks of 10 at 10 gr: settled good 14 of 20 against the glide's 18, 5 overs against 2, 13.7 s against 10.9 s. It fails all three passes written before the run.
- **The cause:** without a calibration, the PID stops its fine tube at its least speed, 0.1 rev/s on this unit. The glide's calibration sets 0.058 from the scale's lag. The faster stop scatters 1.6 times as widely, which doubles the top-ups, and each top-up at that speed can release an over.
- **What the learning buys:** today's PID goes from about 20% good to 70 to 75% with no calibration. That makes it a fallback for a powder not yet calibrated, not a replacement for the glide.
- **Then, in a second run,** the PID's least speed was learnt the glide's way. It matched the glide's accuracy and was 4.5 s slower ("The second experiment").
- **A correction (19:40), from the rows by phase** (`bench/phase_times.py`): the whole 4.5 s is in the bulk, before the reading comes within 0.7 gr of the target. That is the coarse tube's settings: the owner's coarse kp of 0.05 and the 5 gr coarse stop, tuned on a lagging reading on another rig. The approach to the stop and the rest take the same time as the glide's. The first run's reading, "the gain is the price" and "the calibration's lag at every speed buys time", is not shown by these throws.

## The question

The owner, to the session before: can an agent pick up the existing legacy PID system, add the continuous learning the glide uses, and do as well, or maybe better? He asked this session to study the notes, propose a PID-based system with learning, design a side by side against the glide, and throw it on the rig.

What the notes already said (`pid_and_staged_2026-11-22.md`, `talk_2026-11-22.md`):
- The PID loop is fast because it never waits, and over because it acts on a reading half a second or more behind the cup.
- Fed the calibration's forecast, it is no longer over, but it ends short, since it has no finish.
- The "guarded PID" (the PID stream on the reading, the forecast stopping it, a top-up at the least speed) got 41% good at 0.02 gr with AR2209, against the glide's 72%.

All three used the calibration, or no learning at all. **What was never tried: a PID that needs no calibration, whose forecast is learnt from its own throws.** That is the one thing a PID-based system could offer that the glide cannot: no 13-minute calibration of 170 gr per powder. So the proposal is that.

## The proposal: PID with a learnt forecast (`controller=pid_learnt`)

**What stays of today's PID loop:**
- the profile's gains (kp, ki, kd for each tube), its least and top speeds;
- the coarse tube stopping short of the target by the coarse stop threshold, then its back-off;
- the fine tube running from the start, its speed set by its PID.

**What is added.** Each piece is a number of the scale's make, or learnt from the throws. None is measured by a calibration.

1. **The forecast** (a Smith predictor, the textbook mend for a loop with a dead time). The fine PID acts on the reading plus the fine tube's powder still in the air. That powder is its turns over the last 0.9 s (the A&D's lag at the fine tube's flows, measured on 11-21: 0.82 to 1.02 s), counted from its own steps, times **one learnt figure**, the grains a turn as that window sees them.
2. **The stop.** The fine tube stops when the forecast is a digit short of the target (the glide's own aim; half the band, if that is more). Today's fine stop threshold is not used. While the coarse tube's powder may still be landing (within 1 s of its stop), the fine tube pauses at the stop instead of stopping.
3. **One settled reading.** 2.5 s after the stop (the glide's rest on this rig), and no rise for 0.4 s. This is the verdict, as the glide's is.
4. **The top-up is the PID again.** Short by more than the band, the stream resumes: the PID gives the fine tube at least its least speed, stopped by the same forecast; then a rest again. Nothing new: the owner's own idea of 11-22 ("continue to top up at minimum speed, with prediction until reaching +stop threshold").
5. **The learning.** At each throw's first rest, the figure is learnt from what landed after the fine tube stopped, over the turns the window held then. It is a mean of the throws so far, with the newest weighing at least 15% (the glide's share). A fresh powder is learnt in a throw or two, then followed as it drifts.
   - The figure starts on purpose high. The forecast then holds too much powder, so the first throw stops short and the top-up mends it, and a fresh powder never starts over.
     - The firmware's default is 1.0 gr a turn, set from the simulation.
     - On the rig 1.0 was not high enough, and the blocks ran from 2.0 (Results). If the idea is kept, the default becomes 2.0.
   - The figure is kept across charges, for as long as the unit runs. It is not stored, and it starts again if the powder profile changes.

**What it does not need:** a calibration, a moves table, a lag table, a stop table. Its numbers are the profile's PID settings, the scale's lag and rest (one figure each, per make of scale), the band, and the figure it learns.

## The simulation (`tools/charge_sim/pid_learning_study.py`, `fit/study_20261122/pid_learning.txt`)

300 throws a case, in sessions of 25. Each session starts the PID's learning afresh, so the learning-in is in the figures. Judged at the true weight, at the fast band (0.04 gr). The owner's gains are his Pico W's AR2209 profile (fine kp 0.8, least 0.08 rev/s, top 2; coarse kp 0.05). The simulation is kind at the finish (`pid_and_staged_2026-11-22.md`).

| | AR2209 10 gr: good / over / median | AR2209 5 gr | BM2 10 gr | BM2 5 gr |
|---|---|---|---|---|
| Glide, fast, calibrated | 89% / 11% / 10.1 s | 91% / 9% / 18.6 s | 97% / 3% / 9.1 s | 99% / 1% / 8.7 s |
| PID as today, owner's gains | 22% / 78% / 6.2 s | 25% / 75% / 5.7 s | 84% / 16% / 9.3 s | 83% / 17% / 7.8 s |
| PID on the reading, its stop learnt (the literal port) | 54% / 46% / 8.1 s | 54% / 46% / 7.6 s | 90% / 10% / 11.1 s | 91% / 9% / 9.6 s |
| **PID on the learnt forecast, owner's gains** | **85% / 14% / 12.4 s** | **81% / 19% / 11.0 s** | **97% / 3% / 15.4 s** | **97% / 3% / 13.6 s** |
| The same, the bench unit's stored gains (fine kp 2, top 5) | 42% / 58% / 8.6 s | 42% / 58% / 7.0 s | 71% / 29% / 8.3 s | 73% / 27% / 6.9 s |

**What it says:**
- **Accuracy without a calibration: nearly.** With AR2209, 85% good against the glide's 89% at 10 gr, and 81% against 91% at 5 gr. With BM2, the same as the glide.
- **The literal port fails.** Today's loop with the glide's stop learning on the reading halves the overs and no more. The learning takes away the PID's average over, not its scatter, because the flow at its stop changes from throw to throw. The previous session argued this; the run agrees.
- **The forecast is what works**, and learnt from the throws it is good enough.
- **The gain is the price.** At the owner's gains it is 2.3 s slower than the glide at 10 gr (5 to 6 s with BM2, whose figure is lower). At 5 gr it is 7.6 s faster than the glide, whose fine tube is held to 0.5 rev/s where the PID's runs at 2.

**What was tried and dropped:**
- **A higher gain** buys time and costs accuracy, whatever else is done (AR2209 10 gr: kp 0.8 85% / 12.4 s; 1.2 74% / 10.9 s; 1.6 70% / 10.0 s).
- **The P term aimed short of the stop**, by what one lag at the least speed pours, so that any gain reaches its least speed a lag before the stop: no better (80% / 12.5 s at kp 0.8, 73% / 10.5 s at 1.6).

**Why the gain costs accuracy (the root cause).** One figure over a fixed window is right only at the slow speed where it is learnt. At 2 rev/s the powder in the air is not that figure times the turns. So a PID without a calibration must come into its stop slowly, and its gain is the price. **What the calibration buys the glide is the lag at every speed**, so it can come in fast and know what is in the air. That is gate 3's to measure, in the firmware's own time. A PID could have it too, from the same calibration, but then it is the glide with other names (`pid_and_staged_2026-11-22.md`, section 3).

## The side by side on the rig

**The rig:** the Pico 2 W, the A&D FX-300i, the air-exposed AR2209, as thrown all day. 10 gr, the fast band (±0.04 gr), each throw also judged at ±0.02.

**Each as a user would run it:**
- the glide as left: fast mode, the profile's calibration with what the day's charges carried into it, its learning on (`series.py --mode fast --keep-calibration`);
- the PID with learning from a fresh start (its figure at 1.0), with the owner's Pico W gains written into the profile for the session (live, never stored, the stored ones put back at the end).

**The order, ABBA in blocks of 10:** glide 10, PID 10, PID 10, glide 10. The rig drifts (a second between two series an hour apart, `HANDOVER.md` item 7), and ABBA cancels a steady drift. The PID's two blocks follow on, so its second block is its learnt state. Each block is one charge, a series in the same cup.

**Judged alike:** each throw by its weight at rest (the controller's verdict) and by its settled weight (the next throw's base less its own); time from the throw's start to its rest. Both controllers end the same way, at a settled reading, so their times compare.

**The passes, written before the run** (for 20 throws each; a difference of one throw is 5 points):
- **The same as the glide:** settled good within 0.04 gr at most 2 throws fewer than the glide's (10 points), overs at most 1 throw more, median time at most 2.5 s longer (the simulation's 2.3 s and some).
- **Better:** as many good or more, no more overs, and quicker.
- **Worse:** otherwise, with the cause named from the rows.
- **Expected, from the simulation:** "the same", slower by about 2 s. The PID's first throw of its first block should stop short and be topped up. A fresh powder starts short, never over.

**What 20 throws can and cannot tell:** a gap of 20 points shows; one of 5 does not. The verdict is for this powder, rig and band, not for BM2.

## The firmware (built for this experiment)

- `controller=pid_learnt`, a third choice of `settings/charge controller`, in the same filler byte (no stored layout changes).
- `src/charge_pid_learnt.c`: the controller, the simulation's `LearntPid` in C, run every millisecond from the unit's step count.
- Console:
  - `cpid` lists its figure and constants;
  - `cpid gr_turn=<x> lag_s=<x> rest_s=<x> ...` sets them live, never stored;
  - `cpid reset` puts the figure back to its prior.
- Each throw prints a `Charge: throw ...` line in the staged controller's form, and its rows go to `ccrec`, so `series.py` throws it (`--controller pid_learnt`).

## Results: the rig, 18:25 to 18:39

**Before the blocks, two dry throws** (18:25, `logs/adr009/20261122_182521_series_pid_dry`): it runs, 12.0 and 13.2 s, both within 0.04 gr. **One prediction failed there.**
- The learnt figure came out at 1.3 to 1.5 gr a turn, above the cautious start of 1.0, so the first throw was not short (+0.040). The rig's A&D holds a small change back longer than the simulation's scale, so a turn's powder takes longer to show at the stop's low flow than 0.9 s.
- So the PID's blocks started from 2.0 (`cpid gr_turn_prior=2`, live), to keep the intent that a fresh powder starts short, never over. P1's first throw then stopped 0.18 gr short and was topped up.

**The blocks.** 10 gr, judged at ±0.04 gr. The glide as it threw best at 10 gr today (`fine_top_rps=1.0`, its fine-figure learning off, the stop's learning and carry on). The PID with the owner's Pico W gains. `bench/abba_look.py` sums them from each series' rows:

| Block | Rest good / over | Settled good / over | Median | First rest: mean, sd | Topped up | Fine tube at its stop |
|---|---|---|---|---|---|---|
| G1, glide | 9 / 1 | 8 / 2 | 11.2 s | −0.032, 0.045 | 3 | 0.058 rev/s |
| P1, PID from fresh | 9 / 1 | 8 / 1 | 14.3 s | −0.040, 0.085 | 5 | 0.100 rev/s |
| P2, PID learnt | 6 / 4 | 6 / 4 | 13.5 s | −0.028, 0.062 | 4 | 0.100 rev/s |
| G2, glide | 10 / 0 | 10 / 0 | 10.7 s | −0.014, 0.044 | 1 | 0.058 rev/s |
| **Glide, 20** | **19 / 1** | **18 / 2** | **10.9 s** | −0.023, 0.045 | 4 | 0.058 |
| **PID with learning, 20** | **15 / 5** | **14 / 5** | **13.7 s** | −0.034, 0.073 | 9 | 0.100 |

Recordings: `logs/adr009/20261122_182641_series_abba_g1`, `_182929_series_abba_p1`, `_183254_series_abba_p2`, `_183611_series_abba_g2`. P2's run ended `VERDICT FAIL`: the runner missed one `ccrec` reply after its last throw. All ten throws' rows are in the recording, and the log has no assert, panic or halt.

**Against the passes:**
- settled good: 14 against 18. The bar was at most 2 fewer; it is 4 fewer. **Fail.**
- overs: 5 against 2. The bar was at most 1 more. **Fail.**
- time: 13.7 against 10.9 s. The bar was at most 2.5 s longer; it is 2.8 s. **Fail.**
- **Verdict: worse.** The legacy PID with learning does not do as well as the glide on this rig, in accuracy or in time.

**Why, from the rows:**
1. **The PID stops its fine tube at 0.100 rev/s; the glide stops at 0.058.**
   - The owner's profile asks 0.08 rev/s. Today's loop raises it to the motor's own least speed, 0.1 on this unit (`get_motor_min_speed`).
   - The glide's least speed comes from its calibration: the band over the scale's lag.
   - The powder in the air at the stop scales with that speed, and so does its scatter: the first rests spread by 0.073 gr against 0.045.
2. **The wider spread costs time and overs.**
   - Aimed a digit short, more first rests are short beyond the band: 9 throws of 20 topped up against 4, at 4 to 5 s each.
   - The top-up is the stream again at the same 0.1 rev/s, so it ends with the same spread: 2 of the 5 overs came from a top-up after a short first rest.
3. **The simulation was kind,** as known: it ran the fine tube's least speed at the profile's 0.08 and scattered less at the stop. It gave 85% against the glide's 89%; the rig gives 70% against 90%.

**What the learning did achieve:**
- With no calibration at all, today's loop went from 22% good at 0.04 gr on this rig (the simulation with the owner's gains; 10 of 12 throws over on the rig on 11-20) to 70 to 75%.
- A fresh powder started short and was topped up, not over (from the prior of 2.0).
- The figure settled within a session (2.0, 1.0, then 0.9 to 1.0 by throw 5).
- So a PID with a learnt forecast is a working fallback for a powder not yet calibrated. **It is no replacement for the glide.**

**What this does not settle:**
- **Whether a lower least speed would close the gap.**
  - The simulation at 0.03 rev/s gave 87% against 85%, at 0.3 s more (AR2209, 10 gr).
  - On the rig the PID's spread is mostly the stop's speed, so it would narrow the gap in accuracy and widen it in time.
  - A slower stop buys accuracy with time, which the glide already does by its calibration.
  - The motor's least speed is a setting of the motors (Settings › Motors), not the PID's.
- **BM2**, where the simulation had the two equal in accuracy.

**Found on the way, not mended** (the experiment's code stays only if the owner keeps the idea):
- **A charge on the simulated scale teaches the PID's figure.** `charge_cycle` with `pid_learnt` moved it from 0.82 to 0.76. The glide's carry refuses the simulated scale; this does not.
- **The learnt figure is in RAM only,** lost at a restart.

## The second experiment: the least speed learnt (2026-11-22 night)

**Why:** the first rig run named the cause. The PID stopped its fine tube at 0.1 rev/s (the motor's least speed), the glide at 0.058, and the first rests spread 1.6 times as widely. The learnt figure already holds what the glide's calibration uses to set its least speed: the powder in the air at a speed is the figure × the lag × that speed. So the PID's least speed is set the glide's way, with no calibration: **the speed at which the powder in the air is the band** (`cpid floor_band_share=1`). It never goes above the profile's least speed (with the motor's), and never below 0.02 rev/s. On the rig's figure of about 0.9 that is about 0.05 rev/s. The cautious start's default is now 2.0, as the first run threw it.

**What it tests:** whether one learnt figure and one lag can stand in for the calibration's lag table at the stop. If they can, a PID-shaped controller needs a far smaller calibration. If not, the lag at every speed is what the calibration buys.

**The simulation** (`pid_learning_study.py --night`, `fit/study_20261122/pid_learning_floor.txt`, 300 throws a case, judged at 0.04 gr):

| | AR2209 10 gr: good / over / median | AR2209 5 gr | BM2 10 gr | BM2 5 gr |
|---|---|---|---|---|
| Glide, fast | 89% / 11% / 10.1 s | 91% / 9% / 18.6 s | 97% / 3% / 9.1 s | 99% / 1% / 8.7 s |
| PID learnt, least 0.1 (as the rig ran) | 85% / 15% / 11.9 s | 86% / 14% / 10.7 s | 97% / 3% / 15.5 s | 95% / 5% / 13.6 s |
| PID learnt, least speed learnt | 85% / 15% / 12.3 s | 86% / 14% / 11.6 s | 95% / 5% / 15.5 s | 96% / 4% / 13.7 s |

- The unit's own C gives the same within the noise (in the same file).
- **The simulation cannot judge this one:** its stops scatter less than the rig's, so the speed of the stop hardly moves its accuracy. It says the change does no harm and costs 0.4 to 0.9 s. The rig judges the accuracy.

**The passes, written before the rig** (the same blocks as the first run: glide 10, PID 10 from a fresh start, PID 10 learnt, glide 10; 10 gr; fast):
- **The mechanism:**
  - the PID's fine tube stops at 0.06 rev/s or slower (the median of its throws);
  - its first rests spread by 0.055 gr or less (the first run: 0.073; the glide: 0.045).
- **Against the glide, the bars of the first run:** settled good at most 2 throws fewer, overs at most 1 more, median at most 2.5 s longer.
- **Expected:**
  - the mechanism passes;
  - accuracy comes close to the glide's (16 to 18 settled good of 20);
  - the time fails, 3 to 4 s longer, because the P approach to a slower floor is longer and the glide's aim-short offset is what removes that.

**The rig, 19:10 to 19:22** (build 2.0.3-170-dirty at 50d7511; the same blocks; `bench/abba_look.py`):

| Block | Settled good / over | Median | First rest: mean, sd | Topped up | Fine tube at its stop |
|---|---|---|---|---|---|
| G1, glide | 9 / 1 | 11.2 s | +0.002, 0.052 | 2 | 0.058 rev/s |
| P1, PID from fresh, least speed learnt | 9 / 1 | 14.4 s | −0.012, 0.041 | 2 | 0.043 |
| P2, the same, learnt | 7 / 3 | 16.5 s | −0.012, 0.056 | 4 | 0.032 |
| G2, glide | 6 / 4 | 11.0 s | +0.014, 0.068 | 2 | 0.058 |
| **Glide, 20** | **15 / 5** | **11.0 s** | +0.008, 0.059 | 4 | 0.058 |
| **PID, least speed learnt, 20** | **16 / 4** | **15.5 s** | −0.012, 0.048 | 6 | 0.036 |

Recordings: `logs/adr009/20261122_191001_series_night_g1`, `_191245_series_night_p1`, `_191617_series_night_p2`, `_192001_series_night_g2`. P2 ended `VERDICT FAIL` on the runner's missed `ccrec` reply again; all its rows are in the recording.

**Against the passes:**
- **the mechanism: passed.**
  - The fine tube stops at 0.036 rev/s, against the first run's 0.100.
  - The first rests spread by 0.048 gr (bar 0.055), where the first run's spread by 0.073.
- **settled good: passed,** 16 against the glide's 15.
- **overs: passed,** 4 against 5.
- **time: failed,** 15.5 against 11.0 s, 4.5 s longer (bar 2.5 s).
- **The rig was rougher tonight for both.** The glide's spread was 0.059 against 0.045 in the first run, and its last block had 4 overs, two of them clumps of +0.10 and +0.12. The ABBA order shares that out between the two; the figures compare within this run, not with the first.

**What it says:**
1. **For accuracy, one learnt figure and one lag are enough.** With its least speed set from what it learnt, the PID with no calibration is as accurate as the glide on this rig and powder. So for accuracy, the lag table and the stop tables of the calibration are not needed.
2. **The time is lost in the bulk, by the coarse tube's settings.** Corrected at 19:40: the first reading of this section said the P approach and the lag by speed cost the time; the rows by phase say otherwise (`bench/phase_times.py`, medians):

   | | Bulk, to within 0.7 gr of the target | Approach, to the fine tube's stop | Rest and top-ups |
   |---|---|---|---|
   | Glide, 20 | 3.20 s | 4.60 s | 2.58 s |
   | PID, least speed learnt, 20 | 7.77 s | 4.47 s | 2.54 s |
   | (first run) glide / PID, least 0.1 | 3.03 / 6.78 s | 4.66 / 3.60 s | 2.58 / 2.54 s |

   - The glide runs its coarse tube at 2 rev/s until its forecast is 1.44 gr short.
   - The owner's coarse kp of 0.05 holds the coarse tube at 0.1 to 0.25 rev/s, and it stops 5 gr short (the coarse stop threshold). The fine tube at 2 rev/s pours most of the throw.
   - Both settings were right for a PID on a lagging reading on his own rig.
   - The learnt figure and the least speed do pull on each other. As the least speed fell, the figure rose (1.18 to 1.39 over P2), because the A&D holds a small change back longer at a lower flow. That cost about 1 s: P2's approach took 5.14 s against P1's 4.22 s.
3. **For the design:**
   - The accuracy comes from the predictor and the finish: the forecast, the stop's speed set from it, one settled reading, the learning.
   - On these throws the time comes from the coarse tube's gain and its stop, not the finish.
   - The next test is the owner's option 1: the coarse tube's PID settings made from the calibration. That means a coarse kp from its grains a turn and glide time, and a coarse stop from its room, with the coarse tube's own powder in the forecast. If that closes the 4.5 s, a PID-shaped controller with an auto-tuner matches the glide on this rig.
   - The pull between the figure and the least speed needs a bound or a figure by speed (about 1 s here).

## The third experiment: the coarse tube set from the calibration (the owner's option 1, 2026-11-22 night)

**Why:** the second run's 4.5 s was all in the bulk, from the owner's coarse settings (kp 0.05, a stop 5 gr short), tuned on a lagging reading on another rig. The owner chose to test option 1: the PID's numbers made from the calibration.

**What changes,** the coarse tube only (the fine tube stays as the second run: the owner's gains, its least speed learnt):
- **Its PID numbers from the unit's calibration:**
  - kp = 1 / (coarse grains a turn × coarse glide time) = 1 / (10.194 × 0.320) = 0.306;
  - its stop at the coarse room, 1.43 gr;
  - between the glide's coarse floor and top, 0.3 and 2 rev/s.
- **Its powder in the forecast** (`cpid coarse_gr_turn=10.194 coarse_lag_s=0.51`, the calibration's lag at 2 rev/s): until now the PID's forecast held only the fine tube's.
- **The glide's margin for it** (`cpid coarse_margin_share=0.164`, 3 × the calibration's spread a turn): both tubes act as if that much more of the coarse tube's unseen powder were coming.
- **The back-off off** (`coarse_stop_backoff_enable=false`, live), for the reason below.

**Found in the simulation on the way, and mended before the rig:**
- **The fine figure ran away** (to 4 to 8 gr a turn; throws stopped 5 gr short). With the coarse tube fast, much of what landed after the fine tube's stop was the coarse tube's. Now:
  - the coarse tube's unseen powder at that moment is taken off before the fine figure learns;
  - a throw whose margin on it is more than the band teaches nothing.
- **The back-off spills.** The rig measured that reversing the coarse tube drops 0.83 gr of AR2209 on average (`records/pico2w_ar2209.json`, `coarse.reverse_spill_gr`, from the 11-21 coarse stops).
  - With a 5 gr coarse margin that never mattered; with 1.43 gr it is an over.
  - The simulation, AR2209 at 10 gr: 83% good with the back-off, 89% without. The glide has none.
  - The frame's way to keep a reverse is to count its spill in the forecast, a measured number (`controller_design_draft.md`).

**The simulation** (`pid_learning_study.py --coarse`, `fit/study_20261122/pid_learning_coarse.txt`, back-off on; the back-off's effect checked apart, 300 throws a case):

| 10 gr, judged at 0.04 gr | AR2209: good / over / median | BM2 |
|---|---|---|
| Glide | 89% / 11% / 10.1 s | 97% / 3% / 9.1 s |
| PID, least speed learnt (the second run's) | 85% / 15% / 12.3 s | 95% / 5% / 15.5 s |
| PID, coarse from the calibration, back-off on | 83% / 17% / 10.0 s | 97% / 3% / 9.5 s |
| PID, coarse from the calibration, back-off off | 89% / 11% / 11.0 s | 98% / 2% / 10.1 s |

The unit's C agrees with the Python on 900 throws (79% and 78%, back-off on, AR2209 at 10 gr, another seed).

**The passes, written before the rig** (the same blocks: glide 10, PID 10 from a fresh start, PID 10 learnt, glide 10; 10 gr; fast):
- **The mechanism:** the PID's bulk (to within 0.7 gr) has a median at most 1 s longer than the glide's (the second run: 4.6 s longer).
- **Against the glide, the same bars:** settled good at most 2 throws fewer, overs at most 1 more, median at most 2.5 s longer.
- **Expected, from the simulation:**
  - the mechanism passes;
  - accuracy equal within the noise;
  - the median about 1 s longer (P1's cautious first throw adds to it);
  - **all three bars pass.**

**The rig, 19:51 to 20:04** (build 2.0.3-173-dirty at 104fbd2; `bench/abba_look.py`, `bench/phase_times.py`):

| Block | Settled good / over | Median | First rest: mean, sd | Fine tube at its stop | Bulk / approach / rest |
|---|---|---|---|---|---|
| G1, glide | 7 / 3 | 11.8 s | −0.010, 0.051 | 0.058 rev/s | |
| P1, PID coarse from the calibration, fresh | 9 / 1 | 14.2 s | −0.020, 0.050 | 0.028 | |
| P2, the same, learnt | 10 / 0 | 15.4 s | +0.004, 0.034 | 0.028 | |
| G2, glide | 9 / 1 | 10.9 s | −0.012, 0.040 | 0.058 | |
| **Glide, 20** | **16 / 4** | **11.2 s** | −0.011, 0.045 | 0.058 | 3.2 / 4.4 / 2.6 s |
| **PID, coarse from the calibration, 20** | **19 / 1** | **14.8 s** | −0.008, 0.043 | 0.028 | 6.2 / 5.3 / 2.5 s |

Recordings: `logs/adr009/20261122_195132_series_coarse_g1`, `_195425_series_coarse_p1`, `_195812_series_coarse_p2`, `_200144_series_coarse_g2`. Before it the owner confirmed the cup had room; the scale's zero no longer told what was in it.

**Against the passes:**
- **settled good: passed, and better than the glide,** 19 against 16;
- **overs: passed, and better,** 1 against 4;
- **the mechanism: failed,** the bulk 3.0 s longer (bar 1 s);
- **time: failed,** 3.6 s longer (bar 2.5 s).

**Why the bulk stayed long,** from the rows (P1, throw 4):
- **The coarse tube stopped early.** At 0.5 s the forecast stood at 8.3 gr, the coarse tube's stop at 8.57, and the coarse tube stopped at 0.74 s. What it had poured landed at about 6.8 gr.
- **The forecast was high by the fine tube's powder in the air at speed.** The fine tube ran at 2 rev/s and was forecast with the figure learnt at its creep: 1.48 gr a turn of its window, so about 2.7 gr in the air, where about 1 gr was (0.8 gr a turn × about 0.6 s).
- **The A&D lags longer at a low flow,** so a figure learnt at the creep overstates the powder in the air at speed. Over P2 it climbed further (1.48 to 2.06) and the least speed fell to 0.028 rev/s.
- **The result is a forecast that errs high:**
  - every stop comes early and slow, which is why accuracy is the best of the night (first rests' spread 0.034 in P2) and the time the worst;
  - the fine tube then made up 2 to 3 gr at its P approach.
- **The simulation missed it:** its scale does not lag longer at a low flow as the A&D does. Its learnt figure stays near 0.75 gr a turn where the rig's runs from 1.2 to 2.1.
- **So this time the lag at every speed does cost time:** the fine tube's powder in the air at speed has to come from the calibration's lags by flow, the glide's predictor. The learnt figure would then trim only the creep.

**What the three runs say together:**
- **Accuracy is the frame's.** The forecast, a stop at a least speed set from it, one settled reading and the learning gave the PID settings the glide's accuracy or better in both later runs, with or without a calibration.
- **Time is the predictor's at speed.**
  - With the owner's coarse settings the bulk was slow by design (5 gr short at 0.1 to 0.25 rev/s).
  - With the calibration's coarse settings it is slow because the one learnt figure overstates the fine tube's powder at speed.
  - The next test is the owner's option 2 proper: the PID law on the glide's own predictor (the calibration's figures and lags by flow, as `charge_control.c` keeps them in parcels), the learning trimming them.
- **A reverse spills, measured:** 0.83 gr of AR2209 a back-off. Keeping today's back-off in the frame means counting that in the forecast; until then it is off where the coarse tube stops close.
