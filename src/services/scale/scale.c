#include <math.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <semphr.h>
#include <inttypes.h>
#include <string.h>

#include "chip.h"
#include "chip_serial.h"
#include "rtos_port.h"

#include "configuration.h"
#include "scale.h"
#include "eeprom.h"
#include "scale.h"
#include "common.h"
#include "settings.h"
#include "scale_reader.h"

extern scale_handle_t simulated_scale_handle;

scale_config_t scale_config;

// The stored driver's task, and the simulated scale's while the remote control has it on (ADR-001)
static TaskHandle_t scale_task_handle = NULL;
static TaskHandle_t scale_simulation_task_handle = NULL;
static scale_driver_t scale_simulation_stored_driver;

const eeprom_scale_data_t default_scale_persistent_config = {
    .scale_data_rev = 0,
    .scale_driver = SCALE_DRIVER_AND_FXI,
    .scale_baudrate = BAUDRATE_19200,
    .scale_uart_format = UART_FMT_8D_1S_NP,
    // The calibration's scale: never calibrated
    .settle_ms = -1.0f,
    .lag = {{-1.0f, -1.0f}, {-1.0f, -1.0f}, {-1.0f, -1.0f}, {-1.0f, -1.0f}, {-1.0f, -1.0f}},
};


/*
    The driver that runs, which the stored one is but while the remote control's simulated scale stands in for it
    (ADR-003, gate 3: a test stand-in is never in the stored settings, so no save stores it)
*/
static scale_driver_t scale_running_driver = SCALE_DRIVER_AND_FXI;


// The makes the build carries, handed over by the start-up's wiring (scale_register_make())
#define SCALE_MAKES_MAX 12
static struct {
    scale_driver_t driver;
    const scale_make_t *make;
} scale_makes[SCALE_MAKES_MAX];
static size_t scale_make_count = 0;

// The make the reader reads the port for; NULL while the simulated scale runs, and with no make for the driver
static const scale_make_t *scale_running_make = NULL;


bool scale_register_make(scale_driver_t driver, const scale_make_t *make) {
    if (scale_make_count >= SCALE_MAKES_MAX) {
        return false;
    }
    scale_makes[scale_make_count].driver = driver;
    scale_makes[scale_make_count].make = make;
    scale_make_count += 1;
    return true;
}


static const scale_make_t *scale_make_for(scale_driver_t driver) {
    for (size_t idx = 0; idx < scale_make_count; idx += 1) {
        if (scale_makes[idx].driver == driver) {
            return scale_makes[idx].make;
        }
    }
    return NULL;
}


// The make that reads the scale: the one asked for, or none. A make the build does not carry, or a number that is no
// make, has nothing read in its place (ADR-006, gate 9): no reading comes, and what needs one refuses (scale_reads())
static void scale_use_driver(scale_driver_t scale_driver) {
    scale_running_driver = scale_driver;
    scale_running_make = scale_driver == SCALE_DRIVER_SIMULATED ? NULL : scale_make_for(scale_driver);
}


bool scale_reads(void) {
    return scale_running_driver == SCALE_DRIVER_SIMULATED || scale_running_make != NULL;
}


void set_scale_driver(scale_driver_t scale_driver) {
    // Update the persistent settings
    scale_config.persistent_config.scale_driver = scale_driver;
    scale_use_driver(scale_driver);
}


scale_driver_t scale_get_running_driver(void) {
    return scale_running_driver;
}


/*
    The weight's unit, where the make's frame carries it (ADR-003, gate 4): the A&D's, from its last good frame, in
    lower case and trimmed ("g", "gn"). Every other make cannot tell.
*/
static char scale_unit[4] = "";


