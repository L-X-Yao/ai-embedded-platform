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
        mock_flash_buf[(size_t)offset + i] &= buf[i];
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

/* ---------- records on flash ---------- */

/* struct kv_hdr_data at FDB_WRITE_GRAN 1, counted from the magic word. */
#define REC_LEN_OFF        4u
#define REC_CRC_OFF        8u
#define REC_NAME_LEN_OFF  12u
#define REC_VALUE_LEN_OFF 16u
#define REC_DATA_OFF      20u   /* name, then value */
#define REC_HDR_SIZE      24u   /* the record starts 4 bytes before the magic */

static uint8_t *find_record(const char *key)
{
    static const uint8_t magic[4] = { 'K', 'V', '0', '0' };
    size_t key_len = strlen(key);

    for (size_t i = 0; i + REC_DATA_OFF + key_len < MOCK_FLASH_SIZE; i++) {
        if (memcmp(&mock_flash_buf[i], magic, sizeof(magic)) == 0 &&
            memcmp(&mock_flash_buf[i + REC_DATA_OFF], key, key_len) == 0) {
            return &mock_flash_buf[i];
        }
    }
    return NULL;
}

static uint32_t record_crc(const uint8_t *rec)
{
    uint32_t len;
    uint32_t crc;

    memcpy(&len, &rec[REC_LEN_OFF], sizeof(len));
    crc = fdb_calc_crc32(0, &rec[REC_NAME_LEN_OFF], sizeof(uint32_t));
    crc = fdb_calc_crc32(crc, &rec[REC_VALUE_LEN_OFF], sizeof(uint32_t));
    return fdb_calc_crc32(crc, &rec[REC_DATA_OFF], len - REC_HDR_SIZE);
}

/* A record with a valid CRC and a name longer than this build allows: what
 * a build with a larger FDB_KV_NAME_MAX leaves on flash. */
static void check_long_name_record_is_skipped(uint8_t name_len)
{
    uint8_t value[200];
    struct fdb_blob blob;
    uint32_t stored_crc;
    uint32_t crc;

    memset(value, 'v', sizeof(value));
    reset_flash();
    init_kvdb();
    fdb_kv_set(&test_kvdb, "before", "1");
    fdb_kv_set_blob(&test_kvdb, "forged", fdb_blob_make(&blob, value, sizeof(value)));
    fdb_kv_set(&test_kvdb, "after", "2");
    fdb_kvdb_deinit(&test_kvdb);

    uint8_t *rec = find_record("forged");
    ASSERT_TRUE(rec != NULL);

    /* record_crc() has to agree with the library before it can forge. */
    memcpy(&stored_crc, &rec[REC_CRC_OFF], sizeof(stored_crc));
    ASSERT_EQ_HEX(record_crc(rec), stored_crc);

    rec[REC_NAME_LEN_OFF] = name_len;
    crc = record_crc(rec);
    memcpy(&rec[REC_CRC_OFF], &crc, sizeof(crc));

    ASSERT_TRUE(init_kvdb() == FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_get_blob(&test_kvdb, "forged",
                              fdb_blob_make(&blob, value, sizeof(value))), 0);

    /* Its neighbours survive, and the database stays usable across a boot. */
    char *val = fdb_kv_get(&test_kvdb, "before");
    ASSERT_TRUE(val != NULL && strcmp(val, "1") == 0);
    val = fdb_kv_get(&test_kvdb, "after");
    ASSERT_TRUE(val != NULL && strcmp(val, "2") == 0);

    ASSERT_TRUE(fdb_kv_set(&test_kvdb, "later", "3") == FDB_NO_ERR);
    fdb_kvdb_deinit(&test_kvdb);
    ASSERT_TRUE(init_kvdb() == FDB_NO_ERR);
    val = fdb_kv_get(&test_kvdb, "later");
    ASSERT_TRUE(val != NULL && strcmp(val, "3") == 0);
    val = fdb_kv_get(&test_kvdb, "after");
    ASSERT_TRUE(val != NULL && strcmp(val, "2") == 0);
}

