// SPDX-License-Identifier: MIT
/*
 * telemetry built against a snprintf that cannot print floats, as
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
#include "telemetry.c"
#undef snprintf

static char record[TELEMETRY_BUF_SIZE + 1];

static telemetry_err_t keep_record(void *ctx, const char *data, uint16_t len)
{
    (void)ctx;
    memcpy(record, data, len);
    record[len] = '\0';
    return TELEMETRY_OK;
}

static telemetry_err_t publish_one(telemetry_format_t format, float value)
{
    static telemetry_t t;

    record[0] = '\0';
    telemetry_init(&t, keep_record, NULL, format);
    telemetry_register(&t, "temp");
    telemetry_update(&t, "temp", value);
    return telemetry_publish(&t, 7);
}

TEST(test_float_is_published_without_float_printf)
{
    float_conversions = 0;

    ASSERT_EQ(publish_one(TELEMETRY_FMT_JSON, 100.5f), TELEMETRY_OK);
    ASSERT_STR_EQ(record, "{\"ts\":7,\"temp\":100.5}");
    ASSERT_EQ(publish_one(TELEMETRY_FMT_CSV, 100.5f), TELEMETRY_OK);
    ASSERT_STR_EQ(record, "7,100.5");
    ASSERT_EQ(publish_one(TELEMETRY_FMT_KEY_VALUE, 100.5f), TELEMETRY_OK);
    ASSERT_STR_EQ(record, "ts=7 temp=100.5");

    ASSERT_EQ(float_conversions, 0);
}

TEST(test_float_vectors)
{
    static const struct {
        float value;
        const char *text;
    } vectors[] = {
        { 0.0f,        "7,0" },
        { -0.0f,       "7,-0" },
        { 1.0f,        "7,1" },
        { 10.0f,       "7,10" },      /* only zeros after the point go */
        { 100.0f,      "7,100" },
        { 60.0f,       "7,60" },
        { 25.5f,       "7,25.5" },
        { 1.23f,       "7,1.23" },
        { 1.05f,       "7,1.05" },    /* a zero that stays */
        { 0.5f,        "7,0.5" },
        { 0.25f,       "7,0.25" },
        { 0.125f,      "7,0.12" },    /* exact tie: to the even digit */
        { 0.375f,      "7,0.38" },    /* exact tie: to the even digit */
        { 1.125f,      "7,1.12" },
        { -0.375f,     "7,-0.38" },
        { 0.005f,      "7,0" },       /* stored as 0.00499999988 */
        { 0.015f,      "7,0.01" },    /* stored as 0.0149999997 */
        { 0.995f,      "7,1" },       /* stored as 0.995000005: carries */
        { 123.45f,     "7,123.45" },
        { -9.99f,      "7,-9.99" },
        { 16777216.0f, "7,16777216" },
        { 1e15f,       "7,999999986991104" },
        { FLT_MIN,     "7,0" },
        { -FLT_MIN,    "7,-0" },
    };

    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        ASSERT_EQ(publish_one(TELEMETRY_FMT_CSV, vectors[i].value),
                  TELEMETRY_OK);
        ASSERT_STR_EQ(record, vectors[i].text);
    }
}

/* update refuses these, but a field can be written directly */
TEST(test_non_finite_prints_zero)
{
    char text[8];

    ASSERT_EQ(fmt_float(text, (int)sizeof(text), NAN), 1);
    ASSERT_STR_EQ(text, "0");
    ASSERT_EQ(fmt_float(text, (int)sizeof(text), INFINITY), 1);
    ASSERT_STR_EQ(text, "0");
    ASSERT_EQ(fmt_float(text, (int)sizeof(text), -INFINITY), 1);
    ASSERT_STR_EQ(text, "0");
}

/* Printing with the real snprintf and trimming is the oracle, at any size. */
TEST(test_float_text_matches_printf)
{
    uint32_t rng = 0x9E3779B9u;

    for (uint32_t i = 0; i < 400000u; i++) {
        uint32_t bits;
        float value;
        char want[64];
        char got[64];

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
        ASSERT_TRUE(n > 3 && (size_t)n < sizeof(want));
        if (want[n - 1] == '0') {
            want[--n] = '\0';
            if (want[n - 1] == '0') {
                want[--n] = '\0';
                want[--n] = '\0';  /* the point */
            }
        }
        ASSERT_EQ(fmt_float(got, (int)sizeof(got), value), n);
        ASSERT_STR_EQ(got, want);
    }
}

TEST_MAIN("telemetry without float printf")
    RUN(test_float_is_published_without_float_printf);
    RUN(test_float_vectors);
    RUN(test_non_finite_prints_zero);
    RUN(test_float_text_matches_printf);
TEST_MAIN_END()
