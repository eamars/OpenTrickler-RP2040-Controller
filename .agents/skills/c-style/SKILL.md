---
name: c-style
description: Enforce and review this project's C/C++ coding style for RP2040/RP2350 Pico SDK firmware. Use this skill whenever writing, editing, or reviewing .c, .h, .cpp, .hpp, CMakeLists, Pico SDK module code, FreeRTOS task/queue/semaphore code, u8g2/MUI display code, lwIP/CYW43 wireless or REST endpoint code, EEPROM configuration code, PIO/UART/I2C/SPI/PWM driver code, or embedded target configuration in this repository. Apply it proactively for C/C++ refactors, bug fixes, feature work, and code-review requests; do not wait for the user to ask for style explicitly.
---

# C/C++ Style Guide

This skill captures the C/C++ style used in this repository. Apply it proactively when writing or reviewing firmware code.

This project is Pico SDK firmware for RP2040/RP2350-class boards, with FreeRTOS, u8g2/MUI, lwIP/CYW43 networking, EEPROM-backed configuration, PIO stepper control, and small C++ bridge islands. The strongest style signal is tracked project code under `src/` and target hardware configuration under `targets/`.

Treat `library/` code as third-party unless the user explicitly asks to modify it. Generated files under `src/generated/` should normally be regenerated rather than hand edited.

The style is direct embedded C with pragmatic C++ where it helps integrate C++ helpers or device libraries: explicit initialization, visible hardware control, clear task/queue flow, and small abstractions only where the surrounding module already supports them.

---

## Rule 1 - Match the local module shape

Keep code organized as small module pairs in `src/`: `feature.c` / `feature.h`, or `feature.cpp` / `feature.h` when C++ is required. Public APIs are declared in the header and implemented in the source file. File-local helpers, callbacks, buffers, task handles, and state should be `static` unless existing code deliberately shares them.

Prefer existing module vocabulary and prefixes:

- `*_init`, `*_config_init`, `*_config_save`
- `*_task`, `*_render_task`, `*_wait_for_*`
- `http_rest_*`, `http_get_*`, `populate_rest_*`, `apply_rest_*`
- `*_set_*`, `get_*`, `*_enable`
- hardware names such as `motor_*`, `scale_*`, `servo_gate_*`, `wireless_*`, `neopixel_led_*`

**Wrong:**
```c
void update(void) {
    ...
}
```

**Right:**
```c
static void wireless_update_state(void) {
    ...
}
```

When reviewing: check whether a new symbol should be public. If not, make it `static` and name it with the module's existing vocabulary.

---

## Rule 2 - Use the project's C naming conventions

Use `snake_case` for project-owned functions, variables, fields, files, callbacks, and tasks. Use `*_t` typedef names for structs, enums, and function pointer types where the surrounding code does. Use `ALL_CAPS` for macros and enum values.

Keep Pico SDK, FreeRTOS, lwIP, u8g2, CYW43, and vendor-library names as-is. Do not wrap library types just to make names prettier.

**Wrong:**
```c
typedef struct {
    int TimeoutMs;
} TimerConfig;

void StartTimer(TimerConfig *config);
```

**Right:**
```c
typedef struct {
    int timeout_ms;
} timer_config_t;

void start_timer(timer_config_t *config);
```

When reviewing: flag CamelCase in project-owned C APIs unless it comes from an external type, constant, generated code, or local legacy API that the edit is intentionally preserving.

---

## Rule 3 - Match formatting in the current file

Use four-space indentation. Most source uses K&R braces for functions and control blocks:

```c
bool motor_config_init(void) {
    bool is_ok = true;

    if (!is_ok) {
        return false;
    }

    return true;
}
```

Pointer spacing varies in the existing code, with both `char * s` and `char *s` present. Match the surrounding file instead of imposing a repo-wide rewrite. Avoid formatting-only churn outside the edited area.

Keep designated initializers for config/default structures, with short inline comments where they describe hardware, EEPROM layout, units, or user-visible behavior.

**Right:**
```c
const eeprom_wireless_metadata_t default_eeprom_wireless_metadata = {
    .wireless_data_rev = 0,
    .ssid = "",
    .pw = "",
    .auth = AUTH_WPA2_MIXED_PSK,
    .timeout_ms = 30000,    // 30s
    .enable = false,
};
```

When reviewing: distinguish style consistency from broad cleanup. Prefer focused fixes over reformatting whole files.

---

## Rule 4 - Use local error and return contracts

This project usually returns `bool` for success/failure and small project enums for richer initialization failures. Preserve that style unless the surrounding module already uses another contract.

Use early returns for invalid state, failed allocation, failed hardware initialization, failed EEPROM reads/writes, and task/queue creation failures. Diagnostics are commonly `printf(...)`; keep messages actionable and include hardware/config context when available.

**Wrong:**
```c
int init_sensor(sensor_t *ctx) {
    if (ctx == NULL) {
        return -1;
    }

    return sensor_open(ctx);
}
```

