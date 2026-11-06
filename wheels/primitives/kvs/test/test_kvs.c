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

/* A missing partition leaves storage.part NULL, and the failure message
 * of init prints the partition name. */
TEST(test_tsdb_init_missing_partition)
{
    struct fdb_tsdb db;

    memset(&db, 0, sizeof(db));
    ASSERT_EQ(fdb_tsdb_init(&db, "log", "nosuchpart", tsdb_get_time, 64, NULL),
              FDB_PART_NOT_FOUND);
}

/* After a failed init the sector size is 0, so a traversal never ends:
 * on the code this guards, this test hangs instead of failing. */
TEST(test_tsdb_iter_after_failed_init)
{
    struct fdb_tsdb db;
    int n = 0;

    memset(&db, 0, sizeof(db));
    ASSERT_EQ(fdb_tsdb_init(&db, "log", "nosuchpart", tsdb_get_time, 64, NULL),
              FDB_PART_NOT_FOUND);

    fdb_tsl_iter(&db, count_tsl_cb, &n);
    fdb_tsl_iter_reverse(&db, count_tsl_cb, &n);
    fdb_tsl_iter_by_time(&db, 0, 100, count_tsl_cb, &n);
    fdb_tsl_iter_by_time(&db, 100, 0, count_tsl_cb, &n);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(fdb_tsl_query_count(&db, 0, 100, FDB_TSL_WRITE), 0);
}

/* ---------- tsdb on a working database ---------- */

#define TSDB_MAX_LEN  64  /* the max_len init_tsdb passes */
#define TSDB_BIG_LOG  60  /* 76 bytes with its index: over 50 logs a sector */
#define SEEN_MAX      640
/* A 4096-byte sector less its header holds this many of the big logs. */
#define BIG_LOGS_PER_SECTOR_MIN 53
#define BIG_LOGS_PER_SECTOR_MAX 54

struct tsl_seen {
    int count;
    fdb_time_t time[SEEN_MAX];
    int value[SEEN_MAX];
    fdb_tsl_status_t status[SEEN_MAX];
};

/* Note each log as an iterator hands it over, its value read back from flash. */
static int8_t note_tsl_cb(fdb_tsl_t tsl, void *arg)
{
    struct tsl_seen *seen = arg;
    struct fdb_blob blob;
    int value = 0;

    if (seen->count < SEEN_MAX) {
        fdb_blob_read((fdb_db_t)&test_tsdb,
                      fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &value,
                                                         sizeof(value))));
        seen->time[seen->count] = tsl->time;
        seen->value[seen->count] = value;
        seen->status[seen->count] = tsl->status;
        seen->count++;
    }
    return 0;
}

static fdb_err_t fresh_tsdb(void)
{
    reset_flash();
    tsdb_now = 0;
    return init_tsdb();
}

static fdb_err_t append_log(fdb_time_t time, int value, size_t len)
{
    uint8_t log[TSDB_MAX_LEN + 1];
    struct fdb_blob blob;

    memset(log, 0, sizeof(log));
    memcpy(log, &value, sizeof(value));
    return fdb_tsl_append_with_ts(&test_tsdb, fdb_blob_make(&blob, log, len),
                                  time);
}

static fdb_time_t tsdb_last_time(void)
{
    fdb_time_t last = -1;

    fdb_tsdb_control(&test_tsdb, FDB_TSDB_CTRL_GET_LAST_TIME, &last);
    return last;
}

