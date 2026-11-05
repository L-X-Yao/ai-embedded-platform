// SPDX-License-Identifier: MIT
/*
 * hw_model_bme680.h — BME680 hardware register model from datasheet
 *
 * Models Bosch BME680 register behavior (BST-BME680-DS001-00):
 * - Chip ID 0x61 at 0xD0 (read-only)
 * - Calibration coeff1 at 0x89..0xA1 (25 bytes, read-only)
 * - Calibration coeff2 at 0xE1..0xF0 (16 bytes, read-only)
 * - Extra gas calibration at 0x00, 0x02, 0x04 (read-only)
 * - Soft reset via 0xE0 = 0xB6
 * - ctrl_hum (0x72), ctrl_meas (0x74), ctrl_gas1 (0x71)
 * - Forced measurement via ctrl_meas mode bits [1:0]
 * - Meas status at 0x1D, data at 0x1F..0x2B (13 bytes)
 */

#ifndef HW_MODEL_BME680_H
#define HW_MODEL_BME680_H

#include "hw_model.h"

/* --- BME680 register addresses --- */
#define BME680_REG_CHIP_ID     0xD0u
#define BME680_REG_RESET       0xE0u
#define BME680_REG_COEFF1      0x89u
#define BME680_REG_COEFF2      0xE1u
#define BME680_REG_CTRL_HUM    0x72u
#define BME680_REG_CTRL_MEAS   0x74u
#define BME680_REG_CTRL_GAS1   0x71u
#define BME680_REG_GAS_WAIT0   0x64u
#define BME680_REG_RES_HEAT0   0x5Au
#define BME680_REG_MEAS_STATUS 0x1Du
#define BME680_REG_DATA_START  0x1Fu

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t   hw;
    uint32_t         true_temp_adc;   /* 20-bit raw */
    uint32_t         true_press_adc;  /* 20-bit raw */
    uint16_t         true_hum_adc;    /* 16-bit raw */
    uint16_t         true_gas_adc;    /* 10-bit raw */
    uint8_t          true_gas_range;  /* 4-bit */
    int              measure_count;
} bme680_model_t;

/* Set reference calibration data (producing reasonable output) */
static void bme680_model_set_reference_calib(bme680_model_t *m)
{
    /* coeff1 at 0x89..0xA1 (25 bytes) — indices based on driver parsing */
    uint8_t c1[25];
    memset(c1, 0, sizeof(c1));
    /* par_t2 at c1[1..2] = 26435 */
    c1[1] = 0x43; c1[2] = 0x67;
    /* par_t3 at c1[3] = -2 (0xFE) */
    c1[3] = 0xFE;
    /* par_p1 at c1[5..6] = 36000 */
    c1[5] = 0xA0; c1[6] = 0x8C;
    /* par_p2 at c1[7..8] = -10000 */
    c1[7] = 0xF0; c1[8] = 0xD8;
    /* par_p3 at c1[9] = 88 */
    c1[9] = 88;
    /* par_p4 at c1[11..12] = 7000 */
    c1[11] = 0x58; c1[12] = 0x1B;
    /* par_p5 at c1[13..14] = -100 */
    c1[13] = 0x9C; c1[14] = 0xFF;
    /* par_p7 at c1[15] = 30 */
    c1[15] = 30;
    /* par_p6 at c1[16] = 30 */
    c1[16] = 30;
    /* par_p8 at c1[19..20] = -800 */
    c1[19] = 0xE0; c1[20] = 0xFC;
    /* par_p9 at c1[21..22] = -2000 */
    c1[21] = 0x30; c1[22] = 0xF8;
    /* par_p10 at c1[23] = 30 */
    c1[23] = 30;
    for (unsigned i = 0; i < 25; i++) {
        m->hw.regs[BME680_REG_COEFF1 + i] = c1[i];
        m->hw.reset_values[BME680_REG_COEFF1 + i] = c1[i];
    }

    /* coeff2 at 0xE1..0xF0 (16 bytes) */
    uint8_t c2[16];
    memset(c2, 0, sizeof(c2));
    /* par_h2 at c2[0..1] high nibble: (c2[0]<<4) | (c2[1]>>4) = 800 => c2[0]=50, c2[1]=0x00 */
    c2[0] = 50; c2[1] = 0x00;
    /* par_h1 at c2[1..2] low nibble: (c2[2]<<4) | (c2[1]&0x0F) = 750 => c2[2]=46, c2[1]|=0x0E */
    c2[1] |= 0x0E; c2[2] = 46;
    /* par_h3 at c2[3] = -2 */
    c2[3] = 0xFE;
    /* par_h4 at c2[4] = 0 */
    c2[4] = 0;
    /* par_h5 at c2[5] = 0 */
    c2[5] = 0;
    /* par_h6 at c2[6] = 120 */
    c2[6] = 120;
    /* par_h7 at c2[7] = -100 */
    c2[7] = 0x9C;
    /* par_t1 at c2[8..9] = 25000 */
    c2[8] = 0xA8; c2[9] = 0x61;
    /* par_gh2 at c2[10..11] = -5000 */
    c2[10] = 0x78; c2[11] = 0xEC;
    /* par_gh1 at c2[12] = -30 */
    c2[12] = 0xE2;
    /* par_gh3 at c2[13] = -2 */
    c2[13] = 0xFE;
    for (unsigned i = 0; i < 16; i++) {
        m->hw.regs[BME680_REG_COEFF2 + i] = c2[i];
        m->hw.reset_values[BME680_REG_COEFF2 + i] = c2[i];
    }

    /* Extra gas calibration registers */
    /* 0x02: res_heat_range at bits [5:4] = 1 */
    m->hw.regs[0x02] = 0x10;
    m->hw.reset_values[0x02] = 0x10;
    /* 0x00: res_heat_val = 50 */
    m->hw.regs[0x00] = 50;
    m->hw.reset_values[0x00] = 50;
    /* 0x04: range_sw_err at bits [7:4] = 0 */
    m->hw.regs[0x04] = 0x00;
    m->hw.reset_values[0x04] = 0x00;
}

