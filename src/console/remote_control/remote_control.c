/*
 * The remote control's commands (ADR-001, step 3). One command line in, one reply line out:
 * `@ok <command> ...` or `@err <reason>`. Lines a command sends before its reply also start with `@`;
 * everything else on the console is log. No transport here: rc_console.c feeds it lines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "chip.h"

#include "remote_control.h"
#include "system_control.h"
#include "charge_mode.h"
#include "scale.h"
#include "profile.h"
#include "eeprom.h"
#include "unit_id.h"
#include "version.h"
#include "motors.h"
#include "settings.h"
#include "activity.h"
#include "update.h"
#include "calibration.h"
#include "wireless.h"


// Longest list of tasks `stats` looks through for the smallest stack margin
#define REMOTE_CONTROL_MAX_TASKS 24

// `reboot` restarts this long after its reply is out, so the host has read it
#define REMOTE_CONTROL_REBOOT_DELAY_MS 500

typedef struct {
    const char *name;
    remote_control_command_fn fn;
} remote_control_command_t;

// Other modules' commands, handed over at start-up (remote_control_register())
#define REMOTE_CONTROL_HANDED_MAX 12
static remote_control_command_t remote_control_handed[REMOTE_CONTROL_HANDED_MAX];
static size_t remote_control_handed_count = 0;



static bool remote_control_reboot_pending = false;
static char remote_control_tmc_line[256];
static TaskStatus_t remote_control_task_status[REMOTE_CONTROL_MAX_TASKS];


static bool remote_control_help(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx);


static bool remote_control_state(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    profile_t *profile = profile_get_selected();
    uint32_t up_s = (uint32_t) (chip_time_us() / 1000000);
    char unit_id[UNIT_ID_LENGTH + 1];
    unit_id_get(unit_id, sizeof(unit_id));

    // The profile's name is stored without a terminator: print it by its length
    snprintf(reply, reply_size,
             "@ok state charge=%d weight=%.3f target=%.3f scale=\"%s\" ver=%s vcs=%s build=%s board=%s id=%s "
             "up=%lu profile=\"%.*s\" form=%d sim=%d coarse=%.3f fine=%.3f drivers=%s,%s",
             (int) charge_mode_get_state(),
             scale_get_current_measurement(),
             charge_mode_get_throw_target(),
             get_scale_driver_string(),
             version_string, vcs_hash, build_type,
             system_board_name(),
             unit_id,
             (unsigned long) up_s,
             (int) sizeof(profile->name), profile->name,
             activity_screen_takes_starts() ? 0 : -1,
             scale_simulation_is_on() ? 1 : 0,
             motor_get_velocity(SELECT_COARSE_TRICKLER_MOTOR),
             motor_get_velocity(SELECT_FINE_TRICKLER_MOTOR),
             motor_driver_get_status_string(SELECT_COARSE_TRICKLER_MOTOR),
             motor_driver_get_status_string(SELECT_FINE_TRICKLER_MOTOR));
    return true;
}


static bool remote_control_stats(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    UBaseType_t task_count = uxTaskGetSystemState(remote_control_task_status, REMOTE_CONTROL_MAX_TASKS, NULL);
    if (task_count == 0) {
        snprintf(reply, reply_size, "@err more than %d tasks", REMOTE_CONTROL_MAX_TASKS);
        return false;
    }

    // `stats tasks`: each task's stack never touched, in bytes, first (ADR-010, gate 5: the job task's and the poll's
    // stacks were sized from it)
    while (*args == ' ') {
        args += 1;
    }
    if (strcmp(args, "tasks") == 0) {
        for (UBaseType_t idx = 0; idx < task_count; idx += 1) {
            char line[80];
            snprintf(line, sizeof(line), "@task name=\"%s\" stack_left=%lu priority=%u",
                     remote_control_task_status[idx].pcTaskName,
                     (unsigned long) (remote_control_task_status[idx].usStackHighWaterMark * sizeof(StackType_t)),
                     (unsigned) remote_control_task_status[idx].uxCurrentPriority);
            write(line, write_ctx);
        }
    }

    // The task closest to overflowing its stack
    UBaseType_t min_idx = 0;
    for (UBaseType_t idx = 1; idx < task_count; idx += 1) {
        if (remote_control_task_status[idx].usStackHighWaterMark < remote_control_task_status[min_idx].usStackHighWaterMark) {
            min_idx = idx;
        }
    }

    // The charge controller's cost since the start (ADR-009, gate 3): its time each millisecond and each reading, most
    // and mean in microseconds, and the heap's least free while a charge ran
    charge_cost_t cost;
    charge_mode_get_cost(&cost);
    snprintf(reply, reply_size, "@ok stats heap_free=%u heap_min=%u tasks=%u stack_min=\"%s\":%u bad_frames=%lu "
             "charge_tick_us=%lu/%lu charge_reading_us=%lu/%lu charge_heap_low=%lu",
             (unsigned) xPortGetFreeHeapSize(),
             (unsigned) xPortGetMinimumEverFreeHeapSize(),
             (unsigned) task_count,
             remote_control_task_status[min_idx].pcTaskName,
             (unsigned) remote_control_task_status[min_idx].usStackHighWaterMark,
             (unsigned long) scale_get_bad_frames(),
             (unsigned long) cost.tick_most_us, (unsigned long) cost.tick_mean_us,
             (unsigned long) cost.reading_most_us, (unsigned long) cost.reading_mean_us,
             (unsigned long) cost.heap_low);
    return true;
}


static bool remote_control_reboot(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    remote_control_reboot_pending = true;
    snprintf(reply, reply_size, "@ok reboot");
    return true;
}


// `sim on|off`: main's simulated scale stands in for the stored one, for this run only (ADR-001, F2)
static bool remote_control_sim(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                               void *write_ctx) {
    if (strcmp(args, "on") == 0) {
        if (!scale_simulation_start()) {
            snprintf(reply, reply_size, "@err the simulation cannot start: is the stored driver the simulated one?");
            return false;
        }
    }
    else if (strcmp(args, "off") == 0) {
        if (charge_mode_get_state() != CHARGE_MODE_EXIT) {
            snprintf(reply, reply_size, "@err charge mode is running");
            return false;
        }
        scale_simulation_stop();
    }
    // The simulated scale's faults (ADR-013): bad, quiet <ms>, rate <per second>; never stored
    else if (strcmp(args, "bad") == 0 || strncmp(args, "quiet ", 6) == 0) {
        if (!scale_simulation_is_on()) {
            snprintf(reply, reply_size, "@err the simulation is off");
            return false;
        }
        if (args[0] == 'b') {
            simulated_scale_send_bad();
        }
        else {
            char *end;
            long ms = strtol(args + 6, &end, 10);
            if (end == args + 6 || *end != '\0' || ms < 1 || ms > 60000) {
                snprintf(reply, reply_size, "@err usage: sim quiet <ms, 1 to 60000>");
                return false;
            }
            simulated_scale_go_quiet((uint32_t) ms);
        }
    }
    else if (strncmp(args, "rate ", 5) == 0) {
        char *end;
        float per_second = strtof(args + 5, &end);
        if (end == args + 5 || *end != '\0' || !simulated_scale_set_rate(per_second)) {
            snprintf(reply, reply_size, "@err usage: sim rate <readings a second, 1 to 50>");
            return false;
        }
    }
    else if (strncmp(args, "weight ", 7) == 0) {
        // The simulated scale shows this weight until `sim weight off` (ADR-012); never stored
        if (strcmp(args + 7, "off") == 0) {
            simulated_scale_force_weight(false, 0.0f);
        }
        else {
            char *end;
            float weight = strtof(args + 7, &end);
            if (end == args + 7 || *end != '\0' || !isfinite(weight)) {
                snprintf(reply, reply_size, "@err usage: sim weight <a number|off>");
                return false;
            }
            simulated_scale_force_weight(true, weight);
        }
    }
    else if (*args != '\0') {
        snprintf(reply, reply_size, "@err usage: sim [on|off|bad|quiet <ms>|rate <per second>|weight <a number|off>]");
        return false;
    }

    snprintf(reply, reply_size, "@ok sim=%d scale=\"%s\" rate=%.1f", scale_simulation_is_on() ? 1 : 0,
             get_scale_driver_string(), simulated_scale_get_rate());
    return true;
}


// scale tap on|off: the A&D driver copies each frame to the console as "@sf" lines (ADR-013); never stored
static bool remote_control_scale(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    if (strcmp(args, "tap on") == 0) {
        scale_tap_set(true);
    }
    else if (strcmp(args, "tap off") == 0) {
        scale_tap_set(false);
    }
    else if (*args != '\0') {
        snprintf(reply, reply_size, "@err usage: scale [tap on|tap off]");
        return false;
    }

    snprintf(reply, reply_size, "@ok scale tap=%d scale=\"%s\"", scale_tap_is_on() ? 1 : 0, get_scale_driver_string());
    return true;
}


// `motor <coarse|fine> <rev/s>`: enable the driver and turn the trickler; 0 stops it and disables the driver.
// `motor`: the steps made so far
static bool remote_control_motor(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    // Alone: the steps counted on each STEP pin, those fed to the PIO, the feeds that came late, and the rate the
    // pulses were last fed at, with the unit's clock so that two replies give a speed (ADR-012)
    if (*args == '\0') {
        step_pulses_stats_t coarse;
        step_pulses_stats_t fine;
        motor_get_step_stats(SELECT_COARSE_TRICKLER_MOTOR, &coarse);
        motor_get_step_stats(SELECT_FINE_TRICKLER_MOTOR, &fine);
        snprintf(reply, reply_size,
                 "@ok motor t_us=%llu counted=%d coarse_made=%lu coarse_fed=%lu coarse_underruns=%lu coarse_rate=%.2f "
                 "fine_made=%lu fine_fed=%lu fine_underruns=%lu fine_rate=%.2f",
                 (unsigned long long) chip_time_us(), (coarse.counted_on_pin && fine.counted_on_pin) ? 1 : 0,
                 (unsigned long) coarse.steps_made, (unsigned long) coarse.steps_fed, (unsigned long) coarse.underruns,
                 motor_get_step_rate(SELECT_COARSE_TRICKLER_MOTOR),
                 (unsigned long) fine.steps_made, (unsigned long) fine.steps_fed, (unsigned long) fine.underruns,
                 motor_get_step_rate(SELECT_FINE_TRICKLER_MOTOR));
        return true;
    }

    size_t name_len = strcspn(args, " ");
    motor_select_t selected_motor;
    if (name_len == 6 && strncmp(args, "coarse", 6) == 0) {
        selected_motor = SELECT_COARSE_TRICKLER_MOTOR;
    }
    else if (name_len == 4 && strncmp(args, "fine", 4) == 0) {
        selected_motor = SELECT_FINE_TRICKLER_MOTOR;
    }
    else {
        snprintf(reply, reply_size, "@err usage: motor | motor <coarse|fine> <rev/s>");
        return false;
    }

    char *end;
    float speed = strtof(args + name_len, &end);
    if (end == args + name_len || *end != '\0' || !isfinite(speed) ||
        fabsf(speed) > get_motor_max_speed(selected_motor)) {
        snprintf(reply, reply_size, "@err speed: a number of rev/s within the motor's maximum, %u",
                 (unsigned) get_motor_max_speed(selected_motor));
        return false;
    }

    // The modes drive the motors themselves
    if (!activity_screen_takes_starts()) {
        snprintf(reply, reply_size, "@err a mode is running");
        return false;
    }

    if (speed == 0.0f) {
        motor_enable(selected_motor, false);    // also commands speed 0
    }
    else {
        motor_enable(selected_motor, true);
        motor_set_speed(selected_motor, speed);
    }

    snprintf(reply, reply_size, "@ok motor");
    return true;
}


typedef struct {
    remote_control_write_fn write;
    void *write_ctx;
} remote_control_tmc_ctx_t;


static void remote_control_tmc_emit(const char *line, void *ctx) {
    remote_control_tmc_ctx_t *tmc_ctx = (remote_control_tmc_ctx_t *) ctx;

    snprintf(remote_control_tmc_line, sizeof(remote_control_tmc_line), "@tmc %s", line);
    if (tmc_ctx->write) {
        tmc_ctx->write(remote_control_tmc_line, tmc_ctx->write_ctx);
    }
}


/*
 * `tmc <coarse|fine>`: the driver's registers by field, as `@tmc r ...` lines read from the chip now and `@tmc w ...`
 * lines for those that can only be written, then its serial line's counts (ADR-012).
 * `tmc <coarse|fine> setup <n>`: the driver set up n times from the stored settings, nine checked writes each.
 * `tmc <coarse|fine> lose` and `find`: for proofs, the driver made to look absent, and found again. Never stored.
 * `tmc <coarse|fine> fault <short|hot|reset|supply|none>`: for proofs, the driver read to report a fault. Never stored.
 */
