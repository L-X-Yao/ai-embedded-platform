// SPDX-License-Identifier: MIT
/*
 * hw_model_ccs811.h — CCS811 hardware register model from datasheet
 *
 * Models CCS811 datasheet (ams AG, v1-00) register behavior. Registers are
 * mailboxes, not memory: p13, "the address does not increment". So all traffic
 * goes through the model's own HAL callbacks, ccs811_model_write_reg,
 * ccs811_model_write_raw and ccs811_model_read_regs, which:
 *
 * - know the application map of p15 and the bootloader map of p24, and raise
 *   WRITE_REG_INVALID or READ_REG_INVALID (p22) for anything else
 * - stream ALG_RESULT_DATA 0x02 as its 8 bytes of p18, apart from RAW_DATA 0x03
 * - take ENV_DATA 0x05 only as one 4-byte write
 * - start the application only on the single byte 0xF4, with no data
 * - count a read longer than the mailbox in over_reads (p15 note 1)
 *
 * Time passes in ccs811_model_advance_ms() and in ccs811_model_delay_ms. A
 * sample comes at the measurement interval of MEAS_MODE (p17) and sets
 * DATA_READY, which a read of ALG_RESULT_DATA clears (p16).
 */

#ifndef HW_MODEL_CCS811_H
#define HW_MODEL_CCS811_H

#include "hw_model.h"

#define CCS811_REG_STATUS      0x00
#define CCS811_REG_MEAS_MODE   0x01
#define CCS811_REG_ALG_RESULT  0x02
#define CCS811_REG_RAW_DATA    0x03
#define CCS811_REG_ENV_DATA    0x05
#define CCS811_REG_NTC         0x06
#define CCS811_REG_THRESHOLDS  0x10
#define CCS811_REG_BASELINE    0x11
#define CCS811_REG_HW_ID       0x20
#define CCS811_REG_HW_VERSION  0x21
#define CCS811_REG_FW_BOOT_VER 0x23
#define CCS811_REG_FW_APP_VER  0x24
#define CCS811_REG_ERROR_ID    0xE0
#define CCS811_REG_APP_ERASE   0xF1
#define CCS811_REG_APP_START   0xF4
#define CCS811_REG_SW_RESET    0xFF

/* ERROR_ID codes, p22 */
#define CCS811_MODEL_WRITE_REG_INVALID 0x01u
#define CCS811_MODEL_READ_REG_INVALID  0x02u
#define CCS811_MODEL_MEASMODE_INVALID  0x04u

/* Measurement interval of drive modes 0 to 4, p17. Mode 0 takes no samples. */
static const uint32_t ccs811_model_interval_ms[5] = { 0, 1000, 10000, 60000, 250 };

typedef struct {
    hw_reg_model_t hw;
    uint16_t       true_eco2;
    uint16_t       true_tvoc;
    uint16_t       true_raw;
    bool           app_valid;
    bool           app_mode;
    bool           data_ready;
    int            measure_count;
    uint32_t       until_sample_ms;  /* time left to the next sample */
    uint8_t        alg_result[4];    /* eCO2 high, low, TVOC high, low */
    uint16_t       raw_data;         /* RAW_DATA mailbox */
    uint8_t        env_data[4];      /* ENV_DATA mailbox */
    int            env_write_count;  /* whole 4-byte writes to ENV_DATA */
    uint8_t        error_id;         /* ERROR_ID mailbox */
    int            over_reads;       /* reads longer than the mailbox */
} ccs811_model_t;

static void ccs811_model_update_status(ccs811_model_t *m)
{
    uint8_t st = 0;
    if (m->app_valid) st |= 0x10;
    if (m->app_mode) st |= 0x80;  /* FW_MODE */
    if (m->data_ready) st |= 0x08;
    if (m->error_id != 0) st |= 0x01;
    m->hw.regs[CCS811_REG_STATUS] = st;
}

static void ccs811_model_flag(ccs811_model_t *m, uint8_t code)
{
    m->error_id |= code;
    ccs811_model_update_status(m);
}

static uint8_t ccs811_model_drive_mode(const ccs811_model_t *m)
{
    return (uint8_t)((m->hw.regs[CCS811_REG_MEAS_MODE] >> 4) & 0x07);
}

