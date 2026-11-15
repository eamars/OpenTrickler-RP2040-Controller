/*
 * The TMC2209 stepper driver: see tmc2209.h.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tmc2209.h"

// GCONF
#define TMC2209_GCONF_I_SCALE_ANALOG (1u << 0)
#define TMC2209_GCONF_EN_SPREADCYCLE (1u << 2)
#define TMC2209_GCONF_PDN_DISABLE (1u << 6)
#define TMC2209_GCONF_MSTEP_REG_SELECT (1u << 7)
#define TMC2209_GCONF_MULTISTEP_FILT (1u << 8)

// GSTAT: each flag is cleared by writing it as 1
#define TMC2209_GSTAT_ALL 0x07u

// SLAVECONF
#define TMC2209_SLAVECONF_SENDDELAY_SHIFT 8

// IHOLD_IRUN
#define TMC2209_IHOLD_SHIFT 0
#define TMC2209_IRUN_SHIFT 8
#define TMC2209_IHOLDDELAY_SHIFT 16

// CHOPCONF
#define TMC2209_CHOPCONF_TOFF_SHIFT 0
#define TMC2209_CHOPCONF_HSTRT_SHIFT 4
#define TMC2209_CHOPCONF_HEND_SHIFT 7
#define TMC2209_CHOPCONF_TBL_SHIFT 15
#define TMC2209_CHOPCONF_VSENSE (1u << 17)
#define TMC2209_CHOPCONF_MRES_SHIFT 24
#define TMC2209_CHOPCONF_INTPOL (1u << 28)

// PWMCONF
#define TMC2209_PWMCONF_OFS_SHIFT 0
#define TMC2209_PWMCONF_GRAD_SHIFT 8
#define TMC2209_PWMCONF_FREQ_SHIFT 16
#define TMC2209_PWMCONF_AUTOSCALE (1u << 18)
#define TMC2209_PWMCONF_AUTOGRAD (1u << 19)
#define TMC2209_PWMCONF_REG_SHIFT 24
#define TMC2209_PWMCONF_LIM_SHIFT 28

/*
 * The values the units have run with since the Trinamic library set them (its defaults, and stealthChop): kept,
 * so that this driver changes the current's reference and nothing else about how a motor turns.
 */
#define TMC2209_SENDDELAY 2         // 3 x 8 bit times before an answer: two drivers share the wire (Klipper)
#define TMC2209_TOFF 3
#define TMC2209_HSTRT 0
#define TMC2209_HEND 2
#define TMC2209_TBL 1
#define TMC2209_IHOLDDELAY 10
#define TMC2209_TPOWERDOWN 20
#define TMC2209_PWM_OFS 36
#define TMC2209_PWM_GRAD 14
#define TMC2209_PWM_FREQ 1
#define TMC2209_PWM_REG 8
#define TMC2209_PWM_LIM 12
#define TMC2209_HOLD_CURRENT_PCT 50

// The current's maths (Klipper, tmc2130.py): full-scale sense voltage by the vsense bit, and the chip's own 20 mOhm
#define TMC2209_VFS_LOW_SENSE 0.32f
#define TMC2209_VFS_HIGH_SENSE 0.18f
#define TMC2209_R_INTERNAL_OHM 0.020f
#define TMC2209_CURRENT_STEPS 32
#define TMC2209_SQRT2 1.41421356f       // RMS to peak


typedef struct {
    const char *name;
    uint8_t reg;
} tmc2209_register_t;

typedef struct {
    uint8_t reg;
    const char *name;
    uint32_t mask;
} tmc2209_field_t;


// The registers that can be read back, in the order they are shown
static const tmc2209_register_t tmc2209_read_registers[] = {
    {"GCONF", TMC2209_REG_GCONF},
    {"GSTAT", TMC2209_REG_GSTAT},
    {"IFCNT", TMC2209_REG_IFCNT},
    {"IOIN", TMC2209_REG_IOIN},
    {"TSTEP", TMC2209_REG_TSTEP},
    {"SG_RESULT", TMC2209_REG_SG_RESULT},
    {"MSCNT", TMC2209_REG_MSCNT},
    {"CHOPCONF", TMC2209_REG_CHOPCONF},
    {"DRV_STATUS", TMC2209_REG_DRV_STATUS},
    {"PWMCONF", TMC2209_REG_PWMCONF},
    {"PWM_SCALE", TMC2209_REG_PWM_SCALE},
    {"PWM_AUTO", TMC2209_REG_PWM_AUTO},
};

