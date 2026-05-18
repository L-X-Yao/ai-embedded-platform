// SPDX-License-Identifier: MIT
/*
 * prop_button_handler.c — property-based tests for button_handler
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_button_handler.c ../button_handler.c -o prop_button_handler
 */

#include "test_harness.h"
#include "property_test.h"
#include "../button_handler.h"
#include <string.h>

static bool mock_read_state;
static bool mock_read(void *ctx) {
    (void)ctx;
    return mock_read_state;
}

static int callback_count;
static void mock_callback(void *ctx, uint8_t id, btn_event_t event) {
    (void)ctx; (void)id; (void)event;
    callback_count++;
}

/* Property: random register + tick sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    btn_handler_t bh;
    callback_count = 0;
    btn_handler_init(&bh, mock_callback, NULL,
                     (uint16_t)ARBI_RANGE(seed, 1, 10),
                     (uint16_t)ARBI_RANGE(seed, 10, 100),
                     (uint16_t)ARBI_RANGE(seed, 5, 50),
                     (uint16_t)ARBI_RANGE(seed, 10, 50));

    uint8_t n_buttons = (uint8_t)ARBI_RANGE(seed, 1, BTN_HANDLER_MAX_BUTTONS);
    for (uint8_t i = 0; i < n_buttons; i++) {
        btn_handler_register(&bh, i, mock_read, NULL);
    }

    uint8_t ticks = (uint8_t)ARBI_RANGE(seed, 1, 200);
    for (uint8_t i = 0; i < ticks; i++) {
        mock_read_state = (bool)(ARBI_UINT8(seed) & 1);
        btn_handler_tick(&bh);
    }

    PROP_ASSERT(btn_handler_count(&bh) <= BTN_HANDLER_MAX_BUTTONS);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    btn_handler_t bh1, bh2;
    btn_handler_init(&bh1, mock_callback, NULL, 5, 50, 30, 20);
    btn_handler_init(&bh2, mock_callback, NULL, 5, 50, 30, 20);
    btn_handler_init(&bh2, mock_callback, NULL, 5, 50, 30, 20);

    PROP_ASSERT_EQ(btn_handler_count(&bh1), btn_handler_count(&bh2));
}

/* Property: count never exceeds max after over-registering */
PROPERTY(count_bounded, 5000) {
    btn_handler_t bh;
    btn_handler_init(&bh, mock_callback, NULL, 5, 50, 30, 20);

    for (uint8_t i = 0; i < BTN_HANDLER_MAX_BUTTONS + 4; i++) {
        btn_handler_register(&bh, i, mock_read, NULL);
    }

    PROP_ASSERT(btn_handler_count(&bh) <= BTN_HANDLER_MAX_BUTTONS);
}

TEST_MAIN("button_handler properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(count_bounded);
}
TEST_MAIN_END()