void scale_publish_unit(const char *unit, size_t len) {
    char trimmed[4];
    size_t used = 0;
    for (size_t idx = 0; idx < len && used < sizeof(trimmed) - 1; idx += 1) {
        char c = unit[idx];
        if (c == ' ') {
            continue;
        }
        trimmed[used++] = (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
    }
    trimmed[used] = '\0';
    memcpy(scale_unit, trimmed, sizeof(scale_unit));
}


const char *scale_get_unit(void) {
    if (scale_running_driver != SCALE_DRIVER_AND_FXI || scale_unit[0] == '\0') {
        return NULL;
    }
    return scale_unit;
}


/*
    Each zero asked, at the unit or over the API, counted (gate 4): a charge waiting for zero sees one came, and does
    not take readings from before it, or a zero still to land, for a steady zero
*/
static volatile uint32_t scale_zero_count = 0;


void scale_ask_zero(void) {
    scale_zero_count += 1;
    if (scale_running_driver == SCALE_DRIVER_SIMULATED) {
        simulated_scale_handle.force_zero();
    }
    else if (scale_running_make != NULL && scale_running_make->zero != NULL) {
        scale_write(scale_running_make->zero, strlen(scale_running_make->zero));
    }
}


uint32_t scale_get_zero_count(void) {
    return scale_zero_count;
}


bool scale_can_zero(void) {
    if (scale_running_driver == SCALE_DRIVER_SIMULATED) {
        return true;
    }
    // The make says its zero command, where it has one (scale_make.h)
    return scale_running_make != NULL && scale_running_make->zero != NULL;
}

void set_scale_uart_format(scale_uart_format_t format) {
    scale_config.persistent_config.scale_uart_format = format;

    switch (format) {
        case UART_FMT_8D_1S_NP:
            chip_serial_format(SCALE_UART, 8, 1, CHIP_SERIAL_PARITY_NONE);
            break;
        case UART_FMT_7D_1S_NP:
            chip_serial_format(SCALE_UART, 7, 1, CHIP_SERIAL_PARITY_NONE);
            break;
        default:
            break;
    }
}


uint32_t get_scale_baudrate(scale_baudrate_t scale_baudrate) {
    uint32_t baudrate_uint = 0;

    switch (scale_baudrate) {
        case BAUDRATE_4800:
            baudrate_uint = 4800;
            break;
        case BAUDRATE_9600:
            baudrate_uint = 9600;
            break;
        case BAUDRATE_19200:
            baudrate_uint = 19200;
            break;
        default:
            break;
    }

    return baudrate_uint;
}

void set_scale_baudrate(scale_baudrate_t baudrate) {
    scale_config.persistent_config.scale_baudrate = baudrate;
    chip_serial_baud(SCALE_UART, get_scale_baudrate(baudrate));
}


const char * get_scale_driver_string() {
    const char * scale_driver_string = NULL;
    if (!scale_reads()) {
        return "none: the stored make is not in this build";
    }

    switch (scale_running_driver) {
        case SCALE_DRIVER_AND_FXI:
            scale_driver_string = "AND FX-i Std";
            break;
        case SCALE_DRIVER_STEINBERG_SBS:
            scale_driver_string = "Steinberg SBS";
            break;
        case SCALE_DRIVER_USSOLID_JFDBS:
            scale_driver_string = "US Solid JFDBS";
            break;
        case SCALE_DRIVER_GNG_JJB:
            scale_driver_string = "GNG JJB";
            break;
        case SCALE_DRIVER_JM_SCIENCE:
            scale_driver_string = "JM Science";
            break;
        case SCALE_DRIVER_CREEDMOOR:
            scale_driver_string = "Creedmoor";
            break;
        case SCALE_DRIVER_RADWAG_PS_R2:
            scale_driver_string = "Radwag PS R2";
            break;
        case SCALE_DRIVER_SARTORIUS:
            scale_driver_string = "Sartorius";
            break;
        case SCALE_DRIVER_GENERIC_DRV:
            scale_driver_string = "Generic Driver";
            break;
        case SCALE_DRIVER_FA_EN:
            scale_driver_string = "FA-E/EN";
            break;
        case SCALE_DRIVER_SIMULATED:
            scale_driver_string = "Simulated Scale";
            break;
        default:
            break;
    }

    return scale_driver_string;
}


/*
    The scale's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/settings/scale). The stored driver
    only: the simulated scale standing in for it never shows here, and no save stores it.
*/
static const setting_choice_t scale_driver_choices[] = {
    {.held = SCALE_DRIVER_AND_FXI, .text = "and_fxi", .label = "A&D FX-i"},
    {.held = SCALE_DRIVER_STEINBERG_SBS, .text = "steinberg_sbs", .label = "Steinberg SBS"},
    {.held = SCALE_DRIVER_GNG_JJB, .text = "gng_jjb", .label = "G&G JJB"},
    {.held = SCALE_DRIVER_USSOLID_JFDBS, .text = "ussolid_jfdbs", .label = "US Solid JFDBS"},
    {.held = SCALE_DRIVER_JM_SCIENCE, .text = "jm_science", .label = "JM Science"},
    {.held = SCALE_DRIVER_CREEDMOOR, .text = "creedmoor", .label = "Creedmoor"},
    {.held = SCALE_DRIVER_RADWAG_PS_R2, .text = "radwag_ps_r2", .label = "Radwag PS R2"},
    {.held = SCALE_DRIVER_SARTORIUS, .text = "sartorius", .label = "Sartorius"},
    {.held = SCALE_DRIVER_GENERIC_DRV, .text = "generic", .label = "Generic"},
    {.held = SCALE_DRIVER_FA_EN, .text = "fa_en", .label = "FA-E/EN"},
    {.held = SCALE_DRIVER_SIMULATED, .text = "simulated", .label = "Simulated (no scale)", .short_label = "Simulated"},
};

static const setting_choice_t scale_baud_choices[] = {
    {.held = BAUDRATE_4800, .number = 4800, .label = "4800 baud", .short_label = "4800"},
    {.held = BAUDRATE_9600, .number = 9600, .label = "9600 baud", .short_label = "9600"},
    {.held = BAUDRATE_19200, .number = 19200, .label = "19200 baud", .short_label = "19200"},
};

static const setting_choice_t scale_format_choices[] = {
    {.held = UART_FMT_8D_1S_NP, .text = "8n1", .label = "8 data bits, no parity, 1 stop bit", .short_label = "8N1"},
    {.held = UART_FMT_7D_1S_NP, .text = "7n1", .label = "7 data bits, no parity, 1 stop bit", .short_label = "7N1"},
};


#define SCALE_FIELD(member) SETTING_AT(eeprom_scale_data_t, member)
// One flow of the lag's table, its two fields: lag_<n>_flow_per_s and lag_<n>_ms
#define SCALE_LAG_FIELDS(n) \
    {.name = "lag_" #n "_flow_per_s", .label = "Lag " #n ", flow", .group = "Calibration", \
     .about = "The flow the scale's lag was measured at, slowest first; null, never calibrated", \
     .type = SETTING_NUMBER, SCALE_FIELD(lag[n - 1].flow_per_s), .unit = "weight/s", .min = 0.0f, .max = 1000.0f, \
     .nullable = true, .null_held = -1.0f, .applies = "now"}, \
    {.name = "lag_" #n "_ms", .label = "Lag " #n, .group = "Calibration", \
     .about = "How far the reading ran behind the powder poured at that flow; null, never calibrated", \
     .type = SETTING_NUMBER, SCALE_FIELD(lag[n - 1].lag_ms), .unit = "ms", .min = 0.0f, .max = 10000.0f, \
     .nullable = true, .null_held = -1.0f, .applies = "now"}

static const setting_field_t scale_fields[] = {
    {.name = "driver", .label = "Make", .short_label = "Make",
     .about = "The make of scale, whose driver reads it; its reader is made at the start",
     .type = SETTING_CHOICE, SCALE_FIELD(scale_driver), SETTING_CHOICES(scale_driver_choices),
     .applies = "at_restart", .unknown_kept = true},    // a make no make has reads nothing (ADR-006, gate 9)
    {.name = "baud", .label = "Baud rate", .group = "Serial port",
     .about = "The scale's serial port speed, as the scale is set",
     .type = SETTING_CHOICE, SCALE_FIELD(scale_baudrate), SETTING_CHOICES(scale_baud_choices), .applies = "now"},
    {.name = "format", .label = "Frame format", .group = "Serial port",
     .about = "The scale's serial frame, as the scale is set",
     .type = SETTING_CHOICE, SCALE_FIELD(scale_uart_format), SETTING_CHOICES(scale_format_choices),
     .applies = "now"},
    // ADR-009 step 1: the calibration's scale, items 3 and 4
    {.name = "settle_ms", .label = "Settle time", .group = "Calibration",
     .about = "How long a small change takes from the reading's first move to its rest, as the calibration found it; "
              "null, never calibrated",
     .type = SETTING_NUMBER, SCALE_FIELD(settle_ms), .unit = "ms", .min = 0.0f, .max = 30000.0f,
     .nullable = true, .null_held = -1.0f, .applies = "now"},
    SCALE_LAG_FIELDS(1),
    SCALE_LAG_FIELDS(2),
    SCALE_LAG_FIELDS(3),
    SCALE_LAG_FIELDS(4),
    SCALE_LAG_FIELDS(5),
};


static void *scale_settings_live(int32_t id) {
    (void) id;
    return &scale_config.persistent_config;
}


static void scale_settings_apply(const void *before, int32_t id) {
    (void) id;
    const eeprom_scale_data_t *was = before;
    if (scale_config.persistent_config.scale_baudrate != was->scale_baudrate) {
        set_scale_baudrate(scale_config.persistent_config.scale_baudrate);
    }
    if (scale_config.persistent_config.scale_uart_format != was->scale_uart_format) {
        set_scale_uart_format(scale_config.persistent_config.scale_uart_format);
    }
}


static bool scale_settings_is_saved(void) {
    return config_is_saved(EEPROM_SCALE_CONFIG_BASE_ADDR, &scale_config.persistent_config,
                           sizeof(eeprom_scale_data_t));
}


static const settings_resource_t scale_settings = {
    .path = "/api/v1/settings/scale",
    .save_path = "/api/v1/settings/scale/save",
    .revert_path = "/api/v1/settings/scale/revert",
    .addr = EEPROM_SCALE_CONFIG_BASE_ADDR,
    .label = "Scale",
    .about = "The scale on the serial port",
    SETTING_FIELDS(scale_fields),
    .refused_while = SETTINGS_REFUSED_IN_THROW,
    .size = sizeof(eeprom_scale_data_t),
    .live = scale_settings_live,
    .apply = scale_settings_apply,
    .save = scale_config_save,
    .is_saved = scale_settings_is_saved,
};


const char *scale_driver_api_name(scale_driver_t driver) {
    for (size_t idx = 0; idx < sizeof(scale_driver_choices) / sizeof(scale_driver_choices[0]); idx += 1) {
        if (scale_driver_choices[idx].held == (int32_t) driver) {
            return scale_driver_choices[idx].text;
        }
    }
    return "other";
}


bool scale_init() {
    bool is_ok = true;     // the scale starts whatever its load found (ADR-017, gate 1)

    // Read config from EEPROM: whatever is found, the struct holds settings to run on (ADR-017, gate 1)
    load_config(EEPROM_SCALE_CONFIG_BASE_ADDR, &scale_config.persistent_config, &default_scale_persistent_config,
                sizeof(eeprom_scale_data_t));
#if RC_TEST_SCALE_MAKE_UNKNOWN
    // A test build (ADR-017, gate 1): a make no make has, stored, as only a damaged byte or another firmware could
    scale_config.persistent_config.scale_driver = (scale_driver_t) 55;
    save_config(EEPROM_SCALE_CONFIG_BASE_ADDR, &scale_config.persistent_config, sizeof(eeprom_scale_data_t));
#endif

    // The scale's serial port, at its stored rate and in its stored format
    chip_serial_open(SCALE_UART, SCALE_UART_TX, SCALE_UART_RX,
                     get_scale_baudrate(scale_config.persistent_config.scale_baudrate));
    set_scale_uart_format(scale_config.persistent_config.scale_uart_format);

    // Create control variables
    // Semaphore to indicate the availability of new measurement. 
    scale_config.scale_measurement_ready = xSemaphoreCreateBinary();

    // Mutex to control the access to the serial port write
    scale_config.scale_serial_write_access_mutex = xSemaphoreCreateMutex();

    // Initialize the measurement variable
    scale_config.current_scale_measurement = NAN;

    // Initialize the driver handle
    printf("Scale driver: %x\n", scale_config.persistent_config.scale_driver);
    set_scale_driver(scale_config.persistent_config.scale_driver);

    // The task that reads: the port's one reader for a make (ADR-017, gate 2), or the simulated scale's. Its handle
    // is kept so the simulation can stand in for it. A stored make the build does not carry runs nothing in its
    // place: said now, and a charge and a calibration refuse
    if (scale_running_make != NULL) {
        if (!rtos_task_start(scale_reader_task, "Scale Task", 1024, (void *) scale_running_make, 9,
                             &scale_task_handle)) {
            printf("Scale: no room for its reader's task\n");
        }
    }
    else if (scale_running_driver == SCALE_DRIVER_SIMULATED) {
        rtos_task_start(simulated_scale_handle.read_loop_task, "Scale Task", 1024, NULL, 9, &scale_task_handle);
    }
    else {
        printf("Scale: the stored make (%d) is not in this build: no scale reads, and a charge and a calibration "
               "are refused\n", (int) scale_config.persistent_config.scale_driver);
    }

    settings_register(&scale_settings);

    return is_ok;
}


bool scale_simulation_start(void) {
    if (scale_simulation_task_handle != NULL) {
        return true;
    }
    // A unit that runs the simulated driver already has it; there is nothing to stand in for. With no make in the
    // build for the stored one there is no task to stand in for, and the simulation, asked, runs alone
    if (scale_running_driver == SCALE_DRIVER_SIMULATED) {
        return false;
    }

    scale_simulation_stored_driver = scale_running_driver;
    if (scale_task_handle != NULL) {
        vTaskSuspend(scale_task_handle);
    }

    // The simulated scale publishes only while it is the driver that runs. The stored settings do not change
    // (ADR-003, gate 3): no save stores the stand-in, and a restart brings the driver that ran back
    simulated_scale_handle.force_zero();
    scale_use_driver(SCALE_DRIVER_SIMULATED);

    if (!rtos_task_start(simulated_scale_handle.read_loop_task, "Sim Scale Task", 1024, NULL, 9,
                         &scale_simulation_task_handle)) {
        scale_simulation_task_handle = NULL;
        scale_use_driver(scale_simulation_stored_driver);
        if (scale_task_handle != NULL) {
            vTaskResume(scale_task_handle);
        }
        return false;
    }
    return true;
}


void scale_simulation_stop(void) {
    if (scale_simulation_task_handle == NULL) {
        return;
    }

    vTaskDelete(scale_simulation_task_handle);
    scale_simulation_task_handle = NULL;
    scale_use_driver(scale_simulation_stored_driver);

    // Drop what the scale sent while its task was suspended; the reading is unknown until its next frame, which the
    // reader finds afresh: what it held is of a frame before, and the port's next bytes are in a frame's middle
    while (chip_serial_read(SCALE_UART) != CHIP_SERIAL_NONE) {
    }
    scale_reader_restart();
    scale_config.current_scale_measurement = NAN;
    if (scale_task_handle != NULL) {
        vTaskResume(scale_task_handle);
    }
}


// The frame tap (ADR-013): written by the console's task, read by the driver's, a bool either way
static volatile bool scale_tap_on = false;


void scale_tap_set(bool on) {
    scale_tap_on = on;
}


bool scale_tap_is_on(void) {
    return scale_tap_on;
}


void scale_tap_frame(const uint8_t * bytes, size_t len, float value) {
    if (!scale_tap_on) {
        return;
    }

    // One line, built whole and printed with one call, so lines of other tasks do not cut into it. Hex, because
    // a frame that does not parse may hold anything, its line ending included.
    char line[96];
    size_t pos = snprintf(line, sizeof(line), "@sf %lu %g ", (unsigned long) (chip_time_us() / 1000), value);
    for (size_t idx = 0; idx < len && pos + 3 < sizeof(line); idx += 1) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02x", bytes[idx]);
    }
    printf("%s\n", line);
}