static bool remote_control_tmc(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                               void *write_ctx) {
    size_t name_len = strcspn(args, " ");
    motor_select_t selected_motor;
    if (name_len == 6 && strncmp(args, "coarse", 6) == 0) {
        selected_motor = SELECT_COARSE_TRICKLER_MOTOR;
    }
    else if (name_len == 4 && strncmp(args, "fine", 4) == 0) {
        selected_motor = SELECT_FINE_TRICKLER_MOTOR;
    }
    else {
        snprintf(reply, reply_size, "@err usage: tmc <coarse|fine> [setup <n>|lose|find|fault <word>]");
        return false;
    }

    const char *rest = args + name_len;
    while (*rest == ' ') {
        rest += 1;
    }

    int setups_failed = 0;
    if (strncmp(rest, "setup ", 6) == 0) {
        // The motors are still: a driver is not written while a mode turns it
        if (!activity_screen_takes_starts()) {
            snprintf(reply, reply_size, "@err a mode is running");
            return false;
        }

        char *end;
        long count = strtol(rest + 6, &end, 10);
        if (end == rest + 6 || *end != '\0' || count < 1 || count > 1000) {
            snprintf(reply, reply_size, "@err setup: 1 to 1000 times");
            return false;
        }
        for (long idx = 0; idx < count; idx += 1) {
            if (!motor_driver_setup(selected_motor)) {
                setups_failed += 1;
            }
        }
    }
    else if (strncmp(rest, "fault ", 6) == 0) {
        // For proofs, never stored: the driver is read to report this fault until `fault none`
        if (!motor_driver_inject_fault(selected_motor, rest + 6)) {
            snprintf(reply, reply_size, "@err fault: short, hot, reset, supply or none");
            return false;
        }
    }
    else if (strcmp(rest, "lose") == 0 || strcmp(rest, "find") == 0) {
        // For proofs, never stored: the driver looks absent until `find`; a mode, or `setup`, then sets it up again
        motor_driver_set_lost(selected_motor, rest[0] == 'l');
    }
    else if (*rest != '\0') {
        snprintf(reply, reply_size, "@err usage: tmc <coarse|fine> [setup <n>|lose|find|fault <word>]");
        return false;
    }
    else {
        remote_control_tmc_ctx_t tmc_ctx = {write, write_ctx};
        if (!motor_driver_dump(selected_motor, remote_control_tmc_emit, &tmc_ctx)) {
            snprintf(reply, reply_size, "@err the %s driver does not answer", get_motor_select_string(selected_motor));
            return false;
        }
    }

    motor_driver_stats_t stats;
    motor_driver_get_stats(selected_motor, &stats);
    snprintf(reply, reply_size,
             "@ok tmc ready=%d ma=%u writes=%lu write_retries=%lu read_retries=%lu failures=%lu setups_failed=%d",
             stats.ready ? 1 : 0, (unsigned) stats.current_ma,
             (unsigned long) stats.writes, (unsigned long) stats.write_retries,
             (unsigned long) stats.read_retries, (unsigned long) stats.failures, setups_failed);
    return setups_failed == 0;
}


