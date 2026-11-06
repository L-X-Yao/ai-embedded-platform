// SPDX-License-Identifier: MIT
/*
 * hw_model_bmp280.h — BMP280 hardware register model from datasheet
 *
 * Models BST-BMP280-DS001-26 register behavior:
 * - Chip ID 0x58 at 0xD0 (read-only)
 * - Calibration data at 0x88..0x9F (read-only, set from "fused" values)
 * - Soft reset via 0xE0 = 0xB6, which always reads 0x00
 * - ctrl_meas (0xF4): mode bits [1:0], 01 and 10 forced, back to 00 at the end
 * - Status register 0xF3: bit 0 (im_update) during an NVM copy, then bit 3
 *   (measuring) for the rest of the conversion
 * - ADC output at 0xF7..0xFC, reset state 0x80 in each MSB, changing only when
 *   a conversion ends
 * - No access for tstartup after power-on or a soft reset
 *
 * Time passes through bmp280_model_advance_us(), the HAL delay and the bus: a
 * transfer costs its clocks at bus_khz. Table 13 gives the maximum for five
 * oversampling pairs. 1.25 + 2.3 T + 2.3 P + 0.575 ms matches all five to the
 * printed decimal, so the model takes it for every pair. The manual gives no
 * length for the NVM copy, so BMP280_MODEL_NVM_COPY_US is the model's choice.
 */

#ifndef HW_MODEL_BMP280_H
#define HW_MODEL_BMP280_H

#include "hw_model.h"

/* --- BMP280 register addresses (from datasheet Table 18) --- */
#define BMP280_REG_CALIB00   0x88
#define BMP280_REG_ID        0xD0
#define BMP280_REG_RESET     0xE0
#define BMP280_REG_STATUS    0xF3
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG    0xF5
#define BMP280_REG_PRESS_MSB 0xF7
#define BMP280_REG_TEMP_MSB  0xFA

#define BMP280_STATUS_MEASURING 0x08
#define BMP280_STATUS_IM_UPDATE 0x01

/* p.8: tstartup */
#define BMP280_MODEL_STARTUP_US  2000u
#define BMP280_MODEL_NVM_COPY_US 500u
#define BMP280_MODEL_BUS_KHZ     400u

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int32_t          true_temp_raw;   /* "real" temperature ADC (20-bit) */
    int32_t          true_press_raw;  /* "real" pressure ADC (20-bit) */
    int              measure_count;   /* how many forced measurements triggered */
    uint32_t         bus_khz;
    uint32_t         startup_left_us;  /* no ACK until 0 */
    uint32_t         nvm_left_us;      /* NVM copy still running */
    uint32_t         measure_left_us;  /* conversion after the copy */
    bool             image_loading;    /* the copy of a power-on reset */
    uint8_t          latched_ctrl_meas;
} bmp280_model_t;

/* Example calibration. The manual prints none, so this is not a device's data. */
static void bmp280_model_set_reference_calib(bmp280_model_t *m)
{
    /* dig_T1..T3, dig_P1..P9 (example values producing ~25°C / ~101325 Pa) */
    uint8_t calib[24] = {
        0x70, 0x6B,   /* dig_T1 = 27504 */
        0x43, 0x67,   /* dig_T2 = 26435 */
        0x18, 0xFC,   /* dig_T3 = -1000 */
        0x7D, 0x8E,   /* dig_P1 = 36477 */
        0x43, 0xD6,   /* dig_P2 = -10685 */
        0xD0, 0x0B,   /* dig_P3 = 3024 */
        0x27, 0x0B,   /* dig_P4 = 2855 */
        0x8C, 0x00,   /* dig_P5 = 140 */
        0xF9, 0xFF,   /* dig_P6 = -7 */
        0x8C, 0x3C,   /* dig_P7 = 15500 */
        0xF8, 0xC6,   /* dig_P8 = -14600 */
        0x70, 0x17,   /* dig_P9 = 6000 */
    };
    for (unsigned i = 0; i < 24; i++) {
        m->hw.regs[BMP280_REG_CALIB00 + i] = calib[i];
        m->hw.reset_values[BMP280_REG_CALIB00 + i] = calib[i];
    }
}

/* Write ADC values for "true" temperature and pressure into data registers.
 * A channel whose osrs field is 000 is skipped and reads 0x80000 (Tables 21, 22). */
