// SPDX-License-Identifier: MIT
/*
 * hw_model_ccs811.h — CCS811 hardware register model from datasheet
 *
 * Models CCS811 datasheet (ams AG) register behavior:
 * - HW_ID 0x81 at 0x20 (read-only)
 * - STATUS at 0x00 with APP_VALID, DATA_READY, FW_MODE bits
 * - APP_START: only the single byte 0xF4, with no data, starts the application
 * - MEAS_MODE at 0x01 controls drive mode
 * - ALG_RESULT at 0x02 (8 bytes, read-only)
 * - ENV_DATA at 0x05: one 4-byte write through write_raw, held apart from regs
 */

#ifndef HW_MODEL_CCS811_H
#define HW_MODEL_CCS811_H

#include "hw_model.h"

#define CCS811_REG_STATUS      0x00
#define CCS811_REG_MEAS_MODE   0x01
#define CCS811_REG_ALG_RESULT  0x02
#define CCS811_REG_ENV_DATA    0x05
#define CCS811_REG_HW_ID       0x20
#define CCS811_REG_APP_START   0xF4

typedef struct {
    hw_reg_model_t hw;
    uint16_t       true_eco2;
    uint16_t       true_tvoc;
    bool           app_mode;
    bool           data_ready;
    int            measure_count;
    uint8_t        env_data[4];      /* ENV_DATA mailbox */
    int            env_write_count;  /* whole 4-byte writes to ENV_DATA */
} ccs811_model_t;

static void ccs811_model_update_status(ccs811_model_t *m)
{
    uint8_t st = 0x10;  /* APP_VALID always set */
    if (m->app_mode) st |= 0x80;  /* FW_MODE */
    if (m->data_ready) st |= 0x08;
    m->hw.regs[CCS811_REG_STATUS] = st;
}

static void ccs811_model_update_data(ccs811_model_t *m)
{
    m->hw.regs[0x02] = (uint8_t)(m->true_eco2 >> 8);
    m->hw.regs[0x03] = (uint8_t)(m->true_eco2 & 0xFF);
    m->hw.regs[0x04] = (uint8_t)(m->true_tvoc >> 8);
    m->hw.regs[0x05] = (uint8_t)(m->true_tvoc & 0xFF);
}

static void ccs811_model_on_meas_mode(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    ccs811_model_t *m = (ccs811_model_t *)user;
    uint8_t mode = (uint8_t)((new_val >> 4) & 0x07);
    if (mode > 0) {
        m->data_ready = true;
        ccs811_model_update_data(m);
        ccs811_model_update_status(m);
        m->measure_count++;
    }
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

    /* ALG_RESULT (read-only) */
    for (int i = 0; i < 8; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(CCS811_REG_ALG_RESULT + i));
    }

    /* Callbacks */
    hw_model_set_on_write(&m->hw, CCS811_REG_MEAS_MODE, ccs811_model_on_meas_mode);

    /* Default values */
    m->true_eco2 = 412;
    m->true_tvoc = 5;

    /* p19: 50 %RH and 25 C */
    m->env_data[0] = 0x64;
    m->env_data[2] = 0x64;

    hw_model_reset(&m->hw);
    /* Set initial STATUS: APP_VALID */
    m->hw.regs[CCS811_REG_STATUS] = 0x10;
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

    if (hw_model__should_fail(hw)) return false;

    if (len == 1 && data[0] == CCS811_REG_APP_START) {
        m->app_mode = true;
        ccs811_model_update_status(m);
    } else if (len == 5 && data[0] == CCS811_REG_ENV_DATA) {
        memcpy(m->env_data, &data[1], sizeof(m->env_data));
        m->env_write_count++;
    }
    return true;
}

static void ccs811_model_set_air_quality(ccs811_model_t *m,
                                         uint16_t eco2, uint16_t tvoc)
{
    m->true_eco2 = eco2;
    m->true_tvoc = tvoc;
    if (m->data_ready) {
        ccs811_model_update_data(m);
    }
}

#endif /* HW_MODEL_CCS811_H */
