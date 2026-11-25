/*
 * ADR-009 step 2: the charge controller (charge_controller.h), ported from tools/charge_sim/step2.py's Step2 and
 * legacy_pid.py's LegacyPid, which it is judged against call for call (tests/step2_sim.py).
 */
#include "charge_controller.h"

#include <math.h>
#include <string.h>

#define PARCEL_S 0.01
#define KNOWN_TURNS 0.15        // fine turns shown since the quiet before the throw's own weight a turn is taken
#define KNOWN_ROWS 5
#define REST_TURNS 0.2          // fine turns turned out after the quiet for a throw's own, at rest, to be carried
#define FIT_ROUNDS 4            // the weight a turn and its lags solved together
#define BACKOFF_MOST_MS 15000.0


// Step 1's lag at a flow: straight between the table's points, the end's value beyond them (numpy.interp)
static double lag_at(const charge_config_t *config, double flow) {
    const int n = config->lag_count;
    if (flow <= config->lag_flow_per_s[0]) {
        return config->lag_s[0];
    }
    for (int i = 1; i < n; i++) {
        if (flow < config->lag_flow_per_s[i]) {
            double slope = ((double) config->lag_s[i] - config->lag_s[i - 1]) /
                           ((double) config->lag_flow_per_s[i] - config->lag_flow_per_s[i - 1]);
            return slope * (flow - config->lag_flow_per_s[i - 1]) + config->lag_s[i - 1];
        }
    }
    return config->lag_s[n - 1];
}


static double lag_most(const charge_config_t *config) {
    double most = config->lag_s[0];
    for (int i = 1; i < config->lag_count; i++) {
        most = fmax(most, config->lag_s[i]);
    }
    return most;
}


