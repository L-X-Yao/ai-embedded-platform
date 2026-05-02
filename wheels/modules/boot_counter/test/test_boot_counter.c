#include "test_harness.h"
#include "boot_counter.h"
#include <string.h>

/* ================================================================
 * Mock storage backend
 * ================================================================ */

static uint8_t g_storage[sizeof(boot_counter_record_t)];
static bool    g_fail_read;
static bool    g_fail_write;
static int     g_write_count;

static void mock_reset(void)
{
    memset(g_storage, 0, sizeof(g_storage));
    g_fail_read  = false;
    g_fail_write = false;
    g_write_count = 0;
}

static int mock_read(uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (g_fail_read) { return -1; }
    if (size > sizeof(g_storage)) { return -1; }
    memcpy(buf, g_storage, size);
    return 0;
}

static int mock_write(const uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (g_fail_write) { return -1; }
    if (size > sizeof(g_storage)) { return -1; }
    memcpy(g_storage, buf, size);
    g_write_count++;
    return 0;
}

static boot_counter_storage_t make_storage(void)
{
    boot_counter_storage_t s;
    s.read     = mock_read;
    s.write    = mock_write;
    s.user_ctx = NULL;
    return s;
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();

    boot_counter_err_t err = boot_counter_init(&ctx, &stor);
    ASSERT_EQ(err, BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx), 0);
    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 0);
}

TEST(test_init_null_ctx)
{
    boot_counter_storage_t stor = make_storage();
    ASSERT_EQ(boot_counter_init(NULL, &stor), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_init_null_storage)
{
    boot_counter_t ctx;
    ASSERT_EQ(boot_counter_init(&ctx, NULL), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_init_null_read_cb)
{
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    stor.read = NULL;
    ASSERT_EQ(boot_counter_init(&ctx, &stor), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_init_null_write_cb)
{
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    stor.write = NULL;
    ASSERT_EQ(boot_counter_init(&ctx, &stor), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_load_save_roundtrip)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    /* Increment to set count=1, then save to mock storage */
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 1);

    /* Create a fresh context and load from same storage */
    boot_counter_t ctx2;
    boot_counter_init(&ctx2, &stor);
    ASSERT_EQ(boot_counter_load(&ctx2), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx2), 1);
}

TEST(test_increment)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 1);
}

TEST(test_multiple_increments)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    }
    ASSERT_EQ(boot_counter_get_count(&ctx), 10);

    /* Verify persistence: reload from storage */
    boot_counter_t ctx2;
    boot_counter_init(&ctx2, &stor);
    boot_counter_load(&ctx2);
    ASSERT_EQ(boot_counter_get_count(&ctx2), 10);
}

TEST(test_reset_reason)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    ASSERT_EQ(boot_counter_set_reset_reason(&ctx, 0x04), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx), 0x04);

    /* Verify persistence */
    boot_counter_t ctx2;
    boot_counter_init(&ctx2, &stor);
    boot_counter_load(&ctx2);
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx2), 0x04);
}

TEST(test_uptime_ticks)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 0);

    boot_counter_tick_second(&ctx);
    boot_counter_tick_second(&ctx);
    boot_counter_tick_second(&ctx);
    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 3);

    for (int i = 0; i < 100; i++) {
        boot_counter_tick_second(&ctx);
    }
    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 103);
}

TEST(test_storage_read_failure)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    g_fail_read = true;
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_ERR_STORAGE);
    /* After failed load, count should be zero (safe default) */
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
}

TEST(test_storage_write_failure)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    g_fail_write = true;
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_STORAGE);
    /* In-memory count still incremented (write failed after bump) */
    ASSERT_EQ(boot_counter_get_count(&ctx), 1);
}

TEST(test_set_reset_reason_write_failure)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    g_fail_write = true;
    ASSERT_EQ(boot_counter_set_reset_reason(&ctx, 7), BOOT_COUNTER_ERR_STORAGE);
    /* In-memory reason still set */
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx), 7);
}

TEST(test_null_ctx_getters)
{
    ASSERT_EQ(boot_counter_get_count(NULL), 0);
    ASSERT_EQ(boot_counter_get_reset_reason(NULL), 0);
    ASSERT_EQ(boot_counter_get_uptime_seconds(NULL), 0);
}

TEST(test_null_ctx_mutators)
{
    ASSERT_EQ(boot_counter_load(NULL), BOOT_COUNTER_ERR_PARAM);
    ASSERT_EQ(boot_counter_increment(NULL), BOOT_COUNTER_ERR_PARAM);
    ASSERT_EQ(boot_counter_set_reset_reason(NULL, 1), BOOT_COUNTER_ERR_PARAM);
    /* tick_second with NULL should not crash */
    boot_counter_tick_second(NULL);
    ASSERT_TRUE(1);
}

TEST(test_increment_persists_each_call)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    g_write_count = 0;
    boot_counter_increment(&ctx);
    boot_counter_increment(&ctx);
    boot_counter_increment(&ctx);
    /* Each increment should trigger a write */
    ASSERT_EQ(g_write_count, 3);
}

/* ================================================================ */

TEST_MAIN("boot_counter")
    RUN(test_init_valid);
    RUN(test_init_null_ctx);
    RUN(test_init_null_storage);
    RUN(test_init_null_read_cb);
    RUN(test_init_null_write_cb);
    RUN(test_load_save_roundtrip);
    RUN(test_increment);
    RUN(test_multiple_increments);
    RUN(test_reset_reason);
    RUN(test_uptime_ticks);
    RUN(test_storage_read_failure);
    RUN(test_storage_write_failure);
    RUN(test_set_reset_reason_write_failure);
    RUN(test_null_ctx_getters);
    RUN(test_null_ctx_mutators);
    RUN(test_increment_persists_each_call);
TEST_MAIN_END()
