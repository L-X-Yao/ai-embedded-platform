/*
 * prop_param_table.c — property-based tests for param_table
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_param_table.c ../param_table.c -o prop_param_table
 */

#include "test_harness.h"
#include "property_test.h"
#include "../param_table.h"
#include <string.h>

/* Property: random define/set/get sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    param_table_t pt;
    param_table_init(&pt);

    static const char *keys[] = {"a", "b", "c", "d", "e", "f"};
    static const char *vals[] = {"100", "hello", "0", "true", "-5", "xyz"};
    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 5;
        const char *key = keys[ARBI_UINT8(seed) % 6];
        if (op == 0) {
            bool ro = (bool)(ARBI_UINT8(seed) & 1);
            param_table_define(&pt, key, vals[ARBI_UINT8(seed) % 6], ro);
        } else if (op == 1) {
            param_table_set(&pt, key, vals[ARBI_UINT8(seed) % 6]);
        } else if (op == 2) {
            char buf[PARAM_VALUE_SIZE];
            param_table_get(&pt, key, buf, sizeof(buf));
        } else if (op == 3) {
            param_table_find(&pt, key);
        } else {
            char buf[512];
            param_table_list(&pt, buf, sizeof(buf));
        }
    }

    PROP_ASSERT(param_table_count(&pt) <= PARAM_MAX_ENTRIES);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    param_table_t pt1, pt2;
    param_table_init(&pt1);
    param_table_init(&pt2);
    param_table_init(&pt2);

    PROP_ASSERT_EQ(param_table_count(&pt1), param_table_count(&pt2));
}

/* Property: define then get returns default value */
PROPERTY(define_get_default, 10000) {
    param_table_t pt;
    param_table_init(&pt);

    static const char *vals[] = {"42", "hello", "0", "abc"};
    const char *defval = vals[ARBI_UINT8(seed) % 4];

    param_err_t err = param_table_define(&pt, "test", defval, false);
    PROP_ASSERT_EQ(err, PARAM_OK);

    char got[PARAM_VALUE_SIZE];
    err = param_table_get(&pt, "test", got, sizeof(got));
    PROP_ASSERT_EQ(err, PARAM_OK);
    PROP_ASSERT(strcmp(got, defval) == 0);
}

/* Property: set then get roundtrip */
PROPERTY(set_get_roundtrip, 10000) {
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "x", "0", false);

    /* Generate a random value string (short alpha) */
    char val[16];
    uint8_t vlen = (uint8_t)ARBI_RANGE(seed, 1, 15);
    for (uint8_t i = 0; i < vlen; i++)
        val[i] = (char)ARBI_RANGE(seed, 'a', 'z');
    val[vlen] = '\0';

    param_table_set(&pt, "x", val);
    char got[PARAM_VALUE_SIZE];
    param_table_get(&pt, "x", got, sizeof(got));
    PROP_ASSERT(strcmp(got, val) == 0);
}

TEST_MAIN("param_table properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(define_get_default);
    RUN(set_get_roundtrip);
}
TEST_MAIN_END()
