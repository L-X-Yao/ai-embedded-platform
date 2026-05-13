// SPDX-License-Identifier: MIT
/*
 * ht16k33 — HT16K33 I2C LED matrix/segment controller driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Holtek HT16K33: 16x8 LED matrix driver with I2C interface.
 * 16-byte display RAM, 16-level PWM brightness, command-based control.
 * Supports up to 8 devices on one bus (addr 0x70-0x77 via A0-A2 pins).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef HT16K33_H
#define HT16K33_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    HT16K33_OK        =  0,
    HT16K33_ERR_I2C   = -1,
    HT16K33_ERR_PARAM = -2,
} ht16k33_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ht16k33_i2c_t;

/* ---------- constants ---------- */

#define HT16K33_DEV_ADDR       0x70u
#define HT16K33_DISPLAY_RAM_SIZE  16u
#define HT16K33_BRIGHTNESS_MAX    15u

/* ---------- configuration ---------- */

typedef struct {
    uint8_t addr;         /* I2C address: 0x70-0x77 */
    uint8_t brightness;   /* 0-15 duty cycle */
} ht16k33_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const ht16k33_i2c_t *i2c;
    uint8_t addr;
} ht16k33_t;

/* ---------- API ---------- */

/** Initialize: oscillator on, display on, set brightness.
 *  @thread_safe: no @isr_safe: no */
ht16k33_err_t ht16k33_init(ht16k33_t *dev,
                            const ht16k33_i2c_t *i2c,
                            const ht16k33_cfg_t *cfg);

/** Set brightness (0-15 duty cycle).
 *  @thread_safe: no @isr_safe: no */
ht16k33_err_t ht16k33_set_brightness(ht16k33_t *dev, uint8_t level);

/** Write raw display RAM (up to 16 bytes starting at offset 0).
 *  @thread_safe: no @isr_safe: no */
ht16k33_err_t ht16k33_write_raw(ht16k33_t *dev,
                                const uint8_t *buf, uint8_t len);

/** Clear display RAM (zero all 16 bytes).
 *  @thread_safe: no @isr_safe: no */
ht16k33_err_t ht16k33_clear(ht16k33_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* HT16K33_H */