/* p17: "A new sample is placed in ALG_RESULT_DATA and RAW_DATA registers and
 * the DATA_READY bit in the STATUS register is set", but "In mode 4, the
 * ALG_RESULT_DATA is not updated, only RAW_DATA". */
static void ccs811_model_sample(ccs811_model_t *m)
{
    if (ccs811_model_drive_mode(m) != 4) {
        m->alg_result[0] = (uint8_t)(m->true_eco2 >> 8);
        m->alg_result[1] = (uint8_t)(m->true_eco2 & 0xFF);
        m->alg_result[2] = (uint8_t)(m->true_tvoc >> 8);
        m->alg_result[3] = (uint8_t)(m->true_tvoc & 0xFF);
    }
    m->raw_data = m->true_raw;
    m->data_ready = true;
    ccs811_model_update_status(m);
    m->measure_count++;
}

/* Let time pass: one sample per measurement interval */
static void ccs811_model_advance_ms(ccs811_model_t *m, uint32_t ms)
{
    uint32_t interval = ccs811_model_interval_ms[ccs811_model_drive_mode(m)];

    if (!m->app_mode || interval == 0) return;

    while (ms >= m->until_sample_ms) {
        ms -= m->until_sample_ms;
        ccs811_model_sample(m);
        m->until_sample_ms = interval;
    }
    m->until_sample_ms -= ms;
}

/* p17: DRIVE_MODE 1xx apart from 100 is reserved, p22: MEASMODE_INVALID. Bit 7
 * and bits 1:0 are reserved. */
static void ccs811_model_write_meas_mode(ccs811_model_t *m, uint8_t val)
{
    uint8_t mode = (uint8_t)((val >> 4) & 0x07);

    if (mode > 4) {
        ccs811_model_flag(m, CCS811_MODEL_MEASMODE_INVALID);
        return;
    }
    m->hw.regs[CCS811_REG_MEAS_MODE] = (uint8_t)(val & 0x7C);
    m->until_sample_ms = ccs811_model_interval_ms[mode];
}

/* Fill box with a readable mailbox and return its size, or 0 if the current map
 * has none: p24 for boot mode, p15 for application mode. */
static uint8_t ccs811_model_mailbox(const ccs811_model_t *m, uint8_t reg,
                                    uint8_t box[8])
{
    memset(box, 0, 8);

    switch (reg) {
    case CCS811_REG_STATUS:
    case CCS811_REG_HW_ID:
        box[0] = m->hw.regs[reg];
        return 1;
    case CCS811_REG_HW_VERSION:
        box[0] = 0x10;  /* "The value is 0x1X" */
        return 1;
    case CCS811_REG_FW_BOOT_VER:
    case CCS811_REG_FW_APP_VER:
        return 2;
    case CCS811_REG_ERROR_ID:
        box[0] = m->error_id;
        return 1;
    default:
        break;
    }

    if (!m->app_mode) return 0;

    switch (reg) {
    case CCS811_REG_MEAS_MODE:
        box[0] = m->hw.regs[reg];
        return 1;
    case CCS811_REG_ALG_RESULT:
        /* p18: eCO2, TVOC, STATUS, ERROR_ID, RAW_DATA */
        memcpy(box, m->alg_result, sizeof(m->alg_result));
        box[4] = m->hw.regs[CCS811_REG_STATUS];
        box[5] = m->error_id;
        box[6] = (uint8_t)(m->raw_data >> 8);
        box[7] = (uint8_t)(m->raw_data & 0xFF);
        return 8;
    case CCS811_REG_RAW_DATA:
        box[0] = (uint8_t)(m->raw_data >> 8);
        box[1] = (uint8_t)(m->raw_data & 0xFF);
        return 2;
    case CCS811_REG_NTC:
        return 4;
    case CCS811_REG_BASELINE:
        return 2;
    default:
        return 0;
    }
}

/* The W and R/W registers of the current map */
static bool ccs811_model_writable(const ccs811_model_t *m, uint8_t reg)
{
    if (reg == CCS811_REG_SW_RESET) return true;
    if (!m->app_mode) {
        return reg >= CCS811_REG_APP_ERASE && reg <= CCS811_REG_APP_START;
    }
    return reg == CCS811_REG_MEAS_MODE || reg == CCS811_REG_ENV_DATA ||
           reg == CCS811_REG_THRESHOLDS || reg == CCS811_REG_BASELINE;
}

