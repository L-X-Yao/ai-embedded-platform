// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "string_utils.h"

/* ── trim tests ─────────────────────────────────────────────────────── */

TEST(test_trim_basic)
{
    char buf[32];
    ASSERT_EQ(str_trim("  hello  ", buf, sizeof(buf)), STRING_UTILS_OK);
    ASSERT_TRUE(strcmp(buf, "hello") == 0);
}

TEST(test_ltrim)
{
    char buf[32];
    ASSERT_EQ(str_ltrim("  hello  ", buf, sizeof(buf)), STRING_UTILS_OK);
    ASSERT_TRUE(strcmp(buf, "hello  ") == 0);
}

TEST(test_rtrim)
{
    char buf[32];
    ASSERT_EQ(str_rtrim("  hello  ", buf, sizeof(buf)), STRING_UTILS_OK);
    ASSERT_TRUE(strcmp(buf, "  hello") == 0);
}

TEST(test_trim_empty)
{
    char buf[16];
    ASSERT_EQ(str_trim("   ", buf, sizeof(buf)), STRING_UTILS_OK);
    ASSERT_TRUE(strcmp(buf, "") == 0);
}

TEST(test_trim_null)
{
    char buf[16];
    ASSERT_EQ(str_trim(NULL, buf, sizeof(buf)), STRING_UTILS_ERR_PARAM);
    ASSERT_EQ(str_trim("hi", NULL, sizeof(buf)), STRING_UTILS_ERR_PARAM);
}

/* ── split tests ────────────────────────────────────────────────────── */

TEST(test_split_basic)
{
    char input[] = "a,b,c";
    char *tokens[4];
    int n = str_split(input, ',', tokens, 4);
    ASSERT_EQ(n, 3);
    ASSERT_TRUE(strcmp(tokens[0], "a") == 0);
    ASSERT_TRUE(strcmp(tokens[1], "b") == 0);
    ASSERT_TRUE(strcmp(tokens[2], "c") == 0);
}

TEST(test_split_max_tokens)
{
    char input[] = "1:2:3:4";
    char *tokens[2];
    int n = str_split(input, ':', tokens, 2);
    ASSERT_EQ(n, 2);
    ASSERT_TRUE(strcmp(tokens[0], "1") == 0);
    ASSERT_TRUE(strcmp(tokens[1], "2") == 0);
}

TEST(test_split_null)
{
    ASSERT_EQ(str_split(NULL, ',', NULL, 0), 0);
}

/* ── starts_with / ends_with ────────────────────────────────────────── */

TEST(test_starts_with)
{
    ASSERT_TRUE(str_starts_with("hello world", "hello"));
    ASSERT_FALSE(str_starts_with("hello world", "world"));
    ASSERT_TRUE(str_starts_with("abc", ""));
    ASSERT_FALSE(str_starts_with(NULL, "x"));
}

TEST(test_ends_with)
{
    ASSERT_TRUE(str_ends_with("hello world", "world"));
    ASSERT_FALSE(str_ends_with("hello world", "hello"));
    ASSERT_TRUE(str_ends_with("abc", ""));
    ASSERT_FALSE(str_ends_with("ab", "abc"));
}

/* ── parsing tests ──────────────────────────────────────────────────── */

TEST(test_to_int)
{
    int32_t v = 0;
    ASSERT_EQ(str_to_int("42", &v), STRING_UTILS_OK);
    ASSERT_EQ(v, 42);

    ASSERT_EQ(str_to_int("-100", &v), STRING_UTILS_OK);
    ASSERT_EQ(v, -100);

    ASSERT_EQ(str_to_int("+7", &v), STRING_UTILS_OK);
    ASSERT_EQ(v, 7);

    ASSERT_EQ(str_to_int("abc", &v), STRING_UTILS_ERR_FMT);
    ASSERT_EQ(str_to_int("", &v), STRING_UTILS_ERR_FMT);
    ASSERT_EQ(str_to_int(NULL, &v), STRING_UTILS_ERR_PARAM);
}

