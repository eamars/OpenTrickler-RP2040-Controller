# The PID loop and the staged controller: what each does well, and how to combine them

ADR-009, gate 2, 2026-11-22. The owner asked, after the gate's proofs: the pros and cons of the new controller against today's PID loop; how accurate the calibration is; and how to combine the two into something fast, accurate, adaptable by calibration, and easy to tune and share, "not a one is better than another scenario". The PID loop is simulated with the owner's own hand-tuned gains, read on 2026-11-22 from his Pico W (192.168.2.168, its AR2209 profile, unchanged since 11-19).

Every figure here is measured, on the rig or in the simulation, and says which. The simulation is known to be kind at the finish: its stops scatter less than the rig's (sd 0.028 gr against 0.046 on the rig), so its share of good throws runs about ten points above the rig's. Its ranking of controllers and its times have matched the rig so far (series B against the simulation with the PC's lags: 18.0 s against 18.6 s).

## 1. What each does well, and where each fails

**The simulation, 300 throws of 10 gr each** (`tools/charge_sim/fit/study_20261122/compare.txt`; "slow" is the measured scale with every delay doubled and four readings a second):

| Rig | Scale | Controller | Good within 0.02 | Within 0.04 | Over (0.02) | Mean error | Median |
|---|---|---|---|---|---|---|---|
| AR2209 | measured | PID, the owner's Pico W gains | 10% | 22% | 90% | +0.112 gr | 6.2 s |
| AR2209 | measured | PID, the bench's stored gains | 0% | 0% | 100% | +1.437 gr | 2.8 s |
| AR2209 | measured | staged, glide at 0.02 | 72% | 83% | 26% | +0.016 gr | 12.5 s |
| AR2209 | measured | staged, glide at 0.04 | 64% | 91% | 17% | +0.002 gr | 10.0 s |
| AR2209 | measured | staged, jog at 0.02 | 76% | 89% | 15% | −0.002 gr | 15.0 s |
| AR2209 | slow | PID, the owner's | 0% | 0% | 100% | +1.547 gr | 4.8 s |
| AR2209 | slow | staged, glide at 0.02 | 67% | 81% | 30% | +0.024 gr | 26.9 s |
| BM2 | measured | PID, the owner's | 68% | 84% | 32% | +0.035 gr | 9.3 s |
| BM2 | measured | staged, glide at 0.02 | 90% | 97% | 10% | +0.003 gr | 13.0 s |
| BM2 | measured | staged, jog at 0.02 | 95% | 98% | 5% | −0.005 gr | 10.4 s |
| BM2 | slow | PID, the owner's | 0% | 0% | 100% | +0.781 gr | 7.1 s |
| BM2 | slow | staged, glide at 0.02 | 77% | 90% | 22% | +0.010 gr | 29.7 s |

On the rig (Pico 2 W, A&D FX-300i, the air-exposed AR2209, 10 gr): today's PID loop with nearly the owner's settings was over in 10 throws of 12 (2026-11-20); the staged controller was 80% good within 0.02 gr in 11.7 s (series D) and 80% within 0.04 gr in 9.2 s (series E).

**The PID loop**

- **Its strengths:**
  - It's fast: 6 to 9 s, as it never waits for the scale to settle before it acts.
  - It's simple: per tube, two gains that matter, a top and a floor, and two stop thresholds. People understand them and share them today.
  - It needs no calibration, and it uses no model of the powder: a powder that pours more or less a turn changes nothing in how it runs.
- **Where it fails:** it acts on the reading as if the reading were the cup, and the reading is half a second or more behind it. Everything in the air when it stops lands after it, so it is over by about the flow times the scale's lag. It shows in its mean error, +0.11 gr with the owner's gains on AR2209, and on a scale twice as late every throw is over.
- **What it costs to work around:** the only cure inside a PID loop is a low gain and a slow finish, tuned by hand for each powder, tube and scale together. The owner's gains, good on his own trickler, give 68% on BM2 and 10% on AR2209 on the bench's rig; the bench's own stored gains give 0%. Gains do not carry from one rig to another, and they mean nothing a person can measure.
- **Its finish:** it has none. A throw that stops short ends short.

**The staged controller**

- **Its forecast:** it acts on the reading plus the powder still in the air, counted from the turns and the lags the calibration measured. So it does not carry the PID loop's bias: the mean error is within a digit in every set above.
- **Its finish:** a verdict at rest, small top-ups, a return to the stream if far short, and a stop that learns from the throws.
- **Robust to a scale:**
  - On a scale twice as late it stays 67 to 77% good, recalibrated on that scale, where the PID loop falls to 0. The cost is time (27 to 30 s).
  - On the unit, every scale fault tried is met as designed: frames that do not parse, the scale quiet in the stream, the creep or a move, the scale lost, a stop, a driver fault (progress.md, 13:46).
- **Robust to its own calibration:** a wrong calibration costs time and hardly any accuracy (section 2).
- **Its weaknesses:**
  - It's slower: 10 to 15 s where the PID loop takes 6 to 9 s. The time is in waiting: a rest of 2.3 to 2.8 s before each judgement, and 2 to 4 s for each small move.
  - It needs a calibration for each powder on each unit (13 minutes and 170 gr today, by the PC; on the unit in gate 3).
  - It holds about forty numbers where the PID loop holds a dozen, and the code is larger (5 KB of heap during a charge).
- **Its limit is the powder:** with a clumpy powder its small moves are a lottery. The rig's AR2209 reaches about 70% within 0.02 gr whatever the finish.

**Both:** a cup lifted mid-throw is poured on at full speed, by both loops (progress.md, 13:46).

## 2. How accurate the calibration is

**As the rig's throws see it** (`bench/verify_series.py`, series A to E, 125 throws, against the trial's calibration made by the PC):

