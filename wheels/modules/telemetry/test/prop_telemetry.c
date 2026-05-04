/*
 * prop_telemetry.c — property-based tests for telemetry
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_telemetry.c ../telemetry.c -o prop_telemetry
 */

#include "test_harness.h"
#include "property_test.h"
#include "../telemetry.h"
#include <string.h>

static telemetry_err_t mock_send(void *ctx, const char *data, uint16_t len) {
    (void)ctx; (void)data; (void)len;
    return TELEMETRY_OK;
}

/* Property: random register/update/publish sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    telemetry_t t;
    telemetry_format_t fmts[] = { TELEMETRY_FMT_JSON, TELEMETRY_FMT_CSV,
                                   TELEMETRY_FMT_KEY_VALUE };
    telemetry_init(&t, mock_send, NULL, fmts[ARBI_UINT8(seed) % 3]);

    static const char *names[] = {"temp", "hum", "pres", "volt", "curr"};
    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 4;
        if (op == 0) {
            telemetry_register(&t, names[ARBI_UINT8(seed) % 5]);
        } else if (op == 1) {
            float val = (float)(int32_t)ARBI_UINT32(seed) / 100.0f;
            telemetry_update(&t, names[ARBI_UINT8(seed) % 5], val);
        } else if (op == 2) {
            telemetry_publish(&t, ARBI_UINT32(seed));
        } else {
            telemetry_reset(&t);
        }
    }

    /* Counters are always accessible */
    (void)telemetry_get_send_count(&t);
    (void)telemetry_get_error_count(&t);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    telemetry_t t1, t2;
    telemetry_init(&t1, mock_send, NULL, TELEMETRY_FMT_JSON);
    telemetry_init(&t2, mock_send, NULL, TELEMETRY_FMT_JSON);
    telemetry_init(&t2, mock_send, NULL, TELEMETRY_FMT_JSON);

    PROP_ASSERT_EQ(telemetry_get_send_count(&t1), telemetry_get_send_count(&t2));
    PROP_ASSERT_EQ(telemetry_get_error_count(&t1), telemetry_get_error_count(&t2));
}

/* Property: register then update_by_index works for all registered fields */
PROPERTY(update_by_index_consistent, 10000) {
    telemetry_t t;
    telemetry_init(&t, mock_send, NULL, TELEMETRY_FMT_JSON);

    uint8_t n = (uint8_t)ARBI_RANGE(seed, 1, 5);
    static const char *names[] = {"a", "b", "c", "d", "e"};
    for (uint8_t i = 0; i < n; i++) {
        telemetry_register(&t, names[i]);
    }

    /* Update by index should succeed for valid indices */
    for (uint8_t i = 0; i < n; i++) {
        float val = (float)(int16_t)ARBI_UINT16(seed);
        telemetry_err_t err = telemetry_update_by_index(&t, i, val);
        PROP_ASSERT_EQ(err, TELEMETRY_OK);
    }
    /* Index out of range should fail */
    telemetry_err_t err = telemetry_update_by_index(&t, n + 1, 0.0f);
    PROP_ASSERT(err != TELEMETRY_OK);
}

TEST_MAIN("telemetry properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(update_by_index_consistent);
}
TEST_MAIN_END()
