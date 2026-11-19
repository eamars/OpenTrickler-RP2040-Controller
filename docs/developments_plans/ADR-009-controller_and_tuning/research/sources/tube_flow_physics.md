# Grains through a slowly rotating rifled tube: physics for a trickler simulation model

Desk research, 2026-11-19. No hardware touched, no repository changed.
Scope (cut down by the owner while this was being written): generic dry granular flow only. Grains are described by size, shape (sphere or short cylinder) and bulk density. Cohesion, charging, surface finish and wear are left out. Where cohesion would change a result, a single line says so.
Units: speeds are **tube rev/s**, written n. The firmware commands tube speed directly, over 0.05 to 5 rev/s for both tubes.

Each statement is tagged by where it comes from:
- **[E]** an established result from the literature, with its source;
- **[D]** my own derivation, shown in full;
- **[S]** speculation, to be tested;
- **[?]** something I could not find or could not check.

---

## 0. What the hardware documents say

Sources: `C:\workspace\OpenTrickler\Manual\assembly.md`, `tuning_guide.md`, `STL\OpenTrickler\README.md`, `STL\Powder Hopper\README.md`, and the community `Dewey/Rear Reducer Mods/README.md`.

- **The assembly manual** only points to the per-part guides.
- **The OpenTrickler README says this about the tubes:**
  - Both tubes go through bearings in the rear body: a 6804 on top for the large tube and a 6801 below for the small one. The front body slides over them, with a 6804 on top and a "608" below. The front bottom bearing named in the text differs from the BOM, which lists two 6804 and two 6801; this is a discrepancy in the document.
  - Both tubes are belt-driven by GT2 belts. The longer belt (86T/87T) goes to the right motor.
  - Both tubes are "recommended to print with random Z seam position". That is the only statement about surface seams.
  - **Choice of small tube:**
    - low flow: "for small size kernels, or ball powder"; the default design, with its rear end blocked;
    - mid flow: medium kernels;
    - high flow: large kernels, with its rear end open, "good for high flow application".
- **Feed path:**
  - A rear and a front "volume reducer" insert sit next to the bearings, with a sliding door at each end.
  - The hopper slides onto the top of the rear body. Its base rotates between ON and OFF, so the hopper has its own valve.
  - The volume reducer is therefore the chamber the hopper empties into, and the tubes take grains from it. This is my reading of the parts list; the text does not describe the feed path.
- **Ball-propellant community note:** a community note on the rear bearing plate for ball propellants enlarges "the openings slightly" and adds "a 60 degree chamfer". Users have therefore met feed restrictions at the rear plate with small spherical grains.
- **Tilt:** **no document states a tilt angle** for the tubes. "Horizontal or nearly so" from the brief stands unverified [?]. The model below treats any tilt as a parameter absorbed into the fitted transport gain.
- **Tuning guide:**
  - The controller is PID with Ki held at 0.
  - Coarse stop threshold is 3 to 5 gr. Fine stop threshold is the scale's resolution (0.02 gr).
  - Targets are under 10 s for the coarse stage and under 15 s overall.
  - None of this is physics, but it sets which time scales matter: 0.1 s to 10 s.

The current firmware's simulated scale (`src/simulated_scale.c`) is the baseline any new model replaces:
- flow ∝ |speed| per tube;
- a hard threshold of 0.2;
- uniform ±0.05 noise added to each reading.

---

## 1. Axial transport: mass flow against speed

### 1.1 Plain drum, no flights: Saeman / Kramers–Croockewit [E]

