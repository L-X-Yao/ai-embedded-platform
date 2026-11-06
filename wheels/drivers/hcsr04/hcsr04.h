// SPDX-License-Identifier: MIT
/*
 * hcsr04 -- HC-SR04 ultrasonic distance sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal + timer_hal (injected)
 * Instances: multi
 *
 * GPIO trigger + echo timing. Send 10us HIGH pulse on trigger pin,
 * measure echo pulse width, convert to distance.
 * Range: 2cm - 400cm. Timeout at 25000us (~4.3m).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the hcsr04_t struct.
 */

#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    HCSR04_OK           =  0,
    HCSR04_ERR_PARAM    = -1,
    HCSR04_ERR_TIMEOUT  = -2,
    HCSR04_ERR_RANGE    = -3,
} hcsr04_err_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void *ctx;
    void (*trigger_high)(void *ctx);
    void (*trigger_low)(void *ctx);
    bool (*echo_read)(void *ctx);        /* read echo pin level */
    void (*delay_us)(void *ctx, uint32_t us);
    uint32_t (*get_time_us)(void *ctx);  /* microsecond timestamp */
} hcsr04_hal_t;

/* ---------- device context ---------- */

typedef struct {
    const hcsr04_hal_t *hal;
} hcsr04_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
hcsr04_err_t hcsr04_init(hcsr04_t *dev, const hcsr04_hal_t *hal);

/** Trigger measurement and return distance in centimeters.
 *  Start measurements at least 60 ms apart. A call that finds echo still high
 *  from the last pulse waits for it to end, and returns HCSR04_ERR_TIMEOUT
 *  without triggering if it does not end within 25 ms.
 *  @thread_safe: no @isr_safe: no */
hcsr04_err_t hcsr04_read_cm(hcsr04_t *dev, float *distance_cm);

/** Trigger measurement and return raw echo pulse width in microseconds.
 *  The same cycle rule as hcsr04_read_cm applies.
 *  @thread_safe: no @isr_safe: no */
hcsr04_err_t hcsr04_read_us(hcsr04_t *dev, uint32_t *echo_us);

#ifdef __cplusplus
}
#endif

#endif /* HCSR04_H */
