/*
 * hw_model_bmp280.h — BMP280 hardware register model from datasheet
 *
 * Models BST-BMP280-DS001-26 register behavior:
 * - Chip ID 0x58 at 0xD0 (read-only)
 * - Calibration data at 0x88..0x9F (read-only, set from "fused" values)
 * - Soft reset via 0xE0 = 0xB6
 * - Forced measurement via ctrl_meas (0xF4) mode bits [1:0]
 * - Status register 0xF3 bit 3 (measuring) auto-clears after trigger
 * - ADC output at 0xF7..0xFC computed from calibration + "true" values
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

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    int32_t          true_temp_raw;   /* "real" temperature ADC (20-bit) */
    int32_t          true_press_raw;  /* "real" pressure ADC (20-bit) */
    int              measure_count;   /* how many forced measurements triggered */
} bmp280_model_t;

/* Datasheet reference calibration (from BST-BMP280-DS001-26 appendix 8.1) */
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

/* Write ADC values for "true" temperature and pressure into data registers */
static void bmp280_model_set_adc_output(bmp280_model_t *m)
{
    /* pressure: 0xF7..0xF9 (20-bit, MSB first, [3:0] of xlsb) */
    uint32_t p = (uint32_t)m->true_press_raw & 0xFFFFFu;
    m->hw.regs[0xF7] = (uint8_t)(p >> 12);
    m->hw.regs[0xF8] = (uint8_t)(p >> 4);
    m->hw.regs[0xF9] = (uint8_t)((p & 0x0F) << 4);

    /* temperature: 0xFA..0xFC (20-bit, same layout) */
    uint32_t t = (uint32_t)m->true_temp_raw & 0xFFFFFu;
    m->hw.regs[0xFA] = (uint8_t)(t >> 12);
    m->hw.regs[0xFB] = (uint8_t)(t >> 4);
    m->hw.regs[0xFC] = (uint8_t)((t & 0x0F) << 4);
}

/* Callback: ctrl_meas write triggers measurement */
static void bmp280_model_on_ctrl_meas(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bmp280_model_t *m = (bmp280_model_t *)user;
    uint8_t mode = new_val & 0x03;
    if (mode == 0x01 || mode == 0x03) {
        /* forced or normal mode → start measurement */
        m->hw.regs[BMP280_REG_STATUS] |= 0x08; /* measuring = 1 */
        /* simulate instant completion: write ADC, clear measuring */
        bmp280_model_set_adc_output(m);
        m->hw.regs[BMP280_REG_STATUS] = (uint8_t)(m->hw.regs[BMP280_REG_STATUS] & ~0x08u);
        if (mode == 0x01) {
            /* forced mode: revert to sleep after measurement */
            m->hw.regs[BMP280_REG_CTRL_MEAS] &= 0xFC;
        }
        m->measure_count++;
    }
}

/* Callback: reset register write */
static void bmp280_model_on_reset(void *user, uint8_t reg,
                                  uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bmp280_model_t *m = (bmp280_model_t *)user;
    if (new_val == 0xB6) {
        hw_model_reset(&m->hw);
    }
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

    hw_model_reset(&m->hw);
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