// The registers the set-up writes, by tmc2209_written_t
static const tmc2209_register_t tmc2209_written_registers[TMC2209_WRITTEN_COUNT] = {
    [TMC2209_WRITTEN_SLAVECONF] = {"SLAVECONF", TMC2209_REG_SLAVECONF},
    [TMC2209_WRITTEN_GCONF] = {"GCONF", TMC2209_REG_GCONF},
    [TMC2209_WRITTEN_CHOPCONF] = {"CHOPCONF", TMC2209_REG_CHOPCONF},
    [TMC2209_WRITTEN_IHOLD_IRUN] = {"IHOLD_IRUN", TMC2209_REG_IHOLD_IRUN},
    [TMC2209_WRITTEN_TPOWERDOWN] = {"TPOWERDOWN", TMC2209_REG_TPOWERDOWN},
    [TMC2209_WRITTEN_PWMCONF] = {"PWMCONF", TMC2209_REG_PWMCONF},
    [TMC2209_WRITTEN_TPWMTHRS] = {"TPWMTHRS", TMC2209_REG_TPWMTHRS},
    [TMC2209_WRITTEN_TCOOLTHRS] = {"TCOOLTHRS", TMC2209_REG_TCOOLTHRS},
};

// Fields, from Klipper's tables
static const tmc2209_field_t tmc2209_fields[] = {
    {TMC2209_REG_GCONF, "i_scale_analog", 0x01u << 0},
    {TMC2209_REG_GCONF, "internal_rsense", 0x01u << 1},
    {TMC2209_REG_GCONF, "en_spreadcycle", 0x01u << 2},
    {TMC2209_REG_GCONF, "shaft", 0x01u << 3},
    {TMC2209_REG_GCONF, "index_otpw", 0x01u << 4},
    {TMC2209_REG_GCONF, "index_step", 0x01u << 5},
    {TMC2209_REG_GCONF, "pdn_disable", 0x01u << 6},
    {TMC2209_REG_GCONF, "mstep_reg_select", 0x01u << 7},
    {TMC2209_REG_GCONF, "multistep_filt", 0x01u << 8},
    {TMC2209_REG_GCONF, "test_mode", 0x01u << 9},

    {TMC2209_REG_GSTAT, "reset", 0x01u << 0},
    {TMC2209_REG_GSTAT, "drv_err", 0x01u << 1},
    {TMC2209_REG_GSTAT, "uv_cp", 0x01u << 2},

    {TMC2209_REG_IFCNT, "ifcnt", 0xFFu},

    {TMC2209_REG_SLAVECONF, "senddelay", 0x0Fu << 8},

    {TMC2209_REG_IOIN, "enn", 0x01u << 0},
    {TMC2209_REG_IOIN, "ms1", 0x01u << 2},
    {TMC2209_REG_IOIN, "ms2", 0x01u << 3},
    {TMC2209_REG_IOIN, "diag", 0x01u << 4},
    {TMC2209_REG_IOIN, "pdn_uart", 0x01u << 6},
    {TMC2209_REG_IOIN, "step", 0x01u << 7},
    {TMC2209_REG_IOIN, "spread_en", 0x01u << 8},
    {TMC2209_REG_IOIN, "dir", 0x01u << 9},
    {TMC2209_REG_IOIN, "version", 0xFFu << 24},

    {TMC2209_REG_IHOLD_IRUN, "ihold", 0x1Fu << 0},
    {TMC2209_REG_IHOLD_IRUN, "irun", 0x1Fu << 8},
    {TMC2209_REG_IHOLD_IRUN, "iholddelay", 0x0Fu << 16},

    {TMC2209_REG_TPOWERDOWN, "tpowerdown", 0xFFu},
    {TMC2209_REG_TSTEP, "tstep", 0xFFFFFu},
    {TMC2209_REG_TPWMTHRS, "tpwmthrs", 0xFFFFFu},
    {TMC2209_REG_TCOOLTHRS, "tcoolthrs", 0xFFFFFu},
    {TMC2209_REG_SG_RESULT, "sg_result", 0x3FFu},
    {TMC2209_REG_MSCNT, "mscnt", 0x3FFu},

    {TMC2209_REG_CHOPCONF, "toff", 0x0Fu << 0},
    {TMC2209_REG_CHOPCONF, "hstrt", 0x07u << 4},
    {TMC2209_REG_CHOPCONF, "hend", 0x0Fu << 7},
    {TMC2209_REG_CHOPCONF, "tbl", 0x03u << 15},
    {TMC2209_REG_CHOPCONF, "vsense", 0x01u << 17},
    {TMC2209_REG_CHOPCONF, "mres", 0x0Fu << 24},
    {TMC2209_REG_CHOPCONF, "intpol", 0x01u << 28},
    {TMC2209_REG_CHOPCONF, "dedge", 0x01u << 29},
    {TMC2209_REG_CHOPCONF, "diss2g", 0x01u << 30},
    {TMC2209_REG_CHOPCONF, "diss2vs", 0x01u << 31},

    {TMC2209_REG_DRV_STATUS, "otpw", 0x01u << 0},
    {TMC2209_REG_DRV_STATUS, "ot", 0x01u << 1},
    {TMC2209_REG_DRV_STATUS, "s2ga", 0x01u << 2},
    {TMC2209_REG_DRV_STATUS, "s2gb", 0x01u << 3},
    {TMC2209_REG_DRV_STATUS, "s2vsa", 0x01u << 4},
    {TMC2209_REG_DRV_STATUS, "s2vsb", 0x01u << 5},
    {TMC2209_REG_DRV_STATUS, "ola", 0x01u << 6},
    {TMC2209_REG_DRV_STATUS, "olb", 0x01u << 7},
    {TMC2209_REG_DRV_STATUS, "t120", 0x01u << 8},
    {TMC2209_REG_DRV_STATUS, "t143", 0x01u << 9},
    {TMC2209_REG_DRV_STATUS, "t150", 0x01u << 10},
    {TMC2209_REG_DRV_STATUS, "t157", 0x01u << 11},
    {TMC2209_REG_DRV_STATUS, "cs_actual", 0x1Fu << 16},
    {TMC2209_REG_DRV_STATUS, "stealth", 0x01u << 30},
    {TMC2209_REG_DRV_STATUS, "stst", 0x01u << 31},

    {TMC2209_REG_PWMCONF, "pwm_ofs", 0xFFu << 0},
    {TMC2209_REG_PWMCONF, "pwm_grad", 0xFFu << 8},
    {TMC2209_REG_PWMCONF, "pwm_freq", 0x03u << 16},
    {TMC2209_REG_PWMCONF, "pwm_autoscale", 0x01u << 18},
    {TMC2209_REG_PWMCONF, "pwm_autograd", 0x01u << 19},
    {TMC2209_REG_PWMCONF, "freewheel", 0x03u << 20},
    {TMC2209_REG_PWMCONF, "pwm_reg", 0x0Fu << 24},
    {TMC2209_REG_PWMCONF, "pwm_lim", 0x0Fu << 28},

    {TMC2209_REG_PWM_SCALE, "pwm_scale_sum", 0xFFu << 0},
    {TMC2209_REG_PWM_SCALE, "pwm_scale_auto", 0x1FFu << 16},

    {TMC2209_REG_PWM_AUTO, "pwm_ofs_auto", 0xFFu << 0},
    {TMC2209_REG_PWM_AUTO, "pwm_grad_auto", 0xFFu << 16},
};

