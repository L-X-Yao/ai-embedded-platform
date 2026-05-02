// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "param_table.h"

/* ---- helpers ---- */
static int g_cb_count;
static char g_cb_old[PARAM_VALUE_SIZE];
static char g_cb_new[PARAM_VALUE_SIZE];

static void test_change_cb(void *ctx, const char *key,
                           const char *old_val, const char *new_val)
{
    (void)key;
    int *counter = (int *)ctx;
    (*counter)++;
    g_cb_count = *counter;
    strncpy(g_cb_old, old_val, PARAM_VALUE_SIZE - 1);
    g_cb_old[PARAM_VALUE_SIZE - 1] = '\0';
    strncpy(g_cb_new, new_val, PARAM_VALUE_SIZE - 1);
    g_cb_new[PARAM_VALUE_SIZE - 1] = '\0';
}

/* ---- tests ---- */

TEST(test_init)
{
    param_table_t pt;
    ASSERT_EQ(PARAM_OK, param_table_init(&pt));
    ASSERT_EQ(0, param_table_count(&pt));
}

TEST(test_define_and_get)
{
    param_table_t pt;
    param_table_init(&pt);
    ASSERT_EQ(PARAM_OK, param_table_define(&pt, "baud", "115200", false));

    char val[PARAM_VALUE_SIZE];
    ASSERT_EQ(PARAM_OK, param_table_get(&pt, "baud", val, sizeof(val)));
    ASSERT_TRUE(strcmp("115200", val) == 0);
    ASSERT_EQ(1, param_table_count(&pt));
}

TEST(test_set_and_get)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "mode", "auto", false);

    ASSERT_EQ(PARAM_OK, param_table_set(&pt, "mode", "manual"));

    char val[PARAM_VALUE_SIZE];
    ASSERT_EQ(PARAM_OK, param_table_get(&pt, "mode", val, sizeof(val)));
    ASSERT_TRUE(strcmp("manual", val) == 0);
}

TEST(test_readonly_rejection)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "hw_ver", "3.1", true);

    ASSERT_EQ(PARAM_ERR_READONLY, param_table_set(&pt, "hw_ver", "4.0"));

    /* Value unchanged */
    char val[PARAM_VALUE_SIZE];
    ASSERT_EQ(PARAM_OK, param_table_get(&pt, "hw_ver", val, sizeof(val)));
    ASSERT_TRUE(strcmp("3.1", val) == 0);
}

TEST(test_change_callback)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "freq", "1000", false);

    int counter = 0;
    g_cb_count = 0;
    ASSERT_EQ(PARAM_OK, param_table_set_callback(&pt, "freq",
                                                  test_change_cb, &counter));
    ASSERT_EQ(PARAM_OK, param_table_set(&pt, "freq", "2000"));

    ASSERT_EQ(1, g_cb_count);
    ASSERT_TRUE(strcmp("1000", g_cb_old) == 0);
    ASSERT_TRUE(strcmp("2000", g_cb_new) == 0);
}

TEST(test_list_format)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "a", "1", false);
    param_table_define(&pt, "b", "2", false);

    char buf[128];
    int n = param_table_list(&pt, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strcmp("a=1\nb=2\n", buf) == 0);
    ASSERT_EQ(8, n);
}

TEST(test_max_entries)
{
    param_table_t pt;
    param_table_init(&pt);

    /* Keys must persist — param_table stores pointer, not copy. */
    static char names[PARAM_MAX_ENTRIES][8];
    for (int i = 0; i < PARAM_MAX_ENTRIES; i++) {
        names[i][0] = 'p';
        names[i][1] = (char)('0' + i / 10);
        names[i][2] = (char)('0' + i % 10);
        names[i][3] = '\0';
        ASSERT_EQ(PARAM_OK, param_table_define(&pt, names[i], "x", false));
    }
    ASSERT_EQ(PARAM_ERR_FULL, param_table_define(&pt, "extra", "y", false));
    ASSERT_EQ(PARAM_MAX_ENTRIES, param_table_count(&pt));
}

TEST(test_not_found)
{
    param_table_t pt;
    param_table_init(&pt);

    char val[32];
    ASSERT_EQ(PARAM_ERR_NOT_FOUND, param_table_get(&pt, "nope", val, sizeof(val)));
    ASSERT_EQ(PARAM_ERR_NOT_FOUND, param_table_set(&pt, "nope", "x"));
}

