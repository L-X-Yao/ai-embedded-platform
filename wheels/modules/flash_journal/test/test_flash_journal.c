// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "flash_journal.h"

/* ---------- Simulated flash storage --------------------------------------- */

#define SIM_SECTOR_COUNT  4
#define SIM_SECTOR_SIZE   128
#define SIM_FLASH_SIZE    (SIM_SECTOR_COUNT * SIM_SECTOR_SIZE)

static uint8_t sim_flash[SIM_FLASH_SIZE];
static int sim_read_calls;
static int sim_write_calls;
static int sim_erase_calls;
static bool sim_fail_read;
static bool sim_fail_write;
static bool sim_fail_erase;

static void sim_reset(void)
{
    memset(sim_flash, 0xFF, sizeof(sim_flash));
    sim_read_calls = 0;
    sim_write_calls = 0;
    sim_erase_calls = 0;
    sim_fail_read = false;
    sim_fail_write = false;
    sim_fail_erase = false;
}

static flash_journal_err_t sim_read(void *ctx, uint32_t sector,
                                    uint32_t offset, uint8_t *buf,
                                    uint32_t len)
{
    (void)ctx;
    sim_read_calls++;
    if (sim_fail_read) return FLASH_JOURNAL_ERR_STORAGE;
    if (sector >= SIM_SECTOR_COUNT) return FLASH_JOURNAL_ERR_PARAM;
    if (offset + len > SIM_SECTOR_SIZE) return FLASH_JOURNAL_ERR_PARAM;
    memcpy(buf, &sim_flash[sector * SIM_SECTOR_SIZE + offset], len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t sim_write(void *ctx, uint32_t sector,
                                     uint32_t offset, const uint8_t *data,
                                     uint32_t len)
{
    (void)ctx;
    sim_write_calls++;
    if (sim_fail_write) return FLASH_JOURNAL_ERR_STORAGE;
    if (sector >= SIM_SECTOR_COUNT) return FLASH_JOURNAL_ERR_PARAM;
    if (offset + len > SIM_SECTOR_SIZE) return FLASH_JOURNAL_ERR_PARAM;
    memcpy(&sim_flash[sector * SIM_SECTOR_SIZE + offset], data, len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t sim_erase(void *ctx, uint32_t sector)
{
    (void)ctx;
    sim_erase_calls++;
    if (sim_fail_erase) return FLASH_JOURNAL_ERR_STORAGE;
    if (sector >= SIM_SECTOR_COUNT) return FLASH_JOURNAL_ERR_PARAM;
    memset(&sim_flash[sector * SIM_SECTOR_SIZE], 0xFF, SIM_SECTOR_SIZE);
    return FLASH_JOURNAL_OK;
}

static const flash_journal_storage_t sim_storage = {
    .read  = sim_read,
    .write = sim_write,
    .erase = sim_erase,
    .ctx   = NULL,
};

/* ---------- Helper to init a fresh journal -------------------------------- */

static flash_journal_t jrnl;

static void fresh_journal(void)
{
    sim_reset();
    flash_journal_init(&jrnl, &sim_storage, SIM_SECTOR_COUNT, SIM_SECTOR_SIZE);
}

/* ---------- Tests --------------------------------------------------------- */

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
    flash_journal_t zj;
    memset(&zj, 0, sizeof(zj));
    ASSERT_EQ(flash_journal_count(&zj), 0);
}

TEST(test_init_null_params)
{
    flash_journal_t j;
    ASSERT_EQ(flash_journal_init(NULL, &sim_storage, 4, 128),
              FLASH_JOURNAL_ERR_PARAM);
    ASSERT_EQ(flash_journal_init(&j, NULL, 4, 128),
              FLASH_JOURNAL_ERR_PARAM);
}

TEST(test_init_bad_geometry)
{
    flash_journal_t j;
    /* zero sectors */
    ASSERT_EQ(flash_journal_init(&j, &sim_storage, 0, 128),
              FLASH_JOURNAL_ERR_PARAM);
    /* too many sectors */
    ASSERT_EQ(flash_journal_init(&j, &sim_storage,
              FLASH_JOURNAL_MAX_SECTORS + 1, 128),
              FLASH_JOURNAL_ERR_PARAM);
    /* sector too small for overhead */
    ASSERT_EQ(flash_journal_init(&j, &sim_storage, 4,
              FLASH_JOURNAL_OVERHEAD),
              FLASH_JOURNAL_ERR_PARAM);
}

TEST(test_init_empty_flash)
{
    sim_reset();
    flash_journal_t j;
    ASSERT_EQ(flash_journal_init(&j, &sim_storage, SIM_SECTOR_COUNT,
              SIM_SECTOR_SIZE), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&j), 0);
}

TEST(test_append_and_read_latest)
{
    fresh_journal();
    const uint8_t data[] = "hello";
    ASSERT_EQ(flash_journal_append(&jrnl, data, 5), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 1);

    uint8_t buf[32];
    uint32_t size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_latest(&jrnl, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(size, 5);
    ASSERT_TRUE(memcmp(buf, "hello", 5) == 0);
}

TEST(test_append_multiple_read_latest)
{
    fresh_journal();
    const uint8_t d1[] = "first";
    const uint8_t d2[] = "second";
    flash_journal_append(&jrnl, d1, 5);
    flash_journal_append(&jrnl, d2, 6);

    ASSERT_EQ(flash_journal_count(&jrnl), 2);

    uint8_t buf[32];
    uint32_t size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_latest(&jrnl, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(size, 6);
    ASSERT_TRUE(memcmp(buf, "second", 6) == 0);
}

TEST(test_read_nth)
{
    fresh_journal();
    const uint8_t d1[] = {0xAA};
    const uint8_t d2[] = {0xBB};
    const uint8_t d3[] = {0xCC};
    flash_journal_append(&jrnl, d1, 1);
    flash_journal_append(&jrnl, d2, 1);
    flash_journal_append(&jrnl, d3, 1);

    uint8_t buf[8];
    uint32_t size;

    size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_nth(&jrnl, 0, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(buf[0], 0xAA);

    size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_nth(&jrnl, 1, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(buf[0], 0xBB);

    size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_nth(&jrnl, 2, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(buf[0], 0xCC);

    /* out of bounds */
    size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_nth(&jrnl, 3, buf, &size),
              FLASH_JOURNAL_ERR_PARAM);
}

TEST(test_read_empty_journal)
{
    fresh_journal();
    uint8_t buf[16];
    uint32_t size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_latest(&jrnl, buf, &size),
              FLASH_JOURNAL_ERR_EMPTY);
}

TEST(test_erase_all)
{
    fresh_journal();
    const uint8_t data[] = "test";
    flash_journal_append(&jrnl, data, 4);
    flash_journal_append(&jrnl, data, 4);
    ASSERT_EQ(flash_journal_count(&jrnl), 2);

    ASSERT_EQ(flash_journal_erase_all(&jrnl), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 0);

    uint8_t buf[16];
    uint32_t size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_latest(&jrnl, buf, &size),
              FLASH_JOURNAL_ERR_EMPTY);
}

TEST(test_sector_wraparound)
{
    fresh_journal();

    /* Each entry: 14 overhead + 10 payload = 24 bytes.
     * sector_size=128 => floor(128/24)=5 per sector.
     * 4 sectors => 20 total slots. Appending 25 forces wraparound. */
    uint8_t payload[10];

    for (int i = 0; i < 25; i++) {
        memset(payload, (uint8_t)i, sizeof(payload));
        ASSERT_EQ(flash_journal_append(&jrnl, payload, 10), FLASH_JOURNAL_OK);
    }

    /* After wraparound, count < 25 (oldest entries were overwritten) */
    uint32_t count = flash_journal_count(&jrnl);
    ASSERT_TRUE(count > 0);
    ASSERT_TRUE(count <= 20);  /* at most 4 sectors * 5 entries */

    /* Latest should be readable and have the last pattern */
    uint8_t buf[16];
    uint32_t size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_latest(&jrnl, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(size, 10);
    /* Verify last entry has pattern 24 (0-based) */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(buf[i], 24);
    }
}

TEST(test_wraparound_overwrites_oldest)
{
    fresh_journal();

    /* Fill all 4 sectors, then keep appending to force overwrite */
    uint8_t payload[10];
    for (int i = 0; i < 30; i++) {
        memset(payload, (uint8_t)i, sizeof(payload));
        flash_journal_append(&jrnl, payload, 10);
    }

    /* Count should be less than 30 (oldest entries were overwritten) */
    uint32_t count = flash_journal_count(&jrnl);
    ASSERT_TRUE(count > 0);
    ASSERT_TRUE(count < 30);

    /* All remaining entries should be readable */
    uint8_t buf[16];
    for (uint32_t n = 0; n < count; n++) {
        uint32_t size = sizeof(buf);
        ASSERT_EQ(flash_journal_read_nth(&jrnl, n, buf, &size),
                  FLASH_JOURNAL_OK);
        ASSERT_EQ(size, 10);
    }
}

TEST(test_init_recovers_state)
{
    fresh_journal();
    const uint8_t d1[] = "alpha";
    const uint8_t d2[] = "beta";
    flash_journal_append(&jrnl, d1, 5);
    flash_journal_append(&jrnl, d2, 4);

    /* Re-init from same flash content (simulating power cycle) */
    flash_journal_t j2;
    ASSERT_EQ(flash_journal_init(&j2, &sim_storage, SIM_SECTOR_COUNT,
              SIM_SECTOR_SIZE), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&j2), 2);

    uint8_t buf[16];
    uint32_t size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_latest(&j2, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(size, 4);
    ASSERT_TRUE(memcmp(buf, "beta", 4) == 0);

    /* Appending after recovery should work */
    const uint8_t d3[] = "gamma";
    ASSERT_EQ(flash_journal_append(&j2, d3, 5), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&j2), 3);
}

TEST(test_append_null_params)
{
    fresh_journal();
    uint8_t data[] = {1};
    ASSERT_EQ(flash_journal_append(NULL, data, 1), FLASH_JOURNAL_ERR_PARAM);
    ASSERT_EQ(flash_journal_append(&jrnl, NULL, 1), FLASH_JOURNAL_ERR_PARAM);
    ASSERT_EQ(flash_journal_append(&jrnl, data, 0), FLASH_JOURNAL_ERR_PARAM);
}

TEST(test_append_too_large)
{
    fresh_journal();
    /* Entry larger than sector */
    uint8_t big[SIM_SECTOR_SIZE];
    memset(big, 0xAA, sizeof(big));
    ASSERT_EQ(flash_journal_append(&jrnl, big, SIM_SECTOR_SIZE),
              FLASH_JOURNAL_ERR_FULL);
}

TEST(test_storage_write_failure)
{
    fresh_journal();
    const uint8_t data[] = "test";

    sim_fail_write = true;
    ASSERT_EQ(flash_journal_append(&jrnl, data, 4),
              FLASH_JOURNAL_ERR_STORAGE);
    sim_fail_write = false;
}

TEST(test_count_null)
{
    ASSERT_EQ(flash_journal_count(NULL), 0);
}

TEST(test_erase_all_null)
{
    ASSERT_EQ(flash_journal_erase_all(NULL), FLASH_JOURNAL_ERR_PARAM);
}

TEST(test_variable_size_entries)
{
    fresh_journal();
    const uint8_t short_data[] = {0x01};
    const uint8_t medium_data[] = "medium-payload";
    const uint8_t long_data[] = "this is a somewhat longer payload for testing";

    ASSERT_EQ(flash_journal_append(&jrnl, short_data, 1), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_append(&jrnl, medium_data, 14), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_append(&jrnl, long_data, 46), FLASH_JOURNAL_OK);

    ASSERT_EQ(flash_journal_count(&jrnl), 3);

    uint8_t buf[64];
    uint32_t size;

    size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_nth(&jrnl, 0, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(size, 1);
    ASSERT_EQ(buf[0], 0x01);

    size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_nth(&jrnl, 1, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(size, 14);
    ASSERT_TRUE(memcmp(buf, "medium-payload", 14) == 0);

    size = sizeof(buf);
    ASSERT_EQ(flash_journal_read_nth(&jrnl, 2, buf, &size), FLASH_JOURNAL_OK);
    ASSERT_EQ(size, 46);
    ASSERT_TRUE(memcmp(buf, long_data, 46) == 0);
}

TEST(test_corrupt_magic_on_init)
{
    /* Write a valid entry, then corrupt the magic bytes in flash.
     * Re-init should find 0 entries (corrupt entry skipped). */
    fresh_journal();
    const uint8_t data[] = "valid";
    ASSERT_EQ(flash_journal_append(&jrnl, data, 5), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 1);

    /* Corrupt the magic bytes at sector 0, offset 0 */
    sim_flash[0] = 0xFF;
    sim_flash[1] = 0xFF;
    sim_flash[2] = 0xFF;
    sim_flash[3] = 0xFF;

    /* Re-init — scan should skip the corrupted entry */
    flash_journal_t j2;
    ASSERT_EQ(flash_journal_init(&j2, &sim_storage, SIM_SECTOR_COUNT,
              SIM_SECTOR_SIZE), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&j2), 0);
}

TEST(test_corrupt_crc_on_read)
{
    /* Write a valid entry, then corrupt the CRC bytes in flash.
     * read_latest should return ERR_CORRUPT. */
    fresh_journal();
    const uint8_t data[] = "payload";
    ASSERT_EQ(flash_journal_append(&jrnl, data, 7), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 1);

    /* Corrupt the CRC footer: entry is at offset 0, CRC is at
     * offset = HDR_SIZE(10) + payload_len(7) = 17 */
    uint32_t crc_pos = FLASH_JOURNAL_HDR_SIZE + 7;
    sim_flash[crc_pos] ^= 0xFF;

    /* read_latest scans from flash — should detect CRC mismatch */
    uint8_t buf[32];
    uint32_t size = sizeof(buf);
    flash_journal_err_t err = flash_journal_read_latest(&jrnl, buf, &size);
    /* After corruption, the entry is no longer valid */
    ASSERT_TRUE(err == FLASH_JOURNAL_ERR_CORRUPT || err == FLASH_JOURNAL_ERR_EMPTY);
}

TEST(test_err_size_output_buffer_too_small)
{
    /* Append a 20-byte payload, then read with a buffer smaller than 20.
     * read_entry should return ERR_SIZE. */
    fresh_journal();
    uint8_t data[20];
    memset(data, 0xAB, sizeof(data));
    ASSERT_EQ(flash_journal_append(&jrnl, data, 20), FLASH_JOURNAL_OK);

    /* Try to read with a 5-byte buffer — too small for 20-byte payload */
    uint8_t small_buf[5];
    uint32_t size = sizeof(small_buf);
    ASSERT_EQ(flash_journal_read_latest(&jrnl, small_buf, &size),
              FLASH_JOURNAL_ERR_SIZE);
}

TEST_MAIN("flash_journal")
    RUN(test_zero_init_safety);
    RUN(test_init_null_params);
    RUN(test_init_bad_geometry);
    RUN(test_init_empty_flash);
    RUN(test_append_and_read_latest);
    RUN(test_append_multiple_read_latest);
    RUN(test_read_nth);
    RUN(test_read_empty_journal);
    RUN(test_erase_all);
    RUN(test_sector_wraparound);
    RUN(test_wraparound_overwrites_oldest);
    RUN(test_init_recovers_state);
    RUN(test_append_null_params);
    RUN(test_append_too_large);
    RUN(test_storage_write_failure);
    RUN(test_count_null);
    RUN(test_erase_all_null);
    RUN(test_variable_size_entries);
    RUN(test_corrupt_magic_on_init);
    RUN(test_corrupt_crc_on_read);
    RUN(test_err_size_output_buffer_too_small);
TEST_MAIN_END()
