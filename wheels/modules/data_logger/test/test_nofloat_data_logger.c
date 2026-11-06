// SPDX-License-Identifier: MIT
/*
 * data_logger built against a snprintf that cannot print floats, as
 * newlib-nano is without _printf_float.  Full newlib can, but takes the
 * memory from the heap, which a wheel may not use: so a float must never
 * reach snprintf at all.
 */
#include "test_harness.h"
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int float_conversions;

static int nano_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    if (strchr(fmt, 'f') != NULL) {
        float_conversions++;
        if (size > 0) buf[0] = '\0';
        return 0;
    }
    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

#define snprintf nano_snprintf
#include "data_logger.c"
#undef snprintf

static char record[DATA_LOGGER_BUF_SIZE + 1];

static data_logger_err_t keep_record(void *ctx, const uint8_t *data,
                                     uint16_t len)
{
    (void)ctx;
    memcpy(record, data, len);
    record[len] = '\0';
    return DATA_LOGGER_OK;
}

static const data_logger_storage_t storage = { keep_record, NULL, NULL };

static const data_logger_field_t float_int[] = {
    { "x", DATA_LOGGER_FIELD_FLOAT },
    { "n", DATA_LOGGER_FIELD_INT32 },
};

static data_logger_err_t log_float(float value)
{
    data_logger_t dl;
    data_logger_err_t err;

    data_logger_init(&dl, &storage, float_int, 1);
    data_logger_begin(&dl, 7);
    err = data_logger_add_float(&dl, value);
    return err != DATA_LOGGER_OK ? err : data_logger_commit(&dl);
}

TEST(test_float_is_written_without_float_printf)
{
    data_logger_t dl;

    float_conversions = 0;
    data_logger_init(&dl, &storage, float_int, 2);
    data_logger_begin(&dl, 7);
    ASSERT_EQ(data_logger_add_float(&dl, 100.5f), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_add_int(&dl, 42), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_commit(&dl), DATA_LOGGER_OK);

    ASSERT_STR_EQ(record, "7,100.50,42\n");
    ASSERT_EQ(float_conversions, 0);
}

TEST(test_float_vectors)
{
    static const struct {
        float value;
        const char *text;
    } vectors[] = {
        { 0.0f,        "7,0.00\n" },
        { -0.0f,       "7,-0.00\n" },
        { 1.0f,        "7,1.00\n" },
        { 0.5f,        "7,0.50\n" },
        { 0.25f,       "7,0.25\n" },
        { 0.125f,      "7,0.12\n" },  /* exact tie: to the even digit */
        { 0.375f,      "7,0.38\n" },  /* exact tie: to the even digit */
        { 1.125f,      "7,1.12\n" },
        { -0.375f,     "7,-0.38\n" },
        { 0.005f,      "7,0.00\n" },  /* stored as 0.00499999988 */
        { 0.015f,      "7,0.01\n" },  /* stored as 0.0149999997 */
        { 0.995f,      "7,1.00\n" },  /* stored as 0.995000005: carries */
        { 123.45f,     "7,123.45\n" },
        { -9.99f,      "7,-9.99\n" },
        { 16777216.0f, "7,16777216.00\n" },
        { 1e15f,       "7,999999986991104.00\n" },
        { FLT_MIN,     "7,0.00\n" },
        { -FLT_MIN,    "7,-0.00\n" },
        { INFINITY,    "7,inf\n" },
        { -INFINITY,   "7,-inf\n" },
        { NAN,         "7,nan\n" },
    };

    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        ASSERT_EQ(log_float(vectors[i].value), DATA_LOGGER_OK);
        ASSERT_STR_EQ(record, vectors[i].text);
    }
}

/* Printing with the real snprintf is the oracle: 19 characters fit. */
TEST(test_float_text_matches_printf)
{
    uint32_t rng = 0x9E3779B9u;

    for (uint32_t i = 0; i < 400000u; i++) {
        uint32_t bits;
        float value;
        char want[64];
        char line[80];

        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        bits = rng;
        if ((i & 1u) != 0) {  /* every other one in the range that gets used */
            bits = (bits & 0x807FFFFFu) | ((100u + (rng >> 8) % 90u) << 23);
        }
        memcpy(&value, &bits, sizeof(value));
        if (isnan(value) || isinf(value)) continue;

        int n = snprintf(want, sizeof(want), "%.2f", (double)value);
        data_logger_err_t err = log_float(value);
        if (n >= 20) {
            ASSERT_EQ(err, DATA_LOGGER_ERR_OVERFLOW);
            continue;
        }
        ASSERT_EQ(err, DATA_LOGGER_OK);
        snprintf(line, sizeof(line), "7,%s\n", want);
        ASSERT_STR_EQ(record, line);
    }
}

TEST_MAIN("data_logger without float printf")
    RUN(test_float_is_written_without_float_printf);
    RUN(test_float_vectors);
    RUN(test_float_text_matches_printf);
TEST_MAIN_END()
