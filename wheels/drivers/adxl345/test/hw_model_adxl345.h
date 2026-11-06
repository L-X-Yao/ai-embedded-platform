// SPDX-License-Identifier: MIT
/*
 * hw_model_adxl345.h — ADXL345 hardware register model from datasheet
 *
 * Models Analog Devices ADXL345 datasheet (Rev. G) register behavior:
 * - Chip ID 0xE5 at 0x00 (read-only), reset values of table 19
 * - BW_RATE (0x2C) sets the sample period and the turn-on time
 * - POWER_CTL (0x2D) controls measurement mode
 * - DATA_FORMAT (0x31) sets the scale, clipping and justification of a sample
 * - INT_SOURCE (0x30) shows DATA_READY, cleared by reading the data
 * - DATAX0..DATAZ1 (0x32..0x37) read-only, written only by a sample
 *
 * Time passes in adxl345_model_advance_us() and in the delay_ms callback. The
 * first sample comes a turn-on time after Measure, then one per period. Reads
 * in between see the last sample, or the reset zeros before the first.
 *
 * Use adxl345_model_read_regs as the read callback, so that DATA_READY clears.
 */

#ifndef HW_MODEL_ADXL345_H
#define HW_MODEL_ADXL345_H

#include "hw_model.h"

/* --- ADXL345 register addresses --- */
#define ADXL345_REG_DEVID        0x00
#define ADXL345_REG_ACT_TAP      0x2B
#define ADXL345_REG_BW_RATE      0x2C
#define ADXL345_REG_POWER_CTL    0x2D
#define ADXL345_REG_INT_SOURCE   0x30
#define ADXL345_REG_DATA_FORMAT  0x31
#define ADXL345_REG_DATAX0       0x32
#define ADXL345_REG_FIFO_STATUS  0x39

#define ADXL345_DATA_READY       0x80u

/* p.4: the turn-on time is "tau + 1.1 in milliseconds" */
#define ADXL345_TURN_ON_EXTRA_US 1100u

/* Sample period of each rate code of table 7, tau = 1/(data rate) */
static const uint32_t adxl345_model_period_us[16] = {
    10000000u, 5000000u, 2564103u, 1282051u, 641026u, 319489u, 160000u, 80000u,
    40000u, 20000u, 10000u, 5000u, 2500u, 1250u, 625u, 313u,
};

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t hw;
    int16_t        true_x;   /* acceleration in 1/256 g */
    int16_t        true_y;
    int16_t        true_z;
    int            measure_count;
    bool           measuring;
    uint32_t       until_sample_us;  /* time left to the next sample */
    int            config_writes_while_measuring;
} adxl345_model_t;

static uint32_t adxl345_model_period(const adxl345_model_t *m)
{
    return adxl345_model_period_us[m->hw.regs[ADXL345_REG_BW_RATE] & 0x0Fu];
}

/* An acceleration as DATA_FORMAT presents it. p.3: 256, 128, 64 and 32 LSB/g in
 * 10-bit mode, 256 LSB/g and 10 to 13 bits in full resolution. p.27: "All data,
 * except that for the +/-16 g range, must be clipped to avoid rollover." */
static uint16_t adxl345_model_present(const adxl345_model_t *m, int16_t accel)
{
    uint8_t fmt       = m->hw.regs[ADXL345_REG_DATA_FORMAT];
    int     range     = fmt & 0x03;
    bool    full_res  = (fmt & 0x08u) != 0;
    int     bits      = full_res ? 10 + range : 10;
    int32_t counts    = full_res ? accel : (int32_t)accel / (1 << range);
    int32_t span      = (int32_t)1 << bits;

    if (range == 3) {
        counts = ((counts + span / 2) % span + span) % span - span / 2;
    } else if (counts > span / 2 - 1) {
        counts = span / 2 - 1;
    } else if (counts < -(span / 2)) {
        counts = -(span / 2);
    }

    /* p.27: Justify "selects left-justified (MSB) mode" */
    if (fmt & 0x04u) counts *= (int32_t)1 << (16 - bits);

    return (uint16_t)(counts & 0xFFFF);
}

static void adxl345_model_store(adxl345_model_t *m, uint8_t reg, int16_t accel)
{
    uint16_t out = adxl345_model_present(m, accel);
    m->hw.regs[reg]                = (uint8_t)(out & 0xFFu);
    m->hw.regs[(uint8_t)(reg + 1)] = (uint8_t)(out >> 8);
}

