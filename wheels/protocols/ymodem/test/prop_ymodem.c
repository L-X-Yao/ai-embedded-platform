// SPDX-License-Identifier: MIT
/*
 * prop_ymodem.c — property-based tests for ymodem
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_ymodem.c ../ymodem.c -o prop_ymodem
 */

#include "test_harness.h"
#include "property_test.h"
#include "../ymodem.h"
#include <string.h>

/* --- mock IO callbacks --- */
static ymodem_err_t mock_write_block(void *ctx, uint32_t offset,
                                      const uint8_t *data, uint16_t len) {
    (void)ctx; (void)offset; (void)data; (void)len;
    return YMODEM_OK;
}

static ymodem_err_t mock_send_byte(void *ctx, uint8_t byte) {
    (void)ctx; (void)byte;
    return YMODEM_OK;
}

static const ymodem_io_t mock_io = {
    .write_block   = mock_write_block,
    .send_byte     = mock_send_byte,
    .on_file_start = NULL,
    .ctx           = NULL,
};

/* Property: feed random bytes never crashes */
PROPERTY(feed_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    ymodem_t ym;
    memset(&ym, 0, sizeof(ym));
    ymodem_init(&ym, &mock_io);
    ymodem_start(&ym);

    uint8_t num_bytes = (uint8_t)ARBI_RANGE(seed, 1, 200);
    for (uint8_t i = 0; i < num_bytes; i++) {
        uint8_t byte = ARBI_UINT8(seed);
        ymodem_feed(&ym, byte);
        ymodem_state_t st = ymodem_get_state(&ym);
        if (st == YMODEM_STATE_COMPLETE || st == YMODEM_STATE_ERROR)
            break;
    }
}

/* Property: init is idempotent */
PROPERTY(init_idempotent, 5000) {
    (void)prop__seed_orig; (void)prop__iter;
    ymodem_t ym1, ym2;
    memset(&ym1, 0, sizeof(ym1));
    memset(&ym2, 0, sizeof(ym2));

    ymodem_init(&ym1, &mock_io);
    ymodem_init(&ym2, &mock_io);
    ymodem_init(&ym2, &mock_io); /* second init */

    PROP_ASSERT_EQ(ymodem_get_state(&ym1), ymodem_get_state(&ym2));
    PROP_ASSERT_EQ(ymodem_get_file_size(&ym1), ymodem_get_file_size(&ym2));
}

/* Property: state queries are consistent after random operations */
PROPERTY(state_consistent, 10000) {
    ymodem_t ym;
    memset(&ym, 0, sizeof(ym));
    ymodem_init(&ym, &mock_io);

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 0, 20);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 3;
        if (op == 0) {
            ymodem_start(&ym);
        } else if (op == 1) {
            ymodem_feed(&ym, ARBI_UINT8(seed));
        } else {
            ymodem_cancel(&ym);
        }
    }

    ymodem_state_t st = ymodem_get_state(&ym);
    PROP_ASSERT(st >= YMODEM_STATE_IDLE && st <= YMODEM_STATE_ERROR);
    (void)ymodem_get_file_size(&ym);
    (void)ymodem_get_filename(&ym);
}

TEST_MAIN("ymodem properties") {
    RUN(feed_no_crash);
    RUN(init_idempotent);
    RUN(state_consistent);
}
TEST_MAIN_END()