TEST(test_tsdb_iterators_return_logs_in_time_order)
{
    static struct tsl_seen seen;
    struct fdb_blob blob;
    const int n = 5;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    for (int i = 0; i < n; i++) {
        int value = 100 + i;

        ASSERT_EQ(fdb_tsl_append(&test_tsdb,
                                 fdb_blob_make(&blob, &value, sizeof(value))),
                  FDB_NO_ERR);
    }

    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, n);
    for (int i = 0; i < n; i++) {
        ASSERT_EQ(seen.time[i], i + 1);
        ASSERT_EQ(seen.value[i], 100 + i);
        ASSERT_EQ(seen.status[i], FDB_TSL_WRITE);
    }

    seen.count = 0;
    fdb_tsl_iter_reverse(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, n);
    for (int i = 0; i < n; i++) {
        ASSERT_EQ(seen.time[i], n - i);
        ASSERT_EQ(seen.value[i], 100 + n - 1 - i);
    }
}

static int8_t stop_at_third_cb(fdb_tsl_t tsl, void *arg)
{
    (void)tsl;
    return ++*(int *)arg == 3;
}

TEST(test_tsdb_iterators_stop_when_the_callback_asks)
{
    int n;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    for (int i = 1; i <= 5; i++) {
        ASSERT_EQ(append_log(i, i, sizeof(int)), FDB_NO_ERR);
    }

    n = 0;
    fdb_tsl_iter(&test_tsdb, stop_at_third_cb, &n);
    ASSERT_EQ(n, 3);
    n = 0;
    fdb_tsl_iter_reverse(&test_tsdb, stop_at_third_cb, &n);
    ASSERT_EQ(n, 3);
    n = 0;
    fdb_tsl_iter_by_time(&test_tsdb, 1, 5, stop_at_third_cb, &n);
    ASSERT_EQ(n, 3);
    n = 0;
    fdb_tsl_iter_by_time(&test_tsdb, 5, 1, stop_at_third_cb, &n);
    ASSERT_EQ(n, 3);
}

TEST(test_tsdb_refuses_old_times_and_long_logs)
{
    static struct tsl_seen seen;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    ASSERT_EQ(append_log(10, 1, sizeof(int)), FDB_NO_ERR);
    ASSERT_EQ(append_log(10, 2, sizeof(int)), FDB_WRITE_ERR);
    ASSERT_EQ(append_log(9, 3, sizeof(int)), FDB_WRITE_ERR);
    ASSERT_EQ(append_log(11, 4, TSDB_MAX_LEN), FDB_NO_ERR);
    ASSERT_EQ(append_log(12, 5, TSDB_MAX_LEN + 1), FDB_WRITE_ERR);

    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, 2);
    ASSERT_EQ(seen.time[0], 10);
    ASSERT_EQ(seen.value[0], 1);
    ASSERT_EQ(seen.time[1], 11);
    ASSERT_EQ(seen.value[1], 4);
    ASSERT_EQ(tsdb_last_time(), 11);
}

TEST(test_tsdb_appends_with_a_status)
{
    static struct tsl_seen seen;
    struct fdb_blob blob;
    uint8_t too_long[TSDB_MAX_LEN + 1] = {0};
    int value = 7;
    fdb_time_t when = 50;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    ASSERT_EQ(fdb_tsl_and_state_append(&test_tsdb,
                                       fdb_blob_make(&blob, &value, sizeof(value)),
                                       FDB_TSL_USER_STATUS1), FDB_NO_ERR);
    ASSERT_EQ(fdb_tsl_and_state_append_with_ts(&test_tsdb,
                                               fdb_blob_make(&blob, &value, sizeof(value)),
                                               FDB_TSL_USER_STATUS2, &when), FDB_NO_ERR);
    ASSERT_EQ(fdb_tsl_and_state_append_with_ts(&test_tsdb,
                                               fdb_blob_make(&blob, &value, sizeof(value)),
                                               FDB_TSL_USER_STATUS1, &when), FDB_WRITE_ERR);
    ASSERT_EQ(fdb_tsl_and_state_append(&test_tsdb,
                                       fdb_blob_make(&blob, too_long, sizeof(too_long)),
                                       FDB_TSL_USER_STATUS1), FDB_WRITE_ERR);

    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, 2);
    ASSERT_EQ(seen.time[0], 1);
    ASSERT_EQ(seen.status[0], FDB_TSL_USER_STATUS1);
    ASSERT_EQ(seen.value[0], 7);
    ASSERT_EQ(seen.time[1], 50);
    ASSERT_EQ(seen.status[1], FDB_TSL_USER_STATUS2);
}

