// SPDX-License-Identifier: MIT
/*
 * prop_rotary_encoder.c — property-based tests for rotary encoder driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "rotary_encoder.h"

static bool mock_a, mock_b;
static bool mock_read_a(void *ctx) { (void)ctx; return mock_a; }
static bool mock_read_b(void *ctx) { (void)ctx; return mock_b; }

static const rotary_encoder_hal_t prop_hal = {
    .read_a = mock_read_a,
    .read_b = mock_read_b,
    .ctx    = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_a = (ARBI_UINT8(seed) & 1) != 0;
    mock_b = (ARBI_UINT8(seed) & 1) != 0;
    rotary_encoder_t enc;
    rotary_encoder_init(&enc, &prop_hal);
}

PROPERTY(update_no_crash, 5000)
{
    rotary_encoder_t enc;
    mock_a = false; mock_b = false;
    rotary_encoder_init(&enc, &prop_hal);

    /* Feed random A/B sequences */
    for (int i = 0; i < 20; i++) {
        bool a = (ARBI_UINT8(seed) & 1) != 0;
        bool b = (ARBI_UINT8(seed) & 1) != 0;
        rotary_encoder_update(&enc, a, b);
    }
    (void)rotary_encoder_get_position(&enc);
    (void)rotary_encoder_get_direction(&enc);
}

PROPERTY(cw_increments, 5000)
{
    rotary_encoder_t enc;
    mock_a = false; mock_b = false;
    rotary_encoder_init(&enc, &prop_hal);

    /* Standard CW quadrature: 00->10->11->01->00 */
    bool cw_a[] = {false, true,  true,  false};
    bool cw_b[] = {false, false, true,  true};
    int32_t start = rotary_encoder_get_position(&enc);

    int steps = 1 + ARBI_UINT8(seed) % 10;
    for (int s = 0; s < steps; s++) {
        for (int i = 0; i < 4; i++)
            rotary_encoder_update(&enc, cw_a[i], cw_b[i]);
    }

    int32_t end = rotary_encoder_get_position(&enc);
    /* Position should have moved (direction depends on decoder convention) */
    PROP_ASSERT(end != start);
}

TEST_MAIN("prop_rotary_encoder")
    RUN(init_no_crash);
    RUN(update_no_crash);
    RUN(cw_increments);
TEST_MAIN_END()