**Right:**
```c
bool sensor_init(sensor_t *ctx) {
    if (ctx == NULL) {
        printf("Sensor context is NULL\n");
        return false;
    }

    if (!sensor_open(ctx)) {
        printf("Unable to initialize sensor\n");
        return false;
    }

    return true;
}
```

For multi-stage hardware initialization, use a module enum when the caller needs to distinguish failures:

```c
typedef enum {
    MOTOR_INIT_OK = 0,
    MOTOR_INIT_CFG_ERR = 2,
    MOTOR_INIT_COARSE_DRV_ERR = 3,
    MOTOR_INIT_FINE_DRV_ERR = 4,
    MOTOR_INIT_PIO_ERR = 5,
} motor_init_err_t;
```

When reviewing: flag silent failures, ignored allocation/task creation failures in new code, and ad-hoc integer return codes when a local enum or `bool` contract already exists.

---

## Rule 5 - Follow the EEPROM configuration pattern

Persistent configuration lives in EEPROM-backed structs with defaults and CRC validation through shared helpers in `common.c`.

The usual pattern is:

- A persistent config struct in the module header, often named `eeprom_*_data_t` or `eeprom_*_metadata_t`
- A global live module config object in the source file
- A `const ... default_*` initializer with `.revision = 0` or similar legacy revision field
- `*_config_init()` calls `load_config(...)`
- `*_config_save()` calls `save_config(...)`
- Successful init registers the save handler with `eeprom_register_handler(...)`
- REST handlers update live config and save only when the `ee` parameter is true

**Right:**
```c
bool wireless_config_save() {
    bool is_ok = save_config(EEPROM_WIRELESS_CONFIG_BASE_ADDR,
                             &wireless_config.eeprom_wireless_metadata,
                             sizeof(eeprom_wireless_metadata_t));
    return is_ok;
}
```

When writing a new config feature, preserve EEPROM layout compatibility. Add fields carefully, keep defaults explicit, and update the relevant EEPROM address/revision policy only when required.

---

## Rule 6 - Keep target hardware configuration in `targets/`

Board-specific pin and peripheral mappings live under `targets/`, for example `targets/raspberrypi_pico_w_config.h`. Use macros for fixed hardware resources:

```c
#define MOTOR_UART uart1
#define MOTOR_UART_TX 4
#define MOTOR_UART_RX 5
#define MOTOR_PIO pio0
```

Project modules should consume these macros via `configuration.h` rather than hard-coding pins or peripheral instances locally.

When adding RP2040/RP2350 board support, prefer a new target config header and CMake board selection updates over conditional clutter in feature modules.

---

## Rule 7 - Use Pico SDK hardware APIs directly and visibly

Hardware control should be explicit and close to the module that owns it. Prefer Pico SDK calls already used in the codebase:

- GPIO: `gpio_init`, `gpio_set_dir`, `gpio_put`, `gpio_set_function`
- UART: `uart_init`, `uart_write_blocking`, `uart_read_blocking`, `uart_is_readable_within_us`
- PIO: `pio_claim_unused_sm`, `pio_add_program`, `pio_sm_set_enabled`, `pio_sm_put_blocking`
- Clocks/time: `clock_get_hz`, `time_us_32`, `busy_wait_us`, `busy_wait_ms`
- CYW43/lwIP: protect lwIP calls with `cyw43_arch_lwip_begin()` / `cyw43_arch_lwip_end()` where the surrounding code does

Check return values for resource-claiming APIs and report failures:

```c
int sm = pio_claim_unused_sm(pio, true);
if (sm < 0) {
    printf("Unable to claim state machine, err: %d\n", sm);
    return false;
}
```

When reviewing: flag hard-coded pin numbers, hidden hardware side effects, unchecked PIO/UART resource setup, and lwIP calls that violate the local CYW43 locking pattern.

---

## Rule 8 - Treat FreeRTOS tasks, queues, and semaphores explicitly

Task code favors visible loops, explicit queue commands, and simple state machines. Task functions use `void *p` or `void *args`, cast near the top or at use sites, then loop.

Creation commonly uses `xTaskCreate(...)` with literal task names, stack sizes such as `configMINIMAL_STACK_SIZE` or module-specific values, and explicit priorities. New code should check queue/semaphore allocation and task creation when practical.

**Right:**
```c
wireless_ctrl_queue = xQueueCreate(5, sizeof(wireless_ctrl_t));
if (!wireless_ctrl_queue) {
    printf("Unable to create wireless control queue\n");
    return;
}
```

Use `vTaskDelayUntil(...)` for periodic loops that should keep cadence, and `vTaskDelay(pdMS_TO_TICKS(...))` for simple sleeps.

When reviewing: flag hidden background behavior, unbounded busy loops after the scheduler starts, queue item-size mismatches, blocking calls in high-priority paths without a clear reason, and missing synchronization around shared display buffers or shared hardware.

---

## Rule 9 - Build u8g2 and MUI views directly

Display code is intentionally imperative: clear the buffer, set font, draw strings/lines/shapes, send the buffer, and delay at the required cadence. Keep layout calculations local and readable.

