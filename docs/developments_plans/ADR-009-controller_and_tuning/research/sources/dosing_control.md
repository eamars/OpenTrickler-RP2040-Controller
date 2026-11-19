# Dosing control for a powder trickler: what industry, theory and other dispensers do

Desk research, 2026-11-19. No hardware touched, no repository changed.

How to read the labels:

- **[Vendor]** a manufacturer's manual or product page says it.
- **[Patent]** a patent or application describes it. A patent describes a method, not necessarily the firmware that ships.
- **[Paper]** peer-reviewed or textbook result.
- **[Forum]** users' reports. Anecdote, not measurement.
- **[Mine]** my own reasoning or arithmetic. Check it before relying on it.
- **[Memory]** I know it but could not re-open the source in this session.

---

## 1. Bottom line

1. **Recommend architecture B**: flow commanded in proportion to the remaining weight, with the gain taken from the scale's measured delay; no I term, no D term; the last few kernels by nudge-and-wait on settled readings; one learned "spill" number per tube corrected after every throw. It is today's P term kept, and everything around it replaced.
2. **The accuracy is decided by the finish, not by the approach.** Every published design that claims better than 0.1 gr ends with pulses and waits (RCBS MatchMaster, the Frankford/Battenfeld patent, the SuperTrickler application, and the "jog" of Mettler Toledo and Siemens). Continuous slow turning to the end cannot reach ±0.02 gr on any scale that lags: the powder that lands after the decision has a spread of its own.
3. **The physical floor is the clump, not the controller.** With one kernel of 0.02 gr equal to the whole tolerance, a release of three kernels at the end is an overcharge whatever the control law. If a tube cannot release two or fewer kernels per nudge about 97% of the time, no software meets "±0.02, never over" for that powder. A 15-second nudge test measures this and gives a pass or fail per powder.
4. **Dither verdict:** pulsed nudges with a pause are established practice. High-frequency dither on a rotating tube: I found no evidence for or against in this geometry. The physics is plausible (vibration breaks arches), a competitor filed for a vibratory last stage, but vibration next to a force-restoration balance is a risk. Treat it as a bench experiment judged by the same nudge test, not as a design assumption.
5. **Integral action belongs between throws, not inside one.** That is exactly how Siemens, Mettler Toledo and HBM do it: a fraction of the last throw's error corrects the next throw's cutoff.

---

## 2. The problem in control terms

**[Mine]**, from the task description and a read of `C:\workspace\OpenTrickler-RP2040-Controller\src\charge_mode.cpp` (lines 553 to 604).

The cup is an integrator: mass in the cup is the sum of what fell. The input is one-sided: flow cannot be negative. So an overshoot cannot be corrected, and the controller's whole job is to never commit more powder than there is room for.

Three things land after the controller decides to stop:

- **The blind window.** The reading is old by the scale's filter lag, its report interval and the serial delay. Call the sum θ. Powder that fell during θ is not in the reading yet.
- **In flight and let-through.** Powder in the air and kernels on the tube's lip that fall after the motor stops.
- **The crossing clump.** The drop that takes the weight past the stop point is a whole kernel or a whole clump, never a fraction.

Today's loop keeps the fine tube turning at no less than its minimum speed until the reading is within 0.03 gr of the target (line 553, line 590). So at the moment of the stop, the tube is still feeding at its minimum flow, and the reading is θ old. What lands afterwards is about (minimum flow × θ) plus a kernel or clump. The fixed 0.03 gr allowance covers that only when minimum flow × θ is small. An illustration: 0.05 gr/s × 0.35 s is 0.018 gr on average with a spread of about 0.02 gr, which puts roughly one throw in eight over +0.02. On a scale with a 1.5 s lag the same tube lands 0.075 gr late. This is the root of "overcharge is the common failure", and it gets worse with scale lag, with a higher minimum speed, and with clumping.

The broken invariant, in one line: **at the instant the controller commits to stop, the powder still to land, plus a safety multiple of its spread, must be less than the room left.** Everything below is a way of keeping that true.

The I term cannot help. The error is positive for the whole throw, so the integral only grows; it adds speed late, which is where speed hurts. Windup here is not a fault case, it is the normal case.

---

## 3. Industrial practice for gain-in-weight filling

### 3.1 The common pattern

**[Vendor]** Every batching controller I read uses the same sequence:

1. **Tare** on a still scale.
2. **Coarse and fine feed together** until a coarse cutoff, then **fine alone** until a fine cutoff. Rice Lake's 920i FlexWeigh says it plainly: fast and slow on until the dribble weight, then fast off and slow on until the target less the preact ([datasheet](https://core.instrumart.com/assets/RiceLake-920i_101-datasheet.pdf)). This is what the firmware already does with its two tubes.
3. **The fine cutoff is before the target** by the amount expected to land afterwards. Names: preact (Rice Lake, Hardy, Rockwell), spill (Mettler Toledo), trailing weight (Siemens), residual flow (HBM), in-flight.
4. **Wait for standstill**, then **check the tolerance**.
5. **If under: jog.** Short pulses of the fine feed, each followed by a wait for standstill, until in tolerance.
6. **After the throw: correct the cutoff** from the final error, for the next throw.

### 3.2 The concrete algorithms

