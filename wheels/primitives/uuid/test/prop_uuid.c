/*
 * prop_uuid.c — property-based tests for uuid
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_uuid.c ../uuid.c -o prop_uuid
 */

#include "test_harness.h"
#include "property_test.h"
#include "../uuid.h"
#include <string.h>

/* ---- xorshift RNG adapter for uuid -------------------------------- */

static uint32_t g_rng_state;

static void test_rng(void *ctx, uint8_t *buf, uint16_t len) {
    (void)ctx;
    for (uint16_t i = 0; i < len; i++) {
        g_rng_state ^= g_rng_state << 13;
        g_rng_state ^= g_rng_state >> 17;
        g_rng_state ^= g_rng_state << 5;
        buf[i] = (uint8_t)(g_rng_state & 0xFF);
    }
}

/* Property: generated UUIDs are unique (no collision in 100 per seed) */
PROPERTY(uniqueness, 5000) {
    uuid_t uuids[100];
    g_rng_state = seed;

    for (int i = 0; i < 100; i++) {
        uuid_err_t err = uuid_v4_generate(&uuids[i], test_rng, NULL);
        PROP_ASSERT_EQ(err, UUID_OK);
    }

    /* Pairwise uniqueness check */
    for (int i = 0; i < 100; i++) {
        for (int j = i + 1; j < 100; j++) {
            PROP_ASSERT(!uuid_equal(&uuids[i], &uuids[j]));
        }
    }
}

/* Property: generated UUID has correct version (4) and variant bits */
PROPERTY(version_variant, 10000) {
    uuid_t u;
    g_rng_state = seed;
    uuid_v4_generate(&u, test_rng, NULL);

    /* Version nibble: byte 6, high nibble must be 0x4 */
    PROP_ASSERT_EQ((u.bytes[6] >> 4) & 0x0F, 4);

    /* Variant: byte 8, top 2 bits must be 10 */
    PROP_ASSERT_EQ((u.bytes[8] >> 6) & 0x03, 2);
}

/* Property: to_string / from_string roundtrip */
PROPERTY(string_roundtrip, 10000) {
    uuid_t u, parsed;
    g_rng_state = seed;
    uuid_v4_generate(&u, test_rng, NULL);

    char buf[37];
    uuid_err_t err = uuid_to_string(&u, buf, sizeof(buf));
    PROP_ASSERT_EQ(err, UUID_OK);

    /* Verify format: 8-4-4-4-12 with dashes */
    PROP_ASSERT_EQ(buf[8], '-');
    PROP_ASSERT_EQ(buf[13], '-');
    PROP_ASSERT_EQ(buf[18], '-');
    PROP_ASSERT_EQ(buf[23], '-');
    PROP_ASSERT_EQ(strlen(buf), 36);

    err = uuid_from_string(&parsed, buf);
    PROP_ASSERT_EQ(err, UUID_OK);
    PROP_ASSERT(uuid_equal(&u, &parsed));
}

/* Property: nil UUID is nil, generated UUID is not nil */
PROPERTY(nil_check, 5000) {
    PROP_ASSERT(uuid_is_nil(&UUID_NIL));

    uuid_t u;
    g_rng_state = seed;
    uuid_v4_generate(&u, test_rng, NULL);
    PROP_ASSERT(!uuid_is_nil(&u));
}

TEST_MAIN("uuid properties") {
    RUN(uniqueness);
    RUN(version_variant);
    RUN(string_roundtrip);
    RUN(nil_check);
}
TEST_MAIN_END()
