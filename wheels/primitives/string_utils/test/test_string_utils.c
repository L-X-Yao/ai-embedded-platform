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
    RUN(test_join_basic);
    RUN(test_join_overflow);
    RUN(test_join_null_param);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