TEST(test_value_overflow)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "short", "ok", false);

    /* Build a string that is exactly PARAM_VALUE_SIZE chars (too long). */
    char big[PARAM_VALUE_SIZE + 1];
    memset(big, 'A', PARAM_VALUE_SIZE);
    big[PARAM_VALUE_SIZE] = '\0';

    ASSERT_EQ(PARAM_ERR_OVERFLOW, param_table_set(&pt, "short", big));
}

TEST(test_null_params)
{
    param_table_t pt;
    param_table_init(&pt);

    ASSERT_EQ(PARAM_ERR_PARAM, param_table_init(NULL));
    ASSERT_EQ(PARAM_ERR_PARAM, param_table_define(NULL, "k", "v", false));
    ASSERT_EQ(PARAM_ERR_PARAM, param_table_define(&pt, NULL, "v", false));
    ASSERT_EQ(PARAM_ERR_PARAM, param_table_set(NULL, "k", "v"));
    ASSERT_EQ(PARAM_ERR_PARAM, param_table_set(&pt, NULL, "v"));
    ASSERT_EQ(PARAM_ERR_PARAM, param_table_get(NULL, "k", NULL, 32));
    ASSERT_EQ(-1, param_table_find(NULL, "k"));
    ASSERT_EQ(0, param_table_count(NULL));
    ASSERT_EQ(-1, param_table_list(NULL, NULL, 0));
}

TEST(test_find)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "alpha", "a", false);
    param_table_define(&pt, "beta", "b", false);

    ASSERT_EQ(0, param_table_find(&pt, "alpha"));
    ASSERT_EQ(1, param_table_find(&pt, "beta"));
    ASSERT_EQ(-1, param_table_find(&pt, "gamma"));
}

TEST(test_overwrite_existing)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "val", "first", false);
    param_table_set(&pt, "val", "second");
    param_table_set(&pt, "val", "third");

    char val[PARAM_VALUE_SIZE];
    param_table_get(&pt, "val", val, sizeof(val));
    ASSERT_TRUE(strcmp("third", val) == 0);
}

TEST(test_multiple_params)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "x", "10", false);
    param_table_define(&pt, "y", "20", false);
    param_table_define(&pt, "z", "30", true);
    ASSERT_EQ(3, param_table_count(&pt));

    char val[PARAM_VALUE_SIZE];
    param_table_get(&pt, "x", val, sizeof(val));
    ASSERT_TRUE(strcmp("10", val) == 0);
    param_table_get(&pt, "y", val, sizeof(val));
    ASSERT_TRUE(strcmp("20", val) == 0);
    param_table_get(&pt, "z", val, sizeof(val));
    ASSERT_TRUE(strcmp("30", val) == 0);

    /* Set x, leave y, z is readonly */
    param_table_set(&pt, "x", "99");
    param_table_get(&pt, "x", val, sizeof(val));
    ASSERT_TRUE(strcmp("99", val) == 0);
    ASSERT_EQ(PARAM_ERR_READONLY, param_table_set(&pt, "z", "0"));
}

TEST(test_buffer_too_small)
{
    param_table_t pt;
    param_table_init(&pt);
    param_table_define(&pt, "longval", "hello_world", false);

    char tiny[4];
    ASSERT_EQ(PARAM_ERR_OVERFLOW, param_table_get(&pt, "longval", tiny, sizeof(tiny)));
}

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
    param_table_t pt;
    memset(&pt, 0, sizeof(pt));
    ASSERT_EQ(0, param_table_count(&pt));
    ASSERT_EQ(-1, param_table_find(&pt, "any"));

    char buf[64];
    int n = param_table_list(&pt, buf, sizeof(buf));
    ASSERT_EQ(0, n);
    ASSERT_TRUE(strcmp("", buf) == 0);
}

TEST_MAIN("param_table")
    RUN(test_init);
    RUN(test_define_and_get);
    RUN(test_set_and_get);
    RUN(test_readonly_rejection);
    RUN(test_change_callback);
    RUN(test_list_format);
    RUN(test_max_entries);
    RUN(test_not_found);
    RUN(test_value_overflow);
    RUN(test_null_params);
    RUN(test_find);
    RUN(test_overwrite_existing);
    RUN(test_multiple_params);
    RUN(test_buffer_too_small);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
