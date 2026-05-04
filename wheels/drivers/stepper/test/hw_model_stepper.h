// SPDX-License-Identifier: MIT
/*
 * hw_model_stepper.h — Signal decoder model for step/dir stepper driver
 *
 * NOT a register model. The stepper driver outputs step pulses and
 * direction signals via HAL callbacks. This model captures the pulse
 * sequence and tracks net steps, direction changes, and step timing.
 */

#ifndef HW_MODEL_STEPPER_H
#define HW_MODEL_STEPPER_H

#include "stepper.h"
#include <string.h>

#define STEPPER_MODEL_MAX_EVENTS  2048

typedef enum {
    STEPPER_MODEL_DIR_CW  = 0,
    STEPPER_MODEL_DIR_CCW = 1,
} stepper_model_dir_t;

typedef struct {
    int32_t  step_delta;   /* +1 CW, -1 CCW */
    uint32_t delay_us;     /* delay after this step (0 if last) */
} stepper_model_event_t;

typedef struct {
    /* decoded state */
    int32_t                net_steps;
    uint32_t               total_steps;
    stepper_model_dir_t    current_dir;

    /* raw capture */
    stepper_model_event_t  events[STEPPER_MODEL_MAX_EVENTS];
    uint32_t               event_count;

    /* internal tracking */
    bool     dir_pin_level;     /* false=CW, true=CCW (matches driver) */
    bool     last_step_level;
    uint32_t pending_delay_us;
} stepper_model_t;

static void stepper_model_init(stepper_model_t *m)
{
    memset(m, 0, sizeof(*m));
}

/* --- HAL callbacks wired to model --- */

static void stepper_model_step_pin(void *ctx, bool level)
{
    stepper_model_t *m = (stepper_model_t *)ctx;
    /* detect rising edge = one step */
    if (level && !m->last_step_level) {
        int32_t delta = m->dir_pin_level ? -1 : +1;
        m->net_steps += delta;
        m->total_steps++;

        if (m->event_count < STEPPER_MODEL_MAX_EVENTS) {
            m->events[m->event_count].step_delta = delta;
            m->events[m->event_count].delay_us   = 0; /* filled on next delay */
            m->event_count++;
        }
    }
    m->last_step_level = level;
}

static void stepper_model_dir_pin(void *ctx, bool level)
{
    stepper_model_t *m = (stepper_model_t *)ctx;
    m->dir_pin_level = level;
    m->current_dir = level ? STEPPER_MODEL_DIR_CCW : STEPPER_MODEL_DIR_CW;
}

static void stepper_model_delay_us(void *ctx, uint32_t us)
{
    stepper_model_t *m = (stepper_model_t *)ctx;
    /* 2us delays are internal pulse widths, skip them for ramp analysis */
    if (us <= 2) return;
    m->pending_delay_us = us;
    /* attach delay to the most recent step event */
    if (m->event_count > 0) {
        m->events[m->event_count - 1].delay_us = us;
    }
}

static stepper_hal_t stepper_model_hal(stepper_model_t *m)
{
    stepper_hal_t hal;
    hal.step_pin  = stepper_model_step_pin;
    hal.dir_pin   = stepper_model_dir_pin;
    hal.delay_us  = stepper_model_delay_us;
    hal.ctx       = m;
    return hal;
}

/* --- Query API --- */

static int32_t stepper_model_get_steps(const stepper_model_t *m)
{
    return m->net_steps;
}

static uint32_t stepper_model_get_total_steps(const stepper_model_t *m)
{
    return m->total_steps;
}

static stepper_model_dir_t stepper_model_get_direction(const stepper_model_t *m)
{
    return m->current_dir;
}

static uint32_t stepper_model_get_event_count(const stepper_model_t *m)
{
    return m->event_count;
}

/*
 * Check if a ramp segment has monotonically decreasing delays (accel).
 * Returns 1 if delays in [start, end) are strictly non-increasing.
 */
static int stepper_model_delays_decreasing(const stepper_model_t *m,
                                            uint32_t start, uint32_t end)
{
    if (end > m->event_count) end = m->event_count;
    for (uint32_t i = start + 1; i < end; i++) {
        if (m->events[i].delay_us > m->events[i - 1].delay_us) {
            return 0;
        }
    }
    return 1;
}

/*
 * Check if a ramp segment has monotonically increasing delays (decel).
 * Returns 1 if delays in [start, end) are strictly non-decreasing.
 */
static int stepper_model_delays_increasing(const stepper_model_t *m,
                                            uint32_t start, uint32_t end)
{
    if (end > m->event_count) end = m->event_count;
    for (uint32_t i = start + 1; i < end; i++) {
        if (m->events[i].delay_us < m->events[i - 1].delay_us) {
            return 0;
        }
    }
    return 1;
}

#endif /* HW_MODEL_STEPPER_H */
