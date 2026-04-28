#include <stdio.h>
#include <string.h>
#include <flashdb.h>
#include <fal.h>

/* ---------- test harness ---------- */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

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
    memcpy(buf, mock_flash_buf + offset, size);
    return (int)size;
}

static int mock_write(long offset, const uint8_t *buf, size_t size)
{
    for (size_t i = 0; i < size; i++)
        mock_flash_buf[offset + i] &= buf[i];
    return (int)size;
}

static int mock_erase(long offset, size_t size)
{
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

static void test_fal_init(void)
{
    printf("test_fal_init\n");
    int n = fal_init();
    ASSERT(n == 3, "3 partitions");
}

static void test_kvdb_init(void)
{
    printf("test_kvdb_init\n");
    reset_flash();
    fdb_err_t err = init_kvdb();
    ASSERT(err == FDB_NO_ERR, "kvdb init ok");
}

static void test_default_kv(void)
{
    printf("test_default_kv\n");
    reset_flash();
    init_kvdb();

    char *val = fdb_kv_get(&test_kvdb, "boot_count");
    ASSERT(val != NULL, "boot_count exists");
    ASSERT(strcmp(val, "0") == 0, "boot_count default is 0");

    val = fdb_kv_get(&test_kvdb, "device_id");
    ASSERT(val != NULL, "device_id exists");
    ASSERT(strcmp(val, "A01") == 0, "device_id default is A01");
}

static void test_set_get_string(void)
{
    printf("test_set_get_string\n");
    reset_flash();
    init_kvdb();

    fdb_err_t err = fdb_kv_set(&test_kvdb, "wifi_ssid", "MyNetwork");
    ASSERT(err == FDB_NO_ERR, "set string ok");

    char *val = fdb_kv_get(&test_kvdb, "wifi_ssid");
    ASSERT(val != NULL, "get string not null");
    ASSERT(strcmp(val, "MyNetwork") == 0, "string value matches");
}

static void test_overwrite(void)
{
    printf("test_overwrite\n");
    reset_flash();
    init_kvdb();

    fdb_kv_set(&test_kvdb, "counter", "1");
    fdb_kv_set(&test_kvdb, "counter", "2");
    fdb_kv_set(&test_kvdb, "counter", "3");

    char *val = fdb_kv_get(&test_kvdb, "counter");
    ASSERT(val != NULL, "overwritten value exists");
    ASSERT(strcmp(val, "3") == 0, "latest value is 3");
}

static void test_delete(void)
{
    printf("test_delete\n");
    reset_flash();
    init_kvdb();

    fdb_kv_set(&test_kvdb, "temp_key", "temp_val");
    ASSERT(fdb_kv_get(&test_kvdb, "temp_key") != NULL, "key exists before delete");

    fdb_err_t err = fdb_kv_del(&test_kvdb, "temp_key");
    ASSERT(err == FDB_NO_ERR, "delete ok");

    char *val = fdb_kv_get(&test_kvdb, "temp_key");
    ASSERT(val == NULL, "key gone after delete");
}

static void test_blob(void)
{
    printf("test_blob\n");
    reset_flash();
    init_kvdb();

    uint32_t write_val = 0xDEADBEEF;
    struct fdb_blob blob;
    fdb_err_t err = fdb_kv_set_blob(&test_kvdb, "magic",
                                     fdb_blob_make(&blob, &write_val, sizeof(write_val)));
    ASSERT(err == FDB_NO_ERR, "set blob ok");

    uint32_t read_val = 0;
    size_t read_len = fdb_kv_get_blob(&test_kvdb, "magic",
                                       fdb_blob_make(&blob, &read_val, sizeof(read_val)));
    ASSERT(read_len == sizeof(uint32_t), "blob read 4 bytes");
    ASSERT(read_val == 0xDEADBEEF, "blob value matches");
}

static void test_persistence(void)
{
    printf("test_persistence\n");
    reset_flash();
    init_kvdb();

    fdb_kv_set(&test_kvdb, "persist_key", "persist_val");
    fdb_kvdb_deinit(&test_kvdb);

    /* Re-init without erasing flash — data should survive */
    fdb_err_t err = init_kvdb();
    ASSERT(err == FDB_NO_ERR, "re-init ok");

    char *val = fdb_kv_get(&test_kvdb, "persist_key");
    ASSERT(val != NULL, "persisted key survives re-init");
    ASSERT(strcmp(val, "persist_val") == 0, "persisted value correct");
}

static void test_get_nonexistent(void)
{
    printf("test_get_nonexistent\n");
    reset_flash();
    init_kvdb();

    char *val = fdb_kv_get(&test_kvdb, "no_such_key");
    ASSERT(val == NULL, "nonexistent returns NULL");
}

static void test_multiple_keys(void)
{
    printf("test_multiple_keys\n");
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
        ASSERT(got != NULL && strcmp(got, expected) == 0, "batch key matches");
    }
}

static void test_iterator(void)
{
    printf("test_iterator\n");
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
    ASSERT(count >= 4, "iterator finds all keys");
}

/* ---------- main ---------- */

int main(void)
{
    tests_run = tests_fail = 0;

    test_fal_init();
    test_kvdb_init();
    test_default_kv();
    test_set_get_string();
    test_overwrite();
    test_delete();
    test_blob();
    test_persistence();
    test_get_nonexistent();
    test_multiple_keys();
    test_iterator();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
