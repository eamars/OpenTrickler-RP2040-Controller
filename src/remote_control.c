/*
 * The remote control's commands (ADR-001, step 3). One command line in, one reply line out:
 * `@ok <command> ...` or `@err <reason>`. Lines a command sends before its reply also start with `@`;
 * everything else on the console is log. No transport here: rc_console.c feeds it lines.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "remote_control.h"
#include "charge_mode.h"
#include "scale.h"
#include "profile.h"
#include "eeprom.h"
#include "version.h"


// Longest list of tasks `stats` looks through for the smallest stack margin
#define REMOTE_CONTROL_MAX_TASKS 24

// `reboot` restarts this long after its reply is out, so the host has read it
#define REMOTE_CONTROL_REBOOT_DELAY_MS 500

#if PICO_RP2350
#define REMOTE_CONTROL_BOARD "pico2_w"
#else
#define REMOTE_CONTROL_BOARD "pico_w"
#endif


typedef bool (*remote_control_command_fn)(const char *args, char *reply, size_t reply_size,
                                          remote_control_write_fn write, void *write_ctx);

typedef struct {
    const char *name;
    remote_control_command_fn fn;
} remote_control_command_t;


extern charge_mode_config_t charge_mode_config;
extern scale_config_t scale_config;
extern eeprom_metadata_t metadata;

static bool remote_control_reboot_pending = false;
static TaskStatus_t remote_control_task_status[REMOTE_CONTROL_MAX_TASKS];


static bool remote_control_help(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx);


static bool remote_control_state(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    profile_t *profile = profile_get_selected();
    uint32_t up_s = (uint32_t) (time_us_64() / 1000000);

    // unique_id and the profile name are stored without a terminator: print them by their length
    snprintf(reply, reply_size,
             "@ok state charge=%d weight=%.3f target=%.3f scale=\"%s\" ver=%s vcs=%s build=%s board=%s id=%.*s "
             "up=%lu profile=\"%.*s\"",
             (int) charge_mode_config.charge_mode_state,
             scale_config.current_scale_measurement,
             charge_mode_config.target_charge_weight,
             get_scale_driver_string(),
             version_string, vcs_hash, build_type,
             REMOTE_CONTROL_BOARD,
             (int) sizeof(metadata.unique_id), metadata.unique_id,
             (unsigned long) up_s,
             (int) sizeof(profile->name), profile->name);
    return true;
}


static bool remote_control_stats(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                 void *write_ctx) {
    UBaseType_t task_count = uxTaskGetSystemState(remote_control_task_status, REMOTE_CONTROL_MAX_TASKS, NULL);
    if (task_count == 0) {
        snprintf(reply, reply_size, "@err more than %d tasks", REMOTE_CONTROL_MAX_TASKS);
        return false;
    }

    // The task closest to overflowing its stack
    UBaseType_t min_idx = 0;
    for (UBaseType_t idx = 1; idx < task_count; idx += 1) {
        if (remote_control_task_status[idx].usStackHighWaterMark < remote_control_task_status[min_idx].usStackHighWaterMark) {
            min_idx = idx;
        }
    }

    snprintf(reply, reply_size, "@ok stats heap_free=%u heap_min=%u tasks=%u stack_min=\"%s\":%u",
             (unsigned) xPortGetFreeHeapSize(),
             (unsigned) xPortGetMinimumEverFreeHeapSize(),
             (unsigned) task_count,
             remote_control_task_status[min_idx].pcTaskName,
             (unsigned) remote_control_task_status[min_idx].usStackHighWaterMark);
    return true;
}


static bool remote_control_reboot(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                  void *write_ctx) {
    remote_control_reboot_pending = true;
    snprintf(reply, reply_size, "@ok reboot");
    return true;
}


static const remote_control_command_t remote_control_commands[] = {
    {"help", remote_control_help},
    {"state", remote_control_state},
    {"stats", remote_control_stats},
    {"reboot", remote_control_reboot},
};

#define REMOTE_CONTROL_COMMAND_COUNT (sizeof(remote_control_commands) / sizeof(remote_control_commands[0]))


static bool remote_control_help(const char *args, char *reply, size_t reply_size, remote_control_write_fn write,
                                void *write_ctx) {
    size_t len = snprintf(reply, reply_size, "@ok help");
    for (size_t idx = 0; idx < REMOTE_CONTROL_COMMAND_COUNT && len < reply_size; idx += 1) {
        len += snprintf(reply + len, reply_size - len, " %s", remote_control_commands[idx].name);
    }
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

    snprintf(reply, reply_size, "@err unknown command \"%.*s\"", (int) name_len, line);
    return false;
}


void remote_control_after_reply(void) {
    if (remote_control_reboot_pending) {
        watchdog_reboot(0, 0, REMOTE_CONTROL_REBOOT_DELAY_MS);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