TEST(test_to_float)
{
    float f = 0.0f;
    ASSERT_EQ(str_to_float("3.14", &f), STRING_UTILS_OK);
    ASSERT_NEAR(f, 3.14f, 0.01f);

    ASSERT_EQ(str_to_float("-0.5", &f), STRING_UTILS_OK);
    ASSERT_NEAR(f, -0.5f, 0.01f);

    ASSERT_EQ(str_to_float("100", &f), STRING_UTILS_OK);
    ASSERT_NEAR(f, 100.0f, 0.01f);

    ASSERT_EQ(str_to_float("abc", &f), STRING_UTILS_ERR_FMT);
    ASSERT_EQ(str_to_float("", &f), STRING_UTILS_ERR_FMT);
    ASSERT_EQ(str_to_float(NULL, &f), STRING_UTILS_ERR_PARAM);
}

/* ── range error tests ──────────────────────────────────────────────── */

TEST(test_to_int_overflow_positive)
{
    int32_t v = 0;
    /* 99999999999 overflows INT32_MAX (2147483647) */
    ASSERT_EQ(str_to_int("99999999999", &v), STRING_UTILS_ERR_RANGE);
}

TEST(test_to_int_overflow_negative)
{
    int32_t v = 0;
    /* -99999999999 overflows INT32_MIN (-2147483648) */
    ASSERT_EQ(str_to_int("-99999999999", &v), STRING_UTILS_ERR_RANGE);
}

TEST(test_to_int_overflow_boundary)
{
    int32_t v = 0;
    /* 2147483648 is INT32_MAX + 1, should overflow */
    ASSERT_EQ(str_to_int("2147483648", &v), STRING_UTILS_ERR_RANGE);

    /* INT32_MAX itself should succeed */
    ASSERT_EQ(str_to_int("2147483647", &v), STRING_UTILS_OK);
    ASSERT_EQ(v, 2147483647);

    /* INT32_MIN should succeed */
    ASSERT_EQ(str_to_int("-2147483648", &v), STRING_UTILS_OK);
    ASSERT_EQ(v, -2147483647 - 1);  /* avoid literal warning */

    /* INT32_MIN - 1 should overflow */
    ASSERT_EQ(str_to_int("-2147483649", &v), STRING_UTILS_ERR_RANGE);
}

/* ── join tests ─────────────────────────────────────────────────────── */

TEST(test_join_basic)
{
    const char *toks[] = {"a", "b", "c"};
    char buf[32];
    ASSERT_EQ(str_join(toks, 3, ",", buf, sizeof(buf)), STRING_UTILS_OK);
    ASSERT_TRUE(strcmp(buf, "a,b,c") == 0);
}

TEST(test_join_overflow)
{
    const char *toks[] = {"hello", "world"};
    char buf[8];
    ASSERT_EQ(str_join(toks, 2, ",", buf, sizeof(buf)), STRING_UTILS_ERR_OVERFLOW);
    /* should be truncated but NUL-terminated */
    ASSERT_EQ(buf[7], '\0');
}

TEST(test_join_null_param)
{
    char buf[16];
    ASSERT_EQ(str_join(NULL, 0, ",", buf, sizeof(buf)), STRING_UTILS_ERR_PARAM);
}

/* ── zero-init safety ───────────────────────────────────────────────── */

TEST(test_zero_init_safety)
{
    char buf[16] = {0};
    /* empty string trim should not crash */
    ASSERT_EQ(str_trim("", buf, sizeof(buf)), STRING_UTILS_OK);
    ASSERT_TRUE(strcmp(buf, "") == 0);
}

/* Digits past the float range are a range error, as in str_to_int. */
TEST(test_float_out_of_range)
{
    static const char *const big[] = {
        "999999999999999999999999999999999999999999999",  /* 45 nines */
        "340282350000000000000000000000000000000000",     /* 3.4e41 */
        "-999999999999999999999999999999999999999999999",
        "+340282350000000000000000000000000000000000.5",
    };
    float v = 7.0f;

    for (size_t i = 0; i < sizeof(big) / sizeof(big[0]); i++) {
        ASSERT_EQ(str_to_float(big[i], &v), STRING_UTILS_ERR_RANGE);
        ASSERT_TRUE(v == 7.0f); /* output untouched, as in str_to_int */
    }
}

TEST(test_float_large_but_in_range)
{
    float v = 0.0f;

    ASSERT_EQ(str_to_float("300000000000000000000000000000000000000", &v),
              STRING_UTILS_OK);
    ASSERT_TRUE(v > 2.9e38f && v < 3.1e38f);
    ASSERT_EQ(str_to_float("-300000000000000000000000000000000000000", &v),
              STRING_UTILS_OK);
    ASSERT_TRUE(v < -2.9e38f && v > -3.1e38f);
}

