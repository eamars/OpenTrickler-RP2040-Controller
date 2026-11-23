#ifndef CHARGE_RULES_H_
#define CHARGE_RULES_H_

/*
    Every fixed number of the charge, in one place (ADR-009, gate N).

    To build a firmware of your own with another value: change it here and build. Nothing else needs to be found: no
    other file of the charge holds a number of its own, and CI fails when one appears (tools/charge_sim/check_numbers.py).
    The safety bounds are here too; changing one is yours to answer for.

    Each line names its row in docs/developments_plans/ADR-009-controller_and_tuning/numbers.md, which gives where
    the value came from and what is decided of it, and says its unit and what it is for. In the project's own firmware
    a value here changes only by an ADR.

    These are not a user's settings and not what a calibration measures. A setting is in the settings' tables; what a
    unit measures of its scale, its tubes and its powder is in its calibration.

    A number without a suffix is read in the controller's own precision (charge_control.c is also built in double for
    the PC's check against its reference, tools/charge_sim/staged.py, which names the same numbers). The numbers that
    are another module's are that module's: the scale's rhythm (scale.h, rows E24) and the drivers' polls (motors.c,
    rows E17, E25).

    "The bench's" marks a value taken from the unit the controller was developed on. It fails the test for a fixed
    number and is replaced at the gate named; until then it is here as it was.
*/

// --- The frame's constants (numbers.md, section C): the defaults of charge_control_default_constants ---------------
#define CHARGE_RULE_MOVE_RPS 0.2                // C1, rev/s: a small move's speed. The bench's; measured from gate 3
#define CHARGE_RULE_PARCEL_S 0.01               // C2, s: the powder turned out is counted in parcels this long
#define CHARGE_RULE_DRIBBLE_DIGITS_S 8.0        // C6, digits/s: the jog finish's dribble. Goes with the jog (F.5)
#define CHARGE_RULE_COARSE_SPREADS 3.0          // C7: spreads of the coarse powder unseen that both tubes keep clear of
#define CHARGE_RULE_CREEP_BAND_SHARE 1.0        // C8: the creep's powder still in the air, as a share of the band
#define CHARGE_RULE_CREEP_MOST_S 6.0            // C9, s: the creep's stretch never takes longer than this
#define CHARGE_RULE_LEARN_SHARE 0.15            // C10: the share of a throw's first error that moves the stop
#define CHARGE_RULE_LEARN_MOST_DIGITS 2.0       // C11, digits: no throw teaches the stop more than this (a bound)
#define CHARGE_RULE_LEARN_DEAD_DIGITS 0.25      // C12, digits: nor less: under it is the reading's own rounding
#define CHARGE_RULE_LEARN_FINE_PRIOR 0.0        // C13: 0, the fine figure is not learnt: off until gate 3 has measured
                                                // the fine tube's lag across the user's speeds. On, it is the throws
                                                // the calibration's figure counts for (3 when it was thrown)
#define CHARGE_RULE_LEARN_FINE_MOST 0.25        // C14: the learnt fine figure never further than this share off (a bound)
#define CHARGE_RULE_LEARN_FINE_SPLIT 1.0        // C15: 1, a stream's fine figure and a finish's; 0, one. Decided at gate 3
#define CHARGE_RULE_STILL_LEAST_S 0.4           // C16, s: at rest, no new high for this long. The bench's; without effect at 0 (N.3): goes at F.5
#define CHARGE_RULE_MOST_MOVES 8                // C17: tries a throw gets after its first rest (a bound)
#define CHARGE_RULE_THROW_MOST_S 120.0          // C18, s: a throw past it ends "limit" (a bound)
#define CHARGE_RULE_FINE_MOST_TIMES 4.0         // C19, times: a throw whose fine tube turns more than this many times its
                                                //   target over its grains a turn, and its moves' turns, ends "limit"
                                                //   (a tube pouring a third of its figure needs 3); no figure, no turn
#define CHARGE_RULE_NOISE_DIGITS 2.0            // C24, digits: the scale's noise is the zero's mean margin (A9) and this
                                                //   many digits (a reading's flicker, a drift of a digit in a throw):
                                                //   a reading below the base by more is the cup lifted; a rise by more
                                                //   is powder shown
#define CHARGE_RULE_NO_POWDER_RISES 30.0        // C25, noises: while a tube turns in the stream, more forecast powder
                                                //   due than this many of the noise since the last rise, the first
                                                //   waiting longer than the longest lag, ends the throw "no powder"
#define CHARGE_RULE_ZERO_MARGIN_DEFAULT 0.02     // A9's default, the scale's unit: the user's zero mean margin, until
                                                //   the charge gives the controller the user's own
#define CHARGE_RULE_OVER_TOLERANCES 100.0      // C20, tolerances: a reading this many of the user's tolerance over the
                                                //   target stops all (3 at the default 0.03), in the scale's unit
