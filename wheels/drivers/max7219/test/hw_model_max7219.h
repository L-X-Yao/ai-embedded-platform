// SPDX-License-Identifier: MIT
/*
 * hw_model_max7219.h — MAX7219 hardware register model
 *
 * Models Maxim MAX7219 SPI LED driver register behavior:
 * - NOOP at 0x00: writes are ignored
 * - DIGIT0..DIGIT7 at 0x01..0x08: digit/row data
 * - DECODE_MODE at 0x09: BCD decode bitmask
 * - INTENSITY at 0x0A: brightness (0x00-0x0F)
 * - SCAN_LIMIT at 0x0B: number of digits - 1 (0x00-0x07)
 * - SHUTDOWN at 0x0C: bit 0 = 0 (shutdown), 1 (normal)
 * - DISPLAY_TEST at 0x0F: bit 0 = 1 (all-on test mode)
 *
 * SPI protocol: 16-bit shift (D11..D8 = register, D7..D0 = data, D15..D12
 * don't care). A transfer only fills the shift register, "regardless of the
 * state of LOAD" (p.6). The register is written on LOAD's rising edge, in
 * max7219_model_cs_high().
 */

#ifndef HW_MODEL_MAX7219_H
#define HW_MODEL_MAX7219_H

#include "hw_model.h"

/* --- Register addresses --- */
#define MAX7219_REG_NOOP         0x00
#define MAX7219_REG_DIGIT0       0x01
#define MAX7219_REG_DIGIT7       0x08
#define MAX7219_REG_DECODE_MODE  0x09
#define MAX7219_REG_INTENSITY    0x0A
#define MAX7219_REG_SCAN_LIMIT   0x0B
#define MAX7219_REG_SHUTDOWN     0x0C
#define MAX7219_REG_DISPLAY_TEST 0x0F

typedef struct {
    hw_reg_model_t  hw;
    bool            shutdown;       /* true = display off */
    bool            display_test;   /* true = all segments on */
    int             write_count;    /* total SPI writes */
    uint8_t         shift[2];       /* 16-bit shift register, D15 first */
    bool            load_high;      /* level of the LOAD pin */
} max7219_model_t;

/* Callback: SHUTDOWN register */
static void max7219_model_on_shutdown(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    max7219_model_t *m = (max7219_model_t *)user;
    m->shutdown = (new_val & 0x01) == 0;
}

/* Callback: DISPLAY_TEST register */
static void max7219_model_on_display_test(void *user, uint8_t reg,
                                          uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    max7219_model_t *m = (max7219_model_t *)user;
    m->display_test = (new_val & 0x01) != 0;
}

/* Callback: INTENSITY — only low nibble is valid */
static void max7219_model_on_intensity(void *user, uint8_t reg,
                                       uint8_t old_val, uint8_t new_val)
{
    (void)user; (void)reg; (void)old_val; (void)new_val;
    /* Intensity value is simply stored; HW uses only bits [3:0] */
}

static void max7219_model_init(max7219_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults from datasheet (Table 1):
     * - SHUTDOWN: 0x00 (shutdown mode active on power-up)
     * - DECODE_MODE: 0x00 (no decode)
     * - INTENSITY: 0x00 (minimum)
     * - SCAN_LIMIT: 0x00 (display digit 0 only)
     * - DISPLAY_TEST: 0x00 (normal operation)
     * - All digit registers: undefined (we use 0x00)
     */
    hw_model_set_reset_value(&m->hw, MAX7219_REG_SHUTDOWN, 0x00);
    hw_model_set_reset_value(&m->hw, MAX7219_REG_DECODE_MODE, 0x00);
    hw_model_set_reset_value(&m->hw, MAX7219_REG_INTENSITY, 0x00);
    hw_model_set_reset_value(&m->hw, MAX7219_REG_SCAN_LIMIT, 0x00);
    hw_model_set_reset_value(&m->hw, MAX7219_REG_DISPLAY_TEST, 0x00);

    /* INTENSITY: only low nibble matters (mask bits [3:0]) */
    hw_model_set_writable_mask(&m->hw, MAX7219_REG_INTENSITY, 0x0F);

    /* SCAN_LIMIT: only bits [2:0] are valid (0-7) */
    hw_model_set_writable_mask(&m->hw, MAX7219_REG_SCAN_LIMIT, 0x07);

    /* SHUTDOWN: only bit 0 */
    hw_model_set_writable_mask(&m->hw, MAX7219_REG_SHUTDOWN, 0x01);

    /* DISPLAY_TEST: only bit 0 */
    hw_model_set_writable_mask(&m->hw, MAX7219_REG_DISPLAY_TEST, 0x01);

    /* NOOP register: read-only (writes do nothing) */
    hw_model_set_readonly(&m->hw, MAX7219_REG_NOOP);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, MAX7219_REG_SHUTDOWN, max7219_model_on_shutdown);
    hw_model_set_on_write(&m->hw, MAX7219_REG_DISPLAY_TEST, max7219_model_on_display_test);
    hw_model_set_on_write(&m->hw, MAX7219_REG_INTENSITY, max7219_model_on_intensity);

    m->shutdown = true;  /* POR: shutdown mode */
    m->display_test = false;
    m->write_count = 0;
    m->load_high = true;

    hw_model_reset(&m->hw);
}

/* --- SPI HAL shim: converts SPI transfers into register model writes --- */

/* The MAX7219 SPI protocol sends 16 bits: [11:8]=register, [7:0]=data.
 * We intercept the transfer call and keep the last 16 bits. */
__attribute__((unused))
static bool max7219_model_spi_transfer(void *ctx, const uint8_t *tx,
                                       uint8_t *rx, uint16_t len)
{
    (void)rx;
    max7219_model_t *m = (max7219_model_t *)ctx;
    if (len != 2 || !tx) return false;

    m->shift[0] = tx[0];
    m->shift[1] = tx[1];
    m->write_count++;
    return true;
}

/* p.5: "The last 16 bits of serial data are latched on LOAD's rising edge." */
static void max7219_model_latch(max7219_model_t *m)
{
    uint8_t reg = m->shift[0] & 0x0Fu;  /* D12-D15 are don't care */
    uint8_t val = m->shift[1];

    /* NOOP: ignore */
    if (reg == MAX7219_REG_NOOP) return;

    /* Valid register range: 0x01-0x0C, 0x0F */
    if (reg >= 0x01 && reg <= 0x0C) {
        hw_model__write_reg(&m->hw, reg, val);
    } else if (reg == MAX7219_REG_DISPLAY_TEST) {
        hw_model__write_reg(&m->hw, reg, val);
    }
    /* All other addresses are ignored by the chip */
}

__attribute__((unused))
static void max7219_model_cs_low(void *ctx)
{
    ((max7219_model_t *)ctx)->load_high = false;
}

__attribute__((unused))
static void max7219_model_cs_high(void *ctx)
{
    max7219_model_t *m = (max7219_model_t *)ctx;

    if (!m->load_high) max7219_model_latch(m);
    m->load_high = true;
}

#endif /* HW_MODEL_MAX7219_H */