#define TMC2209_FIELD_COUNT (sizeof(tmc2209_fields) / sizeof(tmc2209_fields[0]))
#define TMC2209_READ_REGISTER_COUNT (sizeof(tmc2209_read_registers) / sizeof(tmc2209_read_registers[0]))

// One line of tmc2209_dump. Not on the stack: the tasks that ask have small ones. The caller serialises the dumps
static char tmc2209_dump_line[224];


// The current's step (IRUN or IHOLD, 0 to 31) nearest a current, for one setting of the vsense bit
static uint8_t tmc2209_current_to_bits(float current_a, float r_sense_ohm, bool vsense) {
    float v_full_scale = vsense ? TMC2209_VFS_HIGH_SENSE : TMC2209_VFS_LOW_SENSE;
    int bits = (int) (TMC2209_CURRENT_STEPS * (r_sense_ohm + TMC2209_R_INTERNAL_OHM) * current_a * TMC2209_SQRT2 /
                      v_full_scale + 0.5f) - 1;

    if (bits < 0) {
        bits = 0;
    }
    if (bits > TMC2209_CURRENT_STEPS - 1) {
        bits = TMC2209_CURRENT_STEPS - 1;
    }
    return (uint8_t) bits;
}


static float tmc2209_bits_to_current(uint8_t bits, float r_sense_ohm, bool vsense) {
    float v_full_scale = vsense ? TMC2209_VFS_HIGH_SENSE : TMC2209_VFS_LOW_SENSE;

    return (bits + 1) * v_full_scale / (TMC2209_CURRENT_STEPS * (r_sense_ohm + TMC2209_R_INTERNAL_OHM) * TMC2209_SQRT2);
}


/*
 * The run and hold steps and the vsense bit for a run current. The sensitive range (vsense set) first; the other
 * only if the current is beyond it and the other comes closer (Klipper's _calc_current).
 */
