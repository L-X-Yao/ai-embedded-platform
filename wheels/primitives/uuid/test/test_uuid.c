// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "uuid.h"

/* ---- deterministic RNG for testing ------------------------------------- */

static uint8_t rng_counter;

static void rng_deterministic(void *ctx, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = rng_counter++;
    }
}

static void rng_all_ff(void *ctx, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    memset(buf, 0xFF, len);
}

static void rng_all_zero(void *ctx, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    memset(buf, 0x00, len);
}

/* ---- tests ------------------------------------------------------------- */

TEST(test_v4_version_bits)
{
    uuid_t uuid;
    rng_counter = 0;
    uuid_err_t err = uuid_v4_generate(&uuid, rng_deterministic, NULL);
    ASSERT_EQ(err, UUID_OK);
    /* Version nibble (byte[6] high nibble) must be 4 */
    ASSERT_EQ((uuid.bytes[6] >> 4) & 0x0F, 4);
}

TEST(test_v4_variant_bits)
{
    uuid_t uuid;
    rng_counter = 0;
    uuid_err_t err = uuid_v4_generate(&uuid, rng_deterministic, NULL);
    ASSERT_EQ(err, UUID_OK);
    /* Variant (byte[8] upper 2 bits) must be 10 */
    ASSERT_EQ((uuid.bytes[8] >> 6) & 0x03, 2);
}

TEST(test_v4_deterministic_rng)
{
    uuid_t uuid;
    rng_counter = 0;
    uuid_err_t err = uuid_v4_generate(&uuid, rng_deterministic, NULL);
    ASSERT_EQ(err, UUID_OK);

    /* RNG fills bytes 0..15 with 0x00..0x0F, then version/variant applied */
    ASSERT_EQ(uuid.bytes[0], 0x00);
    ASSERT_EQ(uuid.bytes[5], 0x05);
    ASSERT_EQ(uuid.bytes[6], 0x46);  /* 0x06 -> 0x40 | 0x06 = 0x46 */
    ASSERT_EQ(uuid.bytes[8], 0x88);  /* 0x08 -> 0x80 | 0x08 = 0x88 */
    ASSERT_EQ(uuid.bytes[15], 0x0F);
}

TEST(test_v4_all_ff_rng)
{
    uuid_t uuid;
    uuid_err_t err = uuid_v4_generate(&uuid, rng_all_ff, NULL);
    ASSERT_EQ(err, UUID_OK);
    /* byte[6]: 0xFF -> 0x40 | 0x0F = 0x4F */
    ASSERT_EQ(uuid.bytes[6], 0x4F);
    /* byte[8]: 0xFF -> 0x80 | 0x3F = 0xBF */
    ASSERT_EQ(uuid.bytes[8], 0xBF);
}

TEST(test_to_string_format)
{
    uuid_t uuid;
    rng_counter = 0;
    uuid_v4_generate(&uuid, rng_deterministic, NULL);

    char buf[37];
    uuid_err_t err = uuid_to_string(&uuid, buf, 37);
    ASSERT_EQ(err, UUID_OK);

    /* Verify format: 8-4-4-4-12 */
    ASSERT_EQ(buf[8], '-');
    ASSERT_EQ(buf[13], '-');
    ASSERT_EQ(buf[18], '-');
    ASSERT_EQ(buf[23], '-');
    ASSERT_EQ(strlen(buf), 36);

    /* Verify version character is '4' */
    ASSERT_EQ(buf[14], '4');
}

TEST(test_to_string_buffer_too_small)
{
    uuid_t uuid = {0};
    char buf[36]; /* one byte too small */
    uuid_err_t err = uuid_to_string(&uuid, buf, 36);
    ASSERT_EQ(err, UUID_ERR_PARAM);
}

TEST(test_from_string_valid)
{
    const char *str = "550e8400-e29b-41d4-a716-446655440000";
    uuid_t uuid;
    uuid_err_t err = uuid_from_string(&uuid, str);
    ASSERT_EQ(err, UUID_OK);
    ASSERT_EQ(uuid.bytes[0], 0x55);
    ASSERT_EQ(uuid.bytes[1], 0x0e);
    ASSERT_EQ(uuid.bytes[2], 0x84);
    ASSERT_EQ(uuid.bytes[3], 0x00);
    ASSERT_EQ(uuid.bytes[4], 0xe2);
    ASSERT_EQ(uuid.bytes[5], 0x9b);
}

TEST(test_from_string_uppercase)
{
    const char *lower = "550e8400-e29b-41d4-a716-446655440000";
    const char *upper = "550E8400-E29B-41D4-A716-446655440000";
    uuid_t uuid_lo, uuid_up;
    ASSERT_EQ(uuid_from_string(&uuid_lo, lower), UUID_OK);
    ASSERT_EQ(uuid_from_string(&uuid_up, upper), UUID_OK);
    ASSERT_TRUE(uuid_equal(&uuid_lo, &uuid_up));
}

TEST(test_from_string_invalid_format)
{
    uuid_t uuid;
    /* Missing hyphens */
    ASSERT_EQ(uuid_from_string(&uuid, "550e8400e29b41d4a716446655440000xxxx"), UUID_ERR_FORMAT);
    /* Invalid hex char */
    ASSERT_EQ(uuid_from_string(&uuid, "550e8400-e29b-41d4-a716-44665544000g"), UUID_ERR_FORMAT);
    /* Hyphens in wrong positions */
    ASSERT_EQ(uuid_from_string(&uuid, "550e840-0e29b-41d4-a716-446655440000"), UUID_ERR_FORMAT);
}