| Number | Calibration | The throws | Verdict |
|---|---|---|---|
| The lags (scale and powder) | as measured by the PC | 0.055 s shorter | wrong: the PC's own latency was in them |
| The coarse tube's grains a turn | 9.60 | 9.88 to 10.50 (+3 to +9% by series) | low |
| The fine tube's grains a turn, at the finish | 0.797 | 0.61 to 0.75 (−5 to −23%) | high |
| The coarse tube's spread from throw to throw | 6.1% | 4.7 to 6.2% | right |
| What still arrives after a stop from the creep | +0.027 gr, sd 0.028 | +0.026 to +0.051, sd 0.031 to 0.048 | the mean right, the spread too small by up to 0.02 gr |
| The forecast where the fine tube stops (the algorithm) | | rest − forecast −0.006 to +0.004 gr | right |

What is wrong was measured the wrong way: the PC's latency in the lags, and each tube's grains a turn taken from short runs from rest at 1 rev/s, where a throw runs the coarse tube from 2 down to 0.3 rev/s and finishes the fine tube at 0.03. Gate 3's calibration on the unit removes the first by itself, and the plan already has its rehearsals be a throw's own last stretch, which takes care of the others. `verify_series.py` on its throws is the check.

**What those errors cost, in the simulation** (glide at 0.02, `fit/study_20261122/calibration_errors.txt`):

| Calibration | AR2209 good | AR2209 median | BM2 good | BM2 median |
|---|---|---|---|---|
| Right | 72% | 12.5 s | 90% | 13.0 s |
| Coarse grains a turn 6% low | 74% | 12.0 s | 83% | 13.3 s |
| Fine grains a turn 15% high | 74% | 14.4 s | 84% | 14.7 s |
| Lags 0.055 s long | 73% | 18.2 s | 90% | 18.0 s |
| All three | 74% | 18.2 s | 84% | 19.8 s |

**So a calibration that is off makes the staged controller slow, not wrong.** The lags cost 6 s, as on the rig (series B: 18 s; series D, the lags and grains mended: 11.7 s). The grains a turn cost up to six points with BM2, within three standard errors with AR2209. The closed loop (the rest, the moves, the learnt stop) puts the weight right; an accurate calibration is what makes it fast.

## 3. How to combine them

**A test first: the PID loop fed the forecast.** The owner's gains, untouched, acting on the staged controller's forecast in place of the reading (`legacy_pid.ForecastPid`, `fit/study_20261122/forecast_pid.txt`):

