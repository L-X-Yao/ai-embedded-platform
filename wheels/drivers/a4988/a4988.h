// SPDX-License-Identifier: MIT
/*
 * a4988 — Allegro A4988 stepper motor driver (GPIO)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Step/Dir interface with microstepping control (MS1/MS2/MS3).
 * ENABLE pin: LOW=enabled, HIGH=disabled.
 * Step pulse: minimum 1us HIGH then LOW.
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

/* ---------- microstepping mode ---------- */

typedef enum {
    A4988_STEP_FULL      = 0,   /* MS1=0, MS2=0, MS3=0 */
    A4988_STEP_HALF      = 1,   /* MS1=1, MS2=0, MS3=0 */
    A4988_STEP_QUARTER   = 2,   /* MS1=0, MS2=1, MS3=0 */
    A4988_STEP_EIGHTH    = 3,   /* MS1=1, MS2=1, MS3=0 */
    A4988_STEP_SIXTEENTH = 4,   /* MS1=1, MS2=1, MS3=1 */
} a4988_microstep_t;

/* ---------- pin identifiers in config ---------- */

typedef struct {
    uint8_t step;
    uint8_t dir;
    uint8_t enable;
    uint8_t ms1;
    uint8_t ms2;
    uint8_t ms3;
} a4988_pins_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*write_pin)(void *ctx, uint8_t pin_id, bool level);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} a4988_hal_t;

/* ---------- device handle ---------- */

typedef struct {
    const a4988_hal_t *hal;
    a4988_pins_t       pins;
    bool               enabled;
    a4988_dir_t        direction;
    a4988_microstep_t  microstep;
} a4988_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- @isr_safe: no */

/** Initialize driver, set ENABLE HIGH (disabled), STEP LOW, configure full-step. */
a4988_err_t a4988_init(a4988_t *dev, const a4988_hal_t *hal,
                       const a4988_pins_t *pins);

/** Enable/disable motor driver. enable=true -> ENABLE LOW (active). */
a4988_err_t a4988_enable(a4988_t *dev, bool enable);

/** Set rotation direction. */
a4988_err_t a4988_set_direction(a4988_t *dev, a4988_dir_t dir);

/** Execute one step pulse (1us HIGH then LOW). */
a4988_err_t a4988_step(a4988_t *dev);

/** Execute N steps with delay_us between each. Blocking. */
a4988_err_t a4988_step_n(a4988_t *dev, uint32_t steps,
                         uint32_t delay_us_between);

/** Set microstepping mode via MS1/MS2/MS3 pins. */
a4988_err_t a4988_set_microstep(a4988_t *dev, a4988_microstep_t mode);

#ifdef __cplusplus
}
#endif

#endif /* A4988_H */