static void tmc2209_calc_current(float run_current_a, float r_sense_ohm, bool *vsense, uint8_t *irun, uint8_t *ihold) {
    *vsense = true;
    *irun = tmc2209_current_to_bits(run_current_a, r_sense_ohm, true);

    if (*irun == TMC2209_CURRENT_STEPS - 1) {
        float current_a = tmc2209_bits_to_current(*irun, r_sense_ohm, true);
        if (current_a < run_current_a) {
            uint8_t irun_low_sense = tmc2209_current_to_bits(run_current_a, r_sense_ohm, false);
            float current_low_sense_a = tmc2209_bits_to_current(irun_low_sense, r_sense_ohm, false);
            if (fabsf(run_current_a - current_low_sense_a) < fabsf(run_current_a - current_a)) {
                *vsense = false;
                *irun = irun_low_sense;
            }
        }
    }

    *ihold = tmc2209_current_to_bits(run_current_a * TMC2209_HOLD_CURRENT_PCT / 100.0f, r_sense_ohm, *vsense);
}


// MRES: 0 for 256 microsteps, 8 for full steps
static uint8_t tmc2209_microsteps_to_mres(uint16_t microsteps) {
    uint8_t mres = 8;

    while (microsteps > 1) {
        microsteps >>= 1;
        mres -= 1;
    }
    return mres;
}


static void tmc2209_build_registers(tmc2209_t *driver) {
    float r_sense_ohm = driver->r_sense_mohm / 1000.0f;
    bool vsense;
    uint8_t irun;
    uint8_t ihold;

    tmc2209_calc_current(driver->current_ma / 1000.0f, r_sense_ohm, &vsense, &irun, &ihold);

    driver->written[TMC2209_WRITTEN_SLAVECONF] = (uint32_t) TMC2209_SENDDELAY << TMC2209_SLAVECONF_SENDDELAY_SHIFT;

    // The UART and not the PDN pin, microsteps from the register and not the MS pins (they are the address),
    // stealthChop. I_scale_analog stays clear: the trimmer is not the current's reference
    driver->written[TMC2209_WRITTEN_GCONF] = TMC2209_GCONF_PDN_DISABLE | TMC2209_GCONF_MSTEP_REG_SELECT |
                                             TMC2209_GCONF_MULTISTEP_FILT;

    driver->written[TMC2209_WRITTEN_CHOPCONF] = ((uint32_t) TMC2209_TOFF << TMC2209_CHOPCONF_TOFF_SHIFT) |
                                                ((uint32_t) TMC2209_HSTRT << TMC2209_CHOPCONF_HSTRT_SHIFT) |
                                                ((uint32_t) TMC2209_HEND << TMC2209_CHOPCONF_HEND_SHIFT) |
                                                ((uint32_t) TMC2209_TBL << TMC2209_CHOPCONF_TBL_SHIFT) |
                                                (vsense ? TMC2209_CHOPCONF_VSENSE : 0) |
                                                ((uint32_t) tmc2209_microsteps_to_mres(driver->microsteps) << TMC2209_CHOPCONF_MRES_SHIFT) |
                                                TMC2209_CHOPCONF_INTPOL;

    driver->written[TMC2209_WRITTEN_IHOLD_IRUN] = ((uint32_t) ihold << TMC2209_IHOLD_SHIFT) |
                                                  ((uint32_t) irun << TMC2209_IRUN_SHIFT) |
                                                  ((uint32_t) TMC2209_IHOLDDELAY << TMC2209_IHOLDDELAY_SHIFT);

    driver->written[TMC2209_WRITTEN_TPOWERDOWN] = TMC2209_TPOWERDOWN;

    driver->written[TMC2209_WRITTEN_PWMCONF] = ((uint32_t) TMC2209_PWM_OFS << TMC2209_PWMCONF_OFS_SHIFT) |
                                               ((uint32_t) TMC2209_PWM_GRAD << TMC2209_PWMCONF_GRAD_SHIFT) |
                                               ((uint32_t) TMC2209_PWM_FREQ << TMC2209_PWMCONF_FREQ_SHIFT) |
                                               TMC2209_PWMCONF_AUTOSCALE | TMC2209_PWMCONF_AUTOGRAD |
                                               ((uint32_t) TMC2209_PWM_REG << TMC2209_PWMCONF_REG_SHIFT) |
                                               ((uint32_t) TMC2209_PWM_LIM << TMC2209_PWMCONF_LIM_SHIFT);

    // stealthChop at every speed, coolStep and stall detection off
    driver->written[TMC2209_WRITTEN_TPWMTHRS] = 0;
    driver->written[TMC2209_WRITTEN_TCOOLTHRS] = 0;
}


