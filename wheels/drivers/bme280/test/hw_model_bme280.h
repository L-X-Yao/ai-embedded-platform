// SPDX-License-Identifier: MIT
/*
 * hw_model_bme280.h — BME280 hardware register model from datasheet
 *
 * Models BST-BME280-DS002-15 register behavior:
 * - Chip ID 0x60 at 0xD0 (read-only)
 * - Calibration data at 0x88..0xA1 (26 bytes, temp+press+h1) (read-only)
 * - Humidity calibration at 0xE1..0xE7 (7 bytes) (read-only)
 * - Soft reset via 0xE0 = 0xB6
 * - ctrl_hum (0xF2): humidity oversampling (must write before ctrl_meas)
 * - ctrl_meas (0xF4): forced measurement via mode bits [1:0]
 * - Status register 0xF3 bit 3 (measuring) auto-clears after trigger
 * - ADC output at 0xF7..0xFE (press[3]+temp[3]+hum[2])
 */

#ifndef HW_MODEL_BME280_H
#define HW_MODEL_BME280_H

#include "hw_model.h"

/* --- BME280 register addresses (from datasheet) --- */
#define BME280_REG_CALIB00    0x88
#define BME280_REG_CALIB_H1   0xA1
#define BME280_REG_ID         0xD0
#define BME280_REG_RESET      0xE0
#define BME280_REG_CALIB_H2   0xE1
#define BME280_REG_CTRL_HUM   0xF2
#define BME280_REG_STATUS     0xF3
#define BME280_REG_CTRL_MEAS  0xF4
#define BME280_REG_CONFIG     0xF5
#define BME280_REG_PRESS_MSB  0xF7
#define BME280_REG_HUM_MSB    0xFD

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int32_t          true_temp_raw;   /* "real" temperature ADC (20-bit) */
    int32_t          true_press_raw;  /* "real" pressure ADC (20-bit) */
    int32_t          true_hum_raw;    /* "real" humidity ADC (16-bit) */
    int              measure_count;
} bme280_model_t;

/* Datasheet reference calibration (producing ~25C / ~101325 Pa / ~50 %RH) */
static void bme280_model_set_reference_calib(bme280_model_t *m)
{
    /* dig_T1..T3, dig_P1..P9 at 0x88..0x9F (24 bytes) */
    uint8_t calib_tp[24] = {
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
        m->hw.regs[BME280_REG_CALIB00 + i] = calib_tp[i];
        m->hw.reset_values[BME280_REG_CALIB00 + i] = calib_tp[i];
    }
    /* 0xA0..0xA1: reserved + dig_H1 */
    m->hw.regs[0xA0] = 0x00;
    m->hw.reset_values[0xA0] = 0x00;
    m->hw.regs[BME280_REG_CALIB_H1] = 0x4B;   /* dig_H1 = 75 */
    m->hw.reset_values[BME280_REG_CALIB_H1] = 0x4B;

    /* Humidity calibration at 0xE1..0xE7 (7 bytes) */
    uint8_t calib_h[7] = {
        0x6E, 0x01,   /* dig_H2 = 366 (little-endian) */
        0x00,         /* dig_H3 = 0 */
        0x12,         /* dig_H4 high nibble => dig_H4 = (0x12 << 4) | (0x34 & 0x0F) = 0x124 = 292 */
        0x34,         /* dig_H4 low nibble[3:0] + dig_H5 low nibble[7:4] */
        0x00,         /* dig_H5 high byte => dig_H5 = (0x00 << 4) | (0x34 >> 4) = 0x3 = 3 */
        0x1E,         /* dig_H6 = 30 */
    };
    for (unsigned i = 0; i < 7; i++) {
        m->hw.regs[BME280_REG_CALIB_H2 + i] = calib_h[i];
        m->hw.reset_values[BME280_REG_CALIB_H2 + i] = calib_h[i];
    }
}

/* Write ADC values into data registers. A channel whose osrs field is 000 is
 * skipped and reads 0x80000, or 0x8000 for humidity (Tables 20, 23, 24).
 * ctrl_hum only takes effect at a ctrl_meas write, so it is read here. */