struct status_change {
    fdb_tsl_status_t status;
    fdb_err_t result;
};

static int8_t set_status_cb(fdb_tsl_t tsl, void *arg)
{
    struct status_change *change = arg;

    change->result = fdb_tsl_set_status(&test_tsdb, tsl, change->status);
    return 0;
}

static fdb_err_t set_log_status(fdb_time_t time, fdb_tsl_status_t status)
{
    struct status_change change = { status, FDB_READ_ERR };

    fdb_tsl_iter_by_time(&test_tsdb, time, time, set_status_cb, &change);
    return change.result;
}

/* Logs 1 to 10: 2, 4 and 6 in USER_STATUS1, 8 deleted, the rest as written. */
static void check_tsdb_counts(void)
{
    ASSERT_EQ(fdb_tsl_query_count_by_status(&test_tsdb, FDB_TSL_WRITE), 6);
    ASSERT_EQ(fdb_tsl_query_count_by_status(&test_tsdb, FDB_TSL_USER_STATUS1), 3);
    ASSERT_EQ(fdb_tsl_query_count_by_status(&test_tsdb, FDB_TSL_DELETED), 1);
    ASSERT_EQ(fdb_tsl_query_count_by_status(&test_tsdb, FDB_TSL_USER_STATUS2), 0);

    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 1, 5, FDB_TSL_WRITE), 3);
    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 1, 5, FDB_TSL_USER_STATUS1), 2);
    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 5, 1, FDB_TSL_WRITE), 3);
    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 6, 6, FDB_TSL_USER_STATUS1), 1);
    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 0, 100, FDB_TSL_DELETED), 1);
    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 11, 20, FDB_TSL_WRITE), 0);
}

TEST(test_tsdb_query_counts_by_time_and_status)
{
    int failures_before = tests_fail;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    for (int i = 1; i <= 10; i++) {
        ASSERT_EQ(append_log(i, i, sizeof(int)), FDB_NO_ERR);
    }
    ASSERT_EQ(set_log_status(2, FDB_TSL_USER_STATUS1), FDB_NO_ERR);
    ASSERT_EQ(set_log_status(4, FDB_TSL_USER_STATUS1), FDB_NO_ERR);
    ASSERT_EQ(set_log_status(6, FDB_TSL_USER_STATUS1), FDB_NO_ERR);
    ASSERT_EQ(set_log_status(8, FDB_TSL_DELETED), FDB_NO_ERR);

    check_tsdb_counts();
    ASSERT_EQ(tests_fail, failures_before);

    /* The status lives in flash, so a reboot keeps it. */
    ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
    check_tsdb_counts();
}

TEST(test_tsdb_clean_forgets_every_log)
{
    static struct tsl_seen seen;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    for (int i = 1; i <= 2 * BIG_LOGS_PER_SECTOR_MAX; i++) {
        ASSERT_EQ(append_log(i, i, TSDB_BIG_LOG), FDB_NO_ERR);
    }

    ASSERT_EQ(fdb_tsl_clean(&test_tsdb), FDB_NO_ERR);

    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    fdb_tsl_iter_reverse(&test_tsdb, note_tsl_cb, &seen);
    fdb_tsl_iter_by_time(&test_tsdb, 0, 1000, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, 0);
    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 0, 1000, FDB_TSL_WRITE), 0);
    ASSERT_EQ(fdb_tsl_query_count_by_status(&test_tsdb, FDB_TSL_WRITE), 0);
    ASSERT_EQ(tsdb_last_time(), 0);

    /* The clock starts again, in this boot and the next. */
    ASSERT_EQ(append_log(1, 42, sizeof(int)), FDB_NO_ERR);
    ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, 1);
    ASSERT_EQ(seen.value[0], 42);
}

