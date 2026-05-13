// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "log_flash.h"

#include <string.h>

/* Mock flash: RAM-backed, 4 sectors x 64 bytes */
#define SECTOR_SIZE  64
#define SECTOR_COUNT 4
#define FLASH_SIZE   (SECTOR_SIZE * SECTOR_COUNT)

static uint8_t g_flash[FLASH_SIZE];

static int mock_read(void *ctx, uint32_t offset, uint8_t *buf, size_t len) {
    (void)ctx;
    if (offset + len > FLASH_SIZE) return -1;
    memcpy(buf, g_flash + offset, len);
    return 0;
}

static int mock_write(void *ctx, uint32_t offset, const uint8_t *data, size_t len) {
    (void)ctx;
    if (offset + len > FLASH_SIZE) return -1;
    memcpy(g_flash + offset, data, len);
    return 0;
}

static int g_erase_fail_after = -1;
static int g_erase_call_count = 0;

static int mock_erase(void *ctx, uint32_t sector_offset, size_t sector_size) {
    (void)ctx;
    if (sector_offset + sector_size > FLASH_SIZE) return -1;
    if (g_erase_fail_after >= 0 && g_erase_call_count >= g_erase_fail_after)
        return -1;
    g_erase_call_count++;
    memset(g_flash + sector_offset, 0xFF, sector_size);
    return 0;
}

static const log_flash_hal_t g_hal = {
    .read = mock_read,
    .write = mock_write,
    .erase = mock_erase,
};

static log_flash_t g_f;

static void reset(void) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_erase_fail_after = -1;
    g_erase_call_count = 0;
    log_flash_init(&g_f, &g_hal, NULL, 0, SECTOR_SIZE, SECTOR_COUNT);
}

TEST(test_init) {
    reset();
    ASSERT_EQ(log_flash_total(&g_f), 0);
    ASSERT_EQ(log_flash_dropped(&g_f), 0);
}

TEST(test_write_and_read) {
    reset();
    log_flash_sink(&g_f, "hello\n", 6);
    ASSERT_EQ(log_flash_total(&g_f), 1);

    char out[256];
    size_t n = log_flash_read(&g_f, out, sizeof(out));
    ASSERT_EQ(n, 6);
    ASSERT_MEMEQ(out, "hello\n");
}

TEST(test_multiple_entries) {
    reset();
    log_flash_sink(&g_f, "AAA\n", 4);
    log_flash_sink(&g_f, "BBB\n", 4);
    log_flash_sink(&g_f, "CCC\n", 4);
    ASSERT_EQ(log_flash_total(&g_f), 3);

    char out[256];
    size_t n = log_flash_read(&g_f, out, sizeof(out));
    ASSERT_EQ(n, 12);
    ASSERT_MEMEQ(out, "AAA\nBBB\nCCC\n");
}

TEST(test_sector_rollover) {
    reset();
    /* Each entry = 3 (header) + 10 (data) = 13 bytes.
     * Sector is 64 bytes, fits 4 entries (52 bytes), 5th triggers rollover. */
    const char *msg = "0123456789";
    log_flash_sink(&g_f, msg, 10);
    log_flash_sink(&g_f, msg, 10);
    log_flash_sink(&g_f, msg, 10);
    log_flash_sink(&g_f, msg, 10);
    ASSERT_EQ(g_f.write_sector, 0);

    log_flash_sink(&g_f, msg, 10);
    ASSERT_EQ(g_f.write_sector, 1);
    ASSERT_EQ(log_flash_total(&g_f), 5);
}

TEST(test_wrap_around) {
    reset();
    /* Fill all 4 sectors to force wrap-around */
    const char *msg = "AAAAAAAAAAAAAAAAAA";
    size_t len = 18;
    /* Each entry = 3 + 18 = 21 bytes, 3 per sector */
    for (int i = 0; i < 12; i++) {
        log_flash_sink(&g_f, msg, len);
    }
    /* Should have wrapped around */
    ASSERT_EQ(log_flash_total(&g_f), 12);

    /* Write one more to trigger overwrite of oldest sector */
    log_flash_sink(&g_f, "NEW\n", 4);
    ASSERT_EQ(log_flash_total(&g_f), 13);
}

TEST(test_entry_too_large) {
    reset();
    /* Entry larger than sector size should be dropped */
    char big[128];
    memset(big, 'X', sizeof(big));
    log_flash_sink(&g_f, big, sizeof(big));
    ASSERT_EQ(log_flash_dropped(&g_f), 1);
    ASSERT_EQ(log_flash_total(&g_f), 0);
}

TEST(test_erase_all) {
    reset();
    log_flash_sink(&g_f, "data\n", 5);
    log_flash_sink(&g_f, "more\n", 5);
    ASSERT_EQ(log_flash_total(&g_f), 2);

    ASSERT_EQ(log_flash_erase_all(&g_f), LOG_FLASH_OK);
    ASSERT_EQ(g_f.write_sector, 0);
    ASSERT_EQ(g_f.write_pos, 0);

    char out[256];
    size_t n = log_flash_read(&g_f, out, sizeof(out));
    ASSERT_EQ(n, 0);
}

TEST(test_erase_all_hal_failure) {
    reset();
    log_flash_sink(&g_f, "data\n", 5);
    ASSERT_EQ(log_flash_total(&g_f), 1);

    g_erase_fail_after = g_erase_call_count;
    ASSERT_EQ(log_flash_erase_all(&g_f), LOG_FLASH_ERR_HAL);
    ASSERT_EQ(log_flash_total(&g_f), 1);
}

TEST(test_read_partial_buffer) {
    reset();
    log_flash_sink(&g_f, "long line here\n", 15);

    char out[5];
    size_t n = log_flash_read(&g_f, out, sizeof(out));
    ASSERT_EQ(n, 5);
    ASSERT_MEMEQ(out, "long ");
}

TEST(test_null_safety) {
    log_flash_sink(NULL, "x", 1);
    ASSERT_EQ(log_flash_total(NULL), 0);
    ASSERT_EQ(log_flash_dropped(NULL), 0);
    ASSERT_EQ(log_flash_erase_all(NULL), LOG_FLASH_ERR_PARAM);
    ASSERT_EQ(log_flash_init(NULL, &g_hal, NULL, 0, 64, 4), LOG_FLASH_ERR_PARAM);

    log_flash_t f;
    ASSERT_EQ(log_flash_init(&f, NULL, NULL, 0, 64, 4), LOG_FLASH_ERR_PARAM);
    ASSERT_EQ(log_flash_init(&f, &g_hal, NULL, 0, 0, 4), LOG_FLASH_ERR_PARAM);
    ASSERT_EQ(log_flash_init(&f, &g_hal, NULL, 0, 64, 0), LOG_FLASH_ERR_PARAM);
}

TEST_MAIN("log_flash")
    RUN(test_init);
    RUN(test_write_and_read);
    RUN(test_multiple_entries);
    RUN(test_sector_rollover);
    RUN(test_wrap_around);
    RUN(test_entry_too_large);
    RUN(test_erase_all);
    RUN(test_erase_all_hal_failure);
    RUN(test_read_partial_buffer);
    RUN(test_null_safety);
TEST_MAIN_END()
