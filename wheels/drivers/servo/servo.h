/*
 * servo — PWM servo motor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    pwm_hal (injected)
 *
 * Maps angle (0-180°) to pulse width (typically 500-2500µs).
 * Caller provides PWM set-pulse-width callback.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SERVO_OK         =  0,
    SERVO_ERR_PARAM  = -1,
    SERVO_ERR_HAL    = -2,
} servo_err_t;

typedef struct {
    bool (*set_pulse_us)(void *ctx, uint32_t pulse_us);
    void *ctx;
} servo_pwm_t;

typedef struct {
    uint16_t min_pulse_us;
    uint16_t max_pulse_us;
    uint16_t max_angle;
} servo_cfg_t;

typedef struct {
    servo_pwm_t pwm;
    servo_cfg_t cfg;
    uint16_t    current_angle;
} servo_t;

servo_err_t servo_init(servo_t *dev, const servo_pwm_t *pwm,
                       const servo_cfg_t *cfg);

servo_err_t servo_set_angle(servo_t *dev, uint16_t angle);

servo_err_t servo_set_pulse(servo_t *dev, uint32_t pulse_us);

uint16_t servo_get_angle(const servo_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_H */