TEST(test_record_name_too_long)
{
    check_long_name_record_is_skipped(FDB_KV_NAME_MAX + 1);
    check_long_name_record_is_skipped(180);
    check_long_name_record_is_skipped(255);
}

/* fdb_def.h: "the KV max name length must less then it". */
TEST(test_key_at_name_max_is_refused)
{
    char key[FDB_KV_NAME_MAX + 2];
    uint32_t value = 64;
    struct fdb_blob blob;

    reset_flash();
    init_kvdb();

    memset(key, 'k', sizeof(key));
    key[FDB_KV_NAME_MAX] = '\0';
    ASSERT_TRUE(fdb_kv_set_blob(&test_kvdb, key,
                                fdb_blob_make(&blob, &value, sizeof(value))) == FDB_KV_NAME_ERR);
    ASSERT_TRUE(find_record(key) == NULL);

    key[FDB_KV_NAME_MAX] = 'k';
    key[FDB_KV_NAME_MAX + 1] = '\0';
    ASSERT_TRUE(fdb_kv_set_blob(&test_kvdb, key,
                                fdb_blob_make(&blob, &value, sizeof(value))) == FDB_KV_NAME_ERR);
}

TEST(test_every_accepted_key_survives_reboot)
{
    char key[FDB_KV_NAME_MAX + 2];
    uint32_t value = 7;
    uint32_t read_back = 0;
    struct fdb_blob blob;

    for (size_t len = FDB_KV_NAME_MAX - 2; len <= FDB_KV_NAME_MAX + 1; len++) {
        reset_flash();
        init_kvdb();

        memset(key, 'k', sizeof(key));
        key[len] = '\0';
        fdb_err_t err = fdb_kv_set_blob(&test_kvdb, key,
                                        fdb_blob_make(&blob, &value, sizeof(value)));
        fdb_kvdb_deinit(&test_kvdb);
        if (err != FDB_NO_ERR) {
            continue;
        }

        ASSERT_TRUE(init_kvdb() == FDB_NO_ERR);
        ASSERT_EQ(fdb_kv_get_blob(&test_kvdb, key,
                                  fdb_blob_make(&blob, &read_back, sizeof(read_back))),
                  sizeof(read_back));
    }
}

TEST(test_longest_key_survives_reboot)
{
    char key[FDB_KV_NAME_MAX];
    uint32_t value = 63;
    uint32_t read_back = 0;
    struct fdb_blob blob;

    reset_flash();
    init_kvdb();

    memset(key, 'k', sizeof(key));
    key[FDB_KV_NAME_MAX - 1] = '\0';
    ASSERT_TRUE(fdb_kv_set_blob(&test_kvdb, key,
                                fdb_blob_make(&blob, &value, sizeof(value))) == FDB_NO_ERR);
    fdb_kvdb_deinit(&test_kvdb);

    ASSERT_TRUE(init_kvdb() == FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_get_blob(&test_kvdb, key,
                              fdb_blob_make(&blob, &read_back, sizeof(read_back))),
              sizeof(read_back));
    ASSERT_EQ(read_back, 63);

    /* Setting it again replaces the record, and deleting it removes it. */
    value = 64;
    ASSERT_TRUE(fdb_kv_set_blob(&test_kvdb, key,
                                fdb_blob_make(&blob, &value, sizeof(value))) == FDB_NO_ERR);
    ASSERT_TRUE(fdb_kv_del(&test_kvdb, key) == FDB_NO_ERR);
    fdb_kvdb_deinit(&test_kvdb);

    ASSERT_TRUE(init_kvdb() == FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_get_blob(&test_kvdb, key,
                              fdb_blob_make(&blob, &read_back, sizeof(read_back))), 0);
}

/* ---------- main ---------- */

/* ---------- tsdb ---------- */

#define TSDB_PART_ADDR    53248 /* the "tsdb" partition in fal_cfg.h */
#define TSDB_SECTOR_COUNT 3