// The middle of a few values, the two middle ones' mean for an even count (statistics.median, numpy.median)
static double median_of(const double *values, int count) {
    double sorted[CHARGE_LAG_POINTS > CHARGE_CARRY_THROWS ? CHARGE_LAG_POINTS : CHARGE_CARRY_THROWS];
    for (int i = 0; i < count; i++) {
        int j = i;
        while (j > 0 && sorted[j - 1] > values[i]) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = values[i];
    }
    return count % 2 ? sorted[count / 2] : (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0;
}


static double lag_median(const charge_config_t *config) {
    double lags[CHARGE_LAG_POINTS];
    for (int i = 0; i < config->lag_count; i++) {
        lags[i] = config->lag_s[i];
    }
    return median_of(lags, config->lag_count);
}


bool charge_controller_carried(const charge_controller_t *controller, double *k) {
    if (controller->carry_count == 0) {
        return false;
    }
    *k = median_of(controller->carry, controller->carry_count);
    return true;
}


void charge_controller_init(charge_controller_t *controller) {
    controller->carry_count = 0;
    controller->done = true;
    controller->coarse_rps = 0.0;
    controller->fine_rps = 0.0;
}


static const charge_parcel_t *parcel_at(const charge_controller_t *controller, int i) {
    return &controller->parcels[controller->parcel_first + i];
}


static double parcel_at_s(const charge_controller_t *controller, const charge_parcel_t *parcel) {
    return (controller->start_ms + parcel->at_ms) / 1000.0;
}


static double parcel_speed(const charge_parcel_t *parcel) {
    return parcel->turns / (parcel->ms / 1000.0);
}


// A parcel kept. Before the quiet only those the air may need again; from the quiet on every one, for the throw's own
// weight a turn and its rest; past the room the oldest go, and the throw is cut
static void parcel_keep(charge_controller_t *controller, double at_s, double turns, double now_s) {
    if (!controller->quiet_known) {
        double newest = lag_most(&controller->config) + PARCEL_S;
        while (controller->parcel_count > 0 &&
               parcel_at_s(controller, parcel_at(controller, 0)) < now_s - newest) {
            controller->parcel_first++;
            controller->parcel_count--;
        }
    }
    if (controller->parcel_first + controller->parcel_count == CHARGE_PARCELS_MAX) {
        if (controller->parcel_count == CHARGE_PARCELS_MAX) {
            controller->parcel_first++;
            controller->parcel_count--;
            controller->fit_cut = true;
        }
        memmove(controller->parcels, &controller->parcels[controller->parcel_first],
                (size_t) controller->parcel_count * sizeof(charge_parcel_t));
        controller->parcel_first = 0;
    }
    charge_parcel_t *parcel = &controller->parcels[controller->parcel_first + controller->parcel_count];
    parcel->turns = (float) turns;
    parcel->at_ms = (uint32_t) lround(at_s * 1000.0 - controller->start_ms);
    parcel->ms = (uint32_t) lround((now_s - at_s) * 1000.0);
    controller->parcel_count++;
    if (controller->parcel_count > controller->parcels_most) {
        controller->parcels_most = controller->parcel_count;
    }
    if (controller->rest_known) {
        controller->rest_certain += turns;      // turned out after the quiet: not shown at it
    }
}


static void air_add(charge_controller_t *controller, double due_s, double turns) {
    if (controller->air_count == CHARGE_AIR_MAX) {
        return;                         // never on the lags a scale has: the oldest parcels show first
    }
    charge_air_t *air = &controller->air[(controller->air_first + controller->air_count) % CHARGE_AIR_MAX];
    air->due_s = due_s;
    air->turns = (float) turns;
    controller->air_count++;
    controller->unseen += turns;
}


bool charge_controller_start(charge_controller_t *controller, double target, double now_ms) {
    const charge_config_t *config = &controller->config;
    if (config->lag_count < 1 || config->lag_count > CHARGE_LAG_POINTS) {
        return false;
    }
    controller->start_ms = now_ms;
    controller->target = target;
    controller->coarse_target = fmax(0.0, target - config->coarse_threshold);
    controller->coarse_rps = 0.0;
    controller->fine_rps = 0.0;
    controller->done = false;
    controller->coarse_integral = 0.0;
    controller->fine_integral = 0.0;
    controller->coarse_last_error = 0.0;
    controller->fine_last_known = false;
    controller->last_sample_ms = now_ms;
    controller->first_sample = true;
    controller->coarse_moves = true;
    controller->backoff_on = false;
    controller->fine_speed = 0.0;
    controller->parcel = 0.0;
    controller->parcel_start_s = now_ms / 1000.0;
    controller->last_tick_s = now_ms / 1000.0;
    controller->reading = 0.0;
    controller->quiet_known = false;
    controller->k_known = charge_controller_carried(controller, &controller->k);
    controller->own_known = false;
    controller->held = false;
    controller->fit_cut = false;
    controller->rest_known = false;
    controller->rest_cut = false;
    controller->edge_count = 0;
    controller->unseen = 0.0;
    controller->air_first = 0;
    controller->air_count = 0;
    controller->parcel_first = 0;
    controller->parcel_count = 0;
    controller->parcels_most = 0;
    controller->row_count = 0;
    return true;
}


static double forecast(const charge_controller_t *controller) {
    return controller->reading + (controller->k_known ? controller->k * controller->unseen : 0.0);
}


static void stop(charge_controller_t *controller) {
    controller->fine_rps = 0.0;
    controller->coarse_rps = 0.0;
    controller->done = true;
}


void charge_controller_tick(charge_controller_t *controller, double now_ms) {
    const charge_config_t *config = &controller->config;
    if (controller->done) {
        return;
    }
    double now = now_ms / 1000.0;
    double dt = now - controller->last_tick_s;
    controller->last_tick_s = now;
    double change = config->acceleration_rps2 * dt;
    double v = controller->fine_speed;
    v = v < controller->fine_rps ? fmin(controller->fine_rps, v + change) : fmax(controller->fine_rps, v - change);
    controller->fine_speed = v;
    if (v > 0.0) {
        controller->parcel += v * dt;
    }
    if (now - controller->parcel_start_s >= PARCEL_S) {
        if (controller->parcel > 0.0) {
            double speed = controller->parcel / (now - controller->parcel_start_s);
            parcel_keep(controller, controller->parcel_start_s, controller->parcel, now);
            air_add(controller, controller->parcel_start_s + lag_at(config, speed * (controller->k_known ? controller->k : 0.0)),
                    controller->parcel);
        }
        controller->parcel = 0.0;
        controller->parcel_start_s = now;
    }
    while (controller->air_count > 0 && controller->air[controller->air_first].due_s <= now) {
        controller->unseen -= controller->air[controller->air_first].turns;
        controller->air_first = (controller->air_first + 1) % CHARGE_AIR_MAX;
        controller->air_count--;
    }
    if (!controller->quiet_known && !controller->coarse_moves && !controller->backoff_on) {
        controller->quiet_known = true;
        controller->quiet_at_s = now + lag_most(config) + config->settle_s;
    }
    // The stop, the moment the forecast reaches the target, below the coarse tube's threshold
    if (!controller->coarse_moves && controller->fine_rps > 0.0 && forecast(controller) >= controller->target) {
        stop(controller);
    }
}


/*
 * The throw's own weight a turn: the slope of the readings since the quiet on the fine turns shown, each parcel shown
 * after the lag at its own flow, and so at the weight a turn being found: solved by repeating from the table's middle
 * lag. False until KNOWN_TURNS are shown in KNOWN_ROWS readings. Each parcel's turns count from the first reading at
 * or after it is due (numpy's searchsorted on the parcels sorted by when they are due, done the other way round)
 */
static bool fit(charge_controller_t *controller, double *k_out) {
    const charge_config_t *config = &controller->config;
    const int rows = controller->row_count;
    if (rows < KNOWN_ROWS) {
        return false;
    }
    const double middle = lag_median(config);
    double k = 0.0;
    bool first = true;
    for (int round = 0; round < FIT_ROUNDS; round++) {
        for (int r = 0; r < rows; r++) {
            controller->shown_at_row[r] = 0.0;
        }
        for (int i = 0; i < controller->parcel_count; i++) {
            const charge_parcel_t *parcel = parcel_at(controller, i);
            double lag = first ? middle : lag_at(config, parcel_speed(parcel) * k);
            double due = parcel_at_s(controller, parcel) + lag;
            // the first reading at or after it is due; none: never shown
            int lo = 0, hi = rows;
            while (lo < hi) {
                int mid = (lo + hi) / 2;
                if ((controller->start_ms / 1000.0 + controller->rows[mid].t_s) < due) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            if (lo > 0 && lo < rows) {
                controller->shown_at_row[lo] += parcel->turns;
            }
        }
        // x: the turns shown since the first reading; the least squares' slope of the weights on it
        double x = 0.0, sum_x = 0.0, sum_w = 0.0;
        for (int r = 0; r < rows; r++) {
            x += controller->shown_at_row[r];
            controller->shown_at_row[r] = x;
            sum_x += x;
            sum_w += controller->rows[r].weight;
        }
        if (x < KNOWN_TURNS) {
            return false;
        }
        double mean_x = sum_x / rows, mean_w = sum_w / rows, sxx = 0.0, sxw = 0.0;
        for (int r = 0; r < rows; r++) {
            double dx = controller->shown_at_row[r] - mean_x;
            sxx += dx * dx;
            sxw += dx * (controller->rows[r].weight - mean_w);
        }
        k = sxw / sxx;
        if (!(k > 0.0)) {
            return false;
        }
        first = false;
    }
    *k_out = k;
    return true;
}


// The weight a turn the forecast takes now: the carried one where there is one, else the throw's own once known; the
// air's parcels made due again at it
static void choose(charge_controller_t *controller) {
    const charge_config_t *config = &controller->config;
    double carried;
    bool has_carried = charge_controller_carried(controller, &carried);
    // The throw's own is found only where it is taken: with nothing carried. Past the room it is held as last found
    if (!has_carried && !controller->fit_cut) {
        controller->own_known = controller->row_count > 0 && fit(controller, &controller->own);
    }
    controller->k_known = has_carried || controller->own_known;
    controller->k = has_carried ? carried : controller->own;
    double now = controller->last_tick_s;
    double k = controller->k_known ? controller->k : 0.0;
    controller->air_first = 0;
    controller->air_count = 0;
    controller->unseen = 0.0;
    double newest = lag_most(config) + PARCEL_S;
    for (int i = 0; i < controller->parcel_count; i++) {
        const charge_parcel_t *parcel = parcel_at(controller, i);
        double at = parcel_at_s(controller, parcel);
        if (at < now - newest) {
            continue;
        }
        double due = at + lag_at(config, parcel_speed(parcel) * k);
        if (due > now) {
            air_add(controller, due, parcel->turns);
        }
    }
}


/*
 * The first reading after the quiet: the parcels turned out within the slowest lag before it kept as its edge (whether
 * each is shown at it depends on the weight a turn, known only at rest), those since the quickest lag before it
 * counted as not shown at it; every parcel after counts too (parcel_keep)
 */
static void rest_begin(charge_controller_t *controller, double quiet_s, double weight) {
    const charge_config_t *config = &controller->config;
    double quickest = config->lag_s[0];
    for (int i = 1; i < config->lag_count; i++) {
        quickest = fmin(quickest, config->lag_s[i]);
    }
    controller->rest_known = true;
    controller->rest_quiet_s = quiet_s;
    controller->rest_at_quiet = weight;
    controller->rest_certain = 0.0;
    for (int i = 0; i < controller->parcel_count; i++) {
        const charge_parcel_t *parcel = parcel_at(controller, i);
        double at = parcel_at_s(controller, parcel);
        if (at > quiet_s - quickest) {
            controller->rest_certain += parcel->turns;
        } else if (at >= quiet_s - lag_most(config) - PARCEL_S) {
            if (controller->edge_count == CHARGE_EDGE_MAX) {
                controller->rest_cut = true;
                continue;
            }
            controller->edge[controller->edge_count++] = *parcel;
        }
    }
}


static double derivative_of(const charge_controller_t *controller, double error, double last_error,
                            double elapsed_ms) {
    if (controller->first_sample || elapsed_ms <= 0.0) {
        return 0.0;
    }
    return (error - last_error) / elapsed_ms;
}


// The coarse tube turned back for the stored turns once it stops, as coarse_trickler_backoff() does
static void backoff(charge_controller_t *controller, double now_ms) {
    const charge_config_t *config = &controller->config;
    if (!config->backoff_enable || config->backoff_turns <= 0.0f) {
        return;
    }
    double speed = config->backoff_rps > 0.0f ? config->backoff_rps : config->coarse.least_rps;
    if (speed <= 0.0) {
        return;
    }
    double time_ms = fmin(BACKOFF_MOST_MS, config->backoff_turns / speed * 1000.0);
    if (time_ms < 1.0) {
        return;
    }
    controller->coarse_rps = -speed;
    controller->backoff_on = true;
    controller->backoff_until_ms = now_ms + time_ms;
}


void charge_controller_reading(charge_controller_t *controller, double now_ms, double weight) {
    const charge_config_t *config = &controller->config;
    if (controller->done) {
        return;
    }
    controller->reading = weight;
    if (controller->backoff_on && now_ms >= controller->backoff_until_ms) {
        controller->coarse_rps = 0.0;
        controller->backoff_on = false;
    }
    if (controller->quiet_known && now_ms / 1000.0 >= controller->quiet_at_s) {
        if (!controller->rest_known) {
            rest_begin(controller, now_ms / 1000.0, weight);
        }
        if (controller->row_count < CHARGE_ROWS_MAX) {
            charge_row_t *row = &controller->rows[controller->row_count++];
            row->t_s = (float) ((now_ms - controller->start_ms) / 1000.0);
            row->weight = (float) weight;
        } else {
            controller->fit_cut = true;
        }
    }
    choose(controller);

    // The coarse tube: today's
    double coarse_error = controller->coarse_target - weight;
    if (coarse_error <= 0.0 && controller->coarse_moves) {
        controller->coarse_moves = false;
        controller->coarse_rps = 0.0;
        backoff(controller, now_ms);
    }
    double elapsed_ms = now_ms - controller->last_sample_ms;
    if (controller->coarse_moves && !controller->backoff_on) {
        controller->coarse_integral += coarse_error;
        double derivative = derivative_of(controller, coarse_error, controller->coarse_last_error, elapsed_ms);
        double speed = config->coarse.kp * coarse_error + config->coarse.ki * controller->coarse_integral +
                       config->coarse.kd * derivative;
        controller->coarse_rps = fmax(config->coarse.least_rps, fmin(speed, config->coarse.most_rps));
    }

    // The fine tube: the profile's PID on the forecast's error, within its envelope
    double error = controller->target - forecast(controller);
    if (!controller->coarse_moves && error <= 0.0) {
        stop(controller);
        return;
    }
    if (!controller->held) {
        controller->fine_integral += error;
    }
    double last = controller->fine_last_known ? controller->fine_last_error : error;
    double derivative = derivative_of(controller, error, last, elapsed_ms);
    double asked = config->fine.kp * error + config->fine.ki * controller->fine_integral + config->fine.kd * derivative;
    double least = config->fine.least_rps;
    double top = config->fine.most_rps;
    if (controller->k_known && controller->k > 0.0) {
        // No more powder in the air than the room left
        double lag = lag_at(config, controller->fine_speed * controller->k);
        top = fmin(top, least + fmax(0.0, error) / (controller->k * lag));
    }
    controller->fine_rps = fmax(least, fmin(asked, top));
    controller->held = !(least < asked && asked < top);
    controller->last_sample_ms = now_ms;
    controller->fine_last_error = error;
    controller->fine_last_known = true;
    controller->coarse_last_error = coarse_error;
    controller->first_sample = false;
}


double charge_controller_verdict_after_s(const charge_controller_t *controller) {
    return lag_most(&controller->config) + controller->config.settle_s;
}


void charge_controller_at_rest(charge_controller_t *controller, double reading) {
    const charge_config_t *config = &controller->config;
    if (!controller->rest_known || controller->rest_cut) {
        return;
    }
    double quiet = controller->rest_quiet_s;
    double at_quiet = controller->rest_at_quiet;
    double k = controller->k_known ? controller->k : (controller->own_known ? controller->own : 0.0);
    // The fine turns not yet shown at the quiet: every parcel due after it, and the one being turned out
    double alone = controller->parcel + controller->rest_certain;
    for (int i = 0; i < controller->edge_count; i++) {
        const charge_parcel_t *parcel = &controller->edge[i];
        if (parcel_at_s(controller, parcel) + lag_at(config, parcel_speed(parcel) * k) > quiet) {
            alone += parcel->turns;
        }
    }
    if (alone >= REST_TURNS && reading > at_quiet) {
        if (controller->carry_count == CHARGE_CARRY_THROWS) {
            memmove(controller->carry, &controller->carry[1], (CHARGE_CARRY_THROWS - 1) * sizeof(double));
            controller->carry_count--;
        }
        controller->carry[controller->carry_count++] = (reading - at_quiet) / alone;
    }
}
