/*
 * The charge record's history, for the shared migration (common.c, load_config_layout). ADR-009, 2026-11-26.
 *
 * The rule is never insert, always append (the owner, 2026-11-26). This record was changed by insertion twice before
 * the rule: v1.19 put coarse_stop_gate_ratio in at offset 20, and v2.0.0 (3e469cd) the three backoff fields at offset
 * 24. So it is append-only only from v2.0.0's 64 bytes; a shorter record read as a prefix comes out shifted (ADR-009's
 * review, B1: a v1.21 record turned on a precharge of 4.66 hours). Below 64 bytes it is read only as v1.21's layout, the
 * owner's floor ("your compatibility only back track to v1.21"), field by field; any other shorter record gets the
 * defaults.
 *
 * v1.21's layout as its own build stored it (arm-none-eabi-gdb's ptype /o on v1.21's ELF, 2026-11-26): 52 bytes, the
 * revision at 0, the four thresholds and margins at 4 to 16, the gate ratio at 20, decimal_places at 24 (one byte),
 * precharge_enable at 25, precharge_time_ms at 28, precharge_speed_rps at 32, the four colours at 36 to 48. Where each
 * lies now is the compiler's (offsetof), so the map holds on any build of this struct, the host's too.
 */
#include <stddef.h>

#include "charge_mode.h"

#define CHARGE_V121_SIZE 52
#define CHARGE_APPEND_ONLY_FROM 64      // v2.0.0's record: every change since was an append

#define FROM_V121(at, bytes, member) \
    {.was = (at), .was_size = (bytes), .now = (uint16_t) offsetof(eeprom_charge_mode_data_t, member), \
     .now_size = (uint8_t) sizeof(((eeprom_charge_mode_data_t *) 0)->member)}

static const config_field_map_t charge_from_v121[] = {
    FROM_V121(4, 4, coarse_stop_threshold),
    FROM_V121(8, 4, fine_stop_threshold),
    FROM_V121(12, 4, set_point_sd_margin),
    FROM_V121(16, 4, set_point_mean_margin),
    FROM_V121(20, 4, coarse_stop_gate_ratio),
    FROM_V121(24, 1, decimal_places),
    FROM_V121(25, 1, precharge_enable),
    FROM_V121(28, 4, precharge_time_ms),
    FROM_V121(32, 4, precharge_speed_rps),
    FROM_V121(36, 4, neopixel_normal_charge_colour),
    FROM_V121(40, 4, neopixel_under_charge_colour),
    FROM_V121(44, 4, neopixel_over_charge_colour),
    FROM_V121(48, 4, neopixel_not_ready_colour),
};

static const config_past_layout_t charge_past[] = {
    {.size = CHARGE_V121_SIZE, .fields = charge_from_v121,
     .field_count = sizeof(charge_from_v121) / sizeof(charge_from_v121[0])},
};

const config_layout_t charge_mode_layout = {
    .append_only_from = CHARGE_APPEND_ONLY_FROM,
    .past = charge_past,
    .past_count = sizeof(charge_past) / sizeof(charge_past[0]),
};