void tmc2209_init(tmc2209_t *driver, tmc_uart_exchange_fn exchange, uint8_t addr) {
    memset(driver, 0x0, sizeof(tmc2209_t));
    tmc_uart_init(&driver->bus, exchange, addr);
}


bool tmc2209_microsteps_is_valid(uint16_t microsteps) {
    return microsteps >= 1 && microsteps <= 256 && (microsteps & (microsteps - 1)) == 0;
}


bool tmc2209_setup(tmc2209_t *driver, uint16_t current_ma, uint16_t r_sense_mohm, uint16_t microsteps) {
    uint32_t gstat;

    driver->ready = false;
    driver->current_ma = current_ma;
    driver->r_sense_mohm = r_sense_mohm;
    driver->microsteps = microsteps;
    tmc2209_build_registers(driver);

    // Is the driver there? Its flags from before (a reset, an error) are cleared: each by writing it as 1
    if (!tmc_uart_read(&driver->bus, TMC2209_REG_GSTAT, &gstat)) {
        return false;
    }
    if (!tmc_uart_write(&driver->bus, TMC2209_REG_GSTAT, TMC2209_GSTAT_ALL)) {
        return false;
    }

    for (int idx = 0; idx < TMC2209_WRITTEN_COUNT; idx += 1) {
        if (!tmc_uart_write(&driver->bus, tmc2209_written_registers[idx].reg, driver->written[idx])) {
            return false;
        }
    }

    driver->ready = true;
    return true;
}


bool tmc2209_read_status(tmc2209_t *driver, uint32_t *drv_status, uint32_t *gstat) {
    return tmc_uart_read(&driver->bus, TMC2209_REG_DRV_STATUS, drv_status) &&
           tmc_uart_read(&driver->bus, TMC2209_REG_GSTAT, gstat);
}


bool tmc2209_clear_gstat(tmc2209_t *driver, uint32_t flags) {
    return tmc_uart_write(&driver->bus, TMC2209_REG_GSTAT, flags);
}


uint16_t tmc2209_get_run_current_ma(const tmc2209_t *driver) {
    uint32_t ihold_irun = driver->written[TMC2209_WRITTEN_IHOLD_IRUN];
    uint8_t irun = (ihold_irun >> TMC2209_IRUN_SHIFT) & 0x1F;
    bool vsense = (driver->written[TMC2209_WRITTEN_CHOPCONF] & TMC2209_CHOPCONF_VSENSE) != 0;

    return (uint16_t) lroundf(tmc2209_bits_to_current(irun, driver->r_sense_mohm / 1000.0f, vsense) * 1000.0f);
}


// One register as a line: `<prefix> <name> <hex>` and each of its fields that is not zero
static void tmc2209_format_register(const char *prefix, const tmc2209_register_t *reg, uint32_t value) {
    size_t len = snprintf(tmc2209_dump_line, sizeof(tmc2209_dump_line), "%s %s %08lx", prefix, reg->name,
                          (unsigned long) value);

    for (size_t idx = 0; idx < TMC2209_FIELD_COUNT && len < sizeof(tmc2209_dump_line); idx += 1) {
        const tmc2209_field_t *field = &tmc2209_fields[idx];
        if (field->reg != reg->reg) {
            continue;
        }

        uint32_t field_value = (value & field->mask) >> __builtin_ctz(field->mask);
        if (field_value != 0) {
            len += snprintf(tmc2209_dump_line + len, sizeof(tmc2209_dump_line) - len, " %s=%lu", field->name,
                            (unsigned long) field_value);
        }
    }
}


bool tmc2209_dump(tmc2209_t *driver, tmc2209_line_fn emit, void *ctx) {
    for (size_t idx = 0; idx < TMC2209_READ_REGISTER_COUNT; idx += 1) {
        uint32_t value;
        if (!tmc_uart_read(&driver->bus, tmc2209_read_registers[idx].reg, &value)) {
            return false;
        }
        tmc2209_format_register("r", &tmc2209_read_registers[idx], value);
        emit(tmc2209_dump_line, ctx);
    }

    // What cannot be read back, as it was last built
    for (int idx = 0; idx < TMC2209_WRITTEN_COUNT; idx += 1) {
        uint8_t reg = tmc2209_written_registers[idx].reg;
        if (reg == TMC2209_REG_GCONF || reg == TMC2209_REG_CHOPCONF || reg == TMC2209_REG_PWMCONF) {
            continue;
        }
        tmc2209_format_register("w", &tmc2209_written_registers[idx], driver->written[idx]);
        emit(tmc2209_dump_line, ctx);
    }

    return true;
}
