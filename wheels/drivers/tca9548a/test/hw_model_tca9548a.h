// SPDX-License-Identifier: MIT
/*
 * hw_model_tca9548a.h — TCA9548A bus model
 *
 * Models TI TCA9548A I2C multiplexer, from SCPS207H, September 2024:
 * - One control byte and no register numbers: the controller "merely
 *   writes the register data after the target address, skipping the
 *   register number" (p16)
 * - Every byte written after the address goes into the control byte and
 *   the last one stays (p16, p17), so a register byte sent ahead of a
 *   read overwrites it too
 * - A read is the address with R/W=1, and the chip answers at once with
 *   the control byte (p16, Figure 7-5)
 * - Each bit enables one of 8 downstream channels
 * - POR state: all channels disabled (0x00) (p14, p18)
 * - Only the strapped address gets an ACK (p15)
 *
 * The callbacks work at bus level, so tests see the exact bytes.
 * hw_reg_model_t is kept for its failure injection only.
 * Not modelled: a channel turning on at the STOP (p17).
 */

#ifndef HW_MODEL_TCA9548A_H
#define HW_MODEL_TCA9548A_H

#include "hw_model.h"

#define TCA9548A_MODEL_LOG_MAX 8

typedef struct {
    hw_reg_model_t  hw;
    uint8_t         addr;          /* strapped address */
    uint8_t         ctrl;          /* the control byte */
    int             write_count;   /* write transactions acknowledged */
    int             read_count;    /* read transactions acknowledged */
    uint16_t        last_write_len;                  /* bytes after addr */
    uint8_t         last_write[TCA9548A_MODEL_LOG_MAX];
} tca9548a_model_t;

static void tca9548a_model_init(tca9548a_model_t *m, uint8_t addr)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->addr = addr;
}

/* START, address with R/W=0, data bytes, STOP */
__attribute__((unused))
static bool tca9548a_model_write_raw(void *ctx, uint8_t dev_addr,
                                     const uint8_t *data, uint16_t len)
{
    tca9548a_model_t *m = (tca9548a_model_t *)ctx;
    if (hw_model__should_fail(&m->hw)) return false;
    if (dev_addr != m->addr) return false;

    m->write_count++;
    m->last_write_len = len;
    for (uint16_t i = 0; i < len; i++) {
        if (i < TCA9548A_MODEL_LOG_MAX) m->last_write[i] = data[i];
        m->ctrl = data[i];
    }
    return true;
}

/* START, address with R/W=1, data bytes, STOP */
__attribute__((unused))
static bool tca9548a_model_read_raw(void *ctx, uint8_t dev_addr,
                                    uint8_t *data, uint16_t len)
{
    tca9548a_model_t *m = (tca9548a_model_t *)ctx;
    if (hw_model__should_fail(&m->hw)) return false;
    if (dev_addr != m->addr) return false;

    m->read_count++;
    for (uint16_t i = 0; i < len; i++) data[i] = m->ctrl;
    return true;
}

/* HAL delay stub */
__attribute__((unused))
static void tca9548a_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_TCA9548A_H */
