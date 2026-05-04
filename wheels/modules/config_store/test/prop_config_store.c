/*
 * prop_config_store.c — property-based tests for config_store
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_config_store.c ../config_store.c -o prop_config_store
 */

#include "test_harness.h"
#include "property_test.h"
#include "../config_store.h"
#include <string.h>

/* Property: random define/set/get sequences never crash, count stays valid */
PROPERTY(random_ops_no_crash, 10000) {
    config_store_t cs;
    config_store_init(&cs, NULL);

    static const char *keys[] = {"a", "b", "c", "d", "e", "f"};
    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 5;
        const char *key = keys[ARBI_UINT8(seed) % 6];
        if (op == 0) {
            config_store_define_int(&cs, key, (int32_t)ARBI_UINT32(seed), -1000, 1000);
        } else if (op == 1) {
            config_store_set_int(&cs, key, (int32_t)ARBI_RANGE(seed, -2000, 2000));
        } else if (op == 2) {
            int32_t val = 0;
            config_store_get_int(&cs, key, &val);
        } else if (op == 3) {
            config_store_define_bool(&cs, key, (bool)(ARBI_UINT8(seed) & 1));
        } else {
            config_store_reset_defaults(&cs);
        }
    }

    uint8_t count = config_store_count(&cs);
    PROP_ASSERT(count <= CONFIG_STORE_MAX_ENTRIES);
}

/* Property: init twice == init once (idempotent) */
PROPERTY(init_idempotent, 5000) {
    config_store_t cs1, cs2;
    config_store_init(&cs1, NULL);
    config_store_init(&cs2, NULL);
    config_store_init(&cs2, NULL); /* second init */

    PROP_ASSERT_EQ(config_store_count(&cs1), config_store_count(&cs2));
}

/* Property: define then get returns default value */
PROPERTY(define_get_default, 10000) {
    config_store_t cs;
    config_store_init(&cs, NULL);

    int32_t def = (int32_t)ARBI_RANGE(seed, -1000, 1000);
    config_store_err_t err = config_store_define_int(&cs, "test", def, -2000, 2000);
    PROP_ASSERT_EQ(err, CONFIG_STORE_OK);

    int32_t got = 0;
    err = config_store_get_int(&cs, "test", &got);
    PROP_ASSERT_EQ(err, CONFIG_STORE_OK);
    PROP_ASSERT_EQ(got, def);
}

/* Property: set then get returns set value */
PROPERTY(set_get_roundtrip, 10000) {
    config_store_t cs;
    config_store_init(&cs, NULL);
    config_store_define_int(&cs, "x", 0, -10000, 10000);

    int32_t val = (int32_t)ARBI_RANGE(seed, -10000, 10000);
    config_store_err_t err = config_store_set_int(&cs, "x", val);
    PROP_ASSERT_EQ(err, CONFIG_STORE_OK);

    int32_t got = 0;
    err = config_store_get_int(&cs, "x", &got);
    PROP_ASSERT_EQ(err, CONFIG_STORE_OK);
    PROP_ASSERT_EQ(got, val);
}

TEST_MAIN("config_store properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(define_get_default);
    RUN(set_get_roundtrip);
}
TEST_MAIN_END()