static struct fdb_tsdb test_tsdb;
static fdb_time_t tsdb_now;

static fdb_time_t tsdb_get_time(void)
{
    return ++tsdb_now;
}

static fdb_err_t init_tsdb(void)
{
    uint32_t sec_size = MOCK_BLOCK_SIZE;

    memset(&test_tsdb, 0, sizeof(test_tsdb));
    fdb_tsdb_control(&test_tsdb, FDB_TSDB_CTRL_SET_SEC_SIZE, &sec_size);
    return fdb_tsdb_init(&test_tsdb, "log", "tsdb", tsdb_get_time, 64, NULL);
}

/* What a power cut between erase and header write leaves. */
static void blank_tsdb_sector(int n)
{
    memset(mock_flash_buf + TSDB_PART_ADDR + n * MOCK_BLOCK_SIZE, 0xFF,
           MOCK_BLOCK_SIZE);
}

static bool tsdb_sector_has_header(int n)
{
    const uint8_t *hdr = mock_flash_buf + TSDB_PART_ADDR + n * MOCK_BLOCK_SIZE;

    for (size_t i = 0; i < 32; i++) {
        if (hdr[i] != 0xFF) {
            return true;
        }
    }
    return false;
}

static int8_t count_tsl_cb(fdb_tsl_t tsl, void *arg)
{
    (void)tsl;
    (*(int *)arg)++;
    return 0; /* carry on */
}

static int tsdb_count(void)
{
    int n = 0;

    fdb_tsl_iter(&test_tsdb, count_tsl_cb, &n);
    return n;
}

/* The format that repairs a bad header must not skip it, whatever sector
 * came before it, or the logs written next are lost at the following boot. */
TEST(test_tsdb_init_repairs_blank_sector)
{
    for (int bad = 0; bad < TSDB_SECTOR_COUNT; bad++) {
        struct fdb_blob blob;
        int val = 7;

        reset_flash();
        tsdb_now = 0;
        ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
        blank_tsdb_sector(bad);
        ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
        ASSERT_TRUE(tsdb_sector_has_header(bad));

        for (int i = 0; i < 10; i++) {
            ASSERT_EQ(fdb_tsl_append(&test_tsdb,
                                     fdb_blob_make(&blob, &val, sizeof(val))),
                      FDB_NO_ERR);
        }
        ASSERT_EQ(tsdb_count(), 10);
        ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
        ASSERT_EQ(tsdb_count(), 10);
    }
}

TEST(test_tsdb_clean_repairs_blank_sector)
{
    for (int bad = 0; bad < TSDB_SECTOR_COUNT; bad++) {
        reset_flash();
        tsdb_now = 0;
        ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
        blank_tsdb_sector(bad);
        ASSERT_EQ(fdb_tsl_clean(&test_tsdb), FDB_NO_ERR);
        ASSERT_TRUE(tsdb_sector_has_header(bad));
    }
}

static void note_percent(uint8_t percent, void *arg)
{
    *(uint8_t *)arg = percent;
}

TEST(test_tsdb_clean_progress_repairs_blank_sector)
{
    for (int bad = 0; bad < TSDB_SECTOR_COUNT; bad++) {
        uint8_t percent = 0;

        reset_flash();
        tsdb_now = 0;
        ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
        blank_tsdb_sector(bad);
        ASSERT_EQ(fdb_tsl_clean_with_progress(&test_tsdb, note_percent, &percent),
                  FDB_NO_ERR);
        ASSERT_EQ(percent, 100);
        ASSERT_TRUE(tsdb_sector_has_header(bad));
    }
}

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
    RUN(test_record_name_too_long);
    RUN(test_key_at_name_max_is_refused);
    RUN(test_every_accepted_key_survives_reboot);
    RUN(test_longest_key_survives_reboot);
    RUN(test_tsdb_init_repairs_blank_sector);
    RUN(test_tsdb_clean_repairs_blank_sector);
    RUN(test_tsdb_clean_progress_repairs_blank_sector);
TEST_MAIN_END()