// `charge <gr>` starts charge mode as the API does; `charge stop` stops it, as the API's stop does (ADR-003, gate 4)
static bool remote_control_charge(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    if (strcmp(args, "stop") == 0) {
        if (!activity_request_stop(ACTIVITY_CHARGE, ACTIVITY_BY_CONSOLE)) {
            snprintf(reply, reply_size, "@err charge mode is not running");
            return false;
        }
        snprintf(reply, reply_size, "@ok charge stop");
        return true;
    }

    char *end;
    float target = strtof(args, &end);
    if (end == args || *end != '\0' || !isfinite(target) || target <= 0.0f || target > 1000.0f) {
        snprintf(reply, reply_size, "@err usage: charge <0..1000> | charge stop");
        return false;
    }
    if (activity_get() != ACTIVITY_NONE || !activity_screen_takes_starts()) {
        snprintf(reply, reply_size, "@err a mode is running");
        return false;
    }
    if (!charge_mode_target_fits(target)) {
        snprintf(reply, reply_size, "@err %.3f is not a target the menu's digits can show", target);
        return false;
    }
    switch (charge_mode_request_start(ACTIVITY_BY_CONSOLE, target, -1)) {
        case CHARGE_START_OK:
            break;
        case CHARGE_START_DRIVER_FAULT:
            snprintf(reply, reply_size, "@err a motor driver is not OK: %s", motors_get_fault());
            return false;
        default:
            snprintf(reply, reply_size, "@err charge mode is already running or asked");
            return false;
    }

    snprintf(reply, reply_size, "@ok charge target=%.3f", target);
    return true;
}