TEST(test_tsdb_logs_survive_a_reboot)
{
    static struct tsl_seen seen;
    const int n = BIG_LOGS_PER_SECTOR_MAX + 10; /* into the second sector */

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    for (int i = 1; i <= n; i++) {
        ASSERT_EQ(append_log(i, 1000 + i, TSDB_BIG_LOG), FDB_NO_ERR);
    }

    ASSERT_EQ(fdb_tsdb_deinit(&test_tsdb), FDB_NO_ERR);
    ASSERT_EQ(init_tsdb(), FDB_NO_ERR);

    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, n);
    for (int i = 0; i < n; i++) {
        ASSERT_EQ(seen.time[i], i + 1);
        ASSERT_EQ(seen.value[i], 1000 + i + 1);
    }
    ASSERT_EQ(tsdb_last_time(), n);
    ASSERT_EQ(append_log(n, 0, sizeof(int)), FDB_WRITE_ERR);
    ASSERT_EQ(append_log(n + 1, 0, sizeof(int)), FDB_NO_ERR);
}

TEST(test_tsdb_deinit_stops_the_database)
{
    int n = 0;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    ASSERT_EQ(append_log(1, 1, sizeof(int)), FDB_NO_ERR);
    ASSERT_EQ(fdb_tsdb_deinit(&test_tsdb), FDB_NO_ERR);

    ASSERT_EQ(append_log(2, 2, sizeof(int)), FDB_INIT_FAILED);
    fdb_tsl_iter(&test_tsdb, count_tsl_cb, &n);
    fdb_tsl_iter_reverse(&test_tsdb, count_tsl_cb, &n);
    fdb_tsl_iter_by_time(&test_tsdb, 0, 10, count_tsl_cb, &n);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(fdb_tsl_query_count(&test_tsdb, 0, 10, FDB_TSL_WRITE), 0);
    ASSERT_EQ(fdb_tsl_query_count_by_status(&test_tsdb, FDB_TSL_WRITE), 0);
}

/* The logs left must be the newest ones, whole, in order, both ways round. */
static void check_newest_logs_left(int total)
{
    static struct tsl_seen seen;

    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_GE(seen.count, (TSDB_SECTOR_COUNT - 1) * BIG_LOGS_PER_SECTOR_MIN);
    ASSERT_LE(seen.count, TSDB_SECTOR_COUNT * BIG_LOGS_PER_SECTOR_MAX);
    for (int i = 0; i < seen.count; i++) {
        ASSERT_EQ(seen.time[i], total - seen.count + 1 + i);
        ASSERT_EQ(seen.value[i], seen.time[i]);
    }

    int kept = seen.count;

    seen.count = 0;
    fdb_tsl_iter_reverse(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, kept);
    for (int i = 0; i < kept; i++) {
        ASSERT_EQ(seen.time[i], total - i);
    }
}

TEST(test_tsdb_rollover_keeps_the_newest_logs)
{
    const int total = 7 * BIG_LOGS_PER_SECTOR_MAX;
    int failures_before = tests_fail;
    bool rollover = false;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    fdb_tsdb_control(&test_tsdb, FDB_TSDB_CTRL_GET_ROLLOVER, &rollover);
    ASSERT_TRUE(rollover);

    for (int i = 1; i <= total; i++) {
        ASSERT_EQ(append_log(i, i, TSDB_BIG_LOG), FDB_NO_ERR);
    }
    check_newest_logs_left(total);
    ASSERT_EQ(tests_fail, failures_before);

    ASSERT_EQ(init_tsdb(), FDB_NO_ERR);
    check_newest_logs_left(total);
    ASSERT_EQ(tests_fail, failures_before);

    ASSERT_EQ(tsdb_last_time(), total);
    ASSERT_EQ(append_log(total + 1, total + 1, TSDB_BIG_LOG), FDB_NO_ERR);
    check_newest_logs_left(total + 1);
}

