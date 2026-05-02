#include "test_harness.h"
#include <string.h>
#include "version_info.h"

TEST(test_parse_valid)
{
    version_t v;
    ASSERT_EQ(version_parse("1.2.3", &v), VERSION_INFO_OK);
    ASSERT_EQ(v.major, 1);
    ASSERT_EQ(v.minor, 2);
    ASSERT_EQ(v.patch, 3);
}

TEST(test_parse_zero)
{
    version_t v;
    ASSERT_EQ(version_parse("0.0.0", &v), VERSION_INFO_OK);
    ASSERT_EQ(v.major, 0);
    ASSERT_EQ(v.minor, 0);
    ASSERT_EQ(v.patch, 0);
}

TEST(test_parse_large)
{
    version_t v;
    ASSERT_EQ(version_parse("65535.0.1", &v), VERSION_INFO_OK);
    ASSERT_EQ(v.major, 65535);
}

TEST(test_parse_invalid_empty)
{
    version_t v;
    ASSERT_EQ(version_parse("", &v), VERSION_INFO_ERR_FMT);
}

TEST(test_parse_invalid_missing_patch)
{
    version_t v;
    ASSERT_EQ(version_parse("1.2", &v), VERSION_INFO_ERR_FMT);
}

TEST(test_parse_invalid_trailing)
{
    version_t v;
    ASSERT_EQ(version_parse("1.2.3-beta", &v), VERSION_INFO_ERR_FMT);
}

TEST(test_parse_invalid_alpha)
{
    version_t v;
    ASSERT_EQ(version_parse("a.b.c", &v), VERSION_INFO_ERR_FMT);
}

TEST(test_parse_null)
{
    version_t v;
    ASSERT_EQ(version_parse(NULL, &v), VERSION_INFO_ERR_PARAM);
    ASSERT_EQ(version_parse("1.0.0", NULL), VERSION_INFO_ERR_PARAM);
}

TEST(test_compare_equal)
{
    version_t a = {1, 2, 3};
    version_t b = {1, 2, 3};
    ASSERT_EQ(version_compare(&a, &b), 0);
}

TEST(test_compare_major)
{
    version_t a = {1, 0, 0};
    version_t b = {2, 0, 0};
    ASSERT_EQ(version_compare(&a, &b), -1);
    ASSERT_EQ(version_compare(&b, &a),  1);
}

TEST(test_compare_minor)
{
    version_t a = {1, 2, 0};
    version_t b = {1, 3, 0};
    ASSERT_EQ(version_compare(&a, &b), -1);
    ASSERT_EQ(version_compare(&b, &a),  1);
}

TEST(test_compare_patch)
{
    version_t a = {1, 2, 3};
    version_t b = {1, 2, 4};
    ASSERT_EQ(version_compare(&a, &b), -1);
    ASSERT_EQ(version_compare(&b, &a),  1);
}

TEST(test_compare_null)
{
    version_t a = {1, 0, 0};
    ASSERT_EQ(version_compare(NULL, &a), 0);
    ASSERT_EQ(version_compare(&a, NULL), 0);
}

TEST(test_compatible_same)
{
    version_t req = {1, 2, 0};
    version_t act = {1, 2, 0};
    ASSERT_TRUE(version_compatible(&req, &act));
}

TEST(test_compatible_newer_minor)
{
    version_t req = {1, 2, 0};
    version_t act = {1, 3, 0};
    ASSERT_TRUE(version_compatible(&req, &act));
}

TEST(test_compatible_older_minor)
{
    version_t req = {1, 3, 0};
    version_t act = {1, 2, 0};
    ASSERT_FALSE(version_compatible(&req, &act));
}

TEST(test_compatible_different_major)
{
    version_t req = {1, 0, 0};
    version_t act = {2, 0, 0};
    ASSERT_FALSE(version_compatible(&req, &act));
}

TEST(test_compatible_null)
{
    version_t v = {1, 0, 0};
    ASSERT_FALSE(version_compatible(NULL, &v));
    ASSERT_FALSE(version_compatible(&v, NULL));
}

TEST(test_to_string)
{
    version_t v = {1, 2, 3};
    char buf[32];
    int n = version_to_string(&v, buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_TRUE(strcmp(buf, "1.2.3") == 0);
}

TEST(test_to_string_buf_too_small)
{
    version_t v = {10, 20, 30};
    char buf[4];
    ASSERT_EQ(version_to_string(&v, buf, sizeof(buf)), -1);
}

TEST(test_to_string_null)
{
    version_t v = {0, 0, 0};
    char buf[16];
    ASSERT_EQ(version_to_string(NULL, buf, sizeof(buf)), -1);
    ASSERT_EQ(version_to_string(&v, NULL, 16), -1);
    ASSERT_EQ(version_to_string(&v, buf, 0), -1);
}

TEST(test_is_valid)
{
    version_t v = {1, 2, 3};
    ASSERT_TRUE(version_is_valid(&v));
}

TEST(test_is_valid_zero)
{
    version_t v = {0, 0, 0};
    ASSERT_TRUE(version_is_valid(&v));
}

TEST(test_is_valid_null)
{
    ASSERT_FALSE(version_is_valid(NULL));
}

TEST(test_zero_init_safety)
{
    version_t v;
    memset(&v, 0, sizeof(v));
    ASSERT_TRUE(version_is_valid(&v));
    char buf[16];
    int n = version_to_string(&v, buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_TRUE(strcmp(buf, "0.0.0") == 0);
}

TEST(test_roundtrip)
{
    version_t v1 = {12, 34, 56};
    char buf[32];
    version_to_string(&v1, buf, sizeof(buf));
    version_t v2;
    ASSERT_EQ(version_parse(buf, &v2), VERSION_INFO_OK);
    ASSERT_EQ(version_compare(&v1, &v2), 0);
}

TEST_MAIN("version_info")
    RUN(test_parse_valid);
    RUN(test_parse_zero);
    RUN(test_parse_large);
    RUN(test_parse_invalid_empty);
    RUN(test_parse_invalid_missing_patch);
    RUN(test_parse_invalid_trailing);
    RUN(test_parse_invalid_alpha);
    RUN(test_parse_null);
    RUN(test_compare_equal);
    RUN(test_compare_major);
    RUN(test_compare_minor);
    RUN(test_compare_patch);
    RUN(test_compare_null);
    RUN(test_compatible_same);
    RUN(test_compatible_newer_minor);
    RUN(test_compatible_older_minor);
    RUN(test_compatible_different_major);
    RUN(test_compatible_null);
    RUN(test_to_string);
    RUN(test_to_string_buf_too_small);
    RUN(test_to_string_null);
    RUN(test_is_valid);
    RUN(test_is_valid_zero);
    RUN(test_is_valid_null);
    RUN(test_zero_init_safety);
    RUN(test_roundtrip);
TEST_MAIN_END()