**Sources:**
- Saeman (1951), *Chem. Eng. Prog.* 47:508;
- Kramers & Croockewit (1952), *Chem. Eng. Sci.* 1(6):259–265, "The passage of granular solids through inclined rotary kilns";
- Lebas et al. (1995), *Can. J. Chem. Eng.* 73:173, found the Kramers–Croockewit equation reproduced both residence time and the bed depth profile in a pilot kiln ([record](https://zendy.io/title/10.1002/cjce.5450730202));
- secondary uses of the equation: Spurling et al. (2001) and Perron & Bui (1990).

The full text was not retrieved. The equation below is in its commonly cited form [? form checked from memory, consistent with the secondary sources]:

```
Φv = (4π/3) n R³ ( tan α / sin β  −  (dh/dx) / tan β ) · (2h/R − h²/R²)^{3/2}
```

Symbols:
- Φv: volumetric flow;
- n: rev/s;
- R: bore radius;
- α: tube tilt (positive downhill toward the outlet);
- β: dynamic angle of repose;
- h(x): bed depth at axial position x.

What follows from it:
- **Horizontal tube (α = 0):** flow is driven only by the bed being deeper at the inlet than at the outlet. Each avalanche runs down the steepest line of the surface, which has an axial component when the surface is tilted along the axis.
- **Flow is exactly proportional to n** for a given bed-depth profile. The model is quasi-static: bed shape does not depend on speed in the rolling regime.
- **Holdup grows with Φv/n.** Feed the same mass rate at a lower speed and the tube fills up more.

**[D] Order of magnitude for the large tube**, taking R = 7.2 mm (mean of crest and groove), L = 46.8 mm, β = 32°, horizontal:
- Integrating dh/dx from a flooded inlet (h_in = R, half full) to a free outlet edge gives Φv ≈ 227 mm³ per rev.
- At a bulk density of 0.9 g/cc that is ≈ 0.20 g/rev ≈ **3.1 gr/rev**.
- With a 1.2 mm lip at the outlet (6.0 mm radius against 7.2 mm) the result is almost unchanged (222 mm³/rev). The lip matters for holdup and spill-over behaviour, not for the mean.
- With the inlet only 4 mm deep: ≈ 1.0 gr/rev. **Flow is very sensitive to inlet fill.** Inlet fill is set by the chamber level and the inlet geometry, not by the tube.

Caveat: the equation is a continuum model. The bore is only 11 to 18 stick diameters across, so the numbers are for scale only.

### 1.2 Tube with helical grooves: the screw term [D, from the screw-conveyor principle]

**[E] Screw conveyors.** The textbook capacity of a screw conveyor is

```
Q = (π/4)(D² − d²) · S · n · φ · C
```

where S is the pitch, φ the fill fraction and C an inclination factor. Material advances by up to one pitch per turn **only if it does not rotate with the screw**.

**[D] The rifled tube.** In a rifled tube the bed stays at the bottom: it slides and avalanches back against the rotating wall. The groove flanks then sweep through it like five shallow screw flights of height 0.59 mm (large tube) or 0.47 mm (small tube). The upper bound is plug transport at one lead per turn:

```
q_screw ≤ ρb · A_bed · Lead · n
```

| Tube | Fill 10% | Fill 20% |
|---|---|---|
| Large (πR² ≈ 163 mm², lead 20 mm) | 326 mm³/rev ≈ 4.5 gr/rev | 9 gr/rev |
| Small (πR² ≈ 38 mm², lead 11 mm) | 42 mm³/rev ≈ 0.6 gr/rev | 1.2 gr/rev |

Realistic values are a fraction η of this bound. Only grains near the wall feel a flight about half a grain high, and the rest ride on them. Because η is unknown, it has to be fitted. [?] No experimental η for shallow internal rifling was found. The search found only rotary-dryer and DEM work on flights. One result reported that inclined flights increased axial speed by about 26% over a plain drum.

**[D] Separating the two terms by reversing rotation.**
- **The screw term changes sign with rotation direction.** The bed-gradient (Kramers–Croockewit) term does not.
- So running the same |n| forward and backward and comparing gives screw and gradient contributions directly: q_fwd = q_KC + q_screw, q_rev = q_KC − q_screw. This is a cheap, decisive experiment if the driver can turn backwards.
- [?] The handedness of each tube's rifling relative to its normal rotation was not established here. Check it on the STL: if the rifling pushes grains rearward, it acts as a metering brake, not a conveyor.

### 1.3 When is mass rate ∝ speed, and what bends it

| Mechanism | Effect on q(n) | Applies to | Status |
|---|---|---|---|
| Flooded inlet, fixed bed profile | q ∝ n (Kramers–Croockewit and screw both linear) | large tube, open rear in the chamber | [E] form, [D] application |
| Inlet-limited feed (small window, §4) | q → q_in,max, **independent of n**: saturation | small (fine) tube | [D] |
| Inventory dynamics | after a speed change, q overshoots or undershoots until holdup re-equilibrates (§1.4) | both; strongest on the fine tube | [D] |
| Wall slip (bed slides as a block on a smooth wall: Mellmann's "slipping") | q collapses toward 0 at low wall friction; no surface renewal | smooth nozzle section (last 9 mm of the small tube); any worn groove | [E] regime exists; [S] here |
| Approach to centrifuging (Fr → 1) | bed lifts, cataracts, then sticks to the wall: q drops | **not reached in 0.05–5 rev/s** (§1.5) | [E] + [D] |
| Low-speed intermittency (§2) | mean still ∝ n, but delivered in discrete avalanches: apparent dead band | both, below about 0.1–0.3 rev/s | [D]/[S] |

### 1.4 Holdup and delay [D]

**Residence time with an axial bed velocity of η·Lead·n:**

```
τ_res = L_tube / (η · Lead · n)
```

| Tube | Revolutions per transit (η = 1) | τ_res at 1 rev/s, η = 0.5 |
|---|---|---|
| Large | 46.8 / 20 = 2.3 | ≈ 4.7 s |
| Small | 54 / 11 = 4.9 | ≈ 10 s |

**The response to a speed change is NOT delayed by τ_res.** Grains already at the outlet respond to the new speed at once, since outflow ∝ n × local bed depth at the outlet. What is delayed is the *re-filling* of the tube.

**A single-compartment model (holdup M):**

```
dM/dt = q_in(n, M) − q_out,     q_out = k_t · n · M
```

- **Steady state:** M_ss = q_in / (k_t n).
- **Inlet-limited tube:** q_in is roughly constant.
  - After running slowly, M_ss is large.
  - A step up in speed then dumps that inventory: q_out jumps to k_t·n_new·M_old, then decays with time constant 1/(k_t n_new) to q_in.
  - **This is a candidate root cause of "speed does not correlate with delivery rate".** The rate depends on the speed history through M.
- **Flooded inlet (coarse tube):** q_in ≈ k_in · n · (M_max − M). M stays near a fixed fraction of M_max, and q stays ≈ ∝ n with a short re-equilibration.

### 1.5 Centrifuging and other critical speeds [E] definition, [D] numbers

**[E] Definitions.** Fr = ω²R/g with ω = 2πn. A single grain at the wall centrifuges at Fr = 1:

```
n_c = (1/2π) · sqrt(g/R)
```

Measured transitions for a filled drum are Fr_c ≈ 1, rising above 1 at low fill. Source: Santos et al. (2016), *Braz. J. Chem. Eng.* 33(3), [doi 10.1590/0104-6632.20160333s20150128](https://scielo.br/j/bjce/a/ytYHqkDk3fnz9nXfLrq9MTj/?lang=en), citing Mellmann (2001).

| Radius used | n_c (tube rev/s) | Fr at 5 rev/s | Fr at 0.05 rev/s | Fr at 0.2 rev/s |
|---|---|---|---|---|
| Large, crest 6.9 mm | **6.0** | 0.69 | 6.9e-5 | 1.1e-3 |
| Large, groove 7.5 mm | **5.8** | 0.76 | 7.5e-5 | 1.2e-3 |
| Large, lip 6.0 mm | 6.4 | 0.60 | 6.0e-5 | 9.7e-4 |
| Small, crest 3.28 mm | **8.7** | 0.33 | 3.3e-5 | 5.3e-4 |
| Small, groove 3.74 mm | **8.2** | 0.38 | 3.8e-5 | 6.0e-4 |
| Small, nozzle 2.85 mm | 9.3 | 0.29 | 2.9e-5 | 4.6e-4 |

**[D] Verdict:**
- Neither tube centrifuges in 0.05–5 rev/s.
- **The large tube reaches Fr ≈ 0.7 at 5 rev/s.** That is well into the cataracting range, where grains are thrown from the wall: the regime boundary is about Fr 0.3–0.4 [E, approximate, from secondary sources citing cascading-to-cataracting near Fr ≈ 0.37]. At the top of its range the large tube's q(n) can therefore bend: grains thrown across the bore, possibly out of the end in a wider spray.
- The small tube stays below Fr 0.4 everywhere.
- A dynamic speed limit is still worth keeping: the 5 rev/s cap is about 0.85 n_c for the large tube.

---

## 2. Bed-motion regimes and avalanches

### 2.1 Regimes [E]

**Sources:**
- Mellmann (2001), "The transverse motion of solids in rotating cylinders — forms of motion and transition behavior", *Powder Technol.* 118:251–270 ([record](https://www.scilit.net/publications/b2f7bc30a4145647bcf4ecfa92c502c5)). It gives the sequence slipping, surging, slumping, rolling, cascading, cataracting, centrifuging, with a bed-behaviour diagram of Fr and wall friction against fill.
- Rolling ranges as quoted by Santos et al. (2016): 10⁻⁴ < Fr < 10⁻² (Mellmann) and 5·10⁻⁴ < Fr < 2·10⁻² (Henein et al. 1983).
- Slumping lies below the rolling range. Cascading lies above it, up to about 0.1–0.4, then cataracting.
- **Slipping** happens at low wall friction and low fill: the bed slides back as a block. Santos et al. glued P80 sandpaper to the wall to suppress it.
- **Balmforth & McElwaine** ([drum.pdf](https://personal.math.ubc.ca/~njb/Research/drum.pdf)) treat slumping to rolling as a bifurcation with hysteresis (shown for sand), not a sharp Fr threshold.
- **Bed turnover:** the slump-to-roll transition occurs where the turnover times of the two modes are equal (Birmingham group, [record](https://research.birmingham.ac.uk/en/publications/granular-motion-in-rotating-drums-bed-turnover-time-and-slumping-)).

**[D] Where the trickler sits.**
- At 0.05 rev/s: Fr ≈ 3–8·10⁻⁵, which is slumping.
- At 0.2 rev/s: Fr ≈ 5–12·10⁻⁴, the slumping-to-rolling boundary.
- 1–3 rev/s: rolling to cascading.
- Large tube near 5 rev/s: cataracting.
- **The slumping-to-rolling transition falls right inside the fine tube's working range.**

### 2.2 Avalanche period at low speed [E] + [D]

**[E] Avalanche angles.** In slumping, the surface steepens with the wall up to the maximum angle of stability θ_m, then avalanches down to the angle of repose θ_r.
- Jaeger, Liu & Nagel (1989), *PRL* 62:40, "Relaxation at the angle of repose": a drum stays static up to θ_m, slips to θ_r, and with vibration the angle relaxes logarithmically.
- Typical Δθ = θ_m − θ_r is 2–3° for glass beads; one cited abstract gives 27.8° and 25.6°.
- Larger Δθ is expected for angular or elongated grains [S: magnitude not found].

**[D] Avalanche period and transition speed.**
- Avalanche period: T_av = Δθ / (360° · n).
  - With Δθ = 3°, n = 0.05: T_av ≈ 0.17 s (6 Hz).
  - n = 0.2: 42 ms.
- An avalanche lasts t_d ~ a few × sqrt(ℓ/g) for surface length ℓ ~ 10 mm, so t_d ≈ 30–100 ms.
- When T_av < t_d, avalanches merge into continuous rolling: n_trans ≈ Δθ/(360·t_d) ≈ 0.1–0.3 rev/s.
- This agrees with the Fr boundary in §2.1.
- **Prediction:** below about 0.1–0.3 rev/s, discharge from the tube end is a train of discrete spills at a few Hz. Above that, it is a fluctuating continuous stream.

### 2.3 Finite size: a bore of 5–15 grains [D]/[S]

**Bore-to-grain ratios:**

| Section | Sticks (by diameter) | Sticks (by length) | Balls |
|---|---|---|---|
| Large bore (14–15 mm) | 11–18 | 6–14 | 18–48 |
| Small bore (6.6–7.5 mm) | 5–9 | — | 8–25 |
| Small nozzle (5.7 mm) | 4–7 | — | — |

**[S] Expected finite-size effects**, by analogy with small-pile avalanche studies (Held et al. 1990 measured avalanche statistics on a small conical pile). [?] No study of a drum only 5–10 grains across was found.
- The bed is only a few grains deep, so an "avalanche" is a few to a few tens of grains.
- Sizes are broadly distributed, not quasi-periodic. Large piles tend to give quasi-periodic system-spanning avalanches; small ones give broad distributions.
- Stick-like grains lock into the shallow grooves, about half a grain deep. That raises Δθ and the chance of a stick-slip of the whole bed against the wall.
- **Discharge happens only at the outlet edge.** What matters is avalanches in the last few mm of bed: with a few grains per spill, the clump is 1 to ~20 grains ≈ 1–40 mg ≈ 0.02–0.6 gr.
- At the fine stage this is the same order as the stop threshold. **Grain-scale randomness is a first-order effect for the fine tube**, not a nuisance term.

---

## 3. Discharge as a random process, and how to identify it

### 3.1 Evidence and models [E]

**Orifice (silo) flow:**
- Flowing intervals and avalanche sizes are exponentially distributed, i.e. a Poisson process with constant clogging probability per grain.
- Clog durations follow a power law.
- Sources: Zuriguel et al., reviews in [EPJ WoC 2017](https://en.unav.edu/documents/14434042/14552703/2017_EPJEWOC14001002_Zuriguel.pdf/d0f9caa1-9eb2-adbf-5072-9b37e7515b5f) and [Invited review: clogging in bottlenecks, arXiv:1412.5806](https://ar5iv.labs.arxiv.org/html/1412.5806).
- The exponential shape breaks down for non-spherical grains, several orifices, and slots.

**Rotating drum:** the power spectrum of avalanche flow was measured by Jaeger et al. (1989) and follow-ups ([Uni Augsburg](https://opus.bibliothek.uni-augsburg.de/opus4/files/42273/42273.pdf)).
- [?] No paper was found that measures **outflow-mass statistics from the open end of a small horizontal rotating tube**. The model below is assembled, not taken from a source.

### 3.2 Model [D]

**Compound Poisson, or renewal, discharge:**
- Events (spills) come at rate r(n).
- Each spill has mass c, independent and identically distributed, with mean c̄ and second moment E[c²].
- Delivered mass in a window τ:

```
E[m(τ)] = r c̄ τ = q τ
Var[m(τ)] = r E[c²] τ          (Poisson events)
F = Var/E = E[c²]/c̄  (units of mass)  = c̄ (1 + CV_c²)
```

- With exponential clump sizes, F = 2c̄.
- **The spread of delivered mass grows as sqrt(mean):** σ_m = sqrt(F · m̄). This is the quantity that sets the fine stage's last-interval error.
- **Renewal events (quasi-periodic avalanches, inter-event CV < 1):** variance grows more slowly at τ longer than the event period.
  - Asymptotically Var ≈ (CV_T² r) E[c]² τ + r Var(c) τ.
  - A purely periodic event train with fixed clumps has bounded Var.
- **Slow drift** (holdup relaxing, chamber level) adds Var ∝ τ² at long τ.

**[D] Example at fine-stage rates:** q = 10 mg/s (0.15 gr/s) with c̄ = 2 mg exponential gives F = 4 mg.
- Over 1 s: σ = 6.3 mg ≈ 0.1 gr.
- That is well above the scale's resolution (1 mg). **Clumping is identifiable from the weight trace.**

### 3.3 Identification from open-loop weight-against-time traces [D]

Work on the cumulative weight W(t_k) at 20 Hz, never on differenced single samples.

1. **Mean flow:** a least-squares slope over the steady part of each run. Check that it is steady with a CUSUM or a rolling slope.
2. **Variance-time curve:**
   - For lags τ = 0.05 s … 30 s, compute Var[W(t+τ) − W(t) − q̂τ] over the run.
   - Subtract the scale's own floor, 2σ_s², measured on an idle run.
   - Slope 1 on log-log is Poisson-like, and the level gives F.
   - Slope < 1 at τ > T_av means renewal (regular avalanches).
   - Slope 2 at long τ means drift (holdup, feed).
   - A dip or bump at τ = 1/n or 1/(5n) means once-per-rev or groove ripple.
3. **Fano factor:** F(τ) = Var/mean from step 2. A plateau value gives the effective clump mass c_eff = F/2 if exponential, or F if all clumps are equal.
4. **Allan deviation of the rate estimate:**
   - σ_A²(τ) ∝ 1/τ for white flow noise (Poisson);
   - flat for flicker;
   - rising for drift.
   - Its minimum gives the best averaging window, which is also the shortest window over which a controller can trust a rate estimate. This is the most directly useful single plot for control.
5. **Spectrum of the rate:** compute the periodogram of the detrended dW/dt, or of W minus the fitted line.
   - Look for lines at n (eccentricity, seam, belt run-out) and 5n (five groove ends passing the lip).
   - Belt mesh is at 32n or 22n; the stepper full-step frequency is far higher.
   - The 20 Hz sampling puts the Nyquist frequency at 10 Hz, so a line at 5n is visible only up to n = 2 rev/s.
   - The scale's internal filter will attenuate further. [?] The A&D's filter response is not known here.
   - **Better: synchronous averaging.** Fold dW/dt against tube angle, using the firmware's step count and clock (the `motor` console command) as the angle reference. This recovers the ripple's amplitude and phase well below the noise.

---

## 4. Inlet: grains through a few-grain-wide opening

### 4.1 Beverloo and its small-orifice correction [E]

**Beverloo:**

```
W = C ρb sqrt(g) (D₀ − k d)^{5/2}
```

with C ≈ 0.55–0.65 and k ≈ 1.3–2.9 (larger for non-spherical grains). Its validity has been tested for d > 0.5 mm and orifices large enough not to jam (Mankoc et al. 2007).

**Mankoc et al. (2007), *Granular Matter* 9:407, [arXiv:0707.4550](https://ar5iv.labs.arxiv.org/html/0707.4550):**

```
W_b = C' (1 − ½ e^{−b(R−1)}) (R − 1)^{5/2},     R = D₀/d
```

- 3D fit for glass beads: C' = 64.6 beads/s, b = 0.051.
- Valid for R = 1.5–100.

**Independence from head [E, Janssen]:** wall friction carries the column's weight once the fill height exceeds about 1–2 container widths. The outflow from an orifice then does not depend on fill height. [?] Not re-fetched; this is standard textbook material.
- [D] Consequence: the hopper level should not matter until the chamber above the tube inlet nearly empties.

### 4.2 Clogging and arching [E]

- **3D spheres:** mean avalanche size between clogs diverges as ⟨s⟩ = A (R_c − R)^{−γ}, with R_c = 4.94 ± 0.03, γ = 6.9 ± 0.2 and A = 9900 (Zuriguel et al. 2005, as reported in arXiv:1412.5806).
  - The review's text elsewhere says "about 5" in another unit. The caption value is used here.
  - Thomas & Durian argue instead for an exponential growth with no true critical size, so clogs become extremely rare but are never impossible above R ≈ 5.
- **Slots** (Davies & Desai): minimum width/particle ratios of 3.3 (horizontal slot) and 4.6 (vertical slot), against 6.4 for a circular outlet in the same study.
- **Elongated grains** (rice, lentils) increase the critical outlet size: clogging is more likely.
- **Vibration does not change the probability of forming a clog.** It adds a probability of breaking one (Mankoc et al. 2009, *PRE* 80:011309; [review](https://ar5iv.labs.arxiv.org/html/1412.5806)).

### 4.3 The fine tube's window [D]

**Geometry:**
- The window is about 3 mm axially and 55° wide at r ≈ 3.7 mm, so about 3.6 mm of arc.
- That makes an opening of about 3 × 3.6 mm, hydraulic diameter D_h ≈ 3.3 mm.
- Against d = 1 mm sticks, R ≈ 3.3; against 0.5 mm balls, R ≈ 6.5.

**Clogging:**
- **For sticks the window is below the spherical critical ratio (4.9)**, and elongation makes it worse. A static window of this size would clog.
- The window is not static: it moves under the chamber's grains at 2π·3.7 mm · n ≈ 23 mm/s per rev/s. Shearing an arch across the opening breaks it (rotating-silo work exists, e.g. [arXiv:2210.14115](https://arxiv.org/pdf/2210.14115), on elongated grains discharged under rotational shear).
- [S] Expect intermittent, not blocked, filling, with pass-to-pass variation in fill.

**Fill rate while the window is open (Beverloo with D₀ = D_h):**

| Grain | W | Mankoc factor at R−1 |
|---|---|---|
| 0.5 mm balls, k = 1.5 | 0.52 g/s | ×0.62 |
| 1 mm sticks, k = 1.5–2.0 | 0.09–0.22 g/s | ×0.55 |

**Inlet saturation:**
- The window is open to the chamber for a fraction α/2π of each turn: at least 55/360 = 0.15, more if the chamber covers a sector wider than the window [?].
- Fill per revolution is m_rev = W · (α/2π) / n, so the inlet's mass rate is

```
q_in,max ≈ W · α/2π   — independent of n
```

  - Balls: ≈ 0.08 × 0.62 ≈ 0.05 g/s ≈ **0.8 gr/s**.
  - Sticks: ≈ 0.008–0.018 g/s ≈ **0.1–0.3 gr/s**.
- Window open time per pass is (α/2π)/n: 0.15 s at 1 rev/s and 0.03 s at 5 rev/s. The latter is close to the time a Beverloo flow needs to establish (~sqrt(D/g) ≈ 20 ms). At high n, fill per pass therefore falls below the steady estimate, so q_in can even *decrease* at high n [S].
- **Window position:** when the window faces down, grains in the tube can fall back out into the chamber. The window is a two-way exchange [S; it depends on where grains lie in the reducer cavity, which the documents do not show].

**[D] Consequence for the fine tube:**
- At low n, transport capacity (∝ n) is the limit. The tube fills to M_ss = q_in/(k_t n) and is capped at what the bore holds, after which the window is choked by the full bore.
- At high n, the window is the limit: q saturates at q_in,max, about 0.1–0.8 gr/s depending on grain.
- **The crossover speed is a key identifiable parameter.**
- [S] The "high flow" small tube has an open rear end. That removes this saturation, which is consistent with the README's wording ("good for high flow application").

**Large tube:** its open rear end has radius ~7 mm, so R = 11–48. There is no clogging, and the flooded-inlet model of §1.1 applies.

---

## 5. Grain-material effects

Dropped at the owner's request.
- Where cohesion would matter, it raises Δθ (bigger, rarer avalanches), causes wall slip or sticking, and makes the inlet clog more.
- In the model all of that is absorbed into the fitted clump and dead-band parameters, and nothing more is said here.

---

## 6. Vibration and oscillatory rotation to regularise slow discharge

### 6.1 What is known [E]

**Dimensionless acceleration:** Γ = A ω² / g.

**Vibrated silos:**
- Horizontal vibration increases orifice flow rate.
- Vertical vibration decreases it as Γ rises.
- Vibration breaks existing arches, with an unclogging probability that rises with Γ and outlet size. It does not prevent new ones. Arches break at defects, where a neighbour angle exceeds 180° (Lozano et al. 2015, *PRE* 91:062203).
- Unclogging times follow a power law with exponent below 2 at low Γ, which means unbounded mean waits.
- Sources: [review](https://ar5iv.labs.arxiv.org/html/1412.5806), [Janda et al. arXiv:0907.0114](https://ar5iv.arxiv.org/html/0907.0114), [Mankoc et al. 2009](https://en.unav.edu/documents/15083165/15313038/2009.PRE80.pdf/bd27d560-dcd8-73e2-16a4-b704ef1658a7).

**Micro-feeding by a vibrating tube:**
- **Matsusaka, Urakawa & Masuda (1995)**, *Adv. Powder Technol.* 6:283–293: a capillary 0.58–1.26 mm across, vibrated ultrasonically, gives stable continuous feeding at a few mg/s ([KAKEN](https://kaken.nii.ac.jp/en/grant/KAKENHI-PROJECT-07855086), [Kyoto repository](https://repository.kulib.kyoto-u.ac.jp/dspace/bitstream/2433/217931/1/S0921-8831(08)60518-X.pdf)). That work is on fine particles where adhesion dominates.
- **Yang & Evans:** on/off and flow-rate control of granular flow in open capillaries by acoustic vibration (*Powder Technol.* 2003), and a review, "Metering and dispensing of powder; the quest for new solid freeforming techniques" (*Powder Technol.* 178:56–72, 2007, [record](https://eprints.soton.ac.uk/165057)).
- The principle shared with the trickler: a tube kept near jamming is switched on and off, or metered, by vibration amplitude. Flow stops within a period when vibration stops.

**Vibratory conveyors** [E, textbook; not re-fetched]:
- They run at throw numbers of order 1–3, typically 25–60 Hz.
- A grain slides when the tangential acceleration exceeds μg (Γ_t > μ ≈ 0.4–0.7), and hops when the normal component exceeds g.

[?] No study was found of oscillating (back-and-forth) rotation of a horizontal rifled tube for metering, nor of "tapped tricklers" in the literature. Hobby practice with tapped or vibrated tricklers exists, but no measured source was found.

### 6.2 What the stepper, belt and tube allow [D]

**Γ = 1 at the wall needs an oscillation of angle amplitude θ_a = g / (R ω²):**

| Frequency | Small tube, R 3.5 mm | Large tube, R 7.2 mm |
|---|---|---|
| 10 Hz | ±41° | ±20° |
| 20 Hz | ±10° | ±4.9° |
| 50 Hz | ±1.6° | ±0.8° |
| 100 Hz | ±0.4° | ±0.2° |

- **Converting to the motor:**
  - Small tube: motor angle = tube angle × 22/40 = 0.55.
  - Large tube: × 32/40 = 0.8.
  - So ±1.6° of the small tube at 50 Hz is ±0.9° at the motor, about half a full step (1.8°).
- **Feasibility:**
  - [D] This is feasible in principle with microstepping. Peak motor speed is θ_a·ω ≈ 0.028 rad × 314 = 8.8 rad/s at the tube, about 1.4 tube rev/s, within range.
  - [S] Belt elasticity and rotor inertia will low-pass this. A 20–50 Hz oscillation may excite the belt-tube resonance, and the amplitude at the tube is unknown without measurement.
  - Torque is the binding limit: tube inertia is tiny, but the rotor's (~50–60 g·cm² for a NEMA17) needs I·θ_a·ω² of torque.
  - [D] Rotor ≈ 5.4e-6 kg·m² × 0.016 rad (motor) × 98,700 ≈ 8.5 mN·m, comfortably below a NEMA17's holding torque. Feasible.
- **Direction of the shaking:** the tangential direction at the bed (bottom of the bore) is roughly horizontal, the direction that *increases* orifice and surface flow in the silo literature. That is favourable.

**Net transport under symmetric oscillation [D]:**
- The screw term reverses with direction, and the gradient term follows each avalanche. Symmetric oscillation about a fixed mean therefore transports nothing on average, except at the outlet edge: grains that cross the lip fall and do not come back. That is a rectifier.
- [S] Oscillation superimposed on a slow mean rotation should do two things:
  - keep the bed surface near its angle of repose, with Δθ effectively shrunk toward zero;
  - turn the outlet's few-Hz large-clump spills into a stream of smaller spills at the oscillation frequency.
- This is the most plausible route to "about one grain at a time". It is untested here, and **worth a bench experiment, not a model term yet**.

---

## 7. Flight and landing as seen by the scale

**[D] Kinematics, from rest at the lip:**
- t_f = sqrt(2h/g): 30 mm → 78 ms; 80 mm → 128 ms.
- Landing speed v = sqrt(2gh): 0.77 and 1.25 m/s.
- Horizontal exit speed 2πnR ≤ 0.23 m/s at 5 rev/s (large tube). This affects where grains land, not when.

**[E] The hourglass result** (Shen & Scott 1985, *Am. J. Phys.* 53:787; Sack & Pöschel, [arXiv:cond-mat/0701776](https://ar5iv.arxiv.org/html/cond-mat/0701776)):
- In steady flow, the impact force exactly compensates the weight of the mass in flight.
- [D] Check: in-flight mass is q·t_f; impact force is q·v = q·g·t_f, which is the same in weight units.
- **Scale reading during steady flow = total mass discharged so far.** There is no offset and no "column" to anticipate.
- Effects remain only at the start (nothing is read for t_f) and the end (reading constant while the column lands).
- Example at q = 1 gr/s with h = 50 mm: the in-flight mass is 0.1 gr. That is relevant to a naive controller, but cancelled by the impacts.

**Corrections:**
- **Bounce:** restitution e adds up to e·q·t_f to the reading while flowing. That is ≤ 0.1·e gr at 1 gr/s and ≤ 0.005 gr at fine rates: negligible.
- **Scatter out of the cup:** a mass loss, not a dynamic. Measure it once by weighing what lands outside.
- **Single-grain impulse:** 1 mg × 1 m/s = 1 µN·s, filtered by the pan and the scale. Negligible beyond the scale's own noise.
- **Net effect:** a pure delay of about 0.1 s. At 20 Hz with the scale's internal filter this cannot be separated from the scale's own lag, so the two are lumped.

---

## 8. Judgement: the simplest model that matters for control

### 8.1 The proposed list, item by item

| Proposed item | Verdict | Why |
|---|---|---|
| Speed-dependent mean flow map with saturation | **Keep.** Linear in n for the coarse tube (flooded inlet), linear with saturation for the fine tube (window inlet, q_in,max) | §1.1–1.3, §4.3 |
| Dead band | **Replace** a hard threshold with intermittency. Mean flow stays ∝ n down to low n, but arrives in rare discrete spills. Keep a small true dead band only if the data show zero mean flow (wall slip, lip) | §2.2–2.3 |
| Tube inventory / transport delay | **Keep, as holdup M with outflow ∝ n·M**, not as a pure delay. It produces history dependence and overshoot after slow running | §1.4 |
| Clumped stochastic discharge | **Keep.** Compound renewal events: rate r(n) ≈ n·360/Δθ, clump mean = flow/r, gamma-distributed size | §2.2, §3.2 |
| Once-per-rev ripple | **Keep, plus a 5n term** (five grooves). Cheap to model, phase-locked to tube angle | §3.3 |
| Trailing drops | **Keep.** Model as: after a stop, the outlet bed is left between θ_r and θ_m, so a last spill comes with probability p_trail after a random delay (triggered by vibration such as the other tube running) | §2.2 |
| Flight delay | **Merge** into one dead time with the scale's lag; no in-flight mass offset (hourglass result) | §7 |
| **Add:** scale dynamics | First-order lag τ_s, 20 Hz sampling, 1 mg quantisation, white noise σ_s. The controller sees the scale, not the cup | — |

### 8.2 States and parameters (per tube unless noted)

**States:**

| State | Meaning |
|---|---|
| M | holdup near the outlet, mg |
| θ_acc | tube rotation since the last spill |
| θ_next | random spill threshold, drawn at each spill |
| φ | tube angle, for ripple |
| Cup mass and scale filter state | shared by both tubes |

**Dynamics:**

```
dM/dt  = q_in − q_out_mean
q_in   = min( k_in · n · (M_max − M) ,  Q_in )          (coarse: Q_in = ∞)
q_out_mean = k_t · n · M · (1 + a1 cos(φ+ψ1) + a5 cos(5φ+ψ5))
spill when θ_acc ≥ θ_next ~ Gamma(mean Δθ, shape κθ);  then θ_acc = 0
spill mass = accumulated q_out_mean since last spill × Gamma(1, shape κc)  (removed from M)
stop: with prob p_trail, one spill after Exp(τ_trail)
cup(t) = Σ spills delayed by t_d
scale  = LPF(τ_s)[cup] sampled 20 Hz + N(0, σ_s²), quantised 1 mg
```

**Parameters (≈ 12 per tube):** k_t, k_in, M_max, Q_in, Δθ, κθ, κc, a1, ψ1, a5, ψ5, p_trail, τ_trail, and the shared t_d, τ_s, σ_s.

The renewal spill of §3.2 gives the start-up dead time for free: after a stop, the first spill needs up to Δθ of rotation, a time Δθ/(360n), which is seconds at 0.05 rev/s.

### 8.3 Which parameters can be identified (20 Hz, 1 mg)

| Parameter | Identifiable? | From |
|---|---|---|
| σ_s, quantisation, drift | yes | idle run |
| τ_s + t_d | partly. Lumped from the flow-start step; cleanly from a hand-dropped known mass | E0 |
| k_t·M_ss (gain q/n), saturation Q_in, crossover n* | yes | E1 map |
| k_t and M_max separately (holdup) | yes, from step-up overshoot after slow running and its decay time 1/(k_t n) | E2/E3 |
| Δθ (spill rate) | yes at low n, from start-up dead time and from the variance-time knee at τ ≈ T_av. Individual spills are resolvable only if T_av ≳ 0.2–0.5 s (n ≲ 0.02–0.05 for Δθ 3°) | E1, E2 |
| Clump-size spread κc | yes via Fano F(τ) when F ≳ 2 mg; poorly for single-grain clumps (≈ 1 mg, at resolution) | E1 |
| a1, a5 | yes for a1 when n ≤ ~2 rev/s and synchronous averaging is used; a5 only for n ≲ 1–2 rev/s; amplitude attenuated by τ_s (correct for it) | E1 |
| p_trail, τ_trail | yes, with enough repeats (≥ 10 stops) | E2 |
| Screw versus gradient share | yes, if the tube can run in reverse | E5 |
| t_f alone | no (merged) | — |

### 8.4 Open-loop experiments

**General rules:**
- Hopper loaded with one powder.
- Owner runs them: the bench rules forbid an agent's run with powder in the hopper.
- Log raw 20 Hz weight plus the step-count clock.
- Speeds log-spaced: 0.05, 0.08, 0.12, 0.2, 0.3, 0.5, 0.8, 1.2, 2, 3, 5 tube rev/s, in random order.
- Before every run, a fixed preconditioning of 10 s at 1 rev/s, then 5 s stopped. This fixes holdup and avalanche state.

**The experiments:**
- **E0, the scale:**
  - 5 min idle (noise, drift);
  - 10 drops by hand of a ~50 mg object onto the pan (step response τ_s, settle time);
  - 2 min with each tube's motor turning on an empty hopper (vibration's effect on the reading).
- **E1, steady map per tube:**
  - Each speed until about 30 gr or 60 s, whichever comes first. Low speeds stay time-capped (60 s at 0.05 rev/s gives few grams on the fine tube; acceptable).
  - 3 repeats each, about 33 runs per tube.
  - Gives q(n), Q_in, n*, F(τ), Allan curves, the spectrum and a1/a5.
- **E2, start and stop:**
  - At 0.1, 0.3 and 1 rev/s: start from rest after the precondition, run 20 s, stop, record 30 s.
  - 10 repeats at 0.1 rev/s, 5 at the others.
  - Gives dead time, its spread (Δθ), p_trail and τ_trail.
- **E3, history:**
  - Run A: 60 s at 0.1 rev/s, then a step to 2 rev/s for 20 s.
  - Run B: 60 s at 2 rev/s (same step).
  - Also steps of 0.3 ↔ 1 rev/s up and down.
  - 3 repeats.
  - The difference between A and B identifies holdup M_max and k_t. This is the test that decides whether the inventory state is needed.
- **E4, hopper level:** E1 at 0.3 and 2 rev/s with the hopper full and nearly empty, 3 repeats. This confirms head independence or shows when it fails.
- **E5, direction:** E1 at 0.3 and 1 rev/s run in reverse, if the driver allows it, 3 repeats. Gives the screw versus gradient share.
- **E6, optional:** an oscillation sweep on the fine tube, superimposed on 0.1 rev/s mean: 20 and 50 Hz at Γ ≈ 0.3, 1, 2 at the wall (amplitudes from §6.2). Compare F and the dead time with E1 and E2. **Exploratory, not part of the model.**

Total time is roughly 2–3 h per tube with powder, most of it E1.

### 8.5 Expected to be negligible (leave out)

- Centrifuging: n_c = 5.8–6.4 rev/s for the large tube, 8.2–9.3 for the small, both above the 5 rev/s limit. Cataracting in the large tube near 5 rev/s goes into the fitted map, not a separate term.
- In-flight mass, because impacts cancel it.
- Bounce force.
- Separate flight delay.
- Hopper head, until nearly empty.
- Stepper microstep ripple and belt-tooth frequencies (well above the scale's bandwidth).
- Grain-property effects as separate terms.

### 8.6 Unverified

- **Equations quoted from memory or secondary sources rather than the papers themselves:**
  - the exact form of Kramers–Croockewit;
  - Mellmann's full regime table;
  - Janssen;
  - the vibratory conveyor throw numbers.
- **Not found in the literature:**
  - an η for shallow internal rifling;
  - any measured outflow statistics from small horizontal rotating tubes;
  - any study of oscillating rotation for metering.
- **Not stated in the hardware documents:**
  - tube tilt;
  - the rifling's handedness against rotation;
  - which end the large tube's 6 mm lip is at;
  - how much of each turn the fine window sees grains.
- **Derived numbers not to rely on:** the flow estimates (3 gr/rev coarse; 0.1–0.8 gr/s fine saturation) and the 0.1–0.3 rev/s slumping-to-rolling speed are order-of-magnitude derivations for checking against E1, not predictions.
- **The scale:** the A&D's internal filter response is unknown.
