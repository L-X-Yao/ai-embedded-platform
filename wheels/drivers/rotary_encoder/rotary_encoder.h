// SPDX-License-Identifier: MIT
/*
 * rotary_encoder — quadrature rotary encoder decoder
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Software quadrature decoder. Poll or feed A/B pin states on each edge.
 * Tracks position (int32_t) and direction. Pure C99, no heap.
 * Thread safety: caller owns the rotary_encoder_t struct.
 */

#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    bool (*read_a)(void *ctx);   /**< Read pin A level (true = high). */
    bool (*read_b)(void *ctx);   /**< Read pin B level (true = high). */
    void *ctx;
} rotary_encoder_hal_t;

/* ---------- direction enum ---------- */

typedef enum {
    ROTARY_ENCODER_DIR_NONE = 0,
    ROTARY_ENCODER_DIR_CW   = 1,
    ROTARY_ENCODER_DIR_CCW  = -1,
} rotary_encoder_dir_t;

/* ---------- instance state ---------- */

typedef struct {
    const rotary_encoder_hal_t *hal;
    int32_t          position;
    rotary_encoder_dir_t direction;
    uint8_t          last_state;
} rotary_encoder_t;

/* ---------- API ---------- */

/** Initialize encoder with HAL callbacks. @thread_safe: no @isr_safe: no */
void rotary_encoder_init(rotary_encoder_t *enc,
                         const rotary_encoder_hal_t *hal);

/** Feed current A/B pin states to update position and direction. @thread_safe: no @isr_safe: yes */
void rotary_encoder_update(rotary_encoder_t *enc, bool a, bool b);

/** Poll HAL pins and update state. @thread_safe: no @isr_safe: no */
void rotary_encoder_poll(rotary_encoder_t *enc);

/** Return accumulated position count. @thread_safe: no @isr_safe: yes */
int32_t rotary_encoder_get_position(const rotary_encoder_t *enc);

/** Return last detected rotation direction. @thread_safe: no @isr_safe: yes */
rotary_encoder_dir_t rotary_encoder_get_direction(const rotary_encoder_t *enc);

/** Reset position to zero and clear direction. @thread_safe: no @isr_safe: no */
void rotary_encoder_reset(rotary_encoder_t *enc);

#ifdef __cplusplus
}
#endif

#endif /* ROTARY_ENCODER_H */