bool scale_simulation_is_on(void) {
    return scale_simulation_task_handle != NULL;
}


const eeprom_scale_data_t *scale_get_stored(void) {
    return &scale_config.persistent_config;
}


bool scale_config_save() {
    bool is_ok = save_config(EEPROM_SCALE_CONFIG_BASE_ADDR, &scale_config.persistent_config, sizeof(eeprom_scale_data_t));
    return is_ok;
}


static inline void _take_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreTake(scale_config.scale_serial_write_access_mutex, portMAX_DELAY);
    }
}


static inline void _give_mutex(BaseType_t scheduler_state) {
    if (scheduler_state != taskSCHEDULER_NOT_STARTED){
        xSemaphoreGive(scale_config.scale_serial_write_access_mutex);
    }
}


void scale_write(const char * command, size_t len) {
    BaseType_t scheduler_state = xTaskGetSchedulerState();

    _take_mutex(scheduler_state);

    chip_serial_write(SCALE_UART, (const uint8_t *) command, len);

    _give_mutex(scheduler_state);
}


float scale_get_current_measurement() {
    return scale_config.current_scale_measurement;
}


// Frames that did not give a finite number since the start (ADR-013); written by one driver task at a time
static volatile uint32_t scale_bad_frames = 0;

// The scale's rhythm (ADR-013): the time between its recent readings, kept by the publishing task, read by the
// charge's. One 32-bit word at a time either way.
#define SCALE_RHYTHM_INTERVALS 16
static volatile uint32_t scale_rhythm_ms[SCALE_RHYTHM_INTERVALS];
static volatile uint32_t scale_rhythm_count = 0;        // intervals kept since the start; the next goes at count % N
static volatile TickType_t scale_last_reading_tick = 0;
static volatile bool scale_has_read = false;


