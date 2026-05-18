// SPDX-License-Identifier: MIT
/*
 * prop_xmodem.c — property-based tests for xmodem
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_xmodem.c ../xmodem.c -o prop_xmodem
 */

#include "test_harness.h"
#include "property_test.h"
#include "../xmodem.h"
#include <string.h>

/* --- mock IO callbacks --- */
static xmodem_err_t mock_write_block(void *ctx, uint32_t offset,
                                      const uint8_t *data, uint16_t len) {
    (void)ctx; (void)offset; (void)data; (void)len;
    return XMODEM_OK;
}

static xmodem_err_t mock_send_byte(void *ctx, uint8_t byte) {
    (void)ctx; (void)byte;
    return XMODEM_OK;
}

static const xmodem_io_t mock_io = {
    .write_block = mock_write_block,
    .send_byte   = mock_send_byte,
    .ctx         = NULL,
};

/* Property: init then feed random bytes never crashes */
PROPERTY(feed_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_init(&xm, &mock_io, (bool)(ARBI_UINT8(seed) & 1));
    xmodem_start(&xm);

    uint8_t num_bytes = (uint8_t)ARBI_RANGE(seed, 1, 200);
    for (uint8_t i = 0; i < num_bytes; i++) {
        uint8_t byte = ARBI_UINT8(seed);
        xmodem_feed(&xm, byte);
        /* Bail on terminal states to avoid testing invalid sequences */
        xmodem_state_t st = xmodem_get_state(&xm);
        if (st == XMODEM_STATE_COMPLETE || st == XMODEM_STATE_ERROR)
            break;
    }
}

/* Property: init is idempotent (init twice == init once) */
PROPERTY(init_idempotent, 5000) {
    (void)prop__seed_orig; (void)prop__iter;
    xmodem_t xm1, xm2;
    memset(&xm1, 0, sizeof(xm1));
    memset(&xm2, 0, sizeof(xm2));
    bool use_crc = (bool)(ARBI_UINT8(seed) & 1);

    xmodem_init(&xm1, &mock_io, use_crc);
    xmodem_init(&xm2, &mock_io, use_crc);
    xmodem_init(&xm2, &mock_io, use_crc); /* second init */

    PROP_ASSERT_EQ(xmodem_get_state(&xm1), xmodem_get_state(&xm2));
    PROP_ASSERT_EQ(xmodem_get_total_bytes(&xm1), xmodem_get_total_bytes(&xm2));
}

/* Property: state queries are consistent after any sequence */
PROPERTY(state_consistent, 10000) {
    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_init(&xm, &mock_io, true);

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 0, 20);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 3;
        if (op == 0) {
            xmodem_start(&xm);
        } else if (op == 1) {
            xmodem_feed(&xm, ARBI_UINT8(seed));
        } else {
            xmodem_cancel(&xm);
        }
    }

    /* State must always be a valid enum value */
    xmodem_state_t st = xmodem_get_state(&xm);
    PROP_ASSERT(st >= XMODEM_STATE_IDLE && st <= XMODEM_STATE_ERROR);
    /* total_bytes is always accessible */
    (void)xmodem_get_total_bytes(&xm);
}

TEST_MAIN("xmodem properties") {
    RUN(feed_no_crash);
    RUN(init_idempotent);
    RUN(state_consistent);
}
TEST_MAIN_END()