/* Fill the database with rollover off, and say how many logs it took. */
static int fill_tsdb_without_rollover(void)
{
    bool rollover = false;
    int stored = 0;

    fdb_tsdb_control(&test_tsdb, FDB_TSDB_CTRL_SET_ROLLOVER, &rollover);
    while (stored < SEEN_MAX &&
           append_log(stored + 1, stored + 1, TSDB_BIG_LOG) == FDB_NO_ERR) {
        stored++;
    }
    return stored;
}

TEST(test_tsdb_without_rollover_reports_full)
{
    static struct tsl_seen seen;

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    int stored = fill_tsdb_without_rollover();

    ASSERT_GE(stored, TSDB_SECTOR_COUNT * BIG_LOGS_PER_SECTOR_MIN);
    ASSERT_LE(stored, TSDB_SECTOR_COUNT * BIG_LOGS_PER_SECTOR_MAX);
    ASSERT_EQ(append_log(stored + 1, 0, TSDB_BIG_LOG), FDB_SAVED_FULL);
    ASSERT_EQ(append_log(stored + 2, 0, TSDB_BIG_LOG), FDB_SAVED_FULL);

    seen.count = 0;
    fdb_tsl_iter(&test_tsdb, note_tsl_cb, &seen);
    ASSERT_EQ(seen.count, stored);
    ASSERT_EQ(seen.time[0], 1);
    ASSERT_EQ(seen.time[stored - 1], stored);
}

/* Logs at the even times 2, 4 ... 2n. Compare every window with what the
 * header promises, worked out from the times alone: the logs from..to, oldest
 * first, or newest first when from is above to. Returns the windows that
 * differ. */
static int by_time_windows_wrong(int n, int to_step)
{
    static struct tsl_seen seen;
    int top = 2 * n + 2;
    int wrong = 0;

    for (int from = 0; from <= top; from++) {
        for (int to = 0; to <= top; to += to_step) {
            int low = from <= to ? from : to;
            int high = from <= to ? to : from;
            int first = (low + 1) / 2 < 1 ? 1 : (low + 1) / 2;
            int last = high / 2 > n ? n : high / 2;
            int want = last >= first ? last - first + 1 : 0;
            bool same;

            seen.count = 0;
            fdb_tsl_iter_by_time(&test_tsdb, from, to, note_tsl_cb, &seen);
            same = seen.count == want;
            for (int i = 0; same && i < want; i++) {
                int log = from <= to ? first + i : last - i;

                same = seen.time[i] == 2 * log && seen.value[i] == log;
            }
            if (!same) {
                wrong++;
            }
        }
    }
    return wrong;
}

static void fill_even_times(int n, size_t len)
{
    for (int i = 1; i <= n; i++) {
        ASSERT_EQ(append_log(2 * i, i, len), FDB_NO_ERR);
    }
}

TEST(test_tsdb_by_time_returns_the_window_across_sectors)
{
    const int n = 5 * BIG_LOGS_PER_SECTOR_MIN / 2; /* two and a half sectors */

    ASSERT_EQ(fresh_tsdb(), FDB_NO_ERR);
    fill_even_times(n, TSDB_BIG_LOG);
    ASSERT_EQ(by_time_windows_wrong(n, 3), 0);
}

/* ---------- kv objects and the integrity check ---------- */

#define KVDB_PART_ADDR 4096 /* the "kvdb" partition in fal_cfg.h */

