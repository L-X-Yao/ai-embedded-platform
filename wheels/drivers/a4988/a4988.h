// SPDX-License-Identifier: MIT
/*
 * a4988 — Allegro A4988 stepper motor driver (GPIO)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Step/Dir interface: rising edge on STEP = one microstep, DIR = direction.
 * Microstepping via MS1/MS2/MS3. ENABLE is active-LOW.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the a4988_t struct.
 */

#ifndef A4988_H
#define A4988_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    A4988_OK        =  0,
    A4988_ERR_PARAM = -1,
} a4988_err_t;

/* ---------- direction ---------- */

typedef enum {
    A4988_DIR_CW  = 0,
    A4988_DIR_CCW = 1,
} a4988_dir_t;

/* ---------- microstep mode ---------- */

typedef enum {
    A4988_STEP_FULL     = 0,
    A4988_STEP_HALF     = 1,
    A4988_STEP_QUARTER  = 2,
    A4988_STEP_EIGHTH   = 3,
    A4988_STEP_SIXTEENTH = 4,
} a4988_microstep_t;

/* ---------- pin IDs ---------- */

typedef struct {
    uint8_t step;
    uint8_t dir;
    uint8_t enable;
    uint8_t ms1;
    uint8_t ms2;
    uint8_t ms3;
} a4988_cfg_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*write_pin)(void *ctx, uint8_t pin_id, bool level);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} a4988_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const a4988_hal_t *hal;
    a4988_cfg_t cfg;
    a4988_dir_t direction;
    a4988_microstep_t microstep;
    bool enabled;
} a4988_t;

/* ---------- API (all @thread_safe: caller_must_lock) @isr_safe: no ---------- */

/** Initialize driver. Motor disabled, direction CW, full-step mode. */
a4988_err_t a4988_init(a4988_t *dev, const a4988_hal_t *hal,
                       const a4988_cfg_t *cfg);

/** Enable or disable motor driver (ENABLE pin active-LOW). */
a4988_err_t a4988_enable(a4988_t *dev, bool on);

/** Set rotation direction. */
a4988_err_t a4988_set_direction(a4988_t *dev, a4988_dir_t dir);

/** Single step pulse (2us HIGH, then LOW). */
a4988_err_t a4988_step(a4988_t *dev);

/** Step N times with delay_us between steps. */
a4988_err_t a4988_step_n(a4988_t *dev, uint32_t count, uint32_t delay_us_between);

/** Set microstepping mode via MS1/MS2/MS3 pins. */
a4988_err_t a4988_set_microstep(a4988_t *dev, a4988_microstep_t mode);

#ifdef __cplusplus
}
#endif

#endif /* A4988_H */
