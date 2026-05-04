/*
 * prop_kvs.c — property-based tests for kvs (FlashDB key-value store)
 *
 * Requires: property_test.h from tooling/test/
 * Build (from test/ directory):
 *   gcc -std=c99 -I. -I../vendor/inc -I../../../primitives/fal/vendor/inc \
 *       -I../../../../tooling/test \
 *       prop_kvs.c ../vendor/src/fdb.c ../vendor/src/fdb_kvdb.c \
 *       ../vendor/src/fdb_tsdb.c ../vendor/src/fdb_utils.c \
 *       ../../../primitives/fal/vendor/src/fal.c \
 *       ../../../primitives/fal/vendor/src/fal_flash.c \
 *       ../../../primitives/fal/vendor/src/fal_partition.c \
 *       -o prop_kvs
 *
 * Note: must compile from test/ directory so fal_cfg.h and fdb_cfg.h are found.
 */

#include "test_harness.h"
#include "property_test.h"
#include <flashdb.h>
#include <fal.h>
#include <string.h>
#include <stdio.h>

/* --- mock flash device (same as test_kvs.c) ----------------------------- */

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

/* --- helpers ------------------------------------------------------------ */

static struct fdb_kvdb test_kvdb;

static struct fdb_default_kv_node default_kv_table[] = {
    { "boot_count", "0",   0 },
    { "device_id",  "A01", 0 },
};

static void reset_and_init_kvdb(void)
{
    memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
    fal_init();

    struct fdb_default_kv default_kv;
    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

    memset(&test_kvdb, 0, sizeof(test_kvdb));
    uint32_t sec_size = MOCK_BLOCK_SIZE;
    fdb_kvdb_control(&test_kvdb, FDB_KVDB_CTRL_SET_SEC_SIZE, &sec_size);
    fdb_kvdb_init(&test_kvdb, "test", "kvdb", &default_kv, NULL);
}

/* --- properties --------------------------------------------------------- */

/* Property: set then get roundtrip — always returns what was written */
PROPERTY(set_get_roundtrip, 5000) {
    reset_and_init_kvdb();

    /* Generate a random key (1-15 chars, alphanumeric) */
    char key[16];
    unsigned klen = (unsigned)ARBI_RANGE(seed, 1, 15);
    for (unsigned i = 0; i < klen; i++) {
        key[i] = (char)('a' + (ARBI_UINT8(seed) % 26));
    }
    key[klen] = '\0';

    /* Generate a random value (1-31 chars, printable) */
    char val[32];
    unsigned vlen = (unsigned)ARBI_RANGE(seed, 1, 31);
    for (unsigned i = 0; i < vlen; i++) {
        val[i] = (char)('0' + (ARBI_UINT8(seed) % 75));
        /* Keep it printable and not NUL */
        if (val[i] < '!' || val[i] > '~') val[i] = 'x';
    }
    val[vlen] = '\0';

    fdb_err_t err = fdb_kv_set(&test_kvdb, key, val);
    PROP_ASSERT(err == FDB_NO_ERR);

    char *got = fdb_kv_get(&test_kvdb, key);
    PROP_ASSERT(got != NULL);
    PROP_ASSERT(strcmp(got, val) == 0);
}

/* Property: determinism — same key/value ops produce same results */
PROPERTY(deterministic_ops, 5000) {
    char results[2][64];

    for (int pass = 0; pass < 2; pass++) {
        reset_and_init_kvdb();

        uint32_t local_seed = seed;
        char key[8];
        unsigned klen = (unsigned)ARBI_RANGE(local_seed, 1, 7);
        for (unsigned i = 0; i < klen; i++) {
            key[i] = (char)('a' + (ARBI_UINT8(local_seed) % 26));
        }
        key[klen] = '\0';

        char val[16];
        unsigned vlen = (unsigned)ARBI_RANGE(local_seed, 1, 15);
        for (unsigned i = 0; i < vlen; i++) {
            val[i] = (char)('a' + (ARBI_UINT8(local_seed) % 26));
        }
        val[vlen] = '\0';

        fdb_kv_set(&test_kvdb, key, val);
        char *got = fdb_kv_get(&test_kvdb, key);
        if (got) {
            strncpy(results[pass], got, 63);
            results[pass][63] = '\0';
        } else {
            results[pass][0] = '\0';
        }
    }
    PROP_ASSERT(strcmp(results[0], results[1]) == 0);
}

/* Property: overwrite always reflects last value */
PROPERTY(overwrite_last_wins, 5000) {
    reset_and_init_kvdb();

    unsigned n = (unsigned)ARBI_RANGE(seed, 2, 8);
    char last_val[16];

    for (unsigned i = 0; i < n; i++) {
        char val[16];
        unsigned vlen = (unsigned)ARBI_RANGE(seed, 1, 15);
        for (unsigned j = 0; j < vlen; j++) {
            val[j] = (char)('a' + (ARBI_UINT8(seed) % 26));
        }
        val[vlen] = '\0';
        fdb_kv_set(&test_kvdb, "shared_key", val);
        memcpy(last_val, val, vlen + 1);
    }

    char *got = fdb_kv_get(&test_kvdb, "shared_key");
    PROP_ASSERT(got != NULL);
    PROP_ASSERT(strcmp(got, last_val) == 0);
}

/* Property: delete makes key unreadable */
PROPERTY(delete_removes_key, 5000) {
    reset_and_init_kvdb();

    char key[8];
    unsigned klen = (unsigned)ARBI_RANGE(seed, 1, 7);
    for (unsigned i = 0; i < klen; i++) {
        key[i] = (char)('a' + (ARBI_UINT8(seed) % 26));
    }
    key[klen] = '\0';

    fdb_kv_set(&test_kvdb, key, "temp_value");
    PROP_ASSERT(fdb_kv_get(&test_kvdb, key) != NULL);

    fdb_kv_del(&test_kvdb, key);
    char *got = fdb_kv_get(&test_kvdb, key);
    PROP_ASSERT(got == NULL);
}

/* Property: blob roundtrip — binary data survives set/get */
PROPERTY(blob_roundtrip, 5000) {
    reset_and_init_kvdb();

    uint8_t data[32];
    unsigned dlen = (unsigned)ARBI_RANGE(seed, 1, 32);
    ARBI_FILL(seed, data, dlen);

    struct fdb_blob blob;
    fdb_err_t err = fdb_kv_set_blob(&test_kvdb, "blob_key",
                                     fdb_blob_make(&blob, data, dlen));
    PROP_ASSERT(err == FDB_NO_ERR);

    uint8_t readback[32];
    memset(readback, 0, sizeof(readback));
    size_t rlen = fdb_kv_get_blob(&test_kvdb, "blob_key",
                                    fdb_blob_make(&blob, readback, sizeof(readback)));
    PROP_ASSERT_EQ(rlen, dlen);
    PROP_ASSERT_MEM_EQ(readback, data, dlen);
}

TEST_MAIN("kvs properties") {
    RUN(set_get_roundtrip);
    RUN(deterministic_ops);
    RUN(overwrite_last_wins);
    RUN(delete_removes_key);
    RUN(blob_roundtrip);
}
TEST_MAIN_END()
