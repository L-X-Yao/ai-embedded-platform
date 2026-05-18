// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <flashdb.h>
#include <fal.h>

/* ---------- test harness ---------- */
/* ---------- mock flash device ---------- */

#define MOCK_FLASH_SIZE  (64 * 1024)
#define MOCK_BLOCK_SIZE  4096

static uint8_t mock_flash_buf[MOCK_FLASH_SIZE];

static int mock_init(void)
{
    memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
    return 0;
}

static int mock_read(long offset, uint8_t *buf, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    memcpy(buf, mock_flash_buf + offset, size);
    return (int)size;
}

static int mock_write(long offset, const uint8_t *buf, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    for (size_t i = 0; i < size; i++)
        mock_flash_buf[offset + i] &= buf[i];
    return (int)size;
}

static int mock_erase(long offset, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    memset(mock_flash_buf + offset, 0xFF, size);
    return (int)size;
}

const struct fal_flash_dev mock_flash_dev = {
    .name      = "mock_flash",
    .addr      = 0,
    .len       = MOCK_FLASH_SIZE,
    .blk_size  = MOCK_BLOCK_SIZE,
    .ops       = { .init = mock_init, .read = mock_read,
                   .write = mock_write, .erase = mock_erase },
    .write_gran = 1,
};

/* ---------- helpers ---------- */

static struct fdb_kvdb test_kvdb;

static void reset_flash(void)
{
    memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
}

static struct fdb_default_kv_node default_kv_table[] = {
    { "boot_count", "0",   0 },
    { "device_id",  "A01", 0 },
};

static fdb_err_t init_kvdb(void)
{
    struct fdb_default_kv default_kv;
    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

    memset(&test_kvdb, 0, sizeof(test_kvdb));
    uint32_t sec_size = MOCK_BLOCK_SIZE;
    fdb_kvdb_control(&test_kvdb, FDB_KVDB_CTRL_SET_SEC_SIZE, &sec_size);

    return fdb_kvdb_init(&test_kvdb, "test", "kvdb", &default_kv, NULL);
}

/* ---------- tests ---------- */

TEST(test_fal_init)
{
    int n = fal_init();
    ASSERT_TRUE(n == 3);
}

TEST(test_kvdb_init)
{
    reset_flash();
    fdb_err_t err = init_kvdb();
    ASSERT_TRUE(err == FDB_NO_ERR);
}

TEST(test_default_kv)
{
    reset_flash();
    init_kvdb();

    char *val = fdb_kv_get(&test_kvdb, "boot_count");
    ASSERT_TRUE(val != NULL);
    ASSERT_TRUE(strcmp(val, "0") == 0);

    val = fdb_kv_get(&test_kvdb, "device_id");
    ASSERT_TRUE(val != NULL);
    ASSERT_TRUE(strcmp(val, "A01") == 0);
}

TEST(test_set_get_string)
{
    reset_flash();
    init_kvdb();

    fdb_err_t err = fdb_kv_set(&test_kvdb, "wifi_ssid", "MyNetwork");
    ASSERT_TRUE(err == FDB_NO_ERR);

    char *val = fdb_kv_get(&test_kvdb, "wifi_ssid");
    ASSERT_TRUE(val != NULL);
    ASSERT_TRUE(strcmp(val, "MyNetwork") == 0);
}

TEST(test_overwrite)
{
    reset_flash();
    init_kvdb();

    fdb_kv_set(&test_kvdb, "counter", "1");
    fdb_kv_set(&test_kvdb, "counter", "2");
    fdb_kv_set(&test_kvdb, "counter", "3");

    char *val = fdb_kv_get(&test_kvdb, "counter");
    ASSERT_TRUE(val != NULL);
    ASSERT_TRUE(strcmp(val, "3") == 0);
}

TEST(test_delete)
{
    reset_flash();
    init_kvdb();

    fdb_kv_set(&test_kvdb, "temp_key", "temp_val");
    ASSERT_TRUE(fdb_kv_get(&test_kvdb, "temp_key") != NULL);

    fdb_err_t err = fdb_kv_del(&test_kvdb, "temp_key");
    ASSERT_TRUE(err == FDB_NO_ERR);

    char *val = fdb_kv_get(&test_kvdb, "temp_key");
    ASSERT_TRUE(val == NULL);
}

TEST(test_blob)
{
    reset_flash();
    init_kvdb();

    uint32_t write_val = 0xDEADBEEF;
    struct fdb_blob blob;
    fdb_err_t err = fdb_kv_set_blob(&test_kvdb, "magic",
                                     fdb_blob_make(&blob, &write_val, sizeof(write_val)));
    ASSERT_TRUE(err == FDB_NO_ERR);

    uint32_t read_val = 0;
    size_t read_len = fdb_kv_get_blob(&test_kvdb, "magic",
                                       fdb_blob_make(&blob, &read_val, sizeof(read_val)));
    ASSERT_TRUE(read_len == sizeof(uint32_t));
    ASSERT_TRUE(read_val == 0xDEADBEEF);
}

TEST(test_persistence)
{
    reset_flash();
    init_kvdb();

    fdb_kv_set(&test_kvdb, "persist_key", "persist_val");
    fdb_kvdb_deinit(&test_kvdb);

    /* Re-init without erasing flash — data should survive */
    fdb_err_t err = init_kvdb();
    ASSERT_TRUE(err == FDB_NO_ERR);

    char *val = fdb_kv_get(&test_kvdb, "persist_key");
    ASSERT_TRUE(val != NULL);
    ASSERT_TRUE(strcmp(val, "persist_val") == 0);
}

TEST(test_get_nonexistent)
{
    reset_flash();
    init_kvdb();

    char *val = fdb_kv_get(&test_kvdb, "no_such_key");
    ASSERT_TRUE(val == NULL);
}

TEST(test_multiple_keys)
{
    reset_flash();
    init_kvdb();

    for (int i = 0; i < 20; i++) {
        char key[16], val[16];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "val_%d", i);
        fdb_kv_set(&test_kvdb, key, val);
    }

    for (int i = 0; i < 20; i++) {
        char key[16], expected[16];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(expected, sizeof(expected), "val_%d", i);
        char *got = fdb_kv_get(&test_kvdb, key);
        ASSERT_TRUE(got != NULL && strcmp(got, expected) == 0);
    }
}

TEST(test_iterator)
{
    reset_flash();
    init_kvdb();

    fdb_kv_set(&test_kvdb, "iter_a", "1");
    fdb_kv_set(&test_kvdb, "iter_b", "2");

    struct fdb_kv_iterator iterator;
    fdb_kv_iterator_init(&test_kvdb, &iterator);

    int count = 0;
    while (fdb_kv_iterate(&test_kvdb, &iterator)) {
        count++;
    }
    /* defaults (2) + iter_a + iter_b = 4 */
    ASSERT_TRUE(count >= 4);
}

/* ---------- main ---------- */

TEST_MAIN("kvs")
    RUN(test_fal_init);
    RUN(test_kvdb_init);
    RUN(test_default_kv);
    RUN(test_set_get_string);
    RUN(test_overwrite);
    RUN(test_delete);
    RUN(test_blob);
    RUN(test_persistence);
    RUN(test_get_nonexistent);
    RUN(test_multiple_keys);
    RUN(test_iterator);
TEST_MAIN_END()