Use the display access helpers where shared buffer access matters:

- `get_display_handler()`
- `acquire_display_buffer_access()`
- `release_display_buffer_access()`

**Right:**
```c
u8g2_t *display_handler = get_display_handler();

u8g2_ClearBuffer(display_handler);
u8g2_SetFont(display_handler, u8g2_font_helvB08_tr);
u8g2_DrawStr(display_handler, 5, 10, title_string);
u8g2_SendBuffer(display_handler);
```

When reviewing: flag display updates from multiple tasks without a clear locking or ownership story, oversized UI abstractions, and text drawing that can exceed fixed display buffers without bounds checks.

---

## Rule 10 - Keep REST handlers small and static-buffer based

REST endpoint handlers usually accept `struct fs_file *file, int num_params, char *params[], char *values[]`, update live settings from compact parameter names, and return static response buffers with `FS_FILE_FLAGS_HEADER_INCLUDED`.

Prefer bounded formatting:

```c
static char json_buffer[256];

snprintf(json_buffer,
         sizeof(json_buffer),
         "%s{\"enabled\":%s}",
         http_json_header,
         boolean_to_string(enabled));
```

Do not expose secrets such as WiFi passwords in responses. Preserve the existing compact parameter mapping style (`w0`, `m1`, `c13`, `ee`) unless the user asks for an API redesign.

When reviewing: flag response buffer overflow risk, returning stack memory through `file->data`, unsanitized secret output, and failing to set `file->len`, `file->index`, and `file->flags`.

---

## Rule 11 - Use memory allocation conservatively

This firmware uses standard C allocation (`malloc`, `free`) in project code and C++ objects in selected C++ modules. Keep allocations bounded, check for `NULL`, and free on every failure path.

Prefer static buffers for small HTTP responses, display strings, and fixed hardware data where the existing module does. Avoid long-lived heap allocations in fast control loops unless there is a clear ownership model.

**Right:**
```c
uint8_t *buf = malloc(write_size);
if (!buf) {
    printf("Unable to allocate buffer with size: %d\n", write_size);
    return false;
}

...

free(buf);
```

When reviewing: flag unchecked allocation, allocator mismatches, stack buffers that are too large for FreeRTOS task stacks, and heap use inside timing-sensitive loops.

---

## Rule 12 - Comments explain hardware intent, units, and compatibility

Comments should explain hardware intent, timing assumptions, EEPROM compatibility, units, REST parameter mappings, and concurrency assumptions. Keep comments short and practical. Preserve useful `TODO`, `FIXME`, and `NOTE` markers when they describe real incomplete work or constraints.

**Wrong:**
```c
// Set value to true
enabled = true;
```

**Right:**
```c
// At 250k baud rate the transmit time is about 320us. Need to wait long enough to not read the echo message back.
busy_wait_us(20);
```

When reviewing: flag misleading comments and empty narration, but do not demand Doxygen for every internal helper. Public headers may use brief comments where the surrounding header already does.

---

## Rule 13 - Keep C++ as a bridge, not a second architecture

Use C++ where it is already needed for helper classes or modules such as `FloatRingBuffer` and selected mode implementations. Keep public headers C-callable with `extern "C"` guards when C modules need to call them.

**Right:**
```c
#ifdef __cplusplus
extern "C" {
#endif

bool charge_mode_config_init(void);

#ifdef __cplusplus
}
#endif
```

Avoid introducing classes, templates, exceptions, RTTI-heavy patterns, STL containers, or broad C++ architecture into firmware modules unless the surrounding module already depends on them and the user explicitly wants that direction.

When reviewing: flag C++ abstractions that make firmware control flow harder to inspect or call from C.

---

## Rule 14 - Keep CMake changes local and Pico SDK-native

Top-level CMake uses Pico SDK initialization, FreeRTOS import, u8g2, Trinamic interface sources, and `src/CMakeLists.txt` for generated/support targets. Preserve this structure.

Prefer:

- `target_sources(...)` for explicit generated or library-owned source additions
- `target_link_libraries(...)` for Pico SDK hardware libraries such as `hardware_pio`, `hardware_spi`, `hardware_i2c`, `hardware_pwm`
- target config include paths through `PICO_BOARD_HEADER_DIRS` and `targets/`

Do not vendor new dependencies into `library/` or alter SDK import behavior unless the user asks for that scope.

---

## Review Workflow

When asked to review C/C++ code:

1. Read the file or diff in full, plus the nearby header/source pair when relevant.
2. Prioritize behavioral risks first: hardware resource claims, task/queue/semaphore safety, display buffer concurrency, EEPROM compatibility, REST buffer lifetime/size, PIO/UART timing, CYW43/lwIP locking, and memory ownership.
3. Then list style violations against these rules with file and line references.
4. Suggest the smallest correction that matches the surrounding module.
5. If no issues are found, say so and mention any residual test or hardware-verification gap.

When writing C/C++ code:

Apply these rules before editing. Match the current file's local conventions, keep edits scoped, and prefer existing helpers and patterns over new abstractions.