static void bme280_model_set_adc_output(bme280_model_t *m, uint8_t ctrl_meas)
{
    uint8_t osrs_t = (uint8_t)(ctrl_meas >> 5);
    uint8_t osrs_p = (uint8_t)((ctrl_meas >> 2) & 0x07);
    uint8_t osrs_h = (uint8_t)(m->hw.regs[BME280_REG_CTRL_HUM] & 0x07);

    /* pressure: 0xF7..0xF9 (20-bit, MSB first) */
    uint32_t p = osrs_p ? ((uint32_t)m->true_press_raw & 0xFFFFFu) : 0x80000u;
    m->hw.regs[0xF7] = (uint8_t)(p >> 12);
    m->hw.regs[0xF8] = (uint8_t)(p >> 4);
    m->hw.regs[0xF9] = (uint8_t)((p & 0x0F) << 4);

    /* temperature: 0xFA..0xFC (20-bit) */
    uint32_t t = osrs_t ? ((uint32_t)m->true_temp_raw & 0xFFFFFu) : 0x80000u;
    m->hw.regs[0xFA] = (uint8_t)(t >> 12);
    m->hw.regs[0xFB] = (uint8_t)(t >> 4);
    m->hw.regs[0xFC] = (uint8_t)((t & 0x0F) << 4);

    /* humidity: 0xFD..0xFE (16-bit, MSB first) */
    uint16_t h = osrs_h ? (uint16_t)(m->true_hum_raw & 0xFFFF) : (uint16_t)0x8000;
    m->hw.regs[0xFD] = (uint8_t)(h >> 8);
    m->hw.regs[0xFE] = (uint8_t)(h & 0xFF);
}

/* Callback: ctrl_meas write triggers measurement */
static void bme280_model_on_ctrl_meas(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bme280_model_t *m = (bme280_model_t *)user;
    uint8_t mode = new_val & 0x03;
    if (mode == 0x01 || mode == 0x03) {
        m->hw.regs[BME280_REG_STATUS] |= 0x08;
        bme280_model_set_adc_output(m, new_val);
        m->hw.regs[BME280_REG_STATUS] = (uint8_t)(m->hw.regs[BME280_REG_STATUS] & ~0x08u);
        if (mode == 0x01) {
            m->hw.regs[BME280_REG_CTRL_MEAS] &= 0xFC;
        }
        m->measure_count++;
    }
}

/* Callback: reset register write */
static void bme280_model_on_reset(void *user, uint8_t reg,
                                  uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bme280_model_t *m = (bme280_model_t *)user;
    if (new_val == 0xB6) {
        hw_model_reset(&m->hw);
    }
}

static void bme280_model_init(bme280_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, BME280_REG_ID, 0x60);
    hw_model_set_readonly(&m->hw, BME280_REG_ID);

    /* Calibration registers 0x88..0xA1 (read-only) */
    for (int i = 0; i < 26; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BME280_REG_CALIB00 + i));
    }
    /* Humidity calibration 0xE1..0xE7 (read-only) */
    for (int i = 0; i < 7; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BME280_REG_CALIB_H2 + i));
    }
    bme280_model_set_reference_calib(m);

    /* Status register (read-only) */
    hw_model_set_readonly(&m->hw, BME280_REG_STATUS);

    /* ADC output registers 0xF7..0xFE (read-only, 8 bytes) */
    for (int i = 0; i < 8; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BME280_REG_PRESS_MSB + i));
    }

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, BME280_REG_CTRL_MEAS, bme280_model_on_ctrl_meas);
    hw_model_set_on_write(&m->hw, BME280_REG_RESET, bme280_model_on_reset);

    /* Default "true" environment: ~25C, ~101325 Pa, ~50 %RH */
    m->true_temp_raw = 519888;
    m->true_press_raw = 415680;
    m->true_hum_raw = 28000;

    hw_model_reset(&m->hw);
}

static void bme280_model_set_temperature(bme280_model_t *m, int32_t adc_raw)
{
    m->true_temp_raw = adc_raw;
}

static void bme280_model_set_pressure(bme280_model_t *m, int32_t adc_raw)
{
    m->true_press_raw = adc_raw;
}

static void bme280_model_set_humidity(bme280_model_t *m, int32_t adc_raw)
{
    m->true_hum_raw = adc_raw;
}

#endif /* HW_MODEL_BME280_H */