**Siemens SIWAREX FTA** ([device manual, sections 5.5, 5.15, 5.16](https://cache.industry.siemens.com/dl/files/155/17970155/att_919578/v1/Manual_FTA_en_211.pdf)) **[Vendor]**

- Fine shut-off point = set value − trailing weight. Coarse shut-off point = set value − fine weight − trailing weight.
- **Proportional controller, run to run:** G(n+1) = G(n) + (A − S) × C/100%, where G is the trailing weight, A the last net weight, S the set weight, C a control factor in percent.
- **Maximum one-time correction:** the change per throw is capped, so one wild throw cannot wreck the cutoff.
- **Controller Optimum Plus/Minus:** a band around the target inside which no correction is made. It stops the learner chasing noise.
- **Fine time controller:** a second learner moves the *coarse* cutoff so that the fine phase lasts a set time: F(n+1) = F(n) + K × D_fine × (t_set − t_actual). So the final error trims the fine cutoff, and the fine phase's duration trims the coarse cutoff. One scalar each, no ambiguity about which cutoff was wrong.
- **Automatic post dosing:** after standstill, if the weight is below set − TU1, either a continuous fine signal or **pulse mode**; the pause between pulses is the standstill wait.
- A choice of what the learners do after a faulty throw: limit, or reset.

**Mettler Toledo IND570 Fill-570** ([technical manual](https://mt.com/dam/ind/IND_PDF/2024/ind570(xx)/ind570/technical-manual/30205348_C_MAN_TM_Fill-570_EN.pdf)) **[Vendor]**

- **Auto spill adjustment:** the error averaged over 1 to 9 cycles, 1 to 99% of it applied to the spill value. The manual's advice: average more and apply less for accuracy over time; fewer and more for speed.
- **Auto jog:** when the settled weight is below the lower tolerance, the feed is pulsed. Pulse time 0.1 to 9.9 s, to be set so short that one pulse cannot carry the weight across the whole tolerance band. Pause time 0.1 to 9.9 s, to be set long enough for the scale to come to rest so the tolerance can be judged.
- **Learn mode:** during a normal first fill, at a test point (10 to 90% of target) the feeds are stopped, the spill is measured, the feed is run again for a set time and stopped again, and the fine feed and spill values are filled in. No separate calibration run.

**HBM WTX120** ([manual, mirrored](https://www.manualslib.de/manual/743822/Hbm-Wtx120.html?page=103)) **[Vendor, third-party host]**

- Optimisation in three strengths: correction factor 1, 0.5 or 0.25 of the last error into the next cutoff.
- **Redosing does not change the cutoff points.** The learner uses the result of the main feed, not of the top-up.
- Lockout times after each valve change (about 10% of that phase's time) during which the weight is not judged: the scale is ringing.
- A residual flow time before the check weighing, set from the feeder and the filter's settling time.

**Mettler Toledo Q.iMPACT "Predictive Adaptive Control", from the Procter & Gamble patent US 6,380,495 (Ash, Chappell, Ivey, 2002)** ([Google Patents](https://patents.google.com/patent/US6380495)) **[Patent]** This is the most useful single source for us.

- Flow Q is estimated from the filtered scale readings.
- **Predicted spill = K1 × Q + K2 × Q².** Feed stops when weight + predicted spill reaches the target.
- K1 is a time: **filter time constant + valve let-through − (initial downward velocity / g)**. So K1 is the blind window and the let-through added together, expressed as seconds of flow. K2 covers the momentum of fast vertical streams and is zero for slow or horizontal feeds.
- After each feed, the actual spill gives a new point (Q, spill), and K1, K2 are refitted by recursive least squares with a forgetting weight (they report 0.17 for new data).
- **A gate on what is learned:** a new point inside the usual cluster updates the model; a point outside the reference box is ignored; a consistent shift resets the model. A disturbed throw does not teach.
- Mettler's own notes add a "Spill Only" mode with no prediction, meant for very slow feeds and for feeds whose flow is erratic (search summary of the [PAC database notes](https://ds-rgr.mt.com/dam/product_organizations/industry/Load_Cells/Downloads/Transmitter/ind780qi/documentation/PACDatabaseNotes.pdf); the document itself refused my fetch).

**Others** **[Vendor, via search summaries]**: Hardy's HI 1756-nDF averages its auto-preact over a set number of fills and ignores flow early in the fill; Rockwell's PlantPAx PDOSE has a self-tuning preact, a dribble stage and a "bump" for under-tolerance. **[Memory]** Rice Lake's 920i setpoints have a preact "learn" mode with an adjustment fraction, a stabilisation wait and a count of batches between corrections; I could not open the manual (file too large) to confirm the names.

### 3.3 Tolerance against speed

**[Vendor + Mine]** The trade is the same everywhere: the tighter the tolerance, the lower the flow has to be at the cutoff, because what lands after the cutoff scales with the flow (K1 × Q) and so does its spread. Industry buys speed back in three ways: a high coarse flow with an early coarse cutoff, a fine phase held to a set duration (Siemens), and a jog for the remainder so that the fine cutoff can be set safely short. The AutoTrickler manual states the user's version of it: an occasional overshoot costs a few seconds to put right and may be worth a faster cycle overall ([manual, p. 16](https://cdn.shopify.com/s/files/1/0525/7235/6768/files/AutoTrickler_V4.pdf)).

### 3.4 What does not carry over

**[Mine]** Industrial fills are thousands of particles past the tolerance: the spill is a smooth quantity and its average is all that matters. Here the tolerance is one kernel. The learned preact removes the *average* error; it cannot remove the kernel-sized spread. That is why the jog is not optional for us, and why Mettler's "Spill Only for erratic flow" note is the regime the fine tube's end lives in.

---

## 4. Control theory that fits

### 4.1 Dead time and lag

- **[Paper]** A proportional loop around an integrator with a dead time θ, de/dt = −k·e(t−θ), is free of overshoot only while k·θ ≤ 1/e ≈ 0.37, and unstable beyond k·θ = π/2. (Classical result for this delay equation; any dead-time text, for example Normey-Rico and Camacho, *Control of Dead-time Processes*, 2007.)
- **[Mine]** That gives the tuning rule that replaces Kp: **commanded flow = 0.37/θ × (room left)**. With θ = 0.35 s the gain is about 1 per second: from 1 gr out to 0.05 gr out takes about 3 s. With θ = 1.5 s the gain is 0.25 per second and the same stretch takes 12 s. A scale's lag sets the speed limit; no tuning beats it without prediction.
- **[Paper]** A Smith predictor (Smith 1957; for integrating processes Åström, Hang and Lim, IEEE TAC 39(2), 1994) removes the delay from the loop by acting on a model's estimate of the present. For us that is dead reckoning: mass now ≈ last reading + estimated flow × θ. This is the same thing as P&G's "weight + K1 × Q".
- **[Mine]** Prediction raises the safe gain to roughly 1/θ and beyond, limited by how wrong the flow estimate can be. So prediction matters on slow scales and hardly at all on the A&D.
- **[Paper/Patent]** A Kalman filter with states (mass, flow) and the scale's lag as a third state is the full version. K-Tron's loss-in-weight feeder patents do exactly this: US 4,775,949 (Kalata, 1988) estimates weight and mass flow from noisy weight, and inflates its noise model by a smooth function of the normalised residual so that a disturbance (a refill, a bump) does not corrupt the flow estimate; US 4,954,975 adds self-tuning of the noise parameters ([patent](https://patents.google.com/patent/US4775949A/en)). Sinchai et al. (ICASSP 2014, pp. 2189–2193) use a Kalman filter to predict the cutoff setpoint of a filling weigher whose overshoot varies cycle to cycle (abstract only seen).

### 4.2 Fastest approach that cannot overshoot

- **[Paper]** For an integrator with a bounded one-sided input, the time-optimal law is bang-bang: full flow until the "stopping distance" equals the room left, then stop. Real systems soften the end into a braking curve that is proportional near the target (proximate time-optimal control: Workman 1987; Franklin, Powell and Workman, *Digital Control of Dynamic Systems*). Reference governors (Gilbert, Kolmanovsky and Tan 1995; survey by Garone, Di Cairano and Kolmanovsky, Automatica 2017) are the general tool for "never violate a constraint".
- **[Mine]** Our stopping distance at flow Q is K1 × Q plus a margin for its spread. Turned round: **allowed flow = (room left − margin) / K1**. That is "speed proportional to remaining error", with the proportionality fixed by the measured K1 rather than tuned, and a **margin in kernels** instead of a floor speed. When the allowed flow falls below the lowest flow the tube delivers regularly, continuous feeding ends and nudging begins. A floor speed that keeps turning, as today, breaks the invariant of section 2.
- **[Mine]** Gain scheduling by remaining weight is what the commercial dispensers' three or four speed bands are (section 5). The formula above is the same schedule with one parameter instead of six.

### 4.3 PID, properly, and why not

- **[Paper]** A sample-rate-independent PID integrates error × dt, differentiates over dt with a filter, and clamps or back-calculates the integral when the output saturates (Åström and Hägglund, *Advanced PID Control*, 2006).
- **[Mine]** Even done properly: the plant already integrates, so there is no steady offset for an I term to remove inside a throw; the error never changes sign, so anti-windup has nothing to unwind; and a D term on a 10 Hz reading quantised at about one kernel is mostly noise. A flow estimate from a short regression over readings (as the Battenfeld patent and P&G do) is the usable form of the derivative. **[Forum]** The DIY community has reached the same place by trial: a forum reply advises leaving Ki at zero and tuning only Kp and Kd.
- **[Mine]** And gains tuned on one scale cannot carry to another, because the safe gain is 0.37/θ.

### 4.4 Learning across throws

- **[Paper]** Run-to-run control with an exponentially weighted correction (Sachs, Hu and Ingolfsson, IEEE Trans. Semicond. Manuf., 1995; Moyne, del Castillo and Hurwitz, *Run-to-Run Control in Semiconductor Manufacturing*, 2001) and iterative learning control (Bristow, Tharayil and Alleyne, IEEE Control Systems Magazine, 2006; Wang, Gao and Doyle, J. Process Control, 2009) are the theory behind section 3.2. The Siemens formula *is* the EWMA run-to-run controller.
- **[Mine]** What to learn: K1 per tube (the spill per unit flow), and a scale factor on the flow map. What not to learn: a full speed trajectory (ILC proper). The disturbance here is random kernel arrival, which does not repeat, and ILC only removes what repeats.
- **[Vendor]** Guards that industry uses and we should copy: a cap per correction, a dead band, averaging over a few throws, no learning from a topped-up or disturbed throw.

### 4.5 Random kernels: the stopping rule and the floor

- **[Paper]** If drops arrive at random (a compound Poisson stream: events at rate λ, each of random mass X), the mass landing in a blind window θ has mean λθ·E[X] and variance λθ·E[X²]. The overshoot past a threshold by the crossing drop has a limiting mean of E[X²]/(2·E[X]) and is bounded by Lorden's inequality, E[overshoot] ≤ E[X²]/E[X] (Lorden 1970; Gut, *Stopped Random Walks*, 2009).
- **[Paper]** Where to aim under an asymmetric cost is the "canning problem": Hunter and Kartha (J. Quality Technology, 1977), Bisgaard, Hunter and Pallesen (Technometrics 26, 1984), Golhar and Pollock (1988). The answer has the newsvendor form: aim low by enough that the probability of an over equals the ratio of the cost of one more step to the cost of a reject.
- **[Mine]** Consequences:
  - Continuous feeding at flow Q with blind window θ leaves a spread of √(Q·θ·E[X²]/E[X]) that no preact can remove. For single 0.02 gr kernels at 0.05 gr/s and θ = 0.3 s that is 0.017 gr; at θ = 1 s, 0.032 gr. To get that spread under 0.01 gr needs Q·θ under 0.005 gr, less than a kernel a second on the A&D, and the crossing kernel adds its own spread on top: in the sketch below it takes 0.01 gr/s, a kernel every two seconds, to bring overs down to 5%. At that point it is nudge-and-wait anyway, done badly.
  - With nudge-and-wait there is no blind window: the decision is made on a settled reading. The only error left is the crossing drop. The rule "stop at the first settled reading inside the band; nudge while below it" is optimal when in-band costs nothing, because another nudge can only risk an over.
  - The price of "never over" is a small low bias: about half a kernel (−0.007 gr in the sketch below).
  - Nudge size is the newsvendor choice: large nudges while four or more kernels out, the smallest reliable nudge inside two. RCBS's patent does exactly this ("long trickle" scaled by distance, "short trickle" fixed).
  - **The floor:** the chance of an over is about the chance that the crossing nudge releases more kernels than the band is wide. For 0.02 gr kernels that is three or more. For 0.04 gr sticks one kernel is the whole band, and ±0.02 is at the edge of what is physically possible even with perfect single release.

### 4.6 A sketch in numbers

**[Mine]** Monte-Carlo, `C:\Users\rba90\AppData\Local\Temp\claude\2fb2dc2951acbaff\dfd06104-7deb-4a2e-ac3a-225f22b90f42\scratchpad\research\sim\finish_sim.py`. Kernel 0.02 gr with 15% spread; clumps geometric. It shows orders of magnitude only; the real clump statistics are unknown until measured.

Continuous finish, the cutoff set to the exact average of what lands late (the best any learned preact can do), single kernels:

| Flow at cutoff | Blind window | Spread of final error | Share over +0.02 |
|---|---|---|---|
| 0.20 gr/s | 0.3 s | 0.036 gr | 28% |
| 0.05 gr/s | 0.3 s | 0.019 gr | 15% |
| 0.05 gr/s | 1.0 s | 0.032 gr | 25% |
| 0.02 gr/s | 0.3 s | 0.013 gr | 8% |
| 0.01 gr/s | 0.3 s | 0.010 gr | 5% |
| 0.01 gr/s | 2.0 s | 0.021 gr | 17% |

Nudge-and-wait finish, stop at the first settled reading no more than 0.02 gr under target:

| Drops per nudge (average) | Clump size (average) | Mean error | Share over +0.02 | Share under −0.02 |
|---|---|---|---|---|
| 0.3 | 1 | −0.007 gr | 2% | 0 |
| 0.7 | 1 | −0.003 gr | 8% | 0 |
| 1.5 | 1 | +0.005 gr | 21% | 0 |
| 0.3 | 2 | +0.016 gr | 32% | 0 |
| 0.3 | 3 | +0.039 gr | 51% | 0 |

Read: nudging beats the best possible continuous finish by a wide margin, never ends under, and is ruined by clumping. Clump size is the number to measure first.

---

## 5. How existing reloading dispensers do it

| Product | What is published | Kind of source |
|---|---|---|
| **AutoTrickler V4** (Adam MacDonald) | Two tubes on belts. Large tube up to about 50 gr/s, ramping down to a grain or two short of the target; the small tube finishes with what the vendor calls a "ramping curve that takes calibrated flow rates into account" (AutoTrickler product page). A one-minute **Calibrate** primes both tubes and measures each tube's natural flow at constant speed; the small tube's should be 0.10 to 0.20 gr/s, corrected by *tilting the housing with shims*, not by software. One Speed slider sets three things: large tube speed, small tube speed, and the **transition weight** where the small tube takes over. Stops within ±0.02 gr; resumes if the cup is between 0.10 gr and target. Needs the A&D at FAST response and 20 readings a second. The manual's own account of overshoot: with too little powder in the tube there are gaps, the motor speeds up to make up for them, and clumps then fall unpredictably; with too much, several kernels fall together even at cautious settings. | [Product page](https://autotrickler.com/pages/autotrickler-v4), [manual, Oct 2021](https://cdn.shopify.com/s/files/1/0525/7235/6768/files/AutoTrickler_V4.pdf) **[Vendor]** |
| | About 10 s a charge; overthrows reported from hardly any to nearly a third of throws with Varget and 8208; a later firmware added a buffer setting to slow down earlier for large kernels. | **[Forum]**, search summaries |
| **RCBS MatchMaster** | Manual: user parameters for the large tube's stop point (E), the small tube's stop point (F), **long trickle** rotation (100 to 1000 ms) and response wait (250 to 2500 ms), short trickle start (H), **short trickle** rotation (25 to 250 ms) and wait (250 to 2500 ms). | [Manual p. 16](https://www.manualslib.com/manual/1793244/Rcbs-98941.html?page=16) **[Vendor]** |
| | Patent: a **Learn** process per powder (classify the powder from one timed run; five speeds of 5 s each with 5 s pauses to find the flow at each speed, noting that flow rises with speed only up to a point and then levels off or falls; five more around the best). In a throw: a ~3 s sample to measure the flow, switch points to medium, slow and trickle **computed from the measured flow and the gap** and recomputed as it goes; a powder-dependent **safe stop point**; then partial rotations with pauses, resumed only when the scale is stable. No learning between throws described. | US 12,326,325 B2, Dunning and Hemeyer, RCBS, 2025 ([Google Patents](https://patents.google.com/patent/US12326325)) **[Patent]** |
| | Claimed ±0.04 gr in match mode. Times: under 7 s tuned (Ultimate Reloader, 38.5 gr H4350); 48 s in match mode in another test (all4shooters, R901). | [UR](https://ultimatereloader.com/0lx9) and search summary |
| **RCBS ChargeMaster** | Three hidden transition points, by default 15.68, 3.42 and 1.08 gr before target (fast, medium, slow, trickle). Tuned sets reach 10 to 15 s with more overthrows. The larger tube lets powder heap up and overthrows by 0.1 to 0.2 gr; the fix users found is mechanical, a straw in the tube. | [benchrest.com thread](https://benchrest.com/forum/threads/rcbs-chargemaster-parameters-and-how-to-speed-it-up.82003) **[Forum]** |
| **Frankford Intellidropper** (Battenfeld / AOB) | Patent: optional calibration (prime 4 s, timed run, adjust duty until the rate is in a band; fast, medium, slow). In a throw, **timed open-loop cycles**: amount = gap − a stage offset; run time = amount ÷ stored rate. During a run, after 0.4 s, a **linear regression of weight against time** estimates the real rate and moves the end time. Then a settle wait. Finish: **pulses of about 18 ms with a scale read after each**. Measured rates overwrite the stored ones each cycle; after an over-dispense the stored duty is reduced. | US 11,473,890 B1 ([Google Patents](https://patents.google.com/patent/US11473890)) **[Patent]** |
| | Claimed ±0.1 gr; one owner: two overcharges in 150. | Search summary **[Forum]** |
| **Hornady Auto Charge / Pro** | Two user numbers: a trickle speed (factory 0.4 to 0.8, range 0.1 to 25.5) and a trickle start point 0.2 to 1.6 gr before target. Pro: SPEED and TIME, three presets, ±0.1 gr claimed, shows "Over". | [Hornady FAQ](https://www.hornady.com/support/faqs/how-do-i-adjust-trickle-speed-on-a-lock-n-load-auto-charge-with-a-serial-number-of-2840-or-above-to-prevent-overthrows) **[Vendor]** |
| **Lyman Gen 6** | ±0.1 gr claimed, about 2 gr/s. Nothing on its control found. | Search summary |
| **SuperTrickler** (Rex Petersen; software by Peter Kowald, a retired process-control engineer) | Application: a rotary first stage (stepper and auger) and a **vibratory feeder as the last stage**; vibration reduced from about 85 to 95% of target; optionally **pulsed vibration with the scale read in the gaps**; a tube sloping up by more than 10° so nothing falls after the stop. Abandoned in 2025. | US 2023/0358520 A1 ([Google Patents](https://patents.google.com/patent/US20230358520A1/en)) **[Patent]**; [AccurateShooter bulletin](https://bulletin.accurateshooter.com/2023/01/new-supertrickler-precision-powder-dispenser/) |
| **Ingenuity Precision trickler** | A slotted disc that singles out kernels, in place of the AutoTrickler V3's tube; 6 to 9 s a charge; jams with some powders (8208). | **[Forum]** |
| **Prometheus Gen II** | A tuned mechanical balance; ±0.03 gr over ten charges in the maker's own article; about 10 s a charge by users. How it meters the last kernels: not found. | [6mmBR](https://www.6mmbr.com/prometheus.html) (written by the maker), **[Forum]** |
| **Dandy / Omega** | Hand tricklers with two speeds; the Dandy auto version runs until a photo-beam on a beam scale is broken. No weight feedback. | **[Forum]** |
| **OpenPowder** (DIY, load cell) | Fast phase to 80 to 95% by step count from a grams-per-step calibration; a 300 to 500 ms pause; then bursts sized at 80% of what remains, at three falling speeds, to within 0.005 g. Calibration: three bursts of 400 steps. | Project README **[DIY]** |
| **Ammolytics Open Trickler**, **Powder-O-Matic** | PID on a PWM trickler motor; two steppers. Algorithms not documented in their READMEs. | Project READMEs **[DIY]** |

What the table says **[Mine]**:

- No commercial maker who publishes uses a PID on weight (one DIY project does). They use speed bands by remaining weight, a stop point before the target, and a pulsed finish.
- The two newest patents (RCBS, Battenfeld) both measure the **flow** during the throw and compute their switch points from it. That is P&G's K1 × Q idea, arrived at separately.
- Per-powder knowledge is a **flow calibration**, run by one button, taking about a minute (AutoTrickler, RCBS, Battenfeld).
- The product with the best reputation fixes flow that is out of range **mechanically** (tilt), and says so. Software is not expected to rescue a tube that delivers too much or too little.
- No measured overthrow rate for any product was found. All figures are owners' impressions.

---

## 6. Low-speed inconsistency, dither, and nudges

### 6.1 Why slow turning is erratic

**[Paper]** A slowly turning drum does not pour; it avalanches. At low rotation the bed is carried up, slips, and rests, again and again (the "slumping" regime); only above a transition speed does the surface flow continuously ("rolling"). Rajchenbach (Phys. Rev. Lett. 65, 2221, 1990); Henein, Brimacombe and Watkinson (Metall. Trans. B 14, 1983); Mellmann (Powder Technology 118, 2001). The transition depends on the material and the fill. Cohesive ("sticky") powders avalanche in larger lumps. Screw feeders show the same: discharge is irregular below about 2 rev/min, and single screws pulsate at low setpoints (bulk-solids handbook and Coperion troubleshooting article, search summaries).

**[Mine]** So "erratic at low speed" and "flow not proportional to speed" are one phenomenon, and it is the tube's physics, not the stepper's. It means each powder and tube has a **lowest speed at which flow is regular**, and continuous feeding should never be commanded below it. Below that, the choice is between irregular avalanches the controller cannot see coming, and deliberate small nudges it can.

### 6.2 Is dither used anywhere?

For:

- **[Paper]** Vibration lets grains pass openings that would otherwise jam, down to one grain diameter (Janda et al., EPL 87, 24002, 2009), by making arches less likely and breaking those that form (Mankoc et al., Phys. Rev. E 80, 011309, 2009).
- **[Vendor/Paper]** Vibratory trays are the standard industrial answer for a uniform fine discharge of free-flowing granules; vibratory micro-feeders dose pharmaceutical powders at 1 to 15 mg/s.
- **[Patent]** The SuperTrickler application puts a vibratory feeder at the end, at reduced intensity, optionally pulsed, and says (from experience, no data) that lowering the vibration near the target makes the flow more regular and easier to control. Hornady has a patent on a vibratory hand trickler (US 10,563,966, 50 to 225 Hz).
- **[Patent]** Moretto's US 6,981,619 doses plastic granules with a rotor that **swings back and forth** 40 to 60° in about 40 ms per cycle, each swing releasing a small pulse, stopped by a balance. Its argument against continuous screws: where the screw stops changes what falls. Its pulse is 0.1 g or more, five thousand times our kernel.
- **[Memory]** Ultrasonic vibration of a capillary acts as a valve for fine powders, dose proportional to the vibration's duration (Yang and Evans' group; review in Powder Technology 178, 2007).

Against, or missing:

- I found **no** published use of a small, fast angular dither superimposed on a slowly rotating tube, in tricklers or in industry, and no measurement of its effect on single-kernel release.
- **[Paper]** Vibration does not guarantee flow: Mankoc et al. report jams that continuous vibration cannot restart. More flow is not the same as more regular flow.
- **[Paper]** In a vibratory micro-feeder study, modulating the excitation with a PID gave an uneven powder bed, and *constant* settings dosed most accurately (AIChE 2015 abstract, Graz group).
- **[Mine]** The balance is the risk. A&D's own FAST/MID/SLOW response exists because the balance feels vibration; the AutoTrickler manual warns the platform must not move. Dither next to the pan may cost more in reading noise than it gains in flow.

**Verdict [Mine]:** do not design around dither. Design around nudges, which need nothing new, and test dither (and a forward-then-back "flick", which is Moretto's idea at our scale) as variants of the nudge on the bench. The judge is one number: the share of nudges that release three or more kernels.

### 6.3 Nudge-and-wait against continuous slow feeding

- **[Vendor/Patent]** Nudge-and-wait is what Mettler, Siemens, RCBS, Battenfeld and the SuperTrickler application all do at the end.
- **[Mine]** On a fast scale a nudge cycle is the nudge plus one settle, perhaps 0.4 to 0.6 s; three to five nudges cost 2 to 3 s. On a slow scale with a 1.5 s settle the same finish costs 5 to 8 s. That cost is real, but section 4.6 shows the alternative on a slow scale is not a faster good throw, it is a bad one.
- **[Mine]** The slower the scale, the more the continuous phase must leave for the nudges, because its landing spread grows with the lag. A slow scale is slower by physics. The controller should say so (show the expected time), not hide it by turning the tube into the blind window.

---

## 7. Adapting to scales

**One number per scale, measured once [Mine, after P&G's K1].** Run a tube at a steady flow for two seconds and stop it. Then:

- effective delay θ = (settled weight − the reading at the instant of the stop command) ÷ the flow just before the stop;
- settle time = how long until readings stay within one count.

θ lumps the filter lag, the report interval, the serial delay and the powder's fall. It is exactly what the controller needs, it needs no model of the scale's filter, and every throw re-measures it for free (the spill at the coarse cutoff). The report interval the firmware already measures (`scale_get_usual_interval_ms`).

**One controller for all scales [Mine].** The flow gain is 0.37/θ, the nudge pause is the settle time, and the margin before nudging grows with √θ. Nothing else changes between an A&D and a slow strain-gauge scale. A slow scale gets a slower throw automatically, with no per-scale tuning.

**Cheap gain [Vendor].** The AutoTrickler sets the A&D to FAST response and 20 readings a second. If the trickler runs the A&D at 10 a second, that setting halves part of θ for nothing. Worth checking against what the firmware's driver assumes.

**Real mass or a disturbed reading?**

- **[Mine]** Physics gives two free checks. Powder cannot leave the cup: a reading that *falls* by more than the noise is a disturbance. Powder cannot arrive faster than the tubes deliver: a jump larger than (maximum flow × interval + a clump) is a disturbance. Neither needs a model.
- **[Patent]** The model-based check is K-Tron's: compare each reading with the predicted one, and trust the reading less the larger the normalised residual. The simple form: if |reading − predicted| exceeds three times the usual scatter, hold the motors and do not use that reading.
- **[Memory]** A&D's stream marks each reading stable or unstable (ST/US). That flag is the balance's own motion detector and is the natural input for "settled".
- **[Vendor]** After the throw: when the cup comes back empty and the scale is not near zero, the last charge is suspect (AutoTrickler manual, p. 15). A zero check at cup return catches drift and draughts after the fact.

**Verification and top-up without chasing noise [Mine, from the vendors' jog rules].**

- Decide only on a settled reading. Stopping needs one; **adding powder needs two that agree**, a settle time apart. A draught rarely holds a false "under" steady for two windows, and the asymmetry matches the cost.
- Nudge only when the settled shortfall is outside the band. Inside the band, stop: another nudge can only make it worse.
- If the scale says "under" by much more than the controller's own account of what it delivered allows, wait and read again before adding: the model disagrees with the scale, and the scale is the one a draught can fool.
- The learner takes the first settled weight after the continuous phase, never the weight after nudges (HBM's rule), and skips a throw that was flagged disturbed (P&G's gate).
- An unsettled scale is not a reason to keep feeding. The firmware already has "Scale: unsteady"; the finish should pause into it.

---

## 8. Adapting to powders

**What a profile should hold [Mine]**, in place of six PID gains and four speed limits. Per tube unless noted:

| Field | What it is | How it is found |
|---|---|---|
| Flow map | Flow (gr/s) at four or five speeds | Calibration run |
| Highest useful speed | Where flow stops rising with speed (RCBS notes it levels off or falls) | Calibration run |
| Lowest regular speed | Below this the flow avalanches | Calibration run: the scatter of weight gained per interval |
| Spill time K | Seconds of flow that land after a stop, beyond the scale's θ | The stop at the end of each calibration step; then every throw |
| Kernel mass (fine tube) | The smallest repeatable gain per nudge | Nudge test |
| Nudge size | The angle that releases about one kernel in three nudges | Nudge test |
| Nudge quality | Share of nudges releasing three or more kernels | Nudge test: **the powder's pass or fail** |
| Trim | A correction of a few percent on the flow map, and on K | Run to run, capped |

The scale's θ and settle time live with the scale, not the powder. The margin (in kernels) and the tolerance live with the user.

**How industry gets these numbers [Vendor/Patent]:**

- A short open-loop routine started by one button: AutoTrickler's Calibrate (about a minute, both tubes at constant speed); RCBS's Learn (5 s on, 5 s off at each of five speeds, twice); Battenfeld's (prime, timed run, adjust until the rate is in a band).
- Or no routine at all: Mettler's Learn mode stops and restarts the feed in the middle of the first real fill; Battenfeld's uncalibrated mode starts from deliberately high rate guesses so the first cycles are short, and overwrites them with what it measures.
- Then refinement in normal use: a fraction of each throw's error into the cutoff (Siemens, Mettler, HBM), or a refit of spill against flow (P&G).

**A calibration for this machine [Mine]:** prime; each tube at four speeds for 3 s with a stop and settle between (flow map, K, and the scale's θ in the same run); then thirty nudges of the fine tube (kernel mass, nudge size, nudge quality). About a minute and a half, into a cup the user empties back into the hopper. A new profile could instead start from cautious defaults and learn in its first throws, as Battenfeld's does; the calibration is better because it ends in a verdict about the powder.

**Sticky powders and long sticks [Mine].** They show up as a high lowest-regular-speed and a poor nudge quality. The controller's honest answer is a larger margin and more, smaller nudges (slower), and when the nudge test fails, to say that this powder with this tube will not hold ±0.02. The commercial answer is mechanical: tilt (AutoTrickler), inserts that line kernels up (MK Machining's overserts), a notched disc (Ingenuity), a straw (ChargeMaster).

---

## 9. Three architectures, ranked simplest to most capable

### A. Stages with learned cutoffs and a nudged finish

Industry's bulk/dribble/jog, as Siemens and Mettler ship it.

- **How:** coarse and fine run at one fixed speed each. Coarse stops at a learned cutoff. Fine stops at a learned cutoff a few kernels short. Settle. Nudge until in band. After the throw: the first settled error trims the fine cutoff; the fine phase's duration trims the coarse cutoff (Siemens' two learners).
- **Needs to know:** two speeds, two cutoffs, a nudge size, the scale's settle time. All but the settle time per powder *and per scale*, because the cutoffs absorb the scale's lag.
- **Fixes:** overcharge (the finish is on settled readings); undercharge (top-up); erratic low speed (never runs slow); flow not proportional (only two speeds are used); printed-tube variation (learned).
- **Does not fix:** a change of target or scale needs a few throws to relearn; the first throws after a powder change are off; a throw where the flow differed from the usual lands wrong, because the cutoff is a fixed weight.
- **Cost:** small. Tuning burden near zero once it learns. Easy to explain: three numbers on a screen.
- **Slow scale:** the fine cutoff must sit far back (its landing spread is large), so there are many nudges with long pauses. Accurate, but perhaps 30 to 40 s.
- **Failure modes:** a learner taught by a disturbed throw (guard: cap, dead band, gate); fixed cutoffs wrong for small targets where the coarse tube never reaches steady flow.

### B. Flow by remaining weight, cutoffs from measured flow, a nudged finish, learned spill

P&G's prediction and the RCBS/Battenfeld patents, reduced to what this machine needs.

- **How:** both tubes run from the start, as now. Each is commanded a **flow**, not a speed: flow = gain × (room left − that tube's margin), through the flow map, capped at the highest useful speed. The gain is 0.37/θ. The coarse tube's margin is about a grain; the fine tube's is two or three kernels. A tube stops for good when its allowed flow falls below its lowest regular flow, or when reading + K × measured flow reaches its mark. Then settle, nudge on settled readings, and trim K and the flow map from the first settled error.
- **Needs to know:** per scale: θ and settle time. Per powder and tube: the flow map with its two limits, K, and the nudge figures. All from the one calibration. The user sets a tolerance and nothing else.
- **Fixes:** everything A fixes, plus: scale differences (θ is the scale's, the rest is the powder's, so a new scale needs no relearning of powders); flow not proportional (the map); target changes (the law is in terms of room left, not fixed weights); a throw whose flow is unusual (the cutoff follows the measured flow); and it yields the predicted weight that the disturbance check needs.
- **Cost:** moderate. More code than A: a flow estimate by regression, a flow map and its inverse, a calibration routine. But *fewer* settings than A, and none the user tunes. It is today's structure (two tubes, proportional to error) with the gain derived instead of entered, which lowers the risk of the change.
- **Slow scale:** degrades gracefully and predictably: the gain falls with θ, the margin grows, the throw takes longer. If the times on slow scales are unacceptable, add dead reckoning (act on reading + flow × θ), which raises the usable gain about threefold. Not needed for the A&D; build it only on evidence.
- **Failure modes:** a flow map that is wrong (hopper nearly empty, tube not primed, humidity): the in-throw flow measurement covers the coarse tube; the fine tube relies on the margin and the nudges, which is why the margin is in kernels and not zero. A flow estimate corrupted by a draught: gate it with the physics checks. The first throw after a refill: prime, as every vendor does.

### C. Full estimator, stochastic stopping, trajectory learning

- **How:** a Kalman filter on (mass, flow, scale lag) driven by commanded speed (K-Tron's patents); a stopping threshold computed from a fitted clump distribution and the cost ratio; iterative learning of the whole speed trajectory.
- **Needs to know:** everything B does, plus noise variances, a clump-size distribution per powder (dozens of throws to estimate), and cost weights.
- **Fixes beyond B:** principled rejection of disturbed readings; the stopping margin set by measured statistics instead of "two or three kernels"; slightly faster on slow scales.
- **Cost:** high. Kalman filters assume smooth Gaussian noise and this flow is lumpy, so the filter is wrong in the regime that matters. Nobody without the theory can tell why it stopped where it did. The gain over B is small, because B's residual error is already the clump, which no estimator removes.
- **Slow scale:** best in theory; most exposed to a wrong model in practice.
- **Failure modes:** silent ones: a mis-set variance that makes it over-trust the model or the scale; a learned trajectory fitted to one hopper level.

### Recommendation: B

Why:

1. **It attacks the root cause directly.** The stop is no longer taken while feeding into the blind window; the flow at any moment is what the room left can absorb.
2. **It separates the scale from the powder,** which is the only way "one controller, many scales, automatic tuning" can be true. A cannot do this: its cutoffs mix the two.
3. **It has the fewest things for the user to set:** a tolerance. Everything else is measured.
4. **It is the smallest step from today's code** that removes the cause: keep the two-tube proportional structure; replace Kp by 0.37/θ through a flow map; delete the I and D terms and the floor speed; add stop, settle, nudge; add one learner.
5. **C's extra capability buys almost nothing** until B's logs show a residual that a better estimator could remove.

A is the fallback, and B contains it: with the flow map flat and the gain infinite, B is A.

**Build order [Mine]**, each step ending in a pass or fail on a unit:

1. **The nudge test and the nudged finish**, alone. It decides whether ±0.02 is reachable for a powder and tube at all, and it is the part every architecture shares. Pass: fewer than about 3% of nudges release three or more kernels; a finish from 0.1 gr under ends in band with no over in, say, 50 throws.
2. **Calibration and the flow law** for both tubes. Pass: the continuous phase ends between two and five kernels short in nearly all throws on the A&D.
3. **The learner and the checks** (cap, dead band, gate, two-reading rule for top-up).
4. **Dead reckoning**, only if a slow scale's times are judged too long.

**What would change my mind:**

- **Towards A:** logs showing that the flow at a fixed speed varies throw to throw by more than about 30% even after priming. Then the flow map is fiction, the measured-flow cutoff is the only part of B that works, and fixed stages with learned cutoffs are as good and simpler.
- **Towards A:** a decision to support only the A&D. Then scale separation has no value and θ can be a constant.
- **Towards hardware, not C:** a nudge test that fails for common powders (three or more kernels released too often). No controller fixes that. The options are the tube's lip and tilt, an insert, a notched disc, or a vibratory last stage, each judged by the same test.
- **Towards parts of C:** B's logged errors showing structure that repeats (a drift with hopper level, a pattern over the first throws of a session), or draughts corrupting the flow estimate often enough that a weighted filter would pay.
- **Against the nudged finish:** a scale so slow to settle that five nudges cost more than a rejected throw now and then. The cost ratio in section 4.5 then favours fewer, larger nudges and an accepted overthrow rate; the structure stays.

One limit no architecture moves: the time of a large charge is the coarse tube's maximum flow. At 3 gr/s, 90 gr is 30 s before any finishing. The AutoTrickler's large tube is quoted at about 50 gr/s. "10 to 20 s" for the upper targets is a question for the coarse tube, not the controller.

---

## 10. What I could not find or verify

- **AutoTrickler's algorithm.** Only the vendor's phrases ("ramping curve", "calibrated flow rates") and the app's three sliders. No patent found. That it speeds up when flow drops is the manual's own statement; anything further about its internals would be a guess.
- **Ingenuity Precision, Prometheus, Lyman, Hornady internals.** Only forum descriptions and user-facing settings.
- **Whether the patents match shipped firmware** (RCBS, Battenfeld/Frankford, SuperTrickler). The SuperTrickler application was abandoned; whether the product uses pulsed vibration is unknown.
- **Any measured overthrow rate or error distribution** for any commercial dispenser. Everything is owners' impressions.
- **Any study of dither on a rotary tube trickler.** None found, for or against.
- **Coperion K-Tron and Schenck batching application notes.** Not retrieved; only K-Tron's Kalman patents and a fragment of a Coperion article.
- **Mettler's PAC database notes and the Rice Lake 920i manual text.** Fetches refused or too large; I relied on a search summary for the first and on memory for the second.
- **Mettler Quantos and Chemspeed dosing algorithms** (the nearest industrial relatives: milligram powder dosing onto a balance). Not published. A Fluid Management patent (US 7,527,078) describes an iterative powder dispense that reaches target in three or four passes with a safety margin and self-updating calibration; seen in summary only.
- **A&D's response figures** beyond roughly one second to stabilise at FAST, and the ST/US stability flag, which I state from memory.
- **The ultrasonic micro-feeding papers** (Yang and Evans' group): cited from memory; the search for them failed.
- **Two forum pages** (Sniper's Hide) refused the fetch; their content is from search summaries.
- **My own numbers** (the 0.37/θ gain's practical margin, the nudge counts and times, the Monte-Carlo) are reasoning, not measurement. The real θ of each scale, the real clump statistics of each powder and tube, and the real lowest regular speed are unknown until measured on a unit.
- Whether the firmware reads the A&D in grains (0.02 gr steps, so the tolerance is one count) or grams (0.001 g, about 0.015 gr) I did not check; it matters for how finely the band can be judged.

---

## 11. Sources

Vendor manuals and pages

- Siemens, SIWAREX FTA device manual: https://cache.industry.siemens.com/dl/files/155/17970155/att_919578/v1/Manual_FTA_en_211.pdf
- Mettler Toledo, IND570 Fill-570 technical manual (30205348 C, 11/2023): https://mt.com/dam/ind/IND_PDF/2024/ind570(xx)/ind570/technical-manual/30205348_C_MAN_TM_Fill-570_EN.pdf
- Mettler Toledo, PAC database notes (Q.iMPACT): https://ds-rgr.mt.com/dam/product_organizations/industry/Load_Cells/Downloads/Transmitter/ind780qi/documentation/PACDatabaseNotes.pdf
- HBM WTX120 manual (mirror): https://www.manualslib.de/manual/743822/Hbm-Wtx120.html?page=103
- Rice Lake 920i FlexWeigh 101 datasheet: https://core.instrumart.com/assets/RiceLake-920i_101-datasheet.pdf
- Hardy HI 1756-nDF manual (mirror): https://www.manualsdir.com/manuals/657625/hardy-hi-1756-ndf.html?page=26
- AutoTrickler V4 product page: https://autotrickler.com/pages/autotrickler-v4
- AutoTrickler V4 user manual (October 2021): https://cdn.shopify.com/s/files/1/0525/7235/6768/files/AutoTrickler_V4.pdf
- RCBS MatchMaster instructions, user-defined powder configuration: https://www.manualslib.com/manual/1793244/Rcbs-98941.html?page=16
- Hornady, trickle speed FAQ: https://www.hornady.com/support/faqs/how-do-i-adjust-trickle-speed-on-a-lock-n-load-auto-charge-with-a-serial-number-of-2840-or-above-to-prevent-overthrows
- MK Machining on the V4 and powders: https://www.mkmachining.com/why-your-autotrickler-v4-changes-behavior-between-powders/

Patents

- US 6,380,495, Ash, Chappell, Ivey (Procter & Gamble), 2002, material transfer with predicted spill: https://patents.google.com/patent/US6380495
- US 4,775,949, Kalata (K-Tron), 1988, weigh feeding with stochastic control: https://patents.google.com/patent/US4775949A/en ; US 4,954,975 (self-tuning)
- US 12,326,325 B2, Dunning, Hemeyer (RCBS), 2025: https://patents.google.com/patent/US12326325
- US 11,473,890 B1, Kinney et al. (Battenfeld / AOB Products), 2022: https://patents.google.com/patent/US11473890
- US 2023/0358520 A1, Petersen (Modgunn Security), abandoned 2025: https://patents.google.com/patent/US20230358520A1/en
- US 10,563,966, Haberman (Hornady), 2020, vibratory powder trickler: https://patents.google.com/patent/US10563966
- US 6,981,619, Moretto, 2006, oscillating metering rotor: https://patents.google.com/patent/US6981619
- US 7,527,078 (Fluid Management), iterative powder dispensing: https://patents.google.com/patent/US7527078

Papers and books

- Rajchenbach, Phys. Rev. Lett. 65, 2221 (1990); Henein, Brimacombe, Watkinson, Metall. Trans. B 14 (1983); Mellmann, Powder Technology 118, 251 (2001): drum flow regimes.
- Janda et al., EPL 87, 24002 (2009): https://arxiv.org/abs/0907.0114 ; Mankoc et al., Phys. Rev. E 80, 011309 (2009): https://arxiv.org/abs/0907.4871
- Sinchai, Saechia, Limpiti, Koseeyaporn, Wardkein, ICASSP 2014, pp. 2189–2193.
- Smith (1957); Åström, Hang, Lim, IEEE TAC 39(2), 1994; Normey-Rico and Camacho, *Control of Dead-time Processes*, 2007.
- Åström and Hägglund, *Advanced PID Control*, 2006.
- Franklin, Powell, Workman, *Digital Control of Dynamic Systems*; Gilbert, Kolmanovsky, Tan (1995); Garone, Di Cairano, Kolmanovsky, Automatica 75 (2017).
- Sachs, Hu, Ingolfsson, IEEE Trans. Semicond. Manuf. 8 (1995); Moyne, del Castillo, Hurwitz (2001); Bristow, Tharayil, Alleyne, IEEE CSM 26(3), 2006; Wang, Gao, Doyle, J. Process Control 19 (2009).
- Lorden, Ann. Math. Statist. 41 (1970); Gut, *Stopped Random Walks*, 2009.
- Hunter and Kartha, J. Quality Technology 9 (1977); Bisgaard, Hunter, Pallesen, Technometrics 26 (1984).
- AIChE 2015, "Accuracy of Micro Powder Dosing and Feeding Via a Vibratory Sieve-Chute": https://aiche.confex.com/aiche/2015/webprogram/Paper427621.html

Reviews and forums (anecdotal)

- ChargeMaster parameters: https://benchrest.com/forum/threads/rcbs-chargemaster-parameters-and-how-to-speed-it-up.82003
- Ultimate Reloader on the MatchMaster: https://ultimatereloader.com/0lx9
- AccurateShooter on the SuperTrickler: https://bulletin.accurateshooter.com/2023/01/new-supertrickler-precision-powder-dispenser/
- 6mmBR on the Prometheus (maker's article): https://www.6mmbr.com/prometheus.html
- OpenPowder: https://github.com/guiguijke/OpenPowder ; Powder-O-Matic: https://github.com/haklein/Powder-O-Matic ; Ammolytics Open Trickler: https://github.com/ammolytics/open-trickler-peripheral

Local files made in this research

- Simulation: `C:\Users\rba90\AppData\Local\Temp\claude\2fb2dc2951acbaff\dfd06104-7deb-4a2e-ac3a-225f22b90f42\scratchpad\research\sim\finish_sim.py`
- Extracted manual text: `C:\Users\rba90\AppData\Local\Temp\claude\2fb2dc2951acbaff\dfd06104-7deb-4a2e-ac3a-225f22b90f42\scratchpad\research\pdftext\` (`fta.txt`, `fill570.txt`, `at_v4.txt`)