TEST(test_kv_object_reads_back_through_a_blob)
{
    struct fdb_kv kv;
    struct fdb_blob blob;
    char buf[16] = {0};
    char small[3] = {0};

    reset_flash();
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set(&test_kvdb, "name", "value1"), FDB_NO_ERR);

    fdb_kv_t obj = fdb_kv_get_obj(&test_kvdb, "name", &kv);

    ASSERT_TRUE(obj == &kv);
    ASSERT_EQ(kv.status, FDB_KV_WRITE);
    ASSERT_TRUE(kv.crc_is_ok);
    ASSERT_EQ(kv.name_len, 4);
    ASSERT_EQ(kv.value_len, 6);
    ASSERT_STR_EQ(kv.name, "name");

    ASSERT_EQ(fdb_blob_read((fdb_db_t)&test_kvdb,
                            fdb_kv_to_blob(obj, fdb_blob_make(&blob, buf, sizeof(buf)))),
              6);
    ASSERT_STR_EQ(buf, "value1");

    /* A short buffer takes the start of the value and no more. */
    ASSERT_EQ(fdb_blob_read((fdb_db_t)&test_kvdb,
                            fdb_kv_to_blob(obj, fdb_blob_make(&blob, small, sizeof(small)))),
              3);
    ASSERT_MEMEQ_N(small, "val", 3);
}

TEST(test_kv_object_of_a_binary_blob)
{
    static const uint8_t data[] = { 0x00, 0xFF, 0x7F, 0x80, 0x00 };
    struct fdb_kv kv;
    struct fdb_blob blob;
    uint8_t buf[8] = {0};

    reset_flash();
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set_blob(&test_kvdb, "bin",
                              fdb_blob_make(&blob, data, sizeof(data))), FDB_NO_ERR);

    ASSERT_TRUE(fdb_kv_get_obj(&test_kvdb, "bin", &kv) == &kv);
    ASSERT_EQ(kv.value_len, sizeof(data));
    ASSERT_EQ(fdb_blob_read((fdb_db_t)&test_kvdb,
                            fdb_kv_to_blob(&kv, fdb_blob_make(&blob, buf, sizeof(buf)))),
              sizeof(data));
    ASSERT_MEMEQ_N(buf, data, sizeof(data));
}

TEST(test_kv_object_of_an_absent_key)
{
    struct fdb_kv kv;

    reset_flash();
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_NULL(fdb_kv_get_obj(&test_kvdb, "never_set", &kv));

    ASSERT_EQ(fdb_kv_set(&test_kvdb, "short_lived", "x"), FDB_NO_ERR);
    ASSERT_TRUE(fdb_kv_get_obj(&test_kvdb, "short_lived", &kv) == &kv);
    ASSERT_EQ(fdb_kv_del(&test_kvdb, "short_lived"), FDB_NO_ERR);
    ASSERT_NULL(fdb_kv_get_obj(&test_kvdb, "short_lived", &kv));
}

TEST(test_kvdb_check_passes_a_good_database)
{
    char value[101];

    reset_flash();
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_EQ(fdb_kvdb_check(&test_kvdb), FDB_NO_ERR);

    /* Overwrites, a delete, and enough data to make the collector run. */
    memset(value, 'v', sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    for (int i = 0; i < 600; i++) {
        value[0] = (char)('a' + i % 26);
        ASSERT_EQ(fdb_kv_set(&test_kvdb, "busy", value), FDB_NO_ERR);
    }
    ASSERT_EQ(fdb_kv_del(&test_kvdb, "device_id"), FDB_NO_ERR);
    ASSERT_EQ(fdb_kvdb_check(&test_kvdb), FDB_NO_ERR);
    ASSERT_STR_EQ(fdb_kv_get(&test_kvdb, "busy"), value);
}

TEST(test_kvdb_check_finds_a_damaged_value)
{
    struct fdb_kv kv;

    reset_flash();
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set(&test_kvdb, "name", "value1"), FDB_NO_ERR);
    ASSERT_TRUE(fdb_kv_get_obj(&test_kvdb, "name", &kv) == &kv);

    mock_flash_buf[KVDB_PART_ADDR + kv.addr.value] ^= 0x01;

    ASSERT_TRUE(fdb_kvdb_check(&test_kvdb) != FDB_NO_ERR);
}

