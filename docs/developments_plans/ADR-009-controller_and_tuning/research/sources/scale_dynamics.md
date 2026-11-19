# How the supported scales respond in time

Desk research and code reading, 2026-11-19. Nothing was run on a unit, a serial port or the network; no repository was changed. Code read at branch `adr008_gate1`, commit 9e8975d (the scale drivers are the same on `origin/main` but for 4 lines of `scale.c`).

Every number carries a tag:

- **[D]** documented by the maker (manual or paper, cited);
- **[M]** measured by a named outside source;
- **[P]** measured by this project (ADR-013's progress);
- **[C]** read in this repository's code;
- **[E]** my estimate, to be replaced by a bench measurement;
- **[?]** looked for, not found.

## The short answer

1. **The three behaviours are three separate numbers, and a simulation needs all three per scale:** the time between readings (`T_out`), the delay of the value behind the pan (`L`, dead time plus filter lag), and the share of frames missing. Only `T_out` is known for any scale today (A&D, measured). `L` is published by nobody for small additions; it has to be measured, and section 5 gives a way that needs no powder.
2. **What a controller needs is one figure per scale and setting: `L`, in seconds.** A filtered reading trails a steady flow `q` by `q x L`. That product is the overthrow a lagging scale causes, and the amount to stop early by.
3. **`L` is probably not one number.** Balances switch filters: wide open while the weight moves a lot, closed when it moves a little. Radwag's manual says so in words, Mettler patents it, and A&D's own figure of 1.1 s to settle a 3 kg step to 0.01 g cannot be a fixed filter. So `L` at coarse flow is short and `L` at fine trickle is longer. The identification must measure `L` at several flows.
4. **A quarter second of the G&G's lateness is our own driver, not the scale** [C]. `gng_scale.c` sends its request, reads at once (so it reads the answer to the request before), and sleeps 250 ms. Each value is about 250 ms old when the charge gets it, on top of the scale's own lag. Reading the reply some tens of ms after asking would remove most of it. Not tried on a unit: a suggestion for ADR-009.
5. **Two traps in the A&D's settings** [D]: choosing response MID or SLOW silently sets the output back to 5 readings a second; and zero tracking (`trc`), which the maker says to turn off for "a very light sample", is mentioned in neither OpenTrickler's nor AutoTrickler's set-up guide.

## 1. From the code: how each driver gets its readings

Common to all [C]: one task per driver at priority 9 looks at the UART every `poll` ms and reads what has arrived (the Pico's 32-byte receive FIFO holds it meanwhile). A whole frame is handed to `scale_publish_measurement()`, which wakes the charge loop by a semaphore; the charge loop runs once per reading. Stored serial settings: 19200 baud and 8N1 by default, 4800/9600/19200 and 8N1/7N1 selectable (`scale.c`); there is no parity setting. The scale's "usual interval" is the median of the last 16 intervals between published readings, quantised by the poll period (`scale_get_usual_interval_ms()`); a charge pauses after 3 of them without a reading and ends after 10, never under 1 s (`charge_mode.cpp`, ADR-013).

"Transport age" below is the time from the scale starting to send a frame to the charge loop being woken: the frame's time on the wire plus 0 to one poll period. It does not include the scale's internal lag (sections 2 to 4). "Oldest in use" adds one reading interval: how old the value is just before the next one replaces it.

| Driver (file) | How | Poll | Frame | Flag or unit in the frame | Used? | Reading rate | Transport age | Oldest in use |
|---|---|---|---|---|---|---|---|---|
| A&D FX-i (`and_scale.c`) | Streamed by the scale (`Prt 3`). Zero: `Z` | 20 ms | 17 bytes, `ST,+00000.00 GN` CR LF; 8.9 ms at 19200. Taken only as a whole line of 17 ending CR LF | Header `ST`/`US`/`OL` (stable, unstable, overload; A&D's convention, the manual's header drawing did not extract); unit, 3 characters | Header ignored ("Doesn't really matter though.."); unit published | 20.9 a second measured: 2,512 frames in 120 s, 47.8 ms mean; intervals seen as 41 ms typical, 62 ms longest because of the 20 ms poll [P] | 9 to 29 ms | about 91 ms |
| G&G JJ-B (`gng_scale.c`) | **Polled**: `!p` CR LF sent, then whatever is readable is read (the reply to the previous request), then 250 ms sleep. Tare: `!t` | 250 ms | 14 bytes: sign (2), number (7), unit (3), stop, CR; 14.6 ms at 9600. Taken at the 14th byte | None; unit present | Unit ignored | 4 a second at most [C]; "about 4 samples per second" (the owner, ADR-013) | about 235 to 250 ms (the reply waits in the FIFO for the next loop) | about 500 ms |
| Steinberg SBS-LW (`steinberg_scale.c`) | Streamed. No zero command | 20 ms | 16 bytes: `S ` or `SD` (2), number (10), unit (2), CR LF; 16.7 ms at 9600 | `S`/`SD`, taken here as stable/dynamic from the driver's sample data; unit | Both ignored | [?] the scale's stream rate is not in the manual excerpt found | 17 to 37 ms at 9600 | [?] |
| US Solid JFDBS (`ussolid_scale.c`) | Streamed. No zero command | 20 ms | 15 bytes: sign (2), number (8), unit (3), CR LF; 15.6 ms at 9600. Overload `~~~~` does not parse and is dropped | None; unit | Unit ignored | 2 a second at best for the USS-DBS4..10 family: "PRT-1 Interval 0.5 seconds continuous output" is its fastest [D] | 16 to 36 ms | about 540 ms |
| JM Science (`jm_science_scale.c`) | Streamed; frame starts at `E`. No zero command | 20 ms | 19 bytes: `E`, space, stable state, sign, number (9), space, unit (3), CR LF | Stable-state character; unit | Both ignored | [?] | frame + 0 to 20 ms | [?] |
| Creedmoor TRX-925 (`creedmoor_scale.c`) | Streamed. No zero command | 20 ms | 14 bytes: sign, number (7), space, unit (2), space, CR LF | None; unit | Unit ignored | [?] | frame + 0 to 20 ms | [?] |
| Radwag PS R2 (`radwag_scale.c`) | Streamed (`SUI` frames; continuous transmission is switched on at the balance, the driver's `CU1` helper is never called). Zero sends `T` (tare) | 20 ms | 21 bytes: `SUI`, stability mark, number (12), unit (3), CR LF; 21.9 ms at 9600. Only frames starting `SUI` are taken | `?` unstable, space stable, `^`/`v` out of range; unit | Stability check is commented out; unit ignored | 10 a second at most: the interval is settable "from 1000 to 0.1 s" [D] | 22 to 42 ms at 9600 | about 140 ms at 10 a second |
| Sartorius (`sartorius_scale.c`) | Streamed; any line ending CR or LF, first number in it. No zero command | 1 ms | Variable, up to 31 characters | In Sartorius's SBI format the unit is left blank while the value is unstable (from memory, consistent with the driver's two sample lines; not checked against a manual) | Not used | Depends on the balance's auto-print setting [?] | frame + 0 to 1 ms | [?] |
| Generic (`generic_scale.c`) | Streamed; any line ending LF, first number in it. Zero does nothing | 20 ms | Variable, up to 31 characters | Whatever the scale sends | Not used | [?] | frame + 0 to 20 ms | [?] |
| Simulated (`simulated_scale.c`) | Made by the firmware from the motors' speeds | 100 ms (`sim rate` 1 to 50 a second) | none | none | | 10 a second | 0 | 100 ms |

Notes from the reading:

- **Only the A&D driver calls the frame tap** (`scale_tap_frame`), so today only its frames can be logged with their arrival time.
- **The simulated scale has no lag, no quantisation and no flight time**; its weight is `5 gr/s per rev/s` coarse plus `1` fine, snapped to the target within 0.1 gr, with +-0.05 gr of uniform noise while powder falls [C]. It cannot show an overthrow (ADR-009 says the same).
- **No driver uses a stable flag.** Four frames carry one (A&D, Steinberg, JM Science, Radwag).

## 2. From the makers: principle, figures, settings

### A&D FX-120i / FX-300i (the bench's is an FX-300i)

Source: A&D, "FZ-i / FX-i Series Instruction Manual", 1WMPD4001863C, https://aandd.jp/products/manual/balances/fz_fx-i.pdf. Page numbers are the manual's printed ones (the PDF's page is two more).

| Item | Value | Where |
|---|---|---|
| Principle | "Compact Super Hybrid Sensor" (C-SHS): magnetic force restoration combined with a single-point parallelogram (Roberval) guide | A&D brochure and https://www.aandd.jp/editorial/archive/shs_sensor.htm [D] |
| Capacity, readability | FX-120i 122 g, FX-300i 320 g; 0.001 g | section 20, p.74 [D] |
| Readability in grains | 0.02 GN (capacity 1882 / 4938 GN). So in grain mode a digit is 1.3 mg, coarser than in gram mode | the weighing-units table, p.14 [D] (its rows are shifted in the text layer; 122 g = 1882 GN identifies the row); the frame `+00000.00 GN` [P] |
| Repeatability (sd), linearity | 0.001 g; +-0.002 g | p.74 [D] |
| Stabilisation time | "Approx. 1 second (typical at FAST)"; defined as the time from placing a sample until the stabilisation indicator lights | p.74 and glossary p.76 [D] |
| Sensitivity drift | +-2 ppm/degC (10 to 30 degC) | p.74 [D] |
| Warm-up | at least 30 minutes; one hour before precise use | sections 2 and 3, p.8-9 [D] |
| Response | FAST / MID / SLOW, by holding MODE. "FAST: Fast response, but prone to drafts and vibration." | section 7, p.27 [D] |
| What the response setting changes | FAST: `Cond 0`, `SPd 2` (20 a second). MID: `Cond 1`, `SPd 0` (5 a second). SLOW: `Cond 2`, `SPd 0` (5 a second) | section 7 note, p.27 [D] |
| `Cond` | 0 "Fast response, sensitive value", for "powder target weighing"; 2 "Slow response, stable value". No time constants are given | 10.3 p.39, 10.4 p.42 [D] |
| `Cond` as an averaging time | Only for the Hold (animal) mode: 2 s, 4 s, 8 s for Cond 0, 1, 2. The one place the manual puts seconds on Cond | 10.4 p.42 [D] |
| `St-b` stability band | 0: +-1 digit ... 2: +-3 digits. The stable mark shows "when the fluctuation per second is less than the parameter". It gates the stable header and auto-print, not the stream's values | 10.3 p.39, 10.4 p.42 [D] |
| `trc` zero tracking | 0 off, 1 normal, 2 strong, 3 very strong. "When the weight data is only a few digits, turn the function off for accurate weighing." No window or rate is given | 10.3 p.39, 10.4 p.43 [D] |
| `SPd` | 0: 5, 1: 10, 2: 20 times a second; "influences Stream mode along with Baud rate and Data output pause"; "selected automatically when the response rate is changed" | 10.4 p.43 [D] |
| `Prt 3` stream | "outputs the weight data continuously regardless of the display condition" at the refresh rate; at 20 a second the baud rate must be 4800 or more | 10.3 p.40-41, 10.5 p.44, section 14 p.58 [D] |
| Serial | `bPS` 0..5 = 600..19200; `btPr` 0 = 7 bits even, 1 = 7 odd, 2 = 8 none | 10.3 p.41 [D] |
| Commands | `Q`/`SI` immediate reading, `SIR` continuous, `S` when stable, `Z` re-zero | 16.1 p.62 [D] |
| Factory settings | [?] The manual marks them with a symbol the PDF's text layer does not carry. Which `trc` and `Cond` a new scale has must be read on the scale |

What the project tells users to set (`manuals/OpenTrickler manual for ADFX scale.pdf`, by OE5AMP, linked from the hardware repo's `initialization_guide.md`): `SPd 2`, `Prt 3`, `bPS 5` (19200), `btPr 2` (8N1), response FAST, unit GN. It sets neither `trc` nor `St-b`. AutoTrickler's V2 guide sets the same but `Prt 5` and also leaves `trc` alone (https://res.cloudinary.com/cambridgeenviro/raw/upload/pdfs/atp-v2-instructions.pdf).

**Consequences:**

- A user who later picks MID or SLOW gets 5 readings a second until `SPd` is set to 2 again [D]. The firmware would see it as a usual interval near 200 ms on the A&D driver.
- Zero tracking acts near zero, which is where every charge starts (the cup is zeroed, then powder arrives). What it can swallow is bounded by its window, a few digits, but a few digits is the whole tolerance (1 digit in GN is 0.02 gr). Its window and rate are not published [?]; section 5 measures it.

### G&G JJ223BF and JJ100B

Sources: the repository's `manuals/configure_g&g_scales.md` (the owner's table; its two references at gandg.com.cn no longer load: 502 and an empty reply on 2026-11-19); G&G GmbH, "JJ-BC series Operating Instruction" v1.1, 09/2019, read through ManualsLib's page view (https://www.manualslib.com/manual/3862769/GAndg-Jj223bc.html, pages 4, 14, 15, 16), whose page 4 is headed for JJ223BF to JJ1523BF; "Operating Manual for JJ-B Series" (https://www.manualslib.com/manual/1833431/GAndg-Jj100b.html). ManualsLib's text is scrambled in places; figures taken from it are marked.

| Item | JJ223BF (JJ-BC/BF manual) | JJ100B (JJ-B manual) |
|---|---|---|
| Principle | [?] not stated in the pages read. The family has internal calibration (the JJ223BF's C6) and 0.1 mg siblings, which points to electromagnetic force compensation [E] | [?] "high precision sensors"; at 110 g x 0.001 g with e = 10 d, most likely a strain-gauge cell [E] |
| Capacity, readability | 220 g, 1 mg [D, scrambled table] | 110 g, 0.001 g [D] |
| Repeatability, linearity | +-2 mg, +-3 mg [D, scrambled table] | [?] |
| Stabilisation time | under 3 s (220 and 320 g models) [D, scrambled table]; at which C1/C2 is not said | [?] |
| C1 "sensitivity" | 0 highest .. 5 lowest (default 5) per the owner's table; the JJ-BC manual p.16 describes 0 as fast, for dispensing; higher as "robust but slow" [D] | 0 .. 4, default 2 (owner's table) |
| C2 filter / averaging | 0 shortest .. 4 longest (default 4) per the owner's table; the manual p.16: 0 is filter off, for dispensing [D] | 0 .. 3, default 2 |
| Project's recommendation | C1 = 1, C2 = 1, C3 = 6 (9600 baud, on request), C4 = 33 (`!` in place of ESC) | the same |
| Serial | 8N1; 600 (default) to 9600 baud; 14-character frame, no stable mark; `ESC p` send, `ESC t` tare, `ESC q` calibrate, `ESC s` unit [D p.14-15] | the same commands |
| Stream mode | C3 = 0 "9600 baud, stream mode" and 1 "auto print on stable" exist on the JJ223BF (owner's table). The driver does not use them; the stream's rate is [?] | not listed |
| Zero tracking | [?] not mentioned in the pages read. C1 may contain it | [?] |
| Time to answer `p` | [?] | [?] |

### The other makes

| Make (driver) | Known | To be measured or found |
|---|---|---|
| **US Solid USS-DBS** (`ussolid`) | For USS-DBS4..10 (0.1 mg, "electromagnetic force balance sensor"): continuous output at 0.5, 1, 2 or 3 s (`PRT-1..4`), "continuous mode by default"; 9600 baud default, 8N1; the 15-character frame the driver reads; sensitivity `ASD-0..3` (factory 3, the least sensitive), weighing speed `INT-0..3` (factory 2), "Do not adjust them without consulting the factory" [D: https://file.ussolid.com/content/JFDBS/English_manual_for_JFDBS00008%2855%2C10%2C16%29.pdf, pages 5-6, 14]. Other US Solid manuals list stabilisation within 3 s (DBS81, 1 mg) to 8 s (search result, not opened) | Which model a user has; stabilisation time of this family (the table's "5 Seconds" could not be tied to its row); zero tracking |
| **Radwag PS R2** (`radwag`) | Electromagnetic. Filter V.FAST/FAST/NORMAL/SLOW/V.SLOW (`P2.2.1.1`); "The filter works slower when 'approaching' the weighed mass, it works more intensively for weighed mass within the set range of the filter" (the range is a service setting). Value release FAST / FAST+REL / RELIABLE. Ambient STABLE / UNSTABLE. **Autozero** (`P2.2.1.3`): compares indications "at declared time interval e.g. 1s" near zero and re-zeroes when they differ by less than its range "e.g. one division"; the maker's own warning: "very slow placing of load on the weighing pan (load pouring). Here, zero indication correction can also correct actual indication of loaded mass." Continuous transmission in the current unit, interval 0.1 to 1000 s (`P4.1.3`) [D: Radwag, "LCD Radwag balances" user manual IMMU-03-13-01-15-ENG, Jan 2015, sections 10.4.1 to 10.4.5 p.41-43, 20.1.2 to 20.1.3 p.152, protocol p.162-167; https://www.mws.ltd.uk/wp-content/uploads/2015/02/R-Series-User-Manual-EN-4.pdf] | Stabilisation time per model (the technical table did not extract); autozero's real window and interval; whether newer firmware streams faster than 10 a second |
| **Creedmoor TRX-925** (`creedmoor`) | 925 gr capacity, 0.01 gr; strain gauges by the maker's own description; 30 minutes warm-up [M: Creedmoor Sports' product page and reviews, via search] | Everything dynamic: stream rate, filter, any display hold, zero tracking. No manual with serial details was found |
| **Steinberg SBS-LW-200A/300A** (`steinberg`) | 200 / 300 g, d = 0.001 g, e = 10 d; baud 9600/2400/4800; print settings `0CoM/1CoM/2CoM`, unexplained; 15 minutes warm-up [D: German manual, ManualsLib excerpt]. Principle not stated; at its price a strain-gauge cell [E] | Stream rate, what `S`/`SD` mean exactly, filter, zero tracking, stabilisation time |
| **JM Science** (`jm_science`) | Only what the driver's frame shows: a stable-state character | Model range, principle, rate, settings. Not researched further |
| **Sartorius** (`sartorius`) | Entris II: SBI output manual or automatic, with or without stability; the automatic interval is a setting [D, via search of the Entris II manuals]. Sartorius balances have filter ("ambient conditions"), application filter (final readout / filling) and auto-zero menus (from memory, not checked here) | The model in use; its fastest automatic output; its default parity (many Sartorius default to odd parity, which this firmware cannot set: from memory, check) |

## 3. Measured dynamic behaviour: what is published

Very little is published as measurements. What was found:

- **A&D's own step response, FX-3000i** [M: N. Izumo, A&D, "C-SHS: Compact-Super-Hybrid-Sensor for New Balances", SICE, 25-26 Sept 2008, Fig. 8; https://www.aandd.co.jp/pdf_storage/tech_doc/balance/t_c-shs.pdf]. A 3 kg load on the 3200 g x 0.01 g model: the trace is marked **1.1 s at FAST, 1.6 s at MID, 2.3 s at SLOW**. The body text is Japanese and did not extract, so what exactly ends each time (the stable mark, most likely) is read from the figure alone. It is the same sensor family and firmware generation as the FX-300i, on a model ten times coarser; the ratios 1 : 1.5 : 2.1 are the best published guide to the three settings.
- **The same paper's Fig. 9** shows the FX-3000i's zero and span over 180 minutes holding within about a digit. Legible only in part.
- **The frame rhythm of the bench's FX-300i** [P]: 20.9 a second, no frame lost or broken in 4,418, unchanged with both motors turning (ADR-013). The values' spread with the motors turning was not recorded.
- **Reloaders, anecdote only** (forum posts found by search; the threads themselves could not be opened: bot checks and rate limits): the FX-120i "usually almost instant", with reports of a single kernel sometimes not showing for 3 to 4 s, or a reading sitting one digit high for 2 to 3 s; strain-gauge scales "might not change for 10 seconds" after one kernel, and drift for hours. No poster gives a method or a log. They fit a display that holds small changes (section 4, stage 5), and they say the single-kernel case is the hard one: one kernel is about one digit.
- **AutoTrickler V2** [D: its instructions]: "about 1 grain per second at full speed", "should easily be able to hold 1 milligram" with wind panels and doors closed. No lag figure. No AutoTrickler or other reloading source was found that measured the FX-120i's lag behind a trickle.
- **Adaptive filtering is documented, not measured**: Radwag's sentence above; Mettler-Toledo's US 7,613,575: after a detected load step "at least one filter parameter of the filter is reset" and then follows a time profile, to get "a short settling time" (https://patents.google.com/patent/US7613575).

Not found, after searching [?]:

- a step response of any supported scale to a small mass (tens of mg): rise time, overshoot, settling to one digit;
- the lag of any supported scale behind a ramp;
- a draught-noise amplitude or spectrum for a precision balance (makers' material is qualitative);
- a vibration-sensitivity figure;
- creep figures for the strain-gauge makes;
- anything on falling grains' impact on a balance. The physics below stands in.

**Falling powder, from first principles** [E, a derivation, not a measurement]. Powder leaving the spout at rate `q` and falling a height `h` lands after `t_f = sqrt(2h/g)` with speed `v = g t_f` (80 ms and 0.77 m/s for 30 mm; 100 ms and 1.0 m/s for 50 mm). The pan feels the landed mass plus the stream's momentum flux `q v`, which reads as an extra mass `q v / g = q t_f`: exactly the mass still in the air. So in steady flow the pan reads everything that has left the spout, and the in-flight powder is already counted. The transients: for `t_f` after the flow starts nothing shows; for `t_f` after it stops the reading runs on by up to `q t_f` and then falls back by the same. At a fine trickle of 0.1 gr/s that bump is 0.008 gr and the filter flattens it; at a coarse 5 gr/s it is 0.4 gr for 80 ms, a visible bump at the coarse stop. One kernel (about 1 to 2 mg for common stick powders, reloading lore, not checked) landing at 0.8 m/s is an impulse worth 0.1 mg x s: after a 0.3 s filter, a third of a digit. Kernel impact is below the readability; kernel **granularity** (the weight arrives in steps of about one digit) is not.

## 4. A simulation model

One chain, the same for every make, with a parameter set per make and setting. Time step 1 ms; weights in the scale's own unit.

```
m_spout(t)      mass that has left the spout (from the motor model)          input
1 flight        m_pan  = m_spout(t - t_f) + t_f * q(t - t_f)                 q = d m_spout / dt
                (optionally: arrivals in kernels of mass m_k, Poisson at q / m_k)
2 disturbance   m_pan += draught(t) + bump(t)                                additive
3 sensor        y1 = first-order lag of m_pan(t - T_d), time constant tau_s  servo or load cell
4 filter        e = y1 - y2
                tau = tau_open  if |e| > theta   else tau_closed             the adaptive filter
                y2 += (y1 - y2) * dt / tau
5 hold          shown changes only when |y2 - shown| > H, or after T_hold    display lock; H = 0: none
6 zero track    if |shown| <= W and steady: zero += clamp(shown, +- r * dt)  shown -= zero
7 quantise      out = d * round(shown / d) + (sd noise before rounding)
8 report        every T_out (+- jitter): frame with out and the stable flag
                stable flag = out stayed within +-B digits for T_stab
9 link, driver  + frame time + uniform(0, poll); G&G: + the 250 ms the reply waits
                drop a frame with probability p_miss; a bad frame with p_bad
```

What each stage is for, and what may be left out:

- **Stages 3 and 4 are the scale.** A dead time, a fast lag, and a filter with two speeds. In steady flow the reading trails by `q x L`, with `L = T_d + tau_s + tau` (plus half of `T_out` and the link on average). With the filter closed `L` is long; once `q x tau_closed` exceeds `theta` it opens and the lag stops growing. A moving average of `N` samples at `f_s` behaves as a lag of `(N - 1) / (2 f_s)`; a first-order lag is as good until a measurement says otherwise.
- **Stage 1** matters only at the coarse stop and for the start's dead time. For a first version: a pure delay `t_f`.
- **Stage 5** is for the strain-gauge makes and the single-kernel anecdotes. Leave it at `H = 0` until a bench log shows a hold.
- **Stage 6** matters in the first digits of a charge only.
- **Stage 2:** draught as a first-order random process (correlation time seconds, switched on in bursts), bumps as short pulses. Amplitudes are guesses until logged.
- **Missing readings** are stage 9's `p_miss` and a quiet gap, which the firmware's `sim quiet` and `sim bad` already do.

### Parameters: A&D FX-300i / FX-120i (taken as alike: same sensor, readability and manual [E])

| Parameter | FAST | MID | SLOW | Tag |
|---|---|---|---|---|
| `d` readability | 0.001 g; 0.02 gr in GN | same | same | [D] |
| noise sd before rounding | 0.001 g (repeatability) | same or less | less | [D] for the figure, [E] as noise |
| `T_out` | 47.8 ms (20.9 a second) | 200 ms unless `SPd` is set back to 2 | 200 ms, likewise | [P] for FAST; [D] for the 5 a second |
| output jitter | under 20 ms (hidden by our poll) | [?] | [?] | [P] |
| frame time, poll | 8.9 ms, 0 to 20 ms | same | same | [C] |
| stabilisation time, large step | about 1 s (1.1 s on the FX-3000i) | 1.6 s (FX-3000i) | 2.3 s (FX-3000i) | [D], [M] |
| `T_d` dead time | 0.10 s | 0.10 s | 0.10 s | [E] |
| `tau_s` sensor | 0.03 s | 0.03 s | 0.03 s | [E] |
| `tau_open` | 0.05 s | 0.08 s | 0.12 s | [E], from the large-step times |
| `tau_closed` | 0.20 s (0.15 to 0.4) | 0.40 s (0.3 to 0.8) | 0.80 s (0.6 to 1.6) | [E], scaled by the hold mode's 2 / 4 / 8 s |
| `theta` filter opens above | 5 digits (2 to 20) | same | same | [E] |
| **`L` at a fine trickle** (reading behind pan) | **about 0.35 s** | **about 0.55 s** | **about 0.95 s** | [E] |
| `L` as the charge loop sees it | add 0.04 s (half `T_out`, frame, poll) | add 0.12 s at 5 a second | add 0.12 s | [C] + [E] |
| hold `H` | 0 | 0 | 0 | [E] |
| zero tracking `W`, `r` | off at `trc 0`; else 1 / 2 / 3 digits, 0.5 digit a second | same | same | [E]; only the four levels are [D] |
| stable flag `B`, `T_stab` | `St-b`: +-1 to +-3 digits over about 1 s | same | same | [D] |
| drift | 2 ppm/degC of the load: 0.02 mg/degC on a 10 g cup; zero drift [?] | same | same | [D] |
| `p_miss`, `p_bad` | 0 in 4,418 frames | [?] | [?] | [P] |

Read the `L` row as "between a quarter and a half second at FAST", no finer. At 0.1 gr/s that is 0.03 to 0.05 gr of powder not yet shown, one and a half to two and a half digits.

### Parameters: G&G JJ223BF

| Parameter | At the project's C1 = 1, C2 = 1 | At the factory's C1 = 5, C2 = 4 | Tag |
|---|---|---|---|
| `d` | 0.001 g | same | [D] |
| noise sd | 0.002 g (repeatability +-2 mg) | less | [D, scrambled table], [E] as noise |
| `T_out` | 250 ms plus the loop's own time: at most 4 a second | same | [C] |
| link | each value waits about 235 to 250 ms in the FIFO | same | [C] |
| stabilisation time | [?] | under 3 s, if the manual's figure is at the defaults | [D], [E] for which setting |
| `T_d` (with the time to answer `p`) | 0.2 s | 0.2 s | [E] |
| `tau_open` | 0.15 s | 0.3 s | [E] |
| `tau_closed` | 0.5 s (0.3 to 1.0) | 2 s (1.5 to 2.5) | [E] |
| `theta` | 5 digits | 5 digits | [E] |
| **`L` at a fine trickle**, scale alone | **about 0.7 s** | **about 2.2 s** | [E] |
| `L` as the charge loop sees it | **about 1.0 to 1.1 s** (adds the 250 ms wait and half an interval) | about 2.5 s | [C] + [E] |
| hold, zero tracking | [?] set `H = 0`, `W = 1 digit` until measured | [?] | [E] |

At 0.1 gr/s a lag of 1 s is 0.1 gr unseen: five times the A&D's digit. That is the owner's "on time but late" in one number, and a quarter of it is the driver's.

### The other makes: what to put in until measured

| Make | `T_out` | `L` to start with | Note |
|---|---|---|---|
| US Solid USS-DBS | 500 ms [D] | 1.5 s [E] | Slow by nature first: 2 readings a second caps everything. Factory sensitivity is the least sensitive |
| Radwag PS R2 | 100 ms [D] | 0.4 s at V.FAST, 1.5 s at NORMAL [E] | Autozero is documented to swallow slow pouring: model stage 6 with `W` 1 digit, interval 1 s [D, "e.g."] |
| Creedmoor TRX-925 | [?], try 100 ms | 1 s, with hold `H` 1 to 2 digits, `T_hold` 2 s [E] | Strain gauge: add creep, a slow first-order tail of a few digits over a minute [E] |
| Steinberg SBS-LW | [?], try 100 ms | 1 s, hold as above [E] | |
| JM Science, Sartorius, Generic | [?] | [?] | Nothing to go on; measure before simulating |

## 5. Identifying a scale on the bench, without powder

The aim is four numbers per scale and setting: `T_out`, `L` at a small flow, `L` at a large flow (so `theta`), and the noise; plus what zero tracking takes. Three tests, the first two with no added hardware.

**What the firmware must log** (an extension of `scale tap`, which today is A&D only and stamps in ms after a 20 ms poll):

- for every frame, from every driver: arrival time in microseconds, taken when the frame's last byte is read, with the driver polling every 1 ms while the test runs (or the time taken in a receive interrupt); the frame's raw bytes, so the stable flag and unit are kept; the parsed value; a running number, so a lost line shows;
- on the same clock: each motor's commanded speed and its counted steps (ADR-012 counts them on the STEP pin);
- written to the console in one line per frame as now. At 20 frames a second that is light.

**Test A: listen (no stimulus).** Five minutes idle with the motors off, then five with both turning, hopper empty. Gives `T_out` and its jitter, missing and bad frames, the noise sd and its spectrum up to half the frame rate, the drift over ten minutes, how often the stable flag drops, and the motors' vibration as the scale sees it. Repeat with a door opened or a hand waved at a known time for the draught's size. One run per setting.

**Test B: a small mass placed by hand.** Three sizes: about one digit (a 1 to 2 mg snip of wire, for the hold), 20 mg, 200 mg. Lower each onto the pan with tweezers from a few mm, wait for the stable flag, lift it off. **Ten placings and ten removals per size and setting.** The moment of placing is not known, so this gives the response's shape from its first movement: rise time, overshoot, time to stay within one digit, time to the stable flag, whether a one-digit step is held back. Ten repeats land at ten different phases of the frame clock, so pooled they sample the response every 5 ms on the A&D (25 ms on the G&G: use twenty there). It does not give the dead time; test C does.

**Test C: a ramp made by the trickler's own motor, with a chain.** A fine chain (jewellery chain, weighed: about 2 mg per mm is typical) hangs from a thread wound on a small spool on a trickler shaft, its lower end coiled in the cup. Turning the motor lowers chain into the cup: the mass on the pan grows by (mg per mm) x (mm per step) for each step, with no impact, and the firmware knows every step's time to the millisecond. Turning back lifts it off again, so the test repeats itself and gives a falling ramp too.

- Run constant speeds giving about 0.05, 0.3 and 2 gr/s, each for 10 s up, 5 s still, 10 s down; **five runs per speed and setting.**
- `L` at each speed is (mass lowered - reading) / `q` over the steady part; with 100 frames in it and 1 mg of noise it is good to a few ms on the A&D. If `L` is the same at all three speeds the filter is fixed; if it shortens with speed, the speed where it starts gives `theta`.
- The reading after the motor stops is the step response with an exact time zero: dead time and the catch-up curve.
- The same ramp started from a zeroed pan at the slowest speed, with zero tracking on and then off: mass lowered minus the final reading is what zero tracking took.
- A cheaper stand-in if no chain is at hand: a steady drip of water from a burette or a pinched tube into the cup, the rate taken from the reading's own slope and `L` from the time the reading runs on after the drip is cut. It needs the cut's time by hand, so it is worse.

**Only after the scale is known:** powder adds the flight time `t_f` and the kernels' steps, which are the trickler's and the powder's, not the scale's. A recorded real throw (ADR-009) then checks the whole chain.

**Pass criterion for the model:** the simulated scale, fed the same step counts, reproduces test C's reading within two digits at every frame, for all three speeds. If it does with the two-speed filter, stop there.

## 6. What is unverified, in one place

- Every `tau`, `T_d`, `theta` and `L` above is an estimate. No maker and no reloader publishes a small-signal step or ramp response for any supported scale.
- The A&D's factory settings (`trc`, `Cond`), its zero tracking's window and rate, and the meaning of its standard-format headers were not recoverable from the manual's text layer; `ST`/`US`/`OL` is A&D's known convention.
- The 1.1 / 1.6 / 2.3 s figures are for an FX-3000i and a 3 kg step, read from a figure whose Japanese text did not extract.
- G&G: the weighing principle of the JJ223BF and JJ100B; at which settings "under 3 s" holds; the stream mode's rate; the time to answer `p`; zero tracking. The technical table came through ManualsLib scrambled and names JJ223BC in places where the heading says JJ223BF.
- The claim that the G&G driver adds 250 ms is from reading the code, not from a unit.
- US Solid figures are for the USS-DBS4..10 manual; other US Solid models differ. Steinberg's, JM Science's, Creedmoor's and Sartorius's stream rates and filters are unknown.
- Draught, vibration and creep magnitudes; the kernel mass; OIML's zero-tracking limit (0.5 digit a second, from memory) behind the `r` estimate.
- Forum reports are second-hand: the threads on accurateshooter.com and snipershide.com refused automated reading.

## Sources

- A&D, FZ-i / FX-i Series Instruction Manual 1WMPD4001863C: https://aandd.jp/products/manual/balances/fz_fx-i.pdf
- N. Izumo (A&D), "C-SHS: Compact-Super-Hybrid-Sensor for New Balances", SICE, Sept 2008: https://www.aandd.co.jp/pdf_storage/tech_doc/balance/t_c-shs.pdf
- A&D, Super Hybrid Sensor: https://www.aandd.jp/editorial/archive/shs_sensor.htm
- AutoTrickler V2 instructions: https://res.cloudinary.com/cambridgeenviro/raw/upload/pdfs/atp-v2-instructions.pdf
- G&G JJ-BC series Operating Instruction v1.1 (ManualsLib): https://www.manualslib.com/manual/3862769/GAndg-Jj223bc.html
- G&G JJ-B series Operating Manual (ManualsLib): https://www.manualslib.com/manual/1833431/GAndg-Jj100b.html
- Radwag, LCD balances user manual IMMU-03-13-01-15-ENG: https://www.mws.ltd.uk/wp-content/uploads/2015/02/R-Series-User-Manual-EN-4.pdf
- U.S. Solid, USS-DBS manual (JFDBS00008): https://file.ussolid.com/content/JFDBS/English_manual_for_JFDBS00008%2855%2C10%2C16%29.pdf
- Steinberg SBS-LW-200A manual (ManualsLib, German): https://www.manualslib.de/manual/443854/Steinberg-Sbs-Lw-200A.html
- Creedmoor Sports TRX-925: https://www.creedmoorsports.com/creedmoor-reloading-scale-trx-925-precision
- Mettler-Toledo, US 7,613,575: https://patents.google.com/patent/US7613575
- In the repository: `src/*_scale.c`, `src/scale.c`, `src/charge_mode.cpp`, `manuals/configure_g&g_scales.md`, `manuals/OpenTrickler manual for ADFX scale.pdf`, `manuals/Scale-Settings`, `docs/developments_plans/ADR-013-charge_safety/` (README and progress), `ADR-009-controller_and_tuning/README.md`; in the hardware repository: `Manual/initialization_guide.md`, `Manual/sourcing_guide.md`, `Manual/tuning_guide.md`.
- Text extracted from the fetched PDFs, for checking a quotation, is beside this file in `txt\` (`fxi_p.txt`, `radwag_p.txt`, `cshs.txt`, `ussolid.txt`, `atp2.txt`; the leading number on each line of the `_p` files is the PDF's page).