#define CHARGE_RULE_OVER_MOST_GR 3.0            // C20's old form, the scale's unit: only the PID-learnt experiment's
                                                //   (charge_pid_learnt.c), which goes at F.4
#define CHARGE_RULE_SPEED_LAW 0                 // C21: 0 the glide pours, 1 the PID law. A choice that goes at F.5
#define CHARGE_RULE_PID_ENVELOPE 1              // C22: the frame's guard on the PID law. Always on from F.5

// --- The rules' own numbers (section D) ----------------------------------------------------------------------------
#define CHARGE_RULE_SLOWEST_LAG_LEAST_S 0.3     // D1, s: the slowest fine stop's lag taken as at least this. Goes at F.5
#define CHARGE_RULE_CREEP_LEAST_SHARE 0.5       // D2: the creep never under this share of the slowest stop's speed
#define CHARGE_RULE_CREEP_TOP_STOP 2            // D3, D7: the creep never above this fine stop's speed (from 0); the glide
                                                // time's lags are taken from this stop to the fastest
#define CHARGE_RULE_AIM_BAND_SHARE 0.5          // D4: the first rest is aimed a digit under the target, or this share of
                                                // the band if that is more
#define CHARGE_RULE_JOG_STOP_SPREADS 2.5        // D5: the jog's stop room in spreads. Goes with the jog (F.5)
#define CHARGE_RULE_GLIDE_SPREADS 2.0           // D6: the glide room is the stop room and this many spreads of a stop from
#define CHARGE_RULE_GLIDE_FLOW_TIMES 4.0        // D6: this many times the creep's flow
#define CHARGE_RULE_GLIDE_LAG_SHARE 0.6         // D7, D8: a glide time is this share of its lag
#define CHARGE_RULE_GLIDE_LEAST_S 0.4           // D7, s: the fine glide time's limits. Both go at F.5
#define CHARGE_RULE_GLIDE_MOST_S 2.0
#define CHARGE_RULE_COARSE_GLIDE_LEAST_S 0.3    // D8, s: the coarse glide time's limits. Both go at F.5
#define CHARGE_RULE_COARSE_GLIDE_MOST_S 1.5
#define CHARGE_RULE_COARSE_ROOM_SPREADS 3.0     // D9: the coarse room is this many spreads of a coarse stop from its floor,
#define CHARGE_RULE_COARSE_ROOM_EXTRA_GR 0.2    // D9, the scale's unit: the glide room and this. The bench's; without effect at 0 (N.3): goes at F.5
#define CHARGE_RULE_REST_FLOOR_TIMES 2.0        // D10: the rest is the slowest quiet time of the stops within this many
                                                // times the creep,
#define CHARGE_RULE_REST_EXTRA_S 0.3            // D10, s: and this. The bench's; without effect at 0 (N.3): goes at F.5
#define CHARGE_RULE_JOG_AIM_SDS 2.0             // D11: the jog's aim in SDs of its rehearsals. Goes with the jog (F.5)
#define CHARGE_RULE_COARSE_LEAST_TURNS 0.5      // D12, turns: the coarse tube runs only for more than its room and this
#define CHARGE_RULE_FINE_WINDOW_EXTRA_S 0.3     // D13, s: the fine learning's stretch opens this long after the coarse
                                                // tube's quiet time. The bench's; decided with C13 at gate 3
#define CHARGE_RULE_RISE_DIGITS 0.5             // D14, digits: a reading this much above the highest is a rise
#define CHARGE_RULE_STILL_INTERVALS 2.0         // D15, reading intervals: at rest, no new high for this many, if longer
                                                // than C16
#define CHARGE_RULE_BAND_EDGE_DIGITS 0.25       // D16, digits: a rest within the band and this much is in it (a float's
                                                // tolerance: 10.02 over a cup of 60 gr is 0.0200043)
#define CHARGE_RULE_LEARN_FINE_LEAST_TURNS 0.2  // D17, turns: a stretch shorter than this teaches the fine figure nothing
#define CHARGE_RULE_LEARN_FINE_LEAST_STREAM_TURNS 0.5   // D17, turns: nor a stream shorter than this
#define CHARGE_RULE_TIE_S 1e-4                  // D18, s: two times this close are the same time (not control: the check
                                                // against the reference)
#define CHARGE_RULE_OFFER_COARSE_FACTOR 1.0     // D19: the offer's coarse kp is this over (grains a turn x coarse glide time)
#define CHARGE_RULE_OFFER_FINE_FACTOR 0.6       // D19: the fine kp likewise. The bench's; from each unit's trade at gate 3
#define CHARGE_RULE_VERDICT_DIGITS 0.5          // D20, digits: over or under is judged this much beyond the band
#define CHARGE_RULE_TOP_UP_SHARE 0.5            // D22: a top-up's mean is at most this share of the short and the band
#define CHARGE_RULE_PAUSE_COARSE_DIGITS 1.0     // D23, digits: with this much coarse powder still unseen, the fine tube's
                                                // stop is a pause

