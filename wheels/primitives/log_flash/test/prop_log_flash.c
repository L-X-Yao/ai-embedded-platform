/*
 * prop_log_flash.c — property-based tests for log_flash
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_log_flash.c ../log_flash.c -o prop_log_flash
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_flash.h"
#include <string.h>

/* --- mock flash HAL ----------------------------------------------------- */

#define MOCK_SECTOR_SIZE   256
#define MOCK_SECTOR_COUNT  4
#define MOCK_FLASH_SIZE    (MOCK_SECTOR_SIZE * MOCK_SECTOR_COUNT)

static uint8_t mock_flash[MOCK_FLASH_SIZE];

static int mock_read(void *ctx, uint32_t offset, uint8_t *buf, size_t len)
{
    (void)ctx;
    if (offset + len > MOCK_FLASH_SIZE) return -1;
    memcpy(buf, mock_flash + offset, len);
    return 0;
}

static int mock_write(void *ctx, uint32_t offset, const uint8_t *data, size_t len)
{
    (void)ctx;
    if (offset + len > MOCK_FLASH_SIZE) return -1;
    /* NOR flash semantics: can only clear bits */
    for (size_t i = 0; i < len; i++) {
        mock_flash[offset + i] &= data[i];
    }
    return 0;
}

static int mock_erase(void *ctx, uint32_t sector_offset, size_t sector_size)
{
    (void)ctx;
    if (sector_offset + sector_size > MOCK_FLASH_SIZE) return -1;
    memset(mock_flash + sector_offset, 0xFF, sector_size);
    return 0;
}

static const log_flash_hal_t mock_hal = {
    .read  = mock_read,
    .write = mock_write,
    .erase = mock_erase,
};

static void reset_flash(void)
{
    memset(mock_flash, 0xFF, sizeof(mock_flash));
}

/* --- properties --------------------------------------------------------- */

/* Property: sink never crashes on random input */
PROPERTY(no_crash_random_sink, 50000) {
    reset_flash();
    log_flash_t f;
    int rc = log_flash_init(&f, &mock_hal, NULL, 0, MOCK_SECTOR_SIZE, MOCK_SECTOR_COUNT);
    PROP_ASSERT_EQ(rc, 0);

    char line[128];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, 127);
    ARBI_FILL(seed, (uint8_t *)line, len);
    for (unsigned i = 0; i < len; i++) {
        if (line[i] == '\0') line[i] = 'X';
    }

    log_flash_sink(&f, line, len);
    PROP_ASSERT(1);  /* survived */
}

/* Property: determinism — same sequence produces same state */
PROPERTY(deterministic_state, 5000) {
    uint32_t totals[2], drops[2];

    for (int pass = 0; pass < 2; pass++) {
        reset_flash();
        log_flash_t f;
        log_flash_init(&f, &mock_hal, NULL, 0, MOCK_SECTOR_SIZE, MOCK_SECTOR_COUNT);

        uint32_t local_seed = seed;
        unsigned ops = (unsigned)ARBI_RANGE(local_seed, 1, 30);
        for (unsigned i = 0; i < ops; i++) {
            char line[64];
            unsigned len = (unsigned)ARBI_RANGE(local_seed, 1, 60);
            ARBI_FILL(local_seed, (uint8_t *)line, len);
            for (unsigned j = 0; j < len; j++) {
                if (line[j] == '\0') line[j] = 'A';
            }
            log_flash_sink(&f, line, len);
        }
        totals[pass] = log_flash_total(&f);
        drops[pass] = log_flash_dropped(&f);
    }
    PROP_ASSERT_EQ(totals[0], totals[1]);
    PROP_ASSERT_EQ(drops[0], drops[1]);
}

/* Property: total + dropped == number of sink calls */
PROPERTY(total_plus_dropped_eq_calls, 50000) {
    reset_flash();
    log_flash_t f;
    log_flash_init(&f, &mock_hal, NULL, 0, MOCK_SECTOR_SIZE, MOCK_SECTOR_COUNT);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 40);
    for (unsigned i = 0; i < ops; i++) {
        char line[80];
        unsigned len = (unsigned)ARBI_RANGE(seed, 1, 79);
        ARBI_FILL(seed, (uint8_t *)line, len);
        for (unsigned j = 0; j < len; j++) {
            if (line[j] == '\0') line[j] = 'B';
        }
        log_flash_sink(&f, line, len);
    }

    PROP_ASSERT_EQ(log_flash_total(&f) + log_flash_dropped(&f), ops);
}

/* Property: write-read roundtrip — last written data is recoverable */
PROPERTY(write_read_roundtrip, 5000) {
    reset_flash();
    log_flash_t f;
    log_flash_init(&f, &mock_hal, NULL, 0, MOCK_SECTOR_SIZE, MOCK_SECTOR_COUNT);

    /* Write a single short line */
    char line[32];
    unsigned len = (unsigned)ARBI_RANGE(seed, 1, 30);
    ARBI_FILL(seed, (uint8_t *)line, len);
    for (unsigned i = 0; i < len; i++) {
        if (line[i] == '\0') line[i] = 'C';
    }

    log_flash_sink(&f, line, len);

    if (log_flash_total(&f) > 0) {
        char dst[1024];
        size_t read = log_flash_read(&f, dst, sizeof(dst));
        PROP_ASSERT(read >= len);
        /* The written content should appear in read output */
        PROP_ASSERT(memcmp(dst, line, len) == 0);
    }
}

/* Property: erase_all resets counters and clears data */
PROPERTY(erase_all_resets, 5000) {
    reset_flash();
    log_flash_t f;
    log_flash_init(&f, &mock_hal, NULL, 0, MOCK_SECTOR_SIZE, MOCK_SECTOR_COUNT);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 20);
    for (unsigned i = 0; i < ops; i++) {
        log_flash_sink(&f, "data", 4);
    }

    int rc = log_flash_erase_all(&f);
    PROP_ASSERT_EQ(rc, 0);
    PROP_ASSERT_EQ(log_flash_total(&f), 0);
    PROP_ASSERT_EQ(log_flash_dropped(&f), 0);

    char dst[1024];
    size_t read = log_flash_read(&f, dst, sizeof(dst));
    PROP_ASSERT_EQ(read, 0);
}

TEST_MAIN("log_flash properties") {
    RUN(no_crash_random_sink);
    RUN(deterministic_state);
    RUN(total_plus_dropped_eq_calls);
    RUN(write_read_roundtrip);
    RUN(erase_all_resets);
}
TEST_MAIN_END()