// `setting [<path> [name=value ...|save]]`: the API's settings' tables, read and written (ADR-003, gate 3)
static bool remote_control_setting(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                   void *write_ctx) {
    settings_console(args, reply, reply_size, write, write_ctx);
    return strncmp(reply, "@ok", 3) == 0;
}


// `wifi leave`: the station leaves its network, as when its router goes away; it joins again (ADR-004)
static bool remote_control_wifi(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    (void) write;
    (void) write_ctx;
    while (*args == ' ') {
        args += 1;
    }
    // `wifi join`: the stored network now, with its stored key, through the set-up's join (ADR-008, gate 4): only
    // while the unit is its own hotspot; `network`'s station says how it ends
    if (strcmp(args, "join") == 0) {
        wireless_join_result_t taken = wireless_join_stored(ACTIVITY_BY_CONSOLE);
        static const char *const why[] = {"taken", "busy", "not on its hotspot", "no network stored", "bad key",
                                          "bad security"};
        snprintf(reply, reply_size, taken == WIRELESS_JOIN_TAKEN ? "@ok wifi join asked" : "@err failed wifi join: %s",
                 why[taken]);
        return taken == WIRELESS_JOIN_TAKEN;
    }
    // `wifi scan`: the set-up's scan, waited for, then each network heard as an `@scan` line, strongest first
    if (strcmp(args, "scan") == 0) {
        TickType_t started = xTaskGetTickCount();
        if (!wireless_scan_start()) {
            snprintf(reply, reply_size, "@err busy wifi scan: a scan or a join runs");
            return false;
        }
        while (wireless_scanning() && xTaskGetTickCount() - started < pdMS_TO_TICKS(12000)) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        wireless_network_t network;
        size_t found = 0;
        while (wireless_network_at(found, &network)) {
            char line[80];
            snprintf(line, sizeof(line), "@scan ssid=\"%s\" rssi=%d auth=%s", network.ssid, network.rssi,
                     network.auth);
            write(line, write_ctx);
            found += 1;
        }
        snprintf(reply, reply_size, "@ok wifi scan found=%u took_ms=%lu", (unsigned) found,
                 (unsigned long) ((xTaskGetTickCount() - started) * portTICK_PERIOD_MS));
        return true;
    }
    if (strcmp(args, "leave") != 0) {
        snprintf(reply, reply_size, "@err usage wifi leave|join|scan");
        return false;
    }
    bool left = wireless_station_leave();
    snprintf(reply, reply_size, left ? "@ok wifi left" : "@err failed wifi leave");
    return left;
}