static void ccs811_model_init(ccs811_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* HW_ID (read-only) */
    hw_model_set_reset_value(&m->hw, CCS811_REG_HW_ID, 0x81);
    hw_model_set_readonly(&m->hw, CCS811_REG_HW_ID);

    /* STATUS (read-only, managed by model) */
    hw_model_set_readonly(&m->hw, CCS811_REG_STATUS);

    /* Default values */
    m->true_eco2 = 412;
    m->true_tvoc = 5;

    /* p19: 50 %RH and 25 C */
    m->env_data[0] = 0x64;
    m->env_data[2] = 0x64;

    hw_model_reset(&m->hw);

    /* Boot mode, with a valid application loaded */
    m->app_valid = true;
    ccs811_model_update_status(m);
}

/*
 * Exact bus traffic, for the HAL's write_raw. ctx is &model.hw. Datasheet v1-00
 * p13: the first byte selects a register and the rest is data. p24, p25: only
 * 0xF4 alone starts the application, so a write that carries data does not.
 * p15: ENV_DATA takes its 4 bytes in one write.
 */
__attribute__((unused))
static bool ccs811_model_write_raw(void *ctx, uint8_t dev_addr,
                                   const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    ccs811_model_t *m = (ccs811_model_t *)hw->user;
    uint8_t box[8];

    if (hw_model__should_fail(hw)) return false;
    if (len == 0) return true;

    uint8_t reg = data[0];

    if (len == 1 && reg == CCS811_REG_APP_START && !m->app_mode) {
        if (m->app_valid) {
            m->app_mode = true;
            ccs811_model_update_status(m);
        }
    } else if (!ccs811_model_writable(m, reg) &&
               (len > 1 || ccs811_model_mailbox(m, reg, box) == 0)) {
        /* One byte alone may select a register to read, p13 */
        ccs811_model_flag(m, CCS811_MODEL_WRITE_REG_INVALID);
    } else if (len == 2 && reg == CCS811_REG_MEAS_MODE) {
        ccs811_model_write_meas_mode(m, data[1]);
    } else if (len == 5 && reg == CCS811_REG_ENV_DATA) {
        memcpy(m->env_data, &data[1], sizeof(m->env_data));
        m->env_write_count++;
    }
    return true;
}

/* A register and one data byte, for the HAL's write_reg */
__attribute__((unused))
static bool ccs811_model_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                                   uint8_t val)
{
    const uint8_t tx[2] = { reg, val };

    return ccs811_model_write_raw(ctx, dev_addr, tx, 2);
}

/* The HAL's read_regs: reg is a mailbox, so len bytes all come from it */
__attribute__((unused))
static bool ccs811_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                   uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    ccs811_model_t *m = (ccs811_model_t *)hw->user;
    uint8_t box[8];

    if (hw_model__should_fail(hw)) return false;

    uint8_t size = ccs811_model_mailbox(m, reg, box);
    if (size == 0) {
        ccs811_model_flag(m, CCS811_MODEL_READ_REG_INVALID);
    }
    if (len > size) m->over_reads++;

    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (i < size) ? box[i] : 0x00;
    }

    /* p16: DATA_READY "is cleared when ALG_RESULT_DATA is read" */
    if (reg == CCS811_REG_ALG_RESULT && size != 0) {
        m->data_ready = false;
        ccs811_model_update_status(m);
    }
    return true;
}

/* The HAL's delay_ms: the time a driver waits passes on the chip as well */
__attribute__((unused))
static void ccs811_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;

    hw->delay_ms_total += ms;
    ccs811_model_advance_ms((ccs811_model_t *)hw->user, ms);
}

/* The air on the sensor. The next sample picks it up. */
__attribute__((unused))
static void ccs811_model_set_air_quality(ccs811_model_t *m,
                                         uint16_t eco2, uint16_t tvoc)
{
    m->true_eco2 = eco2;
    m->true_tvoc = tvoc;
}

/* The ADC reading behind RAW_DATA, for the next sample */
__attribute__((unused))
static void ccs811_model_set_raw(ccs811_model_t *m, uint16_t raw)
{
    m->true_raw = raw;
}

/* p16: APP_VALID 0 is "No application firmware loaded" */
__attribute__((unused))
static void ccs811_model_set_app_valid(ccs811_model_t *m, bool valid)
{
    m->app_valid = valid;
    ccs811_model_update_status(m);
}

#endif /* HW_MODEL_CCS811_H */