static void bmp280_model_set_adc_output(bmp280_model_t *m)
{
    uint8_t osrs_t = (uint8_t)(m->latched_ctrl_meas >> 5);
    uint8_t osrs_p = (uint8_t)((m->latched_ctrl_meas >> 2) & 0x07);

    /* pressure: 0xF7..0xF9 (20-bit, MSB first, [3:0] of xlsb) */
    uint32_t p = osrs_p ? ((uint32_t)m->true_press_raw & 0xFFFFFu) : 0x80000u;
    m->hw.regs[0xF7] = (uint8_t)(p >> 12);
    m->hw.regs[0xF8] = (uint8_t)(p >> 4);
    m->hw.regs[0xF9] = (uint8_t)((p & 0x0F) << 4);

    /* temperature: 0xFA..0xFC (20-bit, same layout) */
    uint32_t t = osrs_t ? ((uint32_t)m->true_temp_raw & 0xFFFFFu) : 0x80000u;
    m->hw.regs[0xFA] = (uint8_t)(t >> 12);
    m->hw.regs[0xFB] = (uint8_t)(t >> 4);
    m->hw.regs[0xFC] = (uint8_t)((t & 0x0F) << 4);
}

/* Tables 21, 22: 000 skipped, then x1 to x8, "101, others" x16. */
static uint32_t bmp280_model_oversampling(uint8_t osrs)
{
    if (osrs == 0)
        return 0;
    return (osrs >= 5u) ? 16u : 1u << (osrs - 1u);
}

/* The maximum measurement time, in us: see the top of this file */
static uint32_t bmp280_model_measure_us(uint8_t ctrl_meas)
{
    uint8_t osrs_p = (uint8_t)((ctrl_meas >> 2) & 0x07);
    uint32_t us = 1250u + 2300u * bmp280_model_oversampling((uint8_t)(ctrl_meas >> 5));

    if (osrs_p != 0)
        us += 2300u * bmp280_model_oversampling(osrs_p) + 575u;
    return us;
}

static void bmp280_model_end_nvm_copy(bmp280_model_t *m)
{
    m->image_loading = false;
    m->hw.regs[BMP280_REG_STATUS] = (m->measure_left_us != 0) ? BMP280_STATUS_MEASURING : 0;
}

static void bmp280_model_end_conversion(bmp280_model_t *m)
{
    bmp280_model_set_adc_output(m);
    m->hw.regs[BMP280_REG_STATUS] = 0;
    /* p.15: "the sensor returns to sleep mode". Normal mode stays. */
    if ((m->hw.regs[BMP280_REG_CTRL_MEAS] & 0x03) != 0x03)
        m->hw.regs[BMP280_REG_CTRL_MEAS] &= 0xFC;
}

/* Lets time pass: first the NVM copy, then the conversion after it. */
static void bmp280_model_advance_us(bmp280_model_t *m, uint32_t us)
{
    m->startup_left_us = (us < m->startup_left_us) ? m->startup_left_us - us : 0u;

    if (m->nvm_left_us != 0) {
        if (us < m->nvm_left_us) {
            m->nvm_left_us -= us;
            return;
        }
        us -= m->nvm_left_us;
        m->nvm_left_us = 0;
        bmp280_model_end_nvm_copy(m);
    }
    if (m->measure_left_us != 0) {
        if (us < m->measure_left_us) {
            m->measure_left_us -= us;
            return;
        }
        m->measure_left_us = 0;
        bmp280_model_end_conversion(m);
    }
}

/* Callback: ctrl_meas write triggers measurement */
static void bmp280_model_on_ctrl_meas(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bmp280_model_t *m = (bmp280_model_t *)user;

    m->latched_ctrl_meas = new_val;
    if ((new_val & 0x03) != 0) {
        /* Table 19: the data "are copied at power-on-reset and before every
         * conversion". The copy counts inside the measurement time. */
        m->nvm_left_us = BMP280_MODEL_NVM_COPY_US;
        m->measure_left_us = bmp280_model_measure_us(new_val) - BMP280_MODEL_NVM_COPY_US;
        m->hw.regs[BMP280_REG_STATUS] = BMP280_STATUS_IM_UPDATE;
        m->measure_count++;
    }
}

/* Power-on reset: tstartup with no access, then the NVM copy still runs. */
static void bmp280_model_power_on(bmp280_model_t *m)
{
    hw_model_reset(&m->hw);
    m->startup_left_us = BMP280_MODEL_STARTUP_US;
    m->nvm_left_us = BMP280_MODEL_STARTUP_US + BMP280_MODEL_NVM_COPY_US;
    m->measure_left_us = 0;
    m->image_loading = true;
    m->hw.regs[BMP280_REG_STATUS] = BMP280_STATUS_IM_UPDATE;
}