// `update [try]`: the image slots, and a restart into a trial of the other one (ADR-004)
// ADR-009 step 1: a part of the calibration started, answered, filled, kept or stopped (calibration_console())
static bool remote_control_calibrate(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                     void *write_ctx) {
    (void) write;
    (void) write_ctx;
    return calibration_console(args, reply, reply_size);
}


static bool remote_control_update(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    (void) write;
    (void) write_ctx;
    return update_console(args, reply, reply_size);
}


static const remote_control_command_t remote_control_commands[] = {
    {"help", remote_control_help},
    {"state", remote_control_state},
    {"stats", remote_control_stats},
    {"reboot", remote_control_reboot},
    {"sim", remote_control_sim},
    {"scale", remote_control_scale},
    {"motor", remote_control_motor},
    {"tmc", remote_control_tmc},
    {"charge", remote_control_charge},
    {"setting", remote_control_setting},
    {"update", remote_control_update},
    {"calibrate", remote_control_calibrate},
    {"wifi", remote_control_wifi},
};

#define REMOTE_CONTROL_COMMAND_COUNT (sizeof(remote_control_commands) / sizeof(remote_control_commands[0]))


static bool remote_control_help(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    size_t len = snprintf(reply, reply_size, "@ok help");
    for (size_t idx = 0; idx < REMOTE_CONTROL_COMMAND_COUNT && len < reply_size; idx += 1) {
        len += snprintf(reply + len, reply_size - len, " %s", remote_control_commands[idx].name);
    }
    for (size_t idx = 0; idx < remote_control_handed_count && len < reply_size; idx += 1) {
        len += snprintf(reply + len, reply_size - len, " %s", remote_control_handed[idx].name);
    }
    return true;
}


