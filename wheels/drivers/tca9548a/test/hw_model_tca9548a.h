/*
 * hw_model_tca9548a.h — TCA9548A hardware register model
 *
 * Models TI TCA9548A I2C multiplexer:
 * - Single control register (byte write to device address)
 * - Each bit enables one of 8 downstream channels
 * - POR state: all channels disabled (0x00)
 * - No other registers exist; the chip treats any written byte
 *   as the control value.
 *
 * We model this using hw_model with register 0x00 as the single
 * control register (matching the driver's dummy reg=0x00 convention).
 */

#ifndef HW_MODEL_TCA9548A_H
#define HW_MODEL_TCA9548A_H

#include "hw_model.h"

#define TCA9548A_REG_CTRL  0x00

typedef struct {
    hw_reg_model_t  hw;
    int             write_count;   /* total writes to control register */
    uint8_t         last_mask;     /* last channel mask written */
} tca9548a_model_t;

/* Callback: track channel selection writes */
static void tca9548a_model_on_ctrl(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    tca9548a_model_t *m = (tca9548a_model_t *)user;
    m->write_count++;
    m->last_mask = new_val;
}

static void tca9548a_model_init(tca9548a_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR: all channels disabled */
    hw_model_set_reset_value(&m->hw, TCA9548A_REG_CTRL, 0x00);

    /* All non-existent registers: read-only 0xFF
     * (only reg 0x00 is the actual control register) */
    for (int i = 1; i < HW_MODEL_REG_COUNT; i++) {
        hw_model_set_reset_value(&m->hw, (uint8_t)i, 0xFF);
        hw_model_set_readonly(&m->hw, (uint8_t)i);
    }

    /* Write callback for control register */
    hw_model_set_on_write(&m->hw, TCA9548A_REG_CTRL, tca9548a_model_on_ctrl);

    hw_model_reset(&m->hw);
}

/* HAL delay stub */
__attribute__((unused))
static void tca9548a_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_TCA9548A_H */
