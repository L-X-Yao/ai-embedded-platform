// SPDX-License-Identifier: MIT
/*
 * hw_model_pcf8574.h -- PCF8574 I/O expander hardware model
 *
 * Models NXP PCF8574 behavior:
 * - No register map: single-byte write sets output latches, read returns pin state
 * - Quasi-bidirectional I/O: write 1 to make pin input, pin reads back external state
 * - Output state and external pin state are independent
 */

#ifndef HW_MODEL_PCF8574_H
#define HW_MODEL_PCF8574_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t output_latch;    /* what was last written */
    uint8_t external_pins;   /* simulated external pin state */
    int     write_count;
    int     read_count;
} pcf8574_model_t;

static void pcf8574_model_init(pcf8574_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->output_latch = 0xFF;  /* POR default: all high */
    m->external_pins = 0xFF; /* no external pull-down by default */
}

/*
 * PCF8574 read behavior: returns logical AND of output latch and external pins.
 * If output is 1 (quasi-input), external state determines the read value.
 * If output is 0 (strong pull-down), read always returns 0 for that pin.
 */
static uint8_t pcf8574_model_read_pins(const pcf8574_model_t *m)
{
    return m->output_latch & m->external_pins;
}

/* --- I2C HAL callbacks (command-based, no register address) --- */

__attribute__((unused))
static bool pcf8574_model_i2c_write(void *ctx, uint8_t dev_addr,
                                    const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    pcf8574_model_t *m = (pcf8574_model_t *)ctx;
    if (len >= 1) {
        m->output_latch = data[0];
        m->write_count++;
    }
    return true;
}

__attribute__((unused))
static bool pcf8574_model_i2c_read(void *ctx, uint8_t dev_addr,
                                   uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    pcf8574_model_t *m = (pcf8574_model_t *)ctx;
    if (len >= 1) {
        data[0] = pcf8574_model_read_pins(m);
        m->read_count++;
    }
    return true;
}

/* Helper: set external pin state (simulates external pull-down/up) */
static void pcf8574_model_set_external(pcf8574_model_t *m, uint8_t pins)
{
    m->external_pins = pins;
}

#endif /* HW_MODEL_PCF8574_H */
