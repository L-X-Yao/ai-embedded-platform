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
    /* NOR: programming can only clear bits, only an erase sets them */
    for (size_t i = 0; i < len; i++) g_flash[offset + i] &= data[i];
    return 0;
}

static int g_erase_fail_after = -1;
static int g_erase_fail_next = 0;
static int g_erase_call_count = 0;

static int mock_erase(void *ctx, uint32_t sector_offset, size_t sector_size) {
    (void)ctx;
    if (sector_offset + sector_size > FLASH_SIZE) return -1;
    if (g_erase_fail_after >= 0 && g_erase_call_count >= g_erase_fail_after)
        return -1;
    if (g_erase_fail_next > 0) {
        g_erase_fail_next--;
        return -1;
    }
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
    g_erase_fail_next = 0;
    g_erase_call_count = 0;
    log_flash_init(&g_f, &g_hal, NULL, 0, SECTOR_SIZE, SECTOR_COUNT);
}

/* A 20-byte line: an entry takes 23 bytes, so two fit in a 64-byte sector. */
#define LINE_LEN 20

static void put_line(char c) {
    char line[LINE_LEN];
    memset(line, c, sizeof(line));
    line[LINE_LEN - 1] = '\n';
    log_flash_sink(&g_f, line, sizeof(line));
}

/* Two sectors, both full: A B in sector 0, C E in sector 1. */
static void fill_two_sectors(void) {
    reset();
    log_flash_init(&g_f, &g_hal, NULL, 0, SECTOR_SIZE, 2);
    put_line('A');
    put_line('B');
    put_line('C');
    put_line('E');
}

/* Assert that the log reads back as these lines, oldest first. */
static int read_is(const char *first_chars) {
    char out[SECTOR_SIZE * 2];
    size_t count = strlen(first_chars);
    size_t n = log_flash_read(&g_f, out, sizeof(out));

    if (n != count * LINE_LEN) return 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < LINE_LEN - 1; j++) {
            if (out[i * LINE_LEN + j] != first_chars[i]) return 0;
        }
        if (out[i * LINE_LEN + LINE_LEN - 1] != '\n') return 0;
    }
    return 1;
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

/* The next line must retry the erase, not program over the old sector. */
TEST(test_failed_wrap_erase_is_retried) {
    fill_two_sectors();
    ASSERT_EQ(log_flash_total(&g_f), 4);
    ASSERT_TRUE(read_is("ABCE"));

    g_erase_fail_next = 1;
    put_line('D');
    ASSERT_EQ(log_flash_dropped(&g_f), 1);
    ASSERT_EQ(log_flash_total(&g_f), 4);
    ASSERT_TRUE(read_is("ABCE"));

    put_line('x');
    ASSERT_EQ(log_flash_dropped(&g_f), 1);
    ASSERT_EQ(log_flash_total(&g_f), 5);
    ASSERT_TRUE(read_is("CEx"));
}

/* While the erase keeps failing nothing old is lost and nothing is counted. */
TEST(test_erase_failing_for_good_keeps_old_lines) {
    fill_two_sectors();

    g_erase_fail_after = g_erase_call_count;
    for (int i = 0; i < 5; i++) put_line('x');
    ASSERT_EQ(log_flash_dropped(&g_f), 5);
    ASSERT_EQ(log_flash_total(&g_f), 4);
    ASSERT_TRUE(read_is("ABCE"));
}

/* A line that still fits the current sector needs no erase at all. */
TEST(test_short_line_fits_after_failed_erase) {
    fill_two_sectors();

    g_erase_fail_next = 1;
    put_line('D');
    ASSERT_EQ(log_flash_dropped(&g_f), 1);

    log_flash_sink(&g_f, "ok\n", 3);
    ASSERT_EQ(log_flash_dropped(&g_f), 1);
    ASSERT_EQ(log_flash_total(&g_f), 5);

    char out[SECTOR_SIZE * 2];
    size_t n = log_flash_read(&g_f, out, sizeof(out));
    ASSERT_EQ(n, 4 * LINE_LEN + 3);
    ASSERT_MEMEQ(out + 4 * LINE_LEN, "ok\n");
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
    RUN(test_failed_wrap_erase_is_retried);
    RUN(test_erase_failing_for_good_keeps_old_lines);
    RUN(test_short_line_fits_after_failed_erase);
    RUN(test_read_partial_buffer);
    RUN(test_null_safety);
TEST_MAIN_END()