/* A malformed string stays a format error however long its digits run. */
TEST(test_float_format_beats_range)
{
    float v = 0.0f;

    ASSERT_EQ(str_to_float("999999999999999999999999999999999999999999999x", &v),
              STRING_UTILS_ERR_FMT);
}

/* ---------- truncation ---------- */

#define GUARD 0x7E

/* What a buffer of size bytes must hold once full is cut to fit: the first
 * size - 1 bytes and a NUL, with every byte past the buffer untouched. */
static void check_output(const char *buf, size_t cap, size_t size,
                         const char *full)
{
    size_t len = strlen(full);
    size_t n = size - 1 < len ? size - 1 : len;

    ASSERT_EQ(strlen(buf), n);
    ASSERT_TRUE(memcmp(buf, full, n) == 0);
    for (size_t i = size; i < cap; i++) {
        ASSERT_EQ(buf[i], GUARD);
    }
}

typedef string_utils_err_t (*trim_fn_t)(const char *in, char *out, size_t size);

/* Sizes from one byte to one past the fit cover exact, one short and one byte. */
static void check_trim(trim_fn_t trim, const char *in, const char *full)
{
    char buf[24];

    for (size_t size = 1; size <= strlen(full) + 2; size++) {
        memset(buf, GUARD, sizeof(buf));
        ASSERT_EQ(trim(in, buf, size), STRING_UTILS_OK);
        check_output(buf, sizeof(buf), size, full);
    }
}

static void check_join(const char **toks, int count, const char *delim,
                       const char *full)
{
    char buf[24];
    size_t need = strlen(full) + 1;

    for (size_t size = 1; size <= need + 1; size++) {
        memset(buf, GUARD, sizeof(buf));
        ASSERT_EQ(str_join(toks, count, delim, buf, size),
                  size >= need ? STRING_UTILS_OK : STRING_UTILS_ERR_OVERFLOW);
        check_output(buf, sizeof(buf), size, full);
    }
}

TEST(test_trim_truncation)
{
    check_trim(str_trim, "  abcd  ", "abcd");
    check_trim(str_ltrim, "  abcd  ", "abcd  ");
    check_trim(str_rtrim, "  abcd  ", "  abcd");
    check_trim(str_trim, "abcd", "abcd");
    check_trim(str_trim, "    ", "");
}

TEST(test_join_truncation)
{
    const char *two[] = {"ab", "cd"};
    const char *full_then_more[] = {"abcd", "x"};
    const char *three[] = {"a", "b", "c"};
    const char *empty_first[] = {"", "x"};
    const char *empty_last[] = {"x", ""};

    check_join(two, 2, "-", "ab-cd");
    check_join(two, 2, "--", "ab--cd");
    check_join(two, 2, "---", "ab---cd"); /* long enough to pass the end */
    check_join(two, 2, "", "abcd");
    check_join(full_then_more, 2, "", "abcdx");
    check_join(full_then_more, 2, ",", "abcd,x");
    check_join(three, 3, ", ", "a, b, c");
    check_join(empty_first, 2, "-", "-x");
    check_join(empty_last, 2, "-", "x-");
    check_join(two, 1, "-", "ab");
    check_join(two, 0, "-", "");
}

TEST_MAIN("string_utils")
    RUN(test_trim_basic);
    RUN(test_ltrim);
    RUN(test_rtrim);
    RUN(test_trim_empty);
    RUN(test_trim_null);
    RUN(test_split_basic);
    RUN(test_split_max_tokens);
    RUN(test_split_null);
    RUN(test_starts_with);
    RUN(test_ends_with);
    RUN(test_to_int);
    RUN(test_to_float);
    RUN(test_to_int_overflow_positive);
    RUN(test_to_int_overflow_negative);
    RUN(test_to_int_overflow_boundary);
    RUN(test_join_basic);
    RUN(test_join_overflow);
    RUN(test_join_null_param);
    RUN(test_zero_init_safety);
    RUN(test_float_out_of_range);
    RUN(test_float_large_but_in_range);
    RUN(test_float_format_beats_range);
    RUN(test_trim_truncation);
    RUN(test_join_truncation);
TEST_MAIN_END()
