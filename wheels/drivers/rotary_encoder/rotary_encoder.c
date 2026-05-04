// SPDX-License-Identifier: MIT
/*
 * rotary_encoder — quadrature rotary encoder decoder
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */
#include "rotary_encoder.h"
#include <stddef.h>

/* Quadrature state transition table:
 * old_state | new_state | direction
 * 00 → 01 = CW,  00 → 10 = CCW
 * 01 → 11 = CW,  01 → 00 = CCW
 * 11 → 10 = CW,  11 → 01 = CCW
 * 10 → 00 = CW,  10 → 11 = CCW
 */
static const int8_t quad_table[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

void rotary_encoder_init(rotary_encoder_t *enc,
                         const rotary_encoder_hal_t *hal)
{
    if (!enc) return;
    enc->hal = hal;
    enc->position = 0;
    enc->direction = ROTARY_ENCODER_DIR_NONE;
    enc->last_state = 0;
}

void rotary_encoder_update(rotary_encoder_t *enc, bool a, bool b)
{
    if (!enc) return;
    uint8_t new_state = (uint8_t)((a ? 2 : 0) | (b ? 1 : 0));
    uint8_t idx = (uint8_t)((enc->last_state << 2) | new_state);
    int8_t delta = quad_table[idx];
    if (delta != 0) {
        enc->position += delta;
        enc->direction = (delta > 0) ? ROTARY_ENCODER_DIR_CW : ROTARY_ENCODER_DIR_CCW;
    }
    enc->last_state = new_state;
}

void rotary_encoder_poll(rotary_encoder_t *enc)
{
    if (!enc || !enc->hal) return;
    bool a = enc->hal->read_a(enc->hal->ctx);
    bool b = enc->hal->read_b(enc->hal->ctx);
    (void)rotary_encoder_update(enc, a, b);
}

int32_t rotary_encoder_get_position(const rotary_encoder_t *enc)
{
    if (!enc) return 0;
    return enc->position;
}

rotary_encoder_dir_t rotary_encoder_get_direction(const rotary_encoder_t *enc)
{
    if (!enc) return ROTARY_ENCODER_DIR_NONE;
    return enc->direction;
}

void rotary_encoder_reset(rotary_encoder_t *enc)
{
    if (!enc) return;
    enc->position = 0;
    enc->direction = ROTARY_ENCODER_DIR_NONE;
}
