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
static int sim_cut_after;      /* bytes programmed before power goes, -1 = never */
static int sim_bad_programs;   /* bytes that needed a 0 bit raised to 1 */
static int sim_fail_read_at;   /* value of sim_read_calls that fails, 0 = none */

static void sim_reset(void)
{
    memset(sim_flash, 0xFF, sizeof(sim_flash));
    sim_read_calls = 0;
    sim_write_calls = 0;
    sim_erase_calls = 0;
    sim_fail_read = false;
    sim_fail_write = false;
    sim_fail_erase = false;
    sim_cut_after = -1;
    sim_bad_programs = 0;
    sim_fail_read_at = 0;
}

static flash_journal_err_t sim_read(void *ctx, uint32_t sector,
                                    uint32_t offset, uint8_t *buf,
                                    uint32_t len)
{
    (void)ctx;
    sim_read_calls++;
    if (sim_fail_read || sim_read_calls == sim_fail_read_at) {
        return FLASH_JOURNAL_ERR_STORAGE;
    }
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
    /* NOR flash: programming can only clear bits */
    for (uint32_t i = 0; i < len; i++) {
        uint8_t *cell = &sim_flash[sector * SIM_SECTOR_SIZE + offset + i];
        if (sim_cut_after == 0) return FLASH_JOURNAL_ERR_STORAGE;
        if (sim_cut_after > 0) sim_cut_after--;
        if ((uint8_t)(*cell & data[i]) != data[i]) sim_bad_programs++;
        *cell &= data[i];
    }
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

/* ---------- Power cuts on the NOR model ----------------------------------- */

#define ENTRY4_SIZE  (FLASH_JOURNAL_OVERHEAD + 4u)

static flash_journal_err_t reboot(void)
{
    sim_cut_after = -1;
    return flash_journal_init(&jrnl, &sim_storage, SIM_SECTOR_COUNT,
                              SIM_SECTOR_SIZE);
}

static flash_journal_err_t put(const char *text)
{
    return flash_journal_append(&jrnl, (const uint8_t *)text,
                                (uint16_t)strlen(text));
}

static bool nth_is(uint32_t n, const char *text)
{
    uint8_t buf[16];
    uint32_t size = sizeof(buf);
    return flash_journal_read_nth(&jrnl, n, buf, &size) == FLASH_JOURNAL_OK &&
           size == strlen(text) && memcmp(buf, text, size) == 0;
}

static bool latest_is(const char *text)
{
    uint8_t buf[16];
    uint32_t size = sizeof(buf);
    return flash_journal_read_latest(&jrnl, buf, &size) == FLASH_JOURNAL_OK &&
           size == strlen(text) && memcmp(buf, text, size) == 0;
}

TEST(test_cut_append_keeps_later_entries)
{
    fresh_journal();
    ASSERT_EQ(put("AAAA"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("BBBB"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("CCCC"), FLASH_JOURNAL_OK);

    sim_cut_after = 12;  /* power goes 12 bytes into the fourth entry */
    (void)put("DDDD");

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 3);
    ASSERT_TRUE(latest_is("CCCC"));

    ASSERT_EQ(put("EEEE"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("FFFF"), FLASH_JOURNAL_OK);
    ASSERT_EQ(sim_bad_programs, 0);
    ASSERT_EQ(flash_journal_count(&jrnl), 5);
    ASSERT_TRUE(nth_is(3, "EEEE"));
    ASSERT_TRUE(nth_is(4, "FFFF"));
    ASSERT_TRUE(latest_is("FFFF"));

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 5);
    ASSERT_TRUE(nth_is(0, "AAAA"));
    ASSERT_TRUE(nth_is(2, "CCCC"));
    ASSERT_TRUE(nth_is(3, "EEEE"));
    ASSERT_TRUE(nth_is(4, "FFFF"));

    ASSERT_EQ(put("GGGG"), FLASH_JOURNAL_OK);
    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 6);
    ASSERT_TRUE(latest_is("GGGG"));
}

TEST(test_cut_first_append_of_empty_journal)
{
    fresh_journal();
    sim_cut_after = 12;  /* past the header: the retry has the same seq */
    (void)put("AAAA");

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 0);

    ASSERT_EQ(put("BBBB"), FLASH_JOURNAL_OK);
    ASSERT_EQ(sim_bad_programs, 0);
    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 1);
    ASSERT_TRUE(latest_is("BBBB"));
}

