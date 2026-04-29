/*
 * rotary_encoder — quadrature rotary encoder decoder
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    (none)
 */
#include "rotary_encoder.h"

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

void encoder_init(rotary_encoder_t *enc)
{
    if (!enc) return;
    enc->position = 0;
    enc->direction = ENCODER_DIR_NONE;
    enc->last_state = 0;
}

void encoder_update(rotary_encoder_t *enc, bool a, bool b)
{
    if (!enc) return;
    uint8_t new_state = (uint8_t)((a ? 2 : 0) | (b ? 1 : 0));
    uint8_t idx = (uint8_t)((enc->last_state << 2) | new_state);
    int8_t delta = quad_table[idx];
    if (delta != 0) {
        enc->position += delta;
        enc->direction = (delta > 0) ? ENCODER_DIR_CW : ENCODER_DIR_CCW;
    }
    enc->last_state = new_state;
}

int32_t encoder_get_position(const rotary_encoder_t *enc)
{
    if (!enc) return 0;
    return enc->position;
}

encoder_dir_t encoder_get_direction(const rotary_encoder_t *enc)
{
    if (!enc) return ENCODER_DIR_NONE;
    return enc->direction;
}

void encoder_reset(rotary_encoder_t *enc)
{
    if (!enc) return;
    enc->position = 0;
    enc->direction = ENCODER_DIR_NONE;
}