// --- What a calibration must hold before a throw may use it (section B) --------------------------------------------
#define CHARGE_RULE_COARSE_STOPS_LEAST 1        // B12: coarse stops
#define CHARGE_RULE_FINE_STOPS_LEAST 3          // B12: fine stops

// --- The charge's own fixed numbers (section E) --------------------------------------------------------------------
#define CHARGE_STALE_INTERVALS 3                // E1: no reading for this many usual intervals: the tricklers pause
#define CHARGE_LOST_INTERVALS 10                // E2: this many, and
#define CHARGE_LOST_MIN_MS 1000                 // E2, ms: never under this: the scale is lost
#define CHARGE_USUAL_INTERVAL_UNKNOWN_MS 250    // E3, ms: the rhythm not known yet: take a slow scale's, a G&G's
#define CHARGE_PAUSES_BORNE 2                   // E4: a third pause in one throw ends it
#define CHARGE_STEADY_READINGS 10               // E5: steady after a pause: the zero test less its zero
#define CHARGE_STEADY_SPACING_MS 300            // E5, ms
#define CHARGE_STEADY_LIMIT_MS (2 * CHARGE_STEADY_READINGS * CHARGE_STEADY_SPACING_MS)  // E5: not steady by then: unsteady
#define CHARGE_ZERO_READINGS 10                 // E6: the zero is judged on this many readings,
#define CHARGE_ZERO_SPACING_MS 300              // E6, ms: each waited for up to this long, and this far apart
#define CHARGE_CUP_OFF_BELOW 10                 // E7, the scale's unit: the cup is off when the mean is this far under zero.
                                                // A known fault (a cup lighter than it is never seen to go): mended at F.1
#define CHARGE_CUP_OFF_READINGS 5               // E19: the cup's removal is judged on this many readings,
#define CHARGE_CUP_OFF_WAIT_MS 200              // E19, ms: each waited for up to this long,
#define CHARGE_CUP_OFF_SPACING_MS 300           // E19, ms: this far apart
#define CHARGE_CUP_BACK_WAIT_MS 200             // E20, ms: the cup's return: a reading waited for up to this long,
#define CHARGE_CUP_BACK_SPACING_MS 20           // E20, ms: looked for this often
#define CHARGE_VERDICT_WAIT_MS 1000             // E9, ms: waited before the verdict is given. Goes for the frame at F.5
#define CHARGE_PRECHARGE_AFTER_GATE_MS 500      // E10, ms: the precharge starts this long after the gate's close
#define CHARGE_NO_PRECHARGE_WAIT_MS 20          // E10, ms: with no precharge. Goes with today's loop (gate 6)
#define COARSE_STOP_BACKOFF_MAX_MS 15000        // E11, ms: the back-off's longest (a bound)
#define CHARGE_CARRY_THROWS_MAX 32              // E12: a charge's first errors kept for the carry
#define CHARGE_CARRY_LEAST_THROWS 5             // E12: a charge carries nothing from fewer throws at rest (a bound)
#define CHARGE_CARRY_MOST_DIGITS 3.0f           // E12, digits: a carry never moves the stop more than this at once (a bound)
#define CHARGE_SERIES_CUP_MOST 600.0f           // E13, the scale's unit: the bench's series ends when its cup would hold this
#define CHARGE_DRIVER_CHECK_MS 100              // E17, ms: a motor driver's fault is looked for this often in a throw
#define CHARGE_LEGACY_READING_WAIT_MS 200       // E18, ms: today's loop waits this long for a reading. Goes with it (gate 6)

// --- The experiment of a PID on a learnt forecast (section G): folded into the frame or gone at F.4 ----------------
#define CHARGE_PID_RULE_LAG_S 0.9               // G1, s: the fine tube's powder shows this long after its turn. The bench's
#define CHARGE_PID_RULE_REST_S 2.5              // G1, s: the least wait from a stop to its verdict. The bench's
#define CHARGE_PID_RULE_GR_TURN_PRIOR 2.0       // G2, the scale's unit a turn: the figure a fresh powder starts from. The bench's
#define CHARGE_PID_RULE_COARSE_QUIET_S 1.0      // G3, s: the coarse tube's powder has landed this long after its stop
#define CHARGE_PID_RULE_FLOOR_LEAST_RPS 0.02    // G3, rev/s: the learnt least speed never under this
#define CHARGE_PID_RULE_COARSE_LAG_S 0.6        // G3, s: the coarse tube's lag, when its powder is forecast
#define CHARGE_PID_RULE_DIGIT_GR 0.02           // G4, the scale's unit: the scale's digit. The bench's
#define CHARGE_PID_RULE_LEARN_LEAST_TURNS 0.01  // G9, turns: a stop whose window held fewer teaches nothing
#define CHARGE_PID_RULE_LEARN_THROW_LEAST 0.5   // G9: a throw's figure is held between this share of the learnt one
#define CHARGE_PID_RULE_LEARN_THROW_MOST 2.0    // G9: and this

#endif  // CHARGE_RULES_H_