TEST(test_from_string_wrong_length)
{
    uuid_t uuid;
    /* Too short */
    ASSERT_EQ(uuid_from_string(&uuid, "550e8400-e29b-41d4-a716"), UUID_ERR_FORMAT);
    /* Too long */
    ASSERT_EQ(uuid_from_string(&uuid, "550e8400-e29b-41d4-a716-4466554400001"), UUID_ERR_FORMAT);
}

TEST(test_roundtrip)
{
    uuid_t original, parsed;
    rng_counter = 42;
    uuid_v4_generate(&original, rng_deterministic, NULL);

    char buf[37];
    ASSERT_EQ(uuid_to_string(&original, buf, 37), UUID_OK);
    ASSERT_EQ(uuid_from_string(&parsed, buf), UUID_OK);
    ASSERT_TRUE(uuid_equal(&original, &parsed));
}

TEST(test_equal_same)
{
    uuid_t a, b;
    rng_counter = 0;
    uuid_v4_generate(&a, rng_deterministic, NULL);
    rng_counter = 0;
    uuid_v4_generate(&b, rng_deterministic, NULL);
    ASSERT_TRUE(uuid_equal(&a, &b));
}

TEST(test_equal_different)
{
    uuid_t a, b;
    rng_counter = 0;
    uuid_v4_generate(&a, rng_deterministic, NULL);
    rng_counter = 100;
    uuid_v4_generate(&b, rng_deterministic, NULL);
    ASSERT_TRUE(!uuid_equal(&a, &b));
}

TEST(test_is_nil)
{
    ASSERT_TRUE(uuid_is_nil(&UUID_NIL));

    uuid_t zero;
    memset(&zero, 0, sizeof(zero));
    ASSERT_TRUE(uuid_is_nil(&zero));
}

TEST(test_is_nil_non_nil)
{
    uuid_t uuid;
    rng_counter = 0;
    uuid_v4_generate(&uuid, rng_deterministic, NULL);
    ASSERT_TRUE(!uuid_is_nil(&uuid));
}

TEST(test_null_params)
{
    uuid_t uuid;
    char buf[37];

    /* generate: null uuid */
    ASSERT_EQ(uuid_v4_generate(NULL, rng_deterministic, NULL), UUID_ERR_PARAM);
    /* generate: null rng */
    ASSERT_EQ(uuid_v4_generate(&uuid, NULL, NULL), UUID_ERR_PARAM);
    /* to_string: null uuid */
    ASSERT_EQ(uuid_to_string(NULL, buf, 37), UUID_ERR_PARAM);
    /* to_string: null buf */
    ASSERT_EQ(uuid_to_string(&uuid, NULL, 37), UUID_ERR_PARAM);
    /* from_string: null uuid */
    ASSERT_EQ(uuid_from_string(NULL, "550e8400-e29b-41d4-a716-446655440000"), UUID_ERR_PARAM);
    /* from_string: null str */
    ASSERT_EQ(uuid_from_string(&uuid, NULL), UUID_ERR_PARAM);
    /* equal: null */
    ASSERT_TRUE(!uuid_equal(NULL, &uuid));
    ASSERT_TRUE(!uuid_equal(&uuid, NULL));
    /* is_nil: null */
    ASSERT_TRUE(!uuid_is_nil(NULL));
}

TEST(test_nil_constant_is_all_zeros)
{
    for (int i = 0; i < 16; i++) {
        ASSERT_EQ(UUID_NIL.bytes[i], 0);
    }
}

TEST(test_zero_rng_produces_version_variant)
{
    uuid_t uuid;
    uuid_v4_generate(&uuid, rng_all_zero, NULL);
    /* Even with all-zero RNG, version and variant bits must be set */
    ASSERT_EQ((uuid.bytes[6] >> 4) & 0x0F, 4);
    ASSERT_EQ((uuid.bytes[8] >> 6) & 0x03, 2);
    /* Other bytes remain zero */
    ASSERT_EQ(uuid.bytes[0], 0x00);
    ASSERT_EQ(uuid.bytes[15], 0x00);
}

TEST(test_to_string_exact_value)
{
    /* Construct a known UUID manually */
    uuid_t uuid;
    rng_counter = 0;
    uuid_v4_generate(&uuid, rng_deterministic, NULL);

    char buf[37];
    uuid_to_string(&uuid, buf, 37);

    /* bytes: 00 01 02 03 - 04 05 - 46 07 - 88 09 - 0a 0b 0c 0d 0e 0f */
    ASSERT_EQ(strcmp(buf, "00010203-0405-4607-8809-0a0b0c0d0e0f"), 0);
}

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
    uuid_t uuid = {0};
    ASSERT_TRUE(uuid_is_nil(&uuid));

    char buf[37];
    ASSERT_EQ(uuid_to_string(&uuid, buf, 37), UUID_OK);
}

TEST_MAIN("uuid")
    RUN(test_v4_version_bits);
    RUN(test_v4_variant_bits);
    RUN(test_v4_deterministic_rng);
    RUN(test_v4_all_ff_rng);
    RUN(test_to_string_format);
    RUN(test_to_string_buffer_too_small);
    RUN(test_from_string_valid);
    RUN(test_from_string_uppercase);
    RUN(test_from_string_invalid_format);
    RUN(test_from_string_wrong_length);
    RUN(test_roundtrip);
    RUN(test_equal_same);
    RUN(test_equal_different);
    RUN(test_is_nil);
    RUN(test_is_nil_non_nil);
    RUN(test_null_params);
    RUN(test_nil_constant_is_all_zeros);
    RUN(test_zero_rng_produces_version_variant);
    RUN(test_to_string_exact_value);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