TEST(test_clean_boot_keeps_write_position)
{
    fresh_journal();
    ASSERT_EQ(put("AAAA"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("BBBB"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("CCCC"), FLASH_JOURNAL_OK);

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(jrnl.write_sector, 0);
    ASSERT_EQ(jrnl.write_offset, 3u * ENTRY4_SIZE);

    /* a cut before any byte was programmed left nothing to avoid */
    sim_cut_after = 0;
    (void)put("DDDD");
    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(jrnl.write_sector, 0);
    ASSERT_EQ(jrnl.write_offset, 3u * ENTRY4_SIZE);
}

TEST(test_boot_sees_last_byte_of_sector_programmed)
{
    fresh_journal();
    ASSERT_EQ(put("AAAA"), FLASH_JOURNAL_OK);
    sim_flash[SIM_SECTOR_SIZE - 1] = 0x7F;

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("BBBB"), FLASH_JOURNAL_OK);
    ASSERT_EQ(jrnl.write_sector, 1);
    ASSERT_EQ(jrnl.write_offset, ENTRY4_SIZE);
    ASSERT_EQ(flash_journal_count(&jrnl), 2);
}

/* No read of a boot may fail unnoticed: the journal would look emptier than it
 * is, and the next append would program over what it could not see. */
TEST(test_boot_reports_every_failed_read)
{
    fresh_journal();
    ASSERT_EQ(put("AAAA"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("BBBB"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("CCCC"), FLASH_JOURNAL_OK);

    sim_read_calls = 0;
    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    int reads = sim_read_calls;
    ASSERT_TRUE(reads >= 9);  /* three reads for each of the three entries */

    for (int failing = 1; failing <= reads; failing++) {
        sim_read_calls = 0;
        sim_fail_read_at = failing;
        ASSERT_EQ(reboot(), FLASH_JOURNAL_ERR_STORAGE);
        sim_fail_read_at = 0;
    }

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 3);
    ASSERT_EQ(jrnl.write_sector, 0);
    ASSERT_EQ(jrnl.write_offset, 3u * ENTRY4_SIZE);
    ASSERT_TRUE(nth_is(0, "AAAA"));
    ASSERT_TRUE(latest_is("CCCC"));
}

/* Even a caller that ignores the failed init cannot damage the entries. */
TEST(test_failed_boot_refuses_appends)
{
    fresh_journal();
    ASSERT_EQ(put("AAAA"), FLASH_JOURNAL_OK);
    ASSERT_EQ(put("BBBB"), FLASH_JOURNAL_OK);

    sim_fail_read = true;
    ASSERT_EQ(reboot(), FLASH_JOURNAL_ERR_STORAGE);
    sim_fail_read = false;

    int writes = sim_write_calls;
    int erases = sim_erase_calls;
    ASSERT_TRUE(put("ZZZZ") != FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 0);
    ASSERT_EQ(sim_write_calls, writes);
    ASSERT_EQ(sim_erase_calls, erases);

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 2);
    ASSERT_EQ(put("CCCC"), FLASH_JOURNAL_OK);
    ASSERT_EQ(sim_bad_programs, 0);

    ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 3);
    ASSERT_TRUE(nth_is(0, "AAAA"));
    ASSERT_TRUE(latest_is("CCCC"));
}

/* Entry id -> payload, so a read can be checked from the id alone. */
static uint16_t model_payload(uint32_t id, uint8_t *buf)
{
    uint16_t len = (uint16_t)(4u + id % 40u);
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(id >> (8u * (i % 4u)));
    }
    return len;
}

static uint32_t model_next(uint32_t *rng)
{
    *rng ^= *rng << 13;
    *rng ^= *rng >> 17;
    *rng ^= *rng << 5;
    return *rng;
}

/* Read back as an id, or UINT32_MAX if the entry is not one model_payload
 * would have produced. */
static uint32_t model_id(const uint8_t *buf, uint32_t size)
{
    uint8_t want[64];
    uint32_t id;

    if (size < 4u) return UINT32_MAX;
    id = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
         ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    if (model_payload(id, want) != size || memcmp(want, buf, size) != 0) {
        return UINT32_MAX;
    }
    return id;
}

/* Boots that each end in a cut at a random byte of the last entry.  Ids go up
 * on every attempt, cut or not, so the next append programs different bytes
 * over a cut entry.  After every boot the journal must hold each acknowledged
 * entry from its oldest on, through the newest: only a wrap may drop anything,
 * and only the oldest.  A gap can only be a cut entry, which may also survive
 * when its missing bytes were all 0xFF already. */
TEST(test_power_cut_model)
{
    static bool acked[256];
    uint32_t rng = 0x2545F491u;

    for (int round = 0; round < 300; round++) {
        uint32_t next_id = 0;  /* ids attempted so far */
        memset(acked, 0, sizeof(acked));
        sim_reset();

        for (int boot = 0; boot < 40; boot++) {
            uint8_t buf[64];
            uint32_t size;
            uint32_t expect = 0;
            uint32_t last = 0;
            ASSERT_EQ(reboot(), FLASH_JOURNAL_OK);

            uint32_t count = flash_journal_count(&jrnl);
            for (uint32_t n = 0; n < count; n++) {
                size = sizeof(buf);
                ASSERT_EQ(flash_journal_read_nth(&jrnl, n, buf, &size),
                          FLASH_JOURNAL_OK);
                uint32_t id = model_id(buf, size);
                ASSERT_TRUE(id < next_id);
                if (n == 0) expect = id;
                ASSERT_TRUE(id >= expect);
                for (; expect < id; expect++) {
                    ASSERT_FALSE(acked[expect]);
                }
                expect = id + 1u;
                last = id;
            }
            for (; expect < next_id; expect++) {
                ASSERT_FALSE(acked[expect]);
            }
            if (count > 0) {
                size = sizeof(buf);
                ASSERT_EQ(flash_journal_read_latest(&jrnl, buf, &size),
                          FLASH_JOURNAL_OK);
                ASSERT_EQ(model_id(buf, size), last);
            }

            uint32_t appends = model_next(&rng) % 5u;
            for (uint32_t a = 0; a < appends; a++) {
                uint16_t len = model_payload(next_id, buf);
                ASSERT_EQ(flash_journal_append(&jrnl, buf, len),
                          FLASH_JOURNAL_OK);
                acked[next_id] = true;
                next_id++;
            }

            uint16_t len = model_payload(next_id, buf);
            sim_cut_after = (int)(model_next(&rng) %
                                  (FLASH_JOURNAL_OVERHEAD + len));
            (void)flash_journal_append(&jrnl, buf, len);
            next_id++;
        }
        ASSERT_EQ(sim_bad_programs, 0);
    }
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
    RUN(test_cut_append_keeps_later_entries);
    RUN(test_cut_first_append_of_empty_journal);
    RUN(test_clean_boot_keeps_write_position);
    RUN(test_boot_sees_last_byte_of_sector_programmed);
    RUN(test_boot_reports_every_failed_read);
    RUN(test_failed_boot_refuses_appends);
    RUN(test_power_cut_model);
TEST_MAIN_END()