| Rig, scale | PID on the reading: good / over / under (0.02) | PID on the forecast: good / over / under | Median |
|---|---|---|---|
| AR2209, measured | 10% / 90% / 0% | 42% / 10% / 48% | 6.2 → 9.6 s |
| AR2209, slow | 0% / 100% / 0% | 17% / 10% / 73% | 4.8 → 10.4 s |
| BM2, measured | 68% / 32% / 0% | 35% / 1% / 65% | 9.3 → 14.1 s |
| BM2, slow | 0% / 100% / 0% | 20% / 1% / 79% | 7.1 → 15.0 s |

The forecast takes the PID loop's overs away on every scale, the slow one too: the lag was its whole fault. But with no finish, what was over is now short. **The forecast is one half; a finish that tops up from a verdict at rest is the other.** That is the staged controller. So the combination is not two loops side by side. It is one loop that keeps from each what it does well:

1. **From the staged controller, the parts that make it accurate:** the forecast (the PID loop is blind without it), the verdict at rest, the top-ups, and the stop learnt from the throws.
2. **From the PID loop, its speed and its plainness:**
   - **The stream as a gain.** The staged stream is already proportional: its speed is the forecast's room over (grains a turn × a glide time). Written as a PID loop's gain, that is about 2.2 rev/s a grain for the fine tube and 0.3 for the coarse on this rig and powder. The owner's hand-tuned gains on the reading are 0.8 and 0.05: the forecast is what lets the gain be higher. Shown to a person as "gain", made by the calibration, the profile reads as today's does, and a person who knows PID tuning knows what a higher one does.
   - **The time spent waiting is where the PID loop wins.** It is the 2.3 to 2.8 s rest before each judgement and the 2 to 4 s wait after each small move. Both are set from the slowest stop the calibration watched, not from what the throw in hand shows. A rest judged when the forecast's unseen powder is under half a digit and the reading has stopped rising would end as soon as this throw allows. That is a design for the simulation first; not tried.
3. **Adaptable by calibration, in three parts that are each measured and shared on their own** (the owner's split of 2026-11-20: scale, tube, powder):
   - **The scale:** its lags by flow, its digit and its interval, how long it holds a small change, how long a stop takes to go quiet. One per make of scale, measured once and shared by everyone with that scale (seven or more makes, the owner, 2026-11-21).
   - **The tube:** its grains a turn by speed, and what a reverse spills. One per printed tube, with a short check on each unit.
   - **The powder:** its grains a turn against the tube's, what arrives after a stop and how widely, and its small moves. One per powder lot, shared like a load.
   - **The throws trim the rest.** A unit makes a calibration from the three, and trims each tube's grains a turn and its stop from its own throws, as it already learns its stop (series D: the throws' own grains a turn took 6 s off the median). Today only the stop is learnt; grains a turn from the throws is on the plan's list of ideas, not in a gate.
4. **Easy to tune and to share:**
   - **One routine for the user:** the calibration measures everything, and the user chooses the band and the finish. Nothing else needs a person.
   - **For a person who wants to look:** each number is a physical one ("0.72 gr a turn", "the scale is 0.52 s late"), and the gains are derived from them, not set beside them.
   - **Shared as files:** the scale's part by make and the powder's by lot, so a new user with a known scale and powder needs only the tube's check and a few throws.
   - **The PID loop itself:** it stays the fallback until gate 6 settles what is kept. With a forecast and no finish it ends short, so it is no fallback for accuracy.

## What this does not settle

- Whether the rest can end early, and what that buys: a design to try in the simulation, then the rig.
- Whether grains a turn trimmed from the throws is stable: series by series the coarse tube moved by 3% (9.88, 10.22, 10.18, 10.50), half its spread a throw. A trim needs a few throws, not one.
- The cup lifted mid-throw: both loops pour on it (progress.md, 13:46).
- The Pico W's own rig: its gains were simulated on the Pico 2 W's tubes. The owner's trickler is over about one throw in ten to fifteen on its own rig and powder (2026-11-19); on this rig its gains give 10% good with AR2209, 68% with BM2.