/* Write ADC values into data registers (0x1F..0x2B) */
static void bme680_model_set_data_regs(bme680_model_t *m)
{
    /* press: 0x1F..0x21 (20-bit) */
    m->hw.regs[0x1F] = (uint8_t)(m->true_press_adc >> 12);
    m->hw.regs[0x20] = (uint8_t)(m->true_press_adc >> 4);
    m->hw.regs[0x21] = (uint8_t)((m->true_press_adc & 0x0F) << 4);
    /* temp: 0x22..0x24 (20-bit) */
    m->hw.regs[0x22] = (uint8_t)(m->true_temp_adc >> 12);
    m->hw.regs[0x23] = (uint8_t)(m->true_temp_adc >> 4);
    m->hw.regs[0x24] = (uint8_t)((m->true_temp_adc & 0x0F) << 4);
    /* hum: 0x25..0x26 (16-bit, big-endian) */
    m->hw.regs[0x25] = (uint8_t)(m->true_hum_adc >> 8);
    m->hw.regs[0x26] = (uint8_t)(m->true_hum_adc & 0xFF);
    /* padding: 0x27..0x29 */
    m->hw.regs[0x27] = 0; m->hw.regs[0x28] = 0; m->hw.regs[0x29] = 0;
    /* gas: 0x2A = gas_r MSB (adc_gas[9:2]), 0x2B = gas_r LSB[7:6] + valid[5] + range[3:0] */
    m->hw.regs[0x2A] = (uint8_t)(m->true_gas_adc >> 2);
    m->hw.regs[0x2B] = (uint8_t)(((m->true_gas_adc & 0x03) << 6) |
                                  (1u << 5) |  /* gas_valid = 1 */
                                  (1u << 4) |  /* heat_stab = 1 */
                                  (m->true_gas_range & 0x0F));
    /* meas_status: new_data = 1 */
    m->hw.regs[BME680_REG_MEAS_STATUS] = 0x80;
}

/* Callback: ctrl_meas write triggers measurement */
static void bme680_model_on_ctrl_meas(void *user, uint8_t reg,
                                       uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bme680_model_t *m = (bme680_model_t *)user;
    uint8_t mode = new_val & 0x03;
    if (mode == 0x01) {
        /* forced mode: populate data registers */
        bme680_model_set_data_regs(m);
        /* clear mode bits after measurement */
        m->hw.regs[BME680_REG_CTRL_MEAS] &= 0xFC;
        m->measure_count++;
    }
}

/* Callback: reset register write */
static void bme680_model_on_reset(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    bme680_model_t *m = (bme680_model_t *)user;
    if (new_val == 0xB6) {
        hw_model_reset(&m->hw);
    }
}

static void bme680_model_init(bme680_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only) */
    hw_model_set_reset_value(&m->hw, BME680_REG_CHIP_ID, 0x61);
    hw_model_set_readonly(&m->hw, BME680_REG_CHIP_ID);

    /* Calibration coeff1 0x89..0xA1 (read-only) */
    for (unsigned i = 0; i < 25; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BME680_REG_COEFF1 + i));
    }
    /* Calibration coeff2 0xE1..0xF0 (read-only) */
    for (unsigned i = 0; i < 16; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BME680_REG_COEFF2 + i));
    }
    /* Extra gas calib regs (read-only) */
    hw_model_set_readonly(&m->hw, 0x00);
    hw_model_set_readonly(&m->hw, 0x02);
    hw_model_set_readonly(&m->hw, 0x04);

    /* Meas status (read-only) */
    hw_model_set_readonly(&m->hw, BME680_REG_MEAS_STATUS);

    /* Data registers 0x1F..0x2B (read-only, written by measurement) */
    for (unsigned i = 0; i < 13; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(BME680_REG_DATA_START + i));
    }

    bme680_model_set_reference_calib(m);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, BME680_REG_CTRL_MEAS, bme680_model_on_ctrl_meas);
    hw_model_set_on_write(&m->hw, BME680_REG_RESET, bme680_model_on_reset);

    /* Default "true" environment: ~25C, ~101325 Pa, ~50%RH, ~200k ohm gas */
    m->true_temp_adc  = 500000;
    m->true_press_adc = 400000;
    m->true_hum_adc   = 20000;
    m->true_gas_adc   = 500;
    m->true_gas_range = 4;

    hw_model_reset(&m->hw);
    bme680_model_set_data_regs(m);
}

static void bme680_model_set_temperature(bme680_model_t *m, uint32_t adc)
{
    m->true_temp_adc = adc;
}

static void bme680_model_set_pressure(bme680_model_t *m, uint32_t adc)
{
    m->true_press_adc = adc;
}

#endif /* HW_MODEL_BME680_H */