bool remote_control_register(const char *name, remote_control_command_fn fn) {
    if (remote_control_handed_count >= REMOTE_CONTROL_HANDED_MAX) {
        printf("Console: no room for the command %s\n", name);
        return false;
    }
    remote_control_handed[remote_control_handed_count].name = name;
    remote_control_handed[remote_control_handed_count].fn = fn;
    remote_control_handed_count += 1;
    return true;
}


bool remote_control_execute(const char *line, char *reply, size_t reply_size, remote_control_write_fn write,
                            void *write_ctx) {
    // The command is the first word; the rest of the line is its arguments
    while (*line == ' ') {
        line += 1;
    }
    size_t name_len = strcspn(line, " ");
    const char *args = line + name_len;
    while (*args == ' ') {
        args += 1;
    }

    for (size_t idx = 0; idx < REMOTE_CONTROL_COMMAND_COUNT; idx += 1) {
        const char *name = remote_control_commands[idx].name;
        if (strlen(name) == name_len && strncmp(name, line, name_len) == 0) {
            return remote_control_commands[idx].fn(args, reply, reply_size, write, write_ctx);
        }
    }
    for (size_t idx = 0; idx < remote_control_handed_count; idx += 1) {
        const char *name = remote_control_handed[idx].name;
        if (strlen(name) == name_len && strncmp(name, line, name_len) == 0) {
            return remote_control_handed[idx].fn(args, reply, reply_size, write, write_ctx);
        }
    }

    snprintf(reply, reply_size, "@err unknown command \"%.*s\"", (int) name_len, line);
    return false;
}


void remote_control_after_reply(void) {
    if (remote_control_reboot_pending) {
        chip_restart(REMOTE_CONTROL_REBOOT_DELAY_MS);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