TEST(test_kvdb_check_after_a_failed_init)
{
    struct fdb_kvdb db;

    memset(&db, 0, sizeof(db));
    ASSERT_EQ(fdb_kvdb_init(&db, "test", "nosuchpart", NULL, NULL),
              FDB_PART_NOT_FOUND);
    ASSERT_EQ(fdb_kvdb_check(&db), FDB_INIT_FAILED);
}

TEST(test_kv_set_default_restores_the_table)
{
    reset_flash();
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set(&test_kvdb, "boot_count", "42"), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set(&test_kvdb, "extra", "x"), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_del(&test_kvdb, "device_id"), FDB_NO_ERR);

    ASSERT_EQ(fdb_kv_set_default(&test_kvdb), FDB_NO_ERR);

    ASSERT_STR_EQ(fdb_kv_get(&test_kvdb, "boot_count"), "0");
    ASSERT_STR_EQ(fdb_kv_get(&test_kvdb, "device_id"), "A01");
    ASSERT_NULL(fdb_kv_get(&test_kvdb, "extra"));
    ASSERT_EQ(fdb_kvdb_check(&test_kvdb), FDB_NO_ERR);

    /* And the table still holds after a reboot, with room to write. */
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_STR_EQ(fdb_kv_get(&test_kvdb, "boot_count"), "0");
    ASSERT_NULL(fdb_kv_get(&test_kvdb, "extra"));
    ASSERT_EQ(fdb_kv_set(&test_kvdb, "extra", "y"), FDB_NO_ERR);
    ASSERT_STR_EQ(fdb_kv_get(&test_kvdb, "extra"), "y");
}

TEST(test_kv_print_leaves_the_database_alone)
{
    static const uint8_t data[] = { 0x00, 0xFF, 0x7F };
    static uint8_t before[MOCK_FLASH_SIZE];
    struct fdb_blob blob;

    reset_flash();
    ASSERT_EQ(init_kvdb(), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set(&test_kvdb, "text", "hello"), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set(&test_kvdb, "gone", "x"), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_del(&test_kvdb, "gone"), FDB_NO_ERR);
    ASSERT_EQ(fdb_kv_set_blob(&test_kvdb, "bin",
                              fdb_blob_make(&blob, data, sizeof(data))), FDB_NO_ERR);

    memcpy(before, mock_flash_buf, sizeof(before));
    fdb_kv_print(&test_kvdb);
    ASSERT_MEMEQ_N(mock_flash_buf, before, sizeof(before));
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
    RUN(test_tsdb_iter_after_failed_init);
    RUN(test_tsdb_init_missing_partition);
    RUN(test_tsdb_iterators_return_logs_in_time_order);
    RUN(test_tsdb_iterators_stop_when_the_callback_asks);
    RUN(test_tsdb_refuses_old_times_and_long_logs);
    RUN(test_tsdb_appends_with_a_status);
    RUN(test_tsdb_query_counts_by_time_and_status);
    RUN(test_tsdb_clean_forgets_every_log);
    RUN(test_tsdb_logs_survive_a_reboot);
    RUN(test_tsdb_deinit_stops_the_database);
    RUN(test_tsdb_rollover_keeps_the_newest_logs);
    RUN(test_tsdb_without_rollover_reports_full);
    RUN(test_tsdb_by_time_returns_the_window_across_sectors);
    RUN(test_kv_object_reads_back_through_a_blob);
    RUN(test_kv_object_of_a_binary_blob);
    RUN(test_kv_object_of_an_absent_key);
    RUN(test_kvdb_check_passes_a_good_database);
    RUN(test_kvdb_check_finds_a_damaged_value);
    RUN(test_kvdb_check_after_a_failed_init);
    RUN(test_kv_set_default_restores_the_table);
    RUN(test_kv_print_leaves_the_database_alone);
TEST_MAIN_END()

