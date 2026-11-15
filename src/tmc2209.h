#ifndef TMC2209_H_
#define TMC2209_H_

/*
 * The TMC2209 stepper driver's set-up and status over its serial line (ADR-012, step 1). It knows the chip's
 * registers and nothing else: no pins, no RTOS. The registers and fields are Klipper's tables
 * (klippy/extras/tmc2208.py and tmc2209.py, Kevin O'Connor and Stephan Oelze, GPLv3), and so are the rules:
 *   - each register is built from zero and written whole, never read from the chip and changed. So GCONF goes out
 *     with I_scale_analog clear: the current comes from the driver's own reference, and the module's trimmer does
 *     nothing;
 *   - the current is RMS, worked out from the sense resistor.
 * A second chip is a second file of this shape on the same tmc_uart.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tmc_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registers
#define TMC2209_REG_GCONF 0x00
#define TMC2209_REG_GSTAT 0x01
#define TMC2209_REG_IFCNT 0x02
#define TMC2209_REG_SLAVECONF 0x03
#define TMC2209_REG_IOIN 0x06
#define TMC2209_REG_IHOLD_IRUN 0x10
#define TMC2209_REG_TPOWERDOWN 0x11
#define TMC2209_REG_TSTEP 0x12
#define TMC2209_REG_TPWMTHRS 0x13
#define TMC2209_REG_TCOOLTHRS 0x14
#define TMC2209_REG_SG_RESULT 0x41
#define TMC2209_REG_MSCNT 0x6A
#define TMC2209_REG_CHOPCONF 0x6C
#define TMC2209_REG_DRV_STATUS 0x6F
#define TMC2209_REG_PWMCONF 0x70
#define TMC2209_REG_PWM_SCALE 0x71
#define TMC2209_REG_PWM_AUTO 0x72

// DRV_STATUS: what the driver reports of itself
#define TMC2209_DRV_STATUS_OTPW (1u << 0)       // Warm: the warning before it switches off
#define TMC2209_DRV_STATUS_OT (1u << 1)         // Too hot: switched off
#define TMC2209_DRV_STATUS_S2GA (1u << 2)       // A coil shorted to ground
#define TMC2209_DRV_STATUS_S2GB (1u << 3)
#define TMC2209_DRV_STATUS_S2VSA (1u << 4)      // A coil shorted to the supply
#define TMC2209_DRV_STATUS_S2VSB (1u << 5)
#define TMC2209_DRV_STATUS_T120 (1u << 8)       // Temperature thresholds passed
#define TMC2209_DRV_STATUS_T143 (1u << 9)
#define TMC2209_DRV_STATUS_T150 (1u << 10)
#define TMC2209_DRV_STATUS_T157 (1u << 11)
#define TMC2209_DRV_STATUS_SHORT_MASK (TMC2209_DRV_STATUS_S2GA | TMC2209_DRV_STATUS_S2GB | \
                                       TMC2209_DRV_STATUS_S2VSA | TMC2209_DRV_STATUS_S2VSB)
#define TMC2209_DRV_STATUS_WARN_MASK (TMC2209_DRV_STATUS_OTPW | TMC2209_DRV_STATUS_T120 | TMC2209_DRV_STATUS_T143 | \
                                      TMC2209_DRV_STATUS_T150 | TMC2209_DRV_STATUS_T157)

// GSTAT: what has happened since the flags were last cleared
#define TMC2209_GSTAT_RESET (1u << 0)           // The driver has started anew: its registers are its own defaults
#define TMC2209_GSTAT_DRV_ERR (1u << 1)         // It switched off for heat or a short: DRV_STATUS says which
#define TMC2209_GSTAT_UV_CP (1u << 2)           // Its charge pump's voltage fell: the supply is too low

// The registers the set-up writes, in the order it writes them
typedef enum {
    TMC2209_WRITTEN_SLAVECONF = 0,
    TMC2209_WRITTEN_GCONF,
    TMC2209_WRITTEN_CHOPCONF,
    TMC2209_WRITTEN_IHOLD_IRUN,
    TMC2209_WRITTEN_TPOWERDOWN,
    TMC2209_WRITTEN_PWMCONF,
    TMC2209_WRITTEN_TPWMTHRS,
    TMC2209_WRITTEN_TCOOLTHRS,
    TMC2209_WRITTEN_COUNT,
} tmc2209_written_t;

typedef struct {
    tmc_uart_t bus;

    // What the registers were built from
    uint16_t current_ma;        // Run current, mA RMS
    uint16_t r_sense_mohm;      // Sense resistor, milliohm (110 on the usual modules)
    uint16_t microsteps;        // 1 to 256, a power of two

    // The registers as last built, whether or not the driver took them
    uint32_t written[TMC2209_WRITTEN_COUNT];

    // True once the driver has answered and was seen to take every register
    bool ready;
} tmc2209_t;

// One line of tmc2209_dump, without a line ending
typedef void (*tmc2209_line_fn)(const char *line, void *ctx);

void tmc2209_init(tmc2209_t *driver, tmc_uart_exchange_fn exchange, uint8_t addr);

/*
 * Build every register from these settings and write them all. False, and `ready` false, if the driver does not
 * answer or a write is not seen to arrive.
 */
bool tmc2209_setup(tmc2209_t *driver, uint16_t current_ma, uint16_t r_sense_mohm, uint16_t microsteps);

bool tmc2209_microsteps_is_valid(uint16_t microsteps);

// DRV_STATUS and GSTAT as the driver holds them now. False if it does not answer
bool tmc2209_read_status(tmc2209_t *driver, uint32_t *drv_status, uint32_t *gstat);

// Clear flags of GSTAT: each is cleared by writing it as 1
bool tmc2209_clear_gstat(tmc2209_t *driver, uint32_t flags);

// The run current the driver's 32 steps give for the settings last built, mA RMS
uint16_t tmc2209_get_run_current_ma(const tmc2209_t *driver);

/*
 * The registers by field, one line a register: `r <name> <hex> field=value ...` as read from the chip now, then
 * `w <name> <hex> field=value ...` as last written, for those that cannot be read. Fields at zero are left out.
 * False if a read failed; the lines before it were still sent.
 */
bool tmc2209_dump(tmc2209_t *driver, tmc2209_line_fn emit, void *ctx);

#ifdef __cplusplus
}
#endif

#endif  // TMC2209_H_