void scale_publish_measurement(float weight) {
    if (!isfinite(weight)) {
        scale_bad_frames += 1;
        return;
    }

    TickType_t now = xTaskGetTickCount();
    if (scale_has_read) {
        scale_rhythm_ms[scale_rhythm_count % SCALE_RHYTHM_INTERVALS] = (now - scale_last_reading_tick) * portTICK_PERIOD_MS;
        scale_rhythm_count += 1;
    }
    scale_last_reading_tick = now;
    scale_has_read = true;

    scale_config.current_scale_measurement = weight;
    if (scale_config.scale_measurement_ready) {
        xSemaphoreGive(scale_config.scale_measurement_ready);
    }
}


uint32_t scale_get_bad_frames(void) {
    return scale_bad_frames;
}


uint32_t scale_get_usual_interval_ms(void) {
    uint32_t count = scale_rhythm_count < SCALE_RHYTHM_INTERVALS ? scale_rhythm_count : SCALE_RHYTHM_INTERVALS;
    if (count < SCALE_RHYTHM_MIN_INTERVALS) {
        return 0;
    }

    // The median: a gap now and then, or a frame dropped at the door, does not move it
    uint32_t sorted[SCALE_RHYTHM_INTERVALS];
    for (uint32_t idx = 0; idx < count; idx += 1) {
        uint32_t value = scale_rhythm_ms[idx];
        uint32_t pos = idx;
        while (pos > 0 && sorted[pos - 1] > value) {
            sorted[pos] = sorted[pos - 1];
            pos -= 1;
        }
        sorted[pos] = value;
    }
    return sorted[count / 2];
}


bool scale_take_new_measurement(float * current_measurement) {
    if (xSemaphoreTake(scale_config.scale_measurement_ready, 0) == pdTRUE) {
        *current_measurement = scale_get_current_measurement();
        return true;
    }
    return false;
}


/*
    Block wait for the next available measurement.

    block_time_ms set to 0 to wait indefinitely.
*/
bool scale_block_wait_for_next_measurement(uint32_t block_time_ms, float * current_measurement) {
    TickType_t delay_ticks;

    if (block_time_ms == 0) {
        delay_ticks = portMAX_DELAY;
    }
    else {
        delay_ticks = pdMS_TO_TICKS(block_time_ms);
    }

    // You can only call this once the scheduler starts
    if (xSemaphoreTake(scale_config.scale_measurement_ready, delay_ticks) == pdTRUE){
        *current_measurement = scale_get_current_measurement();

        return true;
    }
    
    // No valid measurement
    return false;
}


