/*
 * rotary_encoder — quadrature rotary encoder decoder
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    (none)
 *
 * Software quadrature decoder. Feed A/B pin states on each edge interrupt.
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

typedef enum {
    ENCODER_DIR_NONE = 0,
    ENCODER_DIR_CW   = 1,
    ENCODER_DIR_CCW  = -1,
} encoder_dir_t;

typedef struct {
    int32_t      position;
    encoder_dir_t direction;
    uint8_t      last_state;
} rotary_encoder_t;

/** @thread_safe: no */
void encoder_init(rotary_encoder_t *enc);

void encoder_update(rotary_encoder_t *enc, bool a, bool b);

int32_t encoder_get_position(const rotary_encoder_t *enc);

encoder_dir_t encoder_get_direction(const rotary_encoder_t *enc);

void encoder_reset(rotary_encoder_t *enc);

#ifdef __cplusplus
}
#endif

#endif /* ROTARY_ENCODER_H */
