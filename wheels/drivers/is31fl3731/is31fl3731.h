// SPDX-License-Identifier: MIT
/*
 * is31fl3731 — ISSI IS31FL3731 LED matrix controller driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Controls 144 LEDs (9x16 matrix)
 * across 8 display frames with individual PWM brightness.
 * Picture display mode, frame selection, LED enable/disable, fill,
 * and shutdown control.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef IS31FL3731_H
#define IS31FL3731_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    IS31FL3731_OK        =  0,
    IS31FL3731_ERR_I2C   = -1,
    IS31FL3731_ERR_ID    = -2,
    IS31FL3731_ERR_PARAM = -3,
} is31fl3731_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} is31fl3731_i2c_t;

/* ---------- constants ---------- */

#define IS31FL3731_ADDR_BASE    0x74u   /* A1:A0 = 00 */
#define IS31FL3731_ADDR_MAX     0x77u   /* A1:A0 = 11 */
#define IS31FL3731_NUM_LEDS     144u
#define IS31FL3731_NUM_FRAMES   8u

/* ---------- configuration ---------- */

typedef struct {
    uint8_t addr;    /* 0x74 - 0x77 */
} is31fl3731_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const is31fl3731_i2c_t *i2c;
    is31fl3731_cfg_t        cfg;
    uint8_t                 current_frame;   /* last selected frame page */
} is31fl3731_t;

/* ---------- API ---------- */

/** Initialize: picture mode showing frame 0, every frame set to all LEDs
 *  enabled, no blink and PWM 0, then exit shutdown. This takes 1455 register
 *  writes because frame data is not assured at power-on.
 *  @thread_safe: no @isr_safe: no */
is31fl3731_err_t is31fl3731_init(is31fl3731_t *dev,
                                 const is31fl3731_i2c_t *i2c,
                                 const is31fl3731_cfg_t *cfg);

/** Select which frame page (0-7) subsequent pixel writes target.
 *  @thread_safe: no @isr_safe: no */
is31fl3731_err_t is31fl3731_set_frame(is31fl3731_t *dev, uint8_t frame);

/** Set one LED brightness (0-255) on a frame. led_idx: 0-143.
 *  @thread_safe: no @isr_safe: no */
is31fl3731_err_t is31fl3731_set_pixel(is31fl3731_t *dev, uint8_t frame,
                                      uint8_t led_idx, uint8_t pwm);

/** Enable or disable one LED on a frame. led_idx: 0-143.
 *  @thread_safe: no @isr_safe: no */
is31fl3731_err_t is31fl3731_set_led_enable(is31fl3731_t *dev, uint8_t frame,
                                           uint8_t led_idx, bool enable);

/** Fill all 144 LEDs on a frame with the same brightness.
 *  @thread_safe: no @isr_safe: no */
is31fl3731_err_t is31fl3731_fill(is31fl3731_t *dev, uint8_t frame,
                                 uint8_t pwm);

/** Switch which frame is displayed (writes to function register page).
 *  @thread_safe: no @isr_safe: no */
is31fl3731_err_t is31fl3731_display_frame(is31fl3731_t *dev, uint8_t frame);

/** Enter or exit software shutdown (SSD in the Shutdown Register). Hardware
 *  shutdown is the SDB pin, which this driver does not control.
 *  @thread_safe: no @isr_safe: no */
is31fl3731_err_t is31fl3731_shutdown(is31fl3731_t *dev, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* IS31FL3731_H */