static void adxl345_model_sample(adxl345_model_t *m)
{
    adxl345_model_store(m, 0x32, m->true_x);
    adxl345_model_store(m, 0x34, m->true_y);
    adxl345_model_store(m, 0x36, m->true_z);
    m->hw.regs[ADXL345_REG_INT_SOURCE] |= ADXL345_DATA_READY;
    m->measure_count++;
}

/* Let time pass: samples fall due at the turn-on time, then once a period */
static void adxl345_model_advance_us(adxl345_model_t *m, uint32_t us)
{
    if (!m->measuring) return;

    while (us >= m->until_sample_us) {
        us -= m->until_sample_us;
        adxl345_model_sample(m);
        m->until_sample_us = adxl345_model_period(m);
    }
    m->until_sample_us -= us;
}

static void adxl345_model_on_power_ctl(void *user, uint8_t reg,
                                       uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    adxl345_model_t *m = (adxl345_model_t *)user;
    bool was_measuring = m->measuring;

    m->measuring = (new_val & 0x08) != 0;
    if (m->measuring && !was_measuring) {
        m->until_sample_us = adxl345_model_period(m) + ADXL345_TURN_ON_EXTRA_US;
    }
}

/* p.12: "It is recommended to configure the device in standby mode" */
static void adxl345_model_on_config(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    adxl345_model_t *m = (adxl345_model_t *)user;

    if (m->measuring) m->config_writes_while_measuring++;
}

static void adxl345_model_init(adxl345_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, ADXL345_REG_DEVID, 0xE5);
    hw_model_set_readonly(&m->hw, ADXL345_REG_DEVID);

    /* p.25: "The default value is 0x0A, which translates to a 100 Hz output
     * data rate." D7:D5 are 0, table 25. */
    hw_model_set_reset_value(&m->hw, ADXL345_REG_BW_RATE, 0x0A);
    hw_model_set_writable_mask(&m->hw, ADXL345_REG_BW_RATE, 0x1F);
    hw_model_set_on_write(&m->hw, ADXL345_REG_BW_RATE, adxl345_model_on_config);

    /* Table 19: INT_SOURCE resets to 00000010b and is read only */
    hw_model_set_reset_value(&m->hw, ADXL345_REG_INT_SOURCE, 0x02);
    hw_model_set_readonly(&m->hw, ADXL345_REG_INT_SOURCE);

    /* Table 19: the other read-only registers */
    hw_model_set_readonly(&m->hw, ADXL345_REG_ACT_TAP);
    hw_model_set_readonly(&m->hw, ADXL345_REG_FIFO_STATUS);

    /* Data registers (read-only, written by measurement engine) */
    for (int i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(ADXL345_REG_DATAX0 + i));
    }

    /* Write callback for POWER_CTL. D7:D6 are 0, table 26. */
    hw_model_set_writable_mask(&m->hw, ADXL345_REG_POWER_CTL, 0x3F);
    hw_model_set_on_write(&m->hw, ADXL345_REG_POWER_CTL,
                          adxl345_model_on_power_ctl);

    /* DATA_FORMAT. D4 is 0, table 34. */
    hw_model_set_writable_mask(&m->hw, ADXL345_REG_DATA_FORMAT, 0xEF);
    hw_model_set_on_write(&m->hw, ADXL345_REG_DATA_FORMAT,
                          adxl345_model_on_config);

    /* Default "true" environment: 0g X, 0g Y, +1g Z */
    m->true_x = 0;
    m->true_y = 0;
    m->true_z = 256;

    hw_model_reset(&m->hw);
}

/* The acceleration on the part, in 1/256 g. The next sample picks it up. */
__attribute__((unused))
static void adxl345_model_set_accel(adxl345_model_t *m,
                                    int16_t x, int16_t y, int16_t z)
{
    m->true_x = x;
    m->true_y = y;
    m->true_z = z;
}

/* HAL read callback. p.27: DATA_READY is "cleared by reading data from the
 * DATAX, DATAY, and DATAZ registers". */
__attribute__((unused))
static bool adxl345_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                    uint8_t *buf, uint16_t len)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;

    if (!hw_model_i2c_read_regs(ctx, dev_addr, reg, buf, len)) return false;
    if (reg <= 0x37 && (unsigned)reg + len > 0x32u) {
        hw->regs[ADXL345_REG_INT_SOURCE] &= (uint8_t)~ADXL345_DATA_READY;
    }
    return true;
}

/* HAL delay callback: the time a driver waits passes on the part as well */
__attribute__((unused))
static void adxl345_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;

    hw->delay_ms_total += ms;
    adxl345_model_advance_us((adxl345_model_t *)hw->user, ms * 1000u);
}

#endif /* HW_MODEL_ADXL345_H */
