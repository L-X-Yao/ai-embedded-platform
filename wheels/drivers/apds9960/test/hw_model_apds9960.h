/*
 * hw_model_apds9960.h — APDS-9960 hardware register model from datasheet
 *
 * Models APDS-9960 register behavior:
 * - Chip ID at 0x92: 0xAB (read-only)
 * - Enable register at 0x80 (PON/AEN/PEN/GEN bits)
 * - Control register at 0x8F
 * - Status register at 0x93 (read-only)
 * - RGBC data at 0x94..0x9B (read-only, 16-bit LE per channel: C,R,G,B)
 * - Proximity data at 0x9C (read-only)
 * - Gesture FIFO at 0xFC..0xFF, level at 0xAE
 * - ATIME at 0x81, PPULSE at 0x8E
 * - Gesture config at 0xA0-0xA3, 0xAB
 */

#ifndef HW_MODEL_APDS9960_H
#define HW_MODEL_APDS9960_H

#include "hw_model.h"

/* --- APDS-9960 register addresses --- */
#define APDS_REG_ENABLE   0x80u
#define APDS_REG_ATIME    0x81u
#define APDS_REG_PPULSE   0x8Eu
#define APDS_REG_CONTROL  0x8Fu
#define APDS_REG_ID       0x92u
#define APDS_REG_STATUS   0x93u
#define APDS_REG_CDATAL   0x94u
#define APDS_REG_PDATA    0x9Cu
#define APDS_REG_GPENTH   0xA0u
#define APDS_REG_GEXTH    0xA1u
#define APDS_REG_GCONF1   0xA2u
#define APDS_REG_GCONF2   0xA3u
#define APDS_REG_GCONF4   0xABu
#define APDS_REG_GFLVL    0xAEu
#define APDS_REG_GSTATUS  0xAFu
#define APDS_REG_GFIFO_U  0xFCu

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_c, true_r, true_g, true_b;
    uint8_t         true_prox;
    /* gesture FIFO: up to 4 entries of U/D/L/R */
    uint8_t         gesture_fifo[16];   /* 4 datasets x 4 bytes */
    uint8_t         gesture_fifo_count; /* number of datasets */
    uint8_t         gesture_fifo_idx;   /* next read index */
} apds9960_model_t;

/* Populate RGBC data registers */
static void apds9960_model_set_color_data(apds9960_model_t *m)
{
    m->hw.regs[0x94] = (uint8_t)(m->true_c & 0xFF);
    m->hw.regs[0x95] = (uint8_t)(m->true_c >> 8);
    m->hw.regs[0x96] = (uint8_t)(m->true_r & 0xFF);
    m->hw.regs[0x97] = (uint8_t)(m->true_r >> 8);
    m->hw.regs[0x98] = (uint8_t)(m->true_g & 0xFF);
    m->hw.regs[0x99] = (uint8_t)(m->true_g >> 8);
    m->hw.regs[0x9A] = (uint8_t)(m->true_b & 0xFF);
    m->hw.regs[0x9B] = (uint8_t)(m->true_b >> 8);
    m->hw.regs[APDS_REG_PDATA] = m->true_prox;
}

/* Callback: enable register write — when PON set, populate sensor data */
static void apds9960_model_on_enable(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    apds9960_model_t *m = (apds9960_model_t *)user;
    if (new_val & 0x01u) { /* PON */
        apds9960_model_set_color_data(m);
        /* Set gesture FIFO level */
        m->hw.regs[APDS_REG_GFLVL] = m->gesture_fifo_count;
        m->gesture_fifo_idx = 0;
    }
}

/* Custom read callback for gesture FIFO — reads advance the index */
static bool apds9960_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                          uint8_t reg, uint8_t *buf,
                                          uint16_t len)
{
    (void)dev_addr;
    apds9960_model_t *m = (apds9960_model_t *)ctx;

    /* Gesture FIFO read: 0xFC..0xFF return next FIFO entry */
    if (reg == APDS_REG_GFIFO_U && len == 4 &&
        m->gesture_fifo_idx < m->gesture_fifo_count) {
        unsigned base = (unsigned)m->gesture_fifo_idx * 4u;
        for (uint16_t i = 0; i < 4; i++)
            buf[i] = m->gesture_fifo[base + i];
        m->gesture_fifo_idx++;
        /* Decrement FIFO level */
        if (m->hw.regs[APDS_REG_GFLVL] > 0)
            m->hw.regs[APDS_REG_GFLVL]--;
        return true;
    }

    /* Normal register read */
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < HW_MODEL_REG_COUNT) ? m->hw.regs[idx] : 0xFF;
    }
    return true;
}

static void apds9960_model_init(apds9960_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID: 0xAB at 0x92 (read-only) */
    hw_model_set_reset_value(&m->hw, APDS_REG_ID, 0xABu);
    hw_model_set_readonly(&m->hw, APDS_REG_ID);

    /* Status register (read-only) */
    hw_model_set_readonly(&m->hw, APDS_REG_STATUS);

    /* RGBC data registers (read-only) */
    for (int i = 0; i < 8; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(APDS_REG_CDATAL + i));
    }

    /* Proximity data (read-only) */
    hw_model_set_readonly(&m->hw, APDS_REG_PDATA);

    /* Gesture FIFO level and status (read-only) */
    hw_model_set_readonly(&m->hw, APDS_REG_GFLVL);
    hw_model_set_readonly(&m->hw, APDS_REG_GSTATUS);

    /* Write callback for enable register */
    hw_model_set_on_write(&m->hw, APDS_REG_ENABLE, apds9960_model_on_enable);

    /* Default simulated color/proximity */
    m->true_c = 1200;
    m->true_r = 400;
    m->true_g = 500;
    m->true_b = 300;
    m->true_prox = 128;

    hw_model_reset(&m->hw);
}

/* Helper: load gesture FIFO with one dataset */
static void apds9960_model_set_gesture(apds9960_model_t *m,
                                        uint8_t count,
                                        const uint8_t *data)
{
    m->gesture_fifo_count = count > 4 ? 4 : count;
    memcpy(m->gesture_fifo, data, (unsigned)m->gesture_fifo_count * 4u);
    m->gesture_fifo_idx = 0;
    m->hw.regs[APDS_REG_GFLVL] = m->gesture_fifo_count;
}

#endif /* HW_MODEL_APDS9960_H */