/* Callback: reset register write. p.24: "Writing other values than 0xB6 has no
 * effect. The readout value is always 0x00." */
static void bmp280_model_on_reset(void *user, uint8_t reg,
                                  uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bmp280_model_t *m = (bmp280_model_t *)user;
    m->hw.regs[BMP280_REG_RESET] = 0;
    if (new_val == 0xB6) {
        bmp280_model_power_on(m);
    }
}

/* A transfer's clocks: 9 per byte, plus START and STOP */
static uint32_t bmp280_model_bus_us(const bmp280_model_t *m, uint32_t bytes)
{
    return (9u * bytes + 2u) * 1000u / m->bus_khz;
}

/* HAL write: p.8, no communication before tstartup has passed */
__attribute__((unused))
static bool bmp280_model_write_reg(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t val)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    bmp280_model_t *m = (bmp280_model_t *)hw->user;
    bool ack = m->startup_left_us == 0
               && hw_model_i2c_write_reg(ctx, dev_addr, reg, val);

    bmp280_model_advance_us(m, bmp280_model_bus_us(m, 3u));
    return ack;
}

/* HAL read: the same, and calibration is not there until the copy is over */
__attribute__((unused))
static bool bmp280_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                   uint8_t *buf, uint16_t len)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    bmp280_model_t *m = (bmp280_model_t *)hw->user;
    bool ack = m->startup_left_us == 0
               && hw_model_i2c_read_regs(ctx, dev_addr, reg, buf, len);

    if (ack && m->image_loading) {
        for (uint16_t i = 0; i < len; i++) {
            unsigned at = (unsigned)reg + i;
            if (at >= BMP280_REG_CALIB00 && at < BMP280_REG_CALIB00 + 24u)
                buf[i] = 0;
        }
    }
    bmp280_model_advance_us(m, bmp280_model_bus_us(m, 3u + len));
    return ack;
}

/* HAL delay: the time the driver waits is the time the chip sees. */
__attribute__((unused))
static void bmp280_model_delay(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    hw_model_delay_ms(hw, ms);
    bmp280_model_advance_us((bmp280_model_t *)hw->user, ms * 1000u);
}

static void bmp280_model_init(bmp280_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, BMP280_REG_ID, 0x58);
    hw_model_set_readonly(&m->hw, BMP280_REG_ID);

    /* Calibration registers (read-only after factory trim) */
    for (int i = 0; i < 24; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BMP280_REG_CALIB00 + i));
    }
    bmp280_model_set_reference_calib(m);

    /* Status register: bit 3 (measuring) and bit 0 (im_update) are read-only */
    hw_model_set_readonly(&m->hw, BMP280_REG_STATUS);

    /* Table 18: each MSB of the data registers resets to 0x80 */
    hw_model_set_reset_value(&m->hw, BMP280_REG_PRESS_MSB, 0x80);
    hw_model_set_reset_value(&m->hw, BMP280_REG_TEMP_MSB, 0x80);

    /* ADC output registers (read-only, written by measurement engine) */
    for (int i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BMP280_REG_PRESS_MSB + i));
    }

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, BMP280_REG_CTRL_MEAS, bmp280_model_on_ctrl_meas);
    hw_model_set_on_write(&m->hw, BMP280_REG_RESET, bmp280_model_on_reset);

    /* Default "true" environment: ~25°C, ~101325 Pa */
    /* ADC raw for 25°C with reference calib ≈ 519888 (0x7EF10) */
    m->true_temp_raw = 519888;
    /* ADC raw for 101325 Pa ≈ 415680 (0x65A40) */
    m->true_press_raw = 415680;
    m->bus_khz = BMP280_MODEL_BUS_KHZ;

    bmp280_model_power_on(m);
}

/* Helper: set simulated temperature (raw ADC) */
static void bmp280_model_set_temperature(bmp280_model_t *m, int32_t adc_raw)
{
    m->true_temp_raw = adc_raw;
}

/* Helper: set simulated pressure (raw ADC) */
static void bmp280_model_set_pressure(bmp280_model_t *m, int32_t adc_raw)
{
    m->true_press_raw = adc_raw;
}

#endif /* HW_MODEL_BMP280_H */
